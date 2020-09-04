// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mqcluster.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iostream>
#include <string>
#include <thread> // For sleep

#include "async_client.h"

const int32 FETCH_TIMEOUT = 250;

using namespace std;

namespace bigbang
{

CMQCluster::CMQCluster(int catNodeIn)
  : thrMqttClient("mqttcli", boost::bind(&CMQCluster::MqttThreadFunc, this)),
    thrPostAddBizNode("postaddbiz", boost::bind(&CMQCluster::NodeThreadFunc, this)),
    pCoreProtocol(nullptr),
    pBlockChain(nullptr),
    pService(nullptr),
    fAbort(false),
    nReqBlkTimerID(0),
    nRollNum(0),
    clientID("")
{
    switch (catNodeIn)
    {
    case 0:
        catNode = NODE_CATEGORY::BBCNODE;
        break;
    case 1:
        catNode = NODE_CATEGORY::FORKNODE;
        break;
    case 2:
        catNode = NODE_CATEGORY::DPOSNODE;
        break;
    }
    std::atomic_init(&isMainChainBlockBest, false);
    std::atomic_init(&fConnected, false);
    array<string, TOPIC_SUFFIX_MAX> v{};
    arrTopic = std::move(v);
}

bool CMQCluster::HandleInitialize()
{
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

    if (!GetObject("dispatcher", pDispatcher))
    {
        Error("Failed to request dispatcher\n");
        return false;
    }

    if (!GetObject("service", pService))
    {
        Error("Failed to request service");
        return false;
    }

    if (!GetObject("forkmanager", pForkManager))
    {
        Error("Failed to request forkmanager");
        return false;
    }

    if (NODE_CATEGORY::BBCNODE == catNode)
    {
        Log("CMQCluster::HandleInitialize(): bbc node so bypass");
        return true;
    }

    addrBroker = dynamic_cast<const CBasicConfig*>(Config())->strMQBrokerURI;
    if (NODE_CATEGORY::FORKNODE == catNode)
    {
        dposNodeCliID = dynamic_cast<const CBasicConfig*>(Config())->strDposNodeID;
        if (dposNodeCliID.empty())
        {
            Error("DPOS NODE CLIENT ID should not be empty for fork node");
            return false;
        }
    }
    prefixTopic = dynamic_cast<const CBasicConfig*>(Config())->strTopicPrefix;

    return true;
}

void CMQCluster::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pDispatcher = nullptr;
    pService = nullptr;
    pForkManager = nullptr;
}

bool CMQCluster::HandleInvoke()
{
    if (NODE_CATEGORY::BBCNODE == catNode)
    {
        if (!ThreadDelayStart(thrPostAddBizNode))
        {
            return false;
        }

        Log("CMQCluster::HandleInvoke(): bbc node so bypass");
        return true;
    }
    if (NODE_CATEGORY::FORKNODE == catNode)
    {
        if (!ThreadDelayStart(thrPostAddBizNode))
        {
            return false;
        }
    }

    uint8 mask = 0;
    if (NODE_CATEGORY::FORKNODE == catNode)
    {
        lastHeightResp = pBlockChain->GetBlockCount(pCoreProtocol->GetGenesisBlockHash()) - 1;
        mask = 2;
    }
    if (NODE_CATEGORY::DPOSNODE == catNode)
    {
        lastHeightResp = -1;
        mask = 4;
    }
    std::vector<storage::CSuperNode> nodes;
    if (!pBlockChain->FetchSuperNode(nodes, mask))
    {
        Error("CMQCluster::HandleInvoke(): fetch super node failed");
        return false;
    }
    if (nodes.size() > 1)
    {
        Error("CMQCluster::HandleInvoke(): there are nodes enrollment info greater than one");
        return false;
    }
    if (nodes.empty())
    {
        Log("CMQCluster::HandleInvoke(): this super node has not enrolled yet");
        if (!ThreadStart(thrMqttClient))
        {
            return false;
        }
        return IIOModule::HandleInvoke();
    }

    {
        boost::unique_lock<boost::mutex> lock(mtxStatus);
        clientID = nodes[0].superNodeID;
        ipAddr = nodes[0].ipAddr;
        for (auto const& fork : nodes[0].vecOwnedForks)
        {
            setBizFork.emplace(fork);
            Log("CMQCluster::HandleInvoke(): super node enrolled fork:[%s]", fork.ToString().c_str());
        }
    }
    Log("CMQCluster::HandleInvoke(): load super node status info successfully \n[%s]",
        nodes[0].ToString().c_str());

    arrTopic = {};
    if (NODE_CATEGORY::FORKNODE == catNode)
    {
        //        pForkManager->SetForkFilter(nodes[0].vecOwnedForks);

        arrTopic[TOPIC_SUFFIX_REQ_BLOCK] = prefixTopic + clientID + vecSuffixTopic[TOPIC_SUFFIX_REQ_BLOCK];
        arrTopic[TOPIC_SUFFIX_RESP_BLOCK] = prefixTopic + clientID + vecSuffixTopic[TOPIC_SUFFIX_RESP_BLOCK];
        arrTopic[TOPIC_SUFFIX_UPDATE_BLOCK] = prefixTopic + dposNodeCliID + vecSuffixTopic[TOPIC_SUFFIX_UPDATE_BLOCK];
        arrTopic[TOPIC_SUFFIX_ASGN_BIZFORK] = prefixTopic + clientID + vecSuffixTopic[TOPIC_SUFFIX_ASGN_BIZFORK];
        Log("CMQCluster::HandleInvoke(): fork node clientid [%s] with sub topics:\n\t[%s]\n\t[%s]\n\t[%s]\npub topic:\n\t[%s]",
            clientID.c_str(), arrTopic[TOPIC_SUFFIX_RESP_BLOCK].c_str(), arrTopic[TOPIC_SUFFIX_UPDATE_BLOCK].c_str(),
            arrTopic[TOPIC_SUFFIX_ASGN_BIZFORK].c_str(), arrTopic[TOPIC_SUFFIX_REQ_BLOCK].c_str());
        /*
        if (!PostBlockRequest(-1))
        {
            Error("CMQCluster::HandleInvoke(): failed to post requesting block");
            return false;
        }*/
    }
    else if (NODE_CATEGORY::DPOSNODE == catNode)
    {
        {
            boost::unique_lock<boost::mutex> lock(mtxStatus);
            if (setBizFork.size() != 1 || *(setBizFork.begin()) != pCoreProtocol->GetGenesisBlockHash())
            {
                Error("CMQCluster::HandleInvoke(): dpos node enrollment info "
                      "invalid [%d] for self",
                      setBizFork.size());
                return false;
            }
        }
        arrTopic[TOPIC_SUFFIX_REQ_BLOCK] = prefixTopic + "+" + vecSuffixTopic[TOPIC_SUFFIX_REQ_BLOCK];
        arrTopic[TOPIC_SUFFIX_RESP_BLOCK] = prefixTopic + "***" + vecSuffixTopic[TOPIC_SUFFIX_RESP_BLOCK]; //only placeholder
        arrTopic[TOPIC_SUFFIX_UPDATE_BLOCK] = prefixTopic + clientID + vecSuffixTopic[TOPIC_SUFFIX_UPDATE_BLOCK];
        arrTopic[TOPIC_SUFFIX_ASGN_BIZFORK] = prefixTopic + "***" + vecSuffixTopic[TOPIC_SUFFIX_ASGN_BIZFORK]; //only placeholder
        Log("CMQCluster::HandleInvoke(): dpos node clientid [%s] with sub topic [%s], pub topics[%s]"
            "[%s][%s]",
            clientID.c_str(), arrTopic[TOPIC_SUFFIX_REQ_BLOCK].c_str(),
            arrTopic[TOPIC_SUFFIX_RESP_BLOCK].c_str(), arrTopic[TOPIC_SUFFIX_UPDATE_BLOCK].c_str(),
            arrTopic[TOPIC_SUFFIX_ASGN_BIZFORK].c_str());

        nodes.clear();
        if (!pBlockChain->FetchSuperNode(nodes, 1 << 1))
        {
            Error("CMQCluster::HandleInvoke(): list all mq fork nodes failed");
            return false;
        }

        {
            boost::unique_lock<boost::mutex> lock(mtxCluster);
            for (auto const& node : nodes)
            {
                mapActiveMQForkNode.emplace(make_pair(node.superNodeID,
                                                      storage::CSuperNode(node.superNodeID, node.ipAddr,
                                                                          node.vecOwnedForks,
                                                                          static_cast<int8>(NODE_CATEGORY::FORKNODE))));
                Log("CMQCluster::HandleInvoke(): load fork node [%s]", node.ToString().c_str());
            }
        }
    }

    nodes.clear();
    if (!pBlockChain->FetchSuperNode(nodes, 1 << 0))
    {
        Error("CMQCluster::HandleInvoke(): list all other outer nodes failed");
        return false;
    }
    {
        boost::unique_lock<boost::mutex> lock(mtxOuter);
        for (auto const& node : nodes)
        {
            mapOuterNode.emplace(make_pair(node.ipAddr,
                                           storage::CSuperNode(node.superNodeID, node.ipAddr,
                                                               node.vecOwnedForks,
                                                               static_cast<int8>(NODE_CATEGORY::BBCNODE))));
            Log("CMQCluster::HandleInvoke(): load outer node [%s]", node.ToString().c_str());
        }
    }

    if (!ThreadStart(thrMqttClient))
    {
        return false;
    }
    return IIOModule::HandleInvoke();
}

void CMQCluster::HandleHalt()
{
    if (NODE_CATEGORY::BBCNODE == catNode)
    {
        Log("CMQCluster::HandleHalt(): bbc node so bypass");
        return;
    }

    IIOModule::HandleHalt();

    fAbort = true;

    {
        boost::unique_lock<boost::mutex> lock(mtxSend);
        deqSendBuff.clear();
    }
    condSend.notify_one();

    if (thrMqttClient.IsRunning())
    {
        thrMqttClient.Interrupt();
    }
    ThreadExit(thrMqttClient);
}

bool CMQCluster::HandleEvent(CEventMQSyncBlock& eventMqSyncBlock)
{
    return true;
}

bool CMQCluster::HandleEvent(CEventMQChainUpdate& eventMqUpdateChain)
{
    Log("CMQCluster::HandleEvent(CEventMQChainUpdate): entering forking event handler");
    if (!fConnected)
    {
        Log("CMQCluster::HandleEvent(CEventMQChainUpdate): connection to mq is lost, ignore forking");
        return true;
    }
    CMqRollbackUpdate& update = eventMqUpdateChain.data;

    if (catNode != NODE_CATEGORY::DPOSNODE)
    {
        Error("CMQCluster::HandleEvent(CEventMQChainUpdate): only dpos node should receive this kind of event");
        return false;
    }

    CRollbackBlock rbc;
    rbc.rbHeight = update.triHeight;
    rbc.rbHash = update.triHash;
    rbc.rbSize = update.actRollBackLen;
    rbc.hashList = update.vShort;

    CBufferPtr spRBC(new CBufStream);
    *spRBC.get() << rbc;

    Log("CMQCluster::HandleEvent(CEventMQChainUpdate): rollback-topic[%s]:forkheight[%d] forkhash[%s] shortlen[%d]",
        arrTopic[TOPIC_SUFFIX_UPDATE_BLOCK].c_str(), rbc.rbHeight, rbc.rbHash.ToString().c_str(), rbc.rbSize);

    AppendSendQueue(arrTopic[TOPIC_SUFFIX_UPDATE_BLOCK], spRBC);

    Log("CMQCluster::HandleEvent(CEventMQChainUpdate): exiting forking event handler");
    return true;
}

bool CMQCluster::HandleEvent(CEventMQEnrollUpdate& eventMqUpdateEnroll)
{
    Log("CMQCluster::HandleEvent(CEventMQEnrollUpdate): starting process of CEventMQEnrollUpdate");
    string id = eventMqUpdateEnroll.data.superNodeClientID;
    vector<uint256> forks = eventMqUpdateEnroll.data.vecForksOwned;

    if (NODE_CATEGORY::FORKNODE == catNode)
    {
        if (storage::CLIENT_ID_OUT_OF_MQ_CLUSTER != id)
        {
            {
                boost::unique_lock<boost::mutex> lock(mtxStatus);
                clientID = id;
                ipAddr = eventMqUpdateEnroll.data.ipAddr;
                for (auto const& fork : forks)
                {
                    setBizFork.emplace(fork);
                }
                arrTopic = {};
                arrTopic[TOPIC_SUFFIX_REQ_BLOCK] = prefixTopic + clientID + vecSuffixTopic[TOPIC_SUFFIX_REQ_BLOCK]; //only placeholder
                arrTopic[TOPIC_SUFFIX_RESP_BLOCK] = prefixTopic + clientID + vecSuffixTopic[TOPIC_SUFFIX_RESP_BLOCK];
                arrTopic[TOPIC_SUFFIX_UPDATE_BLOCK] = prefixTopic + dposNodeCliID + vecSuffixTopic[TOPIC_SUFFIX_UPDATE_BLOCK];
                arrTopic[TOPIC_SUFFIX_ASGN_BIZFORK] = prefixTopic + clientID + vecSuffixTopic[TOPIC_SUFFIX_ASGN_BIZFORK];
            }
            condStatus.notify_all();

            Log("CMQCluster::HandleEvent(CEventMQEnrollUpdate): fork node clientid [%s] with sub topics:[%s];[%s];[%s]\t"
                "pub topic:[%s]",
                clientID.c_str(), arrTopic[TOPIC_SUFFIX_RESP_BLOCK].c_str(),
                arrTopic[TOPIC_SUFFIX_UPDATE_BLOCK].c_str(), arrTopic[TOPIC_SUFFIX_ASGN_BIZFORK].c_str(),
                arrTopic[TOPIC_SUFFIX_REQ_BLOCK].c_str());
            for (const auto& fork : forks)
            {
                Log("CMQCluster::HandleEvent(CEventMQEnrollUpdate): fork [%s] intended to be produced by this node [%s]:",
                    fork.ToString().c_str(), clientID.c_str());
            }

            if (!PostBlockRequest(-1))
            {
                Error("CMQCluster::HandleEvent(CEventMQEnrollUpdate): failed to post requesting block");
                return false;
            }
            // FetchBlock(true, -1);

            return true;
        }
    }
    else if (NODE_CATEGORY::DPOSNODE == catNode)
    {
        if (1 == forks.size() && 0 == eventMqUpdateEnroll.data.ipAddr
            && forks[0] == pCoreProtocol->GetGenesisBlockHash())
        { //dpos node enrolled by self
            {
                boost::unique_lock<boost::mutex> lock(mtxStatus);
                clientID = id;
                ipAddr = 0;
                setBizFork.emplace(forks[0]);
                arrTopic = {};
                arrTopic[TOPIC_SUFFIX_REQ_BLOCK] = prefixTopic + "+" + vecSuffixTopic[TOPIC_SUFFIX_REQ_BLOCK];
                arrTopic[TOPIC_SUFFIX_RESP_BLOCK] = prefixTopic + "***" + vecSuffixTopic[TOPIC_SUFFIX_RESP_BLOCK];        //only placeholder
                arrTopic[TOPIC_SUFFIX_UPDATE_BLOCK] = prefixTopic + clientID + vecSuffixTopic[TOPIC_SUFFIX_UPDATE_BLOCK]; //only placeholder
                arrTopic[TOPIC_SUFFIX_ASGN_BIZFORK] = prefixTopic + "***" + vecSuffixTopic[TOPIC_SUFFIX_ASGN_BIZFORK];    //only placeholder
            }
            condStatus.notify_all();

            Log("CMQCluster::HandleEvent(CEventMQEnrollUpdate): dpos node clientid [%s] with sub topic [%s], pub topics[%s]"
                "[%s][%s]",
                clientID.c_str(), arrTopic[TOPIC_SUFFIX_REQ_BLOCK].c_str(),
                arrTopic[TOPIC_SUFFIX_RESP_BLOCK].c_str(), arrTopic[TOPIC_SUFFIX_UPDATE_BLOCK].c_str(),
                arrTopic[TOPIC_SUFFIX_ASGN_BIZFORK].c_str());

            return true;
        }

        if (storage::CLIENT_ID_OUT_OF_MQ_CLUSTER != id)
        { //fork node enrolled by dpos node
            {
                boost::unique_lock<boost::mutex> lock(mtxCluster);
                mapActiveMQForkNode[id] = storage::CSuperNode(id, eventMqUpdateEnroll.data.ipAddr, forks);
            }
            Log("CMQCluster::HandleEvent(CEventMQEnrollUpdate): dpos node register clientid [%s]",
                eventMqUpdateEnroll.data.superNodeClientID.c_str());

            return true;
        }
    }

    //simulating outer biz fork node used by p2p - todo: for test only
    {
        boost::unique_lock<boost::mutex> lock(mtxOuter);
        mapOuterNode.insert(make_pair(eventMqUpdateEnroll.data.ipAddr,
                                      storage::CSuperNode(id, eventMqUpdateEnroll.data.ipAddr, forks)));
    }
    Log("CMQCluster::HandleEvent(CEventMQEnrollUpdate): super node registered simulating outer biz fork node used by p2p [%s - %s]",
        id.c_str(), storage::CSuperNode::Int2Ip(eventMqUpdateEnroll.data.ipAddr).c_str());

    return true;
}

bool CMQCluster::HandleEvent(CEventMQAgreement& eventMqAgreement)
{
    return true;
}

bool CMQCluster::HandleEvent(CEventMQBizForkUpdate& eventMqBizFork)
{
    Log("CMQCluster::HandleEvent(CEventMQBizForkUpdate): biz forks payload is coming...");

    const storage::CForkKnownIpSetByIp& idxIP = eventMqBizFork.data.get<1>();
    {
        boost::unique_lock<boost::mutex> lock(mtxOuter);
        for (auto const& i : idxIP)
        {
            auto& node = mapOuterNode[i.nodeIP];
            // node.nodeCat = (int8)NODE_CATEGORY::OUTERNODE;
            node.ipAddr = i.nodeIP;
            node.vecOwnedForks.push_back(i.forkID);
            Log("CMQCluster::HandleEvent(CEventMQBizForkUpdate): add IP[%s] biz fork[%s] to mem structure",
                storage::CSuperNode::Int2Ip(i.nodeIP).c_str(), i.forkID.ToString().c_str());
        }
    }

    if (NODE_CATEGORY::FORKNODE == catNode)
    {
        PoolAddBizForkNode();
        Log("CMQCluster::HandleEvent(CEventMQBizForkUpdate): PostAddBizForkNode() done");
    }

    if (NODE_CATEGORY::DPOSNODE == catNode)
    { //spread ip/fork through mq broker
        const storage::CForkKnownIpSetById& idxID = eventMqBizFork.data.get<0>();
        {
            boost::unique_lock<boost::mutex> lock(mtxCluster);
            for (auto const& cli : mapActiveMQForkNode)
            {
                CAssignBizFork biz;
                auto& it = biz.mapIpBizFork;
                for (auto const& fork : cli.second.vecOwnedForks)
                {
                    auto itBegin = idxID.equal_range(fork).first;
                    auto itEnd = idxID.equal_range(fork).second;

                    while (itBegin != itEnd)
                    {
                        it[itBegin->nodeIP].push_back(itBegin->forkID);
                        ++itBegin;
                    }
                }
                if (!it.empty())
                {
                    string topic = prefixTopic + cli.second.superNodeID + vecSuffixTopic[TOPIC_SUFFIX_ASGN_BIZFORK];
                    PostBizForkAssign(topic, biz);
                    mapBizForkUpdateTopic[cli.first] = topic;
                    Log("CMQCluster::HandleEvent(CEventMQBizForkUpdate): PostBizForkAssign to fork node[%s]",
                        cli.second.superNodeID.c_str());
                }
            }
        }
    }

    return true;
}

bool CMQCluster::LogEvent(const string& info)
{
    cout << "callback to CMQCluster when MQ-EVENT" << info << endl;
    Log("CMQCluster::LogMQEvent[%s]", info.c_str());
    return true;
}

bool CMQCluster::GetForkNodeFork(std::vector<uint256>& forks)
{
    forks.clear();
    vector<storage::CSuperNode> nodes;
    if (!pBlockChain->FetchSuperNode(nodes, 1 << 1))
    {
        Error("CMQCluster::GetForkNodeFork(): failed to fetch fork nodes");
        return false;
    }

    if (nodes.empty())
    {
        Log("CMQCluster::GetForkNodeFork(): fork nodes have not been enrolled yet");
        return true;
    }

    if (NODE_CATEGORY::FORKNODE == catNode && nodes.size() > 1)
    {
        Error("CMQCluster::GetForkNodeFork(): fork enrolled by fork node should not be greater than one");
        return false;
    }

    forks = std::move(nodes[0].vecOwnedForks);

    return true;
}

bool CMQCluster::PostBlockRequest(int syncHeight)
{
    Log("CMQCluster::PostBlockRequest(): posting request for block #%d", syncHeight);

    if (clientID.empty())
    {
        Log("CMQCluster::PostBlockRequest(): enrollment is empty for this fork node");
        return true;
    }

    uint256 hash;
    int height;
    if (-1 == syncHeight)
    {
        int64 ts;
        uint16 mint;
        if (!pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hash, height, ts, mint))
        {
            Error("CMQCluster::PostBlockRequest(): failed to get last block");
            return false;
        }
    }
    else
    {
        if (nRollNum)
        {
            boost::unique_lock<boost::mutex> lock(mtxRoll);
            hash = vLongFork.back();
        }
        else
        {
            if (!pBlockChain->GetBlockHash(pCoreProtocol->GetGenesisBlockHash(), syncHeight, hash))
            {
                Error("CMQCluster::PostBlockRequest(): failed to get specific block");
                return false;
            }
        }
        height = syncHeight;
    }
    Log("CMQCluster::PostBlockRequest(): posting request for block id[%d] hash[%s]", height, hash.ToString().c_str());

    CSyncBlockRequest req;
    req.ipAddr = ipAddr;
    req.forkNodeIdLen = clientID.size();
    req.forkNodeId = clientID;
    req.forkNum = setBizFork.size();
    for (auto const& fork : setBizFork)
    {
        req.forkList.emplace_back(fork);
    }
    req.lastHeight = height;
    req.lastHash = hash;
    req.tsRequest = GetTime();
    req.nonce = 1;

    CBufferPtr spSS(new CBufStream);
    *spSS.get() << req;

    AppendSendQueue(arrTopic[TOPIC_SUFFIX_REQ_BLOCK], spSS);
    return true;
}

bool CMQCluster::PostBizForkAssign(const std::string& topic, const CAssignBizFork& assign)
{
    CBufferPtr spSS(new CBufStream);
    *spSS.get() << assign;

    AppendSendQueue(topic, spSS);
    return true;
}

bool CMQCluster::AppendSendQueue(const std::string& topic, CBufferPtr payload)
{
    Log("CMQCluster::AppendSendQueue(): appending msg[%s] to sending queue", topic.c_str());
    {
        boost::unique_lock<boost::mutex> lock(mtxSend);
        if (!deqSendBuff.empty())
        {
            const auto& b = deqSendBuff.back();
            if (topic == b.first)
            {
                Log("CMQCluster::AppendSendQueue(): flow control for msg[%s] to sending queue", topic.c_str());
            }
            else
            {
                deqSendBuff.emplace_back(make_pair(topic, payload));
                Log("CMQCluster::AppendSendQueue(): appended msg[%s] to sending queue", topic.c_str());
            }
        }
        else
        {
            deqSendBuff.emplace_back(make_pair(topic, payload));
            Log("CMQCluster::AppendSendQueue(): appended msg[%s] to sending queue", topic.c_str());
        }
        Log("CMQCluster::AppendSendQueue(): there has/have been left [%d] message(s)", deqSendBuff.size());
    }
    condSend.notify_one();

    return true;
}

void CMQCluster::FetchBlock(bool fObliged, int syncHeight)
{
    if (fObliged && nReqBlkTimerID != 0)
    {
        CancelTimer(nReqBlkTimerID);
        nReqBlkTimerID = 0;
    }
    PostBlockRequest(syncHeight);
    if (nReqBlkTimerID == 0)
    {
        nReqBlkTimerID = SetTimer(FETCH_TIMEOUT, boost::bind(&CMQCluster::RequestBlockTimerFunc, this, _1));
    }
}

void CMQCluster::RequestBlockTimerFunc(uint32 nTimer)
{
    if (nReqBlkTimerID == nTimer)
    {
        if (!PostBlockRequest(-1))
        {
            Error("CMQCluster::RequestBlockTimerFunc(): failed to post request");
        }
        else
        {
            Log("%s: fetching block succeed periodically", __PRETTY_FUNCTION__);
        }
        nReqBlkTimerID = SetTimer(FETCH_TIMEOUT,
                                  boost::bind(&CMQCluster::RequestBlockTimerFunc, this, _1));
    }
}

void CMQCluster::OnReceiveMessage(const std::string& topic, CBufStream& payload)
{
    Log("CMQCluster::OnReceiveMessage(): Received message from [%s]", topic.c_str());
    cout << "\nReceived message from [" << topic << "]..." << endl;
    payload.Dump();

    switch (catNode)
    {
    case NODE_CATEGORY::BBCNODE:
        Error("CMQCluster::OnReceiveMessage(): bbc node should not come here!");
        return;
    case NODE_CATEGORY::FORKNODE:
    {
        Log("CMQCluster::OnReceiveMessage(): current sync height is [%d] on fork node side", int(lastHeightResp));
        if (string::npos != topic.find(vecSuffixTopic[TOPIC_SUFFIX_RESP_BLOCK]) && string::npos != topic.find(prefixTopic))
        { //respond to request block of main chain
            Log("CMQCluster::OnReceiveMessage(): entering of fork node processing RESP_BLOCK");
            //unpack payload
            CSyncBlockResponse resp;
            try
            {
                payload >> resp;
            }
            catch (exception& e)
            {
                StdError(__PRETTY_FUNCTION__, e.what());
                Error("CMQCluster::OnReceiveMessage(): failed to unpack respond msg");
                return;
            }

            if (-1 == resp.height && resp.isBest)
            { //has reached the best height for the first time communication,
                // then set timer to process the following business rather than req/resp model
                FetchBlock(false, -1);
                return;
            }
            /*
            if (-2 == resp.height)
            { //when missing rollback will come here
                if (nReqBlkTimerID != 0)
                {
                    CancelTimer(nReqBlkTimerID);
                    nReqBlkTimerID = 0;
                }
                PostBlockRequest(0); //todo: using checkpoint is better
                return;
            }
*/
            //notify to add new block
            Errno err = pDispatcher->AddNewBlock(resp.block);
            if (err != OK)
            {
                Error("CMQCluster::OnReceiveMessage(): failed to add new block (%d) : %s height[%d]\n",
                      err, ErrorString(err), resp.height);
                if (ERR_ALREADY_HAVE == err)
                {
                    lastHeightResp = resp.height;
                    //check if there are rollbacked blocks
                    if (nRollNum)
                    {
                        boost::unique_lock<boost::mutex> lock(mtxRoll);
                        if (vLongFork.size() < nRollNum)
                        {
                            vLongFork.emplace_back(resp.hash);
                        }
                        else
                        {
                            vLongFork.clear();
                            nRollNum = 0;
                        }
                    }
                    if (!PostBlockRequest(lastHeightResp))
                    {
                        Error("CMQCluster::OnReceiveMessage(): failed to post request on response due to duplication");
                        return;
                    }
                }
                return;
            }
            lastHeightResp = resp.height;

            //check if there are rollbacked blocks
            if (nRollNum)
            {
                boost::unique_lock<boost::mutex> lock(mtxRoll);
                if (vLongFork.size() < nRollNum)
                {
                    vLongFork.emplace_back(resp.hash);
                }
                else
                {
                    vLongFork.clear();
                    nRollNum = 0;
                }
            }

            //iterate to retrieve next one

            if (resp.isBest)
            { //when reach best height, send request by timer
                std::atomic_store(&isMainChainBlockBest, true);
                FetchBlock(false, -1);
            }
            else
            {
                std::atomic_store(&isMainChainBlockBest, false);
                // FetchBlock(false, lastHeightResp);
                PostBlockRequest(lastHeightResp);
            }

            return;
        } // end of dealing with response of requesting block

        if (string::npos != topic.find(vecSuffixTopic[TOPIC_SUFFIX_UPDATE_BLOCK]) && string::npos != topic.find(prefixTopic))
        { //roll back blocks on main chain
            Log("CMQCluster::OnReceiveMessage(): entering of fork node processing UPDATE_BLOCK");
            //unpack payload
            CRollbackBlock rb;
            try
            {
                payload >> rb;
            }
            catch (exception& e)
            {
                StdError(__PRETTY_FUNCTION__, e.what());
                Error("CMQCluster::OnReceiveMessage(): failed to unpack rollback msg");
                return;
            }

            Log("CMQCluster::OnReceiveMessage(): rbheight[%d] from dpos node with size[%d], "
                "lastheight[%d] on this fork node",
                rb.rbHeight, rb.rbSize, int(lastHeightResp));

            if (rb.rbHeight < lastHeightResp)
            {
                //cancel request sync timer
                if (nReqBlkTimerID != 0)
                {
                    CancelTimer(nReqBlkTimerID);
                    nReqBlkTimerID = 0;
                }

                //empty sending buffer
                {
                    boost::unique_lock<boost::mutex> lock(mtxSend);
                    deqSendBuff.clear();
                }
                condSend.notify_one();

                //check hard fork point
                uint256 hash;
                if (!pBlockChain->GetBlockHash(pCoreProtocol->GetGenesisBlockHash(), rb.rbHeight, hash))
                {
                    Error("CMQCluster::OnReceiveMessage(): failed to get hard fork block hash or dismatch"
                          "then re-synchronize block from genesis one");
                    if (!PostBlockRequest(0)) //re-sync from genesis block, todo: prefer checkpoint to this
                    {
                        Error("CMQCluster::OnReceiveMessage(): failed to post request while re-sync");
                    }
                    return;
                }

                if (hash != rb.rbHash)
                {
                    Error("CMQCluster::OnReceiveMessage(): hashes do not match - rbhash[%s], lasthash[%s]",
                          rb.rbHash.ToString().c_str(), hash.ToString().c_str());
                    if (!PostBlockRequest(0)) //re-sync from genesis block, todo: prefer checkpoint to this
                    {
                        Error("CMQCluster::OnReceiveMessage(): failed to post request while re-sync");
                    }
                    return;
                }

                bool fMatch = false;
                Log("CMQCluster::OnReceiveMessage(): rbhash[%s] vs. hash[%s] on this fork node at height[%d]",
                    rb.rbHash.ToString().c_str(), hash.ToString().c_str(), rb.rbHeight);

                //check blocks in rollback
                int nShort = 0;
                for (int i = 0; i < rb.rbSize; ++i)
                {
                    if (!pBlockChain->GetBlockHash(pCoreProtocol->GetGenesisBlockHash(),
                                                   rb.rbHeight + i + 1, hash))
                    {
                        if (i != 0)
                        {
                            Log("CMQCluster::OnReceiveMessage(): exceed to get rollback block hash idx[%d]", i);
                            break;
                        }
                        else
                        {
                            Error("CMQCluster::OnReceiveMessage(): short chain does not match for one on dpos node:1");
                            return;
                        }
                    }
                    Log("CMQCluster::OnReceiveMessage(): fork node blkhsh[%s] vs. dpos node blkhsh[%s] "
                        "at height of [%d]",
                        rb.hashList[i].ToString().c_str(), hash.ToString().c_str(),
                        rb.rbHeight + i + 1);
                    if (hash == rb.hashList[i])
                    {
                        Log("CMQCluster::OnReceiveMessage(): fork node has not been rolled back yet"
                            " with hash [%s]",
                            hash.ToString().c_str());
                        fMatch = true;
                        ++nShort;
                        continue;
                    }
                    else
                    {
                        Error("CMQCluster::OnReceiveMessage(): short chain does not match for one on dpos node:2");
                        if (!PostBlockRequest(0)) //re-sync from genesis block
                        {
                            Error("CMQCluster::OnReceiveMessage(): failed to post request while re-sync");
                        }
                        return;
                    }
                }
                Log("CMQCluster::OnReceiveMessage(): fork node rb[%d] against dpos node rb[%d]", nShort, rb.rbSize);

                //re-request from hard fork point to sync the best chain
                if (fMatch)
                {
                    lastHeightResp = rb.rbHeight;
                    Log("CMQCluster::OnReceiveMessage(): match to prepare rollback: rb.rbHeight[%d] against lastHeightResp[%d]", rb.rbHeight, int(lastHeightResp));
                    if (!PostBlockRequest(lastHeightResp))
                    {
                        Error("CMQCluster::OnReceiveMessage(): failed to post request on rollback");
                        nRollNum = std::min(nShort, rb.rbSize);
                        return;
                    }
                    nRollNum = std::min(nShort, rb.rbSize);
                }

                return;
            }

            FetchBlock(true, -1);

            return;
        } // end of dealing with rollback of main chain

        if (string::npos != topic.find(vecSuffixTopic[TOPIC_SUFFIX_ASGN_BIZFORK]) && string::npos != topic.find(prefixTopic))
        { //receive biz fork list from dpos node by MQ broker
            Log("CMQCluster::OnReceiveMessage(): entering of fork node processing ASGN_BIZFORK");
            //unpack payload
            CAssignBizFork biz;
            try
            {
                payload >> biz;
            }
            catch (exception& e)
            {
                StdError(__PRETTY_FUNCTION__, e.what());
                Error("CMQCluster::OnReceiveMessage(): failed to unpack bizfork msg");
                return;
            }

            vector<storage::CSuperNode> outers;
            {
                boost::unique_lock<boost::mutex> lock(mtxOuter);

                //populate to memory
                for (auto const& it : biz.mapIpBizFork)
                {
                    if (it.first != ipAddr) //filter out one(s) which is/are same as this fork node self - reflected 'mirrored' node ip from dpos node gotten from outer peers through p2p
                    {
                        mapOuterNode[it.first].ipAddr = it.first;
                        mapOuterNode[it.first].nodeCat = 0;
                        mapOuterNode[it.first].vecOwnedForks = it.second;
                        outers.push_back(mapOuterNode[it.first]);
                        Log("CMQCluster::OnReceiveMessage(): adding new outer nodes from mq broker "
                            "by dpos node succeeded [%s]",
                            mapOuterNode[it.first].ToString().c_str());
                        continue;
                    }
                    Log("CMQCluster::OnReceiveMessage(): this ip[%s] is a mirror one by outer peers",
                        storage::CSuperNode::Int2Ip(it.first).c_str());
                }
            }

            //save to db storage
            if (!outers.empty() && !pBlockChain->AddOuterNodes(outers, true))
            {
                Error("CMQCluster::OnReceiveMessage(): failed to add new outer nodes "
                      "from mq broker by dpos node");
                return;
            }
            Log("CMQCluster::OnReceiveMessage(): adding new outer total (%d) nodes from mq broker "
                "by dpos node succeeded",
                outers.size());

            //launch connecting those outer nodes if main chain has been best block
            if (std::atomic_load(&isMainChainBlockBest))
            {
                PoolAddBizForkNode(outers);
            }

            return;
        } // end of dealing with biz fork assignment

        break;
    }
    case NODE_CATEGORY::DPOSNODE:
    {
        //unpack payload
        CSyncBlockRequest req;
        try
        {
            payload >> req;
        }
        catch (exception& e)
        {
            StdError(__PRETTY_FUNCTION__, e.what());
            Error("CMQCluster::OnReceiveMessage(): failed to unpack request msg");
            return;
        }

        //check if requesting fork node has been enrolled
        {
            boost::unique_lock<boost::mutex> lock(mtxCluster);

            auto node = mapActiveMQForkNode.find(req.forkNodeId);
            if (node == mapActiveMQForkNode.end())
            {
                Error("CMQCluster::OnReceiveMessage(): requesting fork node has not enrolled yet");
                return;
            }
            //check if requesting fork node matches the corresponding one enrolled
            if (node->second.vecOwnedForks.size() != req.forkNum)
            {
                Error("CMQCluster::OnReceiveMessage(): requesting fork node number does not match");
                return;
            }
            for (const auto& fork : req.forkList)
            {
                auto pos = find(node->second.vecOwnedForks.begin(), node->second.vecOwnedForks.end(), fork);
                if (pos == node->second.vecOwnedForks.end())
                {
                    Error("CMQCluster::OnReceiveMessage(): requesting fork node detailed forks does not match");
                    return;
                }
            }
        }

        //check height requested
        int best = pBlockChain->GetBlockCount(pCoreProtocol->GetGenesisBlockHash()) - 1;
        CSyncBlockResponse resp;
        if (req.lastHeight > best)
        {
            Error("CMQCluster::OnReceiveMessage(): block height owned by fork node [%s]"
                  "should not be greater than the best one on dpos node",
                  req.forkNodeId.c_str());
            return;
        }
        else if (req.lastHeight == best)
        {
            Log("CMQCluster::OnReceiveMessage(): block height[%d] owned by fork node[%s] "
                "has reached the best one on dpos node, please wait...",
                best, req.forkNodeId.c_str());
            resp.height = -1;
            resp.hash = uint256();
            resp.isBest = 1;
            resp.block = CBlock();
            resp.blockSize = xengine::GetSerializeSize(resp.block);
            return;
        }
        else
        {
            //check height and hash are matched
            uint256 hash;
            if (!pBlockChain->GetBlockHash(pCoreProtocol->GetGenesisBlockHash(), req.lastHeight, hash))
            {
                Error("CMQCluster::OnReceiveMessage(): failed to get checking height and hash match "
                      "at height of [%d] with fork node [%s]",
                      req.lastHeight, req.forkNodeId.c_str());
                return;
            }
            if (hash != req.lastHash)
            { //cause maybe rollback not in time or in process
                Log("CMQCluster::OnReceiveMessage(): height and hash do not match hash[%s] vs. req.lastHash[%s] "
                    "at height of [%d] with fork node [%s]",
                    hash.ToString().c_str(),
                    req.lastHash.ToString().c_str(), req.lastHeight, req.forkNodeId.c_str());
                /*                resp.height = -2;
                resp.hash = uint256();
                resp.isBest = 0;
                resp.block = CBlock();
                resp.blockSize = xengine::GetSerializeSize(resp.block);*/
                return;
            }
            else
            {
                if (!pBlockChain->GetBlockHash(pCoreProtocol->GetGenesisBlockHash(), req.lastHeight + 1, hash))
                {
                    Error("CMQCluster::OnReceiveMessage(): failed to get next block hash at height of [%d] "
                          "with fork node [%s]",
                          req.lastHeight + 1, req.forkNodeId.c_str());
                    return;
                }
                CBlock block;
                if (!pBlockChain->GetBlock(hash, block))
                {
                    Error("CMQCluster::OnReceiveMessage(): failed to get next block for fork node [%s]",
                          req.forkNodeId.c_str());
                    return;
                }

                //reply block requested
                resp.height = req.lastHeight + 1;
                resp.hash = hash;
                resp.isBest = req.lastHeight + 1 < best
                                  ? 0
                                  : 1;
                Log("CMQCluster::OnReceiveMessage(): request[%d] best[%d] isBest[%d]",
                    req.lastHeight + 1, best, resp.isBest);
                resp.blockSize = xengine::GetSerializeSize(block);
                resp.block = move(block);
            }
        }

        CBufferPtr spSS(new CBufStream);
        *spSS.get() << resp;
        string topicRsp = prefixTopic + req.forkNodeId + vecSuffixTopic[TOPIC_SUFFIX_RESP_BLOCK];
        AppendSendQueue(topicRsp, spSS);

        break;
    }
    }
}

class CActionListener : public virtual mqtt::iaction_listener
{
    std::string strName;

    void on_failure(const mqtt::token& tok) override
    {
        std::cout << strName << " failure";
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        std::cout << std::endl;
    }

    void on_success(const mqtt::token& tok) override
    {
        std::cout << strName << " success";
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        auto top = tok.get_topics();
        if (top && !top->empty())
            std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
        std::cout << std::endl;
    }

public:
    CActionListener(const std::string& name)
      : strName(name) {}
};

const int RETRY_ATTEMPTS = 3;
class CMQCallback :
  public virtual mqtt::callback,
  public virtual mqtt::iaction_listener
{
    mqtt::async_client& asynCli;
    mqtt::connect_options& connOpts;
    CMQCluster& mqCluster;
    uint8 nRetry;
    CActionListener subListener;

public:
    CMQCallback(mqtt::async_client& cli, mqtt::connect_options& connOpts, CMQCluster& clusterIn)
      : asynCli(cli), connOpts(connOpts), mqCluster(clusterIn), nRetry(0), subListener("sublistener") {}

    void reconnect()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        try
        {
            asynCli.connect(connOpts, nullptr, *this);
            mqCluster.LogEvent("[reconnect...]");
        }
        catch (const mqtt::exception& exc)
        {
            cerr << "Error: " << exc.what() << std::endl;
            mqCluster.Error("[MQTT_reconnect_ERROR!]");
        }
        mqCluster.LogEvent("[reconnected]");
    }

    void on_failure(const mqtt::token& tok) override
    {
        cout << "\tListener failure for token: [multiple]"
             << tok.get_message_id() << endl;
        mqCluster.LogEvent("[on_failure]");
        if (++nRetry > RETRY_ATTEMPTS)
        {
            mqCluster.Error("[MQTT_retry_to_reconnect_FAILURE!]");
        }
        reconnect();
    }

    void on_success(const mqtt::token& tok) override
    {
        cout << "\tListener success for token: [multiple]"
             << tok.get_message_id() << endl;
        mqCluster.LogEvent("[on_success]");
    }

    void connected(const string& cause) override
    {
        mqCluster.fConnected = true;
        cout << "\nConnection success" << endl;
        if (!cause.empty())
        {
            cout << "\tcause: " << cause << endl;
        }
        mqCluster.LogEvent("[connected]");
        if (CMQCluster::NODE_CATEGORY::FORKNODE == mqCluster.catNode)
        {
            asynCli.subscribe(mqCluster.arrTopic[CMQCluster::TOPIC_SUFFIX_RESP_BLOCK], CMQCluster::QOS1, nullptr, subListener);
            cout << "\nSubscribing to topic '" << mqCluster.arrTopic[CMQCluster::TOPIC_SUFFIX_RESP_BLOCK] << "'\n"
                 << "\tfor client " << mqCluster.clientID
                 << " using QoS" << CMQCluster::QOS1 << endl;

            asynCli.subscribe(mqCluster.arrTopic[CMQCluster::TOPIC_SUFFIX_UPDATE_BLOCK], CMQCluster::QOS1, nullptr, subListener);
            cout << "\nSubscribing to topic '" << mqCluster.arrTopic[CMQCluster::TOPIC_SUFFIX_UPDATE_BLOCK] << "'\n"
                 << "\tfor client " << mqCluster.clientID
                 << " using QoS" << CMQCluster::QOS1 << endl;

            asynCli.subscribe(mqCluster.arrTopic[CMQCluster::TOPIC_SUFFIX_ASGN_BIZFORK], CMQCluster::QOS1, nullptr, subListener);
            cout << "\nSubscribing to topic '" << mqCluster.arrTopic[CMQCluster::TOPIC_SUFFIX_ASGN_BIZFORK] << "'\n"
                 << "\tfor client " << mqCluster.clientID
                 << " using QoS" << CMQCluster::QOS1 << endl;

            mqCluster.FetchBlock(true, -1);
        }
        else if (CMQCluster::NODE_CATEGORY::DPOSNODE == mqCluster.catNode)
        {
            asynCli.subscribe(mqCluster.arrTopic[CMQCluster::TOPIC_SUFFIX_REQ_BLOCK], CMQCluster::QOS1, nullptr, subListener);
            cout << "\nSubscribing to topic '" << mqCluster.arrTopic[CMQCluster::TOPIC_SUFFIX_REQ_BLOCK] << "'\n"
                 << "\tfor client " << mqCluster.clientID
                 << " using QoS" << CMQCluster::QOS1 << endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        cout << endl;
        mqCluster.LogEvent("[subscribed]");
        cout << endl;
    }

    void connection_lost(const string& cause) override
    {
        mqCluster.fConnected = false;
        cout << "\nConnection lost" << endl;
        if (!cause.empty())
        {
            cout << "\tcause: " << cause << endl;
        }
        mqCluster.LogEvent("[connection_lost]");
        nRetry = 0;
        reconnect();
    }

    void message_arrived(mqtt::const_message_ptr msg) override
    {
        cout << "Message arrived" << endl;
        cout << "\ttopic: '" << msg->get_topic() << "'" << endl;
        cout << "\tpayload: '................'\n"
             << endl;
        mqCluster.LogEvent("[message_arrived]");

        xengine::CBufStream ss;
        ss.Write((const char*)&msg->get_payload()[0], msg->get_payload().size());
        mqCluster.OnReceiveMessage(msg->get_topic(), ss);
        /*
        mqCluster.LogEvent("[asyncing...]");
        std::async(std::launch::async,
                   [this, &msg]() {
                       mqCluster.LogEvent("[entered, handling recv...]");
                       xengine::CBufStream ss;
                       ss.Write((const char*)&msg->get_payload()[0], msg->get_payload().size());
                       mqCluster.OnReceiveMessage(msg->get_topic(), ss);
                       mqCluster.LogEvent("[handled recv]");
                   });
        mqCluster.LogEvent("[asynced]");    */
    }

    void delivery_complete(mqtt::delivery_token_ptr tok) override
    {
        cout << "\tDelivery complete for token: ["
             << (tok ? tok->get_message_id() : -1) << "]" << endl;
        mqCluster.LogEvent("[delivery_complete]");
    }
};

int CMQCluster::ClientAgent(MQ_CLI_ACTION action)
{
    try
    {
        static mqtt::async_client client(addrBroker, clientID);

        static mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(20);
        connOpts.set_clean_session(true);

        static mqtt::token_ptr conntok;
        static mqtt::delivery_token_ptr delitok;

        static CMQCallback cb(client, connOpts, *this);

        switch (action)
        {
        case MQ_CLI_ACTION::CONN:
        {
            Log("CMQCluster::ClientAgent(): Initializing for connecting to broker[%s]...", addrBroker.c_str());
            client.set_callback(cb);
            Log("CMQCluster::ClientAgent(): ...set callback OK");
            /*            client.set_message_callback([this](mqtt::const_message_ptr msg) {
                Log("CMQCluster::ClientAgent(): entering lambda");
                std::cout << msg->get_payload_str() << std::endl;
                xengine::CBufStream ss;
                ss.Write((const char*)&msg->get_payload()[0], msg->get_payload().size());
                OnReceiveMessage(msg->get_topic(), ss);
            });
            cout << "  ...OK" << endl;*/

            Log("CMQCluster::ClientAgent(): Connecting...");
            conntok = client.connect();
            Log("CMQCluster::ClientAgent(): Waiting for the connection...");
            conntok->wait();
            Log("CMQCluster::ClientAgent(): ...Connected OK");
            break;
        }
        case MQ_CLI_ACTION::SUB:
        {
            break;
        }
        case MQ_CLI_ACTION::PUB:
        {
            if (!client.is_connected())
            {
                fConnected = false;
                Error("CMQCluster::ClientAgent(): there is no connection to broker yet");
                return -1;
            }

            pair<string, CBufferPtr> buf;

            {
                boost::unique_lock<boost::mutex> lock(mtxSend);
                if (deqSendBuff.empty())
                {
                    Log("CMQCluster::ClientAgent(): there is no message to send");
                    return 0;
                }
                buf = deqSendBuff.front();
                Log("CMQCluster::ClientAgent(): there is/are [%d] message(s) waiting to send", deqSendBuff.size());
            }

            //            while (!deqSendBuff.empty())
            {
                //                pair<string, CBufferPtr> buf = deqSendBuff.front();
                cout << "\nSending message to [" << buf.first << "]..." << endl;
                //                buf.second->Dump();

                mqtt::message_ptr pubmsg = mqtt::make_message(buf.first, buf.second->GetData(), buf.second->GetSize());
                pubmsg->set_qos(QOS1);
                //                pubmsg->set_qos(QOS0);
                if (string::npos != buf.first.find("AssignBizFork"))
                {
                    Log("CMQCluster::ClientAgent(): AssignBizFork so set retained true[%s]", buf.first.c_str());
                    pubmsg->set_retained(true);
                }
                else
                {
                    Log("CMQCluster::ClientAgent(): non-AssignBizFork so set retained false[%s]", buf.first.c_str());
                    pubmsg->set_retained(mqtt::message::DFLT_RETAINED);
                }
                //                delitok = client.publish(pubmsg, nullptr, cb);
                delitok = client.publish(pubmsg);
                delitok->wait();
                cout << "\nSent message to [" << buf.first << "]..." << endl;
                buf.second->Dump();
                Log("CMQCluster::ClientAgent(): published to [%s] successfully", buf.first.c_str());
                //                client.publish(pubmsg);
                /*                if (delitok->wait_for(100))
                {
                    Log("CMQCluster::ClientAgent(): delivery token waiting success[%s]", buf.first.c_str());
                }
                else
                {
                    Error("CMQCluster::ClientAgent(): delivery token waiting fail[%s]", buf.first.c_str());
                }*/

                //                deqSendBuff.pop_front();
            }

            {
                boost::unique_lock<boost::mutex> lock(mtxSend);
                if (!deqSendBuff.empty())
                {
                    Log("CMQCluster::ClientAgent(): pop front of sending deque");
                    deqSendBuff.pop_front();
                    Log("CMQCluster::ClientAgent(): there has/have been left [%d] message(s)", deqSendBuff.size());
                    return 0;
                }
                else
                {
                    Log("CMQCluster::ClientAgent(): error: it should not be empty here normally, maybe rollback thread clean up sending queue");
                    return 0;
                }
            }

            break;
        }
        case MQ_CLI_ACTION::DISCONN:
        {
            //cleanup retained message of biz fork assignment
            if (NODE_CATEGORY::DPOSNODE == catNode)
            {
                boost::unique_lock<boost::mutex> lock(mtxCluster);
                for (auto const& sub : mapBizForkUpdateTopic)
                {
                    mqtt::message_ptr pubmsg = mqtt::make_message(sub.second, nullptr, 0);
                    pubmsg->set_retained(true);
                    delitok = client.publish(pubmsg);
                    delitok->wait();
                    Log("CMQCluster::ClientAgent(): Cleaned up biz fork assignment [%s]", sub.second.c_str());
                }
            }

            // Double check that there are no pending tokens
            auto toks = client.get_pending_delivery_tokens();
            if (!toks.empty())
            {
                Error("CMQCluster::ClientAgent(): There are pending delivery tokens!");
            }

            // Disconnect
            Log("CMQCluster::ClientAgent(): Disconnecting from broker...");
            conntok = client.disconnect();
            conntok->wait();
            Log("CMQCluster::ClientAgent(): ...Disconnected OK");
            break;
        }
        }
    }
    catch (const mqtt::exception& exc)
    {
        cerr << exc.what() << endl;
        return -2;
    }

    return 0;
}

void CMQCluster::MqttThreadFunc()
{
    Log("entering thread function of MQTT");

    //wait for super node itself status available
    if (!fAbort)
    {
        boost::unique_lock<boost::mutex> lock(mtxStatus);
        while (clientID.empty())
        {
            Log("there is no enrollment info, waiting for it coming...");
            condStatus.wait(lock);
        }
    }

    if (NODE_CATEGORY::FORKNODE == catNode)
    {
        PoolAddBizForkNode();
    }

    //establish connection
    ClientAgent(MQ_CLI_ACTION::CONN);

    //subscribe topics
    ClientAgent(MQ_CLI_ACTION::SUB);

    if (NODE_CATEGORY::FORKNODE == catNode)
    { //wait for subscribing done before sending request for main chain block
        //        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    //publish topics
    while (!fAbort)
    {
        {
            boost::unique_lock<boost::mutex> lock(mtxSend);
            while (deqSendBuff.empty() && !fAbort)
            {
                condSend.wait(lock);
            }
        }

        if (fAbort)
        {
            break;
        }

        int ret = ClientAgent(MQ_CLI_ACTION::PUB);
        if (0 == ret)
        {
            Log("thread function of MQTT: go through an iteration of publishing");
        }
        else if (-1 == ret)
        {
            Log("thread function of MQTT: waiting for reconnecting...");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        else
        {
            Error("thread function of MQTT: publish operation is ignormal, leaving MQTT thread with errno[%d]", ret);
            return;
        }
    }

    //disconnect to broker
    ClientAgent(MQ_CLI_ACTION::DISCONN);

    Log("exiting thread function of MQTT");
}

bool CMQCluster::PoolAddBizForkNode(const std::vector<storage::CSuperNode>& outers)
{
    vector<uint32> ips;
    if (outers.empty())
    {
        boost::unique_lock<boost::mutex> lock(mtxOuter);
        for (auto const& node : mapOuterNode)
        {
            ips.push_back(node.first);
        }
    }
    else
    {
        for (auto const& node : outers)
        {
            ips.push_back(node.ipAddr);
        }
    }
    if (!ips.empty())
    {
        if (!pService->AddBizForkNodes(ips))
        {
            Error("CMQCluster::PoolAddBizForkNode(): failed to post add new node msg");
            return false;
        }
        Log("CMQCluster::PoolAddBizForkNode(): posting add new node msg succeeded");
        return true;
    }
    Log("CMQCluster::PoolAddBizForkNode(): there is no outer node for pooling");

    return true;
}

bool CMQCluster::PostAddNode()
{
    vector<storage::CSuperNode> nodes;
    if (NODE_CATEGORY::DPOSNODE != catNode && !pBlockChain->FetchSuperNode(nodes, 1 << 0))
    {
        Error("CMQCluster::PostAddNode(): bbc/fork node failed to fetch outer nodes");
        return false;
    }
    if (nodes.empty())
    {
        Log("CMQCluster::PostAddNode(): bbc/fork node has not had outer nodes yet");
        return true;
    }

    vector<uint32> ips;
    for (const auto& node : nodes)
    {
        ips.push_back(node.ipAddr);
    }
    if (!ips.empty() && !pService->AddBizForkNodes(ips))
    {
        Error("CMQCluster::PostAddNode(): bbc/fork node failed to add outer node for supernode");
        return false;
    }
    return true;
}

void CMQCluster::NodeThreadFunc()
{
    if (PostAddNode())
    {
        Log("CMQCluster::NodeThreadFunc(): bbc/fork node has posted add p2p connection");
    }
    else
    {
        Error("CMQCluster::NodeThreadFunc(): bbc/fork node failed to add p2p connection");
    }
}

} // namespace bigbang
