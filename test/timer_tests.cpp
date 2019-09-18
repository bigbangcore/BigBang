// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "docker/timer.h"

#include <atomic>
#include <boost/test/unit_test.hpp>
#include <thread>

#include "docker/config.h"
#include "docker/docker.h"
#include "message/actor.h"
#include "message/message.h"
#include "message/messagecenter.h"
#include "test_big.h"

using namespace std;
using namespace xengine;

BOOST_FIXTURE_TEST_SUITE(timer_tests, BasicUtfSetup)

struct CTimeoutMessageA : public CTimeoutMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CTimeoutMessageA);
    boost::system_time tm;
    static boost::system_time PublishTime;
    static boost::system_time HandledTimeA;
    static boost::system_time HandledTimeB;
};
INITIALIZE_MESSAGE_STATIC_VAR(CTimeoutMessageA, "TimeoutMessageA");
boost::system_time CTimeoutMessageA::PublishTime;
boost::system_time CTimeoutMessageA::HandledTimeA;
boost::system_time CTimeoutMessageA::HandledTimeB;

struct CTimeoutMessageB : public CTimeoutMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CTimeoutMessageB);
    boost::system_time tm;
    static boost::system_time PublishTime;
    static boost::system_time HandledTimeA;
    static boost::system_time HandledTimeB;
};
INITIALIZE_MESSAGE_STATIC_VAR(CTimeoutMessageB, "TimeoutMessageB");
boost::system_time CTimeoutMessageB::PublishTime;
boost::system_time CTimeoutMessageB::HandledTimeA;
boost::system_time CTimeoutMessageB::HandledTimeB;

struct CTimeoutMessageC : public CTimeoutMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CTimeoutMessageC);
    boost::system_time tm;
    static boost::system_time PublishTime;
    static boost::system_time HandledTimeA;
    static boost::system_time HandledTimeB;
};
INITIALIZE_MESSAGE_STATIC_VAR(CTimeoutMessageC, "TimeoutMessageC");
boost::system_time CTimeoutMessageC::PublishTime;
boost::system_time CTimeoutMessageC::HandledTimeA;
boost::system_time CTimeoutMessageC::HandledTimeB;

struct CTimeoutMessageD : public CTimeoutMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CTimeoutMessageD);
    boost::system_time tm;
    static boost::system_time PublishTime;
    static boost::system_time HandledTimeA;
    static boost::system_time HandledTimeB;
};
INITIALIZE_MESSAGE_STATIC_VAR(CTimeoutMessageD, "TimeoutMessageD");
boost::system_time CTimeoutMessageD::PublishTime;
boost::system_time CTimeoutMessageD::HandledTimeA;
boost::system_time CTimeoutMessageD::HandledTimeB;

class CActorA : public CIOActor
{
public:
    CActorA()
      : CIOActor("actorA") {}
    virtual bool HandleInitialize() override
    {
        RegisterHandler<CTimeoutMessageA>(boost::bind(&CActorA::HandlerMessageA, this, _1));
        RegisterHandler<CTimeoutMessageB>(boost::bind(&CActorA::HandlerMessageB, this, _1));
        RegisterHandler<CTimeoutMessageC>(boost::bind(&CActorA::HandlerMessageC, this, _1));
        return true;
    }

protected:
    void HandlerMessageA(const CTimeoutMessageA& msg)
    {
        CTimeoutMessageA::HandledTimeA = boost::get_system_time();
    }
    void HandlerMessageB(const CTimeoutMessageB& msg)
    {
        CTimeoutMessageB::HandledTimeA = boost::get_system_time();
    }
    void HandlerMessageC(const CTimeoutMessageC& msg)
    {
        CTimeoutMessageC::HandledTimeA = boost::get_system_time();
    }
};

class CActorB : public CIOActor
{
public:
    CActorB()
      : CIOActor("actorB") {}
    virtual bool HandleInitialize() override
    {
        RegisterHandler<CTimeoutMessageA>(boost::bind(&CActorB::HandlerMessage, this, _1));
        RegisterHandler<CTimeoutMessageB>(boost::bind(&CActorB::HandlerMessage, this, _1));
        RegisterHandler<CTimeoutMessageC>(boost::bind(&CActorB::HandlerMessage, this, _1));
        RegisterHandler<CTimeoutMessageD>(boost::bind(&CActorB::HandlerMessage, this, _1));
        return true;
    }

protected:
    void HandlerMessage(const CTimeoutMessage& msg)
    {
        if (msg.Type() == CTimeoutMessageA::nType)
        {
            CTimeoutMessageA::HandledTimeB = boost::get_system_time();
        }
        else if (msg.Type() == CTimeoutMessageB::nType)
        {
            CTimeoutMessageB::HandledTimeB = boost::get_system_time();
        }
        else if (msg.Type() == CTimeoutMessageC::nType)
        {
            CTimeoutMessageC::HandledTimeB = boost::get_system_time();
        }
        else if (msg.Type() == CTimeoutMessageD::nType)
        {
            CTimeoutMessageD::HandledTimeB = boost::get_system_time();
        }
        else
        {
            cout << "Unknown CMessage: " << msg.Type() << endl;
        }
    }
};

BOOST_AUTO_TEST_CASE(basic)
{
    CDocker docker;
    docker.Initialize(new CConfig);

    CActorA* pActorA = new CActorA;
    docker.Attach(pActorA);

    CActorB* pActorB = new CActorB;
    docker.Attach(pActorB);

    CTimer* pTimer = new CTimer;
    docker.Attach(pTimer);

    docker.Run();

    // publish A
    shared_ptr<CTimeoutMessageA> spTimeoutA(new CTimeoutMessageA);
    spTimeoutA->tm = boost::get_system_time();
    auto handledTimeA = spTimeoutA->tm + boost::posix_time::milliseconds(100);

    CSetTimerMessage* pSetTimerA = new CSetTimerMessage(spTimeoutA, handledTimeA);
    PUBLISH_MESSAGE(pSetTimerA);

    // publish B
    shared_ptr<CTimeoutMessageB> spTimeoutB(new CTimeoutMessageB);
    spTimeoutB->tm = boost::get_system_time();
    auto handledTimeB = spTimeoutB->tm + boost::posix_time::milliseconds(300);

    CSetTimerMessage* pSetTimerB = new CSetTimerMessage(spTimeoutB, handledTimeB);
    PUBLISH_MESSAGE(pSetTimerB);

    // publish C
    shared_ptr<CTimeoutMessageC> spTimeoutC(new CTimeoutMessageC);
    spTimeoutC->tm = boost::get_system_time();
    auto handledTimeC = spTimeoutC->tm + boost::posix_time::milliseconds(500);

    CSetTimerMessage* pSetTimerC = new CSetTimerMessage(spTimeoutC, handledTimeC);
    PUBLISH_MESSAGE(pSetTimerC);

    // publish D
    shared_ptr<CTimeoutMessageD> spTimeoutD(new CTimeoutMessageD);
    spTimeoutD->tm = boost::get_system_time();
    auto handledTimeD = spTimeoutD->tm + boost::posix_time::milliseconds(800);

    CSetTimerMessage* pSetTimerD = new CSetTimerMessage(spTimeoutD, handledTimeD);
    PUBLISH_MESSAGE(pSetTimerD);

    // cancel C
    shared_ptr<CCancelTimerMessage> spCancelC(new CCancelTimerMessage());
    spCancelC->spTimeout = spTimeoutC;
    PUBLISH_MESSAGE(spCancelC);

    this_thread::sleep_for(chrono::seconds(1));

    docker.Exit();

    // cout << "Message A handler A. Publish time: " << spTimeoutA->tm << ". Plan timeout: " << handledTimeA << ". Real timeout: " << CTimeoutMessageA::HandledTimeA << ". Duration: " << (CTimeoutMessageA::HandledTimeA - spTimeoutA->tm).total_milliseconds() << endl;
    // cout << "Message A handler B. Publish time: " << spTimeoutA->tm << ". Plan timeout: " << handledTimeA << ". Real timeout: " << CTimeoutMessageA::HandledTimeB << ". Duration: " << (CTimeoutMessageA::HandledTimeB - spTimeoutA->tm).total_milliseconds() << endl;
    // cout << "Message B handler A. Publish time: " << spTimeoutB->tm << ". Plan timeout: " << handledTimeB << ". Real timeout: " << CTimeoutMessageB::HandledTimeA << ". Duration: " << (CTimeoutMessageB::HandledTimeA - spTimeoutB->tm).total_milliseconds() << endl;
    // cout << "Message B handler B. Publish time: " << spTimeoutB->tm << ". Plan timeout: " << handledTimeB << ". Real timeout: " << CTimeoutMessageB::HandledTimeB << ". Duration: " << (CTimeoutMessageB::HandledTimeB - spTimeoutB->tm).total_milliseconds() << endl;
    // cout << "Message C handler A. Publish time: " << spTimeoutC->tm << ". Plan timeout: " << handledTimeC << ". Real timeout: " << CTimeoutMessageC::HandledTimeA << ". Duration: " << (CTimeoutMessageC::HandledTimeA - spTimeoutC->tm).total_milliseconds() << endl;
    // cout << "Message C handler B. Publish time: " << spTimeoutC->tm << ". Plan timeout: " << handledTimeC << ". Real timeout: " << CTimeoutMessageC::HandledTimeB << ". Duration: " << (CTimeoutMessageC::HandledTimeB - spTimeoutC->tm).total_milliseconds() << endl;
    // cout << "Message D handler A. Publish time: " << spTimeoutD->tm << ". Plan timeout: " << handledTimeD << ". Real timeout: " << CTimeoutMessageD::HandledTimeA << ". Duration: " << (CTimeoutMessageD::HandledTimeA - spTimeoutD->tm).total_milliseconds() << endl;
    // cout << "Message D handler B. Publish time: " << spTimeoutD->tm << ". Plan timeout: " << handledTimeD << ". Real timeout: " << CTimeoutMessageD::HandledTimeB << ". Duration: " << (CTimeoutMessageD::HandledTimeB - spTimeoutD->tm).total_milliseconds() << endl;

    auto delta = boost::posix_time::milliseconds(10);
    BOOST_CHECK(CTimeoutMessageA::HandledTimeA >= (handledTimeA - delta) && CTimeoutMessageA::HandledTimeA <= (handledTimeA + delta));
    BOOST_CHECK(CTimeoutMessageA::HandledTimeB >= (handledTimeA - delta) && CTimeoutMessageA::HandledTimeB <= (handledTimeA + delta));
    BOOST_CHECK(CTimeoutMessageB::HandledTimeA >= (handledTimeB - delta) && CTimeoutMessageB::HandledTimeA <= (handledTimeB + delta));
    BOOST_CHECK(CTimeoutMessageB::HandledTimeB >= (handledTimeB - delta) && CTimeoutMessageB::HandledTimeB <= (handledTimeB + delta));
    BOOST_CHECK(CTimeoutMessageC::HandledTimeA.is_not_a_date_time());
    BOOST_CHECK(CTimeoutMessageC::HandledTimeB.is_not_a_date_time());
    BOOST_CHECK(CTimeoutMessageD::HandledTimeA.is_not_a_date_time());
    BOOST_CHECK(CTimeoutMessageD::HandledTimeB >= (handledTimeD - delta) && CTimeoutMessageD::HandledTimeB <= (handledTimeD + delta));
}

BOOST_AUTO_TEST_SUITE_END()