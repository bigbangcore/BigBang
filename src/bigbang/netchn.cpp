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

//////////////////////////////
// CNetChannel

CNetChannel::CNetChannel()
{
    pPeerNet = nullptr;
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pTxPool = nullptr;
    pService = nullptr;
    pDispatcher = nullptr;
}

CNetChannel::~CNetChannel()
{
}

bool CNetChannel::HandleInitialize()
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

    if (!GetObject("blockchain", pBlockChain))
    {
        Error("Failed to request blockchain\n");
        return false;
    }

    if (!GetObject("txpool", pTxPool))
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

    RegisterHandler<CPeerActiveMessage>(boost::bind(&CNetChannel::HandleActive, this, _1));
    RegisterHandler<CPeerDeactiveMessage>(boost::bind(&CNetChannel::HandleDeactive, this, _1));
    RegisterHandler<CPeerSubscribeMessageInBound>(boost::bind(&CNetChannel::HandleSubscribe, this, _1));
    RegisterHandler<CPeerUnsubscribeMessageInBound>(boost::bind(&CNetChannel::HandleUnsubscribe, this, _1));
    RegisterHandler<CPeerInvMessageInBound>(boost::bind(&CNetChannel::HandleInv, this, _1));
    RegisterHandler<CPeerGetDataMessageInBound>(boost::bind(&CNetChannel::HandleGetData, this, _1));
    RegisterHandler<CPeerGetBlocksMessageInBound>(boost::bind(&CNetChannel::HandleGetBlocks, this, _1));
    RegisterHandler<CPeerTxMessageInBound>(boost::bind(&CNetChannel::HandlePeerTx, this, _1));
    RegisterHandler<CPeerBlockMessageInBound>(boost::bind(&CNetChannel::HandlePeerBlock, this, _1));

    return true;
}

void CNetChannel::HandleDeinitialize()
{
    pPeerNet = nullptr;
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pTxPool = nullptr;
    pService = nullptr;
    pDispatcher = nullptr;
}

bool CNetChannel::HandleInvoke()
{
    {
        boost::unique_lock<boost::mutex> lock(mtxPushTx);
        nTimerPushTx = 0;
    }
    return network::INetChannelActor::HandleInvoke();
}

void CNetChannel::HandleHalt()
{
    {
        boost::unique_lock<boost::mutex> lock(mtxPushTx);
        if (nTimerPushTx != 0)
        {
            CancelTimer(nTimerPushTx);
            nTimerPushTx = 0;
        }
        setPushTxFork.clear();
    }

    network::INetChannelActor::HandleHalt();
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
        mapSched.clear();
    }
}

int CNetChannel::GetPrimaryChainHeight()
{
    uint256 hashBlock = uint64(0);
    int nHeight = 0;
    int64 nTime = 0;
    if (pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashBlock, nHeight, nTime))
    {
        return nHeight;
    }
    return 0;
}

bool CNetChannel::IsForkSynchronized(const uint256& hashFork) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
    map<uint256, set<uint64>>::const_iterator it = mapUnsync.find(hashFork);
    return (it == mapUnsync.end() || (*it).second.empty());
}

void CNetChannel::BroadcastBlockInv(const uint256& hashFork, const uint256& hashBlock)
{
    set<uint64> setKnownPeer;
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
        CSchedule& sched = GetSchedule(hashFork);
        sched.GetKnownPeer(network::CInv(network::CInv::MSG_BLOCK, hashBlock), setKnownPeer);
    }

    CPeerInvMessageOutBound* pInvMsg = new CPeerInvMessageOutBound();
    pInvMsg->hashFork = hashFork;
    pInvMsg->vecInv.push_back(network::CInv(network::CInv::MSG_BLOCK, hashBlock));

    {
        boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
        {
            uint64 nNonce = (*it).first;
            if (!setKnownPeer.count(nNonce) && (*it).second.IsSubscribed(hashFork))
            {
                pInvMsg->nNonce = nNonce;
                PUBLISH_MESSAGE(pInvMsg);
            }
        }
    }
}

void CNetChannel::BroadcastTxInv(const uint256& hashFork)
{
    boost::unique_lock<boost::mutex> lock(mtxPushTx);
    if (nTimerPushTx == 0)
    {
        if (!PushTxInv(hashFork))
        {
            setPushTxFork.insert(hashFork);
        }
        nTimerPushTx = SetTimer(PUSHTX_TIMEOUT, boost::bind(&CNetChannel::PushTxTimerFunc, this, _1));
    }
    else
    {
        setPushTxFork.insert(hashFork);
    }
}

void CNetChannel::SubscribeFork(const uint256& hashFork, const uint64& nNonce)
{
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
        if (!mapSched.insert(make_pair(hashFork, CSchedule())).second)
        {
            return;
        }
    }

    CPeerSubscribeMessageOutBound* pSubscribeMsg = new CPeerSubscribeMessageOutBound();
    pSubscribeMsg->nNonce = 0ULL;
    pSubscribeMsg->hashFork = pCoreProtocol->GetGenesisBlockHash();
    pSubscribeMsg->vecForks.push_back(hashFork);
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
        {
            pSubscribeMsg->nNonce = (*it).first;
            PUBLISH_MESSAGE(pSubscribeMsg);
            DispatchGetBlocksEvent(it->first, hashFork);
        }
    }
}

void CNetChannel::UnsubscribeFork(const uint256& hashFork)
{
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
        if (!mapSched.erase(hashFork))
        {
            return;
        }
    }

    CPeerUnsubscribeMessageOutBound* pUnsubscribeMsg = new CPeerUnsubscribeMessageOutBound();
    pUnsubscribeMsg->nNonce = 0ULL;
    pUnsubscribeMsg->hashFork = pCoreProtocol->GetGenesisBlockHash();
    pUnsubscribeMsg->vecForks.push_back(hashFork);

    {
        boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
        {
            pUnsubscribeMsg->nNonce = (*it).first;
            PUBLISH_MESSAGE(pUnsubscribeMsg);
        }
    }
}

void CNetChannel::HandleActive(const CPeerActiveMessage& activeMsg)
{
    uint64 nNonce = activeMsg.nNonce;
    if ((activeMsg.address.nService & network::NODE_NETWORK))
    {
        DispatchGetBlocksEvent(nNonce, pCoreProtocol->GetGenesisBlockHash());

        CPeerSubscribeMessageOutBound* pSubscribeMsg = new CPeerSubscribeMessageOutBound();
        pSubscribeMsg->nNonce = nNonce;
        pSubscribeMsg->hashFork = pCoreProtocol->GetGenesisBlockHash();
        {
            boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
            for (map<uint256, CSchedule>::iterator it = mapSched.begin(); it != mapSched.end(); ++it)
            {
                if ((*it).first != pCoreProtocol->GetGenesisBlockHash())
                {
                    pSubscribeMsg->vecForks.push_back(it->first);
                }
            }
        }
        if (!pSubscribeMsg->vecForks.empty())
        {
            PUBLISH_MESSAGE(pSubscribeMsg);
        }
    }
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
        mapPeer[nNonce] = CNetChannelPeer(activeMsg.address.nService, pCoreProtocol->GetGenesisBlockHash());
        mapUnsync[pCoreProtocol->GetGenesisBlockHash()].insert(nNonce);
    }
    NotifyPeerUpdate(nNonce, true, activeMsg.address);
}

void CNetChannel::HandleDeactive(const CPeerDeactiveMessage& deactiveMsg)
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

void CNetChannel::HandleSubscribe(const CPeerSubscribeMessageInBound& subscribeMsg)
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

void CNetChannel::HandleUnsubscribe(const CPeerUnsubscribeMessageInBound& unsubscribeMsg)
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

void CNetChannel::HandleInv(const CPeerInvMessageInBound& invMsg)
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
                if ((inv.nType == network::CInv::MSG_TX && !pTxPool->Exists(inv.nHash))
                    || (inv.nType == network::CInv::MSG_BLOCK && !pBlockChain->Exists(inv.nHash)))
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

void CNetChannel::HandleGetData(const CPeerGetDataMessageInBound& getDataMsg)
{
    uint64 nNonce = getDataMsg.nNonce;
    const uint256& hashFork = getDataMsg.hashFork;
    for (const network::CInv& inv : getDataMsg.vecInv)
    {
        if (inv.nType == network::CInv::MSG_TX)
        {
            CPeerTxMessageOutBound* pTxMsg = new CPeerTxMessageOutBound();
            pTxMsg->nNonce = nNonce;
            pTxMsg->hashFork = hashFork;
            if (pTxPool->Get(inv.nHash, pTxMsg->tx))
            {
                PUBLISH_MESSAGE(pTxMsg);
            }
            else if (pBlockChain->GetTransaction(inv.nHash, pTxMsg->tx))
            {
                PUBLISH_MESSAGE(pTxMsg);
            }
            else
            {
                // TODO: Penalize
            }
        }
        else if (inv.nType == network::CInv::MSG_BLOCK)
        {
            CPeerBlockMessageOutBound* pBlockMsg = new CPeerBlockMessageOutBound();
            pBlockMsg->nNonce = nNonce;
            pBlockMsg->hashFork = hashFork;
            if (pBlockChain->GetBlock(inv.nHash, pBlockMsg->block))
            {
                PUBLISH_MESSAGE(pBlockMsg);
            }
            else
            {
                // TODO: Penalize
            }
        }
    }
}

void CNetChannel::HandleGetBlocks(const CPeerGetBlocksMessageInBound& getBlocksMsg)
{
    uint64 nNonce = getBlocksMsg.nNonce;
    const uint256& hashFork = getBlocksMsg.hashFork;
    vector<uint256> vBlockHash;
    if (!pBlockChain->GetBlockInv(hashFork, getBlocksMsg.blockLocator, vBlockHash, MAX_GETBLOCKS_COUNT))
    {
        CBlock block;
        if (!pBlockChain->GetBlock(hashFork, block))
        {
            //DispatchMisbehaveEvent(nNonce,CEndpointManager::DDOS_ATTACK,"eventGetBlocks");
            //return true;
        }
    }

    CPeerInvMessageOutBound* pInvMsg = new CPeerInvMessageOutBound();
    pInvMsg->nNonce = nNonce;
    pInvMsg->hashFork = hashFork;
    for (const uint256& hash : vBlockHash)
    {
        pInvMsg->vecInv.push_back(network::CInv(network::CInv::MSG_BLOCK, hash));
    }
    PUBLISH_MESSAGE(pInvMsg);
}

void CNetChannel::HandlePeerTx(const CPeerTxMessageInBound& txMsg)
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
        if (pBlockChain->GetBlockLocation(tx.hashAnchor, hashForkAnchor, nHeightAnchor)
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

void CNetChannel::HandlePeerBlock(const CPeerBlockMessageInBound& blockMsg)
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
        if (pBlockChain->GetBlockLocation(block.hashPrev, hashForkPrev, nHeightPrev))
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

CSchedule& CNetChannel::GetSchedule(const uint256& hashFork)
{
    map<uint256, CSchedule>::iterator it = mapSched.find(hashFork);
    if (it == mapSched.end())
    {
        throw runtime_error("Unknown fork for scheduling.");
    }
    return ((*it).second);
}

void CNetChannel::NotifyPeerUpdate(uint64 nNonce, bool fActive, const network::CAddress& addrPeer)
{
    CNetworkPeerUpdate update;
    update.nPeerNonce = nNonce;
    update.fActive = fActive;
    update.addrPeer = addrPeer;
    pService->NotifyNetworkPeerUpdate(update);
}

void CNetChannel::DispatchGetBlocksEvent(uint64 nNonce, const uint256& hashFork)
{
    CPeerGetBlocksMessageOutBound* pGetBlocksMsg = new CPeerGetBlocksMessageOutBound();
    pGetBlocksMsg->nNonce = nNonce;
    pGetBlocksMsg->hashFork = hashFork;
    if (pBlockChain->GetBlockLocator(hashFork, pGetBlocksMsg->blockLocator))
    {
        PUBLISH_MESSAGE(pGetBlocksMsg);
    }
}

void CNetChannel::DispatchAwardEvent(uint64 nNonce, CEndpointManager::Bonus bonus)
{
    CEventPeerNetReward eventReward(nNonce);
    eventReward.data = bonus;
    pPeerNet->DispatchEvent(&eventReward);
}

void CNetChannel::DispatchMisbehaveEvent(uint64 nNonce, CEndpointManager::CloseReason reason, const std::string& strCaller)
{
    if (!strCaller.empty())
    {
        Log("DispatchMisbehaveEvent : %s\n", strCaller.c_str());
    }

    xengine::CPeerNetCloseMessage* pNetCloseMsg = new xengine::CPeerNetCloseMessage();
    pNetCloseMsg->nNonce = nNonce;
    pNetCloseMsg->closeReason = reason;
    PUBLISH_MESSAGE(pNetCloseMsg);
}

void CNetChannel::SchedulePeerInv(uint64 nNonce, const uint256& hashFork, CSchedule& sched)
{
    CPeerGetDataMessageOutBound* pGetData = new CPeerGetDataMessageOutBound();
    pGetData->nNonce = nNonce;
    pGetData->hashFork = hashFork;
    bool fMissingPrev = false;
    bool fEmpty = true;
    if (sched.ScheduleBlockInv(nNonce, pGetData->vecInv, MAX_PEER_SCHED_COUNT, fMissingPrev, fEmpty))
    {
        if (fMissingPrev)
        {
            DispatchGetBlocksEvent(nNonce, hashFork);
        }
        else if (pGetData->vecInv.empty())
        {
            if (!sched.ScheduleTxInv(nNonce, pGetData->vecInv, MAX_PEER_SCHED_COUNT))
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
    if (!pGetData->vecInv.empty())
    {
        PUBLISH_MESSAGE(pGetData);
    }
}

bool CNetChannel::GetMissingPrevTx(const CTransaction& tx, set<uint256>& setMissingPrevTx)
{
    setMissingPrevTx.clear();
    for (const CTxIn& txin : tx.vInput)
    {
        const uint256& prev = txin.prevout.hash;
        if (!setMissingPrevTx.count(prev))
        {
            if (!pTxPool->Exists(prev) && !pBlockChain->ExistsTx(prev))
            {
                setMissingPrevTx.insert(prev);
            }
        }
    }
    return (!setMissingPrevTx.empty());
}

void CNetChannel::AddNewBlock(const uint256& hashFork, const uint256& hash, CSchedule& sched,
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

void CNetChannel::AddNewTx(const uint256& hashFork, const uint256& txid, CSchedule& sched,
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
            if (pBlockChain->ExistsTx(txid))
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
        BroadcastTxInv(hashFork);
    }
}

void CNetChannel::PostAddNew(const uint256& hashFork, CSchedule& sched,
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

void CNetChannel::SetPeerSyncStatus(uint64 nNonce, const uint256& hashFork, bool fSync)
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
            BroadcastTxInv(hashFork);
        }
        else
        {
            mapUnsync[hashFork].insert(nNonce);
        }
    }
}

void CNetChannel::PushTxTimerFunc(uint32 nTimerId)
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
            nTimerPushTx = SetTimer(PUSHTX_TIMEOUT, boost::bind(&CNetChannel::PushTxTimerFunc, this, _1));
        }
        else
        {
            nTimerPushTx = 0;
        }
    }
}

bool CNetChannel::PushTxInv(const uint256& hashFork)
{
    // if (!IsForkSynchronized(hashFork))
    // {
    //     return false;
    // }

    bool fCompleted = true;
    vector<uint256> vTxPool;
    pTxPool->ListTx(hashFork, vTxPool);
    if (!vTxPool.empty() && !mapPeer.empty())
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
        {
            CNetChannelPeer& peer = it->second;
            if (peer.IsSubscribed(hashFork))
            {
                std::shared_ptr<CPeerInvMessageOutBound> spInvMsg(new CPeerInvMessageOutBound());
                spInvMsg->nNonce = it->first;
                spInvMsg->hashFork = hashFork;

                peer.MakeTxInv(hashFork, vTxPool, spInvMsg->vecInv, network::CInv::MAX_INV_COUNT);
                if (!spInvMsg->vecInv.empty())
                {
                    //PUBLISH_MESSAGE(spInvMsg);
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
