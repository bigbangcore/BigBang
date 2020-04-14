// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "netchn.h"

#include <boost/bind.hpp>
#include <defs.h>

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
    auto it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        return (*it).second.fSynchronized;
    }
    return false;
}

bool CNetChannelPeer::SetSyncStatus(const uint256& hashFork, bool fSync, bool& fInverted)
{
    auto it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        fInverted = ((*it).second.fSynchronized != fSync);
        (*it).second.fSynchronized = fSync;
        return true;
    }
    return false;
}

void CNetChannelPeer::AddKnownTx(const uint256& hashFork, const vector<uint256>& vTxHash, size_t nTotalSynTxCount)
{
    auto it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        (*it).second.AddKnownTx(vTxHash, nTotalSynTxCount);
    }
}

bool CNetChannelPeer::MakeTxInv(const uint256& hashFork, const vector<uint256>& vTxPool, vector<network::CInv>& vInv)
{
    auto it = mapSubscribedFork.find(hashFork);
    if (it != mapSubscribedFork.end())
    {
        CNetChannelPeerFork& peerFork = (*it).second;
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
                else if (!(*it).second.IsKnownTx(txid))
                {
                    vInv.emplace_back(network::CInv(network::CInv::MSG_TX, txid));
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
    pMQCluster = nullptr;
    fStartIdlePushTxTimer = false;
    nNodeCat = NODE_CAT_BBCNODE;
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

    if (!GetObject("mqcluster", pMQCluster))
    {
        Error("Failed to request mqcluster");
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
    pMQCluster = nullptr;
}

bool CNetChannel::HandleInvoke()
{
    {
        boost::unique_lock<boost::mutex> lock(mtxPushTx);
        nTimerPushTx = 0;
        fStartIdlePushTxTimer = false;
    }
    nNodeCat = dynamic_cast<const CBasicConfig*>(Config())->nCatOfNode;
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
    if (pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashBlock, nHeight, nTime))
    {
        return nHeight;
    }
    return 0;
}

bool CNetChannel::IsForkSynchronized(const uint256& hashFork) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
    auto it = mapUnsync.find(hashFork);
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
    eventInv.data.emplace_back(network::CInv(network::CInv::MSG_BLOCK, hashBlock));
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (auto const& p : mapPeer)
        {
            uint64 nNonce = p.first;
            if (!setKnownPeer.count(nNonce) && p.second.IsSubscribed(hashFork))
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

    if (hashFork == pCoreProtocol->GetGenesisBlockHash() && NODE_CAT_FORKNODE == nNodeCat)
    {
        StdLog("NetChannel", "SubscribeFork: succeed in filtering the main chain for fork nodes with nonce[%d]", nNonce);
        return;
    }

    if (hashFork != pCoreProtocol->GetGenesisBlockHash() && NODE_CAT_DPOSNODE == nNodeCat)
    {
        StdLog("NetChannel", "SubscribeFork: succeed in filtering biz chains for dpos nodes with nonce[%d]", nNonce);
        return;
    }

    network::CEventPeerSubscribe eventSubscribe(0ULL, pCoreProtocol->GetGenesisBlockHash());
    eventSubscribe.data.push_back(hashFork);
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwNetPeer);
        for (const auto& p : mapPeer)
        {
            eventSubscribe.nNonce = p.first;
            pPeerNet->DispatchEvent(&eventSubscribe);
            DispatchGetBlocksEvent(p.first, hashFork);
            BroadcastTxInv(hashFork);
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
        for (const auto& p : mapPeer)
        {
            eventUnsubscribe.nNonce = p.first;
            pPeerNet->DispatchEvent(&eventUnsubscribe);
        }
    }
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
        if (NODE_CAT_FORKNODE != nNodeCat)
        {
            DispatchGetBlocksEvent(nNonce, pCoreProtocol->GetGenesisBlockHash());
            BroadcastTxInv(pCoreProtocol->GetGenesisBlockHash());
        }
        else
        {
            StdLog("NetChannel", "CEventPeerActive: succeed in filtering to "
                                 "get main chain from peer[%s]",
                   GetPeerAddressInfo(nNonce).c_str());
        }

        network::CEventPeerSubscribe eventSubscribe(nNonce, pCoreProtocol->GetGenesisBlockHash());
        {
            boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
            for (auto const& sch : mapSched)
            {
                if (sch.first != pCoreProtocol->GetGenesisBlockHash())
                {
                    eventSubscribe.data.push_back(sch.first);
                    StdLog("NetChannel", "CEventPeerActive: prepare to subscribe fork[%s]", sch.first.ToString().c_str());
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
        for (auto& sch : mapSched)
        {
            CSchedule& sched = sch.second;
            set<uint64> setSchedPeer;
            sched.RemovePeer(nNonce, setSchedPeer);

            for (const uint64 nNonceSched : setSchedPeer)
            {
                SchedulePeerInv(nNonceSched, sch.first, sched);
            }
        }
    }
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);

        auto it = mapPeer.find(nNonce);
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

bool CNetChannel::HandleEvent(network::CEventPeerGetBizForks& eventGetBizForks)
{
    uint64 nNonce = eventGetBizForks.nNonce;
    vector<uint256>& bizForks = eventGetBizForks.data;
    StdLog("NetChannel", "CEventPeerGetBizForks: peer[%s] is asking for total [%d] biz forks",
           GetPeerAddressInfo(nNonce).c_str(), bizForks.size());
    for (auto const& f : bizForks)
    {
        StdLog("NetChannel", "CEventPeerGetBizForks: peer[%s] is asking for fork[%s]",
               GetPeerAddressInfo(nNonce).c_str(), f.ToString().c_str());
    }

    vector<storage::CSuperNode> nodes;
    if (!pBlockChain->ListSuperNode(nodes))
    {
        StdError("NetChannel", "CEventPeerGetBizForks: ListSuperNode failed");
        return false;
    }

    StdLog("NetChannel", "CEventPeerGetBizForks: there is/are [%d] node(s) all", nodes.size());
    for (auto const& node : nodes)
    {
        StdLog("NetChannel", "CEventPeerGetBizForks: biz fork as [%s]", node.ToString().c_str());
    }

    //filter out dpos node itself
    for (auto it = nodes.begin(); it != nodes.end(); ++it)
    {
        if (0 == it->ipAddr)
        {
            nodes.erase(it);
            break;
        }
    }

    StdLog("NetChannel", "CEventPeerGetBizForks: there is/are [%d] node(s) without dpos node", nodes.size());
    for (auto const& node : nodes)
    {
        StdLog("NetChannel", "CEventPeerGetBizForks: biz fork as [%s] w/o dpos node", node.ToString().c_str());
    }

    storage::CForkKnownIPSet setForkIp;
    for (auto const& node : nodes)
    {
        for (auto const& fork : node.vecOwnedForks)
        {
            setForkIp.emplace(storage::CForkKnownIP(fork, node.ipAddr));
        }
    }

    if (bizForks.empty())
    { //get all to peer
        map<uint256, vector<uint32>> mapForkIp;
        const storage::CForkKnownIpSetById& idxForkID = setForkIp.get<0>();
        for (auto const& it : idxForkID)
        {
            mapForkIp[it.forkID].emplace_back(it.nodeIP);
        }

        network::CEventPeerBizForks eventFork(nNonce);
        eventFork.data = mapForkIp;
        pPeerNet->DispatchEvent(&eventFork);

        return true;
    }

    for (auto const& fork : bizForks)
    {
        map<uint256, vector<uint32>> mapForkIp;
        const storage::CForkKnownIpSetById& idxForkID = setForkIp.get<0>();
        auto itBegin = idxForkID.equal_range(fork).first;
        auto itEnd = idxForkID.equal_range(fork).second;

        while (itBegin != itEnd)
        {
            mapForkIp[fork].emplace_back(itBegin->nodeIP);
            ++itBegin;
        }

        network::CEventPeerBizForks eventFork(nNonce);
        eventFork.data = mapForkIp;
        pPeerNet->DispatchEvent(&eventFork);
    }

    return true;
}

bool CNetChannel::HandleEvent(network::CEventPeerBizForks& eventBizForks)
{
    uint64 nNonce = eventBizForks.nNonce;
    map<uint256, vector<uint32>> mapForkIp = eventBizForks.data.mapFork;

    StdLog("NetChannel", "CEventPeerBizForks: peer[%s] is feeding total [%d] biz forks",
           GetPeerAddressInfo(nNonce).c_str(), mapForkIp.size());
    for (auto const& fork : mapForkIp)
    {
        StdLog("NetChannel", "CEventPeerBizForks: peer[%s] is feeding total [%d] IPs for fork[%s]",
               fork.second.size(), GetPeerAddressInfo(nNonce).c_str());
        for (auto const& ip : fork.second)
        {
            StdLog("NetChannel", "CEventPeerBizForks: peer[%s] is feeding IP[%s] for fork[%s]",
                   storage::CSuperNode::Int2Ip(ip).c_str(), GetPeerAddressInfo(nNonce).c_str());
        }
    }

    storage::CForkKnownIPSet setForkIp;
    for (auto const& fork : mapForkIp)
    {
        for (auto const ip : fork.second)
        {
            setForkIp.emplace(storage::CForkKnownIP(fork.first, ip));
        }
    }

    map<uint32, vector<uint256>> mapIpForks;
    const storage::CForkKnownIpSetByIp& idxNodeID = setForkIp.get<1>();
    for (auto const& it : idxNodeID)
    {
        mapIpForks[it.nodeIP].emplace_back(it.forkID);
    }

    vector<storage::CSuperNode> nodes;
    for (auto const& it : mapIpForks)
    {
        nodes.emplace_back(storage::CSuperNode(storage::CLIENT_ID_OUT_OF_MQ_CLUSTER, it.first, it.second, 0));
    }

    for (auto const& node : nodes)
    {
        if (!pBlockChain->AddNewSuperNode(node))
        {
            StdError("NetChannel", "CEventPeerBizForks: AddNewSuperNode failed");
            return false;
        }
    }

    //if dpos node self, dispatch them to fork nodes by MQ later
    if (NODE_CAT_DPOSNODE == nNodeCat || NODE_CAT_FORKNODE == nNodeCat)
    {
        CEventMQBizForkUpdate* pEvent = new CEventMQBizForkUpdate(nNonce);
        if (pEvent != nullptr)
        {
            pEvent->data = setForkIp;
            pMQCluster->PostEvent(pEvent);
            StdLog("NetChannel", "CEventPeerBizForks: post biz forks by peer[%s] "
                                 "with total [%d] ips, [%d] forks",
                   GetPeerAddressInfo(nNonce).c_str(), mapIpForks.size(), mapForkIp.size());
        }
        return true;
    }

    // todo: here should send event to add peers to netchannel module if fork nodes

    vector<uint32> vIP;
    for (auto const& it : mapForkIp)
    {
        bool fOwned = false;
        {
            boost::recursive_mutex::scoped_lock scoped_lock(mtxSched);
            fOwned = mapSched.count(it.first);
        }
        if (fOwned)
        {
            vIP.insert(vIP.end(), it.second.begin(), it.second.end());
            StdLog("NetChannel", "CEventPeerBizForks: Add [%d] peer node(s) with biz fork[%s]",
                   it.second.size(), it.first.ToString().c_str());
        }
    }
    for (auto const& nIP : vIP)
    {
        string strIP = storage::CSuperNode::Int2Ip(nIP);
        CNetHost host(strIP, DEFAULT_P2PPORT);
        CEventPeerNetAddNode eventAddNode(0);
        eventAddNode.data = host;
        if (!pPeerNet->DispatchEvent(&eventAddNode))
        {
            StdError("NetChannel", "CEventPeerBizForks: Add peer node[%s] failed", strIP.c_str());
            return false;
        }
        StdLog("NetChannel", "CEventPeerBizForks: Add peer node[%s] succeeded", strIP.c_str());
    }

    return true;
}

bool CNetChannel::HandleEvent(network::CEventPeerSubscribe& eventSubscribe)
{
    uint64 nNonce = eventSubscribe.nNonce;
    uint256& hashFork = eventSubscribe.hashFork;
    StdLog("NetChannel", "CEventPeerSubscribe: peer: %s, fork: %s", GetPeerAddressInfo(nNonce).c_str(), hashFork.GetHex().c_str());
    if (hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
        auto it = mapPeer.find(nNonce);
        if (it != mapPeer.end())
        {
            for (const uint256& hash : eventSubscribe.data)
            {
                if (NODE_CAT_DPOSNODE == nNodeCat)
                {
                    if (hash == pCoreProtocol->GetGenesisBlockHash())
                    {
                        (*it).second.Subscribe(hash);
                        mapUnsync[hash].insert(nNonce);
                    }
                }
                else
                {
                    (*it).second.Subscribe(hash);
                    mapUnsync[hash].insert(nNonce);
                }

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
    StdLog("NetChannel", "CEventPeerUnsubscribe: peer: %s, fork: %s", GetPeerAddressInfo(nNonce).c_str(), hashFork.GetHex().c_str());
    if (hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwNetPeer);
        auto it = mapPeer.find(nNonce);
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
            int nLastBlockHeight = -1;
            for (const network::CInv& inv : eventInv.data)
            {
                if (inv.nType == network::CInv::MSG_TX)
                {
                    vTxHash.push_back(inv.nHash);
                    if (!pTxPool->Exists(inv.nHash) && !pBlockChain->ExistsTx(inv.nHash))
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
                        if (nLastBlockHeight == -1)
                        {
                            uint256 hashLastBlock;
                            int64 nLastTime = 0;
                            if (!pBlockChain->GetLastBlock(hashFork, hashLastBlock, nLastBlockHeight, nLastTime))
                            {
                                StdError("NetChannel", "CEventPeerInv: peer: %s, GetLastBlock fail, fork: %s",
                                         GetPeerAddressInfo(nNonce).c_str(), hashFork.GetHex().c_str());
                                throw runtime_error(string("GetLastBlock fail"));
                            }
                        }
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
            network::CEventPeerBlock eventBlock(nNonce, hashFork);
            if (pBlockChain->GetBlock(inv.nHash, eventBlock.data))
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

    if (NODE_CAT_FORKNODE == nNodeCat)
    {
        if (hashFork == pCoreProtocol->GetGenesisBlockHash())
        {
            StdTrace("NetChannel", "CEventPeerGetBlocks: peer[%s] is asking main chain blocks "
                                   "from a fork node, just ignore it",
                     GetPeerAddressInfo(nNonce).c_str());
            return true;
        }
    }
    if (NODE_CAT_DPOSNODE == nNodeCat)
    {
        if (hashFork != pCoreProtocol->GetGenesisBlockHash())
        {
            StdTrace("NetChannel", "CEventPeerGetBlocks: peer[%s] is asking biz chain blocks "
                                   "from a dpos node, just ignore it",
                     GetPeerAddressInfo(nNonce).c_str());
            return true;
        }
    }

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
        if (pBlockChain->GetLastBlock(hashFork, hashLastBlock, nLastHeight, nLastTime))
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

    if (NODE_CAT_FORKNODE == nNodeCat)
    {
        if (hashFork == pCoreProtocol->GetGenesisBlockHash())
        {
            StdTrace("NetChannel", "CEventPeerTx: peer[%s] is feeding a main chain tx "
                                   "to a fork node, just ignore it",
                     GetPeerAddressInfo(nNonce).c_str());
            return true;
        }
    }
    if (NODE_CAT_DPOSNODE == nNodeCat)
    {
        if (hashFork != pCoreProtocol->GetGenesisBlockHash())
        {
            StdTrace("NetChannel", "CEventPeerTx: peer[%s] is feeding a biz chain tx "
                                   "to a dpos node, just ignore it",
                     GetPeerAddressInfo(nNonce).c_str());
            return true;
        }
    }

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
            sched.RemoveInv(network::CInv(network::CInv::MSG_TX, txid), setSchedPeer);
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

    if (NODE_CAT_FORKNODE == nNodeCat)
    {
        if (hashFork == pCoreProtocol->GetGenesisBlockHash())
        {
            StdTrace("NetChannel", "CEventPeerBlock: peer[%s] is feeding a main chain block "
                                   "to a fork node, just ignore it",
                     GetPeerAddressInfo(nNonce).c_str());
            return true;
        }
    }
    if (NODE_CAT_DPOSNODE == nNodeCat)
    {
        if (hashFork != pCoreProtocol->GetGenesisBlockHash())
        {
            StdTrace("NetChannel", "CEventPeerBlock: peer[%s] is feeding a biz chain block "
                                   "to a dpos node, just ignore it",
                     GetPeerAddressInfo(nNonce).c_str());
            return true;
        }
    }

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
        StdTrace("NetChannel", "CEventPeerBlock: receive block success, peer: %s, height: %d, block hash: %s",
                 GetPeerAddressInfo(nNonce).c_str(), CBlock::GetBlockHeightByHash(hash), hash.GetHex().c_str());

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

            uint256 hashFirst;
            uint256 hashPrev;
            if (CheckPrevBlock(hash, sched, hashFirst, hashPrev))
            {
                if (pBlockChain->GetBlockLocation(hashPrev, hashForkPrev, nHeightPrev))
                {
                    if (hashForkPrev == hashFork)
                    {
                        AddNewBlock(hashFork, hashFirst, sched, setSchedPeer, setMisbehavePeer);
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
        for (const uint256& prev : setMissingPrevTx)
        {
            sched.AddOrphanTxPrev(txid, prev);
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

            if (!sched.IsRepeatBlock(hashBlock))
            {
                if (!pBlockChain->VerifyRepeatBlock(hashFork, *pBlock))
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

            Errno err = pDispatcher->AddNewBlock(*pBlock, nNonceSender);
            if (err == OK)
            {
                StdDebug("NetChannel", "NetChannel AddNewBlock success, peer: %s, height: %d, block: %s",
                         GetPeerAddressInfo(nNonceSender).c_str(), CBlock::GetBlockHeightByHash(hashBlock), hashBlock.GetHex().c_str());

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

} // namespace bigbang
