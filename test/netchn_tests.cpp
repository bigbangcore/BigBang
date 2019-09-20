// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "netchn.h"

#include <boost/test/unit_test.hpp>
#include <chrono>
#include <thread>

#include "blockchain.h"
#include "common/message.h"
#include "core.h"
#include "dispatcher.h"
#include "docker/config.h"
#include "docker/docker.h"
#include "message/actor.h"
#include "message/message.h"
#include "message/messagecenter.h"
#include "netchn.h"
#include "proto.h"
#include "service.h"
#include "test_big.h"
#include "txpool.h"

using namespace xengine;
using namespace bigbang;

BOOST_FIXTURE_TEST_SUITE(netchn_tests, BasicUtfSetup)

class CDummyPeerNet : public CIOProc
{
public:
    CDummyPeerNet()
      : CIOProc("peernet") {}
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

BOOST_AUTO_TEST_CASE(netchn_msg)
{
    CDocker docker;
    BOOST_CHECK(docker.Initialize(new xengine::CConfig));
    BOOST_CHECK(docker.Attach(new CCoreProtocol()));
    BOOST_CHECK(docker.Attach(new CDummyPeerNet()));
    BOOST_CHECK(docker.Attach(new CService()));
    BOOST_CHECK(docker.Attach(new CTxPool()));
    BOOST_CHECK(docker.Attach(new CDispatcher()));
    BOOST_CHECK(docker.Attach(new CBlockChain()));
    BOOST_CHECK(docker.Attach(new bigbang::CNetChannel()));

    BOOST_CHECK(docker.Run());

    auto spActiveMsg = CPeerActiveMessage::Create();
    spActiveMsg->nNonce = 0xff;
    spActiveMsg->address = network::CAddress(network::NODE_NETWORK, network::CEndpoint());
    PUBLISH_MESSAGE(spActiveMsg);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    docker.Exit();
}

BOOST_AUTO_TEST_SUITE_END()
