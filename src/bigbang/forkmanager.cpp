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

    const auto it = mapForkSched.find(hashFork);
    if (it != mapForkSched.end())
    {
        hashParent = it->second.ctxtFork.hashParent;
        hashJoint = it->second.ctxtFork.hashJoint;
        nHeight = it->second.ctxtFork.nJointHeight;
        return true;
    }
    return false;
}

bool CForkManager::LoadForkContext(const uint256& hashPrimaryLastBlockIn, const vector<CForkContext>& vForkCtxt,
                                   const map<uint256, pair<uint256, map<uint256, int>>>& mapValidForkId, vector<uint256>& vActive)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);

    hashPrimaryLastBlock = hashPrimaryLastBlockIn;

    for (const auto& vd : mapValidForkId)
    {
        mapBlockValidFork.insert(make_pair(vd.first, CValidFdForkId(vd.second.first, vd.second.second)));
    }

    for (const CForkContext& ctxt : vForkCtxt)
    {
        if (!AddDbForkContext(ctxt, vActive))
        {
            return false;
        }
    }
    return true;
}

void CForkManager::SetPrimaryLastBlock(const uint256& hashPrimaryLastBlockIn)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
    if (hashPrimaryLastBlockIn != 0)
    {
        hashPrimaryLastBlock = hashPrimaryLastBlockIn;
    }
}

bool CForkManager::VerifyFork(const uint256& hashPrevBlock, const uint256& hashFork, const string& strForkName)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    map<uint256, int> mapValidFork;
    if (GetValidFdForkId(hashPrevBlock, mapValidFork))
    {
        if (mapValidFork.count(hashFork) > 0)
        {
            StdLog("ForkManager", "VerifyFork: Fork existed, fork: %s", hashFork.GetHex().c_str());
            return false;
        }
        for (const auto& vd : mapValidFork)
        {
            const auto mt = mapForkSched.find(vd.first);
            if (mt != mapForkSched.end() && mt->second.ctxtFork.strName == strForkName)
            {
                StdLog("ForkManager", "VerifyFork: Fork name repeated, new fork: %s, valid fork: %s, name: %s",
                       hashFork.GetHex().c_str(), vd.first.GetHex().c_str(), strForkName.c_str());
                return false;
            }
        }
    }
    return true;
}

bool CForkManager::AddValidForkContext(const uint256& hashPrevBlock, const uint256& hashNewBlock, const vector<CForkContext>& vForkCtxt,
                                       bool fCheckPointBlock, uint256& hashRefFdBlock, map<uint256, int>& mapValidFork)
{
    CValidFdForkId& fd = mapBlockValidFork[hashNewBlock];
    if (fCheckPointBlock)
    {
        fd.mapForkId.clear();
        if (hashPrevBlock != 0)
        {
            if (!GetValidFdForkId(hashPrevBlock, fd.mapForkId))
            {
                StdError("ForkManager", "Add fork context: Get prev valid block fail, prev: %s", hashPrevBlock.GetHex().c_str());
                mapBlockValidFork.erase(hashNewBlock);
                return false;
            }
        }
        fd.hashRefFdBlock = uint256();
    }
    else
    {
        const auto it = mapBlockValidFork.find(hashPrevBlock);
        if (it == mapBlockValidFork.end())
        {
            StdError("ForkManager", "Add fork context: Find prev valid block fail, prev: %s", hashPrevBlock.GetHex().c_str());
            mapBlockValidFork.erase(hashNewBlock);
            return false;
        }
        const CValidFdForkId& prevfd = it->second;
        fd.mapForkId.clear();
        if (prevfd.hashRefFdBlock == 0)
        {
            fd.hashRefFdBlock = hashPrevBlock;
        }
        else
        {
            fd.mapForkId.insert(prevfd.mapForkId.begin(), prevfd.mapForkId.end());
            fd.hashRefFdBlock = prevfd.hashRefFdBlock;
        }
    }

    for (const CForkContext& ctxt : vForkCtxt)
    {
        CForkSchedule& sched = mapForkSched[ctxt.hashFork];

        sched.ctxtFork = ctxt;
        sched.fAllowed = IsAllowedFork(ctxt.hashFork, ctxt.hashParent);
        if (ctxt.hashParent != 0)
        {
            mapForkSched[ctxt.hashParent].AddNewJoint(ctxt.hashJoint, ctxt.hashFork);
        }

        fd.mapForkId.insert(make_pair(ctxt.hashFork, CBlock::GetBlockHeightByHash(hashNewBlock)));
    }

    hashRefFdBlock = fd.hashRefFdBlock;
    mapValidFork.clear();
    mapValidFork.insert(fd.mapForkId.begin(), fd.mapForkId.end());
    return true;
}

void CForkManager::RemoveValidForkContext(const uint256& hashBlock)
{
    mapBlockValidFork.erase(hashBlock);
}

void CForkManager::ForkUpdate(const CBlockChainUpdate& update, vector<uint256>& vActive, vector<uint256>& vDeactive)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);

    auto it = mapForkSched.find(update.hashFork);
    if (it != mapForkSched.end() && !it->second.IsJointEmpty())
    {
        for (const CBlockEx& block : boost::adaptors::reverse(update.vBlockAddNew))
        {
            if (!block.IsExtended() && !block.IsVacant())
            {
                vector<uint256> vJointFork;
                it->second.GetJointFork(block.GetHash(), vJointFork);
                if (!vJointFork.empty())
                {
                    for (const uint256& fork : vJointFork)
                    {
                        if (GetValidForkCreatedHeight(hashPrimaryLastBlock, fork) >= 0)
                        {
                            const auto mt = mapForkSched.find(fork);
                            if (mt != mapForkSched.end() && mt->second.IsAllowed())
                            {
                                vActive.push_back(fork);
                            }
                        }
                    }
                }
            }
        }
    }
    if (update.hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        for (const CBlockEx& block : boost::adaptors::reverse(update.vBlockAddNew))
        {
            uint256 hashBlock = block.GetHash();
            for (const CTransaction& tx : block.vtx)
            {
                if (tx.sendTo.GetTemplateId().GetType() == TEMPLATE_FORK && !tx.vchData.empty())
                {
                    uint256 hashNewFork;
                    try
                    {
                        CBlock blockOrigin;
                        CBufStream ss;
                        ss.Write((const char*)&tx.vchData[0], tx.vchData.size());
                        ss >> blockOrigin;
                        hashNewFork = blockOrigin.GetHash();
                    }
                    catch (...)
                    {
                        continue;
                    }
                    if (GetValidForkCreatedHeight(hashBlock, hashNewFork) >= 0)
                    {
                        const auto it = mapForkSched.find(hashNewFork);
                        if (it != mapForkSched.end() && it->second.IsAllowed())
                        {
                            vActive.push_back(hashNewFork);
                        }
                    }
                }
            }
        }

        map<uint256, bool> mapValidFork;
        ListValidFork(mapValidFork);

        auto it = setCurValidFork.begin();
        while (it != setCurValidFork.end())
        {
            auto mt = mapValidFork.find(*it);
            if (mt == mapValidFork.end() || !mt->second)
            {
                vDeactive.push_back(*it);
                setCurValidFork.erase(it++);
            }
            else
            {
                ++it;
            }
        }

        for (const auto& vd : mapValidFork)
        {
            if (vd.second && setCurValidFork.count(vd.first) == 0)
            {
                setCurValidFork.insert(vd.first);
            }
        }
    }
}

void CForkManager::GetValidForkList(map<uint256, bool>& mapFork)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    ListValidFork(mapFork);
}

bool CForkManager::GetSubline(const uint256& hashFork, vector<pair<int, uint256>>& vSubline) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);

    for (const auto& vd : mapForkSched)
    {
        if (vd.second.ctxtFork.hashParent == hashFork)
        {
            vSubline.push_back(make_pair(vd.second.ctxtFork.nJointHeight, vd.second.ctxtFork.hashFork));
        }
    }
    return true;
}

int64 CForkManager::ForkLockedCoin(const uint256& hashFork, const uint256& hashBlock)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);

    const auto it = mapForkSched.find(hashFork);
    if (it != mapForkSched.end())
    {
        int nHeight = GetValidForkCreatedHeight(hashBlock, hashFork);
        if (nHeight < 0)
        {
            return -1;
        }
        int nForkValidHeight = CBlock::GetBlockHeightByHash(hashBlock) - nHeight;
        if (nForkValidHeight < 0)
        {
            nForkValidHeight = 0;
        }
        return CTemplateFork::LockedCoin(nForkValidHeight);
    }
    return -1;
}

int CForkManager::GetForkCreatedHeight(const uint256& hashFork)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    return GetValidForkCreatedHeight(hashPrimaryLastBlock, hashFork);
}

//-----------------------------------------------------------------------------------------------
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

int CForkManager::GetValidForkCreatedHeight(const uint256& hashBlock, const uint256& hashFork)
{
    const auto it = mapBlockValidFork.find(hashBlock);
    if (it != mapBlockValidFork.end())
    {
        int nCreaatedHeight = it->second.GetCreatedHeight(hashFork);
        if (nCreaatedHeight >= 0)
        {
            return nCreaatedHeight;
        }
        if (it->second.hashRefFdBlock != 0)
        {
            const auto mt = mapBlockValidFork.find(it->second.hashRefFdBlock);
            if (mt != mapBlockValidFork.end())
            {
                return mt->second.GetCreatedHeight(hashFork);
            }
        }
    }
    return -1;
}

bool CForkManager::AddDbForkContext(const CForkContext& ctxt, vector<uint256>& vActive)
{
    CForkSchedule& sched = mapForkSched[ctxt.hashFork];
    sched.ctxtFork = ctxt;
    sched.fAllowed = IsAllowedFork(ctxt.hashFork, ctxt.hashParent);

    if (ctxt.hashParent != 0)
    {
        mapForkSched[ctxt.hashParent].AddNewJoint(ctxt.hashJoint, ctxt.hashFork);
    }

    if (ctxt.hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        vActive.push_back(ctxt.hashFork);
        setCurValidFork.insert(ctxt.hashFork);
    }
    else
    {
        if (sched.fAllowed && GetValidForkCreatedHeight(hashPrimaryLastBlock, ctxt.hashFork) >= 0)
        {
            vActive.push_back(ctxt.hashFork);
            setCurValidFork.insert(ctxt.hashFork);
        }
    }
    return true;
}

bool CForkManager::GetValidFdForkId(const uint256& hashBlock, map<uint256, int>& mapFdForkIdOut)
{
    const auto it = mapBlockValidFork.find(hashBlock);
    if (it != mapBlockValidFork.end())
    {
        if (it->second.hashRefFdBlock != 0)
        {
            const auto mt = mapBlockValidFork.find(it->second.hashRefFdBlock);
            if (mt != mapBlockValidFork.end() && !mt->second.mapForkId.empty())
            {
                mapFdForkIdOut.insert(mt->second.mapForkId.begin(), mt->second.mapForkId.end());
            }
        }
        if (!it->second.mapForkId.empty())
        {
            mapFdForkIdOut.insert(it->second.mapForkId.begin(), it->second.mapForkId.end());
        }
        return true;
    }
    return false;
}

void CForkManager::ListValidFork(std::map<uint256, bool>& mapFork)
{
    map<uint256, int> mapValidFork;
    if (GetValidFdForkId(hashPrimaryLastBlock, mapValidFork))
    {
        for (const auto& vd : mapValidFork)
        {
            bool fAllowed = false;
            const auto it = mapForkSched.find(vd.first);
            if (it != mapForkSched.end())
            {
                fAllowed = it->second.fAllowed;
            }
            mapFork.insert(make_pair(vd.first, fAllowed));
        }
    }
}

} // namespace bigbang
