// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "docker/timer.h"

#include <atomic>
#include <boost/test/unit_test.hpp>
#include <thread>

#include "actor/actor.h"
#include "docker/config.h"
#include "docker/docker.h"
#include "message/message.h"
#include "message/messagecenter.h"
#include "test_big.h"

using namespace std;
using namespace xengine;

BOOST_FIXTURE_TEST_SUITE(timer_tests, BasicUtfSetup)

struct CTimeoutMessageA : public CTimeoutMessage
{
    GENERATE_MESSAGE_FUNCTION(CTimeoutMessageA);
    boost::system_time tm;
    static boost::system_time PublishTime;
    static boost::system_time HandledTimeA;
    static boost::system_time HandledTimeB;
};
boost::system_time CTimeoutMessageA::PublishTime;
boost::system_time CTimeoutMessageA::HandledTimeA;
boost::system_time CTimeoutMessageA::HandledTimeB;

struct CTimeoutMessageB : public CTimeoutMessage
{
    GENERATE_MESSAGE_FUNCTION(CTimeoutMessageB);
    boost::system_time tm;
    static boost::system_time PublishTime;
    static boost::system_time HandledTimeA;
    static boost::system_time HandledTimeB;
};
boost::system_time CTimeoutMessageB::PublishTime;
boost::system_time CTimeoutMessageB::HandledTimeA;
boost::system_time CTimeoutMessageB::HandledTimeB;

struct CTimeoutMessageC : public CTimeoutMessage
{
    GENERATE_MESSAGE_FUNCTION(CTimeoutMessageC);
    boost::system_time tm;
    static boost::system_time PublishTime;
    static boost::system_time HandledTimeA;
    static boost::system_time HandledTimeB;
};
boost::system_time CTimeoutMessageC::PublishTime;
boost::system_time CTimeoutMessageC::HandledTimeA;
boost::system_time CTimeoutMessageC::HandledTimeB;

struct CTimeoutMessageD : public CTimeoutMessage
{
    GENERATE_MESSAGE_FUNCTION(CTimeoutMessageD);
    boost::system_time tm;
    static boost::system_time PublishTime;
    static boost::system_time HandledTimeA;
    static boost::system_time HandledTimeB;
};
boost::system_time CTimeoutMessageD::PublishTime;
boost::system_time CTimeoutMessageD::HandledTimeA;
boost::system_time CTimeoutMessageD::HandledTimeB;

class CActorA : public CActor
{
public:
    CActorA()
      : CActor("actorA") {}
    virtual bool HandleInitialize() override
    {
        RegisterHandler({
            REF_HANDLER(CTimeoutMessageA, boost::bind(&CActorA::HandlerMessageA, this, _1), true),
            REF_HANDLER(CTimeoutMessageB, boost::bind(&CActorA::HandlerMessageB, this, _1), true),
            REF_HANDLER(CTimeoutMessageC, boost::bind(&CActorA::HandlerMessageC, this, _1), true),
        });
        return true;
    }
    virtual bool HandleInvoke() override
    {
        return StartActor();
    }
    virtual void HandleHalt() override
    {
        StopActor();
    }
    virtual void HandleDeinitialize() override
    {
        DeregisterHandler();
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

class CActorB : public CActor
{
public:
    CActorB()
      : CActor("actorB") {}
    virtual bool HandleInitialize() override
    {
        RegisterHandler({
            REF_HANDLER(CTimeoutMessageA, boost::bind(&CActorB::HandlerMessage, this, _1), true),
            REF_HANDLER(CTimeoutMessageB, boost::bind(&CActorB::HandlerMessage, this, _1), true),
            REF_HANDLER(CTimeoutMessageC, boost::bind(&CActorB::HandlerMessage, this, _1), true),
            REF_HANDLER(CTimeoutMessageD, boost::bind(&CActorB::HandlerMessage, this, _1), true),
        });
        return true;
    }
    virtual bool HandleInvoke() override
    {
        return StartActor();
    }
    virtual void HandleHalt() override
    {
        StopActor();
    }
    virtual void HandleDeinitialize() override
    {
        DeregisterHandler();
    }

protected:
    void HandlerMessage(const CTimeoutMessage& msg)
    {
        if (msg.Type() == CTimeoutMessageA::MessageType())
        {
            CTimeoutMessageA::HandledTimeB = boost::get_system_time();
        }
        else if (msg.Type() == CTimeoutMessageB::MessageType())
        {
            CTimeoutMessageB::HandledTimeB = boost::get_system_time();
        }
        else if (msg.Type() == CTimeoutMessageC::MessageType())
        {
            CTimeoutMessageC::HandledTimeB = boost::get_system_time();
        }
        else if (msg.Type() == CTimeoutMessageD::MessageType())
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
    InitLog(boost::filesystem::path(), severity_level::TRACE, true, true);

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
    auto spTimeoutA = CTimeoutMessageA::Create();
    spTimeoutA->tm = boost::get_system_time();
    auto handledTimeA = spTimeoutA->tm + boost::posix_time::milliseconds(100);

    auto spSetTimerA = CSetTimerMessage::Create(spTimeoutA, handledTimeA);
    PUBLISH_MESSAGE(spSetTimerA);

    // publish B
    auto spTimeoutB = CTimeoutMessageB::Create();
    spTimeoutB->tm = boost::get_system_time();
    auto handledTimeB = spTimeoutB->tm + boost::posix_time::milliseconds(300);

    auto spSetTimerB = CSetTimerMessage::Create(spTimeoutB, handledTimeB);
    PUBLISH_MESSAGE(spSetTimerB);

    // publish C
    auto spTimeoutC = CTimeoutMessageC::Create();
    spTimeoutC->tm = boost::get_system_time();
    auto handledTimeC = spTimeoutC->tm + boost::posix_time::milliseconds(500);

    auto spSetTimerC = CSetTimerMessage::Create(spTimeoutC, handledTimeC);
    PUBLISH_MESSAGE(spSetTimerC);

    // publish D
    auto spTimeoutD = CTimeoutMessageD::Create();
    spTimeoutD->tm = boost::get_system_time();
    auto handledTimeD = spTimeoutD->tm + boost::posix_time::milliseconds(800);

    auto spSetTimerD = CSetTimerMessage::Create(spTimeoutD, handledTimeD);
    PUBLISH_MESSAGE(spSetTimerD);

    // cancel C
    auto spCancelC = CCancelTimerMessage::Create();
    spCancelC->spTimeout = spTimeoutC;
    PUBLISH_MESSAGE(spCancelC);

    this_thread::sleep_for(chrono::seconds(1));

    docker.Exit();

    cout << "Timer difference Message A of handler A: " << (CTimeoutMessageA::HandledTimeA - handledTimeA).total_milliseconds() << "ms" << endl;
    cout << "Timer difference Message A of handler B: " << (CTimeoutMessageA::HandledTimeB - handledTimeA).total_milliseconds() << "ms" << endl;
    cout << "Timer difference Message B of handler A: " << (CTimeoutMessageB::HandledTimeA - handledTimeB).total_milliseconds() << "ms" << endl;
    cout << "Timer difference Message B of handler B: " << (CTimeoutMessageB::HandledTimeB - handledTimeB).total_milliseconds() << "ms" << endl;
    cout << "Timer difference Message D of handler B: " << (CTimeoutMessageD::HandledTimeB - handledTimeD).total_milliseconds() << "ms" << endl;

    auto delta = boost::posix_time::seconds(1);
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