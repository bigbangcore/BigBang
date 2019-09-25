// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "netchn.h"

#include <boost/test/unit_test.hpp>
#include <chrono>
#include <thread>

#include "blockchain.h"
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
#include "txpool.h"
#include "wallet.h"

using namespace xengine;
using namespace bigbang;

BOOST_FIXTURE_TEST_SUITE(netchn_tests, BasicUtfSetup)

class CDummyPeerNet : public CNetwork
{
public:
    CDummyPeerNet() {}
    virtual bool HandleInitialize() override
    {
        RegisterHandler<CPeerNetCloseMessage>(boost::bind(&CDummyPeerNet::HandleNetClose, this, _1));
        RegisterHandler<CPeerNetRewardMessage>(boost::bind(&CDummyPeerNet::HandleNetReward, this, _1));
        return true;
    }

protected:
    void HandleNetClose(const CPeerNetCloseMessage& msg)
    {
    }

    void HandleNetReward(const CPeerNetRewardMessage& msg)
    {
    }
};

class CDummyNetChannel : public CNetChannel
{
public:
    bool TestActiveNonce(uint64 nNonce)
    {
        return mapPeer.count(nNonce) != 0;
    }

    bool TestDeactiveNonce(uint nNonce)
    {
        return mapPeer.count(nNonce) == 0;
    }

    bool TestSubscribe(uint nNonce, const uint256& fork)
    {
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
    CDocker docker;
    InitLog("./", true, false);
    xengine::CLog log;
    BOOST_CHECK(docker.Initialize(new xengine::CConfig, &log));
    BOOST_CHECK(docker.Attach(new CConsensus()));
    auto pCoreProtocol = new CCoreProtocol();
    BOOST_CHECK(docker.Attach(pCoreProtocol));
    BOOST_CHECK(docker.Attach(new CDummyPeerNet()));
    BOOST_CHECK(docker.Attach(new CForkManager()));
    BOOST_CHECK(docker.Attach(new CWallet()));
    BOOST_CHECK(docker.Attach(new CService()));
    BOOST_CHECK(docker.Attach(new CTxPool()));
    BOOST_CHECK(docker.Attach(new CBlockChain()));
    BOOST_CHECK(docker.Attach(new CBlockMaker()));
    BOOST_CHECK(docker.Attach(new CDataStat()));
    BOOST_CHECK(docker.Attach(new CDispatcher()));
    BOOST_CHECK(docker.Attach(new CDelegatedChannel()));
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

    BOOST_CHECK(pNetChannel->TestActiveNonce(spActiveMsg->nNonce));

    ///////////////////   Subscribe Test  //////////////////

    auto spSubscribeInBound = CPeerSubscribeMessageInBound::Create();
    spSubscribeInBound->nNonce = nTestNonce;
    spSubscribeInBound->hashFork = pCoreProtocol->GetGenesisBlockHash();
    spSubscribeInBound->vecForks.push_back(uint256("testfork"));
    PUBLISH_MESSAGE(spSubscribeInBound);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    pNetChannel->SubscribeFork(uint256("testfork"), nTestNonce);
    BOOST_CHECK(pNetChannel->TestSubscribe(spSubscribeInBound->nNonce, uint256("testfork")));

    ///////////////////   Unsubscribe Test  //////////////////

    auto spUnsubscribeInBound = CPeerUnsubscribeMessageInBound::Create();
    spUnsubscribeInBound->nNonce = nTestNonce;
    spUnsubscribeInBound->hashFork = pCoreProtocol->GetGenesisBlockHash();
    spUnsubscribeInBound->vecForks.push_back(uint256("testfork"));
    PUBLISH_MESSAGE(spUnsubscribeInBound);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    BOOST_CHECK(pNetChannel->TestUnsubscribe(spUnsubscribeInBound->nNonce, uint256("testfork")));

    ///////////////////   Deactive Test  //////////////////

    auto spDeactiveMsg = CPeerDeactiveMessage::Create();
    spDeactiveMsg->nNonce = nTestNonce;
    spDeactiveMsg->address = network::CAddress(network::NODE_NETWORK, network::CEndpoint());
    PUBLISH_MESSAGE(spDeactiveMsg);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    BOOST_CHECK(pNetChannel->TestDeactiveNonce(spDeactiveMsg->nNonce));

    docker.Exit();
}

class CDummyDelegatedChannel : public CDelegatedChannel
{
public:
    bool TestActiveNonce(uint64 nNonce)
    {
        if (schedPeer.mapPeer.count(nNonce) == 0)
        {
            return false;
        }
        return true;
    }

    bool TestDeactiveNonce(uint64 nNonce)
    {
        if (schedPeer.mapPeer.count(nNonce) != 0)
        {
            return false;
        }
        return true;
    }

    bool TestBulletin(uint64 nNonce, uint256 hashAnchor)
    {
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
    CDocker docker;
    InitLog("./", true, false);
    xengine::CLog log;
    BOOST_CHECK(docker.Initialize(new xengine::CConfig, &log));
    BOOST_CHECK(docker.Attach(new CConsensus()));
    auto pCoreProtocol = new CCoreProtocol();
    BOOST_CHECK(docker.Attach(pCoreProtocol));
    BOOST_CHECK(docker.Attach(new CDummyPeerNet()));
    BOOST_CHECK(docker.Attach(new CForkManager()));
    BOOST_CHECK(docker.Attach(new CWallet()));
    BOOST_CHECK(docker.Attach(new CService()));
    BOOST_CHECK(docker.Attach(new CTxPool()));
    BOOST_CHECK(docker.Attach(new CBlockChain()));
    BOOST_CHECK(docker.Attach(new CBlockMaker()));
    BOOST_CHECK(docker.Attach(new CDataStat()));
    BOOST_CHECK(docker.Attach(new CDispatcher()));
    auto pDelegatedChannel = new CDummyDelegatedChannel();
    BOOST_CHECK(docker.Attach(pDelegatedChannel));
    BOOST_CHECK(docker.Attach(new CNetChannel()));

    BOOST_CHECK(docker.Run());

    const uint64 nTestNonce = 0xff;

    ///////////////////   Active Test  //////////////////

    auto spActiveMsg = CPeerActiveMessage::Create();
    spActiveMsg->nNonce = nTestNonce;
    spActiveMsg->address = network::CAddress(network::NODE_DELEGATED, network::CEndpoint());
    PUBLISH_MESSAGE(spActiveMsg);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    BOOST_CHECK(pDelegatedChannel->TestActiveNonce(spActiveMsg->nNonce));

    //////////////////  Bulletin Test   /////////////////

    auto spBulletinMsg = CPeerBulletinMessageInBound::Create();
    spBulletinMsg->nNonce = nTestNonce;
    spBulletinMsg->hashAnchor = uint256("testHashAchor");
    spBulletinMsg->deletegatedBulletin.bmDistribute = 0x001;
    spBulletinMsg->deletegatedBulletin.bmPublish = 0x002;
    PUBLISH_MESSAGE(spBulletinMsg);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    BOOST_CHECK(pDelegatedChannel->TestBulletin(spBulletinMsg->nNonce, spBulletinMsg->hashAnchor));

    //////////////////   Deactive Test  /////////////////

    auto spDeactiveMsg = CPeerDeactiveMessage::Create();
    spDeactiveMsg->nNonce = nTestNonce;
    spDeactiveMsg->address = network::CAddress(network::NODE_DELEGATED, network::CEndpoint());
    PUBLISH_MESSAGE(spDeactiveMsg);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    BOOST_CHECK(pDelegatedChannel->TestDeactiveNonce(spDeactiveMsg->nNonce));

    docker.Exit();
}

BOOST_AUTO_TEST_SUITE_END()
