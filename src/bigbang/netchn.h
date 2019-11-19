// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_NETCHN_H
#define BIGBANG_NETCHN_H

#include "base.h"
#include "message.h"
#include "peernet.h"
#include "peernet/message.h"
#include "schedule.h"
namespace bigbang
{

class CNetChannelControllerPeer
{
    class CNetChannelControllerPeerFork
    {
    public:
        CNetChannelControllerPeerFork()
          : fSynchronized(false) {}
        enum
        {
            NETCHANNEL_KNOWNINV_EXPIREDTIME = 5 * 60,
            NETCHANNEL_KNOWNINV_MAXCOUNT = 1024 * 256
        };
        void AddKnownTx(const std::vector<uint256>& vTxHash);
        bool IsKnownTx(const uint256& txid) const
        {
            return setKnownTx.get<0>().count(txid) != 0;
        }

    protected:
        void ClearExpiredTx(std::size_t nReserved);

    public:
        bool fSynchronized;
        CPeerKnownTxSet setKnownTx;
    };

public:
    CNetChannelControllerPeer() {}
    CNetChannelControllerPeer(uint64 nServiceIn, const uint256& hashPrimary)
      : nService(nServiceIn)
    {
        mapSubscribedFork.insert(std::make_pair(hashPrimary, CNetChannelControllerPeerFork()));
    }
    bool IsSynchronized(const uint256& hashFork) const;
    bool SetSyncStatus(const uint256& hashFork, bool fSync, bool& fInverted);
    void AddKnownTx(const uint256& hashFork, const std::vector<uint256>& vTxHash);
    void Subscribe(const uint256& hashFork)
    {
        mapSubscribedFork.insert(std::make_pair(hashFork, CNetChannelControllerPeerFork()));
    }
    void Unsubscribe(const uint256& hashFork)
    {
        mapSubscribedFork.erase(hashFork);
    }
    bool IsSubscribed(const uint256& hashFork) const
    {
        return mapSubscribedFork.count(hashFork) != 0;
    }
    void MakeTxInv(const uint256& hashFork, const std::vector<uint256>& vTxPool,
                   std::vector<network::CInv>& vInv, std::size_t nMaxCount);

public:
    uint64 nService;
    std::map<uint256, CNetChannelControllerPeerFork> mapSubscribedFork;
};

class INetChannelModel : public IModel
{
public:
    typedef bool ScheduleBlockInvRetBool;
    typedef bool ScheduleTxInvRetBool;
    typedef bool PrevMissingBool;
    typedef bool SyncBool;
    typedef uint256 HashForkType;
    typedef std::tuple<ScheduleBlockInvRetBool,
                       PrevMissingBool,
                       ScheduleTxInvRetBool,
                       SyncBool,
                       HashForkType,
                       std::vector<bigbang::network::CInv>>
        ScheduleResultType;
    typedef std::pair<uint64, ScheduleResultType> ScheduleResultPair;
    typedef std::vector<network::CInv> MakeTxInvResultType;
    typedef std::pair<uint64, MakeTxInvResultType> MakeTxInvResultPair;

public:
    INetChannelModel()
      : IModel("netchannelmodel") {}
    virtual void CleanUpForkScheduler() = 0;
    virtual bool AddNewForkSchedule(const uint256& hashFork) = 0;
    virtual bool RemoveForkSchudule(const uint256& hashFork) = 0;
    virtual bool AddNewInvSchedule(uint64 nNonce, const uint256& hashFork, const network::CInv& inv) = 0;
    virtual std::vector<uint256> GetAllForks() const = 0;
    virtual bool IsForkSynchronized(const uint256& hashFork) const = 0;
    virtual bool GetKnownPeers(const uint256& hashFork, const uint256& hashBlock, std::set<uint64>& setKnownPeers) const = 0;
    virtual std::map<uint64, CNetChannelControllerPeer> GetAllPeers() const = 0;
    virtual void AddPeer(uint64 nNonce, uint64 nService, const uint256& hashPrimary) = 0;
    virtual void RemovePeer(uint64 nNonce) = 0;
    virtual void RemoveUnSynchronizedForkPeerMT(uint64 nNonce, const uint256& hashFork) = 0;
    virtual void AddUnSynchronizedForkPeerMT(uint64 nNonce, const uint256& hashFork) = 0;
    virtual void ScheduleDeactivePeerInv(uint64 nNonce, std::vector<ScheduleResultPair>& schedResult) = 0;
    virtual bool ScheduleActivePeerInv(uint64 nNonce, const uint256& hashFork, std::vector<ScheduleResultPair>& schedResult) = 0;
    virtual bool SetPeerSyncStatus(uint64 nNonce, const uint256& hashFork, bool fSync, bool& fInverted) = 0;
    virtual bool ContainsScheduleMT(const uint256& hashFork) const = 0;
    virtual bool ContainsPeerMT(uint64 nNonce) const = 0;
    virtual void SubscribePeerFork(uint64 nNonce, const uint256& hashFork) = 0;
    virtual void UnsubscribePeerFork(uint64 nNonce, const uint256& hashFork) = 0;
    virtual void AddKnownTxPeer(uint64 nNonce, const uint256& hashFork, const std::vector<uint256>& vTxHash) = 0;
    virtual bool IsPeerEmpty() const = 0;
    virtual void MakeTxInv(const uint256& hashFork, const std::vector<uint256>& vTxPool, std::vector<MakeTxInvResultPair>& txInvResult) = 0;
    virtual bool ReceiveScheduleTx(uint64 nNonce, const uint256& hashFork, const uint256& txid, const CTransaction& tx, std::set<uint64>& setSchedPeer) = 0;
    virtual void InvalidateScheduleTx(const uint256& hashFork, const uint256& txid, std::set<uint64>& setMisbehavePeer) = 0;
    virtual void AddScheduleOrphanTxPrev(const uint256& hashFork, const uint256& txid, const uint256& prevtxid) = 0;
    virtual bool ExistsScheduleInv(const uint256& hashFork, const network::CInv& inv) const = 0;
    virtual bool ReceiveScheduleBlock(uint64 nNonce, const uint256& hashFork, const uint256& blockHash, const CBlock& block, std::set<uint64>& setSchedPeer) = 0;
    virtual void InvalidateScheduleBlock(const uint256& hashFork, const uint256& blockHash, std::set<uint64>& setMisbehavePeer) = 0;
    virtual void AddOrphanBlockPrev(const uint256& hashFork, const uint256& blockHash, const uint256& prevHash) = 0;
    virtual CTransaction* GetScheduleTransaction(const uint256& hashFork, const uint256& txid, uint64& nNonceSender) = 0;
    virtual void GetScheduleNextTx(const uint256& hashFork, const uint256& txid, std::vector<uint256>& vNextTx) = 0;
    virtual void RemoveScheduleInv(const uint256& hashFork, const network::CInv& inv, std::set<uint64>& setSchedPeer) = 0;
    virtual CBlock* GetScheduleBlock(const uint256& hashFork, const uint256& hashBlock, uint64& nNonceSender) = 0;
    virtual void GetScheduleNextBlock(const uint256& hashFork, const uint256& hashBlock, vector<uint256>& vNextBlock) = 0;
};

class CNetChannel : public INetChannelModel
{
    friend class INetChannelController;

public:
    CNetChannel();
    ~CNetChannel();

    void CleanUpForkScheduler() override;
    bool AddNewForkSchedule(const uint256& hashFork) override;
    bool RemoveForkSchudule(const uint256& hashFork) override;
    bool AddNewInvSchedule(uint64 nNonce, const uint256& hashFork, const network::CInv& inv) override;
    std::vector<uint256> GetAllForks() const override;
    bool IsForkSynchronized(const uint256& hashFork) const override;
    bool GetKnownPeers(const uint256& hashFork, const uint256& hashBlock, std::set<uint64>& setKnownPeers) const override;
    std::map<uint64, CNetChannelControllerPeer> GetAllPeers() const override;
    void AddPeer(uint64 nNonce, uint64 nService, const uint256& hashFork) override;
    void RemovePeer(uint64 nNonce) override;
    void RemoveUnSynchronizedForkPeerMT(uint64 nNonce, const uint256& hashFork) override;
    void AddUnSynchronizedForkPeerMT(uint64 nNonce, const uint256& hashFork) override;
    void ScheduleDeactivePeerInv(uint64 nNonce, std::vector<ScheduleResultPair>& schedResult) override;
    bool ScheduleActivePeerInv(uint64 nNonce, const uint256& hashFork, std::vector<ScheduleResultPair>& schedResult) override;
    bool SetPeerSyncStatus(uint64 nNonce, const uint256& hashFork, bool fSync, bool& fInverted) override;
    bool ContainsScheduleMT(const uint256& hashFork) const override;
    bool ContainsPeerMT(uint64 nNonce) const override;
    void SubscribePeerFork(uint64 nNonce, const uint256& hashFork) override;
    void UnsubscribePeerFork(uint64 nNonce, const uint256& hashFork) override;
    void AddKnownTxPeer(uint64 nNonce, const uint256& hashFork, const std::vector<uint256>& vTxHash) override;
    bool IsPeerEmpty() const override;
    void MakeTxInv(const uint256& hashFork, const std::vector<uint256>& vTxPool, std::vector<MakeTxInvResultPair>& txInvResult) override;
    bool ReceiveScheduleTx(uint64 nNonce, const uint256& hashFork, const uint256& txid, const CTransaction& tx, std::set<uint64>& setSchedPeer) override;
    void InvalidateScheduleTx(const uint256& hashFork, const uint256& txid, std::set<uint64>& setMisbehavePeer) override;
    void AddScheduleOrphanTxPrev(const uint256& hashFork, const uint256& txid, const uint256& prevtxid) override;
    bool ExistsScheduleInv(const uint256& hashFork, const network::CInv& inv) const override;
    bool ReceiveScheduleBlock(uint64 nNonce, const uint256& hashFork, const uint256& blockHash, const CBlock& block, std::set<uint64>& setSchedPeer) override;
    void InvalidateScheduleBlock(const uint256& hashFork, const uint256& blockHash, std::set<uint64>& setMisbehavePeer) override;
    void AddOrphanBlockPrev(const uint256& hashFork, const uint256& blockHash, const uint256& prevHash) override;
    CTransaction* GetScheduleTransaction(const uint256& hashFork, const uint256& txid, uint64& nNonceSender) override;
    void GetScheduleNextTx(const uint256& hashFork, const uint256& txid, std::vector<uint256>& vNextTx) override;
    void RemoveScheduleInv(const uint256& hashFork, const network::CInv& inv, std::set<uint64>& setSchedPeer) override;
    CBlock* GetScheduleBlock(const uint256& hashFork, const uint256& hashBlock, uint64& nNonceSender) override;
    void GetScheduleNextBlock(const uint256& hashFork, const uint256& hashBlock, vector<uint256>& vNextBlock) override;

protected:
    enum
    {
        MAX_GETBLOCKS_COUNT = 128
    };
    enum
    {
        MAX_PEER_SCHED_COUNT = 8
    };

    const CSchedule& GetSchedule(const uint256& hashFork) const;
    CSchedule& GetSchedule(const uint256& hashFork);
    bool ContainsSchedule(const uint256& hashFork) const;
    void AddUnSynchronizedForkPeer(uint64 nNonce, const uint256& hashFork);
    void RemoveUnSynchronizedForkPeer(uint64 nNonce, const uint256& hashFork);
    void SchedulePeerInv(uint64 nNonce, const uint256& hashFork, CSchedule& sched, std::vector<ScheduleResultPair>& schedResult);

protected:
    mutable boost::recursive_mutex mtxSched;
    std::map<uint256, CSchedule> mapSched;

    mutable boost::shared_mutex rwNetPeer;
    std::map<uint64, CNetChannelControllerPeer> mapPeer;
    std::map<uint256, std::set<uint64>> mapUnsync;
};

class CNetChannelController : public network::INetChannelController
{
public:
    CNetChannelController();
    ~CNetChannelController();

protected:
    enum
    {
        MAX_GETBLOCKS_COUNT = 128
    };
    enum
    {
        MAX_PEER_SCHED_COUNT = 8
    };

    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;

    void HandleBroadcastBlockInv(const std::shared_ptr<CBroadcastBlockInvMessage> spMsg);
    void HandleBroadcastTxInv(const std::shared_ptr<CBroadcastTxInvMessage> spMsg);
    void HandleSubscribeFork(const std::shared_ptr<CSubscribeForkMessage> spMsg);
    void HandleUnsubscribeFork(const std::shared_ptr<CUnsubscribeForkMessage> spMsg);

    void HandleActive(const std::shared_ptr<CPeerActiveMessage> spMsg);
    void HandleDeactive(const std::shared_ptr<CPeerDeactiveMessage> spMsg);
    void HandleSubscribe(const std::shared_ptr<CPeerSubscribeMessageInBound> spMsg);
    void HandleUnsubscribe(const std::shared_ptr<CPeerUnsubscribeMessageInBound> spMsg);
    void HandleInv(const std::shared_ptr<CPeerInvMessageInBound> spMsg);
    void HandleGetData(const std::shared_ptr<CPeerGetDataMessageInBound> spMsg);
    void HandleGetBlocks(const std::shared_ptr<CPeerGetBlocksMessageInBound> spMsg);
    void HandlePeerTx(const std::shared_ptr<CPeerTxMessageInBound> spMsg);
    void HandlePeerBlock(const std::shared_ptr<CPeerBlockMessageInBound> spMsg);

    void HandleAddedNewBlock(const std::shared_ptr<CAddedBlockMessage> spMsg);
    void HandleAddedNewTx(const std::shared_ptr<CAddedTxMessage> spMsg);

    void DispatchGetBlocksFromHashEvent(uint64 nNonce, const uint256& hashFork, const uint256& hashBlock);
    void DispatchAwardEvent(uint64 nNonce, xengine::CEndpointManager::Bonus bonus);
    void DispatchMisbehaveEvent(uint64 nNonce, xengine::CEndpointManager::CloseReason reason, const std::string& strCaller = "");
    void SchedulePeerInv(uint64 nNonce, const uint256& hashFork, bool fActivedPeer, const uint256& lastBlockHash);
    bool GetMissingPrevTx(const CTransaction& tx, std::set<uint256>& setMissingPrevTx);
    void AddNewBlock(const uint256& hashFork, const uint256& hash, std::set<uint64>& setSchedPeer, std::set<uint64>& setMisbehavePeer);
    void AddNewTx(const uint256& hashFork, const uint256& txid, std::set<uint64>& setSchedPeer, std::set<uint64>& setMisbehavePeer);
    void PostAddNew(const uint256& hashFork, std::set<uint64>& setSchedPeer, std::set<uint64>& setMisbehavePeer);
    void SetPeerSyncStatus(uint64 nNonce, const uint256& hashFork, bool fSync);
    void PushTxTimerFunc(uint32 nTimerId);
    bool PushTxInv(const uint256& hashFork);

protected:
    ICoreProtocol* pCoreProtocol;
    IWorldLineController* pWorldLineCtrl;
    ITxPoolController* pTxPoolCtrl;
    IService* pService;
    INetChannelModel* pNetChannelModel;

    mutable boost::mutex mtxPushTx;
    uint32 nTimerPushTx;
    std::set<uint256> setPushTxFork;
};

} // namespace bigbang

#endif //BIGBANG_NETCHN_H
