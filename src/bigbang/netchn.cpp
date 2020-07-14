// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "netchn.h"

#include <boost/bind.hpp>

#include "schedule.h"

using namespace std;
using namespace xengine;
using boost::asio::ip::tcp;

#define PUSHTX_TIMEOUT (1000)
#define SYNTXINV_TIMEOUT (1000 * 60)

namespace bigbang
{

//////////////////////////////
// CNetChannelPeer

void CNetChannelPeer::CNetChannelPeerFork::AddKnownTx(const vector<uint256>& vTxHash, size_t nTotalSynTxCount)
{
    if (nTotalSynTxCount > NETCHANNEL_KNOWNINV_MAXCOUNT)
    {
        nCacheSynTxCount = nTotalSynTxCount;
    }
    else
    {
        nCacheSynTxCount = NETCHANNEL_KNOWNINV_MAXCOUNT;
    }
    for (const uint256& txid : vTxHash)
    {
        setKnownTx.insert(CPeerKnownTx(txid));
    }
    ClearExpiredTx();
}

void CNetChannelPeer::CNetChannelPeerFork::ClearExpiredTx()
{
    int64 nExpiredAt = GetTime() - NETCHANNEL_KNOWNINV_EXPIREDTIME;
    size_t nCtrlCount = nCacheSynTxCount + network::CInv::MAX_INV_COUNT * 2;
    size_t nMaxCount = nCacheSynTxCount * 2;
    if (nMaxCount < nCtrlCount + network::CInv::MAX_INV_COUNT)
    {
        nMaxCount = nCtrlCount + network::CInv::MAX_INV_COUNT;
    }
    CPeerKnownTxSetByTime& index = setKnownTx.get<1>();
    CPeerKnownTxSetByTime::iterator it = index.begin();
    while (it != index.end() && (((*it).nTime <= nExpiredAt && index.size() > nCtrlCount) || (index.size() > nMaxCount)))
    {
        index.erase(it++);
    }
}

bool CNetChannelPeer::IsSynchronized(const uint256& hashFork) const
{
    map<uint256, CNetChannelPeerFork>::const_iterator it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        return it->second.fSynchronized;
    }
    return false;
}

bool CNetChannelPeer::SetSyncStatus(const uint256& hashFork, bool fSync, bool& fInverted)
{
    map<uint256, CNetChannelPeerFork>::iterator it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        fInverted = (it->second.fSynchronized != fSync);
        it->second.fSynchronized = fSync;
        return true;
    }
    return false;
}

void CNetChannelPeer::AddKnownTx(const uint256& hashFork, const vector<uint256>& vTxHash, size_t nTotalSynTxCount)
{
    map<uint256, CNetChannelPeerFork>::iterator it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        it->second.AddKnownTx(vTxHash, nTotalSynTxCount);
    }
}

bool CNetChannelPeer::MakeTxInv(const uint256& hashFork, const vector<uint256>& vTxPool, vector<network::CInv>& vInv)
{
    map<uint256, CNetChannelPeerFork>::iterator it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        CNetChannelPeerFork& peerFork = it->second;
        switch (peerFork.CheckTxInvSynStatus())
        {
        case CHECK_SYNTXINV_STATUS_RESULT_WAIT_SYN:
            break;
        case CHECK_SYNTXINV_STATUS_RESULT_WAIT_TIMEOUT:
            return false;
        case CHECK_SYNTXINV_STATUS_RESULT_ALLOW_SYN:
        {
            vector<uint256> vTxHash;
            for (const uint256& txid : vTxPool)
            {
                if (vInv.size() >= peerFork.nSingleSynTxInvCount)
                {
                    break;
                }
                else if (!peerFork.IsKnownTx(txid))
                {
                    vInv.push_back(network::CInv(network::CInv::MSG_TX, txid));
                    vTxHash.push_back(txid);
                }
            }
            peerFork.AddKnownTx(vTxHash, vTxPool.size());
            if (!vInv.empty())
            {
                peerFork.nSynTxInvStatus = CNetChannelPeerFork::SYNTXINV_STATUS_WAIT_PEER_RECEIVED;
                peerFork.nSynTxInvSendTime = GetTime();
            }
            break;
        }
        default:
            break;
        }
    }
    return true;
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
    pConsensus = nullptr;
    fStartIdlePushTxTimer = false;
}

CNetChannel::~CNetChannel()
{
}

bool CNetChannel::HandleInitialize()
{
    if (!GetObject("peernet", pPeerNet))
    {
        Error("Failed to request peer net");
        return false;
    }

    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("blockchain", pBlockChain))
    {
        Error("Failed to request blockchain");
        return false;
    }

    if (!GetObject("txpool", pTxPool))
    {
        Error("Failed to request txpool");
        return false;
    }

    if (!GetObject("service", pService))
    {
        Error("Failed to request service");
        return false;
    }

    if (!GetObject("dispatcher", pDispatcher))
    {
        Error("Failed to request dispatcher");
        return false;
    }

    if (!GetObject("consensus", pConsensus))
    {
        Error("Failed to request consensus\n");
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
    pConsensus = nullptr;
}

bool CNetChannel::HandleInvoke()
{
    {
        boost::unique_lock<boost::mutex> lock(mtxPushTx);
        nTimerPushTx = 0;
        fStartIdlePushTxTimer = false;
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
            fStartIdlePushTxTimer = false;
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
    uint16 nMintType = 0;
    if (pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashBlock, nHeight, nTime, nMintType))
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
    if (fStartIdlePushTxTimer && nTimerPushTx != 0)
    {
        CancelTimer(nTimerPushTx);
        nTimerPushTx = 0;
        fStartIdlePushTxTimer = false;
    }
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

    vector<uint64> vPeerNonce;
    network::CEventPeerSubscribe eventSubscribe(0ULL, pCoreProtocol->GetGenesisBlockHash());
    eventSubscribe.data.push_back(hashFork);
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
        {
            vPeerNonce.push_back((*it).first);
            eventSubscribe.nNonce = (*it).first;
            pPeerNet->DispatchEvent(&eventSubscribe);
        }
    }
    if (!vPeerNonce.empty())
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
        for (const uint64& nPeer : vPeerNonce)
        {
            DispatchGetBlocksEvent(nPeer, hashFork);
        }
    }
    BroadcastTxInv(hashFork);
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

bool CNetChannel::SubmitCachePowBlock(const CConsensusParam& consParam)
{
    try
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);

        uint256 hashFork = pCoreProtocol->GetGenesisBlockHash();

        vector<std::pair<uint256, int>> vPowBlockHash;
        GetSchedule(hashFork).GetSubmitCachePowBlock(consParam, vPowBlockHash);

        set<uint64> setSchedPeer;
        set<uint64> setMisbehavePeer;
        for (auto& chash : vPowBlockHash)
        {
            CSchedule& sched = GetSchedule(hashFork); // Resolve unsubscribefork errors
            const uint256& hashBlock = chash.first;
            if (chash.second == 1)
            {
                uint64 nNonceSender = 0;
                CBlock* pBlock = sched.GetBlock(hashBlock, nNonceSender);
                if (pBlock)
                {
                    uint256 hashForkPrev;
                    int nHeightPrev;
                    if (pBlockChain->GetBlockLocation(pBlock->hashPrev, hashForkPrev, nHeightPrev)
                        && hashForkPrev == hashFork)
                    {
                        vector<pair<uint256, uint256>> vRefNextBlock;
                        AddNewBlock(hashFork, hashBlock, sched, setSchedPeer, setMisbehavePeer, vRefNextBlock, false);
                        StdTrace("NetChannel", "SubmitCachePowBlock: add p2p pow block over, height: %d, block: %s",
                                 CBlock::GetBlockHeightByHash(hashBlock), hashBlock.GetHex().c_str());

                        if (!vRefNextBlock.empty())
                        {
                            AddRefNextBlock(vRefNextBlock);
                        }
                    }
                }
            }
            else
            {
                CBlock block;
                if (sched.GetCacheLocalPowBlock(hashBlock, block))
                {
                    Errno err = pDispatcher->AddNewBlock(block, 0);
                    if (err != OK)
                    {
                        StdLog("NetChannel", "SubmitCachePowBlock AddNewBlock fail, block: %s, err: [%d] %s",
                               hashBlock.GetHex().c_str(), err, ErrorString(err));
                    }
                    else
                    {
                        StdTrace("NetChannel", "SubmitCachePowBlock: add local pow block success, block: %s", hashBlock.GetHex().c_str());
                    }
                    GetSchedule(hashFork).RemoveCacheLocalPowBlock(hashBlock); // Resolve unsubscribefork errors
                }
            }
        }

        PostAddNew(hashFork, setSchedPeer, setMisbehavePeer);

        if (setMisbehavePeer.empty() && !vPowBlockHash.empty())
        {
            return true;
        }
    }
    catch (exception& e)
    {
        StdError("NetChannel", "SubmitCachePowBlock error: %s", e.what());
    }
    return false;
}

bool CNetChannel::IsLocalCachePowBlock(int nHeight)
{
    bool ret = false;
    try
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
        CSchedule& sched = GetSchedule(pCoreProtocol->GetGenesisBlockHash());
        ret = sched.CheckCacheLocalPowBlock(nHeight);
    }
    catch (exception& e)
    {
        StdError("NetChannel", "IsLocalCachePowBlock: GetSchedule fail, height: %d, error: %s", nHeight, e.what());
        return false;
    }
    InnerSubmitCachePowBlock();
    return ret;
}

bool CNetChannel::AddCacheLocalPowBlock(const CBlock& block)
{
    bool ret = false;
    try
    {
        boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
        CSchedule& sched = GetSchedule(pCoreProtocol->GetGenesisBlockHash());

        bool fLongChain = false;
        if (pBlockChain->VerifyPowBlock(block, fLongChain) != OK)
        {
            StdError("NetChannel", "AddCacheLocalPowBlock VerifyPowBlock fail: height: %d, block: %s",
                     block.GetBlockHeight(), block.GetHash().GetHex().c_str());
            return false;
        }

        bool fFirst = false;
        if (sched.AddCacheLocalPowBlock(block, fFirst))
        {
            if (fFirst && fLongChain)
            {
                InnerBroadcastBlockInv(pCoreProtocol->GetGenesisBlockHash(), block.GetHash());
                StdDebug("NetChannel", "AddCacheLocalPowBlock InnerBroadcastBlockInv: height: %d, block: %s",
                         block.GetBlockHeight(), block.GetHash().GetHex().c_str());
            }
            ret = true;
        }
    }
    catch (exception& e)
    {
        StdError("NetChannel", "AddCacheLocalPowBlock: GetSchedule fail, block: %s, error: %s", block.GetHash().GetHex().c_str(), e.what());
    }
    if (ret)
    {
        InnerSubmitCachePowBlock();
    }
    return ret;
}

bool CNetChannel::HandleEvent(network::CEventPeerActive& eventActive)
{
    uint64 nNonce = eventActive.nNonce;
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
        mapPeer[nNonce] = CNetChannelPeer(eventActive.data.nService, eventActive.data, pCoreProtocol->GetGenesisBlockHash());
        mapUnsync[pCoreProtocol->GetGenesisBlockHash()].insert(nNonce);
    }
    StdLog("NetChannel", "CEventPeerActive: peer: %s", GetPeerAddressInfo(nNonce).c_str());
    if ((eventActive.data.nService & network::NODE_NETWORK))
    {
        {
            boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
            DispatchGetBlocksEvent(nNonce, pCoreProtocol->GetGenesisBlockHash());
        }
        BroadcastTxInv(pCoreProtocol->GetGenesisBlockHash());

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
    NotifyPeerUpdate(nNonce, true, eventActive.data);
    return true;
}

bool CNetChannel::HandleEvent(network::CEventPeerDeactive& eventDeactive)
{
    uint64 nNonce = eventDeactive.nNonce;
    StdLog("NetChannel", "CEventPeerDeactive: peer: %s", GetPeerAddressInfo(nNonce).c_str());
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
    StdLog("NetChannel", "CEventPeerSubscribe: peer: %s, fork: %s", GetPeerAddressInfo(nNonce).c_str(), hashFork.GetHex().c_str());
    if (hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        vector<uint256> vDispatchHash;
        {
            boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
            map<uint64, CNetChannelPeer>::iterator it = mapPeer.find(nNonce);
            if (it != mapPeer.end())
            {
                for (const uint256& hash : eventSubscribe.data)
                {
                    (*it).second.Subscribe(hash);
                    mapUnsync[hash].insert(nNonce);
                    vDispatchHash.push_back(hash);
                }
            }
        }
        if (!vDispatchHash.empty())
        {
            boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
            for (const uint256& hash : vDispatchHash)
            {
                if (mapSched.count(hash))
                {
                    DispatchGetBlocksEvent(nNonce, hash);
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
    StdLog("NetChannel", "CEventPeerUnsubscribe: peer: %s, fork: %s", GetPeerAddressInfo(nNonce).c_str(), hashFork.GetHex().c_str());
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

            vector<uint256> vTxHash;
            int64 nBlockInvAddCount = 0;
            int64 nBlockInvExistCount = 0;
            uint256 hashLastBlock;
            int nLastBlockHeight = -1;
            int64 nLastTime = 0;
            uint16 nLastMintType = 0;

            if (!pBlockChain->GetLastBlock(hashFork, hashLastBlock, nLastBlockHeight, nLastTime, nLastMintType))
            {
                StdError("NetChannel", "CEventPeerInv: peer: %s, GetLastBlock fail, fork: %s",
                         GetPeerAddressInfo(nNonce).c_str(), hashFork.GetHex().c_str());
                throw runtime_error(string("GetLastBlock fail"));
            }

            for (const network::CInv& inv : eventInv.data)
            {
                if (inv.nType == network::CInv::MSG_TX)
                {
                    vTxHash.push_back(inv.nHash);
                    if ((nLastBlockHeight == 0 || CTxId(inv.nHash).GetTxTime() < nLastTime + MAX_TXINV_INTERVAL_TIME)
                        && !pTxPool->Exists(inv.nHash) && !pBlockChain->ExistsTx(inv.nHash))
                    {
                        if (sched.AddNewInv(inv, nNonce))
                        {
                            StdTrace("NetChannel", "CEventPeerInv: peer: %s, add tx inv success, txid: %s ",
                                     GetPeerAddressInfo(nNonce).c_str(), inv.nHash.GetHex().c_str());
                        }
                        else
                        {
                            StdTrace("NetChannel", "CEventPeerInv: peer: %s, add tx inv fail, txid: %s ",
                                     GetPeerAddressInfo(nNonce).c_str(), inv.nHash.GetHex().c_str());
                        }
                    }
                }
                else if (inv.nType == network::CInv::MSG_BLOCK)
                {
                    uint256 hashLocationFork;
                    int nLocationHeight;
                    uint256 hashLocationNext;
                    if (!pBlockChain->GetBlockLocation(inv.nHash, hashLocationFork, nLocationHeight, hashLocationNext))
                    {
                        uint32 nBlockHeight = CBlock::GetBlockHeightByHash(inv.nHash);
                        if (nBlockHeight > (nLastBlockHeight + CSchedule::MAX_PEER_BLOCK_INV_COUNT / 2))
                        {
                            StdTrace("NetChannel", "CEventPeerInv: peer: %s, block height too high, last height: %d, block height: %d, block hash: %s ",
                                     GetPeerAddressInfo(nNonce).c_str(), nLastBlockHeight, nBlockHeight, inv.nHash.GetHex().c_str());
                        }
                        else
                        {
                            if (sched.AddNewInv(inv, nNonce))
                            {
                                StdTrace("NetChannel", "CEventPeerInv: peer: %s, add block inv success, height: %d, block hash: %s ",
                                         GetPeerAddressInfo(nNonce).c_str(), nBlockHeight, inv.nHash.GetHex().c_str());
                                nBlockInvAddCount++;
                            }
                            else
                            {
                                StdTrace("NetChannel", "CEventPeerInv: peer: %s, add block inv fail, block hash: %s ",
                                         GetPeerAddressInfo(nNonce).c_str(), inv.nHash.GetHex().c_str());
                            }
                        }
                    }
                    else
                    {
                        StdTrace("NetChannel", "CEventPeerInv: peer: %s, block existed, height: %d, block hash: %s ",
                                 GetPeerAddressInfo(nNonce).c_str(), nLocationHeight, inv.nHash.GetHex().c_str());
                        sched.SetLocatorInvBlockHash(nNonce, nLocationHeight, inv.nHash, hashLocationNext);
                        nBlockInvExistCount++;
                    }
                }
            }
            if (!vTxHash.empty())
            {
                StdTrace("NetChannel", "CEventPeerInv: recv tx inv request and send response, count: %ld, peer: %s, fork: %s",
                         vTxHash.size(), GetPeerAddressInfo(nNonce).c_str(), hashFork.GetHex().c_str());

                {
                    boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
                    CNetChannelPeer& peer = mapPeer[nNonce];
                    peer.AddKnownTx(hashFork, vTxHash, 0);
                    peer.SetWaitGetTxComplete(hashFork);
                }

                network::CEventPeerMsgRsp eventMsgRsp(nNonce, hashFork);
                eventMsgRsp.data.nReqMsgType = network::PROTO_CMD_INV;
                eventMsgRsp.data.nReqMsgSubType = MSGRSP_SUBTYPE_TXINV;
                eventMsgRsp.data.nRspResult = MSGRSP_RESULT_TXINV_RECEIVED;
                pPeerNet->DispatchEvent(&eventMsgRsp);
            }
            if (nBlockInvExistCount == MAX_GETBLOCKS_COUNT)
            {
                sched.SetNextGetBlocksTime(nNonce, 0);
            }
            else if (nBlockInvAddCount == MAX_GETBLOCKS_COUNT)
            {
                sched.SetNextGetBlocksTime(nNonce, GET_BLOCKS_INTERVAL_DEF_TIME / 2);
            }
            if (nBlockInvExistCount + nBlockInvAddCount > 0)
            {
                StdTrace("NetChannel", "CEventPeerInv: peer: %s, recv block inv, exist: %ld, add: %ld",
                         GetPeerAddressInfo(nNonce).c_str(), nBlockInvExistCount, nBlockInvAddCount);
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
            if (pTxPool->Get(inv.nHash, eventTx.data) || pBlockChain->GetTransaction(inv.nHash, eventTx.data))
            {
                pPeerNet->DispatchEvent(&eventTx);
                StdTrace("NetChannel", "CEventPeerGetData: get tx success, peer: %s, txid: %s",
                         GetPeerAddressInfo(nNonce).c_str(), inv.nHash.GetHex().c_str());
            }
            else
            {
                StdError("NetChannel", "CEventPeerGetData: Get transaction fail, txid: %s", inv.nHash.GetHex().c_str());
                eventGetFail.data.push_back(inv);
            }
        }
        else if (inv.nType == network::CInv::MSG_BLOCK)
        {
            bool fGetRet = false;
            network::CEventPeerBlock eventBlock(nNonce, hashFork);
            if (hashFork == pCoreProtocol->GetGenesisBlockHash())
            {
                try
                {
                    boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
                    CSchedule& sched = GetSchedule(hashFork);
                    if (sched.GetCachePowBlock(inv.nHash, eventBlock.data))
                    {
                        fGetRet = true;
                    }
                }
                catch (exception& e)
                {
                    DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, string("eventGetData: ") + e.what());
                    return true;
                }
            }
            if (!fGetRet && pBlockChain->GetBlock(inv.nHash, eventBlock.data))
            {
                fGetRet = true;
            }
            if (fGetRet)
            {
                pPeerNet->DispatchEvent(&eventBlock);
                StdTrace("NetChannel", "CEventPeerGetData: get block success, peer: %s, height: %d, block: %s",
                         GetPeerAddressInfo(nNonce).c_str(), CBlock::GetBlockHeightByHash(inv.nHash), inv.nHash.GetHex().c_str());
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
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
        mapPeer[nNonce].SetPeerGetDataTime(hashFork);
    }
    return true;
}

bool CNetChannel::HandleEvent(network::CEventPeerGetBlocks& eventGetBlocks)
{
    uint64 nNonce = eventGetBlocks.nNonce;
    uint256& hashFork = eventGetBlocks.hashFork;
    vector<uint256> vBlockHash;

    StdTrace("NetChannel", "CEventPeerGetBlocks: peer: %s, fork: %s",
             GetPeerAddressInfo(nNonce).c_str(), hashFork.GetHex().c_str());

    if (eventGetBlocks.data.vBlockHash.empty())
    {
        StdError("NetChannel", "CEventPeerGetBlocks: vBlockHash is empty");
        return false;
    }
    if (!pBlockChain->GetBlockInv(hashFork, eventGetBlocks.data, vBlockHash, MAX_GETBLOCKS_COUNT))
    {
        StdError("NetChannel", "CEventPeerGetBlocks: GetBlockInv fail");
        return false;
    }
    if (vBlockHash.empty())
    {
        network::CEventPeerMsgRsp eventMsgRsp(nNonce, hashFork);
        eventMsgRsp.data.nReqMsgType = network::PROTO_CMD_GETBLOCKS;
        eventMsgRsp.data.nReqMsgSubType = MSGRSP_SUBTYPE_NON;
        eventMsgRsp.data.nRspResult = MSGRSP_RESULT_GETBLOCKS_EMPTY;

        uint256 hashLastBlock;
        int nLastHeight = 0;
        int64 nLastTime = 0;
        uint16 nMintType = 0;
        if (pBlockChain->GetLastBlock(hashFork, hashLastBlock, nLastHeight, nLastTime, nMintType))
        {
            for (const uint256& hash : eventGetBlocks.data.vBlockHash)
            {
                if (hash == hashLastBlock)
                {
                    eventMsgRsp.data.nRspResult = MSGRSP_RESULT_GETBLOCKS_EQUAL;
                    break;
                }
            }
        }
        pPeerNet->DispatchEvent(&eventMsgRsp);
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
        StdTrace("NetChannel", "CEventPeerTx: receive tx success, peer: %s, txid: %s",
                 GetPeerAddressInfo(nNonce).c_str(), txid.GetHex().c_str());

        if (tx.IsMintTx())
        {
            StdDebug("NetChannel", "CEventPeerTx: tx is mint, peer: %s, txid: %s",
                     GetPeerAddressInfo(nNonce).c_str(), txid.GetHex().c_str());
            sched.SetDelayedClear(network::CInv(network::CInv::MSG_TX, txid), CSchedule::MAX_MINTTX_DELAYED_TIME); // Solve repeated and fast synchronization
            return true;
        }

        uint256 hashForkAnchor;
        int nHeightAnchor;
        if (pBlockChain->GetBlockLocation(tx.hashAnchor, hashForkAnchor, nHeightAnchor)
            && hashForkAnchor == hashFork)
        {
            AddNewTx(hashFork, txid, sched, setSchedPeer, setMisbehavePeer);
        }
        else
        {
            StdLog("NetChannel", "CEventPeerTx: GetBlockLocation fail, txid: %s, hashAnchor: %s",
                   txid.GetHex().c_str(), tx.hashAnchor.GetHex().c_str());
            sched.InvalidateTx(txid, setMisbehavePeer);
        }
        PostAddNew(hashFork, setSchedPeer, setMisbehavePeer);
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
    uint32 nBlockHeight = block.GetBlockHeight();
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
        StdTrace("NetChannel", "CEventPeerBlock: receive block success, peer: %s, height: %d, block hash: %s",
                 GetPeerAddressInfo(nNonce).c_str(), CBlock::GetBlockHeightByHash(hash), hash.GetHex().c_str());

        if (Config()->nMagicNum == MAINNET_MAGICNUM)
        {
            if (!block.IsExtended() && !pBlockChain->VerifyCheckPoint(hashFork, (int)nBlockHeight, hash))
            {
                StdError("NetChannel", "Fork %s block at height %d does not match checkpoint hash", hashFork.ToString().c_str(), (int)nBlockHeight);
                throw std::runtime_error("block doest not match checkpoint hash");
            }

            // recved forked block before last checkpoint need drop it and do not report DDoS
            if (block.IsSubsidiary())
            {
                auto checkpoint = pBlockChain->UpperBoundCheckPoint(hashFork, nBlockHeight);
                if (!checkpoint.IsNull() && nBlockHeight < checkpoint.nHeight && !pBlockChain->IsSameBranch(hashFork, block))
                {
                    sched.SetDelayedClear(network::CInv(network::CInv::MSG_BLOCK, hash), CSchedule::MAX_SUB_BLOCK_DELAYED_TIME);
                    return true;
                }
            }
        }

        if (hashFork != pCoreProtocol->GetGenesisBlockHash() && !pBlockChain->IsVacantBlockBeforeCreatedForkHeight(hashFork, block))
        {
            StdError("NetChannel", "Fork %s block at height %d is not vacant block", hashFork.ToString().c_str(), (int)nBlockHeight);
            throw std::runtime_error("block is not vacant before valid height of the created fork tx");
        }

        uint256 hashForkPrev;
        int nHeightPrev;
        if (pBlockChain->GetBlockLocation(block.hashPrev, hashForkPrev, nHeightPrev))
        {
            if (hashForkPrev == hashFork)
            {
                vector<pair<uint256, uint256>> vRefNextBlock;
                AddNewBlock(hashFork, hash, sched, setSchedPeer, setMisbehavePeer, vRefNextBlock, true);

                if (!vRefNextBlock.empty())
                {
                    AddRefNextBlock(vRefNextBlock);
                }
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

            uint256 hashFirst;
            uint256 hashPrev;
            if (CheckPrevBlock(hash, sched, hashFirst, hashPrev))
            {
                if (pBlockChain->GetBlockLocation(hashPrev, hashForkPrev, nHeightPrev))
                {
                    if (hashForkPrev == hashFork)
                    {
                        vector<pair<uint256, uint256>> vRefNextBlock;
                        AddNewBlock(hashFork, hashFirst, sched, setSchedPeer, setMisbehavePeer, vRefNextBlock, true);

                        if (!vRefNextBlock.empty())
                        {
                            AddRefNextBlock(vRefNextBlock);
                        }
                    }
                    else
                    {
                        StdLog("NetChannel", "CEventPeerBlock: hashForkPrev != hashFork, hashForkPrev: %s, hashFork: %s, hashBlockPrev: %s",
                               hashForkPrev.GetHex().c_str(), hashFork.GetHex().c_str(), hashPrev.GetHex().c_str());
                        sched.InvalidateBlock(hashFirst, setMisbehavePeer);
                    }
                }
            }
        }

        PostAddNew(hashFork, setSchedPeer, setMisbehavePeer);
    }
    catch (exception& e)
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, string("eventBlock: ") + e.what());
        return true;
    }
    if (block.IsPrimary() && block.IsProofOfWork())
    {
        InnerSubmitCachePowBlock();
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
            StdTrace("NetChannel", "CEventPeerGetFail: get data fail, peer: %s, inv: [%d] %s",
                     GetPeerAddressInfo(nNonce).c_str(), inv.nType, inv.nHash.GetHex().c_str());
            sched.CancelAssignedInv(nNonce, inv);
        }
    }
    catch (exception& e)
    {
        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, string("eventGetFail: ") + e.what());
    }
    return true;
}

bool CNetChannel::HandleEvent(network::CEventPeerMsgRsp& eventMsgRsp)
{
    uint64 nNonce = eventMsgRsp.nNonce;
    uint256& hashFork = eventMsgRsp.hashFork;

    switch (eventMsgRsp.data.nReqMsgType)
    {
    case network::PROTO_CMD_INV:
    {
        if (eventMsgRsp.data.nReqMsgSubType != MSGRSP_SUBTYPE_TXINV)
        {
            StdError("NetChannel", "CEventPeerMsgRsp: PROTO_CMD_INV response message, nReqMsgSubType error, peer: %s, nReqMsgSubType: %d",
                     GetPeerAddressInfo(nNonce).c_str(), eventMsgRsp.data.nReqMsgSubType);
            return false;
        }
        if (eventMsgRsp.data.nRspResult != MSGRSP_RESULT_TXINV_RECEIVED && eventMsgRsp.data.nRspResult != MSGRSP_RESULT_TXINV_COMPLETE)
        {
            StdError("NetChannel", "CEventPeerMsgRsp: PROTO_CMD_INV response message, nRspResult error, peer: %s, nRspResult: %d",
                     GetPeerAddressInfo(nNonce).c_str(), eventMsgRsp.data.nRspResult);
            return false;
        }
        bool fResetTxInvSyn = false;
        {
            boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
            map<uint64, CNetChannelPeer>::iterator it = mapPeer.find(nNonce);
            if (it != mapPeer.end())
            {
                it->second.ResetTxInvSynStatus(hashFork, eventMsgRsp.data.nRspResult == MSGRSP_RESULT_TXINV_COMPLETE);
                fResetTxInvSyn = true;
            }
        }
        if (fResetTxInvSyn)
        {
            StdTrace("NetChannel", "CEventPeerMsgRsp: recv tx inv response: %s, peer: %s, fork: %s",
                     (eventMsgRsp.data.nRspResult == MSGRSP_RESULT_TXINV_COMPLETE ? "peer completed" : "peer received"),
                     GetPeerAddressInfo(nNonce).c_str(), hashFork.GetHex().c_str());
            if (eventMsgRsp.data.nRspResult == MSGRSP_RESULT_TXINV_COMPLETE)
            {
                BroadcastTxInv(hashFork);
            }
        }
        else
        {
            StdError("NetChannel", "CEventPeerMsgRsp: recv tx inv response: %s, not find peer, peer: %s, fork: %s",
                     (eventMsgRsp.data.nRspResult == MSGRSP_RESULT_TXINV_COMPLETE ? "peer completed" : "peer received"),
                     GetPeerAddressInfo(nNonce).c_str(), hashFork.GetHex().c_str());
        }
        break;
    }
    case network::PROTO_CMD_GETBLOCKS:
    {
        try
        {
            boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
            CSchedule& sched = GetSchedule(hashFork);

            if (eventMsgRsp.data.nRspResult == MSGRSP_RESULT_GETBLOCKS_EMPTY)
            {
                uint256 hashInvBlock;
                if (sched.GetLocatorInvBlockHash(nNonce, hashInvBlock) > 0)
                {
                    sched.SetLocatorInvBlockHash(nNonce, 0, uint256(), uint256(1));
                }
                DispatchGetBlocksEvent(nNonce, hashFork);
            }
            else if (eventMsgRsp.data.nRspResult == MSGRSP_RESULT_GETBLOCKS_EQUAL)
            {
                uint256 hashInvBlock;
                int nInvHeight = sched.GetLocatorInvBlockHash(nNonce, hashInvBlock);
                StdTrace("NetChannel", "CEventPeerMsgRsp: peer: %s, synchronization is the same, SynInvHeight: %d, SynInvBlock: %s ",
                         GetPeerAddressInfo(nNonce).c_str(), nInvHeight, hashInvBlock.GetHex().c_str());
                sched.SetNextGetBlocksTime(nNonce, GET_BLOCKS_INTERVAL_EQUAL_TIME);
                SchedulePeerInv(nNonce, hashFork, sched);
            }
            else
            {
                StdError("NetChannel", "CEventPeerMsgRsp: PROTO_CMD_GETBLOCKS response message, nRspResult error, peer: %s, nRspResult: %d",
                         GetPeerAddressInfo(nNonce).c_str(), eventMsgRsp.data.nRspResult);
                return false;
            }
        }
        catch (exception& e)
        {
            DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, string("eventMsgRsp: ") + e.what());
        }
        break;
    }
    default:
    {
        StdError("NetChannel", "CEventPeerMsgRsp: nReqMsgType error, peer: %s, nReqMsgType: %d",
                 GetPeerAddressInfo(nNonce).c_str(), eventMsgRsp.data.nReqMsgType);
        return false;
    }
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
        if (sched.CheckAddInvIdleLocation(nNonce, network::CInv::MSG_BLOCK))
        {
            uint256 hashDepth;
            sched.GetLocatorDepthHash(nNonce, hashDepth);

            uint256 hashInvBlock;
            int nLocatorInvHeight;
            nLocatorInvHeight = sched.GetLocatorInvBlockHash(nNonce, hashInvBlock);

            network::CEventPeerGetBlocks eventGetBlocks(nNonce, hashFork);
            if (nLocatorInvHeight > 0)
            {
                eventGetBlocks.data.vBlockHash.push_back(hashInvBlock);
                sched.SetLocatorDepthHash(nNonce, uint256());
            }
            else
            {
                if (pBlockChain->GetBlockLocator(hashFork, eventGetBlocks.data, hashDepth, MAX_GETBLOCKS_COUNT - 1))
                {
                    sched.SetLocatorDepthHash(nNonce, hashDepth);
                }
            }
            if (!eventGetBlocks.data.vBlockHash.empty())
            {
                StdLog("NetChannel", "DispatchGetBlocksEvent: Peer: %s, nLocatorInvHeight: %d, hashInvBlock: %s, hashFork: %s.",
                       GetPeerAddressInfo(nNonce).c_str(), nLocatorInvHeight, hashInvBlock.GetHex().c_str(), hashFork.GetHex().c_str());
                pPeerNet->DispatchEvent(&eventGetBlocks);
                sched.SetNextGetBlocksTime(nNonce, GET_BLOCKS_INTERVAL_DEF_TIME);
            }
        }
        else
        {
            sched.SetNextGetBlocksTime(nNonce, GET_BLOCKS_INTERVAL_DEF_TIME);
        }
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
    if (sched.ScheduleBlockInv(nNonce, eventGetData.data, 1, fMissingPrev, fEmpty))
    {
        if (fMissingPrev)
        {
            DispatchGetBlocksEvent(nNonce, hashFork);
        }
        else if (eventGetData.data.empty())
        {
            bool fTxReceivedAll = false;
            if (!sched.ScheduleTxInv(nNonce, eventGetData.data, MAX_PEER_SCHED_COUNT, fTxReceivedAll))
            {
                DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK, "SchedulePeerInv1: ScheduleTxInv fail");
            }
            else
            {
                if (fTxReceivedAll)
                {
                    bool fIsWait = false;
                    {
                        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
                        map<uint64, CNetChannelPeer>::iterator it = mapPeer.find(nNonce);
                        if (it != mapPeer.end())
                        {
                            fIsWait = it->second.CheckWaitGetTxComplete(hashFork);
                        }
                    }
                    if (fIsWait)
                    {
                        network::CEventPeerMsgRsp eventMsgRsp(nNonce, hashFork);
                        eventMsgRsp.data.nReqMsgType = network::PROTO_CMD_INV;
                        eventMsgRsp.data.nReqMsgSubType = MSGRSP_SUBTYPE_TXINV;
                        eventMsgRsp.data.nRspResult = MSGRSP_RESULT_TXINV_COMPLETE;
                        pPeerNet->DispatchEvent(&eventMsgRsp);

                        StdTrace("NetChannel", "SchedulePeerInv: send tx inv response: get tx complete, peer: %s, fork: %s",
                                 GetPeerAddressInfo(nNonce).c_str(), hashFork.GetHex().c_str());
                    }
                }
            }
        }
        else
        {
            sched.SetNextGetBlocksTime(nNonce, 0);
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

        string strInv;
        int nInvType = -1;
        for (const auto& inv : eventGetData.data)
        {
            if (nInvType == -1)
            {
                nInvType = inv.nType;
                strInv = inv.nHash.GetHex();
            }
            else
            {
                strInv += (string(",") + inv.nHash.GetHex());
            }
        }
        StdTrace("NetChannel", "SchedulePeerInv: send [%s] getdata request, peer: %s, inv hash: %s",
                 (nInvType == network::CInv::MSG_TX ? "tx" : "block"), GetPeerAddressInfo(nNonce).c_str(), strInv.c_str());
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

bool CNetChannel::CheckPrevTx(const CTransaction& tx, uint64 nNonce, const uint256& hashFork, CSchedule& sched, const set<uint64>& setSchedPeer)
{
    set<uint256> setMissingPrevTx;
    if (GetMissingPrevTx(tx, setMissingPrevTx))
    {
        const uint256& txid = tx.GetHash();

        StdTrace("NetChannel", "CheckPrevTx: missing prev tx, peer: %s, txid: %s",
                 GetPeerAddressInfo(nNonce).c_str(), txid.GetHex().c_str());

        uint256 hashLastBlock;
        int nLastBlockHeight = -1;
        int64 nLastTime = 0;
        uint16 nLastMintType = 0;
        if (!pBlockChain->GetLastBlock(hashFork, hashLastBlock, nLastBlockHeight, nLastTime, nLastMintType))
        {
            StdError("NetChannel", "CheckPrevTx: peer: %s, GetLastBlock fail, fork: %s",
                     GetPeerAddressInfo(nNonce).c_str(), hashFork.GetHex().c_str());
            return false;
        }

        for (const uint256& prev : setMissingPrevTx)
        {
            sched.AddOrphanTxPrev(txid, prev);
            if (CTxId(prev).GetTxTime() >= nLastTime + MAX_TXINV_INTERVAL_TIME)
            {
                continue;
            }
            network::CInv inv(network::CInv::MSG_TX, prev);
            if (!sched.CheckPrevTxInv(inv))
            {
                for (const uint64 nNonceSched : setSchedPeer)
                {
                    if (sched.AddNewInv(inv, nNonceSched))
                    {
                        StdTrace("NetChannel", "CheckPrevTx: missing prev tx, add tx inv success, peer: %s, prev: %s, next: %s",
                                 GetPeerAddressInfo(nNonceSched).c_str(), prev.GetHex().c_str(), txid.GetHex().c_str());
                    }
                    else
                    {
                        StdTrace("NetChannel", "CheckPrevTx: missing prev tx, add tx inv fail, peer: %s, prev: %s, next: %s",
                                 GetPeerAddressInfo(nNonceSched).c_str(), prev.GetHex().c_str(), txid.GetHex().c_str());
                    }
                }
            }
        }
        return false;
    }
    return true;
}

void CNetChannel::AddNewBlock(const uint256& hashFork, const uint256& hash, CSchedule& sched,
                              set<uint64>& setSchedPeer, set<uint64>& setMisbehavePeer, vector<pair<uint256, uint256>>& vRefNextBlock, bool fCheckPow)
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
            uint256 hashBlockRef;
            if (pBlock->IsSubsidiary() || pBlock->IsExtended())
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

                    sched.AddRefBlock(proof.hashRefBlock, hashFork, hashBlock);
                    sched.SetDelayedClear(network::CInv(network::CInv::MSG_BLOCK, hashBlock), CSchedule::MAX_SUB_BLOCK_DELAYED_TIME);
                    return;
                }
                hashBlockRef = proof.hashRefBlock;
            }

            if (!pBlock->IsVacant() && !sched.IsRepeatBlock(hashBlock))
            {
                if (!pBlockChain->VerifyRepeatBlock(hashFork, *pBlock, hashBlockRef))
                {
                    StdLog("NetChannel", "NetChannel AddNewBlock: block repeat mint, peer: %s, height: %d, block: %s",
                           GetPeerAddressInfo(nNonceSender).c_str(), CBlock::GetBlockHeightByHash(hashBlock), hashBlock.GetHex().c_str());
                    if (!sched.SetRepeatBlock(nNonceSender, hashBlock))
                    {
                        StdLog("NetChannel", "NetChannel AddNewBlock: Generate multiple repeat blocks, peer: %s, height: %d, block: %s",
                               GetPeerAddressInfo(nNonceSender).c_str(), CBlock::GetBlockHeightByHash(hashBlock), hashBlock.GetHex().c_str());
                        setMisbehavePeer.insert(nNonceSender);
                    }
                    return;
                }
            }

            if ((fCheckPow || hash != hashBlock) && pBlock->IsPrimary() && pBlock->IsProofOfWork())
            {
                bool fAddBlock = false;
                bool fVerifyPowBlock = false;
                if (!sched.GetPowBlockState(hashBlock, fVerifyPowBlock))
                {
                    StdLog("NetChannel", "NetChannel AddNewBlock: GetPowBlockState fail, peer: %s, height: %d, block: %s",
                           GetPeerAddressInfo(nNonceSender).c_str(), CBlock::GetBlockHeightByHash(hashBlock), hashBlock.GetHex().c_str());
                    setMisbehavePeer.insert(nNonceSender);
                    return;
                }
                if (!fVerifyPowBlock)
                {
                    bool fLongChain = false;
                    if (pBlockChain->VerifyPowBlock(*pBlock, fLongChain) != OK)
                    {
                        StdLog("NetChannel", "AddNewBlock VerifyPowBlock fail, peer: %s, height: %d, block: %s",
                               GetPeerAddressInfo(nNonceSender).c_str(), CBlock::GetBlockHeightByHash(hashBlock), hashBlock.GetHex().c_str());
                        setMisbehavePeer.insert(nNonceSender);
                        return;
                    }
                    sched.SetPowBlockVerifyState(hashBlock, true);

                    if (fLongChain)
                    {
                        uint256 hashLastBlock;
                        int nLastHeight;
                        int64 nLastTime;
                        uint16 nLastMintType;
                        if (!pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashLastBlock, nLastHeight, nLastTime, nLastMintType))
                        {
                            StdLog("NetChannel", "AddNewBlock GetLastBlock fail, peer: %s, height: %d, block: %s",
                                   GetPeerAddressInfo(nNonceSender).c_str(), CBlock::GetBlockHeightByHash(hashBlock), hashBlock.GetHex().c_str());
                            return;
                        }

                        uint256 hashFirstBlock;
                        if (sched.GetFirstCachePowBlock(pBlock->GetBlockHeight(), hashFirstBlock)
                            && hashFirstBlock == hashBlock)
                        {
                            InnerBroadcastBlockInv(hashFork, hashBlock);
                            StdDebug("NetChannel", "AddNewBlock InnerBroadcastBlockInv: height: %d, block: %s",
                                     CBlock::GetBlockHeightByHash(hashBlock), hashBlock.GetHex().c_str());
                        }
                    }
                    else
                    {
                        fAddBlock = true;
                    }
                }

                if (!fAddBlock)
                {
                    set<uint64> setKnownPeer;
                    sched.GetKnownPeer(network::CInv(network::CInv::MSG_BLOCK, hashBlock), setKnownPeer);
                    setSchedPeer.insert(setKnownPeer.begin(), setKnownPeer.end());

                    StdDebug("NetChannel", "AddNewBlock cache pow block, peer: %s, height: %d, block: %s",
                             GetPeerAddressInfo(nNonceSender).c_str(), CBlock::GetBlockHeightByHash(hashBlock), hashBlock.GetHex().c_str());
                    continue;
                }

                uint256 hashForkPrev;
                int nHeightPrev;
                if (!pBlockChain->GetBlockLocation(pBlock->hashPrev, hashForkPrev, nHeightPrev))
                {
                    set<uint64> setKnownPeer;
                    sched.GetKnownPeer(network::CInv(network::CInv::MSG_BLOCK, hashBlock), setKnownPeer);
                    setSchedPeer.insert(setKnownPeer.begin(), setKnownPeer.end());

                    StdDebug("NetChannel", "AddNewBlock pow block not find prev, peer: %s, height: %d, block: %s",
                             GetPeerAddressInfo(nNonceSender).c_str(), CBlock::GetBlockHeightByHash(hashBlock), hashBlock.GetHex().c_str());
                    continue;
                }
            }

            Errno err = pDispatcher->AddNewBlock(*pBlock, nNonceSender);
            if (err == OK)
            {
                StdDebug("NetChannel", "NetChannel AddNewBlock success, peer: %s, height: %d, block: %s",
                         GetPeerAddressInfo(nNonceSender).c_str(), CBlock::GetBlockHeightByHash(hashBlock), hashBlock.GetHex().c_str());

                if (pBlock->IsPrimary())
                {
                    sched.GetNextRefBlock(hashBlock, vRefNextBlock);
                }

                {
                    const CTransaction& tx = pBlock->txMint;
                    uint256 txid = tx.GetHash();
                    {
                        set<uint256> setTx;
                        vector<uint256> vtx;
                        sched.GetNextTx(txid, vtx, setTx);
                        if (!vtx.empty())
                        {
                            set<uint64> setPrevSchedPeer;
                            set<uint64> setPrevMisbehavePeer;
                            for (const uint256& hash : vtx)
                            {
                                AddNewTx(hashFork, hash, sched, setPrevSchedPeer, setPrevMisbehavePeer);
                            }
                        }
                    }
                    if (sched.RemoveInv(network::CInv(network::CInv::MSG_TX, txid), setSchedPeer))
                    {
                        StdDebug("NetChannel", "NetChannel AddNewBlock: remove mint tx inv success, peer: %s, txid: %s",
                                 GetPeerAddressInfo(nNonceSender).c_str(), txid.GetHex().c_str());
                    }
                }

                for (const CTransaction& tx : pBlock->vtx)
                {
                    uint256 txid = tx.GetHash();
                    {
                        set<uint256> setTx;
                        vector<uint256> vtx;
                        sched.GetNextTx(txid, vtx, setTx);
                        if (!vtx.empty())
                        {
                            set<uint64> setPrevSchedPeer;
                            set<uint64> setPrevMisbehavePeer;
                            for (const uint256& hash : vtx)
                            {
                                AddNewTx(hashFork, hash, sched, setPrevSchedPeer, setPrevMisbehavePeer);
                            }
                        }
                    }
                    if (sched.RemoveInv(network::CInv(network::CInv::MSG_TX, txid), setSchedPeer))
                    {
                        StdDebug("NetChannel", "NetChannel AddNewBlock: remove tx inv success, peer: %s, txid: %s",
                                 GetPeerAddressInfo(nNonceSender).c_str(), txid.GetHex().c_str());
                    }
                }

                uint256 hashGetFork;
                int nGetHeight;
                uint256 hashGetNext;
                if (pBlockChain->GetBlockLocation(hashBlock, hashGetFork, nGetHeight, hashGetNext))
                {
                    sched.SetLocatorInvBlockHash(nNonceSender, nGetHeight, hashBlock, hashGetNext);
                }

                set<uint64> setKnownPeer;
                sched.GetNextBlock(hashBlock, vBlockHash);
                sched.RemoveInv(network::CInv(network::CInv::MSG_BLOCK, hashBlock), setKnownPeer);
                DispatchAwardEvent(nNonceSender, CEndpointManager::VITAL_DATA);
                setSchedPeer.insert(setKnownPeer.begin(), setKnownPeer.end());
            }
            else if (err == ERR_ALREADY_HAVE /*&& pBlock->IsVacant()*/)
            {
                StdLog("NetChannel", "NetChannel AddNewBlock: block already have, peer: %s, height: %d, block: %s",
                       GetPeerAddressInfo(nNonceSender).c_str(), CBlock::GetBlockHeightByHash(hashBlock), hashBlock.GetHex().c_str());
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
            if (!CheckPrevTx(*pTx, nNonceSender, hashFork, sched, setSchedPeer))
            {
                continue;
            }

            if (pTxPool->Exists(hashTx) || pBlockChain->ExistsTx(hashTx))
            {
                StdDebug("NetChannel", "NetChannel AddNewTx: tx at blockchain or txpool exists, peer: %s, txid: %s",
                         GetPeerAddressInfo(nNonceSender).c_str(), hashTx.GetHex().c_str());
                sched.GetNextTx(hashTx, vtx, setTx);
                sched.RemoveInv(network::CInv(network::CInv::MSG_TX, hashTx), setSchedPeer);
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
                    StdDebug("NetChannel", "NetChannel AddNewTx fail, remove inv, peer: %s, txid: %s, err: [%d] %s",
                             GetPeerAddressInfo(nNonceSender).c_str(), hashTx.GetHex().c_str(), err, ErrorString(err));
                    sched.RemoveInv(network::CInv(network::CInv::MSG_TX, hashTx), setSchedPeer);
                }
                else
                {
                    StdLog("NetChannel", "NetChannel AddNewTx fail, peer: %s, txid: %s, err: [%d] %s",
                           GetPeerAddressInfo(nNonceSender).c_str(), hashTx.GetHex().c_str(), err, ErrorString(err));
                }
            }
            else if (err == ERR_TRANSACTION_TOO_MANY_CERTTX)
            {
                StdLog("NetChannel", "NetChannel AddNewTx fail, too many cert tx, peer: %s, txid: %s",
                       GetPeerAddressInfo(nNonceSender).c_str(), hashTx.GetHex().c_str());
                sched.SetDelayedClear(network::CInv(network::CInv::MSG_TX, hashTx), CSchedule::MAX_CERTTX_DELAYED_TIME);
            }
            else
            {
                StdLog("NetChannel", "NetChannel AddNewTx fail, invalidate tx, peer: %s, txid: %s, err: [%d] %s",
                       GetPeerAddressInfo(nNonceSender).c_str(), hashTx.GetHex().c_str(), err, ErrorString(err));
                sched.InvalidateTx(hashTx, setMisbehavePeer);
            }
        }
    }
    if (nAddNewTx)
    {
        BroadcastTxInv(hashFork);
    }
}

void CNetChannel::AddRefNextBlock(const vector<pair<uint256, uint256>>& vRefNextBlock)
{
    set<uint256> setHash;
    for (int i = 0; i < vRefNextBlock.size(); i++)
    {
        const uint256& hashNextFork = vRefNextBlock[i].first;
        const uint256& hashNextBlock = vRefNextBlock[i].second;
        if (setHash.find(hashNextBlock) == setHash.end())
        {
            setHash.insert(hashNextBlock);

            try
            {
                CSchedule& sched = GetSchedule(hashNextFork);

                set<uint64> setSchedPeer, setMisbehavePeer;
                vector<pair<uint256, uint256>> vTemp;
                AddNewBlock(hashNextFork, hashNextBlock, sched, setSchedPeer, setMisbehavePeer, vTemp, true);
            }
            catch (exception& e)
            {
                StdError("NetChannel", "AddRefNextBlock fail, fork: %s, block: %s, err: %s",
                         hashNextFork.GetHex().c_str(), hashNextBlock.GetHex().c_str(), e.what());
            }
        }
    }
}

void CNetChannel::PostAddNew(const uint256& hashFork, set<uint64>& setSchedPeer, set<uint64>& setMisbehavePeer)
{
    try
    {
        CSchedule& sched = GetSchedule(hashFork); // Resolve unsubscribefork errors
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
    catch (exception& e)
    {
        StdError("NetChannel", "PostAddNew error, fork: %s, err: %s", hashFork.GetHex().c_str(), e.what());
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
        nTimerPushTx = 0;
        bool fPushComplete = true;
        if (!setPushTxFork.empty())
        {
            set<uint256>::iterator it = setPushTxFork.begin();
            while (it != setPushTxFork.end())
            {
                if (!PushTxInv(*it))
                {
                    fPushComplete = false;
                }
                ++it;
            }
        }
        if (!fPushComplete)
        {
            nTimerPushTx = SetTimer(PUSHTX_TIMEOUT, boost::bind(&CNetChannel::PushTxTimerFunc, this, _1));
            fStartIdlePushTxTimer = false;
        }
        else
        {
            nTimerPushTx = SetTimer(SYNTXINV_TIMEOUT, boost::bind(&CNetChannel::PushTxTimerFunc, this, _1));
            fStartIdlePushTxTimer = true;
        }
    }
}

bool CNetChannel::PushTxInv(const uint256& hashFork)
{
    // if (!IsForkSynchronized(hashFork))
    // {
    //     return false;
    // }

    bool fAllowSynTxInv = false;
    {
        boost::unique_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); !fAllowSynTxInv && it != mapPeer.end(); ++it)
        {
            switch (it->second.CheckTxInvSynStatus(hashFork))
            {
            case CNetChannelPeer::CHECK_SYNTXINV_STATUS_RESULT_WAIT_SYN:
                break;
            case CNetChannelPeer::CHECK_SYNTXINV_STATUS_RESULT_WAIT_TIMEOUT:
                DispatchMisbehaveEvent(it->first, CEndpointManager::RESPONSE_FAILURE, "Wait tx inv response timeout");
                break;
            case CNetChannelPeer::CHECK_SYNTXINV_STATUS_RESULT_ALLOW_SYN:
                fAllowSynTxInv = true;
                break;
            }
        }
    }

    bool fCompleted = true;
    if (fAllowSynTxInv)
    {
        vector<uint256> vTxPool;
        pTxPool->ListTx(hashFork, vTxPool);
        if (!vTxPool.empty() && !mapPeer.empty())
        {
            boost::unique_lock<boost::shared_mutex> rlock(rwNetPeer);
            for (map<uint64, CNetChannelPeer>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
            {
                CNetChannelPeer& peer = it->second;
                network::CEventPeerInv eventInv(it->first, hashFork);
                if (!peer.MakeTxInv(hashFork, vTxPool, eventInv.data))
                {
                    DispatchMisbehaveEvent(it->first, CEndpointManager::RESPONSE_FAILURE, "Wait tx inv response timeout");
                }
                else
                {
                    if (!eventInv.data.empty())
                    {
                        pPeerNet->DispatchEvent(&eventInv);
                        StdTrace("NetChannel", "PushTxInv: send tx inv request, inv count: %ld, peer: %s",
                                 eventInv.data.size(), peer.GetRemoteAddress().c_str());
                        if (fCompleted && eventInv.data.size() == network::CInv::MAX_INV_COUNT)
                        {
                            fCompleted = false;
                        }
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
    map<uint64, CNetChannelPeer>::iterator it = mapPeer.find(nNonce);
    if (it != mapPeer.end())
    {
        return it->second.GetRemoteAddress();
    }
    return string("0.0.0.0");
}

bool CNetChannel::CheckPrevBlock(const uint256& hash, CSchedule& sched, uint256& hashFirst, uint256& hashPrev)
{
    uint256 hashBlock = hash;
    while (hashBlock != 0)
    {
        uint64 nNonceSender = 0;
        CBlock* pBlock = sched.GetBlock(hashBlock, nNonceSender);
        if (pBlock == nullptr)
        {
            break;
        }
        hashFirst = hashBlock;
        hashPrev = pBlock->hashPrev;

        vector<uint256> vNextBlock;
        sched.GetNextBlock(pBlock->hashPrev, vNextBlock);
        if (vNextBlock.empty())
        {
            break;
        }
        bool fNext = false;
        for (const uint256& hashNext : vNextBlock)
        {
            if (hashNext == hashBlock)
            {
                fNext = true;
                break;
            }
        }
        if (!fNext)
        {
            break;
        }

        hashBlock = pBlock->hashPrev;
    }
    return (hashFirst != hash);
}

void CNetChannel::InnerBroadcastBlockInv(const uint256& hashFork, const uint256& hashBlock)
{
    set<uint64> setKnownPeer;
    CSchedule& sched = GetSchedule(hashFork);
    sched.GetKnownPeer(network::CInv(network::CInv::MSG_BLOCK, hashBlock), setKnownPeer);

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

void CNetChannel::InnerSubmitCachePowBlock()
{
    bool fContinue;
    uint256 hashPrevBlock;
    do
    {
        fContinue = false;
        CAgreementBlock agreeBlock;
        pConsensus->GetNextConsensus(agreeBlock);
        if (agreeBlock.hashPrev != 0 && agreeBlock.hashPrev != hashPrevBlock)
        {
            hashPrevBlock = agreeBlock.hashPrev;

            CConsensusParam consParam;
            consParam.hashPrev = agreeBlock.hashPrev;
            consParam.nPrevTime = agreeBlock.nPrevTime;
            consParam.nPrevHeight = agreeBlock.nPrevHeight;
            consParam.nPrevMintType = agreeBlock.nPrevMintType;
            consParam.nWaitTime = agreeBlock.nWaitTime;
            consParam.fPow = agreeBlock.agreement.IsProofOfWork();
            consParam.ret = agreeBlock.ret;

            fContinue = SubmitCachePowBlock(consParam);
        }
    } while (fContinue);
}

} // namespace bigbang
