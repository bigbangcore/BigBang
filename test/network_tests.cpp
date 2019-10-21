// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <thread>

#include "blockmaker.h"
#include "common/message.h"
#include "consensus.h"
#include "core.h"
#include "datastat.h"
#include "delegatedchn.h"
#include "dispatcher.h"
#include "docker/config.h"
#include "docker/docker.h"
#include "forkmanager.h"
#include "message/actor.h"
#include "message/message.h"
#include "message/messagecenter.h"
#include "netchn.h"
#include "proto.h"
#include "service.h"
#include "test_big.h"
#include "txpool/txpool.h"
#include "txpool/txpoolcontroller.h"
#include "wallet.h"
#include "worldline.h"

using namespace xengine;
using namespace bigbang;

BOOST_FIXTURE_TEST_SUITE(network_tests, BasicUtfSetup)

class CDummyPeerNet : public CNetwork
{
public:
    CDummyPeerNet() {}
    virtual bool HandleInitialize() override
    {
        RegisterRefHandler<CPeerNetCloseMessage>(boost::bind(&CDummyPeerNet::HandleNetClose, this, _1));
        RegisterRefHandler<CPeerNetRewardMessage>(boost::bind(&CDummyPeerNet::HandleNetReward, this, _1));
        return true;
    }

    virtual void HandleDeinitialize() override
    {
        DeregisterHandler(CPeerNetCloseMessage::MessageType());
        DeregisterHandler(CPeerNetRewardMessage::MessageType());
    }

    void AddTestNewPeer()
    {
        boost::asio::io_service ioService;
        CIOProc* pProc = new CIOProc("TestProc");
        CIOContainer* pContainer = new CIOCachedContainer(pProc);
        CIOClient* pIoClient = new CSocketClient(pContainer, ioService);
        AddNewPeer(pIoClient, true);
    }

protected:
    void HandleNetClose(const CPeerNetCloseMessage& msg)
    {
    }

    void HandleNetReward(const CPeerNetRewardMessage& msg)
    {
    }
};

class CDummyNetChannelController : public CNetChannelController
{
public:
    bool TestActiveNonce(uint64 nNonce)
    {
        boost::shared_lock<boost::shared_mutex> lock(rwNetPeer);
        return mapPeer.count(nNonce) != 0;
    }

    bool TestDeactiveNonce(uint nNonce)
    {
        boost::shared_lock<boost::shared_mutex> lock(rwNetPeer);
        return mapPeer.count(nNonce) == 0;
    }

    bool TestSubscribe(uint nNonce, const uint256& fork)
    {
        boost::shared_lock<boost::shared_mutex> lock(rwNetPeer);
        if (mapPeer.count(nNonce) == 0
            || mapUnsync.count(fork) == 0
            || mapSched.count(fork) == 0)
        {
            return false;
        }

        if (mapUnsync[fork].count(nNonce) == 0)
        {
            return false;
        }

        return true;
    }

    bool TestUnsubscribe(uint nNonce, const uint256& fork)
    {
        boost::shared_lock<boost::shared_mutex> lock(rwNetPeer);
        if (mapUnsync.count(fork) == 0)
        {
            return false;
        }

        if (mapUnsync[fork].count(nNonce) != 0)
        {
            return false;
        }

        if (mapPeer.count(nNonce) == 0 || mapSched.count(fork) == 0)
        {
            return false;
        }

        return true;
    }
};

BOOST_AUTO_TEST_CASE(netchn_msg)
{
    auto pCfg = new CBasicConfig();
    boost::filesystem::path dataPath(boost::filesystem::current_path().generic_string() + boost::filesystem::path("/data-dir").generic_string());
    boost::filesystem::create_directory(dataPath);
    std::string args = std::string("-datadir=") + dataPath.generic_string();

    const char* argv[] = {
        (char*)("/bin/test"),
        (char*)(args.c_str())
    };
    pCfg->pathData = dataPath;
    BOOST_CHECK(pCfg->Load(2, const_cast<char**>(argv), dataPath.c_str(), "bigbang.conf"));

    CDocker docker;
    InitLog(dataPath.c_str(), true, false);
    xengine::CLog log;

    BOOST_CHECK(docker.Initialize(pCfg, &log));
    BOOST_CHECK(docker.Attach(new CConsensus()));
    auto pCoreProtocol = new CCoreProtocol();
    BOOST_CHECK(docker.Attach(pCoreProtocol));
    BOOST_CHECK(docker.Attach(new CDummyPeerNet()));
    BOOST_CHECK(docker.Attach(new CForkManager()));
    BOOST_CHECK(docker.Attach(new CWallet()));
    BOOST_CHECK(docker.Attach(new CService()));
    BOOST_CHECK(docker.Attach(new CTxPool()));
    BOOST_CHECK(docker.Attach(new CTxPoolController()));
    BOOST_CHECK(docker.Attach(new CWorldLine()));
    BOOST_CHECK(docker.Attach(new CWorldLineController()));
    BOOST_CHECK(docker.Attach(new CBlockMaker()));
    BOOST_CHECK(docker.Attach(new CDataStat()));
    BOOST_CHECK(docker.Attach(new CDispatcher()));
    BOOST_CHECK(docker.Attach(new CDelegatedChannel()));
    BOOST_CHECK(docker.Attach(new CNetChannelController()));
    auto pNetChannel = new CDummyNetChannel();
    BOOST_CHECK(docker.Attach(pNetChannel));

    BOOST_CHECK(docker.Run());

    const uint64 nTestNonce = 0xff;

    ///////////////////   Active Test  //////////////////

    auto spActiveMsg = CPeerActiveMessage::Create();
    spActiveMsg->nNonce = nTestNonce;
    spActiveMsg->address = network::CAddress(network::NODE_NETWORK, network::CEndpoint());
    PUBLISH_MESSAGE(spActiveMsg);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    BOOST_CHECK(pNetChannelCtrl->TestActiveNonce(nTestNonce));

    ///////////////////   Subscribe Test  //////////////////

    auto spSubscribeInBound = CPeerSubscribeMessageInBound::Create();
    spSubscribeInBound->nNonce = nTestNonce;
    spSubscribeInBound->hashFork = pCoreProtocol->GetGenesisBlockHash();
    spSubscribeInBound->vecForks.push_back(uint256("testfork"));
    PUBLISH_MESSAGE(spSubscribeInBound);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto spSubscribeForkMsg = CSubscribeForkMessage::Create(uint256("testfork"), nTestNonce);
    PUBLISH_MESSAGE(spSubscribeForkMsg);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    BOOST_CHECK(pNetChannelCtrl->TestSubscribe(nTestNonce, uint256("testfork")));

    ///////////////////   Unsubscribe Test  //////////////////

    auto spUnsubscribeInBound = CPeerUnsubscribeMessageInBound::Create();
    spUnsubscribeInBound->nNonce = nTestNonce;
    spUnsubscribeInBound->hashFork = pCoreProtocol->GetGenesisBlockHash();
    spUnsubscribeInBound->vecForks.push_back(uint256("testfork"));
    PUBLISH_MESSAGE(spUnsubscribeInBound);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    BOOST_CHECK(pNetChannelCtrl->TestUnsubscribe(nTestNonce, uint256("testfork")));

    ///////////////////   Deactive Test  //////////////////

    auto spDeactiveMsg = CPeerDeactiveMessage::Create();
    spDeactiveMsg->nNonce = nTestNonce;
    spDeactiveMsg->address = network::CAddress(network::NODE_NETWORK, network::CEndpoint());
    PUBLISH_MESSAGE(spDeactiveMsg);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    BOOST_CHECK(pNetChannelCtrl->TestDeactiveNonce(nTestNonce));

    docker.Exit();

    boost::filesystem::remove_all(dataPath);
}

class CDummyDelegatedChannel : public CDelegatedChannel
{
public:
    bool TestActiveNonce(uint64 nNonce)
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwPeer);
        return schedPeer.mapPeer.count(nNonce) != 0;
    }

    bool TestDeactiveNonce(uint64 nNonce)
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwPeer);
        return schedPeer.mapPeer.count(nNonce) == 0;
    }

    bool TestBulletin(uint64 nNonce, uint256 hashAnchor)
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwPeer);
        if (schedPeer.mapPeer.count(nNonce) == 0)
        {
            return false;
        }

        if (!schedPeer.GetPeer(nNonce))
        {
            return false;
        }

        if (dataChain.mapChainData.count(hashAnchor) != 0)
        {
            return false;
        }

        return true;
    }
};

BOOST_AUTO_TEST_CASE(delegated_chn_msg)
{
    auto pCfg = new CBasicConfig();
    boost::filesystem::path dataPath(boost::filesystem::current_path().generic_string() + boost::filesystem::path("/data-dir").generic_string());
    boost::filesystem::create_directory(dataPath);
    std::string args = std::string("-datadir=") + dataPath.generic_string();

    const char* argv[] = {
        (char*)("/bin/test"),
        (char*)(args.c_str())
    };
    pCfg->pathData = dataPath;
    BOOST_CHECK(pCfg->Load(2, const_cast<char**>(argv), dataPath.c_str(), "bigbang.conf"));

    CDocker docker;
    InitLog(dataPath.c_str(), true, false);
    xengine::CLog log;

    BOOST_CHECK(docker.Initialize(pCfg, &log));
    BOOST_CHECK(docker.Attach(new CConsensus()));
    BOOST_CHECK(docker.Attach(new CCoreProtocol()));
    BOOST_CHECK(docker.Attach(new CDummyPeerNet()));
    BOOST_CHECK(docker.Attach(new CForkManager()));
    BOOST_CHECK(docker.Attach(new CWallet()));
    BOOST_CHECK(docker.Attach(new CService()));
    BOOST_CHECK(docker.Attach(new CTxPool()));
    BOOST_CHECK(docker.Attach(new CTxPoolController()));
    BOOST_CHECK(docker.Attach(new CWorldLine()));
    BOOST_CHECK(docker.Attach(new CWorldLineController()));
    BOOST_CHECK(docker.Attach(new CBlockMaker()));
    BOOST_CHECK(docker.Attach(new CDataStat()));
    BOOST_CHECK(docker.Attach(new CDispatcher()));
    BOOST_CHECK(docker.Attach(new CNetChannel()));
    BOOST_CHECK(docker.Attach(new CNetChannelController()));
    auto pDelegatedChannel = new CDummyDelegatedChannel();
    BOOST_CHECK(docker.Attach(pDelegatedChannel));

    BOOST_CHECK(docker.Run());

    const uint64 nTestNonce = 0xff;

    ///////////////////   Active Test  //////////////////

    auto spActiveMsg = CPeerActiveMessage::Create();
    spActiveMsg->nNonce = nTestNonce;
    spActiveMsg->address = network::CAddress(network::NODE_DELEGATED, network::CEndpoint());
    PUBLISH_MESSAGE(spActiveMsg);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    BOOST_CHECK(pDelegatedChannel->TestActiveNonce(nTestNonce));

    //////////////////  Bulletin Test   /////////////////

    auto spBulletinMsg = CPeerBulletinMessageInBound::Create();
    spBulletinMsg->nNonce = nTestNonce;
    spBulletinMsg->hashAnchor = uint256("testHashAchor");
    spBulletinMsg->deletegatedBulletin.bmDistribute = 0x001;
    spBulletinMsg->deletegatedBulletin.bmPublish = 0x002;
    PUBLISH_MESSAGE(spBulletinMsg);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    BOOST_CHECK(pDelegatedChannel->TestBulletin(nTestNonce, uint256("testHashAchor")));

    //////////////////   Deactive Test  /////////////////

    auto spDeactiveMsg = CPeerDeactiveMessage::Create();
    spDeactiveMsg->nNonce = nTestNonce;
    spDeactiveMsg->address = network::CAddress(network::NODE_DELEGATED, network::CEndpoint());
    PUBLISH_MESSAGE(spDeactiveMsg);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    BOOST_CHECK(pDelegatedChannel->TestDeactiveNonce(nTestNonce));

    docker.Exit();

    boost::filesystem::remove_all(dataPath);
}

BOOST_AUTO_TEST_CASE(peernet_msg)
{
    auto pCfg = new CBasicConfig();
    boost::filesystem::path dataPath(boost::filesystem::current_path().generic_string() + boost::filesystem::path("/data-dir").generic_string());
    boost::filesystem::create_directory(dataPath);
    std::string args = std::string("-datadir=") + dataPath.generic_string();

    const char* argv[] = {
        (char*)("/bin/test"),
        (char*)(args.c_str())
    };
    pCfg->pathData = dataPath;
    BOOST_CHECK(pCfg->Load(2, const_cast<char**>(argv), dataPath.c_str(), "bigbang.conf"));

    CDocker docker;
    InitLog(dataPath.c_str(), true, false);
    xengine::CLog log;

    BOOST_CHECK(docker.Initialize(pCfg, &log));
    BOOST_CHECK(docker.Attach(new CConsensus()));
    BOOST_CHECK(docker.Attach(new CCoreProtocol()));
    auto pDummyPeerNet = new CDummyPeerNet();
    BOOST_CHECK(docker.Attach(pDummyPeerNet));
    BOOST_CHECK(docker.Attach(new CForkManager()));
    BOOST_CHECK(docker.Attach(new CWallet()));
    BOOST_CHECK(docker.Attach(new CService()));
    BOOST_CHECK(docker.Attach(new CTxPool()));
    BOOST_CHECK(docker.Attach(new CTxPoolController()));
    BOOST_CHECK(docker.Attach(new CWorldLine()));
    BOOST_CHECK(docker.Attach(new CWorldLineController()));
    BOOST_CHECK(docker.Attach(new CBlockMaker()));
    BOOST_CHECK(docker.Attach(new CDataStat()));
    BOOST_CHECK(docker.Attach(new CDispatcher()));
    BOOST_CHECK(docker.Attach(new CNetChannel()));
    BOOST_CHECK(docker.Attach(new CNetChannelController()));
    BOOST_CHECK(docker.Attach(new CDelegatedChannel()));

    BOOST_CHECK(docker.Run());

    std::promise<std::string> promiseIP;
    std::future<std::string> futureIP = promiseIP.get_future();
    auto spGetIPMsg = CPeerNetGetIPMessage::Create(std::move(promiseIP));
    PUBLISH_MESSAGE(spGetIPMsg);

    BOOST_CHECK(futureIP.get() == "0.0.0.0");

    std::promise<std::size_t> promiseCount;
    std::future<std::size_t> futureCount = promiseCount.get_future();
    auto spGetPeerCountMsg = CPeerNetGetCountMessage::Create(std::move(promiseCount));
    PUBLISH_MESSAGE(spGetPeerCountMsg);

    BOOST_CHECK(futureCount.get() == 0);

    docker.Exit();

    boost::filesystem::remove_all(dataPath);
}

BOOST_AUTO_TEST_SUITE_END()
