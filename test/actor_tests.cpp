// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "message/actor.h"

#include <atomic>
#include <boost/chrono/system_clocks.hpp>
#include <boost/test/unit_test.hpp>
#include <thread>

#include "docker/config.h"
#include "docker/docker.h"
#include "message/message.h"
#include "message/messagecenter.h"
#include "test_big.h"

using namespace std;
using namespace xengine;

BOOST_FIXTURE_TEST_SUITE(actor_tests, BasicUtfSetup)

struct CTestMessageA : public CMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CTestMessageA);
    string strA;
    static atomic<uint> nPublish;
    static atomic<uint> nHandled;
};
INITIALIZE_MESSAGE_TYPE(CTestMessageA);
atomic<uint> CTestMessageA::nPublish;
atomic<uint> CTestMessageA::nHandled;

struct CTestMessageB : public CMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CTestMessageB);
    string strB;
    static atomic<uint> nPublish;
    static atomic<uint> nHandled;
};
INITIALIZE_MESSAGE_TYPE(CTestMessageB);
atomic<uint> CTestMessageB::nPublish;
atomic<uint> CTestMessageB::nHandled;

struct CTestMessageC : public CTestMessageB
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CTestMessageC);
    string strC;
    static atomic<uint> nPublish;
    static atomic<uint> nHandled;
};
INITIALIZE_MESSAGE_TYPE(CTestMessageC);
atomic<uint> CTestMessageC::nPublish;
atomic<uint> CTestMessageC::nHandled;

struct CTestMessageD : public CTestMessageB
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CTestMessageD);
    string strD;
    static atomic<uint> nPublish;
    static atomic<uint> nHandled;
};
INITIALIZE_MESSAGE_TYPE(CTestMessageD);
atomic<uint> CTestMessageD::nPublish;
atomic<uint> CTestMessageD::nHandled;

class CActorA : public CIOActor
{
public:
    CActorA()
      : CIOActor("actorA") {}
    virtual bool HandleInitialize() override
    {
        RegisterHandler<CTestMessageA>(boost::bind(&CActorA::HandlerMessageA, this, _1));
        RegisterHandler<CTestMessageC>(boost::bind(&CActorA::HandlerMessageC, this, _1));
        return true;
    }

    boost::chrono::steady_clock::time_point now;

protected:
    void HandlerMessageA(const CTestMessageA& msg)
    {
        // cout << "Actor A handle message A as CTestMessageA: " << msg.strA << endl;
        CTestMessageA::nHandled++;
        now = boost::chrono::steady_clock::now();
    }
    void HandlerMessageC(const CTestMessageB& msg)
    {
        // cout << "Actor A handle message C as CTestMessageB: " << msg.strB << endl;
        CTestMessageC::nHandled++;
        now = boost::chrono::steady_clock::now();
    }
};

class CActorB : public CIOActor
{
public:
    CActorB()
      : CIOActor("actorB") {}
    virtual bool HandleInitialize() override
    {
        RegisterHandler<CTestMessageB>(boost::bind(&CActorB::HandlerMessage, this, _1));
        RegisterHandler<CTestMessageC>(boost::bind(&CActorB::HandlerMessage, this, _1));
        RegisterHandler<CTestMessageD>(boost::bind(&CActorB::HandlerMessage, this, _1));
        return true;
    }

    boost::chrono::steady_clock::time_point now;

protected:
    void HandlerMessage(const CMessage& msg)
    {
        if (msg.Type() == CTestMessageB::nType)
        {
            // cout << "Actor B handle message B as CMessage" << dynamic_cast<const CTestMessageB&>(msg).strB << endl;
            CTestMessageB::nHandled++;
        }
        else if (msg.Type() == CTestMessageC::nType)
        {
            // cout << "Actor B handle message C as CMessage" << dynamic_cast<const CTestMessageC&>(msg).strC << endl;
            CTestMessageC::nHandled++;
        }
        else if (msg.Type() == CTestMessageD::nType)
        {
            // cout << "Actor B handle message D as CMessage" << dynamic_cast<const CTestMessageD&>(msg).strD << endl;
            CTestMessageD::nHandled++;
        }
        else
        {
            cout << "Unknown CMessage: " << msg.Type() << endl;
        }
        now = boost::chrono::steady_clock::now();
    }
};

void Publish(int nA, int nB, int nC, int nD)
{
    shared_ptr<CTestMessageA> pA(new CTestMessageA);
    pA->strA = "A -> strA";
    for (int i = 0; i < nA; i++)
    {
        CMessageCenter::Publish(pA);
        CTestMessageA::nPublish++;
    }

    shared_ptr<CTestMessageB> pB(new CTestMessageB);
    pB->strB = "B -> strB";
    for (int i = 0; i < nB; i++)
    {
        CMessageCenter::Publish(pB);
        CTestMessageB::nPublish++;
    }

    shared_ptr<CTestMessageC> pC(new CTestMessageC);
    pC->strB = "C -> strB";
    pC->strC = "C -> strC";
    for (int i = 0; i < nC; i++)
    {
        CMessageCenter::Publish(pC);
        CTestMessageC::nPublish++;
    }

    shared_ptr<CTestMessageD> pD(new CTestMessageD);
    pD->strB = "D -> strB";
    pD->strD = "D -> strD";
    for (int i = 0; i < nD; i++)
    {
        CMessageCenter::Publish(pD);
        CTestMessageD::nPublish++;
    }
}

BOOST_AUTO_TEST_CASE(basic)
{
    CDocker docker;
    docker.Initialize(new CConfig);

    CActorA* actorA = new CActorA;
    docker.Attach(actorA);

    CActorB* actorB = new CActorB;
    docker.Attach(actorB);

    docker.Run();

    int nA = 5000, nB = 5000, nC = 5000, nD = 5000;
    auto begin = boost::chrono::steady_clock::now();
    std::thread th1(Publish, nA, nB, nC, nD);
    std::thread th2(Publish, nA, nB, nC, nD);
    std::thread th3(Publish, nA, nB, nC, nD);
    std::thread th4(Publish, nA, nB, nC, nD);
    th1.join();
    th2.join();
    th3.join();
    th4.join();

    sleep(2);
    auto end = max(actorA->now, actorB->now);
    uint nTotal = CTestMessageA::nHandled + CTestMessageB::nHandled + CTestMessageC::nHandled + CTestMessageD::nHandled;
    cout << "4 publishers and 2 actors handled " << nTotal << " times, use time: " << (end - begin).count() << " ns. Average: " << (end - begin).count() / nTotal << "ns." << endl;
    docker.Exit();

    BOOST_CHECK(CTestMessageA::nHandled == CTestMessageA::nPublish && CTestMessageA::nHandled == 4 * nA);
    BOOST_CHECK(CTestMessageB::nHandled == CTestMessageB::nPublish && CTestMessageB::nHandled == 4 * nB);
    BOOST_CHECK(CTestMessageC::nHandled == 2 * CTestMessageC::nPublish && CTestMessageC::nHandled == 4 * 2 * nC);
    BOOST_CHECK(CTestMessageD::nHandled == CTestMessageD::nPublish && CTestMessageD::nHandled == 4 * nD);
}

BOOST_AUTO_TEST_SUITE_END()
