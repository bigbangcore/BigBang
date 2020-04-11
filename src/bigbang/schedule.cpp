// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "schedule.h"

using namespace std;
using namespace xengine;

namespace bigbang
{

///////////////////////////////
// COrphan

size_t COrphan::GetSize()
{
    return mapOrphanByPrev.size();
}

void COrphan::AddNew(const uint256& prev, const uint256& hash)
{
    mapOrphanByPrev.insert(make_pair(prev, hash));
}

void COrphan::Remove(const uint256& hash)
{
    multimap<uint256, uint256>::iterator it = mapOrphanByPrev.begin();
    while (it != mapOrphanByPrev.end())
    {
        if ((*it).second == hash)
        {
            mapOrphanByPrev.erase(it++);
        }
        else
        {
            ++it;
        }
    }
}

void COrphan::GetNext(const uint256& prev, vector<uint256>& vNext)
{
    for (multimap<uint256, uint256>::iterator it = mapOrphanByPrev.lower_bound(prev);
         it != mapOrphanByPrev.upper_bound(prev); ++it)
    {
        vNext.push_back((*it).second);
    }
}

void COrphan::GetNext(const uint256& prev, vector<uint256>& vNext, set<uint256>& setHash)
{
    for (multimap<uint256, uint256>::iterator it = mapOrphanByPrev.lower_bound(prev);
         it != mapOrphanByPrev.upper_bound(prev); ++it)
    {
        if (setHash.insert((*it).second).second)
        {
            vNext.push_back((*it).second);
        }
    }
}

void COrphan::RemoveNext(const uint256& prev)
{
    mapOrphanByPrev.erase(prev);
}

void COrphan::RemoveBranch(const uint256& root, std::vector<uint256>& vBranch)
{
    set<uint256> setBranch;
    vBranch.reserve(mapOrphanByPrev.size());
    GetNext(root, vBranch, setBranch);
    mapOrphanByPrev.erase(root);

    for (size_t i = 0; i < vBranch.size(); i++)
    {
        uint256 hash = vBranch[i];
        GetNext(hash, vBranch, setBranch);
        mapOrphanByPrev.erase(hash);
    }
}

///////////////////////////////
// CSchedule

bool CSchedule::Exists(const network::CInv& inv)
{
    return (!!mapState.count(inv));
}

bool CSchedule::CheckPrevTxInv(const network::CInv& inv)
{
    map<network::CInv, CInvState>::iterator it = mapState.find(inv);
    if (it != mapState.end())
    {
        if (!it->second.IsReceived())
        {
            setMissPrevTxInv.insert(inv);
        }
        return true;
    }
    setMissPrevTxInv.insert(inv);
    return false;
}

void CSchedule::GetKnownPeer(const network::CInv& inv, set<uint64>& setKnownPeer)
{
    map<network::CInv, CInvState>::iterator it = mapState.find(inv);
    if (it != mapState.end())
    {
        setKnownPeer = (*it).second.setKnownPeer;
    }
}

void CSchedule::RemovePeer(uint64 nPeerNonce, set<uint64>& setSchedPeer)
{
    map<uint64, CInvPeer>::iterator it = mapPeer.find(nPeerNonce);
    if (it != mapPeer.end())
    {
        vector<network::CInv> vInvKnown;
        (*it).second.GetKnownInv(vInvKnown);
        for (const network::CInv& inv : vInvKnown)
        {
            CInvState& state = mapState[inv];
            state.setKnownPeer.erase(nPeerNonce);
            if (state.setKnownPeer.empty())
            {
                RemoveOrphan(inv);
                setMissPrevTxInv.erase(inv);
                mapState.erase(inv);
            }
            else if (state.nAssigned == nPeerNonce)
            {
                state.nAssigned = 0;
                state.objReceived = CNil();
                setSchedPeer.insert(state.setKnownPeer.begin(), state.setKnownPeer.end());
            }
        }
        mapPeer.erase(it);
    }
}

bool CSchedule::CheckAddInvIdleLocation(uint64 nPeerNonce, uint32 nInvType)
{
    if (mapState.size() < MAX_INV_COUNT
        && mapPeer[nPeerNonce].GetCount(nInvType) < (nInvType == network::CInv::MSG_TX ? MAX_PEER_TX_INV_COUNT : MAX_PEER_BLOCK_INV_COUNT))
    {
        return true;
    }
    return false;
}

bool CSchedule::AddNewInv(const network::CInv& inv, uint64 nPeerNonce)
{
    if (CheckAddInvIdleLocation(nPeerNonce, inv.nType))
    {
        CInvState& state = mapState[inv];
        state.setKnownPeer.insert(nPeerNonce);
        if (state.nRecvInvTime == 0)
        {
            state.nRecvInvTime = GetTime();
        }
        mapPeer[nPeerNonce].AddNewInv(inv);
        return true;
    }
    return false;
}

bool CSchedule::RemoveInv(const network::CInv& inv, set<uint64>& setKnownPeer)
{
    map<network::CInv, CInvState>::iterator it = mapState.find(inv);
    if (it != mapState.end())
    {
        for (const uint64& nPeerNonce : (*it).second.setKnownPeer)
        {
            mapPeer[nPeerNonce].RemoveInv(inv);
        }
        if ((*it).second.IsReceived())
        {
            if (inv.nType == network::CInv::MSG_BLOCK)
            {
                CBlock& block = boost::get<CBlock>((*it).second.objReceived);
                if (block.IsPrimary() && block.IsProofOfWork())
                {
                    RemoveHeightBlock(block.GetBlockHeight(), inv.nHash);
                }
            }
            RemoveOrphan(inv);
        }
        setMissPrevTxInv.erase(inv);
        setKnownPeer.insert((*it).second.setKnownPeer.begin(), (*it).second.setKnownPeer.end());
        mapState.erase(it);
        return true;
    }
    return false;
}

bool CSchedule::ReceiveBlock(uint64 nPeerNonce, const uint256& hash, const CBlock& block, set<uint64>& setSchedPeer)
{
    map<network::CInv, CInvState>::iterator it = mapState.find(network::CInv(network::CInv::MSG_BLOCK, hash));
    if (it != mapState.end())
    {
        CInvState& state = (*it).second;
        if (state.nAssigned == nPeerNonce && !state.IsReceived())
        {
            state.objReceived = block;
            state.nRecvObjTime = GetTime();
            state.nClearObjTime = GetTime() + MAX_OBJ_WAIT_TIME;
            setSchedPeer.insert(state.setKnownPeer.begin(), state.setKnownPeer.end());
            mapPeer[nPeerNonce].Completed((*it).first);
            if (block.IsPrimary() && block.IsProofOfWork())
            {
                mapHeightBlock[block.GetBlockHeight()].push_back(make_pair(hash, 1));
            }
            return true;
        }
    }
    return false;
}

bool CSchedule::ReceiveTx(uint64 nPeerNonce, const uint256& txid, const CTransaction& tx, set<uint64>& setSchedPeer)
{
    map<network::CInv, CInvState>::iterator it = mapState.find(network::CInv(network::CInv::MSG_TX, txid));
    if (it != mapState.end())
    {
        CInvState& state = (*it).second;
        if (state.nAssigned == nPeerNonce && !state.IsReceived())
        {
            state.objReceived = tx;
            state.nRecvObjTime = GetTime();
            state.nClearObjTime = GetTime() + MAX_OBJ_WAIT_TIME;
            setSchedPeer.insert(state.setKnownPeer.begin(), state.setKnownPeer.end());
            mapPeer[nPeerNonce].Completed((*it).first);
            setMissPrevTxInv.erase((*it).first);
            return true;
        }
    }
    return false;
}

CBlock* CSchedule::GetBlock(const uint256& hash, uint64& nNonceSender)
{
    map<network::CInv, CInvState>::iterator it = mapState.find(network::CInv(network::CInv::MSG_BLOCK, hash));
    if (it != mapState.end())
    {
        CInvState& state = (*it).second;
        if (state.IsReceived())
        {
            nNonceSender = state.nAssigned;
            return &(boost::get<CBlock>(state.objReceived));
        }
    }
    return nullptr;
}

CTransaction* CSchedule::GetTransaction(const uint256& txid, uint64& nNonceSender)
{
    map<network::CInv, CInvState>::iterator it = mapState.find(network::CInv(network::CInv::MSG_TX, txid));
    if (it != mapState.end())
    {
        CInvState& state = (*it).second;
        if (state.IsReceived())
        {
            nNonceSender = state.nAssigned;
            return &(boost::get<CTransaction>(state.objReceived));
        }
    }
    return nullptr;
}

void CSchedule::AddOrphanBlockPrev(const uint256& hash, const uint256& prev)
{
    orphanBlock.AddNew(prev, hash);
}

void CSchedule::AddOrphanTxPrev(const uint256& txid, const uint256& prev)
{
    orphanTx.AddNew(prev, txid);
}

void CSchedule::GetNextBlock(const uint256& hash, vector<uint256>& vNext)
{
    orphanBlock.GetNext(hash, vNext);
}

void CSchedule::GetNextTx(const uint256& txid, vector<uint256>& vNext, set<uint256>& setTx)
{
    orphanTx.GetNext(txid, vNext, setTx);
}

void CSchedule::InvalidateBlock(const uint256& hash, set<uint64>& setMisbehavePeer)
{
    vector<uint256> vInvalid;
    orphanBlock.RemoveBranch(hash, vInvalid);
    for (const uint256& hashInvalid : vInvalid)
    {
        network::CInv inv(network::CInv::MSG_BLOCK, hashInvalid);
        map<network::CInv, CInvState>::iterator it = mapState.find(inv);
        if (it != mapState.end())
        {
            CInvState& state = (*it).second;
            for (const uint64& nPeerNonce : state.setKnownPeer)
            {
                mapPeer[nPeerNonce].RemoveInv(inv);
            }
            setMisbehavePeer.insert(state.setKnownPeer.begin(), state.setKnownPeer.end());
            mapState.erase(it);
        }
    }
    RemoveInv(network::CInv(network::CInv::MSG_BLOCK, hash), setMisbehavePeer);
}

void CSchedule::InvalidateTx(const uint256& txid, set<uint64>& setMisbehavePeer)
{
    vector<uint256> vInvalid;
    orphanTx.RemoveBranch(txid, vInvalid);
    for (const uint256& hashInvalid : vInvalid)
    {
        network::CInv inv(network::CInv::MSG_TX, hashInvalid);
        map<network::CInv, CInvState>::iterator it = mapState.find(inv);
        if (it != mapState.end())
        {
            CInvState& state = (*it).second;
            for (const uint64& nPeerNonce : state.setKnownPeer)
            {
                mapPeer[nPeerNonce].RemoveInv(inv);
            }
            setMissPrevTxInv.erase(inv);
            setMisbehavePeer.insert(state.setKnownPeer.begin(), state.setKnownPeer.end());
            mapState.erase(it);
        }
    }
    RemoveInv(network::CInv(network::CInv::MSG_TX, txid), setMisbehavePeer);
}

bool CSchedule::ScheduleBlockInv(uint64 nPeerNonce, vector<network::CInv>& vInv, size_t nMaxCount, bool& fMissingPrev, bool& fEmpty)
{
    fMissingPrev = false;
    fEmpty = true;

    map<uint64, CInvPeer>::iterator it = mapPeer.find(nPeerNonce);
    if (it != mapPeer.end())
    {
        CInvPeer& peer = (*it).second;
        fEmpty = peer.Empty(network::CInv::MSG_BLOCK);
        if (!peer.IsAssigned())
        {
            bool fReceivedAll;
            if (!ScheduleKnownInv(nPeerNonce, peer, network::CInv::MSG_BLOCK, vInv, nMaxCount, fReceivedAll))
            {
                if (fReceivedAll && peer.CheckNextGetBlocksTime() && CheckAddInvIdleLocation(nPeerNonce, network::CInv::MSG_BLOCK))
                {
                    fMissingPrev = true;
                }
                return (!fReceivedAll || peer.GetCount(network::CInv::MSG_BLOCK) < MAX_PEER_BLOCK_INV_COUNT);
            }
            else
            {
                if (fEmpty && peer.CheckNextGetBlocksTime())
                {
                    fMissingPrev = true;
                }
            }
        }
    }
    return true;
}

bool CSchedule::ScheduleTxInv(uint64 nPeerNonce, vector<network::CInv>& vInv, size_t nMaxCount, bool& fReceivedAll)
{
    map<uint64, CInvPeer>::iterator it = mapPeer.find(nPeerNonce);
    if (it != mapPeer.end() && !(*it).second.IsAssigned())
    {
        CInvPeer& peer = (*it).second;
        if (!ScheduleKnownInv(nPeerNonce, peer, network::CInv::MSG_TX, vInv, nMaxCount, fReceivedAll))
        {
            return (!fReceivedAll || peer.GetCount(network::CInv::MSG_TX) < MAX_PEER_TX_INV_COUNT);
        }
        else
        {
            if (peer.Empty(network::CInv::MSG_TX))
            {
                fReceivedAll = true;
            }
        }
    }
    return true;
}

bool CSchedule::CancelAssignedInv(uint64 nPeerNonce, const network::CInv& inv)
{
    map<network::CInv, CInvState>::iterator it = mapState.find(inv);
    if (it == mapState.end())
    {
        StdWarn("Schedule", "CancelAssignedInv: find inv fail, peer nonce: %ld, inv: [%d] %s", nPeerNonce, inv.nType, inv.nHash.GetHex().c_str());
        return false;
    }
    CInvState& state = (*it).second;
    if (state.nAssigned != nPeerNonce)
    {
        StdWarn("Schedule", "CancelAssignedInv: state.nAssigned != nPeerNonce, peer nonce: %ld, inv: [%d] %s", nPeerNonce, inv.nType, inv.nHash.GetHex().c_str());
        return false;
    }
    if (!state.IsReceived())
    {
        state.nAssigned = 0;
        state.setKnownPeer.erase(nPeerNonce);
        if (state.setKnownPeer.empty())
        {
            RemoveOrphan(inv);
            setMissPrevTxInv.erase(inv);
            mapState.erase(it);
        }
    }

    map<uint64, CInvPeer>::iterator mt = mapPeer.find(nPeerNonce);
    if (mt == mapPeer.end())
    {
        StdWarn("Schedule", "CancelAssignedInv: find peer fail, peer nonce: %ld, inv: [%d] %s", nPeerNonce, inv.nType, inv.nHash.GetHex().c_str());
        return false;
    }
    (*mt).second.RemoveInv(inv);
    return true;
}

bool CSchedule::GetLocatorDepthHash(uint64 nPeerNonce, uint256& hashDepth)
{
    map<uint64, CInvPeer>::iterator it = mapPeer.find(nPeerNonce);
    if (it != mapPeer.end())
    {
        it->second.GetBlockLocatorDepth(hashDepth);
        return true;
    }
    return false;
}

void CSchedule::SetLocatorDepthHash(uint64 nPeerNonce, const uint256& hashDepth)
{
    mapPeer[nPeerNonce].SetBlockLocatorDepth(hashDepth);
}

int CSchedule::GetLocatorInvBlockHash(uint64 nPeerNonce, uint256& hashBlock)
{
    map<uint64, CInvPeer>::iterator it = mapPeer.find(nPeerNonce);
    if (it != mapPeer.end())
    {
        return it->second.GetLocatorInvBlockHash(hashBlock);
    }
    return -1;
}

void CSchedule::SetLocatorInvBlockHash(uint64 nPeerNonce, int nHeight, const uint256& hashBlock, const uint256& hashNext)
{
    mapPeer[nPeerNonce].SetLocatorInvBlockHash(nHeight, hashBlock, hashNext);
}

void CSchedule::SetNextGetBlocksTime(uint64 nPeerNonce, int nWaitTime)
{
    mapPeer[nPeerNonce].SetNextGetBlocksTime(nWaitTime);
}

bool CSchedule::SetRepeatBlock(uint64 nNonce, const uint256& hash)
{
    map<network::CInv, CInvState>::iterator it = mapState.find(network::CInv(network::CInv::MSG_BLOCK, hash));
    if (it != mapState.end())
    {
        it->second.fRepeatMintBlock = true;
        it->second.nClearObjTime = GetTime() + MAX_REPEAT_BLOCK_TIME;
    }
    if (mapPeer[nNonce].AddRepeatBlock(hash) >= MAX_REPEAT_BLOCK_COUNT)
    {
        return false;
    }
    return true;
}

bool CSchedule::IsRepeatBlock(const uint256& hash)
{
    map<network::CInv, CInvState>::iterator it = mapState.find(network::CInv(network::CInv::MSG_BLOCK, hash));
    if (it != mapState.end() && it->second.IsReceived() && it->second.fRepeatMintBlock)
    {
        return true;
    }
    return false;
}

void CSchedule::AddRefBlock(const uint256& hashRefBlock, const uint256& hashFork, const uint256& hashBlock)
{
    mapRefBlock.insert(make_pair(hashRefBlock, make_pair(hashFork, hashBlock)));
}

void CSchedule::RemoveRefBlock(const uint256& hash)
{
    multimap<uint256, pair<uint256, uint256>>::iterator it = mapRefBlock.begin();
    while (it != mapRefBlock.end())
    {
        if (it->second.second == hash)
        {
            mapRefBlock.erase(it++);
        }
        else
        {
            ++it;
        }
    }
}

void CSchedule::GetNextRefBlock(const uint256& hashRefBlock, vector<pair<uint256, uint256>>& vNext)
{
    for (multimap<uint256, pair<uint256, uint256>>::iterator it = mapRefBlock.lower_bound(hashRefBlock);
         it != mapRefBlock.upper_bound(hashRefBlock); ++it)
    {
        vNext.push_back(it->second);
    }
}

bool CSchedule::SetDelayedClear(const network::CInv& inv, int64 nDelayedTime)
{
    map<network::CInv, CInvState>::iterator it = mapState.find(inv);
    if (it != mapState.end())
    {
        it->second.nClearObjTime = GetTime() + nDelayedTime;
        return true;
    }
    return false;
}

bool CSchedule::GetSubmitCachePowBlock(const CConsensusParam& consParam, std::vector<std::pair<uint256, int>>& vPowBlockHash)
{
    for (auto it = mapHeightBlock.begin(); it != mapHeightBlock.end(); ++it)
    {
        if (it->first <= consParam.nPrevHeight)
        {
            for (auto& chash : it->second)
            {
                vPowBlockHash.push_back(chash);
            }
        }
        else if (consParam.ret && it->first == consParam.nPrevHeight + 1)
        {
            if (consParam.fPow || consParam.nWaitTime < -10)
            {
                for (auto& chash : it->second)
                {
                    vPowBlockHash.push_back(chash);
                }
            }
        }
    }
    return true;
}

bool CSchedule::GetFirstCachePowBlock(int nHeight, uint256& hashFirstBlock)
{
    auto it = mapHeightBlock.find(nHeight);
    if (it != mapHeightBlock.end() && it->second.size() > 0)
    {
        hashFirstBlock = it->second[0].first;
        return true;
    }
    return false;
}

bool CSchedule::AddCacheLocalPowBlock(const CBlock& block, bool& fFirst)
{
    int nHeight = block.GetBlockHeight();

    auto mt = mapKcPowBlock.begin();
    while (mt != mapKcPowBlock.end())
    {
        if (mt->first > nHeight - 32)
        {
            break;
        }
        RemoveHeightBlock(mt->first, mt->second.GetHash());
        mapKcPowBlock.erase(mt++);
    }

    auto it = mapKcPowBlock.find(nHeight);
    if (it == mapKcPowBlock.end())
    {
        mapKcPowBlock.insert(make_pair(nHeight, block));
        auto& vb = mapHeightBlock[nHeight];
        fFirst = (vb.size() == 0);
        vb.push_back(make_pair(block.GetHash(), 0));
        return true;
    }
    return false;
}

bool CSchedule::CheckCacheLocalPowBlock(int nHeight)
{
    if (mapKcPowBlock.find(nHeight) != mapKcPowBlock.end())
    {
        return true;
    }
    return false;
}

bool CSchedule::GetCacheLocalPowBlock(const uint256& hash, CBlock& block)
{
    auto it = mapKcPowBlock.find(CBlock::GetBlockHeightByHash(hash));
    if (it != mapKcPowBlock.end() && it->second.GetHash() == hash)
    {
        block = it->second;
        return true;
    }
    return false;
}

void CSchedule::RemoveCacheLocalPowBlock(const uint256& hash)
{
    auto it = mapKcPowBlock.find(CBlock::GetBlockHeightByHash(hash));
    if (it != mapKcPowBlock.end() && it->second.GetHash() == hash)
    {
        RemoveHeightBlock(it->first, it->second.GetHash());
        mapKcPowBlock.erase(it);
    }
}

bool CSchedule::GetCachePowBlock(const uint256& hash, CBlock& block)
{
    auto it = mapHeightBlock.find(CBlock::GetBlockHeightByHash(hash));
    if (it != mapHeightBlock.end())
    {
        if (it->second.size() > 0)
        {
            auto& chash = it->second[0];
            if (chash.second == 1)
            {
                if (chash.first == hash)
                {
                    uint64 nNonceSender = 0;
                    CBlock* pBlock = GetBlock(hash, nNonceSender);
                    if (pBlock)
                    {
                        block = *pBlock;
                        return true;
                    }
                }
            }
            else
            {
                return GetCacheLocalPowBlock(hash, block);
            }
        }
    }
    return false;
}

void CSchedule::RemoveHeightBlock(int nHeight, const uint256& hash)
{
    auto it = mapHeightBlock.find(nHeight);
    if (it != mapHeightBlock.end())
    {
        for (int i = it->second.size() - 1; i >= 0; i--)
        {
            auto& chash = it->second[i];
            if (chash.first == hash)
            {
                it->second.erase(it->second.begin() + i);
            }
        }
        if (it->second.empty())
        {
            mapHeightBlock.erase(it);
        }
    }
}

void CSchedule::RemoveOrphan(const network::CInv& inv)
{
    if (inv.nType == network::CInv::MSG_TX)
    {
        orphanTx.Remove(inv.nHash);
    }
    else if (inv.nType == network::CInv::MSG_BLOCK)
    {
        orphanBlock.Remove(inv.nHash);
        RemoveRefBlock(inv.nHash);
    }
}

bool CSchedule::ScheduleKnownInv(uint64 nPeerNonce, CInvPeer& peer, uint32 type,
                                 vector<network::CInv>& vInv, size_t nMaxCount, bool& fReceivedAll)
{
    size_t nReceived = 0;
    int64 nCurTime = GetTime();
    set<network::CInv> setRemoveInv;
    set<network::CInv> setRemovePeerInv;
    CUInt256List& listKnown = peer.GetKnownList(type);

    vInv.clear();
    if (type == network::CInv::MSG_TX && !setMissPrevTxInv.empty())
    {
        std::set<network::CInv>::iterator mt = setMissPrevTxInv.begin();
        while (mt != setMissPrevTxInv.end())
        {
            const network::CInv& inv = *mt;
            map<network::CInv, CInvState>::iterator it = mapState.find(inv);
            if (it != mapState.end())
            {
                CInvState& state = it->second;
                if (state.nAssigned == 0 && peer.KnownInvExists(inv))
                {
                    if (state.nGetDataCount >= MAX_REGETDATA_COUNT
                        || (state.nGetDataCount >= 1 && nCurTime - state.nRecvInvTime >= MAX_INV_WAIT_TIME)
                        || nCurTime - state.nRecvInvTime >= MAX_INV_WAIT_TIME * 12)
                    {
                        StdLog("Schedule", "ScheduleKnownInv: inv timeout, peer nonce: %ld, inv: [%d] %s, getcount: %d, waittime: %ld",
                               nPeerNonce, inv.nType, inv.nHash.GetHex().c_str(), state.nGetDataCount, nCurTime - state.nRecvInvTime);
                        setRemoveInv.insert(inv);
                        ++mt;
                        continue;
                    }
                    state.nAssigned = nPeerNonce;
                    vInv.push_back(inv);
                    peer.Assign(inv);
                    state.nGetDataCount++;
                    if (vInv.size() >= nMaxCount)
                    {
                        break;
                    }
                }
                ++mt;
            }
            else
            {
                setMissPrevTxInv.erase(mt++);
            }
        }
    }
    if (vInv.size() < nMaxCount)
    {
        for (const uint256& hash : listKnown)
        {
            network::CInv inv(type, hash);
            map<network::CInv, CInvState>::iterator it = mapState.find(inv);
            if (it != mapState.end())
            {
                CInvState& state = it->second;
                if (state.nAssigned == 0)
                {
                    if (state.nGetDataCount >= MAX_REGETDATA_COUNT
                        || (state.nGetDataCount >= 1 && nCurTime - state.nRecvInvTime >= MAX_INV_WAIT_TIME)
                        || nCurTime - state.nRecvInvTime >= MAX_INV_WAIT_TIME * 12)
                    {
                        StdLog("Schedule", "ScheduleKnownInv: inv timeout, peer nonce: %ld, inv: [%d] %s, getcount: %d, waittime: %ld",
                               nPeerNonce, inv.nType, inv.nHash.GetHex().c_str(), state.nGetDataCount, nCurTime - state.nRecvInvTime);
                        setRemoveInv.insert(inv);
                        continue;
                    }
                    state.nAssigned = nPeerNonce;
                    vInv.push_back(inv);
                    peer.Assign(inv);
                    state.nGetDataCount++;
                    if (vInv.size() >= nMaxCount)
                    {
                        break;
                    }
                }
                else if (state.IsReceived())
                {
                    //if (nCurTime - state.nRecvObjTime >= MAX_OBJ_WAIT_TIME)
                    if (nCurTime >= state.nClearObjTime)
                    {
                        StdLog("Schedule", "ScheduleKnownInv: object timeout, peer nonce: %ld, inv: [%d] %s, waittime: %ld",
                               nPeerNonce, inv.nType, inv.nHash.GetHex().c_str(), nCurTime - state.nRecvObjTime);
                        setRemoveInv.insert(inv);
                        continue;
                    }
                    nReceived++;
                }
            }
            else
            {
                setRemovePeerInv.insert(inv);
            }
        }
    }
    if (!setRemoveInv.empty())
    {
        for (const network::CInv& inv : setRemoveInv)
        {
            set<uint64> setKnownPeer;
            RemoveInv(inv, setKnownPeer);
        }
    }
    if (!setRemovePeerInv.empty())
    {
        for (const network::CInv& inv : setRemovePeerInv)
        {
            peer.RemoveInv(inv);
        }
    }
    fReceivedAll = (nReceived == listKnown.size() && nReceived != 0);
    return (!vInv.empty() || listKnown.empty());
}

} // namespace bigbang
