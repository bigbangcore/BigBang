// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "netchn.h"

#include <algorithm>
#include <boost/bind.hpp>

#include "schedule.h"

using namespace std;
using namespace xengine;
using boost::asio::ip::tcp;

#define PUSHTX_TIMEOUT (1000)

namespace bigbang
{

//////////////////////////////
// CNetChannelControllerPeer

void CNetChannelControllerPeer::CNetChannelControllerPeerFork::AddKnownTx(const vector<uint256>& vTxHash)
{
    ClearExpiredTx(vTxHash.size());
    for (const uint256& txid : vTxHash)
    {
        setKnownTx.insert(CPeerKnownTx(txid));
    }
}

void CNetChannelControllerPeer::CNetChannelControllerPeerFork::ClearExpiredTx(size_t nReserved)
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

bool CNetChannelControllerPeer::IsSynchronized(const uint256& hashFork) const
{
    map<uint256, CNetChannelControllerPeerFork>::const_iterator it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        return (*it).second.fSynchronized;
    }
    return false;
}

bool CNetChannelControllerPeer::SetSyncStatus(const uint256& hashFork, bool fSync, bool& fInverted)
{
    map<uint256, CNetChannelControllerPeerFork>::iterator it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        fInverted = ((*it).second.fSynchronized != fSync);
        (*it).second.fSynchronized = fSync;
        return true;
    }
    return false;
}

void CNetChannelControllerPeer::AddKnownTx(const uint256& hashFork, const vector<uint256>& vTxHash)
{
    map<uint256, CNetChannelControllerPeerFork>::iterator it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        (*it).second.AddKnownTx(vTxHash);
    }
}

void CNetChannelControllerPeer::MakeTxInv(const uint256& hashFork, const vector<uint256>& vTxPool,
                                          vector<network::CInv>& vInv, size_t nMaxCount)
{
    map<uint256, CNetChannelControllerPeerFork>::iterator it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        vector<uint256> vTxHash;
        CNetChannelControllerPeerFork& peerFork = (*it).second;
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

void CNetChannel::CleanUpForkScheduler()
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    mapSched.clear();
}

bool CNetChannel::AddNewForkSchedule(const uint256& hashFork)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    return mapSched.insert(make_pair(hashFork, CSchedule())).second;
}

bool CNetChannel::RemoveForkSchudule(const uint256& hashFork)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    return mapSched.erase(hashFork) != 0;
}

bool CNetChannel::AddNewInvSchedule(uint64 nNonce, const uint256& hashFork, const network::CInv& inv)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    if (!ContainsSchedule(hashFork))
    {
        return false;
    }

    CSchedule& sched = GetSchedule(hashFork);
    sched.AddNewInv(inv, nNonce);
    return true;
}

std::vector<uint256> CNetChannel::GetAllForks() const
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    std::vector<uint256> hashForks;
    for (map<uint256, CSchedule>::const_iterator it = mapSched.begin(); it != mapSched.end(); ++it)
    {
        hashForks.emplace_back(it->first);
    }
    return hashForks;
}

bool CNetChannel::IsForkSynchronized(const uint256& hashFork) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
    map<uint256, set<uint64>>::const_iterator it = mapUnsync.find(hashFork);
    return (it == mapUnsync.end() || (*it).second.empty());
}

bool CNetChannel::GetKnownPeers(const uint256& hashFork, const uint256& hashBlock, std::set<uint64>& setKnownPeers) const
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);

    if (!ContainsSchedule(hashFork))
    {
        return false;
    }

    const CSchedule& sched = GetSchedule(hashFork);
    sched.GetKnownPeer(network::CInv(network::CInv::MSG_BLOCK, hashBlock), setKnownPeers);
    return true;
}

void CNetChannel::SubscribePeerFork(uint64 nNonce, const uint256& hashFork)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
    map<uint64, CNetChannelControllerPeer>::iterator it = mapPeer.find(nNonce);
    if (it != mapPeer.end())
    {
        it->second.Subscribe(hashFork);
        AddUnSynchronizedForkPeer(nNonce, hashFork);
    }
}

void CNetChannel::UnsubscribePeerFork(uint64 nNonce, const uint256& hashFork)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
    map<uint64, CNetChannelControllerPeer>::iterator it = mapPeer.find(nNonce);
    if (it != mapPeer.end())
    {
        it->second.Unsubscribe(hashFork);
        RemoveUnSynchronizedForkPeer(nNonce, hashFork);
    }
}

void CNetChannel::AddKnownTxPeer(uint64 nNonce, const uint256& hashFork, const std::vector<uint256>& vTxHash)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
    mapPeer[nNonce].AddKnownTx(hashFork, vTxHash);
}

bool CNetChannel::IsPeerEmpty() const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
    return mapPeer.empty();
}

void CNetChannel::MakeTxInv(const uint256& hashFork, const std::vector<uint256>& vTxPool, std::vector<MakeTxInvResultPair>& txInvResult)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
    for (map<uint64, CNetChannelControllerPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
    {
        CNetChannelControllerPeer& peer = it->second;
        if (peer.IsSubscribed(hashFork))
        {

            MakeTxInvResultType vecInv;
            peer.MakeTxInv(hashFork, vTxPool, vecInv, network::CInv::MAX_INV_COUNT);
            if (!vecInv.empty())
            {
                txInvResult.emplace_back(std::make_pair(it->first, vecInv));
            }
        }
    }
}

bool CNetChannel::ReceiveScheduleTx(uint64 nNonce, const uint256& hashFork, const uint256& txid, const CTransaction& tx, std::set<uint64>& setSchedPeer)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    if (!ContainsSchedule(hashFork))
    {
        return false;
    }

    CSchedule& sched = GetSchedule(hashFork);
    return sched.ReceiveTx(nNonce, txid, tx, setSchedPeer);
}

bool CNetChannel::ReceiveScheduleBlock(uint64 nNonce, const uint256& hashFork, const uint256& blockHash, const CBlock& block, std::set<uint64>& setSchedPeer)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    if (!ContainsSchedule(hashFork))
    {
        return false;
    }

    CSchedule& sched = GetSchedule(hashFork);
    return sched.ReceiveBlock(nNonce, blockHash, block, setSchedPeer);
}

void CNetChannel::InvalidateScheduleTx(const uint256& hashFork, const uint256& txid, std::set<uint64>& setMisbehavePeer)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    if (!ContainsSchedule(hashFork))
    {
        return;
    }

    CSchedule& sched = GetSchedule(hashFork);
    sched.InvalidateTx(txid, setMisbehavePeer);
}

void CNetChannel::InvalidateScheduleBlock(const uint256& hashFork, const uint256& blockHash, std::set<uint64>& setMisbehavePeer)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    if (!ContainsSchedule(hashFork))
    {
        return;
    }

    CSchedule& sched = GetSchedule(hashFork);
    sched.InvalidateBlock(blockHash, setMisbehavePeer);
}

void CNetChannel::AddScheduleOrphanTxPrev(const uint256& hashFork, const uint256& txid, const uint256& prevtxid)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    if (!ContainsSchedule(hashFork))
    {
        return;
    }

    CSchedule& sched = GetSchedule(hashFork);
    sched.AddOrphanTxPrev(txid, prevtxid);
}

void CNetChannel::AddOrphanBlockPrev(const uint256& hashFork, const uint256& blockHash, const uint256& prevHash)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    if (!ContainsSchedule(hashFork))
    {
        return;
    }

    CSchedule& sched = GetSchedule(hashFork);
    sched.AddOrphanBlockPrev(blockHash, prevHash);
}

CTransaction* CNetChannel::GetScheduleTransaction(const uint256& hashFork, const uint256& txid, uint64& nNonceSender)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    if (!ContainsSchedule(hashFork))
    {
        return nullptr;
    }

    CSchedule& sched = GetSchedule(hashFork);
    return sched.GetTransaction(txid, nNonceSender);
}

void CNetChannel::GetScheduleNextTx(const uint256& hashFork, const uint256& txid, std::vector<uint256>& vNextTx)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    if (!ContainsSchedule(hashFork))
    {
        return;
    }
    std::set<uint256> setNextTx;
    CSchedule& sched = GetSchedule(hashFork);
    sched.GetNextTx(txid, vNextTx, setNextTx);
}

void CNetChannel::RemoveScheduleInv(const uint256& hashFork, const network::CInv& inv, std::set<uint64>& setSchedPeer)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    if (!ContainsSchedule(hashFork))
    {
        return;
    }

    CSchedule& sched = GetSchedule(hashFork);
    sched.RemoveInv(inv, setSchedPeer);
}

CBlock* CNetChannel::GetScheduleBlock(const uint256& hashFork, const uint256& hashBlock, uint64& nNonceSender)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    if (!ContainsSchedule(hashFork))
    {
        return nullptr;
    }

    CSchedule& sched = GetSchedule(hashFork);
    return sched.GetBlock(hashBlock, nNonceSender);
}

void CNetChannel::GetScheduleNextBlock(const uint256& hashFork, const uint256& hashBlock, vector<uint256>& vNextBlock)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    if (!ContainsSchedule(hashFork))
    {
        return;
    }

    CSchedule& sched = GetSchedule(hashFork);
    sched.GetNextBlock(hashBlock, vNextBlock);
}

bool CNetChannel::ExistsScheduleInv(const uint256& hashFork, const network::CInv& inv) const
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    if (!ContainsSchedule(hashFork))
    {
        return false;
    }
    const CSchedule& sched = GetSchedule(hashFork);
    return sched.Exists(inv);
}

bool CNetChannel::ContainsPeerMT(uint64 nNonce) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
    map<uint64, CNetChannelControllerPeer>::const_iterator it = mapPeer.find(nNonce);
    return it != mapPeer.end();
}

bool CNetChannel::ContainsScheduleMT(const uint256& hashFork) const
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    map<uint256, CSchedule>::const_iterator it = mapSched.find(hashFork);
    return it != mapSched.end();
}

bool CNetChannel::ContainsSchedule(const uint256& hashFork) const
{
    map<uint256, CSchedule>::const_iterator it = mapSched.find(hashFork);
    return it != mapSched.end();
}

const CSchedule& CNetChannel::GetSchedule(const uint256& hashFork) const
{
    return mapSched.find(hashFork)->second;
}

CSchedule& CNetChannel::GetSchedule(const uint256& hashFork)
{
    return mapSched.find(hashFork)->second;
}

map<uint64, CNetChannelControllerPeer> CNetChannel::GetAllPeers() const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
    return mapPeer;
}

void CNetChannel::AddPeer(uint64 nNonce, uint64 nService, const uint256& hashFork)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
    mapPeer[nNonce] = CNetChannelControllerPeer(nService, hashFork);
    AddUnSynchronizedForkPeer(nNonce, hashFork);
}

void CNetChannel::RemovePeer(uint64 nNonce)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
    map<uint64, CNetChannelControllerPeer>::iterator it = mapPeer.find(nNonce);
    if (it != mapPeer.end())
    {
        for (const auto& subFork : (*it).second.mapSubscribedFork)
        {
            RemoveUnSynchronizedForkPeer(nNonce, subFork.first);
        }
        mapPeer.erase(nNonce);
    }
}

void CNetChannel::RemoveUnSynchronizedForkPeerMT(uint64 nNonce, const uint256& hashFork)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
    RemoveUnSynchronizedForkPeer(nNonce, hashFork);
}

void CNetChannel::AddUnSynchronizedForkPeerMT(uint64 nNonce, const uint256& hashFork)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
    AddUnSynchronizedForkPeer(nNonce, hashFork);
}

void CNetChannel::AddUnSynchronizedForkPeer(uint64 nNonce, const uint256& hashFork)
{
    mapUnsync[hashFork].insert(nNonce);
}

void CNetChannel::RemoveUnSynchronizedForkPeer(uint64 nNonce, const uint256& hashFork)
{
    mapUnsync[hashFork].erase(nNonce);
}

bool CNetChannel::SetPeerSyncStatus(uint64 nNonce, const uint256& hashFork, bool fSync, bool& fInverted)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
    CNetChannelControllerPeer& peer = mapPeer[nNonce];
    return peer.SetSyncStatus(hashFork, fSync, fInverted);
}

void CNetChannel::SchedulePeerInv(uint64 nNonce, const uint256& hashFork, CSchedule& sched, std::vector<ScheduleResultPair>& schedResult)
{
    bool fMissingPrev = false;
    bool fEmpty = true;
    std::vector<bigbang::network::CInv> vecInv;
    INetChannelModel::ScheduleResultPair result;
    result.first = nNonce;

    ScheduleTxInvRetBool txRetBool = true;
    ScheduleBlockInvRetBool blockRetBool = true;
    PrevMissingBool prevMissingBool = false;
    if (sched.ScheduleBlockInv(nNonce, vecInv, MAX_PEER_SCHED_COUNT, fMissingPrev, fEmpty))
    {
        if (fMissingPrev)
        {
            prevMissingBool = true;
        }
        else if (vecInv.empty())
        {
            if (!sched.ScheduleTxInv(nNonce, vecInv, MAX_PEER_SCHED_COUNT))
            {
                txRetBool = false;
            }
        }
    }
    else
    {
        blockRetBool = false;
    }

    schedResult.emplace_back(
        std::make_pair(nNonce,
                       std::make_tuple(blockRetBool,
                                       prevMissingBool,
                                       txRetBool,
                                       fEmpty,
                                       hashFork,
                                       vecInv)));
}

void CNetChannel::ScheduleDeactivePeerInv(uint64 nNonce, std::vector<ScheduleResultPair>& schedResult)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    for (map<uint256, CSchedule>::iterator it = mapSched.begin(); it != mapSched.end(); ++it)
    {
        CSchedule& sched = it->second;
        set<uint64> setSchedPeer;
        sched.RemovePeer(nNonce, setSchedPeer);

        for (const uint64 nNonceSched : setSchedPeer)
        {
            SchedulePeerInv(nNonceSched, it->first, sched, schedResult);
        }
    }
}

bool CNetChannel::ScheduleActivePeerInv(uint64 nNonce, const uint256& hashFork, std::vector<ScheduleResultPair>& schedResult)
{
    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
    if (!ContainsSchedule(hashFork))
    {
        return false;
    }
    CSchedule& sched = GetSchedule(hashFork);
    SchedulePeerInv(nNonce, hashFork, sched, schedResult);
    return true;
}

//////////////////////////////
// CNetChannelController

CNetChannelController::CNetChannelController()
{
    pCoreProtocol = nullptr;
    pWorldLineCtrl = nullptr;
    pTxPoolCtrl = nullptr;
    pService = nullptr;
    pNetChannelModel = nullptr;
}

CNetChannelController::~CNetChannelController()
{
}

bool CNetChannelController::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        ERROR("Failed to request coreprotocol\n");
        return false;
    }

    if (!GetObject("worldlinecontroller", pWorldLineCtrl))
    {
        ERROR("Failed to request worldline\n");
        return false;
    }

    if (!GetObject("txpoolcontroller", pTxPoolCtrl))
    {
        ERROR("Failed to request txpool\n");
        return false;
    }

    if (!GetObject("service", pService))
    {
        ERROR("Failed to request service\n");
        return false;
    }

    if (!GetObject("netchannelmodel", pNetChannelModel))
    {
        ERROR("Failed to request netchannel model\n");
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

    RegisterRefHandler<CAddedBlockMessage>(boost::bind(&CNetChannelController::HandleAddedNewBlock, this, _1));
    RegisterRefHandler<CAddedTxMessage>(boost::bind(&CNetChannelController::HandleAddedNewTx, this, _1));

    return true;
}

void CNetChannelController::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pWorldLineCtrl = nullptr;
    pTxPoolCtrl = nullptr;
    pService = nullptr;
    pNetChannelModel = nullptr;

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
        ERROR("Failed to start actor\n");
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

    pNetChannelModel->CleanUpForkScheduler();
}

void CNetChannelController::HandleBroadcastBlockInv(const CBroadcastBlockInvMessage& invMsg)
{

    set<uint64> setKnownPeer;
    if (!pNetChannelModel->GetKnownPeers(invMsg.hashFork, invMsg.hashBlock, setKnownPeer))
    {
        throw std::runtime_error("Unknown fork for scheduling (GetKnownPeers).");
    }

    const auto allPeers = pNetChannelModel->GetAllPeers();
    for (map<uint64, CNetChannelControllerPeer>::const_iterator it = allPeers.begin(); it != allPeers.end(); ++it)
    {
        uint64 nNonce = it->first;
        if (!setKnownPeer.count(nNonce) && it->second.IsSubscribed(invMsg.hashFork))
        {
            auto spInvMsg = CPeerInvMessageOutBound::Create();
            spInvMsg->nNonce = nNonce;
            spInvMsg->hashFork = invMsg.hashFork;
            spInvMsg->vecInv.push_back(network::CInv(network::CInv::MSG_BLOCK, invMsg.hashBlock));
            PUBLISH_MESSAGE(spInvMsg);
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
    if (!pNetChannelModel->AddNewForkSchedule(subscribeMsg.hashFork))
    {
        return;
    }

    const auto allPeers = pNetChannelModel->GetAllPeers();
    for (map<uint64, CNetChannelControllerPeer>::const_iterator it = allPeers.begin(); it != allPeers.end(); ++it)
    {
        auto spSubscribeMsg = CPeerSubscribeMessageOutBound::Create();
        spSubscribeMsg->nNonce = (*it).first;
        spSubscribeMsg->hashFork = pCoreProtocol->GetGenesisBlockHash();
        spSubscribeMsg->vecForks.push_back(subscribeMsg.hashFork);
        PUBLISH_MESSAGE(spSubscribeMsg);
        DispatchGetBlocksFromHashEvent(it->first, subscribeMsg.hashFork, uint256());
    }
}

void CNetChannelController::HandleUnsubscribeFork(const CUnsubscribeForkMessage& unsubscribeMsg)
{
    if (!pNetChannelModel->RemoveForkSchudule(unsubscribeMsg.hashFork))
    {
        return;
    }

    const auto allPeers = pNetChannelModel->GetAllPeers();
    for (map<uint64, CNetChannelControllerPeer>::const_iterator it = allPeers.begin(); it != allPeers.end(); ++it)
    {
        auto spUnsubscribeMsg = CPeerUnsubscribeMessageOutBound::Create();
        spUnsubscribeMsg->nNonce = (*it).first;
        spUnsubscribeMsg->hashFork = pCoreProtocol->GetGenesisBlockHash();
        spUnsubscribeMsg->vecForks.emplace_back(unsubscribeMsg.hashFork);
        PUBLISH_MESSAGE(spUnsubscribeMsg);
    }
}

void CNetChannelController::HandleActive(const CPeerActiveMessage& activeMsg)
{
    uint64 nNonce = activeMsg.nNonce;
    if ((activeMsg.address.nService & network::NODE_NETWORK))
    {
        DispatchGetBlocksFromHashEvent(nNonce, pCoreProtocol->GetGenesisBlockHash(), uint256());

        auto spSubscribeMsg = CPeerSubscribeMessageOutBound::Create();
        spSubscribeMsg->nNonce = nNonce;
        spSubscribeMsg->hashFork = pCoreProtocol->GetGenesisBlockHash();

        std::vector<uint256> allForks = pNetChannelModel->GetAllForks();

        std::copy_if(allForks.begin(), allForks.end(), spSubscribeMsg->vecForks.begin(),
                     [this](const uint256& hashFork) {
                         return hashFork != pCoreProtocol->GetGenesisBlockHash();
                     });

        if (!spSubscribeMsg->vecForks.empty())
        {
            PUBLISH_MESSAGE(spSubscribeMsg);
        }
    }

    pNetChannelModel->AddPeer(nNonce, activeMsg.address.nService, pCoreProtocol->GetGenesisBlockHash());
    NotifyPeerUpdate(nNonce, true, activeMsg.address);
}

void CNetChannelController::HandleDeactive(const CPeerDeactiveMessage& deactiveMsg)
{
    uint64 nNonce = deactiveMsg.nNonce;
    SchedulePeerInv(nNonce, uint256(), false, uint256());
    pNetChannelModel->RemovePeer(nNonce);
    NotifyPeerUpdate(nNonce, false, deactiveMsg.address);
}

void CNetChannelController::HandleSubscribe(const CPeerSubscribeMessageInBound& subscribeMsg)
{
    uint64 nNonce = subscribeMsg.nNonce;
    const uint256& hashFork = subscribeMsg.hashFork;
    if (hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        for (const uint256& hash : subscribeMsg.vecForks)
        {
            pNetChannelModel->SubscribePeerFork(nNonce, hash);

            if (pNetChannelModel->ContainsScheduleMT(hash))
            {
                DispatchGetBlocksFromHashEvent(nNonce, hash, uint256());
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
        for (const uint256& hash : unsubscribeMsg.vecForks)
        {
            pNetChannelModel->UnsubscribePeerFork(nNonce, hash);
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

        uint256 nLastHaveBlockHash;
        int lastHaveBlockIndex = 0;
        for (int i = 0; i < invMsg.vecInv.size(); ++i)
        {
            const auto& inv = invMsg.vecInv[i];
            if (inv.nType == network::CInv::MSG_BLOCK && pWorldLineCtrl->Exists(inv.nHash))
            {
                nLastHaveBlockHash = inv.nHash;
                lastHaveBlockIndex = i;
            }
        }

        int firstHaveNotBlockIndex = 0;
        if (lastHaveBlockIndex < invMsg.vecInv.size() - 1)
        {
            firstHaveNotBlockIndex = lastHaveBlockIndex + 1;
        }

        vector<uint256> vTxHash;
        for (int i = 0; i < invMsg.vecInv.size(); ++i)
        {
            const auto& inv = invMsg.vecInv[i];
            if (inv.nType == network::CInv::MSG_TX && !pTxPoolCtrl->Exists(inv.nHash))
            {
                pNetChannelModel->AddNewInvSchedule(nNonce, hashFork, inv);
                vTxHash.push_back(inv.nHash);
            }

            if (inv.nType == network::CInv::MSG_BLOCK
                && !pWorldLineCtrl->Exists(inv.nHash)
                && lastHaveBlockIndex > 0
                && firstHaveNotBlockIndex > 0
                && i < firstHaveNotBlockIndex)
            {
                pNetChannelModel->AddNewInvSchedule(nNonce, hashFork, inv);
            }
        }
        if (!vTxHash.empty())
        {
            pNetChannelModel->AddKnownTxPeer(nNonce, hashFork, vTxHash);
        }

        SchedulePeerInv(nNonce, hashFork, true, nLastHaveBlockHash);
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
        spInvMsg->vecInv.emplace_back(network::CInv(network::CInv::MSG_BLOCK, hash));
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

        set<uint64> setSchedPeer, setMisbehavePeer;
        if (!pNetChannelModel->ReceiveScheduleTx(nNonce, hashFork, txid, tx, setSchedPeer))
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
                AddNewTx(hashFork, txid, setSchedPeer, setMisbehavePeer);
            }
            else
            {
                for (const uint256& prev : setMissingPrevTx)
                {
                    pNetChannelModel->AddScheduleOrphanTxPrev(hashFork, txid, prev);
                    network::CInv inv(network::CInv::MSG_TX, prev);
                    if (!pNetChannelModel->ExistsScheduleInv(hashFork, inv))
                    {
                        for (const uint64 nNonceSched : setSchedPeer)
                        {
                            pNetChannelModel->AddNewInvSchedule(nNonceSched, hashFork, inv);
                        }
                    }
                }
            }
        }
        else
        {
            pNetChannelModel->InvalidateScheduleTx(hashFork, txid, setMisbehavePeer);
            setMisbehavePeer.clear();
        }
        PostAddNew(hashFork, setSchedPeer, setMisbehavePeer);
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
        set<uint64> setSchedPeer, setMisbehavePeer;
        if (!pNetChannelModel->ReceiveScheduleBlock(nNonce, hashFork, hash, block, setSchedPeer))
        {
            throw runtime_error("Failed to receive block");
        }

        uint256 hashForkPrev;
        int nHeightPrev;
        if (pWorldLineCtrl->GetBlockLocation(block.hashPrev, hashForkPrev, nHeightPrev))
        {
            if (hashForkPrev == hashFork)
            {
                AddNewBlock(hashFork, hash, setSchedPeer, setMisbehavePeer);
            }
            else
            {
                pNetChannelModel->InvalidateScheduleBlock(hashFork, hash, setMisbehavePeer);
            }
        }
        else
        {
            pNetChannelModel->AddOrphanBlockPrev(hashFork, hash, block.hashPrev);
        }

        PostAddNew(hashFork, setSchedPeer, setMisbehavePeer);
    }
    catch (...)
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, "BlockMessage");
    }
}

void CNetChannelController::HandleAddedNewBlock(const CAddedBlockMessage& addedMsg)
{
    Errno err = static_cast<Errno>(addedMsg.nError);
    const uint256& hashFork = addedMsg.hashFork;
    const CBlock& block = addedMsg.block;
    const uint256& hashBlock = addedMsg.block.GetHash();
    const uint64& nNonceSender = addedMsg.spNonce->nNonce;
    std::vector<uint256> vNextBlockHash;
    set<uint64> setSchedPeer, setMisbehavePeer;
    if (err == OK)
    {
        for (const CTransaction& tx : addedMsg.block.vtx)
        {
            uint256 txid = tx.GetHash();
            pNetChannelModel->RemoveScheduleInv(hashFork, network::CInv(network::CInv::MSG_TX, txid), setSchedPeer);
        }

        set<uint64> setKnownPeer;
        pNetChannelModel->GetScheduleNextBlock(hashFork, hashBlock, vNextBlockHash);
        pNetChannelModel->RemoveScheduleInv(hashFork, network::CInv(network::CInv::MSG_BLOCK, hashBlock), setKnownPeer);
        DispatchAwardEvent(nNonceSender, CEndpointManager::VITAL_DATA);
        setSchedPeer.insert(setKnownPeer.begin(), setKnownPeer.end());
    }
    else if (err == ERR_ALREADY_HAVE && block.IsVacant())
    {
        set<uint64> setKnownPeer;
        pNetChannelModel->GetScheduleNextBlock(hashFork, hashBlock, vNextBlockHash);
        pNetChannelModel->RemoveScheduleInv(hashFork, network::CInv(network::CInv::MSG_BLOCK, hashBlock), setKnownPeer);
        setSchedPeer.insert(setKnownPeer.begin(), setKnownPeer.end());
    }
    else
    {
        pNetChannelModel->InvalidateScheduleBlock(hashFork, hashBlock, setMisbehavePeer);
    }

    for (const uint256& nextBlockHash : vNextBlockHash)
    {
        uint64 nNonceSenderTemp = 0;
        CBlock* pBlock = pNetChannelModel->GetScheduleBlock(hashFork, nextBlockHash, nNonceSenderTemp);
        if (pBlock)
        {
            auto spAddBlockMsg = CAddBlockMessage::Create();
            spAddBlockMsg->hashFork = hashFork;
            spAddBlockMsg->spNonce = CNonce::Create(nNonceSenderTemp);
            spAddBlockMsg->block = *pBlock;
            PUBLISH_MESSAGE(spAddBlockMsg);
        }
    }

    PostAddNew(hashFork, setSchedPeer, setMisbehavePeer);
}

void CNetChannelController::HandleAddedNewTx(const CAddedTxMessage& addedMsg)
{
    Errno err = static_cast<Errno>(addedMsg.nError);
    const uint256& hashFork = addedMsg.hashFork;
    const uint256& hashTx = addedMsg.tx.GetHash();
    const uint64& nNonceSender = addedMsg.spNonce->nNonce;
    std::vector<uint256> vNextTxChain;
    set<uint64> setSchedPeer, setMisbehavePeer;
    if (err == OK)
    {
        pNetChannelModel->GetScheduleNextTx(hashFork, hashTx, vNextTxChain);
        pNetChannelModel->RemoveScheduleInv(hashFork, network::CInv(network::CInv::MSG_TX, hashTx), setSchedPeer);
        DispatchAwardEvent(nNonceSender, CEndpointManager::MAJOR_DATA);

        CBroadcastTxInvMessage msg(hashFork);
        HandleBroadcastTxInv(msg);
    }
    else if (err == ERR_ALREADY_HAVE)
    {
        set<uint64> setKnownPeer;
        pNetChannelModel->GetScheduleNextTx(hashFork, hashTx, vNextTxChain);
        pNetChannelModel->RemoveScheduleInv(hashFork, network::CInv(network::CInv::MSG_TX, hashTx), setKnownPeer);
        setSchedPeer.insert(setKnownPeer.begin(), setKnownPeer.end());
    }
    else if (err != ERR_MISSING_PREV)
    {
        pNetChannelModel->InvalidateScheduleTx(hashFork, hashTx, setMisbehavePeer);
    }

    for (const uint256& txHash : vNextTxChain)
    {
        uint64 nNonceSenderTemp = 0;
        CTransaction* pTx = pNetChannelModel->GetScheduleTransaction(hashFork, txHash, nNonceSenderTemp);
        if (pTx)
        {
            auto spAddTxMsg = CAddTxMessage::Create();
            spAddTxMsg->spNonce = CNonce::Create(nNonceSenderTemp);
            spAddTxMsg->hashFork = hashFork;
            spAddTxMsg->tx = *pTx;
            PUBLISH_MESSAGE(spAddTxMsg);
        }
    }

    PostAddNew(hashFork, setSchedPeer, setMisbehavePeer);
}

void CNetChannelController::NotifyPeerUpdate(uint64 nNonce, bool fActive, const network::CAddress& addrPeer)
{
    CNetworkPeerUpdate update;
    update.nPeerNonce = nNonce;
    update.fActive = fActive;
    // update.addrPeer = addrPeer;
    pService->NotifyNetworkPeerUpdate(update);
}

void CNetChannelController::DispatchGetBlocksFromHashEvent(uint64 nNonce, const uint256& hashFork, const uint256& hashBlock)
{
    auto spGetBlocksMsg = CPeerGetBlocksMessageOutBound::Create();
    spGetBlocksMsg->nNonce = nNonce;
    spGetBlocksMsg->hashFork = hashFork;

    if (!hashBlock)
    {
        if (pWorldLineCtrl->GetBlockLocator(hashFork, spGetBlocksMsg->blockLocator))
        {
            PUBLISH_MESSAGE(spGetBlocksMsg);
        }
    }
    else
    {
        if (pWorldLineCtrl->GetBlockLocatorFromHash(hashFork, hashBlock, spGetBlocksMsg->blockLocator))
        {
            PUBLISH_MESSAGE(spGetBlocksMsg);
        }
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
        INFO("DispatchMisbehaveEvent : %s\n", strCaller.c_str());
    }

    auto spNetCloseMsg = xengine::CPeerNetCloseMessage::Create();
    spNetCloseMsg->nNonce = nNonce;
    spNetCloseMsg->closeReason = reason;
    PUBLISH_MESSAGE(spNetCloseMsg);
}

void CNetChannelController::SchedulePeerInv(uint64 nNonce, const uint256& hashFork, bool fActivedPeer, const uint256& lastBlockHash)
{
    std::vector<INetChannelModel::ScheduleResultPair> vecSchedResult;
    if (!fActivedPeer)
    {
        pNetChannelModel->ScheduleDeactivePeerInv(nNonce, vecSchedResult);
    }
    else
    {
        pNetChannelModel->ScheduleActivePeerInv(nNonce, hashFork, vecSchedResult);
    }

    for (const auto& sechedResult : vecSchedResult)
    {
        const uint64& nNonce = sechedResult.first;
        const INetChannelModel::ScheduleResultType& result = sechedResult.second;

        INetChannelModel::ScheduleBlockInvRetBool fBlock;
        INetChannelModel::PrevMissingBool fPrevMissing;
        INetChannelModel::ScheduleTxInvRetBool fTx;
        INetChannelModel::SyncBool fSync;
        INetChannelModel::HashForkType hashFork;
        std::vector<bigbang::network::CInv> vecInv;

        std::tie(
            fBlock,
            fPrevMissing,
            fTx,
            fSync,
            hashFork,
            vecInv) = result;

        if (!fBlock)
        {
            DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, "ScheduleBlockInv");
        }

        if (fPrevMissing)
        {
            DispatchGetBlocksFromHashEvent(nNonce, hashFork, lastBlockHash);
        }

        if (!fTx)
        {
            DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, "ScheduleTxInv");
        }

        SetPeerSyncStatus(nNonce, hashFork, fSync);

        if (!vecInv.empty())
        {
            auto spGetData = CPeerGetDataMessageOutBound::Create();
            spGetData->nNonce = nNonce;
            spGetData->hashFork = hashFork;
            spGetData->vecInv = vecInv;
            PUBLISH_MESSAGE(spGetData);
        }
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

void CNetChannelController::AddNewBlock(const uint256& hashFork, const uint256& hash, set<uint64>& setSchedPeer, set<uint64>& setMisbehavePeer)
{
    vector<uint256> vBlockHash;
    vBlockHash.push_back(hash);
    for (size_t i = 0; i < vBlockHash.size(); i++)
    {
        uint256 hashBlock = vBlockHash[i];
        uint64 nNonceSender = 0;
        CBlock* pBlock = pNetChannelModel->GetScheduleBlock(hashFork, hashBlock, nNonceSender);
        if (pBlock)
        {
            auto spAddNewBlockMsg = CAddBlockMessage::Create();
            spAddNewBlockMsg->block = *pBlock;
            spAddNewBlockMsg->hashFork = hashFork;
            spAddNewBlockMsg->spNonce = CNonce::Create(nNonceSender);
            PUBLISH_MESSAGE(spAddNewBlockMsg);
        }
    }
}

void CNetChannelController::AddNewTx(const uint256& hashFork, const uint256& txid, set<uint64>& setSchedPeer, set<uint64>& setMisbehavePeer)
{
    vector<uint256> vTxChain;

    vTxChain.push_back(txid);
    for (size_t i = 0; i < vTxChain.size(); i++)
    {
        const uint256& hashTx = vTxChain[i];
        uint64 nNonceSender = 0;
        CTransaction* pTx = pNetChannelModel->GetScheduleTransaction(hashFork, hashTx, nNonceSender);
        if (pTx)
        {
            if (pWorldLineCtrl->ExistsTx(hashTx))
            {
                pNetChannelModel->GetScheduleNextTx(hashFork, hashTx, vTxChain);
                pNetChannelModel->RemoveScheduleInv(hashFork, network::CInv(network::CInv::MSG_TX, hashTx), setSchedPeer);
            }
            else
            {
                auto spAddTxMsg = CAddTxMessage::Create();
                spAddTxMsg->spNonce = CNonce::Create(nNonceSender);
                spAddTxMsg->hashFork = hashFork;
                spAddTxMsg->tx = *pTx;
                PUBLISH_MESSAGE(spAddTxMsg);
            }
        }
    }
}

void CNetChannelController::PostAddNew(const uint256& hashFork, set<uint64>& setSchedPeer, set<uint64>& setMisbehavePeer)
{
    for (const uint64 nNonceSched : setSchedPeer)
    {
        if (!setMisbehavePeer.count(nNonceSched))
        {
            SchedulePeerInv(nNonceSched, hashFork, true, uint256());
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

    if (!pNetChannelModel->SetPeerSyncStatus(nNonce, hashFork, fSync, fInverted))
    {
        return;
    }

    if (fInverted)
    {
        if (fSync)
        {
            pNetChannelModel->RemoveUnSynchronizedForkPeerMT(nNonce, hashFork);
            CBroadcastTxInvMessage msg(hashFork);
            HandleBroadcastTxInv(msg);
        }
        else
        {
            pNetChannelModel->AddUnSynchronizedForkPeerMT(nNonce, hashFork);
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
    if (!vTxPool.empty() && !pNetChannelModel->IsPeerEmpty())
    {
        std::vector<INetChannelModel::MakeTxInvResultPair> vResult;
        pNetChannelModel->MakeTxInv(hashFork, vTxPool, vResult);
        for (const auto& result : vResult)
        {
            auto spInvMsg = CPeerInvMessageOutBound::Create();
            spInvMsg->nNonce = result.first;
            spInvMsg->hashFork = hashFork;
            spInvMsg->vecInv = result.second;
            PUBLISH_MESSAGE(spInvMsg);
            if (fCompleted && result.second.size() == network::CInv::MAX_INV_COUNT)
            {
                fCompleted = false;
            }
        }
    }
    return fCompleted;
}

} // namespace bigbang
