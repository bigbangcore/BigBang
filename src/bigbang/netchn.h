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

class CNetChannelPeerFork
{
public:
    CNetChannelPeerFork()
      : fSynchronized(false), nSynTxInvStatus(SYNTXINV_STATUS_INIT), nSynTxInvSendTime(0), nSynTxInvRecvTime(0), nPrevGetDataTime(0),
        nSingleSynTxInvCount(network::CInv::MAX_INV_COUNT / 2), fWaitGetTxComplete(false)
    {
    }

    void InitTxInvSynData();
    void ResetTxInvSynStatus(bool fIsComplete);
    int CheckTxInvSynStatus();
    void SetPeerGetDataTime();

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
    int nSynTxInvStatus;
    int64 nSynTxInvSendTime;
    int64 nSynTxInvRecvTime;
    int64 nPrevGetDataTime;
    int nSingleSynTxInvCount;
    bool fWaitGetTxComplete;
};

class CNetChannelPeer
{
public:
    CNetChannelPeer()
      : nService(0) {}
    CNetChannelPeer(uint64 nServiceIn, const network::CAddress& addr, const uint256& hashPrimary);
    bool SetSyncStatus(const uint256& hashFork, bool fSync, bool& fInverted);
    void Subscribe(const uint256& hashFork);
    void Unsubscribe(const uint256& hashFork);
    bool IsSubscribed(const uint256& hashFork) const;
    void ResetTxInvSynStatus(const uint256& hashFork, bool fIsComplete);
    void SetWaitGetTxComplete(const uint256& hashFork);
    bool CheckWaitGetTxComplete(const uint256& hashFork);
    void SetPeerGetDataTime(const uint256& hashFork);
    std::string GetRemoteAddress();
    bool CheckSynTxInvStatus(const uint256& hashFork, bool& fTimeout, int& nSingleSynCount);
    void SetSynTxInvStatus(const uint256& hashFork);

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
    bool SubmitCachePowBlock(const CConsensusParam& consParam) override;
    bool IsLocalCachePowBlock(int nHeight) override;
    bool AddCacheLocalPowBlock(const CBlock& block) override;
    bool AddVerifyPowBlock(const uint64& nNonce, const uint256& hashFork, const CBlock& block) override;

protected:
    enum
    {
        MAX_GETBLOCKS_COUNT = 128,
        GET_BLOCKS_INTERVAL_DEF_TIME = 120,
        GET_BLOCKS_INTERVAL_EQUAL_TIME = 600,
        PUSHTX_TIMEOUT = 1000,
        SYNTXINV_TIMEOUT = 1000 * 60
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

    bool HandleEvent(network::CEventLocalBroadcastInv& evenBroadcastInv) override;
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
    void PostAddNew(const uint256& hashFork, std::set<uint64>& setSchedPeer, std::set<uint64>& setMisbehavePeer);
    void SetPeerSyncStatus(uint64 nNonce, const uint256& hashFork, bool fSync);
    void PushTxTimerFunc(uint32 nTimerId);
    void ForkPushTxTimerFunc(uint32 nTimerId);
    bool PushTxInv(const uint256& hashFork);
    const string GetPeerAddressInfo(uint64 nNonce);
    bool CheckPrevBlock(const uint256& hash, CSchedule& sched, uint256& hashFirst, uint256& hashPrev);
    void InnerBroadcastBlockInv(const uint256& hashFork, const uint256& hashBlock);
    void InnerSubmitCachePowBlock();

    const CBasicConfig* Config()
    {
        return dynamic_cast<const CBasicConfig*>(xengine::IBase::Config());
    }

protected:
    network::CBbPeerNet* pPeerNet;
    ICoreProtocol* pCoreProtocol;
    IBlockChain* pBlockChain;
    ITxPool* pTxPool;
    IDispatcher* pDispatcher;
    IService* pService;
    IConsensus* pConsensus;
    IVerify* pVerify;

    mutable boost::recursive_mutex mtxSched;
    std::map<uint256, CSchedule> mapSched;

    mutable boost::shared_mutex rwNetPeer;
    std::map<uint64, CNetChannelPeer> mapPeer;
    std::map<uint256, std::set<uint64>> mapUnsync;

    mutable boost::mutex mtxPushTx;
    uint32 nTimerPushTx;
    std::map<uint256, std::pair<int64, uint32>> mapForkPushTx;
};

} // namespace bigbang

#endif //BIGBANG_NETCHN_H
