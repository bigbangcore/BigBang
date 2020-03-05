// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_CMQCLUSTER_H
#define BIGBANG_CMQCLUSTER_H

#include "base.h"
#include "mqevent.h"
#include "xengine.h"

namespace bigbang
{

class IMQCluster : public xengine::IIOModule, virtual public CMQEventListener
{
public:
    IMQCluster()
      : IIOModule("mqcluster") {}
    virtual bool IsAuthenticated() = 0;
};

class CMQCluster : public IMQCluster
{
    enum NODE_TYPE {BBCNODE, FORKNODE, DPOSNODE};
public:
    CMQCluster();
    ~CMQCluster() = default;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;

    bool HandleEvent(CEventMQSyncBlock& eventMqSyncBlock) override;
    bool HandleEvent(CEventMQUpdateBlock& eventMqUpdateBlock) override;
    bool HandleEvent(CEventMQAgreement& eventMqAgreement) override;

    bool IsAuthenticated() override;

protected:
    mutable boost::shared_mutex rwAccess;
    ICoreProtocol* pCoreProtocol;
    IBlockChain* pBlockChain;
    IService* pService;

    boost::mutex mutex;
    boost::condition_variable condMQ;
    xengine::CThread thrMqttClient;

private:
    void Publish();
    //bool Publish();
    void MqttThreadFunc();
    bool fAuth;
    bool fAbort;
    string clientID;
};

} // namespace bigbang

#endif //BIGBANG_CMQCLUSTER_H
