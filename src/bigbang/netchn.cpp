// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "netchn.h"

#include <boost/bind.hpp>

#include "schedule.h"

using namespace std;
using namespace xengine;
using boost::asio::ip::tcp;

#define PUSHTX_TIMEOUT (1000)

namespace bigbang
{

//////////////////////////////
// CNetChannelPeer

void CNetChannelPeer::CNetChannelPeerFork::AddKnownTx(const vector<uint256>& vTxHash)
{
    ClearExpiredTx(vTxHash.size());
    for (const uint256& txid : vTxHash)
    {
        setKnownTx.insert(CPeerKnownTx(txid));
    }
}

void CNetChannelPeer::CNetChannelPeerFork::ClearExpiredTx(size_t nReserved)
{
    CPeerKnownTxSetByTime& index = setKnownTx.get<1>();
    int64 nExpiredAt = GetTime() - NETCHANNEL_KNOWNINV_EXPIREDTIME;
    size_t nMaxSize = NETCHANNEL_KNOWNINV_MAXCOUNT - nReserved;
    CPeerKnownTxSetByTime::iterator it = index.begin();
    while (it != index.end() && ((*it).nTime <= nExpiredAt || index.size() > nMaxSize))
    {
        index.erase(it++);
    }
}

bool CNetChannelPeer::IsSynchronized(const uint256& hashFork) const
{
    map<uint256, CNetChannelPeerFork>::const_iterator it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        return (*it).second.fSynchronized;
    }
    return false;
}

bool CNetChannelPeer::SetSyncStatus(const uint256& hashFork, bool fSync, bool& fInverted)
{
    map<uint256, CNetChannelPeerFork>::iterator it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        fInverted = ((*it).second.fSynchronized != fSync);
        (*it).second.fSynchronized = fSync;
        return true;
    }
    return false;
}

void CNetChannelPeer::AddKnownTx(const uint256& hashFork, const vector<uint256>& vTxHash)
{
    map<uint256, CNetChannelPeerFork>::iterator it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        (*it).second.AddKnownTx(vTxHash);
    }
}

void CNetChannelPeer::MakeTxInv(const uint256& hashFork, const vector<uint256>& vTxPool,
                                vector<network::CInv>& vInv, size_t nMaxCount)
{
    map<uint256, CNetChannelPeerFork>::iterator it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        vector<uint256> vTxHash;
        CNetChannelPeerFork& peerFork = (*it).second;
        for (const uint256& txid : vTxPool)
        {
            if (vInv.size() >= nMaxCount)
            {
                break;
            }
            else if (!(*it).second.IsKnownTx(txid))
            {
                vInv.push_back(network::CInv(network::CInv::MSG_TX, txid));
                vTxHash.push_back(txid);
            }
        }
        peerFork.AddKnownTx(vTxHash);
    }
}

CNetChannel::CNetChannel()
{
}

CNetChannel::~CNetChannel()
{
}

//////////////////////////////
// CNetChannelController

CNetChannelController::CNetChannelController()
{
    pPeerNet = nullptr;
    pCoreProtocol = nullptr;
    pWorldLineCtrl = nullptr;
    pTxPoolCtrl = nullptr;
    pService = nullptr;
    pDispatcher = nullptr;
}

CNetChannelController::~CNetChannelController()
{
}

bool CNetChannelController::HandleInitialize()
{
    if (!GetObject("peernet", pPeerNet))
    {
        Error("Failed to request peer net\n");
        return false;
    }

    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol\n");
        return false;
    }

    if (!GetObject("worldlinecontroller", pWorldLineCtrl))
    {
        Error("Failed to request worldline\n");
        return false;
    }

    if (!GetObject("txpoolcontroller", pTxPoolCtrl))
    {
        Error("Failed to request txpool\n");
        return false;
    }

    if (!GetObject("service", pService))
    {
        Error("Failed to request service\n");
        return false;
    }

    if (!GetObject("dispatcher", pDispatcher))
    {
        Error("Failed to request dispatcher\n");
        return false;
    }

    RegisterRefHandler<CBroadcastBlockInvMessage>(boost::bind(&CNetChannelController::HandleBroadcastBlockInv, this, _1));
    RegisterRefHandler<CBroadcastTxInvMessage>(boost::bind(&CNetChannelController::HandleBroadcastTxInv, this, _1));
    RegisterRefHandler<CSubscribeForkMessage>(boost::bind(&CNetChannelController::HandleSubscribeFork, this, _1));
    RegisterRefHandler<CUnsubscribeForkMessage>(boost::bind(&CNetChannelController::HandleUnsubscribeFork, this, _1));

    RegisterRefHandler<CPeerActiveMessage>(boost::bind(&CNetChannelController::HandleActive, this, _1));
    RegisterRefHandler<CPeerDeactiveMessage>(boost::bind(&CNetChannelController::HandleDeactive, this, _1));
    RegisterRefHandler<CPeerSubscribeMessageInBound>(boost::bind(&CNetChannelController::HandleSubscribe, this, _1));
    RegisterRefHandler<CPeerUnsubscribeMessageInBound>(boost::bind(&CNetChannelController::HandleUnsubscribe, this, _1));
    RegisterRefHandler<CPeerInvMessageInBound>(boost::bind(&CNetChannelController::HandleInv, this, _1));
    RegisterRefHandler<CPeerGetDataMessageInBound>(boost::bind(&CNetChannelController::HandleGetData, this, _1));
    RegisterRefHandler<CPeerGetBlocksMessageInBound>(boost::bind(&CNetChannelController::HandleGetBlocks, this, _1));
    RegisterRefHandler<CPeerTxMessageInBound>(boost::bind(&CNetChannelController::HandlePeerTx, this, _1));
    RegisterRefHandler<CPeerBlockMessageInBound>(boost::bind(&CNetChannelController::HandlePeerBlock, this, _1));

    return true;
}

void CNetChannelController::HandleDeinitialize()
{
    pPeerNet = nullptr;
    pCoreProtocol = nullptr;
    pWorldLineCtrl = nullptr;
    pTxPoolCtrl = nullptr;
    pService = nullptr;
    pDispatcher = nullptr;

    DeregisterHandler(CBroadcastBlockInvMessage::MessageType());
    DeregisterHandler(CBroadcastTxInvMessage::MessageType());
    DeregisterHandler(CSubscribeForkMessage::MessageType());
    DeregisterHandler(CUnsubscribeForkMessage::MessageType());

    DeregisterHandler(CPeerActiveMessage::MessageType());
    DeregisterHandler(CPeerDeactiveMessage::MessageType());
    DeregisterHandler(CPeerSubscribeMessageInBound::MessageType());
    DeregisterHandler(CPeerUnsubscribeMessageInBound::MessageType());
    DeregisterHandler(CPeerInvMessageInBound::MessageType());
    DeregisterHandler(CPeerGetDataMessageInBound::MessageType());
    DeregisterHandler(CPeerGetBlocksMessageInBound::MessageType());
    DeregisterHandler(CPeerTxMessageInBound::MessageType());
    DeregisterHandler(CPeerBlockMessageInBound::MessageType());
}

bool CNetChannelController::HandleInvoke()
{
    if (!StartActor())
    {
        Error("Failed to start actor\n");
        return false;
    }

    {
        boost::unique_lock<boost::mutex> lock(mtxPushTx);
        nTimerPushTx = 0;
    }

    return network::INetChannelController::HandleInvoke();
}

void CNetChannelController::HandleHalt()
{
    StopActor();

    {
        boost::unique_lock<boost::mutex> lock(mtxPushTx);
        if (nTimerPushTx != 0)
        {
            CancelTimer(nTimerPushTx);
            nTimerPushTx = 0;
        }
        setPushTxFork.clear();
    }

    network::INetChannelController::HandleHalt();

    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
        mapSched.clear();
    }
}

bool CNetChannelController::IsForkSynchronized(const uint256& hashFork) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
    map<uint256, set<uint64>>::const_iterator it = mapUnsync.find(hashFork);
    return (it == mapUnsync.end() || (*it).second.empty());
}

void CNetChannelController::HandleBroadcastBlockInv(const CBroadcastBlockInvMessage& invMsg)
{
    set<uint64> setKnownPeer;
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
        CSchedule& sched = GetSchedule(invMsg.hashFork);
        sched.GetKnownPeer(network::CInv(network::CInv::MSG_BLOCK, invMsg.hashBlock), setKnownPeer);
    }

    {
        boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
        {
            uint64 nNonce = (*it).first;
            if (!setKnownPeer.count(nNonce) && (*it).second.IsSubscribed(invMsg.hashFork))
            {
                auto spInvMsg = CPeerInvMessageOutBound::Create();
                spInvMsg->nNonce = nNonce;
                spInvMsg->hashFork = invMsg.hashFork;
                spInvMsg->vecInv.push_back(network::CInv(network::CInv::MSG_BLOCK, invMsg.hashBlock));
                PUBLISH_MESSAGE(spInvMsg);
            }
        }
    }
}

void CNetChannelController::HandleBroadcastTxInv(const CBroadcastTxInvMessage& invMsg)
{
    boost::unique_lock<boost::mutex> lock(mtxPushTx);
    if (nTimerPushTx == 0)
    {
        if (!PushTxInv(invMsg.hashFork))
        {
            setPushTxFork.insert(invMsg.hashFork);
        }
        nTimerPushTx = SetTimer(PUSHTX_TIMEOUT, boost::bind(&CNetChannelController::PushTxTimerFunc, this, _1));
    }
    else
    {
        setPushTxFork.insert(invMsg.hashFork);
    }
}

void CNetChannelController::HandleSubscribeFork(const CSubscribeForkMessage& subscribeMsg)
{
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
        if (!mapSched.insert(make_pair(subscribeMsg.hashFork, CSchedule())).second)
        {
            return;
        }
    }

    {
        boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
        {
            auto spSubscribeMsg = CPeerSubscribeMessageOutBound::Create();
            spSubscribeMsg->nNonce = (*it).first;
            spSubscribeMsg->hashFork = pCoreProtocol->GetGenesisBlockHash();
            spSubscribeMsg->vecForks.push_back(subscribeMsg.hashFork);
            PUBLISH_MESSAGE(spSubscribeMsg);
            DispatchGetBlocksEvent(it->first, subscribeMsg.hashFork);
        }
    }
}

void CNetChannelController::HandleUnsubscribeFork(const CUnsubscribeForkMessage& unsubscribeMsg)
{
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
        if (!mapSched.erase(unsubscribeMsg.hashFork))
        {
            return;
        }
    }

    {
        boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
        {
            auto spUnsubscribeMsg = CPeerUnsubscribeMessageOutBound::Create();
            spUnsubscribeMsg->nNonce = (*it).first;
            spUnsubscribeMsg->hashFork = pCoreProtocol->GetGenesisBlockHash();
            spUnsubscribeMsg->vecForks.push_back(unsubscribeMsg.hashFork);
            PUBLISH_MESSAGE(spUnsubscribeMsg);
        }
    }
}

void CNetChannelController::HandleActive(const CPeerActiveMessage& activeMsg)
{
    uint64 nNonce = activeMsg.nNonce;
    if ((activeMsg.address.nService & network::NODE_NETWORK))
    {
        DispatchGetBlocksEvent(nNonce, pCoreProtocol->GetGenesisBlockHash());

        auto spSubscribeMsg = CPeerSubscribeMessageOutBound::Create();
        spSubscribeMsg->nNonce = nNonce;
        spSubscribeMsg->hashFork = pCoreProtocol->GetGenesisBlockHash();
        {
            boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
            for (map<uint256, CSchedule>::iterator it = mapSched.begin(); it != mapSched.end(); ++it)
            {
                if ((*it).first != pCoreProtocol->GetGenesisBlockHash())
                {
                    spSubscribeMsg->vecForks.push_back(it->first);
                }
            }
        }
        if (!spSubscribeMsg->vecForks.empty())
        {
            PUBLISH_MESSAGE(spSubscribeMsg);
        }
    }
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
        mapPeer[nNonce] = CNetChannelPeer(activeMsg.address.nService, pCoreProtocol->GetGenesisBlockHash());
        mapUnsync[pCoreProtocol->GetGenesisBlockHash()].insert(nNonce);
    }
    NotifyPeerUpdate(nNonce, true, activeMsg.address);
}

void CNetChannelController::HandleDeactive(const CPeerDeactiveMessage& deactiveMsg)
{
    uint64 nNonce = deactiveMsg.nNonce;
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
        for (map<uint256, CSchedule>::iterator it = mapSched.begin(); it != mapSched.end(); ++it)
        {
            CSchedule& sched = (*it).second;
            set<uint64> setSchedPeer;
            sched.RemovePeer(nNonce, setSchedPeer);

            for (const uint64 nNonceSched : setSchedPeer)
            {
                SchedulePeerInv(nNonceSched, (*it).first, sched);
            }
        }
    }
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);

        map<uint64, CNetChannelPeer>::iterator it = mapPeer.find(nNonce);
        if (it != mapPeer.end())
        {
            for (auto& subFork : (*it).second.mapSubscribedFork)
            {
                mapUnsync[subFork.first].erase(nNonce);
            }
            mapPeer.erase(nNonce);
        }
    }
    NotifyPeerUpdate(nNonce, false, deactiveMsg.address);
}

void CNetChannelController::HandleSubscribe(const CPeerSubscribeMessageInBound& subscribeMsg)
{
    uint64 nNonce = subscribeMsg.nNonce;
    const uint256& hashFork = subscribeMsg.hashFork;
    if (hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
        map<uint64, CNetChannelPeer>::iterator it = mapPeer.find(nNonce);
        if (it != mapPeer.end())
        {
            for (const uint256& hash : subscribeMsg.vecForks)
            {
                (*it).second.Subscribe(hash);
                mapUnsync[hash].insert(nNonce);

                {
                    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
                    if (mapSched.count(hash))
                    {
                        DispatchGetBlocksEvent(nNonce, hash);
                    }
                }
            }
        }
    }
    else
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, "SubscribeMessage");
    }
}

void CNetChannelController::HandleUnsubscribe(const CPeerUnsubscribeMessageInBound& unsubscribeMsg)
{
    uint64 nNonce = unsubscribeMsg.nNonce;
    const uint256& hashFork = unsubscribeMsg.hashFork;
    if (hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
        map<uint64, CNetChannelPeer>::iterator it = mapPeer.find(nNonce);
        if (it != mapPeer.end())
        {
            for (const uint256& hash : unsubscribeMsg.vecForks)
            {
                (*it).second.Unsubscribe(hash);
                mapUnsync[hash].erase(nNonce);
            }
        }
    }
    else
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, "UnsubscribeMessage");
    }
}

void CNetChannelController::HandleInv(const CPeerInvMessageInBound& invMsg)
{
    uint64 nNonce = invMsg.nNonce;
    const uint256& hashFork = invMsg.hashFork;
    try
    {
        if (invMsg.vecInv.size() > network::CInv::MAX_INV_COUNT)
        {
            throw runtime_error("Inv count overflow.");
        }

        {
            boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
            CSchedule& sched = GetSchedule(hashFork);
            vector<uint256> vTxHash;
            for (const network::CInv& inv : invMsg.vecInv)
            {
                if ((inv.nType == network::CInv::MSG_TX && !pTxPoolCtrl->Exists(inv.nHash))
                    || (inv.nType == network::CInv::MSG_BLOCK && !pWorldLineCtrl->Exists(inv.nHash)))
                {
                    sched.AddNewInv(inv, nNonce);
                    if (inv.nType == network::CInv::MSG_TX)
                    {
                        vTxHash.push_back(inv.nHash);
                    }
                }
            }
            if (!vTxHash.empty())
            {
                boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
                mapPeer[nNonce].AddKnownTx(hashFork, vTxHash);
            }
            SchedulePeerInv(nNonce, hashFork, sched);
        }
    }
    catch (...)
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, "InvMessage");
    }
}

void CNetChannelController::HandleGetData(const CPeerGetDataMessageInBound& getDataMsg)
{
    uint64 nNonce = getDataMsg.nNonce;
    const uint256& hashFork = getDataMsg.hashFork;
    for (const network::CInv& inv : getDataMsg.vecInv)
    {
        if (inv.nType == network::CInv::MSG_TX)
        {
            auto spTxMsg = CPeerTxMessageOutBound::Create();
            spTxMsg->nNonce = nNonce;
            spTxMsg->hashFork = hashFork;
            if (pTxPoolCtrl->Get(inv.nHash, spTxMsg->tx))
            {
                PUBLISH_MESSAGE(spTxMsg);
            }
            else if (pWorldLineCtrl->GetTransaction(inv.nHash, spTxMsg->tx))
            {
                PUBLISH_MESSAGE(spTxMsg);
            }
            else
            {
                // TODO: Penalize
            }
        }
        else if (inv.nType == network::CInv::MSG_BLOCK)
        {
            auto spBlockMsg = CPeerBlockMessageOutBound::Create();
            spBlockMsg->nNonce = nNonce;
            spBlockMsg->hashFork = hashFork;
            if (pWorldLineCtrl->GetBlock(inv.nHash, spBlockMsg->block))
            {
                PUBLISH_MESSAGE(spBlockMsg);
            }
            else
            {
                // TODO: Penalize
            }
        }
    }
}

void CNetChannelController::HandleGetBlocks(const CPeerGetBlocksMessageInBound& getBlocksMsg)
{
    uint64 nNonce = getBlocksMsg.nNonce;
    const uint256& hashFork = getBlocksMsg.hashFork;
    vector<uint256> vBlockHash;
    if (!pWorldLineCtrl->GetBlockInv(hashFork, getBlocksMsg.blockLocator, vBlockHash, MAX_GETBLOCKS_COUNT))
    {
        CBlock block;
        if (!pWorldLineCtrl->GetBlock(hashFork, block))
        {
            //DispatchMisbehaveEvent(nNonce,CEndpointManager::DDOS_ATTACK,"eventGetBlocks");
            //return true;
        }
    }

    auto spInvMsg = CPeerInvMessageOutBound::Create();
    spInvMsg->nNonce = nNonce;
    spInvMsg->hashFork = hashFork;
    for (const uint256& hash : vBlockHash)
    {
        spInvMsg->vecInv.push_back(network::CInv(network::CInv::MSG_BLOCK, hash));
    }
    PUBLISH_MESSAGE(spInvMsg);
}

void CNetChannelController::HandlePeerTx(const CPeerTxMessageInBound& txMsg)
{
    uint64 nNonce = txMsg.nNonce;
    const uint256& hashFork = txMsg.hashFork;
    const CTransaction& tx = txMsg.tx;
    uint256 txid = tx.GetHash();

    try
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);

        set<uint64> setSchedPeer, setMisbehavePeer;
        CSchedule& sched = GetSchedule(hashFork);

        if (!sched.ReceiveTx(nNonce, txid, tx, setSchedPeer))
        {
            throw runtime_error("Failed to receive tx");
        }

        uint256 hashForkAnchor;
        int nHeightAnchor;
        if (pWorldLineCtrl->GetBlockLocation(tx.hashAnchor, hashForkAnchor, nHeightAnchor)
            && hashForkAnchor == hashFork)
        {
            set<uint256> setMissingPrevTx;
            if (!GetMissingPrevTx(tx, setMissingPrevTx))
            {
                AddNewTx(hashFork, txid, sched, setSchedPeer, setMisbehavePeer);
            }
            else
            {
                for (const uint256& prev : setMissingPrevTx)
                {
                    sched.AddOrphanTxPrev(txid, prev);
                    network::CInv inv(network::CInv::MSG_TX, prev);
                    if (!sched.Exists(inv))
                    {
                        for (const uint64 nNonceSched : setSchedPeer)
                        {
                            sched.AddNewInv(inv, nNonceSched);
                        }
                    }
                }
            }
        }
        else
        {
            sched.InvalidateTx(txid, setMisbehavePeer);
            setMisbehavePeer.clear();
        }
        PostAddNew(hashFork, sched, setSchedPeer, setMisbehavePeer);
    }
    catch (...)
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, "TxMessage");
    }
}

void CNetChannelController::HandlePeerBlock(const CPeerBlockMessageInBound& blockMsg)
{
    uint64 nNonce = blockMsg.nNonce;
    const uint256& hashFork = blockMsg.hashFork;
    const CBlock& block = blockMsg.block;
    uint256 hash = block.GetHash();

    try
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);

        set<uint64> setSchedPeer, setMisbehavePeer;
        CSchedule& sched = GetSchedule(hashFork);

        if (!sched.ReceiveBlock(nNonce, hash, block, setSchedPeer))
        {
            throw runtime_error("Failed to receive block");
        }

        uint256 hashForkPrev;
        int nHeightPrev;
        if (pWorldLineCtrl->GetBlockLocation(block.hashPrev, hashForkPrev, nHeightPrev))
        {
            if (hashForkPrev == hashFork)
            {
                AddNewBlock(hashFork, hash, sched, setSchedPeer, setMisbehavePeer);
            }
            else
            {
                sched.InvalidateBlock(hash, setMisbehavePeer);
            }
        }
        else
        {
            sched.AddOrphanBlockPrev(hash, block.hashPrev);
        }

        PostAddNew(hashFork, sched, setSchedPeer, setMisbehavePeer);
    }
    catch (...)
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, "BlockMessage");
    }
}

CSchedule& CNetChannelController::GetSchedule(const uint256& hashFork)
{
    map<uint256, CSchedule>::iterator it = mapSched.find(hashFork);
    if (it == mapSched.end())
    {
        throw runtime_error("Unknown fork for scheduling.");
    }
    return ((*it).second);
}

void CNetChannelController::NotifyPeerUpdate(uint64 nNonce, bool fActive, const network::CAddress& addrPeer)
{
    CNetworkPeerUpdate update;
    update.nPeerNonce = nNonce;
    update.fActive = fActive;
    // update.addrPeer = addrPeer;
    pService->NotifyNetworkPeerUpdate(update);
}

void CNetChannelController::DispatchGetBlocksEvent(uint64 nNonce, const uint256& hashFork)
{
    auto spGetBlocksMsg = CPeerGetBlocksMessageOutBound::Create();
    spGetBlocksMsg->nNonce = nNonce;
    spGetBlocksMsg->hashFork = hashFork;
    if (pWorldLineCtrl->GetBlockLocator(hashFork, spGetBlocksMsg->blockLocator))
    {
        PUBLISH_MESSAGE(spGetBlocksMsg);
    }
}

void CNetChannelController::DispatchAwardEvent(uint64 nNonce, CEndpointManager::Bonus bonus)
{
    auto spNetRewardMsg = CPeerNetRewardMessage::Create();
    spNetRewardMsg->nNonce = nNonce;
    spNetRewardMsg->bonus = bonus;
    PUBLISH_MESSAGE(spNetRewardMsg);
}

void CNetChannelController::DispatchMisbehaveEvent(uint64 nNonce, CEndpointManager::CloseReason reason, const std::string& strCaller)
{
    if (!strCaller.empty())
    {
        Log("DispatchMisbehaveEvent : %s\n", strCaller.c_str());
    }

    auto spNetCloseMsg = xengine::CPeerNetCloseMessage::Create();
    spNetCloseMsg->nNonce = nNonce;
    spNetCloseMsg->closeReason = reason;
    PUBLISH_MESSAGE(spNetCloseMsg);
}

void CNetChannelController::SchedulePeerInv(uint64 nNonce, const uint256& hashFork, CSchedule& sched)
{
    auto spGetData = CPeerGetDataMessageOutBound::Create();
    spGetData->nNonce = nNonce;
    spGetData->hashFork = hashFork;
    bool fMissingPrev = false;
    bool fEmpty = true;
    if (sched.ScheduleBlockInv(nNonce, spGetData->vecInv, MAX_PEER_SCHED_COUNT, fMissingPrev, fEmpty))
    {
        if (fMissingPrev)
        {
            DispatchGetBlocksEvent(nNonce, hashFork);
        }
        else if (spGetData->vecInv.empty())
        {
            if (!sched.ScheduleTxInv(nNonce, spGetData->vecInv, MAX_PEER_SCHED_COUNT))
            {
                DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, "SchedulePeerInv1");
            }
        }
        SetPeerSyncStatus(nNonce, hashFork, fEmpty);
    }
    else
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, "SchedulePeerInv2");
    }
    if (!spGetData->vecInv.empty())
    {
        PUBLISH_MESSAGE(spGetData);
    }
}

bool CNetChannelController::GetMissingPrevTx(const CTransaction& tx, set<uint256>& setMissingPrevTx)
{
    setMissingPrevTx.clear();
    for (const CTxIn& txin : tx.vInput)
    {
        const uint256& prev = txin.prevout.hash;
        if (!setMissingPrevTx.count(prev))
        {
            if (!pTxPoolCtrl->Exists(prev) && !pWorldLineCtrl->ExistsTx(prev))
            {
                setMissingPrevTx.insert(prev);
            }
        }
    }
    return (!setMissingPrevTx.empty());
}

void CNetChannelController::AddNewBlock(const uint256& hashFork, const uint256& hash, CSchedule& sched,
                              set<uint64>& setSchedPeer, set<uint64>& setMisbehavePeer)
{
    vector<uint256> vBlockHash;
    vBlockHash.push_back(hash);
    for (size_t i = 0; i < vBlockHash.size(); i++)
    {
        uint256 hashBlock = vBlockHash[i];
        uint64 nNonceSender = 0;
        CBlock* pBlock = sched.GetBlock(hashBlock, nNonceSender);
        if (pBlock != nullptr)
        {
            Errno err = pDispatcher->AddNewBlock(*pBlock, nNonceSender);
            if (err == OK)
            {
                for (const CTransaction& tx : pBlock->vtx)
                {
                    uint256 txid = tx.GetHash();
                    sched.RemoveInv(network::CInv(network::CInv::MSG_TX, txid), setSchedPeer);
                }

                set<uint64> setKnownPeer;
                sched.GetNextBlock(hashBlock, vBlockHash);
                sched.RemoveInv(network::CInv(network::CInv::MSG_BLOCK, hashBlock), setKnownPeer);
                DispatchAwardEvent(nNonceSender, CEndpointManager::VITAL_DATA);
                setSchedPeer.insert(setKnownPeer.begin(), setKnownPeer.end());
            }
            else if (err == ERR_ALREADY_HAVE && pBlock->IsVacant())
            {
                set<uint64> setKnownPeer;
                sched.GetNextBlock(hashBlock, vBlockHash);
                sched.RemoveInv(network::CInv(network::CInv::MSG_BLOCK, hashBlock), setKnownPeer);
                setSchedPeer.insert(setKnownPeer.begin(), setKnownPeer.end());
            }
            else
            {
                sched.InvalidateBlock(hashBlock, setMisbehavePeer);
            }
        }
    }
}

void CNetChannelController::AddNewTx(const uint256& hashFork, const uint256& txid, CSchedule& sched,
                           set<uint64>& setSchedPeer, set<uint64>& setMisbehavePeer)
{
    set<uint256> setTx;
    vector<uint256> vtx;

    vtx.push_back(txid);
    int nAddNewTx = 0;
    for (size_t i = 0; i < vtx.size(); i++)
    {
        uint256 hashTx = vtx[i];
        uint64 nNonceSender = 0;
        CTransaction* pTx = sched.GetTransaction(hashTx, nNonceSender);
        if (pTx != nullptr)
        {
            if (pWorldLineCtrl->ExistsTx(txid))
            {
                return;
            }

            Errno err = pDispatcher->AddNewTx(*pTx, nNonceSender);
            if (err == OK)
            {
                sched.GetNextTx(hashTx, vtx, setTx);
                sched.RemoveInv(network::CInv(network::CInv::MSG_TX, hashTx), setSchedPeer);
                DispatchAwardEvent(nNonceSender, CEndpointManager::MAJOR_DATA);
                nAddNewTx++;
            }
            else if (err != ERR_MISSING_PREV)
            {
                sched.InvalidateTx(hashTx, setMisbehavePeer);
            }
        }
    }
    if (nAddNewTx)
    {
        CBroadcastTxInvMessage msg(hashFork);
        HandleBroadcastTxInv(msg);
    }
}

void CNetChannelController::PostAddNew(const uint256& hashFork, CSchedule& sched,
                             set<uint64>& setSchedPeer, set<uint64>& setMisbehavePeer)
{
    for (const uint64 nNonceSched : setSchedPeer)
    {
        if (!setMisbehavePeer.count(nNonceSched))
        {
            SchedulePeerInv(nNonceSched, hashFork, sched);
        }
    }

    for (const uint64 nNonceMisbehave : setMisbehavePeer)
    {
        DispatchMisbehaveEvent(nNonceMisbehave, CEndpointManager::DDOS_ATTACK, "PostAddNew");
    }
}

void CNetChannelController::SetPeerSyncStatus(uint64 nNonce, const uint256& hashFork, bool fSync)
{
    bool fInverted = false;
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
        CNetChannelPeer& peer = mapPeer[nNonce];
        if (!peer.SetSyncStatus(hashFork, fSync, fInverted))
        {
            return;
        }
    }

    if (fInverted)
    {
        if (fSync)
        {
            mapUnsync[hashFork].erase(nNonce);
            CBroadcastTxInvMessage msg(hashFork);
            HandleBroadcastTxInv(msg);
        }
        else
        {
            mapUnsync[hashFork].insert(nNonce);
        }
    }
}

void CNetChannelController::PushTxTimerFunc(uint32 nTimerId)
{
    boost::unique_lock<boost::mutex> lock(mtxPushTx);
    if (nTimerPushTx == nTimerId)
    {
        if (!setPushTxFork.empty())
        {
            set<uint256>::iterator it = setPushTxFork.begin();
            while (it != setPushTxFork.end())
            {
                if (PushTxInv(*it))
                {
                    setPushTxFork.erase(it++);
                }
                else
                {
                    ++it;
                }
            }
            nTimerPushTx = SetTimer(PUSHTX_TIMEOUT, boost::bind(&CNetChannelController::PushTxTimerFunc, this, _1));
        }
        else
        {
            nTimerPushTx = 0;
        }
    }
}

bool CNetChannelController::PushTxInv(const uint256& hashFork)
{
    // if (!IsForkSynchronized(hashFork))
    // {
    //     return false;
    // }

    bool fCompleted = true;
    vector<uint256> vTxPool;
    pTxPoolCtrl->ListTx(hashFork, vTxPool);
    if (!vTxPool.empty() && !mapPeer.empty())
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
        {
            CNetChannelPeer& peer = it->second;
            if (peer.IsSubscribed(hashFork))
            {
                auto spInvMsg = CPeerInvMessageOutBound::Create();
                spInvMsg->nNonce = it->first;
                spInvMsg->hashFork = hashFork;

                peer.MakeTxInv(hashFork, vTxPool, spInvMsg->vecInv, network::CInv::MAX_INV_COUNT);
                if (!spInvMsg->vecInv.empty())
                {
                    PUBLISH_MESSAGE(spInvMsg);
                    if (fCompleted && spInvMsg->vecInv.size() == network::CInv::MAX_INV_COUNT)
                    {
                        fCompleted = false;
                    }
                }
            }
        }
    }
    return fCompleted;
}

} // namespace bigbang
