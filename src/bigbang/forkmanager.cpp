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

bool CForkManager::LoadForkContext(vector<uint256>& vActive)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);

    vector<CForkContext> vForkCtxt;
    map<uint256, pair<uint256, map<uint256, int>>> mapValidForkId;
    if (!pBlockChain->ListForkContext(vForkCtxt, mapValidForkId))
    {
        return false;
    }

    uint256 hashPrimaryLastBlock;
    int nTempHeight;
    int64 nTempTime;
    uint16 nTempMintType;
    if (!pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashPrimaryLastBlock, nTempHeight, nTempTime, nTempMintType))
    {
        return false;
    }

    for (const auto& vd : mapValidForkId)
    {
        mapBlockValidFork.insert(make_pair(vd.first, CValidFdForkId(vd.second.first, vd.second.second)));
    }

    for (const CForkContext& ctxt : vForkCtxt)
    {
        if (!AddDbForkContext(hashPrimaryLastBlock, ctxt, vActive))
        {
            return false;
        }
    }
    return true;
}

bool CForkManager::VerifyFork(const uint256& hashPrevBlock, const uint256& hashFork, const string& strForkName)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    map<uint256, int> mapValidFork;
    if (GetValidFdForkId(hashPrevBlock, mapValidFork))
    {
        if (mapValidFork.count(hashFork) > 0)
        {
            return false;
        }
        for (const auto& vd : mapValidFork)
        {
            const auto mt = mapForkSched.find(vd.first);
            if (mt != mapForkSched.end() && mt->second.ctxtFork.strName == strForkName)
            {
                return false;
            }
        }
    }
    return true;
}

bool CForkManager::AddForkContext(const uint256& hashPrevBlock, const uint256& hashNewBlock, const vector<CForkContext>& vForkCtxt,
                                  bool fCheckPointBlock, uint256& hashRefFdBlock, map<uint256, int>& mapValidFork)
{
    CValidFdForkId& fd = mapBlockValidFork[hashNewBlock];
    if (fCheckPointBlock)
    {
        fd.mapForkId.clear();
        if (!GetValidFdForkId(hashPrevBlock, fd.mapForkId))
        {
            mapBlockValidFork.erase(hashNewBlock);
            return false;
        }
        fd.hashRefFdBlock = uint256();
    }
    else
    {
        const auto it = mapBlockValidFork.find(hashPrevBlock);
        if (it == mapBlockValidFork.end())
        {
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
        if (mapForkSched.find(ctxt.hashFork) == mapForkSched.end())
        {
            CForkSchedule& sched = mapForkSched[ctxt.hashFork];
            sched.ctxtFork = ctxt;
            sched.fAllowed = IsAllowedFork(ctxt.hashFork, ctxt.hashParent);

            mapForkSched[ctxt.hashParent].AddNewJoint(ctxt.hashJoint, ctxt.hashFork);
        }
        fd.mapForkId.insert(make_pair(ctxt.hashFork, CBlock::GetBlockHeightByHash(hashNewBlock)));
    }
    hashRefFdBlock = fd.hashRefFdBlock;
    mapValidFork.clear();
    mapValidFork.insert(fd.mapForkId.begin(), fd.mapForkId.end());
    return true;
}

void CForkManager::ForkUpdate(const CBlockChainUpdate& update, vector<uint256>& vActive, vector<uint256>& vDeactive)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);

    auto it = mapForkSched.find(update.hashFork);
    if (it != mapForkSched.end() && !it->second.IsJointEmpty())
    {
        uint256 hashPrimaryLastBlock;
        for (const CBlockEx& block : boost::adaptors::reverse(update.vBlockAddNew))
        {
            if (!block.IsExtended() && !block.IsVacant())
            {
                vector<uint256> vJointFork;
                it->second.GetJointFork(block.GetHash(), vJointFork);
                if (!vJointFork.empty())
                {
                    if (hashPrimaryLastBlock == 0)
                    {
                        int nTempHeight;
                        int64 nTempTime;
                        uint16 nTempMintType;
                        if (!pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashPrimaryLastBlock, nTempHeight, nTempTime, nTempMintType))
                        {
                            hashPrimaryLastBlock = 0;
                        }
                    }
                    if (hashPrimaryLastBlock != 0)
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
                            const CForkSchedule& ctxt = it->second;
                            uint256 hashJointBlock;
                            int64 nJointTime;
                            if (pBlockChain->GetLastBlockOfHeight(ctxt.ctxtFork.hashParent,
                                                                  CBlock::GetBlockHeightByHash(ctxt.ctxtFork.hashJoint),
                                                                  hashJointBlock, nJointTime)
                                && hashJointBlock == ctxt.ctxtFork.hashJoint)
                            {
                                vActive.push_back(hashNewFork);
                            }
                        }
                    }
                }
            }
        }
    }
    if (it != mapForkSched.end() && !it->second.IsAllowed())
    {
        vDeactive.push_back(update.hashFork);
    }
}

bool CForkManager::AddDbForkContext(const uint256& hashPrimaryLastBlock, const CForkContext& ctxt, vector<uint256>& vActive)
{
    CForkSchedule& sched = mapForkSched[ctxt.hashFork];
    sched.ctxtFork = ctxt;
    sched.fAllowed = IsAllowedFork(ctxt.hashFork, ctxt.hashParent);

    mapForkSched[ctxt.hashParent].AddNewJoint(ctxt.hashJoint, ctxt.hashFork);

    if (ctxt.hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        vActive.push_back(ctxt.hashFork);
    }
    else
    {
        if (sched.fAllowed && GetValidForkCreatedHeight(hashPrimaryLastBlock, ctxt.hashFork) >= 0)
        {
            vActive.push_back(ctxt.hashFork);
        }
    }
    return true;
}

void CForkManager::GetForkList(vector<uint256>& vFork)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);

    uint256 hashPrimaryLastBlock;
    int nTempHeight;
    int64 nTempTime;
    uint16 nTempMintType;
    if (pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashPrimaryLastBlock, nTempHeight, nTempTime, nTempMintType))
    {
        map<uint256, int> mapValidFork;
        if (!GetValidFdForkId(hashPrimaryLastBlock, mapValidFork))
        {
            return;
        }
        for (const auto& vd : mapValidFork)
        {
            vFork.push_back(vd.first);
        }
    }
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
        uint256 hashValidBlock;
        if (hashBlock == 0)
        {
            int nTempHeight;
            int64 nTempTime;
            uint16 nTempMintType;
            if (!pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashValidBlock, nTempHeight, nTempTime, nTempMintType))
            {
                return CTemplateFork::CreatedCoin();
            }
        }
        else
        {
            hashValidBlock = hashBlock;
        }

        int nHeight = GetValidForkCreatedHeight(hashValidBlock, hashFork);
        if (nHeight < 0)
        {
            if (hashBlock == 0)
            {
                return -1;
            }
            return 0;
        }

        int nForkValidHeight = CBlock::GetBlockHeightByHash(hashValidBlock) - nHeight;
        if (nForkValidHeight < 0)
        {
            nForkValidHeight = 0;
        }

        return CTemplateFork::LockedCoin(nForkValidHeight);
    }
    if (hashBlock == 0)
    {
        return -1;
    }
    return 0;
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

} // namespace bigbang
