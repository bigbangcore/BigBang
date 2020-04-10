// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_SCHEDULE_H
#define BIGBANG_SCHEDULE_H

#include <boost/variant.hpp>

#include "block.h"
#include "proto.h"
#include "struct.h"
#include "transaction.h"
#include "util.h"

using namespace xengine;

namespace bigbang
{

class CInvPeer
{
    class CInvPeerState
    {
    public:
        CInvPeerState()
          : nNextGetBlocksTime(0) {}

        CUInt256List listKnown;
        std::set<uint256> setAssigned;
        int64 nNextGetBlocksTime;
        std::map<uint32, std::set<uint256>> mapRepeat;
    };

public:
    CInvPeer()
      : nInvHeight(0)
    {
    }
    ~CInvPeer()
    {
    }
    bool Empty(uint32 type)
    {
        return GetKnownList(type).empty();
    }
    std::size_t GetCount(uint32 type)
    {
        return GetKnownList(type).size();
    }
    CUInt256List& GetKnownList(uint32 type)
    {
        return invKnown[type - network::CInv::MSG_TX].listKnown;
    }
    std::set<uint256>& GetAssigned(uint32 type)
    {
        return invKnown[type - network::CInv::MSG_TX].setAssigned;
    }
    void GetKnownInv(std::vector<network::CInv>& vInv)
    {
        for (const uint256& hash : invKnown[1].listKnown)
        {
            vInv.push_back(network::CInv(network::CInv::MSG_BLOCK, hash));
        }
        for (const uint256& hash : invKnown[0].listKnown)
        {
            vInv.push_back(network::CInv(network::CInv::MSG_TX, hash));
        }
    }
    void AddNewInv(const network::CInv& inv)
    {
        CUInt256List& listKnown = GetKnownList(inv.nType);
        CUInt256ByValue& idxByValue = listKnown.get<1>();
        idxByValue.erase(inv.nHash);
        listKnown.push_back(inv.nHash);
    }
    void RemoveInv(const network::CInv& inv)
    {
        CUInt256List& listKnown = GetKnownList(inv.nType);
        CUInt256ByValue& idxByValue = listKnown.get<1>();
        idxByValue.erase(inv.nHash);
        GetAssigned(inv.nType).erase(inv.nHash);
        if (inv.nType == network::CInv::MSG_BLOCK)
        {
            std::map<uint32, std::set<uint256>>& mapData = invKnown[network::CInv::MSG_BLOCK - network::CInv::MSG_TX].mapRepeat;
            std::map<uint32, std::set<uint256>>::iterator it = mapData.find(CBlock::GetBlockHeightByHash(inv.nHash));
            if (it != mapData.end())
            {
                it->second.erase(inv.nHash);
                if (it->second.empty())
                {
                    mapData.erase(it);
                }
            }
        }
    }
    bool KnownInvExists(const network::CInv& inv)
    {
        CUInt256List& listKnown = GetKnownList(inv.nType);
        CUInt256ByValue& idxByValue = listKnown.get<1>();
        if (idxByValue.find(inv.nHash) != idxByValue.end())
        {
            return true;
        }
        return false;
    }
    void Assign(const network::CInv& inv)
    {
        GetAssigned(inv.nType).insert(inv.nHash);
    }
    void Completed(const network::CInv& inv)
    {
        GetAssigned(inv.nType).erase(inv.nHash);
    }
    bool IsAssigned()
    {
        return (!invKnown[0].setAssigned.empty() || !invKnown[1].setAssigned.empty());
    }
    void GetBlockLocatorDepth(uint256& hashDepth)
    {
        hashDepth = hashGetBlockLocatorDepth;
    }
    void SetBlockLocatorDepth(const uint256& hashDepth)
    {
        hashGetBlockLocatorDepth = hashDepth;
    }
    int GetLocatorInvBlockHash(uint256& hashBlock)
    {
        if (nInvHeight <= 0 || hashInvBlock == 0)
        {
            return -1;
        }
        hashBlock = hashInvBlock;
        return nInvHeight;
    }
    void SetLocatorInvBlockHash(int nHeight, const uint256& hashBlock, const uint256& hashNext)
    {
        if (nHeight >= nInvHeight)
        {
            nInvHeight = nHeight;
            hashInvBlock = hashBlock;
        }
        else
        {
            if (hashNext == 0 && hashInvBlock == 0)
            {
                nInvHeight = nHeight;
                hashInvBlock = hashBlock;
            }
            else
            {
                nInvHeight--;
                hashInvBlock = 0;
            }
        }
    }
    void SetNextGetBlocksTime(int nWaitTime)
    {
        invKnown[network::CInv::MSG_BLOCK - network::CInv::MSG_TX].nNextGetBlocksTime = GetTime() + nWaitTime;
    }
    bool CheckNextGetBlocksTime()
    {
        return (GetTime() >= invKnown[network::CInv::MSG_BLOCK - network::CInv::MSG_TX].nNextGetBlocksTime);
    }
    int64 AddRepeatBlock(const uint256& hash)
    {
        if (KnownInvExists(network::CInv(network::CInv::MSG_BLOCK, hash)))
        {
            std::set<uint256>& setHash = invKnown[network::CInv::MSG_BLOCK - network::CInv::MSG_TX].mapRepeat[CBlock::GetBlockHeightByHash(hash)];
            setHash.insert(hash);
            return setHash.size();
        }
        return 0;
    }

public:
    CInvPeerState invKnown[2];
    uint256 hashGetBlockLocatorDepth;
    int nInvHeight;
    uint256 hashInvBlock;
};

class COrphan
{
public:
    std::size_t GetSize();
    void AddNew(const uint256& prev, const uint256& hash);
    void Remove(const uint256& hash);
    void GetNext(const uint256& prev, std::vector<uint256>& vNext);
    void GetNext(const uint256& prev, std::vector<uint256>& vNext, std::set<uint256>& setHash);
    void RemoveNext(const uint256& prev);
    void RemoveBranch(const uint256& root, std::vector<uint256>& vBranch);

protected:
    std::multimap<uint256, uint256> mapOrphanByPrev;
};

class CSchedule
{
    typedef boost::variant<CNil, CBlock, CTransaction> CInvObject;
    class CInvState
    {
    public:
        CInvState()
          : nAssigned(0), objReceived(CNil()), nRecvInvTime(0), nRecvObjTime(0), nClearObjTime(0), nGetDataCount(0), fRepeatMintBlock(false) {}
        bool IsReceived()
        {
            return (objReceived.type() != typeid(CNil));
        }

    public:
        uint64 nAssigned;
        CInvObject objReceived;
        std::set<uint64> setKnownPeer;
        int64 nRecvInvTime;
        int64 nRecvObjTime;
        int64 nClearObjTime;
        int nGetDataCount;
        bool fRepeatMintBlock;
    };

public:
    enum
    {
        MAX_INV_COUNT = 1024 * 256,
        MAX_PEER_BLOCK_INV_COUNT = 1024,
        MAX_PEER_TX_INV_COUNT = 1024 * 256,
        MAX_REGETDATA_COUNT = 10,
        MAX_INV_WAIT_TIME = 3600,
        MAX_OBJ_WAIT_TIME = 7200,
        MAX_REPEAT_BLOCK_TIME = 180,
        MAX_REPEAT_BLOCK_COUNT = 4,
        MAX_SUB_BLOCK_DELAYED_TIME = 120,
        MAX_CERTTX_DELAYED_TIME = 180
    };

public:
    bool Exists(const network::CInv& inv);
    bool CheckPrevTxInv(const network::CInv& inv);
    void GetKnownPeer(const network::CInv& inv, std::set<uint64>& setKnownPeer);
    void RemovePeer(uint64 nPeerNonce, std::set<uint64>& setSchedPeer);
    bool CheckAddInvIdleLocation(uint64 nPeerNonce, uint32 nInvType);
    bool AddNewInv(const network::CInv& inv, uint64 nPeerNonce);
    bool RemoveInv(const network::CInv& inv, std::set<uint64>& setKnownPeer);
    bool ReceiveBlock(uint64 nPeerNonce, const uint256& hash, const CBlock& block, std::set<uint64>& setSchedPeer);
    bool ReceiveTx(uint64 nPeerNonce, const uint256& txid, const CTransaction& tx, std::set<uint64>& setSchedPeer);
    CBlock* GetBlock(const uint256& hash, uint64& nNonceSender);
    CTransaction* GetTransaction(const uint256& txid, uint64& nNonceSender);
    void AddOrphanBlockPrev(const uint256& hash, const uint256& prev);
    void AddOrphanTxPrev(const uint256& txid, const uint256& prev);
    void GetNextBlock(const uint256& hash, std::vector<uint256>& vNext);
    void GetNextTx(const uint256& txid, std::vector<uint256>& vNext, std::set<uint256>& setTx);
    void InvalidateBlock(const uint256& hash, std::set<uint64>& setMisbehavePeer);
    void InvalidateTx(const uint256& txid, std::set<uint64>& setMisbehavePeer);
    bool ScheduleBlockInv(uint64 nPeerNonce, std::vector<network::CInv>& vInv, std::size_t nMaxCount, bool& fMissingPrev, bool& fEmpty);
    bool ScheduleTxInv(uint64 nPeerNonce, std::vector<network::CInv>& vInv, std::size_t nMaxCount, bool& fReceivedAll);
    bool CancelAssignedInv(uint64 nPeerNonce, const network::CInv& inv);
    bool GetLocatorDepthHash(uint64 nPeerNonce, uint256& hashDepth);
    void SetLocatorDepthHash(uint64 nPeerNonce, const uint256& hashDepth);
    int GetLocatorInvBlockHash(uint64 nPeerNonce, uint256& hashBlock);
    void SetLocatorInvBlockHash(uint64 nPeerNonce, int nHeight, const uint256& hashBlock, const uint256& hashNext);
    void SetNextGetBlocksTime(uint64 nPeerNonce, int nWaitTime);
    bool SetRepeatBlock(uint64 nNonce, const uint256& hash);
    bool IsRepeatBlock(const uint256& hash);
    void AddRefBlock(const uint256& hashRefBlock, const uint256& hashFork, const uint256& hashBlock);
    void RemoveRefBlock(const uint256& hash);
    void GetNextRefBlock(const uint256& hashRefBlock, std::vector<std::pair<uint256, uint256>>& vNext);
    bool SetDelayedClear(const network::CInv& inv, int64 nDelayedTime);
    bool GetSubmitCachePowBlock(const CConsensusParam& consParam, std::vector<std::pair<uint256, int>>& vPowBlockHash);
    bool GetFirstCachePowBlock(int nHeight, uint256& hashFirstBlock);
    bool AddCacheLocalPowBlock(const CBlock& block, bool& fFirst);
    bool CheckCacheLocalPowBlock(int nHeight);
    bool GetCacheLocalPowBlock(const uint256& hash, CBlock& block);
    void RemoveCacheLocalPowBlock(const uint256& hash);
    bool GetCachePowBlock(const uint256& hash, CBlock& block);
    void RemoveHeightBlock(int nHeight, const uint256& hash);

protected:
    void RemoveOrphan(const network::CInv& inv);
    bool ScheduleKnownInv(uint64 nPeerNonce, CInvPeer& peer, uint32 type,
                          std::vector<network::CInv>& vInv, std::size_t nMaxCount, bool& fReceivedAll);

protected:
    COrphan orphanBlock;
    COrphan orphanTx;
    std::map<uint64, CInvPeer> mapPeer;
    std::map<network::CInv, CInvState> mapState;
    std::set<network::CInv> setMissPrevTxInv;
    std::multimap<uint256, std::pair<uint256, uint256>> mapRefBlock;
    std::map<int, std::vector<std::pair<uint256, int>>> mapHeightBlock;
    std::map<int, CBlock> mapKcPowBlock;
};

} // namespace bigbang

#endif //BIGBANG_SCHEDULE_H
