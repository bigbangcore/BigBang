// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "schedule.h"

#include "util.h"

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

void CSchedule::AddNewInv(const network::CInv& inv, uint64 nPeerNonce)
{
    CInvPeer& peer = mapPeer[nPeerNonce];
    size_t nMaxPeerInv = (inv.nType == network::CInv::MSG_TX ? MAX_PEER_TX_INV_COUNT : MAX_PEER_BLOCK_INV_COUNT);
    if (mapState.size() < MAX_INV_COUNT && peer.GetCount(inv.nType) < nMaxPeerInv)
    {
        mapState[inv].setKnownPeer.insert(nPeerNonce);
        peer.AddNewInv(inv);
    }
}

void CSchedule::RemoveInv(const network::CInv& inv, set<uint64>& setKnownPeer)
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
            RemoveOrphan(inv);
        }
        setKnownPeer.insert((*it).second.setKnownPeer.begin(), (*it).second.setKnownPeer.end());
        mapState.erase(it);
    }
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
            setSchedPeer.insert(state.setKnownPeer.begin(), state.setKnownPeer.end());
            mapPeer[nPeerNonce].Completed((*it).first);
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
            setSchedPeer.insert(state.setKnownPeer.begin(), state.setKnownPeer.end());
            mapPeer[nPeerNonce].Completed((*it).first);
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
                fMissingPrev = fReceivedAll;
                return (!fReceivedAll || peer.GetCount(network::CInv::MSG_BLOCK) < MAX_PEER_BLOCK_INV_COUNT);
            }
        }
    }
    return true;
}

bool CSchedule::ScheduleTxInv(uint64 nPeerNonce, vector<network::CInv>& vInv, size_t nMaxCount)
{
    map<uint64, CInvPeer>::iterator it = mapPeer.find(nPeerNonce);
    if (it != mapPeer.end() && !(*it).second.IsAssigned())
    {
        CInvPeer& peer = (*it).second;

        bool fReceivedAll;

        if (!ScheduleKnownInv(nPeerNonce, peer, network::CInv::MSG_TX, vInv, nMaxCount, fReceivedAll))
        {
            return (!fReceivedAll || peer.GetCount(network::CInv::MSG_TX) < MAX_PEER_TX_INV_COUNT);
        }
    }
    return true;
}

bool CSchedule::CancelAssignedInv(uint64 nPeerNonce, const network::CInv& inv)
{
    CInvState& state = mapState[inv];
    if (state.nAssigned == nPeerNonce)
    {
        state.nAssigned = 0;

        map<uint64, CInvPeer>::iterator it = mapPeer.find(nPeerNonce);
        if (it != mapPeer.end())
        {
            (*it).second.Completed(inv);
        }
        else
        {
            StdWarn("Schedule", "CancelAssignedInv: find peer fail, peer nonce: %ld, inv: [%d] %s", nPeerNonce, inv.nType, inv.nHash.GetHex().c_str());
        }
    }
    else
    {
        StdWarn("Schedule", "CancelAssignedInv: state.nAssigned != nPeerNonce, peer nonce: %ld, inv: [%d] %s", nPeerNonce, inv.nType, inv.nHash.GetHex().c_str());
    }
    return true;
}

int CSchedule::GetLocatorDepth(uint64 nPeerNonce)
{
    map<uint64, CInvPeer>::iterator it = mapPeer.find(nPeerNonce);
    if (it != mapPeer.end())
    {
        return it->second.GetBlockLocatorDepth();
    }
    return 0;
}

void CSchedule::SetLocatorDepth(uint64 nPeerNonce, int nDepth)
{
    CInvPeer& peer = mapPeer[nPeerNonce];
    peer.SetBlockLocatorDepth(nDepth);
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

void CSchedule::SetLocatorInvBlockHash(uint64 nPeerNonce, int nHeight, const uint256& hashBlock)
{
    map<uint64, CInvPeer>::iterator it = mapPeer.find(nPeerNonce);
    if (it != mapPeer.end())
    {
        it->second.SetLocatorInvBlockHash(nHeight, hashBlock);
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
    }
}

bool CSchedule::ScheduleKnownInv(uint64 nPeerNonce, CInvPeer& peer, uint32 type,
                                 vector<network::CInv>& vInv, size_t nMaxCount, bool& fReceivedAll)
{
    size_t nReceived = 0;
    vInv.clear();
    CUInt256List& listKnown = peer.GetKnownList(type);
    for (const uint256& hash : listKnown)
    {
        network::CInv inv(type, hash);
        CInvState& state = mapState[inv];
        if (state.nAssigned == 0)
        {
            state.nAssigned = nPeerNonce;
            vInv.push_back(inv);
            peer.Assign(inv);
            if (vInv.size() >= nMaxCount)
            {
                break;
            }
        }
        else if (state.IsReceived())
        {
            nReceived++;
        }
    }
    fReceivedAll = (nReceived == listKnown.size() && nReceived != 0);
    return (!vInv.empty() || listKnown.empty());
}

} // namespace bigbang
