// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "forkmanager.h"

#include <boost/range/adaptor/reversed.hpp>

#include "template/fork.h"

using namespace std;
using namespace xengine;

namespace bigbang
{

//////////////////////////////
// CForkManager

CForkManager::CForkManager()
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    fAllowAnyFork = false;
}

CForkManager::~CForkManager()
{
}

bool CForkManager::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("blockchain", pBlockChain))
    {
        Error("Failed to request blockchain");
        return false;
    }

    return true;
}

void CForkManager::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
}

bool CForkManager::HandleInvoke()
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);

    fAllowAnyFork = ForkConfig()->fAllowAnyFork;
    if (!fAllowAnyFork)
    {
        setForkAllowed.insert(pCoreProtocol->GetGenesisBlockHash());
        for (const string& strFork : ForkConfig()->vFork)
        {
            uint256 hashFork(strFork);
            if (hashFork != 0)
            {
                setForkAllowed.insert(hashFork);
            }
        }
        for (const string& strFork : ForkConfig()->vGroup)
        {
            uint256 hashFork(strFork);
            if (hashFork != 0)
            {
                setGroupAllowed.insert(hashFork);
            }
        }
    }

    return true;
}

void CForkManager::HandleHalt()
{
    forkSetMgr.Clear();
    mapForkSched.clear();
    setForkAllowed.clear();
    setGroupAllowed.clear();
    fAllowAnyFork = false;
}

bool CForkManager::IsAllowed(const uint256& hashFork) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);

    map<uint256, CForkSchedule>::const_iterator it = mapForkSched.find(hashFork);
    return (it != mapForkSched.end() && it->second.IsAllowed());
}

bool CForkManager::GetJoint(const uint256& hashFork, uint256& hashParent, uint256& hashJoint, int32& nJointHeight) const
{
    CForkContextEx ctxt;
    if (forkSetMgr.RetrieveByFork(hashFork, ctxt))
    {
        hashParent = ctxt.hashParent;
        hashJoint = ctxt.hashJoint;
        nJointHeight = ctxt.nJointHeight;
        return true;
    }
    return false;
}

bool CForkManager::LoadForkContext(vector<uint256>& vActive)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);

    vector<pair<CForkContext, bool>> vForkCtxt;
    if (!pBlockChain->ListForkContext(vForkCtxt))
    {
        return false;
    }

    for (auto& p : vForkCtxt)
    {
        CForkContextEx ctxt(p.first, -1, p.second);
        if (!AddNewForkContext(ctxt, vActive))
        {
            return false;
        }
    }

    return true;
}

void CForkManager::ForkUpdate(const CBlockChainUpdate& update, vector<uint256>& vActive, vector<uint256>& vDeactive)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);

    CForkSchedule& sched = mapForkSched[update.hashFork];
    if (!sched.IsJointEmpty())
    {
        for (const CBlockEx& block : boost::adaptors::reverse(update.vBlockAddNew))
        {
            if (!block.IsExtended() && !block.IsVacant())
            {
                vector<uint256> vecNextFork = sched.RemoveJoint(block.GetHash());
                for (const uint256& hashFork : vecNextFork)
                {
                    if (mapForkSched.find(hashFork) != mapForkSched.end())
                    {
                        vActive.push_back(hashFork);
                    }
                }
                if (sched.IsHalted())
                {
                    vDeactive.push_back(update.hashFork);
                }
            }
        }
    }

    if (update.hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        vector<CForkContextEx> vFork;
        update.forkSetMgr.GetForkContextExList(vFork);
        CForkContextEx curCtxt;
        for (CForkContextEx& ctxt : vFork)
        {
            if (!ctxt.IsActive())
            {
                // delete fork
                if (forkSetMgr.RetrieveByFork(ctxt.hashFork, curCtxt))
                {
                    if (curCtxt.IsActive())
                    {
                        if (pBlockChain->InactivateFork(ctxt.hashFork))
                        {
                            forkSetMgr.ChangeActive(ctxt.hashFork, false);
                            if (mapForkSched.find(ctxt.hashFork) != mapForkSched.end())
                            {
                                mapForkSched.erase(ctxt.hashFork);
                                vDeactive.push_back(ctxt.hashFork);
                            }
                            Log("fork manager inactivate fork: %s", ctxt.hashFork.ToString().c_str());
                        }
                        else
                        {
                            Error("fork manager inactivate fork fail, fork: %s", ctxt.hashFork.ToString().c_str());
                        }
                    }
                    else
                    {
                        Error("fork manager inactivate fork fail, fork is already inactived, fork: %s", ctxt.hashFork.ToString().c_str());
                    }
                }
                else
                {
                    Error("fork manager delete fork fail, no fork: %s", ctxt.hashFork.ToString().c_str());
                }
            }
            else
            {
                // add fork
                if (!forkSetMgr.RetrieveByFork(ctxt.hashFork, curCtxt) || (curCtxt.txidEmbedded != ctxt.txidEmbedded) || !curCtxt.IsActive())
                {
                    if (pBlockChain->AddNewForkContext(ctxt))
                    {
                        AddNewForkContext(ctxt, vActive);
                        Log("fork manager add fork: %s, height: %d", ctxt.hashFork.ToString().c_str(), ctxt.nCreatedHeight);
                    }
                    else
                    {
                        Error("fork manager add fork fail, fork: %s, tx: %s", ctxt.hashFork.ToString().c_str(), ctxt.txidEmbedded.ToString().c_str());
                    }
                }
                else if (curCtxt.nCreatedHeight != ctxt.nCreatedHeight)
                {
                    forkSetMgr.ChangeCreatedHeight(ctxt.hashFork, ctxt.nCreatedHeight);
                    Log("fork manager change fork created height, fork: %s, height: %d", ctxt.hashFork.ToString().c_str(), ctxt.nCreatedHeight);
                }
            }
        }
    }
}

bool CForkManager::AddNewForkContext(CForkContextEx& ctxt, vector<uint256>& vActive)
{
    if (IsAllowedFork(ctxt.hashFork, ctxt.hashParent) && ctxt.fActive && !mapForkSched.count(ctxt.hashFork))
    {
        mapForkSched[ctxt.hashFork] = CForkSchedule(true);

        if (ctxt.hashFork == pCoreProtocol->GetGenesisBlockHash())
        {
            vActive.push_back(ctxt.hashFork);
            forkSetMgr.Insert(CForkContextEx(ctxt, 0));
            return true;
        }

        uint256 hashParent = ctxt.hashParent;
        uint256 hashJoint = ctxt.hashJoint;
        uint256 hashFork = ctxt.hashFork;
        for (;;)
        {
            if (pBlockChain->Exists(hashJoint))
            {
                vActive.push_back(hashFork);
                break;
            }

            CForkSchedule& schedParent = mapForkSched[hashParent];
            if (!schedParent.IsHalted())
            {
                schedParent.AddNewJoint(hashJoint, hashFork);
                break;
            }

            CForkContextEx ctxtParent;
            if (!forkSetMgr.RetrieveByFork(hashParent, ctxtParent))
            {
                return false;
            }
            schedParent.AddNewJoint(hashJoint, hashFork);

            hashParent = ctxtParent.hashParent;
            hashJoint = ctxtParent.hashJoint;
            hashFork = ctxtParent.hashFork;
        }
    }

    if (ctxt.nCreatedHeight < 0)
    {
        uint256 hashFork;
        if (!pBlockChain->GetTxLocation(ctxt.txidEmbedded, hashFork, ctxt.nCreatedHeight))
        {
            return false;
        }
    }
    forkSetMgr.Insert(ctxt);

    return true;
}

void CForkManager::GetForkList(std::vector<uint256>& vFork) const
{
    forkSetMgr.GetForkList(vFork);
}

bool CForkManager::GetSubline(const uint256& hashFork, vector<pair<int32, uint256>>& vSubline) const
{
    set<CForkContextEx> setCtxt;
    if (forkSetMgr.RetrieveByParent(hashFork, setCtxt))
    {
        multimap<int32, uint256> mapSubline;
        for (auto& ctxt : setCtxt)
        {
            mapSubline.insert(make_pair(ctxt.nJointHeight, ctxt.hashFork));
        }
        vSubline.assign(mapSubline.begin(), mapSubline.end());
        return true;
    }
    return false;
}

bool CForkManager::GetCreatedHeight(const uint256& hashFork, int32& nCreatedHeight) const
{
    CForkContextEx ctxt;
    if (forkSetMgr.RetrieveByFork(hashFork, ctxt))
    {
        nCreatedHeight = ctxt.nCreatedHeight;
        return true;
    }

    nCreatedHeight = -1;
    return false;
}

bool CForkManager::GetForkContextEx(const uint256& hashFork, CForkContextEx& ctxt) const
{
    if (forkSetMgr.RetrieveByFork(hashFork, ctxt))
    {
        return true;
    }
    return false;
}

const CForkSetManager& CForkManager::GetForkSetManager() const
{
    return forkSetMgr;
}

bool CForkManager::IsAllowedFork(const uint256& hashFork, const uint256& hashParent) const
{
    if (fAllowAnyFork || setForkAllowed.count(hashFork) || setGroupAllowed.count(hashFork))
    {
        return true;
    }
    if (!setGroupAllowed.empty())
    {
        if (setGroupAllowed.count(hashParent))
        {
            return true;
        }
        uint256 hash = hashParent;
        CForkContextEx ctxtParent;
        while (hash != 0 && forkSetMgr.RetrieveByFork(hash, ctxtParent))
        {
            if (setGroupAllowed.count(ctxtParent.hashParent))
            {
                return true;
            }
            hash = ctxtParent.hashParent;
        }
    }
    return false;
}

} // namespace bigbang
