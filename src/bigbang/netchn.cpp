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
    return network::INetChannel::HandleInvoke();
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

    network::INetChannel::HandleHalt();
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

    network::CEventPeerInv eventInv(0, hashFork);
    eventInv.data.push_back(network::CInv(network::CInv::MSG_BLOCK, hashBlock));
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
        {
            uint64 nNonce = (*it).first;
            if (!setKnownPeer.count(nNonce) && (*it).second.IsSubscribed(hashFork))
            {
                eventInv.nNonce = nNonce;
                pPeerNet->DispatchEvent(&eventInv);
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
            StdLog("NetChannel", "SubscribeFork: mapSched insert fail, hashFork: %s", hashFork.GetHex().c_str());
            return;
        }
        StdLog("NetChannel", "SubscribeFork: mapSched insert success, hashFork: %s", hashFork.GetHex().c_str());
    }

    network::CEventPeerSubscribe eventSubscribe(0ULL, pCoreProtocol->GetGenesisBlockHash());
    eventSubscribe.data.push_back(hashFork);
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
        {
            eventSubscribe.nNonce = (*it).first;
            pPeerNet->DispatchEvent(&eventSubscribe);
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
            StdLog("NetChannel", "UnsubscribeFork: mapSched erase fail, hashFork: %s", hashFork.GetHex().c_str());
            return;
        }
        StdLog("NetChannel", "UnsubscribeFork: mapSched erase success, hashFork: %s", hashFork.GetHex().c_str());
    }

    network::CEventPeerUnsubscribe eventUnsubscribe(0ULL, pCoreProtocol->GetGenesisBlockHash());
    eventUnsubscribe.data.push_back(hashFork);

    {
        boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
        {
            eventUnsubscribe.nNonce = (*it).first;
            pPeerNet->DispatchEvent(&eventUnsubscribe);
        }
    }
}

bool CNetChannel::HandleEvent(network::CEventPeerActive& eventActive)
{
    uint64 nNonce = eventActive.nNonce;
    if ((eventActive.data.nService & network::NODE_NETWORK))
    {
        DispatchGetBlocksEvent(nNonce, pCoreProtocol->GetGenesisBlockHash());

        network::CEventPeerSubscribe eventSubscribe(nNonce, pCoreProtocol->GetGenesisBlockHash());
        {
            boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
            for (map<uint256, CSchedule>::iterator it = mapSched.begin(); it != mapSched.end(); ++it)
            {
                if ((*it).first != pCoreProtocol->GetGenesisBlockHash())
                {
                    eventSubscribe.data.push_back((*it).first);
                }
            }
        }
        if (!eventSubscribe.data.empty())
        {
            pPeerNet->DispatchEvent(&eventSubscribe);
        }
    }
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
        mapPeer[nNonce] = CNetChannelPeer(eventActive.data.nService, eventActive.data, pCoreProtocol->GetGenesisBlockHash());
        mapUnsync[pCoreProtocol->GetGenesisBlockHash()].insert(nNonce);
    }
    NotifyPeerUpdate(nNonce, true, eventActive.data);
    return true;
}

bool CNetChannel::HandleEvent(network::CEventPeerDeactive& eventDeactive)
{
    uint64 nNonce = eventDeactive.nNonce;
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
    NotifyPeerUpdate(nNonce, false, eventDeactive.data);

    return true;
}

bool CNetChannel::HandleEvent(network::CEventPeerSubscribe& eventSubscribe)
{
    uint64 nNonce = eventSubscribe.nNonce;
    uint256& hashFork = eventSubscribe.hashFork;
    if (hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
        map<uint64, CNetChannelPeer>::iterator it = mapPeer.find(nNonce);
        if (it != mapPeer.end())
        {
            for (const uint256& hash : eventSubscribe.data)
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
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, string("eventSubscribe: ") + hashFork.GetHex());
    }

    return true;
}

bool CNetChannel::HandleEvent(network::CEventPeerUnsubscribe& eventUnsubscribe)
{
    uint64 nNonce = eventUnsubscribe.nNonce;
    uint256& hashFork = eventUnsubscribe.hashFork;
    if (hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
        map<uint64, CNetChannelPeer>::iterator it = mapPeer.find(nNonce);
        if (it != mapPeer.end())
        {
            for (const uint256& hash : eventUnsubscribe.data)
            {
                (*it).second.Unsubscribe(hash);
                mapUnsync[hash].erase(nNonce);
            }
        }
    }
    else
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, string("eventUnsubscribe: ") + hashFork.GetHex());
    }

    return true;
}

bool CNetChannel::HandleEvent(network::CEventPeerInv& eventInv)
{
    uint64 nNonce = eventInv.nNonce;
    uint256& hashFork = eventInv.hashFork;
    try
    {
        if (eventInv.data.size() > network::CInv::MAX_INV_COUNT)
        {
            throw runtime_error(string("Inv count overflow, size: ") + to_string(eventInv.data.size()));
        }

        {
            boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
            CSchedule& sched = GetSchedule(hashFork);
            if (eventInv.data.size() == 1 && eventInv.data[0].nType == network::CInv::MSG_BLOCK && eventInv.data[0].nHash == uint256())
            {
                DispatchGetBlocksEvent(nNonce, hashFork);
                return true;
            }
            vector<uint256> vTxHash;
            for (const network::CInv& inv : eventInv.data)
            {
                if (inv.nType == network::CInv::MSG_TX)
                {
                    if (!pTxPool->Exists(inv.nHash) && !pBlockChain->ExistsTx(inv.nHash))
                    {
                        sched.AddNewInv(inv, nNonce);
                        vTxHash.push_back(inv.nHash);
                    }
                }
                else if (inv.nType == network::CInv::MSG_BLOCK)
                {
                    uint256 hashFork;
                    int nHeight;
                    if (!pBlockChain->GetBlockLocation(inv.nHash, hashFork, nHeight))
                    {
                        sched.AddNewInv(inv, nNonce);
                    }
                    else
                    {
                        sched.SetLocatorInvBlockHash(nNonce, nHeight, inv.nHash);
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
    catch (exception& e)
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, string("eventInv: ") + e.what());
    }
    return true;
}

bool CNetChannel::HandleEvent(network::CEventPeerGetData& eventGetData)
{
    uint64 nNonce = eventGetData.nNonce;
    uint256& hashFork = eventGetData.hashFork;
    network::CEventPeerGetFail eventGetFail(nNonce, hashFork);
    for (const network::CInv& inv : eventGetData.data)
    {
        if (inv.nType == network::CInv::MSG_TX)
        {
            network::CEventPeerTx eventTx(nNonce, hashFork);
            if (pTxPool->Get(inv.nHash, eventTx.data))
            {
                pPeerNet->DispatchEvent(&eventTx);
            }
            else if (pBlockChain->GetTransaction(inv.nHash, eventTx.data))
            {
                pPeerNet->DispatchEvent(&eventTx);
            }
            else
            {
                StdError("NetChannel", "CEventPeerGetData: Get transaction fail, txid: %s", inv.nHash.GetHex().c_str());
                eventGetFail.data.push_back(inv);
            }
        }
        else if (inv.nType == network::CInv::MSG_BLOCK)
        {
            network::CEventPeerBlock eventBlock(nNonce, hashFork);
            if (pBlockChain->GetBlock(inv.nHash, eventBlock.data))
            {
                pPeerNet->DispatchEvent(&eventBlock);
            }
            else
            {
                StdError("NetChannel", "CEventPeerGetData: Get block fail, block hash: %s", inv.nHash.GetHex().c_str());
                eventGetFail.data.push_back(inv);
            }
        }
        else
        {
            StdError("NetChannel", "CEventPeerGetData: inv.nType error, nType: %s, nHash: %s", inv.nType, inv.nHash.GetHex().c_str());
            eventGetFail.data.push_back(inv);
        }
    }
    if (!eventGetFail.data.empty())
    {
        pPeerNet->DispatchEvent(&eventGetFail);
    }
    return true;
}

bool CNetChannel::HandleEvent(network::CEventPeerGetBlocks& eventGetBlocks)
{
    uint64 nNonce = eventGetBlocks.nNonce;
    uint256& hashFork = eventGetBlocks.hashFork;
    vector<uint256> vBlockHash;
    if (eventGetBlocks.data.vBlockHash.empty())
    {
        StdError("NetChannel", "CEventPeerGetBlocks: vBlockHash is empty");
        return false;
    }
    if (pBlockChain->GetBlockInv(hashFork, eventGetBlocks.data, vBlockHash, MAX_GETBLOCKS_COUNT))
    {
        if (vBlockHash.empty())
        {
            uint256 hashLastBlock;
            int nLastHeight = 0;
            int64 nLastTime = 0;
            if (pBlockChain->GetLastBlock(hashFork, hashLastBlock, nLastHeight, nLastTime))
            {
                for (const uint256& hash : eventGetBlocks.data.vBlockHash)
                {
                    if (hash == hashLastBlock)
                    {
                        return true;
                    }
                }
            }

            network::CEventPeerInv eventInv(nNonce, hashFork);
            eventInv.data.push_back(network::CInv(network::CInv::MSG_BLOCK, uint256()));
            pPeerNet->DispatchEvent(&eventInv);
        }
        else
        {
            network::CEventPeerInv eventInv(nNonce, hashFork);
            for (const uint256& hash : vBlockHash)
            {
                eventInv.data.push_back(network::CInv(network::CInv::MSG_BLOCK, hash));
            }
            pPeerNet->DispatchEvent(&eventInv);
        }
    }
    return true;
}

bool CNetChannel::HandleEvent(network::CEventPeerTx& eventTx)
{
    uint64 nNonce = eventTx.nNonce;
    uint256& hashFork = eventTx.hashFork;
    CTransaction& tx = eventTx.data;
    uint256 txid = tx.GetHash();

    try
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);

        set<uint64> setSchedPeer, setMisbehavePeer;
        CSchedule& sched = GetSchedule(hashFork);

        if (!sched.ReceiveTx(nNonce, txid, tx, setSchedPeer))
        {
            StdLog("NetChannel", "CEventPeerTx: ReceiveTx fail, txid: %s", txid.GetHex().c_str());
            return true;
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
            StdLog("NetChannel", "CEventPeerTx: GetBlockLocation fail, txid: %s, hashAnchor: %s",
                   txid.GetHex().c_str(), tx.hashAnchor.GetHex().c_str());
            sched.InvalidateTx(txid, setMisbehavePeer);
        }
        PostAddNew(hashFork, sched, setSchedPeer, setMisbehavePeer);
    }
    catch (exception& e)
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, string("eventTx: ") + e.what());
    }
    return true;
}

bool CNetChannel::HandleEvent(network::CEventPeerBlock& eventBlock)
{
    uint64 nNonce = eventBlock.nNonce;
    uint256& hashFork = eventBlock.hashFork;
    CBlock& block = eventBlock.data;
    uint256 hash = block.GetHash();

    try
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);

        set<uint64> setSchedPeer, setMisbehavePeer;
        CSchedule& sched = GetSchedule(hashFork);

        if (!sched.ReceiveBlock(nNonce, hash, block, setSchedPeer))
        {
            StdLog("NetChannel", "CEventPeerBlock: ReceiveBlock fail, block: %s", hash.GetHex().c_str());
            return true;
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
                StdLog("NetChannel", "CEventPeerBlock: hashForkPrev != hashFork, hashForkPrev: %s, hashFork: %s, hashBlockPrev: %s",
                       hashForkPrev.GetHex().c_str(), hashFork.GetHex().c_str(), block.hashPrev.GetHex().c_str());
                sched.InvalidateBlock(hash, setMisbehavePeer);
            }
        }
        else
        {
            sched.AddOrphanBlockPrev(hash, block.hashPrev);
        }

        PostAddNew(hashFork, sched, setSchedPeer, setMisbehavePeer);
    }
    catch (exception& e)
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, string("eventBlock: ") + e.what());
    }
    return true;
}

bool CNetChannel::HandleEvent(network::CEventPeerGetFail& eventGetFail)
{
    uint64 nNonce = eventGetFail.nNonce;
    uint256& hashFork = eventGetFail.hashFork;

    try
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
        CSchedule& sched = GetSchedule(hashFork);

        for (const network::CInv& inv : eventGetFail.data)
        {
            sched.CancelAssignedInv(nNonce, inv);
        }
    }
    catch (exception& e)
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, string("eventGetFail: ") + e.what());
    }
    return true;
}

CSchedule& CNetChannel::GetSchedule(const uint256& hashFork)
{
    map<uint256, CSchedule>::iterator it = mapSched.find(hashFork);
    if (it == mapSched.end())
    {
        throw runtime_error(string("Unknown fork for scheduling, hashFork: ") + hashFork.GetHex());
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
    try
    {
        CSchedule& sched = GetSchedule(hashFork);
        int nDepth = sched.GetLocatorDepth(nNonce);

        uint256 hashInvBlock;
        int nLocatorInvHeight;
        nLocatorInvHeight = sched.GetLocatorInvBlockHash(nNonce, hashInvBlock);

        network::CEventPeerGetBlocks eventGetBlocks(nNonce, hashFork);
        if (nLocatorInvHeight > 0)
        {
            eventGetBlocks.data.vBlockHash.push_back(hashInvBlock);
        }
        if (pBlockChain->GetBlockLocator(hashFork, eventGetBlocks.data, nDepth, MAX_GETBLOCKS_COUNT - 1) && !eventGetBlocks.data.vBlockHash.empty())
        {
            StdLog("NetChannel", "DispatchGetBlocksEvent: nLocatorInvHeight: %d, hashInvBlock: %s, hashFork: %s.",
                   nLocatorInvHeight, hashInvBlock.GetHex().c_str(), hashFork.GetHex().c_str());
            pPeerNet->DispatchEvent(&eventGetBlocks);
        }
        sched.SetLocatorDepth(nNonce, nDepth);
    }
    catch (exception& e)
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, string("DispatchGetBlocksEvent: ") + e.what());
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
        StdLog("NetChannel", "DispatchMisbehaveEvent : %s", strCaller.c_str());
    }

    CEventPeerNetClose eventClose(nNonce);
    eventClose.data = reason;
    pPeerNet->DispatchEvent(&eventClose);
}

void CNetChannel::SchedulePeerInv(uint64 nNonce, const uint256& hashFork, CSchedule& sched)
{
    network::CEventPeerGetData eventGetData(nNonce, hashFork);
    bool fMissingPrev = false;
    bool fEmpty = true;
    if (sched.ScheduleBlockInv(nNonce, eventGetData.data, MAX_PEER_SCHED_COUNT, fMissingPrev, fEmpty))
    {
        if (fMissingPrev)
        {
            DispatchGetBlocksEvent(nNonce, hashFork);
        }
        else if (eventGetData.data.empty())
        {
            if (!sched.ScheduleTxInv(nNonce, eventGetData.data, MAX_PEER_SCHED_COUNT))
            {
                DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, "SchedulePeerInv1: ScheduleTxInv fail");
            }
        }
        SetPeerSyncStatus(nNonce, hashFork, fEmpty);
    }
    else
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, "SchedulePeerInv2: ScheduleBlockInv fail");
    }
    if (!eventGetData.data.empty())
    {
        pPeerNet->DispatchEvent(&eventGetData);
    }
}

bool CNetChannel::GetMissingPrevTx(CTransaction& tx, set<uint256>& setMissingPrevTx)
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
            if (!pBlock->IsPrimary() && !pBlock->IsVacant())
            {
                CProofOfPiggyback proof;
                proof.Load(pBlock->vchProof);

                uint256 hashForkRef;
                int nHeightRef;
                if (!pBlockChain->GetBlockLocation(proof.hashRefBlock, hashForkRef, nHeightRef))
                {
                    StdLog("NetChannel", "NetChannel get ref block fail, peer: %s, block: %s, fork: %s, refblock: %s",
                           GetPeerAddressInfo(nNonceSender).c_str(), hashBlock.GetHex().c_str(),
                           hashFork.GetHex().c_str(), proof.hashRefBlock.GetHex().c_str());

                    set<uint64> setKnownPeer;
                    sched.RemoveInv(network::CInv(network::CInv::MSG_BLOCK, hashBlock), setKnownPeer);
                    setSchedPeer.insert(setKnownPeer.begin(), setKnownPeer.end());
                    return;
                }
            }

            Errno err = pDispatcher->AddNewBlock(*pBlock, nNonceSender);
            if (err == OK)
            {
                StdDebug("NetChannel", "NetChannel AddNewBlock success, peer: %s, block: %s",
                         GetPeerAddressInfo(nNonceSender).c_str(), hashBlock.GetHex().c_str());

                for (const CTransaction& tx : pBlock->vtx)
                {
                    uint256 txid = tx.GetHash();
                    if (sched.RemoveInv(network::CInv(network::CInv::MSG_TX, txid), setSchedPeer))
                    {
                        StdDebug("NetChannel", "NetChannel AddNewBlock: remove tx inv success, peer: %s, txid: %s",
                                 GetPeerAddressInfo(nNonceSender).c_str(), txid.GetHex().c_str());
                    }
                }

                set<uint64> setKnownPeer;
                sched.GetNextBlock(hashBlock, vBlockHash);
                sched.RemoveInv(network::CInv(network::CInv::MSG_BLOCK, hashBlock), setKnownPeer);
                DispatchAwardEvent(nNonceSender, CEndpointManager::VITAL_DATA);
                setSchedPeer.insert(setKnownPeer.begin(), setKnownPeer.end());
            }
            else if (err == ERR_ALREADY_HAVE && pBlock->IsVacant())
            {
                StdLog("NetChannel", "NetChannel AddNewBlock: block already have, peer: %s, block: %s",
                       GetPeerAddressInfo(nNonceSender).c_str(), hashBlock.GetHex().c_str());
                set<uint64> setKnownPeer;
                sched.GetNextBlock(hashBlock, vBlockHash);
                sched.RemoveInv(network::CInv(network::CInv::MSG_BLOCK, hashBlock), setKnownPeer);
                setSchedPeer.insert(setKnownPeer.begin(), setKnownPeer.end());
            }
            else
            {
                StdLog("NetChannel", "NetChannel AddNewBlock fail, peer: %s, block: %s, err: [%d] %s",
                       GetPeerAddressInfo(nNonceSender).c_str(), hashBlock.GetHex().c_str(), err, ErrorString(err));
                sched.InvalidateBlock(hashBlock, setMisbehavePeer);
            }
        }
        else
        {
            StdLog("NetChannel", "NetChannel sched GetBlock fail, peer: %s, block: %s",
                   GetPeerAddressInfo(nNonceSender).c_str(), hashBlock.GetHex().c_str());
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
            if (pBlockChain->ExistsTx(hashTx))
            {
                sched.RemoveInv(network::CInv(network::CInv::MSG_TX, hashTx), setSchedPeer);
                StdDebug("NetChannel", "NetChannel AddNewTx: tx at blockchain exists, peer: %s, txid: %s",
                         GetPeerAddressInfo(nNonceSender).c_str(), hashTx.GetHex().c_str());
                continue;
            }

            Errno err = pDispatcher->AddNewTx(*pTx, nNonceSender);
            if (err == OK)
            {
                StdDebug("NetChannel", "NetChannel AddNewTx success, peer: %s, txid: %s",
                         GetPeerAddressInfo(nNonceSender).c_str(), hashTx.GetHex().c_str());
                sched.GetNextTx(hashTx, vtx, setTx);
                sched.RemoveInv(network::CInv(network::CInv::MSG_TX, hashTx), setSchedPeer);
                DispatchAwardEvent(nNonceSender, CEndpointManager::MAJOR_DATA);
                nAddNewTx++;
            }
            else if (err == ERR_MISSING_PREV
                     || err == ERR_TRANSACTION_CONFLICTING_INPUT
                     || err == ERR_ALREADY_HAVE)
            {
                if (err == ERR_TRANSACTION_CONFLICTING_INPUT || err == ERR_ALREADY_HAVE)
                {
                    sched.RemoveInv(network::CInv(network::CInv::MSG_TX, hashTx), setSchedPeer);
                    StdDebug("NetChannel", "NetChannel AddNewTx fail, remove inv, peer: %s, txid: %s, err: [%d] %s",
                             GetPeerAddressInfo(nNonceSender).c_str(), hashTx.GetHex().c_str(), err, ErrorString(err));
                }
                else
                {
                    StdLog("NetChannel", "NetChannel AddNewTx fail, peer: %s, txid: %s, err: [%d] %s",
                           GetPeerAddressInfo(nNonceSender).c_str(), hashTx.GetHex().c_str(), err, ErrorString(err));
                }
            }
            else
            {
                StdLog("NetChannel", "NetChannel AddNewTx fail, invalidate tx, peer: %s, txid: %s, err: [%d] %s",
                       GetPeerAddressInfo(nNonceSender).c_str(), hashTx.GetHex().c_str(), err, ErrorString(err));
                sched.InvalidateTx(hashTx, setMisbehavePeer);
            }
        }
        else
        {
            StdLog("NetChannel", "NetChannel sched GetTransaction fail, peer: %s, txid: %s",
                   GetPeerAddressInfo(nNonceSender).c_str(), hashTx.GetHex().c_str());
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
        boost::unique_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
        {
            CNetChannelPeer& peer = it->second;
            if (peer.IsSubscribed(hashFork))
            {
                network::CEventPeerInv eventInv(it->first, hashFork);
                peer.MakeTxInv(hashFork, vTxPool, eventInv.data, network::CInv::MAX_INV_COUNT);
                if (!eventInv.data.empty())
                {
                    pPeerNet->DispatchEvent(&eventInv);
                    if (fCompleted && eventInv.data.size() == network::CInv::MAX_INV_COUNT)
                    {
                        fCompleted = false;
                    }
                }
            }
        }
    }
    return fCompleted;
}

const string CNetChannel::GetPeerAddressInfo(uint64 nNonce)
{
    boost::shared_lock<boost::shared_mutex> wlock(rwNetPeer);
    CNetChannelPeer& peer = mapPeer[nNonce];
    boost::asio::ip::tcp::endpoint ep;
    peer.addressRemote.ssEndpoint.GetEndpoint(ep);
    if (ep.address().is_v6())
    {
        return string("[") + ep.address().to_string() + "]:" + to_string(ep.port());
    }
    return ep.address().to_string() + ":" + to_string(ep.port());
}

} // namespace bigbang
