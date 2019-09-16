// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_NETCHN_H
#define BIGBANG_NETCHN_H

#include "base.h"
#include "message.h"
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

class CNetChannel : public network::INetChannelActor
{
public:
    CNetChannel();
    ~CNetChannel();
    /*int GetPrimaryChainHeight() override;
    bool IsForkSynchronized(const uint256& hashFork) const override;
    void BroadcastBlockInv(const uint256& hashFork, const uint256& hashBlock) override;
    void BroadcastTxInv(const uint256& hashFork) override;
    void SubscribeFork(const uint256& hashFork, const uint64& nNonce) override;
    void UnsubscribeFork(const uint256& hashFork) override;*/

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

    /*bool HandleEvent(network::CEventPeerActive& eventActive) override;
    bool HandleEvent(network::CEventPeerDeactive& eventDeactive) override;
    bool HandleEvent(network::CEventPeerSubscribe& eventSubscribe) override;
    bool HandleEvent(network::CEventPeerUnsubscribe& eventUnsubscribe) override;
    bool HandleEvent(network::CEventPeerInv& eventInv) override;
    bool HandleEvent(network::CEventPeerGetData& eventGetData) override;
    bool HandleEvent(network::CEventPeerGetBlocks& eventGetBlocks) override;
    bool HandleEvent(network::CEventPeerTx& eventTx) override;
    bool HandleEvent(network::CEventPeerBlock& eventBlock) override;*/

    void HandleActive(const CPeerActiveMessage& msg);
    void HandleDeactive(const CPeerDeactiveMessage& msg);
    void HandleSubscribe(const CPeerSubscribeMessage& msg);
    void HandleUnsubscribe(const CPeerUnSubscribeMessage& msg);
    void HandleInv(const CPeerInvMessage& msg);
    void HandleGetData(const CPeerGetDataMessage& msg);
    void HandleGetBlocks(const CPeerGetBlocksMessage& msg);
    void HandlePeerTx(const CPeerTxMessage& msg);
    void HandlePeerBlock(const CPeerBlockMessage& msg);

    CSchedule& GetSchedule(const uint256& hashFork);
    void NotifyPeerUpdate(uint64 nNonce, bool fActive, const network::CAddress& addrPeer);
    void DispatchGetBlocksEvent(uint64 nNonce, const uint256& hashFork);
    void DispatchAwardEvent(uint64 nNonce, xengine::CEndpointManager::Bonus bonus);
    void DispatchMisbehaveEvent(uint64 nNonce, xengine::CEndpointManager::CloseReason reason, const std::string& strCaller = "");
    void SchedulePeerInv(uint64 nNonce, const uint256& hashFork, CSchedule& sched);
    bool GetMissingPrevTx(CTransaction& tx, std::set<uint256>& setMissingPrevTx);
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
    IBlockChain* pBlockChain;
    ITxPool* pTxPool;
    IDispatcher* pDispatcher;
    IService* pService;

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
