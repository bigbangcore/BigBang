// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_CMQCLUSTER_H
#define BIGBANG_CMQCLUSTER_H

#include "base.h"
#include "mqdb.h"
#include "mqevent.h"
#include "xengine.h"

namespace bigbang
{

class CSyncBlockRequest
{
    friend xengine::CStream;

public:
    uint32 ipAddr;
    uint8 forkNodeIdLen;
    std::string forkNodeId;
    uint8 forkNum;
    std::vector<uint256> forkList;
    int32 lastHeight;
    uint256 lastHash;
    uint32 tsRequest;
    int16 nonce;

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(ipAddr, opt);
        s.Serialize(forkNodeIdLen, opt);
        s.Serialize(forkNodeId, opt);
        s.Serialize(forkNum, opt);
        s.Serialize(forkList, opt);
        s.Serialize(lastHeight, opt);
        s.Serialize(lastHash, opt);
        s.Serialize(tsRequest, opt);
        s.Serialize(nonce, opt);
    }
};

class CSyncBlockResponse
{
    friend xengine::CStream;

public:
    int32 height;
    uint256 hash;
    uint8 isBest;
    int32 blockSize;
    CBlock block;

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(height, opt);
        s.Serialize(hash, opt);
        s.Serialize(isBest, opt);
        s.Serialize(blockSize, opt);
        s.Serialize(block, opt);
    }
};

class CRollbackBlock
{
    friend xengine::CStream;

public:
    int32 rbHeight;
    uint256 rbHash;
    int rbSize;
    std::vector<uint256> hashList;

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(rbHeight, opt);
        s.Serialize(rbHash, opt);
        s.Serialize(rbSize, opt);
        s.Serialize(hashList, opt);
    }
};

class CAssignBizFork
{
    friend xengine::CStream;

public:
    std::map<uint32, std::vector<uint256>> mapIpBizFork;

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(mapIpBizFork, opt);
    }
};

class IMQCluster : public xengine::IIOModule, virtual public CMQEventListener
{
public:
    IMQCluster()
      : IIOModule("mqcluster") {}
    virtual bool GetForkNodeFork(std::vector<uint256> forks) = 0;
};

class CMQCluster : public IMQCluster
{
    friend class CMQCallback;

    typedef std::shared_ptr<CBufStream> CBufferPtr;

    enum class NODE_CATEGORY : char
    {
        BBCNODE = 0,
        FORKNODE,
        DPOSNODE
    };
    enum class MQ_CLI_ACTION : char
    {
        CONN = 0,
        SUB,
        PUB,
        DISCONN
    };
    enum
    {
        QOS0 = 0,
        QOS1,
        QOS2
    };

public:
    CMQCluster(int catNodeIn);
    ~CMQCluster() = default;

    bool LogEvent(const std::string& info);
    bool GetForkNodeFork(std::vector<uint256> forks) override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;

    bool HandleEvent(CEventMQSyncBlock& eventMqSyncBlock) override;
    bool HandleEvent(CEventMQChainUpdate& eventMqUpdateChain) override;
    bool HandleEvent(CEventMQEnrollUpdate& eventMqUpdateEnroll) override;
    bool HandleEvent(CEventMQAgreement& eventMqAgreement) override;
    bool HandleEvent(CEventMQBizForkUpdate& eventMqBizFork) override;

protected:
    mutable boost::shared_mutex rwAccess;
    ICoreProtocol* pCoreProtocol;
    IBlockChain* pBlockChain;
    IDispatcher* pDispatcher;
    IService* pService;
    IForkManager* pForkManager;

    boost::mutex mutex;
    boost::condition_variable condMQ;
    xengine::CThread thrMqttClient;

private:
    bool PostBlockRequest(int syncHeight = -1);
    bool PostBizForkAssign(const std::string& topic, CAssignBizFork assign);
    bool AppendSendQueue(const std::string& topic, CBufferPtr payload);
    void RequestBlockTimerFunc(uint32 nTimer);
    void OnReceiveMessage(const std::string& topic, CBufStream& payload);
    bool ClientAgent(MQ_CLI_ACTION action);
    void MqttThreadFunc();
    bool PostAddBizForkNode();

    bool fAbort;
    std::string addrBroker;
    std::string dposNodeCliID;
    const std::string prefixTopic = "Cluster01/";
    enum
    {
        TOPIC_SUFFIX_REQ_BLOCK = 0,
        TOPIC_SUFFIX_RESP_BLOCK = 1,
        TOPIC_SUFFIX_UPDATE_BLOCK = 2,
        TOPIC_SUFFIX_ASGN_BIZFORK = 3,
        TOPIC_SUFFIX_MAX
    };
    const std::vector<std::string> vecSuffixTopic = {
        "/SyncBlockReq",
        "/SyncBlockResp",
        "/UpdateBlock",
        "/AssignBizFork"
    };

    NODE_CATEGORY catNode;

    std::vector<std::string> vecTopic;  //shared by both dpos and fork node
    std::map<std::string, std::string> mapBizForkTopic;        //only for dpos node

    boost::mutex mtxStatus;
    boost::condition_variable condStatus;
    std::string clientID;
    uint32 ipAddr;
    std::set<uint256> setBizFork;
    std::atomic_bool isMainChainBlockBest;

    std::map<string, storage::CSuperNode> mapActiveMQForkNode; //only for dpos node
    std::map<uint32, storage::CSuperNode> mapOuterNode;        //for dpos/fork node
    std::map<std::string, std::string> mapBizForkUpdateTopic;

    boost::mutex mtxSend;
    boost::condition_variable condSend;
    std::deque<std::pair<std::string, CBufferPtr>> deqSendBuff; //topic vs. payload
    boost::shared_mutex rwRecv;
    boost::condition_variable condRecv;
    std::deque<std::pair<std::string, xengine::CBufStream>> deqRecvBuff; //topic vs. payload

    std::atomic<int> lastHeightResp;
    std::atomic<uint32> nReqBlkTimerID;

    boost::mutex mtxRoll;
    std::vector<uint256> vLongFork;
    std::atomic<int> nRollNum;
};

} // namespace bigbang

#endif //BIGBANG_CMQCLUSTER_H
