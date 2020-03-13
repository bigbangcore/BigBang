// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mqcluster.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread> // For sleep

#include "async_client.h"

using namespace std;

namespace bigbang
{

CMQCluster::CMQCluster(int catNodeIn)
  : thrMqttClient("mqttcli", boost::bind(&CMQCluster::MqttThreadFunc, this)),
    pCoreProtocol(nullptr),
    pBlockChain(nullptr),
    pService(nullptr),
    fAuth(false),
    fAbort(false),
    srvAddr("tcp://localhost:1883")
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
}

bool CMQCluster::IsAuthenticated()
{
    return fAuth;
}

bool CMQCluster::HandleInitialize()
{
    if (NODE_CATEGORY::FORKNODE == catNode)
    {
        clientID = "FORKNODE-01";
    }
    else if (NODE_CATEGORY::DPOSNODE == catNode)
    {
        clientID = "DPOSNODE";
    }
    else if (NODE_CATEGORY::BBCNODE == catNode)
    {
        Log("CMQCluster::HandleInitialize(): bbc node so go passby");
        return true;
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

    Log("CMQCluster::HandleInitialize() successfully");
    return true;
}

void CMQCluster::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pDispatcher = nullptr;
    pService = nullptr;
}

bool CMQCluster::HandleInvoke()
{
    if (NODE_CATEGORY::BBCNODE == catNode)
    {
        Log("CMQCluster::HandleInvoke(): bbc node so go passby");
        return true;
    }

    std::vector<storage::CForkNode> nodes;
    if (!pBlockChain->ListForkNode(nodes))
    {
        Log("CMQCluster::HandleInvoke(): list fork node failed");
        return false;
    }
    for (const auto& node : nodes)
    {
        mapForkNode.insert(make_pair(node.forkNodeID, node.vecOwnedForks));
        cout << "fork node of MQ: " << node.forkNodeID << endl;
        for (const auto& fork : node.vecOwnedForks)
        {
            cout << "the fork producing subsidiary: " << fork.ToString() << endl;
            Log("CMQCluster::HandleInvoke(): list fork node [%s] fork [%s]",
                node.forkNodeID.c_str(), fork.ToString().c_str());
        }
    }

    if (NODE_CATEGORY::FORKNODE == catNode)
    {
        topicReqBlk = "Cluster01/" + clientID + "/SyncBlockReq";
        if (mapForkNode.size() != 1 || !PostBlockRequest())
        {
            return false;
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
        Log("CMQCluster::HandleHalt(): bbc node so go passby");
        return;
    }

    IIOModule::HandleHalt();

    fAbort = true;

    condMQ.notify_all();
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

bool CMQCluster::HandleEvent(CEventMQUpdateBlock& eventMqUpdateBlock)
{
    return true;
}

bool CMQCluster::HandleEvent(CEventMQAgreement& eventMqAgreement)
{
    return true;
}

bool CMQCluster::LogEvent(const string& info)
{
    cout << "callback to CMQCluster when MQ-EVENT" << info << endl;
    Log("CMQCluster::LogMQEvent[%s]", info.c_str());
    return true;
}

bool CMQCluster::PostBlockRequest()
{
    uint256 hash;
    int height;
    int64 ts;
    if (!pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hash, height, ts))
    {
        return false;
    }

    CSyncBlockRequest req;
    //    req.ipAddr = 16777343; //127.0.0.1
    req.ipAddr = 1111638320;
    req.forkNodeIdLen = clientID.size();
    req.forkNodeId = clientID;
    auto enroll = mapForkNode.begin();
    req.forkNum = (*enroll).second.size();
    req.forkList = (*enroll).second;
    req.lastHeight = height;
    req.lastHash = hash;
    req.tsRequest = ts;
    req.nonce = 1;

    CBufferPtr spSS(new CBufStream);
    *spSS.get() << req;

    AppendSendQueue(topicReqBlk, spSS);
    return true;
}

bool CMQCluster::AppendSendQueue(const std::string& topic,
                                 CBufferPtr payload)
{
    {
        boost::unique_lock<boost::mutex> lock(mtxSend);
        deqSendBuff.emplace_back(make_pair(topic, payload));
    }
    condSend.notify_all();

    return true;
}

void CMQCluster::OnReceiveMessage(const std::string& topic, CBufStream& payload)
{
    payload.Dump();

    switch (catNode)
    {
    case NODE_CATEGORY::BBCNODE:
        Error("CMQCluster::OnReceiveMessage(): bbc node should not come here!");
        return;
    case NODE_CATEGORY::FORKNODE:
    {
        if (topicRbBlk != topic)
        {//respond to request block of main chain
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

            //check if this msg is just for me
            //if (topicReqBlk != clientID)

            //validate this coming block
            if (!pCoreProtocol->ValidateBlock(resp.block))
            {
                Error("CMQCluster::OnReceiveMessage(): failed to validate block");
                return;
            }

            //notify to add new block
            Errno err = pDispatcher->AddNewBlock(resp.block);
            if (err != OK)
            {
                Error("CMQCluster::OnReceiveMessage(): failed to validate block (%d) : %s\n", err, ErrorString(err));
                return;
            }
            lastHeightResp = resp.height;

            //iterate to retrieve next one
            if (!PostBlockRequest())
            {
                Error("CMQCluster::OnReceiveMessage(): failed to post request");
                return;
            }
        }
        else
        {//roll back blocks on main chain
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

        }
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
        auto node = mapForkNode.find(req.forkNodeId);
        if (node == mapForkNode.end())
        {
            Error("CMQCluster::OnReceiveMessage(): requesting fork node has not enrolled yet");
            return;
        }
        //check if requesting fork node matches the corresponding one enrolled
        if (node->second.size() != req.forkNum)
        {
            Error("CMQCluster::OnReceiveMessage(): requesting fork node number does not match");
            return;
        }
        for (const auto& fork : req.forkList)
        {
            auto pos = find(node->second.begin(), node->second.end(), fork);
            if (pos == node->second.end())
            {
                Error("CMQCluster::OnReceiveMessage(): requesting fork node detailed forks does not match");
                return;
            }
        }

        //add this requesting fork node to active list
        mapActiveForkNode[req.ipAddr] = storage::CForkNode(req.forkNodeId, req.forkList);

        //processing request from fork node
        //check height and hash are matched
        uint256 hash;
        if (!pBlockChain->GetBlockHash(pCoreProtocol->GetGenesisBlockHash(), req.lastHeight, hash))
        {
            Error("CMQCluster::OnReceiveMessage(): failed to get checking height and hash match");
            return;
        }
        if (hash != req.lastHash)
        {
            Error("CMQCluster::OnReceiveMessage(): height and hash do not match");
            return;
        }
        if (!pBlockChain->GetBlockHash(pCoreProtocol->GetGenesisBlockHash(), req.lastHeight + 1, hash))
        {
            Error("CMQCluster::OnReceiveMessage(): failed to get next block hash");
            return;
        }
        CBlock block;
        if (!pBlockChain->GetBlock(hash, block))
        {
            Error("CMQCluster::OnReceiveMessage(): failed to get next block");
            return;
        }

        //reply block requested
        CSyncBlockResponse resp;
        resp.height = req.lastHeight + 1;
        resp.hash = hash;
        resp.isBest = resp.height < pBlockChain->GetBlockCount(pCoreProtocol->GetGenesisBlockHash())
                          ? 0
                          : 1;
        resp.blockSize = xengine::GetSerializeSize(block);
        resp.block = move(block);

        CBufferPtr spSS(new CBufStream);
        *spSS.get() << resp;
        string topicRsp = "Cluster01/" + req.forkNodeId + "/SyncBlockResp";
        {
            boost::unique_lock<boost::mutex> lock(mtxSend);
            deqSendBuff.emplace_back(make_pair(topicRsp, spSS));
        }
        condSend.notify_all();

        break;
    }
    }
}

class action_listener : public virtual mqtt::iaction_listener
{
    std::string name_;

    void on_failure(const mqtt::token& tok) override
    {
        std::cout << name_ << " failure";
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        std::cout << std::endl;
    }

    void on_success(const mqtt::token& tok) override
    {
        std::cout << name_ << " success";
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        auto top = tok.get_topics();
        if (top && !top->empty())
            std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
        std::cout << std::endl;
    }

public:
    action_listener(const std::string& name)
      : name_(name) {}
};

const int RETRY_ATTEMPTS = 3;
class callback :
  public virtual mqtt::callback,
  public virtual mqtt::iaction_listener
{
    mqtt::async_client& cli_;
    mqtt::connect_options& connOpts_;
    CMQCluster& cluster_;
    uint8 retry_;
    action_listener subListener_;

public:
    callback(mqtt::async_client& cli, mqtt::connect_options& connOpts, CMQCluster& clusterIn)
      : cli_(cli), connOpts_(connOpts), cluster_(clusterIn), retry_(0), subListener_("sublistener") {}

    void reconnect()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        try
        {
            cli_.connect(connOpts_, nullptr, *this);
            cluster_.LogEvent("[reconnect...]");
        }
        catch (const mqtt::exception& exc)
        {
            cerr << "Error: " << exc.what() << std::endl;
            cluster_.LogEvent("[on_reconnect_ERROR!]");
            exit(1);
        }
        cluster_.LogEvent("[reconnected]");
    }

    void on_failure(const mqtt::token& tok) override
    {
        cout << "\tListener failure for token: [multiple]"
             << tok.get_message_id() << endl;
        cluster_.LogEvent("[on_failure]");
        if (++retry_ > RETRY_ATTEMPTS)
        {
            cluster_.LogEvent("[on_retry_FAILURE!]");
            exit(1);
        }
        reconnect();
    }

    void on_success(const mqtt::token& tok) override
    {
        cout << "\tListener success for token: [multiple]"
             << tok.get_message_id() << endl;
        cluster_.LogEvent("[on_success]");
    }

    void connected(const string& cause) override
    {
        cout << "\nConnection success" << endl;
        if (!cause.empty())
        {
            cout << "\tcause: " << cause << endl;
        }
        cluster_.LogEvent("[connected]");
        if (CMQCluster::NODE_CATEGORY::FORKNODE == cluster_.catNode)
        {
            cluster_.topicRespBlk = "Cluster01/" + cluster_.clientID + "/SyncBlockResp";
            cli_.subscribe(cluster_.topicRespBlk, CMQCluster::QOS1, nullptr, subListener_);
            cout << "\nSubscribing to topic '" << cluster_.topicRespBlk << "'\n"
                 << "\tfor client " << cluster_.clientID
                 << " using QoS" << CMQCluster::QOS1 << endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            cluster_.topicRbBlk = "Cluster01/DPOSNODE/UpdateBlock";
            cli_.subscribe(cluster_.topicRbBlk, CMQCluster::QOS1, nullptr, subListener_);
            cout << "\nSubscribing to topic '" << cluster_.topicRbBlk << "'\n"
                 << "\tfor client " << cluster_.clientID
                 << " using QoS" << CMQCluster::QOS1 << endl;
        }
        else if (CMQCluster::NODE_CATEGORY::DPOSNODE == cluster_.catNode)
        {
            cluster_.topicReqBlk = "Cluster01/+/SyncBlockReq";
            cli_.subscribe(cluster_.topicReqBlk, CMQCluster::QOS1, nullptr, subListener_);
            cout << "\nSubscribing to topic '" << cluster_.topicReqBlk << "'\n"
                 << "\tfor client " << cluster_.clientID
                 << " using QoS" << CMQCluster::QOS1 << endl;
        }
        cout << endl;
        cluster_.LogEvent("[subscribed]");
        cout << endl;
    }

    void connection_lost(const string& cause) override
    {
        cout << "\nConnection lost" << endl;
        if (!cause.empty())
        {
            cout << "\tcause: " << cause << endl;
        }
        cluster_.LogEvent("[connection_lost]");
        retry_ = 0;
        reconnect();
    }

    void message_arrived(mqtt::const_message_ptr msg) override
    {
        cout << "Message arrived" << endl;
        cout << "\ttopic: '" << msg->get_topic() << "'" << endl;
        cout << "\tpayload: '" << msg->to_string() << "'\n"
             << endl;
        cluster_.LogEvent("[message_arrived]");
        xengine::CBufStream ss;
        ss.Write((const char*)&msg->get_payload()[0], msg->get_payload().size());
        cluster_.OnReceiveMessage(msg->get_topic(), ss);
    }

    void delivery_complete(mqtt::delivery_token_ptr tok) override
    {
        cout << "\tDelivery complete for token: "
             << (tok ? tok->get_message_id() : -1) << endl;
        cluster_.LogEvent("[delivery_complete]");
    }
};

bool CMQCluster::ClientAgent(MQ_CLI_ACTION action)
{
    try
    {
        static mqtt::async_client client(srvAddr, clientID);

        static mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(20);
        connOpts.set_clean_session(true);

        static mqtt::token_ptr conntok;
        static mqtt::delivery_token_ptr delitok;

        static callback cb(client, connOpts, *this);

        switch (action)
        {
        case MQ_CLI_ACTION::CONN:
        {
            cout << "Initializing for server '" << srvAddr << "'..." << endl;
            client.set_callback(cb);
            cout << "  ...OK" << endl;

            cout << "\nConnecting..." << endl;
            conntok = client.connect();
            cout << "Waiting for the connection..." << endl;
            conntok->wait();
            cout << "  ...OK" << endl;
            break;
        }
        case MQ_CLI_ACTION::SUB:
        {
            break;
        }
        case MQ_CLI_ACTION::PUB:
        {
            while (!deqSendBuff.empty())
            {
                pair<string, CBufferPtr> buf = deqSendBuff.front();
                cout << "\nSending message to [" << buf.first << "]..." << endl;
                buf.second->Dump();

                mqtt::message_ptr pubmsg = mqtt::make_message(
                    buf.first, buf.second->GetData(), buf.second->GetSize());
                pubmsg->set_qos(QOS1);
                pubmsg->set_retained(mqtt::message::DFLT_RETAINED);
                delitok = client.publish(pubmsg, nullptr, cb);
                delitok->wait_for(100);
                cout << "_._._OK" << endl;

                deqSendBuff.pop_front();
            }
            break;
        }
        case MQ_CLI_ACTION::DISCONN:
        {
            // Double check that there are no pending tokens

            auto toks = client.get_pending_delivery_tokens();
            if (!toks.empty())
                cout << "Error: There are pending delivery tokens!" << endl;

            // Disconnect
            cout << "\nDisconnecting..." << endl;
            conntok = client.disconnect();
            conntok->wait();
            cout << "  ...OK" << endl;
            break;
        }
        }
    }
    catch (const mqtt::exception& exc)
    {
        cerr << exc.what() << endl;
        return false;
    }

    return true;
}

void CMQCluster::MqttThreadFunc()
{
    Log("entering thread function of MQTT");
    //establish connection
    ClientAgent(MQ_CLI_ACTION::CONN);

    //subscribe topics
    ClientAgent(MQ_CLI_ACTION::SUB);

    //publish topics
    while (!fAbort)
    {
        {
            boost::unique_lock<boost::mutex> lock(mtxSend);
            while (!fAbort)
            {
                ClientAgent(MQ_CLI_ACTION::PUB);
                Log("thread function of MQTT: go through an iteration");
                condSend.wait(lock);
            }
        }
    }

    //disconnect to broker
    ClientAgent(MQ_CLI_ACTION::DISCONN);

    Log("exiting thread function of MQTT");
}

} // namespace bigbang
