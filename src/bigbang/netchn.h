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

class CNetChannelPeer
{
    class CNetChannelPeerFork
    {
    public:
        CNetChannelPeerFork()
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
    CNetChannelPeer() {}
    CNetChannelPeer(uint64 nServiceIn, const uint256& hashPrimary)
      : nService(nServiceIn)
    {
        mapSubscribedFork.insert(std::make_pair(hashPrimary, CNetChannelPeerFork()));
    }
    bool IsSynchronized(const uint256& hashFork) const;
    bool SetSyncStatus(const uint256& hashFork, bool fSync, bool& fInverted);
    void AddKnownTx(const uint256& hashFork, const std::vector<uint256>& vTxHash);
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
        return mapSubscribedFork.count(hashFork) != 0;
    }
    void MakeTxInv(const uint256& hashFork, const std::vector<uint256>& vTxPool,
                   std::vector<network::CInv>& vInv, std::size_t nMaxCount);

public:
    uint64 nService;
    std::map<uint256, CNetChannelPeerFork> mapSubscribedFork;
};

class INetChannelModel : public IModel
{
public:
    INetChannelModel()
      : IModel("netchannelmodel") {}
    virtual void CleanUpForkScheduler() = 0;
    virtual bool AddNewForkSchedule(const uint256& hashFork) = 0;
    virtual bool RemoveForkSchudule(const uint256& hashFork) = 0;
    virtual std::vector<uint256> GetAllForks() const = 0;
    virtual bool IsForkSynchronized(const uint256& hashFork) const = 0;
    virtual bool GetKnownPeers(const uint256& hashFork, const uint256& hashBlock, std::set<uint64>& setKnownPeers) const = 0;
    virtual std::map<uint64, CNetChannelPeer> GetAllPeers() const = 0;
    virtual void AddPeer(uint64 nNonce, uint64 nService, const uint256& hashPrimary) = 0;
    virtual void RemovePeer(uint64 nNonce) = 0;
    virtual void RemoveUnSynchronizedForkPeerMT(uint64 nNonce, const uint256& hashFork) = 0;
    virtual void AddUnSynchronizedForkPeerMT(uint64 nNonce, const uint256& hashFork) = 0;
};

class CNetChannelModel : public INetChannelModel
{
public:
    CNetChannelModel();
    ~CNetChannelModel();

    void CleanUpForkScheduler() override;
    bool AddNewForkSchedule(const uint256& hashFork) override;
    bool RemoveForkSchudule(const uint256& hashFork) override;
    std::vector<uint256> GetAllForks() const;
    bool IsForkSynchronized(const uint256& hashFork) const override;
    bool GetKnownPeers(const uint256& hashFork, const uint256& hashBlock, std::set<uint64>& setKnownPeers) const override;
    std::map<uint64, CNetChannelPeer> GetAllPeers() const override;
    void AddPeer(uint64 nNonce, uint64 nService, const uint256& hashFork) override;
    void RemovePeer(uint64 nNonce) override;
    void RemoveUnSynchronizedForkPeerMT(uint64 nNonce, const uint256& hashFork) override;
    void AddUnSynchronizedForkPeerMT(uint64 nNonce, const uint256& hashFork) override;

protected:
    const CSchedule& GetSchedule(const uint256& hashFork) const;
    bool ContainsSchedule(const uint256& hashFork) const;
    void AddUnSynchronizedForkPeer(uint64 nNonce, const uint256& hashFork);
    void RemoveUnSynchronizedForkPeer(uint64 nNonce, const uint256& hashFork);

private:
    mutable boost::recursive_mutex mtxSched;
    std::map<uint256, CSchedule> mapSched;

    mutable boost::shared_mutex rwNetPeer;
    std::map<uint64, CNetChannelPeer> mapPeer;
    std::map<uint256, std::set<uint64>> mapUnsync;
};

class CNetChannel : public network::INetChannelController
{
public:
    CNetChannel();
    ~CNetChannel();

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

    void HandleBroadcastBlockInv(const CBroadcastBlockInvMessage& invMsg);
    void HandleBroadcastTxInv(const CBroadcastTxInvMessage& invMsg);
    void HandleSubscribeFork(const CSubscribeForkMessage& subscribeMsg);
    void HandleUnsubscribeFork(const CUnsubscribeForkMessage& unsubscribeMsg);

    void HandleActive(const CPeerActiveMessage& activeMsg);
    void HandleDeactive(const CPeerDeactiveMessage& deactiveMsg);
    void HandleSubscribe(const CPeerSubscribeMessageInBound& subscribeMsg);
    void HandleUnsubscribe(const CPeerUnsubscribeMessageInBound& unsubscribeMsg);
    void HandleInv(const CPeerInvMessageInBound& invMsg);
    void HandleGetData(const CPeerGetDataMessageInBound& getDataMsg);
    void HandleGetBlocks(const CPeerGetBlocksMessageInBound& getBlocksMsg);
    void HandlePeerTx(const CPeerTxMessageInBound& txMsg);
    void HandlePeerBlock(const CPeerBlockMessageInBound& blockMsg);

    CSchedule& GetSchedule(const uint256& hashFork);
    void NotifyPeerUpdate(uint64 nNonce, bool fActive, const network::CAddress& addrPeer);
    void DispatchGetBlocksEvent(uint64 nNonce, const uint256& hashFork);
    void DispatchAwardEvent(uint64 nNonce, xengine::CEndpointManager::Bonus bonus);
    void DispatchMisbehaveEvent(uint64 nNonce, xengine::CEndpointManager::CloseReason reason, const std::string& strCaller = "");
    void SchedulePeerInv(uint64 nNonce, const uint256& hashFork, CSchedule& sched);
    bool GetMissingPrevTx(const CTransaction& tx, std::set<uint256>& setMissingPrevTx);
    void AddNewBlock(const uint256& hashFork, const uint256& hash, CSchedule& sched,
                     std::set<uint64>& setSchedPeer, std::set<uint64>& setMisbehavePeer);
    void AddNewTx(const uint256& hashFork, const uint256& txid, CSchedule& sched,
                  std::set<uint64>& setSchedPeer, std::set<uint64>& setMisbehavePeer);
    void PostAddNew(const uint256& hashFork, CSchedule& sched,
                    std::set<uint64>& setSchedPeer, std::set<uint64>& setMisbehavePeer);
    void SetPeerSyncStatus(uint64 nNonce, const uint256& hashFork, bool fSync);
    void PushTxTimerFunc(uint32 nTimerId);
    bool PushTxInv(const uint256& hashFork);

protected:
    network::CBbPeerNet* pPeerNet;
    ICoreProtocol* pCoreProtocol;
    IWorldLineController* pWorldLineCntrl;
    ITxPoolController* pTxPoolCntrl;
    IDispatcher* pDispatcher;
    IService* pService;
    INetChannelModel* pNetChannelModel;

    mutable boost::recursive_mutex mtxSched;
    std::map<uint256, CSchedule> mapSched;

    mutable boost::shared_mutex rwNetPeer;
    std::map<uint64, CNetChannelPeer> mapPeer;
    std::map<uint256, std::set<uint64>> mapUnsync;

    mutable boost::mutex mtxPushTx;
    uint32 nTimerPushTx;
    std::set<uint256> setPushTxFork;
};

} // namespace bigbang

#endif //BIGBANG_NETCHN_H
