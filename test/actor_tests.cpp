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
    GENERATE_MESSAGE_FUNCTION(CTestMessageA);
    string strA;
    static atomic<uint> nPublish;
    static atomic<uint> nHandled;
};
atomic<uint> CTestMessageA::nPublish;
atomic<uint> CTestMessageA::nHandled;

struct CTestMessageB : public CMessage
{
    GENERATE_MESSAGE_FUNCTION(CTestMessageB);
    string strB;
    static atomic<uint> nPublish;
    static atomic<uint> nHandled;
};
atomic<uint> CTestMessageB::nPublish;
atomic<uint> CTestMessageB::nHandled;

struct CTestMessageC : public CTestMessageB
{
    GENERATE_MESSAGE_FUNCTION(CTestMessageC);
    string strC;
    static atomic<uint> nPublish;
    static atomic<uint> nHandled;
};
atomic<uint> CTestMessageC::nPublish;
atomic<uint> CTestMessageC::nHandled;

struct CTestMessageD : public CTestMessageB
{
    GENERATE_MESSAGE_FUNCTION(CTestMessageD);
    string strD;
    static atomic<uint> nPublish;
    static atomic<uint> nHandled;
};
atomic<uint> CTestMessageD::nPublish;
atomic<uint> CTestMessageD::nHandled;

class CActorA : public CIOActor
{
public:
    CActorA()
      : CIOActor("actorA"), nHandled(0) {}
    virtual bool HandleInitialize() override
    {
        RegisterRefHandler<CTestMessageA>(boost::bind(&CActorA::HandlerMessageA, this, _1));
        RegisterRefHandler<CTestMessageC>(boost::bind(&CActorA::HandlerMessageC, this, _1));
        return true;
    }
    virtual void HandleDeinitialize() override
    {
        DeregisterHandler(CTestMessageA::MessageType());
        DeregisterHandler(CTestMessageC::MessageType());
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
        RegisterPtrHandler<CTestMessageA>(boost::bind(&CActorB::HandlerMessage, this, _1));
        RegisterPtrHandler<CTestMessageB>(boost::bind(&CActorB::HandlerMessage, this, _1));
        RegisterPtrHandler<CTestMessageC>(boost::bind(&CActorB::HandlerMessage, this, _1));
        RegisterPtrHandler<CTestMessageD>(boost::bind(&CActorB::HandlerMessage, this, _1));
        return true;
    }
    virtual void HandleDeinitialize() override
    {
        DeregisterHandler(CTestMessageA::MessageType());
        DeregisterHandler(CTestMessageB::MessageType());
        DeregisterHandler(CTestMessageC::MessageType());
        DeregisterHandler(CTestMessageD::MessageType());
    }

    virtual void EnterLoop() override
    {
        CIOActor::EnterLoop();
        CIOActorWorker* pWorkerA(new CIOActorWorker);
        pWorkerA->RegisterRefHandler<CTestMessageA>(boost::bind(&CActorB::HandlerMessageA, this, _1));
        mapWorkers[CTestMessageA::MessageType()] = pWorkerA;
        ThreadStart(pWorkerA->GetThread());

        CIOActorWorker* pWorkerB(new CIOActorWorker);
        pWorkerB->RegisterRefHandler<CTestMessageB>(boost::bind(&CActorB::HandlerMessageB, this, _1));
        mapWorkers[CTestMessageB::MessageType()] = pWorkerB;
        ThreadStart(pWorkerB->GetThread());

        CIOActorWorker* pWorkerC(new CIOActorWorker);
        pWorkerC->RegisterRefHandler<CTestMessageC>(boost::bind(&CActorB::HandlerMessageC, this, _1));
        mapWorkers[CTestMessageC::MessageType()] = pWorkerC;
        ThreadStart(pWorkerC->GetThread());

        CIOActorWorker* pWorkerD(new CIOActorWorker);
        pWorkerD->RegisterRefHandler<CTestMessageD>(boost::bind(&CActorB::HandlerMessageD, this, _1));
        mapWorkers[CTestMessageD::MessageType()] = pWorkerD;
        ThreadStart(pWorkerD->GetThread());
    }

    virtual void LeaveLoop() override
    {
        for (auto it = mapWorkers.begin(); it != mapWorkers.end(); it++)
        {
            it->second->Stop();
            delete it->second;
        }
        mapWorkers.clear();
        CIOActor::LeaveLoop();
    }

    atomic<int> nHandled;
    map<uint32, CIOActorWorker*> mapWorkers;

protected:
    void HandlerMessage(const shared_ptr<CMessage>& spMsg)
    {
        if (spMsg->Type() == CTestMessageA::MessageType())
        {
            // CTestMessageA::nHandled++;
            mapWorkers[spMsg->Type()]->Publish(spMsg);
        }
        else if (spMsg->Type() == CTestMessageB::MessageType())
        {
            // CTestMessageB::nHandled++;
            mapWorkers[spMsg->Type()]->Publish(spMsg);
        }
        else if (spMsg->Type() == CTestMessageC::MessageType())
        {
            // CTestMessageC::nHandled++;
            mapWorkers[spMsg->Type()]->Publish(spMsg);
        }
        else if (spMsg->Type() == CTestMessageD::MessageType())
        {
            // CTestMessageD::nHandled++;
            mapWorkers[spMsg->Type()]->Publish(spMsg);
        }
        else
        {
            cout << "Unknown CMessage: " << spMsg->Type() << endl;
        }
    }

    void HandlerMessageA(const CTestMessageA& msg)
    {
        CTestMessageA::nHandled++;
        nHandled++;
    }

    void HandlerMessageB(const CTestMessageB& msg)
    {
        CTestMessageB::nHandled++;
        nHandled++;
    }

    void HandlerMessageC(const CTestMessageC& msg)
    {
        CTestMessageC::nHandled++;
        nHandled++;
    }

    void HandlerMessageD(const CTestMessageD& msg)
    {
        CTestMessageD::nHandled++;
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
        vecMessage.push_back(T::Create());
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
    BOOST_CHECK(CTestMessageA::MessageTag() == CTestMessageA().Tag() && CTestMessageA::MessageTag() == "CTestMessageA");
    BOOST_CHECK(CTestMessageB::MessageTag() == CTestMessageB().Tag() && CTestMessageB::MessageTag() == "CTestMessageB");
    BOOST_CHECK(CTestMessageC::MessageTag() == CTestMessageC().Tag() && CTestMessageC::MessageTag() == "CTestMessageC");
    BOOST_CHECK(CTestMessageD::MessageTag() == CTestMessageD().Tag() && CTestMessageD::MessageTag() == "CTestMessageD");
}

template <typename T>
void Handle(ListMPSCQueue<T>& queue, const atomic<bool>& fStop, atomic<int>& nHandled)
{
    shared_ptr<T> spMessage = nullptr;
    while (!fStop)
    {
        while ((spMessage = queue.Pop()))
        {
            if (spMessage->Type() == CTestMessageA::MessageType())
            {
                CTestMessageA::nHandled++;
            }
            else if (spMessage->Type() == CTestMessageB::MessageType())
            {
                CTestMessageB::nHandled++;
            }
            else if (spMessage->Type() == CTestMessageC::MessageType())
            {
                CTestMessageC::nHandled++;
            }
            else if (spMessage->Type() == CTestMessageD::MessageType())
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
void PublishSPtr(ListMPSCQueue<T>& queue,
                 const vector<shared_ptr<T>>& vecA, const vector<shared_ptr<T>>& vecB,
                 const vector<shared_ptr<T>>& vecC, const vector<shared_ptr<T>>& vecD)
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

template <typename T>
void PublishPtr(ListMPSCQueue<T>& queue, const vector<T*>& vec)
{
    for (auto& p : vec)
    {
        if (p->Type() == CTestMessageA::MessageType())
        {
            CTestMessageA::nPublish++;
        }
        else if (p->Type() == CTestMessageB::MessageType())
        {
            CTestMessageB::nPublish++;
        }
        else if (p->Type() == CTestMessageC::MessageType())
        {
            CTestMessageC::nPublish++;
        }
        else if (p->Type() == CTestMessageD::MessageType())
        {
            CTestMessageD::nPublish++;
        }
        queue.Push(p);
    }
};

// CMessage*
BOOST_AUTO_TEST_CASE(mpscptr)
{
    atomic<bool> fStop(false);
    CTestMessageA::nHandled = CTestMessageA::nPublish = 0;
    CTestMessageB::nHandled = CTestMessageB::nPublish = 0;
    CTestMessageC::nHandled = CTestMessageC::nPublish = 0;
    CTestMessageD::nHandled = CTestMessageD::nPublish = 0;

    ListMPSCQueue<CMessage> msgQueue;
    atomic<int> nMsgHandled(0);
    int nMsgA = 500000, nMsgB = 500000, nMsgC = 500000, nMsgD = 500000;
    vector<CMessage*> vecMsgA = GenerateMessagePtr<CTestMessageA>(nMsgA);
    vector<CMessage*> vecMsgB = GenerateMessagePtr<CTestMessageB>(nMsgB);
    vector<CMessage*> vecMsgC = GenerateMessagePtr<CTestMessageC>(nMsgC);
    vector<CMessage*> vecMsgD = GenerateMessagePtr<CTestMessageD>(nMsgD);

    auto begin = chrono::steady_clock::now();
    thread msgHandler(Handle<CMessage>, ref(msgQueue), cref(fStop), ref(nMsgHandled));
    thread msgTh1(PublishPtr<CMessage>, ref(msgQueue), cref(vecMsgA));
    thread msgTh2(PublishPtr<CMessage>, ref(msgQueue), cref(vecMsgB));
    thread msgTh3(PublishPtr<CMessage>, ref(msgQueue), cref(vecMsgC));
    thread msgTh4(PublishPtr<CMessage>, ref(msgQueue), cref(vecMsgD));
    msgTh1.join();
    msgTh2.join();
    msgTh3.join();
    msgTh4.join();

    int nMsgTotal = (nMsgA + nMsgB + nMsgC + nMsgD);
    while (nMsgHandled != nMsgTotal)
    {
        // cout << "nMsgHandled: " << nMsgHandled << ", nMsgTotal: " << nMsgTotal << endl;
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    auto end = chrono::steady_clock::now();

    fStop = true;
    msgHandler.join();
    cout << "MPSC CMessage*: 4 publishers and 1 handler handled " << nMsgTotal << " times, use time: " << (end - begin).count() << " ns. Average: " << (end - begin).count() / nMsgTotal << "ns." << endl;

    BOOST_CHECK(CTestMessageA::nHandled == CTestMessageA::nPublish && CTestMessageA::nHandled == nMsgA);
    BOOST_CHECK(CTestMessageB::nHandled == CTestMessageB::nPublish && CTestMessageB::nHandled == nMsgB);
    BOOST_CHECK(CTestMessageC::nHandled == CTestMessageC::nPublish && CTestMessageC::nHandled == nMsgC);
    BOOST_CHECK(CTestMessageD::nHandled == CTestMessageD::nPublish && CTestMessageD::nHandled == nMsgD);
}

// shared_ptr<CMessage>
BOOST_AUTO_TEST_CASE(mpscsptr)
{
    atomic<bool> fStop(false);
    CTestMessageA::nHandled = CTestMessageA::nPublish = 0;
    CTestMessageB::nHandled = CTestMessageB::nPublish = 0;
    CTestMessageC::nHandled = CTestMessageC::nPublish = 0;
    CTestMessageD::nHandled = CTestMessageD::nPublish = 0;

    ListMPSCQueue<CMessage> spMsgQueue;
    atomic<int> nSpMsgHandled(0);
    int nSpMsgA = 500000, nSpMsgB = 500000, nSpMsgC = 500000, nSpMsgD = 500000;
    vector<shared_ptr<CMessage>> vecSpMsgA = GenerateMessages<CTestMessageA>(nSpMsgA);
    vector<shared_ptr<CMessage>> vecSpMsgB = GenerateMessages<CTestMessageB>(nSpMsgB);
    vector<shared_ptr<CMessage>> vecSpMsgC = GenerateMessages<CTestMessageC>(nSpMsgC);
    vector<shared_ptr<CMessage>> vecSpMsgD = GenerateMessages<CTestMessageD>(nSpMsgD);

    auto begin = chrono::steady_clock::now();
    thread spMsgHandler(Handle<CMessage>, ref(spMsgQueue), cref(fStop), ref(nSpMsgHandled));
    thread spMsgTh1(PublishSPtr<CMessage>, ref(spMsgQueue), cref(vecSpMsgA), cref(vecSpMsgB), cref(vecSpMsgC), cref(vecSpMsgD));
    thread spMsgTh2(PublishSPtr<CMessage>, ref(spMsgQueue), cref(vecSpMsgA), cref(vecSpMsgB), cref(vecSpMsgC), cref(vecSpMsgD));
    thread spMsgTh3(PublishSPtr<CMessage>, ref(spMsgQueue), cref(vecSpMsgA), cref(vecSpMsgB), cref(vecSpMsgC), cref(vecSpMsgD));
    thread spMsgTh4(PublishSPtr<CMessage>, ref(spMsgQueue), cref(vecSpMsgA), cref(vecSpMsgB), cref(vecSpMsgC), cref(vecSpMsgD));
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
    auto end = chrono::steady_clock::now();

    fStop = true;
    spMsgHandler.join();
    cout << "MPSC shared_ptr<CMessage>: 4 publishers and 1 handler handled " << nSpMsgTotal << " times, use time: " << (end - begin).count() << " ns. Average: " << (end - begin).count() / nSpMsgTotal << "ns." << endl;

    BOOST_CHECK(CTestMessageA::nHandled == CTestMessageA::nPublish && CTestMessageA::nHandled == 4 * nSpMsgA);
    BOOST_CHECK(CTestMessageB::nHandled == CTestMessageB::nPublish && CTestMessageB::nHandled == 4 * nSpMsgB);
    BOOST_CHECK(CTestMessageC::nHandled == CTestMessageC::nPublish && CTestMessageC::nHandled == 4 * nSpMsgC);
    BOOST_CHECK(CTestMessageD::nHandled == CTestMessageD::nPublish && CTestMessageD::nHandled == 4 * nSpMsgD);
}

// single producer and single customer with MPSCQueue
BOOST_AUTO_TEST_CASE(spsc)
{
    atomic<bool> fStop(false);
    CTestMessageA::nHandled = CTestMessageA::nPublish = 0;
    CTestMessageB::nHandled = CTestMessageB::nPublish = 0;
    CTestMessageC::nHandled = CTestMessageC::nPublish = 0;
    CTestMessageD::nHandled = CTestMessageD::nPublish = 0;

    ListMPSCQueue<CMessage> SPSCQueue;
    atomic<int> nSPSCHandled(0);
    int nSPSCA = 500000, nSPSCB = 500000, nSPSCC = 500000, nSPSCD = 500000;
    vector<shared_ptr<CMessage>> vecSPSCA = GenerateMessages<CTestMessageA>(nSPSCA);
    vector<shared_ptr<CMessage>> vecSPSCB = GenerateMessages<CTestMessageB>(nSPSCB);
    vector<shared_ptr<CMessage>> vecSPSCC = GenerateMessages<CTestMessageC>(nSPSCC);
    vector<shared_ptr<CMessage>> vecSPSCD = GenerateMessages<CTestMessageD>(nSPSCD);

    auto begin = chrono::steady_clock::now();
    thread SPSCHandler(Handle<CMessage>, ref(SPSCQueue), cref(fStop), ref(nSPSCHandled));
    thread SPSCTh(PublishSPtr<CMessage>, ref(SPSCQueue), cref(vecSPSCA), cref(vecSPSCB), cref(vecSPSCC), cref(vecSPSCD));
    SPSCTh.join();

    int nSPSCTotal = (nSPSCA + nSPSCB + nSPSCC + nSPSCD);
    while (nSPSCHandled != nSPSCTotal)
    {
        // cout << "nSPSCHandled: " << nSPSCHandled << ", nSPSCTotal: " << nSPSCTotal << endl;
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    auto end = chrono::steady_clock::now();

    fStop = true;
    SPSCHandler.join();
    cout << "MPSC CMessage*: 1 publishers and 1 handler handled " << nSPSCTotal << " times, use time: " << (end - begin).count() << " ns. Average: " << (end - begin).count() / nSPSCTotal << "ns." << endl;

    BOOST_CHECK(CTestMessageA::nHandled == CTestMessageA::nPublish && CTestMessageA::nHandled == nSPSCA);
    BOOST_CHECK(CTestMessageB::nHandled == CTestMessageB::nPublish && CTestMessageB::nHandled == nSPSCB);
    BOOST_CHECK(CTestMessageC::nHandled == CTestMessageC::nPublish && CTestMessageC::nHandled == nSPSCC);
    BOOST_CHECK(CTestMessageD::nHandled == CTestMessageD::nPublish && CTestMessageD::nHandled == nSPSCD);
}

BOOST_AUTO_TEST_SUITE_END()
