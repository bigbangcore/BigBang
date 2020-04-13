// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_NETCHN_H
#define BIGBANG_NETCHN_H

#include "base.h"
#include "peernet.h"
#include "schedule.h"

namespace bigbang
{

class CNetChannelPeer
{
    class CNetChannelPeerFork
    {
    public:
        CNetChannelPeerFork()
          : fSynchronized(false), nSynTxInvStatus(SYNTXINV_STATUS_INIT), nSynTxInvSendTime(0), nSynTxInvRecvTime(0), nPrevGetDataTime(0),
            nSingleSynTxInvCount(network::CInv::MAX_INV_COUNT / 2), fWaitGetTxComplete(false), nCacheSynTxCount(NETCHANNEL_KNOWNINV_MAXCOUNT)
        {
        }
        enum
        {
            NETCHANNEL_KNOWNINV_EXPIREDTIME = 10 * 60,
            NETCHANNEL_KNOWNINV_MAXCOUNT = 1024 * 64
        };
        void AddKnownTx(const std::vector<uint256>& vTxHash, size_t nTotalSynTxCount);
        bool IsKnownTx(const uint256& txid) const
        {
            return (!!setKnownTx.get<0>().count(txid));
        }
        void InitTxInvSynData()
        {
            nSynTxInvStatus = SYNTXINV_STATUS_INIT;
            nSynTxInvSendTime = 0;
            nSynTxInvRecvTime = 0;
            nPrevGetDataTime = 0;
        }
        void ResetTxInvSynStatus(bool fIsComplete)
        {
            if (!fIsComplete)
            {
                if (nSynTxInvStatus == SYNTXINV_STATUS_WAIT_PEER_RECEIVED)
                {
                    int64 nWaitTime = GetTime() - nSynTxInvSendTime;
                    if (nWaitTime < 2)
                    {
                        nSingleSynTxInvCount *= 2;
                    }
                    else if (nWaitTime < 5)
                    {
                        nSingleSynTxInvCount += network::CInv::MIN_INV_COUNT;
                    }
                    else if (nWaitTime > 60)
                    {
                        nSingleSynTxInvCount /= 2;
                    }
                    else if (nWaitTime > 15)
                    {
                        nSingleSynTxInvCount -= network::CInv::MIN_INV_COUNT;
                    }
                    if (nSingleSynTxInvCount > network::CInv::MAX_INV_COUNT)
                    {
                        nSingleSynTxInvCount = network::CInv::MAX_INV_COUNT;
                    }
                    else if (nSingleSynTxInvCount < network::CInv::MIN_INV_COUNT)
                    {
                        nSingleSynTxInvCount = network::CInv::MIN_INV_COUNT;
                    }
                    nSynTxInvStatus = SYNTXINV_STATUS_WAIT_PEER_COMPLETE;
                    nSynTxInvRecvTime = GetTime();
                    nPrevGetDataTime = nSynTxInvRecvTime;
                }
            }
            else
            {
                InitTxInvSynData();
            }
        }
        int CheckTxInvSynStatus()
        {
            int64 nCurTime = GetTime();
            switch (nSynTxInvStatus)
            {
            case SYNTXINV_STATUS_INIT:
                break;
            case SYNTXINV_STATUS_WAIT_PEER_RECEIVED:
                if (nCurTime - nSynTxInvSendTime >= SYNTXINV_RECEIVE_TIMEOUT)
                {
                    return CHECK_SYNTXINV_STATUS_RESULT_WAIT_TIMEOUT;
                }
                break;
            case SYNTXINV_STATUS_WAIT_PEER_COMPLETE:
                if (nCurTime - nPrevGetDataTime >= SYNTXINV_GETDATA_TIMEOUT || nCurTime - nSynTxInvRecvTime >= SYNTXINV_COMPLETE_TIMEOUT)
                {
                    InitTxInvSynData();
                    nSingleSynTxInvCount = network::CInv::MIN_INV_COUNT;
                }
                break;
            default:
                InitTxInvSynData();
                break;
            }
            if (nSynTxInvStatus == SYNTXINV_STATUS_INIT)
            {
                return CHECK_SYNTXINV_STATUS_RESULT_ALLOW_SYN;
            }
            return CHECK_SYNTXINV_STATUS_RESULT_WAIT_SYN;
        }
        void SetPeerGetDataTime()
        {
            nPrevGetDataTime = GetTime();
        }

    protected:
        void ClearExpiredTx();

    public:
        enum
        {
            SYNTXINV_STATUS_INIT,
            SYNTXINV_STATUS_WAIT_PEER_RECEIVED,
            SYNTXINV_STATUS_WAIT_PEER_COMPLETE
        };
        enum
        {
            SYNTXINV_RECEIVE_TIMEOUT = 1200,
            SYNTXINV_COMPLETE_TIMEOUT = 3600 * 5,
            SYNTXINV_GETDATA_TIMEOUT = 600
        };

        bool fSynchronized;
        CPeerKnownTxSet setKnownTx;
        int nSynTxInvStatus;
        int64 nSynTxInvSendTime;
        int64 nSynTxInvRecvTime;
        int64 nPrevGetDataTime;
        int nSingleSynTxInvCount;
        bool fWaitGetTxComplete;
        size_t nCacheSynTxCount;
    };

public:
    CNetChannelPeer() {}
    CNetChannelPeer(uint64 nServiceIn, const network::CAddress& addr, const uint256& hashPrimary)
      : nService(nServiceIn), addressRemote(addr)
    {
        mapSubscribedFork.insert(std::make_pair(hashPrimary, CNetChannelPeerFork()));

        boost::asio::ip::tcp::endpoint ep;
        addressRemote.ssEndpoint.GetEndpoint(ep);
        if (ep.address().is_v6())
        {
            strRemoteAddress = string("[") + ep.address().to_string() + "]:" + std::to_string(ep.port());
        }
        else
        {
            strRemoteAddress = ep.address().to_string() + ":" + std::to_string(ep.port());
        }
    }
    bool IsSynchronized(const uint256& hashFork) const;
    bool SetSyncStatus(const uint256& hashFork, bool fSync, bool& fInverted);
    void AddKnownTx(const uint256& hashFork, const std::vector<uint256>& vTxHash, size_t nTotalSynTxCount);
    void Subscribe(const uint256& hashFork)
    {
        mapSubscribedFork.insert(std::make_pair(hashFork, CNetChannelPeerFork()));
    }
    void Unsubscribe(const uint256& hashFork)
    {
        mapSubscribedFork.erase(hashFork);
    }
    bool IsSubscribed(const uint256& hashFork) const
    {
        return (!!mapSubscribedFork.count(hashFork));
    }
    void ResetTxInvSynStatus(const uint256& hashFork, bool fIsComplete)
    {
        std::map<uint256, CNetChannelPeerFork>::iterator it = mapSubscribedFork.find(hashFork);
        if (it != mapSubscribedFork.end())
        {
            it->second.ResetTxInvSynStatus(fIsComplete);
        }
    }
    void SetWaitGetTxComplete(const uint256& hashFork)
    {
        std::map<uint256, CNetChannelPeerFork>::iterator it = mapSubscribedFork.find(hashFork);
        if (it != mapSubscribedFork.end())
        {
            it->second.fWaitGetTxComplete = true;
        }
    }
    bool CheckWaitGetTxComplete(const uint256& hashFork)
    {
        std::map<uint256, CNetChannelPeerFork>::iterator it = mapSubscribedFork.find(hashFork);
        if (it == mapSubscribedFork.end())
        {
            return false;
        }

        CNetChannelPeerFork& peer = it->second;
        if (peer.fWaitGetTxComplete)
        {
            peer.fWaitGetTxComplete = false;
            return true;
        }
        return false;
    }
    void SetPeerGetDataTime(const uint256& hashFork)
    {
        std::map<uint256, CNetChannelPeerFork>::iterator it = mapSubscribedFork.find(hashFork);
        if (it != mapSubscribedFork.end())
        {
            it->second.SetPeerGetDataTime();
        }
    }
    std::string GetRemoteAddress()
    {
        return strRemoteAddress;
    }
    int CheckTxInvSynStatus(const uint256& hashFork)
    {
        std::map<uint256, CNetChannelPeerFork>::iterator it = mapSubscribedFork.find(hashFork);
        if (it != mapSubscribedFork.end())
        {
            return it->second.CheckTxInvSynStatus();
        }
        return CHECK_SYNTXINV_STATUS_RESULT_WAIT_SYN;
    }
    bool MakeTxInv(const uint256& hashFork, const std::vector<uint256>& vTxPool, std::vector<network::CInv>& vInv);

public:
    enum
    {
        CHECK_SYNTXINV_STATUS_RESULT_WAIT_SYN,
        CHECK_SYNTXINV_STATUS_RESULT_WAIT_TIMEOUT,
        CHECK_SYNTXINV_STATUS_RESULT_ALLOW_SYN
    };

public:
    uint64 nService;
    network::CAddress addressRemote;
    std::string strRemoteAddress;
    std::map<uint256, CNetChannelPeerFork> mapSubscribedFork;
};

class CNetChannel : public network::INetChannel
{
public:
    CNetChannel();
    ~CNetChannel();
    int GetPrimaryChainHeight() override;
    bool IsForkSynchronized(const uint256& hashFork) const override;
    void BroadcastBlockInv(const uint256& hashFork, const uint256& hashBlock) override;
    void BroadcastTxInv(const uint256& hashFork) override;
    void SubscribeFork(const uint256& hashFork, const uint64& nNonce) override;
    void UnsubscribeFork(const uint256& hashFork) override;
    void SubmitCachePowBlock(const CConsensusParam& consParam) override;
    bool IsLocalCachePowBlock(int nHeight) override;
    bool AddCacheLocalPowBlock(const CBlock& block) override;

protected:
    enum
    {
        MAX_GETBLOCKS_COUNT = 128,
        GET_BLOCKS_INTERVAL_DEF_TIME = 120,
        GET_BLOCKS_INTERVAL_EQUAL_TIME = 600
    };
    enum
    {
        MAX_PEER_SCHED_COUNT = 8
    };
    enum
    {
        MSGRSP_SUBTYPE_NON = 0,
        MSGRSP_SUBTYPE_TXINV = 1
    };
    enum
    {
        MSGRSP_RESULT_GETBLOCKS_OK = 0,
        MSGRSP_RESULT_GETBLOCKS_EMPTY = 1,
        MSGRSP_RESULT_GETBLOCKS_EQUAL = 2,
        MSGRSP_RESULT_TXINV_RECEIVED = 3,
        MSGRSP_RESULT_TXINV_COMPLETE = 4
    };

    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;

    bool HandleEvent(network::CEventPeerActive& eventActive) override;
    bool HandleEvent(network::CEventPeerDeactive& eventDeactive) override;
    bool HandleEvent(network::CEventPeerSubscribe& eventSubscribe) override;
    bool HandleEvent(network::CEventPeerUnsubscribe& eventUnsubscribe) override;
    bool HandleEvent(network::CEventPeerInv& eventInv) override;
    bool HandleEvent(network::CEventPeerGetData& eventGetData) override;
    bool HandleEvent(network::CEventPeerGetBlocks& eventGetBlocks) override;
    bool HandleEvent(network::CEventPeerTx& eventTx) override;
    bool HandleEvent(network::CEventPeerBlock& eventBlock) override;
    bool HandleEvent(network::CEventPeerGetFail& eventGetFail) override;
    bool HandleEvent(network::CEventPeerMsgRsp& eventMsgRsp) override;

    CSchedule& GetSchedule(const uint256& hashFork);
    void NotifyPeerUpdate(uint64 nNonce, bool fActive, const network::CAddress& addrPeer);
    void DispatchGetBlocksEvent(uint64 nNonce, const uint256& hashFork);
    void DispatchAwardEvent(uint64 nNonce, xengine::CEndpointManager::Bonus bonus);
    void DispatchMisbehaveEvent(uint64 nNonce, xengine::CEndpointManager::CloseReason reason, const std::string& strCaller = "");
    void SchedulePeerInv(uint64 nNonce, const uint256& hashFork, CSchedule& sched);
    bool GetMissingPrevTx(const CTransaction& tx, std::set<uint256>& setMissingPrevTx);
    bool CheckPrevTx(const CTransaction& tx, uint64 nNonce, const uint256& hashFork, CSchedule& sched, const std::set<uint64>& setSchedPeer);
    void AddNewBlock(const uint256& hashFork, const uint256& hash, CSchedule& sched,
                     std::set<uint64>& setSchedPeer, std::set<uint64>& setMisbehavePeer,
                     std::vector<std::pair<uint256, uint256>>& vRefNextBlock, bool fCheckPow);
    void AddNewTx(const uint256& hashFork, const uint256& txid, CSchedule& sched,
                  std::set<uint64>& setSchedPeer, std::set<uint64>& setMisbehavePeer);
    void AddRefNextBlock(const std::vector<std::pair<uint256, uint256>>& vRefNextBlock);
    void PostAddNew(const uint256& hashFork, CSchedule& sched,
                    std::set<uint64>& setSchedPeer, std::set<uint64>& setMisbehavePeer);
    void SetPeerSyncStatus(uint64 nNonce, const uint256& hashFork, bool fSync);
    void PushTxTimerFunc(uint32 nTimerId);
    bool PushTxInv(const uint256& hashFork);
    const string GetPeerAddressInfo(uint64 nNonce);
    bool CheckPrevBlock(const uint256& hash, CSchedule& sched, uint256& hashFirst, uint256& hashPrev);
    void InnerBroadcastBlockInv(const uint256& hashFork, const uint256& hashBlock);
    void InnerSubmitCachePowBlock();

protected:
    network::CBbPeerNet* pPeerNet;
    ICoreProtocol* pCoreProtocol;
    IBlockChain* pBlockChain;
    ITxPool* pTxPool;
    IDispatcher* pDispatcher;
    IService* pService;
    IConsensus* pConsensus;

    mutable boost::recursive_mutex mtxSched;
    std::map<uint256, CSchedule> mapSched;

    mutable boost::shared_mutex rwNetPeer;
    std::map<uint64, CNetChannelPeer> mapPeer;
    std::map<uint256, std::set<uint64>> mapUnsync;

    mutable boost::mutex mtxPushTx;
    uint32 nTimerPushTx;
    bool fStartIdlePushTxTimer;
    std::set<uint256> setPushTxFork;
};

} // namespace bigbang

#endif //BIGBANG_NETCHN_H
