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

    vector<CForkContext> vForkCtxt;
    if (!pBlockChain->ListForkContext(vForkCtxt))
    {
        return false;
    }

    for (const CForkContext& ctxt : vForkCtxt)
    {
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
                sched.RemoveJoint(block.GetHash(), vActive);
                if (sched.IsHalted())
                {
                    vDeactive.push_back(update.hashFork);
                }
            }
        }
    }
    if (update.hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        for (const CBlockEx& block : boost::adaptors::reverse(update.vBlockAddNew))
        {
            for (const CTransaction& tx : block.vtx)
            {
                CTemplateId tid;
                if (tx.sendTo.GetTemplateId(tid) && tid.GetType() == TEMPLATE_FORK
                    && !tx.vchData.empty()
                    && tx.nAmount >= CTemplateFork::CreatedCoin())
                {
                    CForkContext ctxt;
                    if (pBlockChain->AddNewForkContext(tx, ctxt) == OK)
                    {
                        AddNewForkContext(ctxt, vActive);
                    }
                }
            }
        }
    }
}

bool CForkManager::AddNewForkContext(const CForkContext& ctxt, vector<uint256>& vActive)
{
    if (IsAllowedFork(ctxt.hashFork, ctxt.hashParent))
    {
        mapForkSched.insert(make_pair(ctxt.hashFork, CForkSchedule(true)));

        if (ctxt.hashFork == pCoreProtocol->GetGenesisBlockHash())
        {
            vActive.push_back(ctxt.hashFork);
            forkSetMgr.Add(CForkContextEx(ctxt, 0));
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

            CForkContext ctxtParent;
            if (!pBlockChain->GetForkContext(hashParent, ctxtParent))
            {
                return false;
            }
            schedParent.AddNewJoint(hashJoint, hashFork);

            hashParent = ctxtParent.hashParent;
            hashJoint = ctxtParent.hashJoint;
            hashFork = ctxtParent.hashFork;
        }
    }

    uint256 hashFork;
    int32 nHeight;
    if (!pBlockChain->GetTxLocation(ctxt.txidEmbedded, hashFork, nHeight))
    {
        return false;
    }
    forkSetMgr.Add(CForkContextEx(ctxt, nHeight));

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
        for (auto& link : setCtxt)
        {
            mapSubline.insert(make_pair(link.nJointHeight, link.hashFork));
        }
        vSubline.assign(mapSubline.begin(), mapSubline.end());
        return true;
    }
    return false;
}

bool CForkManager::GetCreatedHeight(const uint256& hashFork, int& nCreatedHeight) const
{
    CForkContextEx ctxt;
    if (forkSetMgr.RetrieveByFork(hashFork, ctxt))
    {
        nCreatedHeight = ctxt.nCreatedHeight;
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
        CForkContext ctxtParent;
        while (hash != 0 && pBlockChain->GetForkContext(hash, ctxtParent))
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
