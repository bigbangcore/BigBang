// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "forkmanager.h"

#include <boost/range/adaptor/reversed.hpp>

#include "defs.h"
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
    int8 nodeCat = dynamic_cast<const CBasicConfig*>(Config())->nCatOfNode;
    if (NODE_CAT_DPOSNODE == nodeCat)
    {
        setForkAllowed.insert(pCoreProtocol->GetGenesisBlockHash());
        return true;
    }

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
    }

    return true;
}

void CForkManager::HandleHalt()
{
    setForkIndex.clear();
    mapForkSched.clear();
    setForkAllowed.clear();
    setGroupAllowed.clear();
    fAllowAnyFork = false;
}

bool CForkManager::IsAllowed(const uint256& hashFork) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);

    map<uint256, CForkSchedule>::const_iterator it = mapForkSched.find(hashFork);
    return (it != mapForkSched.end() && (*it).second.IsAllowed());
}

bool CForkManager::GetJoint(const uint256& hashFork, uint256& hashParent, uint256& hashJoint, int& nHeight) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);

    const CForkLinkSetByFork& idxFork = setForkIndex.get<0>();
    const CForkLinkSetByFork::iterator it = idxFork.find(hashFork);
    if (it != idxFork.end())
    {
        hashParent = (*it).hashParent;
        hashJoint = (*it).hashJoint;
        nHeight = (*it).nJointHeight;
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

    Log("listfork[%d]", vForkCtxt.size());
    for (const CForkContext& ctxt : vForkCtxt)
    {
        Log("listfork[%s][%s][%s]", ctxt.strName.c_str(), ctxt.strSymbol.c_str(), ctxt.hashFork.ToString().c_str());
        if (!AddNewForkContext(ctxt, vActive))
        {
            Error("listfork - add failed[%s][%s][%s]", ctxt.strName.c_str(), ctxt.strSymbol.c_str(), ctxt.hashFork.ToString().c_str());
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
            setForkIndex.insert(CForkLink(ctxt, 0));
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
    int nHeight;
    if (!pBlockChain->GetTxLocation(ctxt.txidEmbedded, hashFork, nHeight))
    {
        return false;
    }
    setForkIndex.insert(CForkLink(ctxt, nHeight));

    return true;
}

void CForkManager::GetForkList(std::vector<uint256>& vFork) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);

    vFork.reserve(setForkIndex.size());
    for (auto it = setForkIndex.begin(); it != setForkIndex.end(); it++)
    {
        vFork.push_back(it->hashFork);
    }
}

bool CForkManager::GetSubline(const uint256& hashFork, vector<pair<int, uint256>>& vSubline) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);

    const CForkLinkSetByParent& idxParent = setForkIndex.get<3>();
    CForkLinkSetByParent::const_iterator itBegin = idxParent.lower_bound(hashFork);
    CForkLinkSetByParent::const_iterator itEnd = idxParent.upper_bound(hashFork);
    if (itBegin == itEnd)
    {
        return false;
    }

    multimap<int, uint256> mapSubline;
    for (; itBegin != itEnd; ++itBegin)
    {
        mapSubline.insert(make_pair(itBegin->nJointHeight, itBegin->hashFork));
    }

    vSubline.assign(mapSubline.begin(), mapSubline.end());

    return true;
}

bool CForkManager::SetForkFilter(const std::vector<uint256>& vFork, const std::vector<uint256>& vGroup)
{
    int8 nodeCat = dynamic_cast<const CBasicConfig*>(Config())->nCatOfNode;
    if (NODE_CAT_DPOSNODE == nodeCat)
    {
        return true;
    }

    {
        boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
        if (!fAllowAnyFork)
        {
            for (auto const& fork : vFork)
            {
                if (fork != 0)
                {
                    setForkAllowed.insert(fork);
                    Log("CForkManager::SetForkFilter: set fork filter[%s]", fork.ToString().c_str());
                }
            }
            for (auto const& group : vGroup)
            {
                if (group != 0)
                {
                    setGroupAllowed.insert(group);
                    Log("CForkManager::SetForkFilter: set fork group filter[%s]", group.ToString().c_str());
                }
            }
        }
    }

    return true;
}

bool CForkManager::GetCreatedHeight(const uint256& hashFork, int& nCreatedHeight) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);

    const CForkLinkSetByFork& idxFork = setForkIndex.get<0>();
    const CForkLinkSetByFork::iterator it = idxFork.find(hashFork);
    if (it != idxFork.end())
    {
        nCreatedHeight = it->nCreatedHeight;
        return true;
    }
    return false;
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
