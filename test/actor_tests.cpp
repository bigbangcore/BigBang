// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "message/actor.h"

#include <atomic>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <thread>

#include "docker/config.h"
#include "docker/docker.h"
#include "lockfree/queue.h"
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
INITIALIZE_MESSAGE_STATIC_VAR(CTestMessageA, "messageA");
atomic<uint> CTestMessageA::nPublish;
atomic<uint> CTestMessageA::nHandled;

struct CTestMessageB : public CMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CTestMessageB);
    string strB;
    static atomic<uint> nPublish;
    static atomic<uint> nHandled;
};
INITIALIZE_MESSAGE_STATIC_VAR(CTestMessageB, "messageB");
atomic<uint> CTestMessageB::nPublish;
atomic<uint> CTestMessageB::nHandled;

struct CTestMessageC : public CTestMessageB
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CTestMessageC);
    string strC;
    static atomic<uint> nPublish;
    static atomic<uint> nHandled;
};
INITIALIZE_MESSAGE_STATIC_VAR(CTestMessageC, "messageC");
atomic<uint> CTestMessageC::nPublish;
atomic<uint> CTestMessageC::nHandled;

struct CTestMessageD : public CTestMessageB
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CTestMessageD);
    string strD;
    static atomic<uint> nPublish;
    static atomic<uint> nHandled;
};
INITIALIZE_MESSAGE_STATIC_VAR(CTestMessageD, "messageD");
atomic<uint> CTestMessageD::nPublish;
atomic<uint> CTestMessageD::nHandled;

class CActorA : public CIOActor
{
public:
    CActorA()
      : CIOActor("actorA"), nHandled(0) {}
    virtual bool HandleInitialize() override
    {
        RegisterHandler<CTestMessageA>(boost::bind(&CActorA::HandlerMessageA, this, _1));
        RegisterHandler<CTestMessageC>(boost::bind(&CActorA::HandlerMessageC, this, _1));
        return true;
    }

    atomic<int> nHandled;

protected:
    void HandlerMessageA(const CTestMessageA& msg)
    {
        // cout << "Actor A handle message A as CTestMessageA: " << msg.strA << endl;
        CTestMessageA::nHandled++;
        nHandled++;
    }
    void HandlerMessageC(const CTestMessageB& msg)
    {
        // cout << "Actor A handle message C as CTestMessageB: " << msg.strB << endl;
        CTestMessageC::nHandled++;
        nHandled++;
    }
};

class CActorB : public CIOActor
{
public:
    CActorB()
      : CIOActor("actorB"), nHandled(0) {}
    virtual bool HandleInitialize() override
    {
        RegisterHandler<CTestMessageA>(boost::bind(&CActorB::HandlerMessage, this, _1));
        RegisterHandler<CTestMessageB>(boost::bind(&CActorB::HandlerMessage, this, _1));
        RegisterHandler<CTestMessageC>(boost::bind(&CActorB::HandlerMessage, this, _1));
        RegisterHandler<CTestMessageD>(boost::bind(&CActorB::HandlerMessage, this, _1));
        return true;
    }

    atomic<int> nHandled;

protected:
    void HandlerMessage(const CMessage& msg)
    {
        if (msg.Type() == CTestMessageA::nType)
        {
            // cout << "Actor B handle message A as CMessage" << dynamic_cast<const CTestMessageB&>(msg).strB << endl;
            CTestMessageA::nHandled++;
        }
        else if (msg.Type() == CTestMessageB::nType)
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
        nHandled++;
    }
};

template <typename T>
vector<shared_ptr<CMessage>> GenerateMessages(int n)
{
    vector<shared_ptr<CMessage>> vecMessage;
    vecMessage.reserve(n);
    for (int i = 0; i < n; i++)
    {
        shared_ptr<T> spT(new T);
        vecMessage.push_back(spT);
    }
    return vecMessage;
}

template <typename T>
vector<CMessage*> GenerateMessagePtr(int n)
{
    vector<CMessage*> vecMessage;
    vecMessage.reserve(n);
    for (int i = 0; i < n; i++)
    {
        vecMessage.push_back(new T);
    }
    return vecMessage;
}

BOOST_AUTO_TEST_CASE(basic)
{
    CTestMessageA::nHandled = CTestMessageA::nPublish = 0;
    CTestMessageB::nHandled = CTestMessageB::nPublish = 0;
    CTestMessageC::nHandled = CTestMessageC::nPublish = 0;
    CTestMessageD::nHandled = CTestMessageD::nPublish = 0;

    CDocker docker;
    docker.Initialize(new CConfig);

    CActorA* pActorA = new CActorA;
    docker.Attach(pActorA);

    CActorB* pActorB = new CActorB;
    docker.Attach(pActorB);

    docker.Run();

    auto Publish = [=](const vector<CMessage*>& vecA, const vector<CMessage*>& vecB,
                       const vector<CMessage*>& vecC, const vector<CMessage*>& vecD) {
        for (const auto& spA : vecA)
        {
            PUBLISH_MESSAGE(spA);
            CTestMessageA::nPublish++;
        }

        for (const auto& spB : vecB)
        {
            PUBLISH_MESSAGE(spB);
            CTestMessageB::nPublish++;
        }

        for (const auto& spC : vecC)
        {
            PUBLISH_MESSAGE(spC);
            CTestMessageC::nPublish++;
        }

        for (const auto& spD : vecD)
        {
            PUBLISH_MESSAGE(spD);
            CTestMessageD::nPublish++;
        }
    };

    int nA = 50000, nB = 50000, nC = 50000, nD = 50000;
    vector<CMessage*> vecA1 = GenerateMessagePtr<CTestMessageA>(nA);
    vector<CMessage*> vecB1 = GenerateMessagePtr<CTestMessageB>(nB);
    vector<CMessage*> vecC1 = GenerateMessagePtr<CTestMessageC>(nC);
    vector<CMessage*> vecD1 = GenerateMessagePtr<CTestMessageD>(nD);
    vector<CMessage*> vecA2 = GenerateMessagePtr<CTestMessageA>(nA);
    vector<CMessage*> vecB2 = GenerateMessagePtr<CTestMessageB>(nB);
    vector<CMessage*> vecC2 = GenerateMessagePtr<CTestMessageC>(nC);
    vector<CMessage*> vecD2 = GenerateMessagePtr<CTestMessageD>(nD);
    vector<CMessage*> vecA3 = GenerateMessagePtr<CTestMessageA>(nA);
    vector<CMessage*> vecB3 = GenerateMessagePtr<CTestMessageB>(nB);
    vector<CMessage*> vecC3 = GenerateMessagePtr<CTestMessageC>(nC);
    vector<CMessage*> vecD3 = GenerateMessagePtr<CTestMessageD>(nD);
    vector<CMessage*> vecA4 = GenerateMessagePtr<CTestMessageA>(nA);
    vector<CMessage*> vecB4 = GenerateMessagePtr<CTestMessageB>(nB);
    vector<CMessage*> vecC4 = GenerateMessagePtr<CTestMessageC>(nC);
    vector<CMessage*> vecD4 = GenerateMessagePtr<CTestMessageD>(nD);

    auto begin = chrono::steady_clock::now();
    thread th1(Publish, cref(vecA1), cref(vecB1), cref(vecC1), cref(vecD1));
    thread th2(Publish, cref(vecA2), cref(vecB2), cref(vecC2), cref(vecD2));
    thread th3(Publish, cref(vecA3), cref(vecB3), cref(vecC3), cref(vecD3));
    thread th4(Publish, cref(vecA4), cref(vecB4), cref(vecC4), cref(vecD4));
    th1.join();
    th2.join();
    th3.join();
    th4.join();

    int shouldHandledA = 4 * (nA + nC);
    int shouldHandledB = 4 * (nA + nB + nC + nD);
    while (pActorA->nHandled != shouldHandledA || pActorB->nHandled != shouldHandledB)
    {
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    auto end = chrono::steady_clock::now();
    uint nTotal = CTestMessageA::nHandled + CTestMessageB::nHandled + CTestMessageC::nHandled + CTestMessageD::nHandled;
    cout << "4 publishers and 2 actors handled " << nTotal << " times, use time: " << (end - begin).count() << " ns. Average: " << (end - begin).count() / nTotal << "ns." << endl;
    docker.Exit();

    BOOST_CHECK(CTestMessageA::nHandled == 2 * CTestMessageA::nPublish && CTestMessageA::nHandled == 4 * 2 * nA);
    BOOST_CHECK(CTestMessageB::nHandled == CTestMessageB::nPublish && CTestMessageB::nHandled == 4 * nB);
    BOOST_CHECK(CTestMessageC::nHandled == 2 * CTestMessageC::nPublish && CTestMessageC::nHandled == 4 * 2 * nC);
    BOOST_CHECK(CTestMessageD::nHandled == CTestMessageD::nPublish && CTestMessageD::nHandled == 4 * nD);
    cout << "CTestMessageA::strTag: " << CTestMessageA::strTag << endl;
    BOOST_CHECK(CTestMessageA::strTag == CTestMessageA().Tag() && CTestMessageA::strTag == "messageA");
    BOOST_CHECK(CTestMessageB::strTag == CTestMessageB().Tag() && CTestMessageB::strTag == "messageB");
    BOOST_CHECK(CTestMessageC::strTag == CTestMessageC().Tag() && CTestMessageC::strTag == "messageC");
    BOOST_CHECK(CTestMessageD::strTag == CTestMessageD().Tag() && CTestMessageD::strTag == "messageD");
}

template <typename T>
void Handle(LockFreeQueue<T>& queue, const atomic<bool>& fStop, atomic<int>& nHandled)
{
    T spMessage = nullptr;
    while (!fStop)
    {
        while ((spMessage = queue.Pop()))
        {
            if (spMessage->Type() == CTestMessageA::nType)
            {
                CTestMessageA::nHandled++;
            }
            else if (spMessage->Type() == CTestMessageB::nType)
            {
                CTestMessageB::nHandled++;
            }
            else if (spMessage->Type() == CTestMessageC::nType)
            {
                CTestMessageC::nHandled++;
            }
            else if (spMessage->Type() == CTestMessageD::nType)
            {
                CTestMessageD::nHandled++;
            }
            else
            {
                cout << "Unknown CMessage: " << spMessage->Type() << endl;
            }
            nHandled++;
        }
        this_thread::sleep_for(chrono::milliseconds(1));
    }
};

template <typename T>
void Publish(LockFreeQueue<T>& queue,
             const vector<T>& vecA, const vector<T>& vecB,
             const vector<T>& vecC, const vector<T>& vecD)
{
    for (auto& spA : vecA)
    {
        queue.Push(spA);
        CTestMessageA::nPublish++;
    }

    for (auto& spB : vecB)
    {
        queue.Push(spB);
        CTestMessageB::nPublish++;
    }

    for (auto& spC : vecC)
    {
        queue.Push(spC);
        CTestMessageC::nPublish++;
    }

    for (auto& spD : vecD)
    {
        queue.Push(spD);
        CTestMessageD::nPublish++;
    }
};

BOOST_AUTO_TEST_CASE(mpsc)
{
    atomic<bool> fStop;

    // CMessage*
    fStop = false;
    CTestMessageA::nHandled = CTestMessageA::nPublish = 0;
    CTestMessageB::nHandled = CTestMessageB::nPublish = 0;
    CTestMessageC::nHandled = CTestMessageC::nPublish = 0;
    CTestMessageD::nHandled = CTestMessageD::nPublish = 0;

    ListMPSCQueue<CMessage*> msgQueue;
    atomic<int> nMsgHandled(0);
    int nMsgA = 500000, nMsgB = 500000, nMsgC = 500000, nMsgD = 500000;
    vector<CMessage*> vecMsgA = GenerateMessagePtr<CTestMessageA>(nMsgA);
    vector<CMessage*> vecMsgB = GenerateMessagePtr<CTestMessageB>(nMsgB);
    vector<CMessage*> vecMsgC = GenerateMessagePtr<CTestMessageC>(nMsgC);
    vector<CMessage*> vecMsgD = GenerateMessagePtr<CTestMessageD>(nMsgD);

    auto begin = chrono::steady_clock::now();
    thread msgHandler(Handle<CMessage*>, ref(msgQueue), cref(fStop), ref(nMsgHandled));
    thread msgTh1(Publish<CMessage*>, ref(msgQueue), cref(vecMsgA), cref(vecMsgB), cref(vecMsgC), cref(vecMsgD));
    thread msgTh2(Publish<CMessage*>, ref(msgQueue), cref(vecMsgA), cref(vecMsgB), cref(vecMsgC), cref(vecMsgD));
    thread msgTh3(Publish<CMessage*>, ref(msgQueue), cref(vecMsgA), cref(vecMsgB), cref(vecMsgC), cref(vecMsgD));
    thread msgTh4(Publish<CMessage*>, ref(msgQueue), cref(vecMsgA), cref(vecMsgB), cref(vecMsgC), cref(vecMsgD));
    msgTh1.join();
    msgTh2.join();
    msgTh3.join();
    msgTh4.join();

    int nMsgTotal = 4 * (nMsgA + nMsgB + nMsgC + nMsgD);
    while (nMsgHandled != nMsgTotal)
    {
        // cout << "nMsgHandled: " << nMsgHandled << ", nMsgTotal: " << nMsgTotal << endl;
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    auto end = chrono::steady_clock::now();

    fStop = true;
    msgHandler.join();
    cout << "MPSC CMessage*: 4 publishers and 1 handler handled " << nMsgTotal << " times, use time: " << (end - begin).count() << " ns. Average: " << (end - begin).count() / nMsgTotal << "ns." << endl;

    BOOST_CHECK(CTestMessageA::nHandled == CTestMessageA::nPublish && CTestMessageA::nHandled == 4 * nMsgA);
    BOOST_CHECK(CTestMessageB::nHandled == CTestMessageB::nPublish && CTestMessageB::nHandled == 4 * nMsgB);
    BOOST_CHECK(CTestMessageC::nHandled == CTestMessageC::nPublish && CTestMessageC::nHandled == 4 * nMsgC);
    BOOST_CHECK(CTestMessageD::nHandled == CTestMessageD::nPublish && CTestMessageD::nHandled == 4 * nMsgD);

    // shared_ptr<CMessage>
    fStop = false;
    CTestMessageA::nHandled = CTestMessageA::nPublish = 0;
    CTestMessageB::nHandled = CTestMessageB::nPublish = 0;
    CTestMessageC::nHandled = CTestMessageC::nPublish = 0;
    CTestMessageD::nHandled = CTestMessageD::nPublish = 0;

    ListMPSCQueue<shared_ptr<CMessage>> spMsgQueue;
    atomic<int> nSpMsgHandled(0);
    int nSpMsgA = 500000, nSpMsgB = 500000, nSpMsgC = 500000, nSpMsgD = 500000;
    vector<shared_ptr<CMessage>> vecSpMsgA = GenerateMessages<CTestMessageA>(nSpMsgA);
    vector<shared_ptr<CMessage>> vecSpMsgB = GenerateMessages<CTestMessageB>(nSpMsgB);
    vector<shared_ptr<CMessage>> vecSpMsgC = GenerateMessages<CTestMessageC>(nSpMsgC);
    vector<shared_ptr<CMessage>> vecSpMsgD = GenerateMessages<CTestMessageD>(nSpMsgD);

    begin = chrono::steady_clock::now();
    thread spMsgHandler(Handle<shared_ptr<CMessage>>, ref(spMsgQueue), cref(fStop), ref(nSpMsgHandled));
    thread spMsgTh1(Publish<shared_ptr<CMessage>>, ref(spMsgQueue), cref(vecSpMsgA), cref(vecSpMsgB), cref(vecSpMsgC), cref(vecSpMsgD));
    thread spMsgTh2(Publish<shared_ptr<CMessage>>, ref(spMsgQueue), cref(vecSpMsgA), cref(vecSpMsgB), cref(vecSpMsgC), cref(vecSpMsgD));
    thread spMsgTh3(Publish<shared_ptr<CMessage>>, ref(spMsgQueue), cref(vecSpMsgA), cref(vecSpMsgB), cref(vecSpMsgC), cref(vecSpMsgD));
    thread spMsgTh4(Publish<shared_ptr<CMessage>>, ref(spMsgQueue), cref(vecSpMsgA), cref(vecSpMsgB), cref(vecSpMsgC), cref(vecSpMsgD));
    spMsgTh1.join();
    spMsgTh2.join();
    spMsgTh3.join();
    spMsgTh4.join();

    int nSpMsgTotal = 4 * (nSpMsgA + nSpMsgB + nSpMsgC + nSpMsgD);
    while (nSpMsgHandled != nSpMsgTotal)
    {
        // cout << "nSpMsgHandled: " << nSpMsgHandled << ", nSpMsgTotal: " << nSpMsgTotal << endl;
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    end = chrono::steady_clock::now();

    fStop = true;
    spMsgHandler.join();
    cout << "MPSC shared_ptr<CMessage>: 4 publishers and 1 handler handled " << nSpMsgTotal << " times, use time: " << (end - begin).count() << " ns. Average: " << (end - begin).count() / nSpMsgTotal << "ns." << endl;

    BOOST_CHECK(CTestMessageA::nHandled == CTestMessageA::nPublish && CTestMessageA::nHandled == 4 * nSpMsgA);
    BOOST_CHECK(CTestMessageB::nHandled == CTestMessageB::nPublish && CTestMessageB::nHandled == 4 * nSpMsgB);
    BOOST_CHECK(CTestMessageC::nHandled == CTestMessageC::nPublish && CTestMessageC::nHandled == 4 * nSpMsgC);
    BOOST_CHECK(CTestMessageD::nHandled == CTestMessageD::nPublish && CTestMessageD::nHandled == 4 * nSpMsgD);

    // single producer and single customer with MPSCQueue
    fStop = false;
    CTestMessageA::nHandled = CTestMessageA::nPublish = 0;
    CTestMessageB::nHandled = CTestMessageB::nPublish = 0;
    CTestMessageC::nHandled = CTestMessageC::nPublish = 0;
    CTestMessageD::nHandled = CTestMessageD::nPublish = 0;

    ListMPSCQueue<CMessage*> SPSCQueue;
    atomic<int> nSPSCHandled(0);
    int nSPSCA = 500000, nSPSCB = 500000, nSPSCC = 500000, nSPSCD = 500000;
    vector<CMessage*> vecSPSCA = GenerateMessagePtr<CTestMessageA>(nSPSCA);
    vector<CMessage*> vecSPSCB = GenerateMessagePtr<CTestMessageB>(nSPSCB);
    vector<CMessage*> vecSPSCC = GenerateMessagePtr<CTestMessageC>(nSPSCC);
    vector<CMessage*> vecSPSCD = GenerateMessagePtr<CTestMessageD>(nSPSCD);

    begin = chrono::steady_clock::now();
    thread SPSCHandler(Handle<CMessage*>, ref(SPSCQueue), cref(fStop), ref(nSPSCHandled));
    thread SPSCTh(Publish<CMessage*>, ref(SPSCQueue), cref(vecSPSCA), cref(vecSPSCB), cref(vecSPSCC), cref(vecSPSCD));
    SPSCTh.join();

    int nSPSCTotal = (nSPSCA + nSPSCB + nSPSCC + nSPSCD);
    while (nSPSCHandled != nSPSCTotal)
    {
        // cout << "nSPSCHandled: " << nSPSCHandled << ", nSPSCTotal: " << nSPSCTotal << endl;
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    end = chrono::steady_clock::now();

    fStop = true;
    SPSCHandler.join();
    cout << "MPSC CMessage*: 1 publishers and 1 handler handled " << nSPSCTotal << " times, use time: " << (end - begin).count() << " ns. Average: " << (end - begin).count() / nSPSCTotal << "ns." << endl;

    BOOST_CHECK(CTestMessageA::nHandled == CTestMessageA::nPublish && CTestMessageA::nHandled == nSPSCA);
    BOOST_CHECK(CTestMessageB::nHandled == CTestMessageB::nPublish && CTestMessageB::nHandled == nSPSCB);
    BOOST_CHECK(CTestMessageC::nHandled == CTestMessageC::nPublish && CTestMessageC::nHandled == nSPSCC);
    BOOST_CHECK(CTestMessageD::nHandled == CTestMessageD::nPublish && CTestMessageD::nHandled == nSPSCD);
}

BOOST_AUTO_TEST_SUITE_END()
