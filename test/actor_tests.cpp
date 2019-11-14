// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "actor/actor.h"

#include <atomic>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <thread>
#include <type_traits>

#include "actor/workermanager.h"
#include "docker/config.h"
#include "docker/docker.h"
#include "lockfree/queue.h"
#include "message/message.h"
#include "message/messagecenter.h"
#include "test_big.h"
// #include "message/calledmessage.h"

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

// struct CTestMessageE : public CCalledMessage<int>
// {
//     GENERATE_MESSAGE_FUNCTION(CTestMessageE);
//     CTestMessageE(int i, int& j, int k)
//         : CCalledMessage(i, j, k)
//     {
//     }
// };

class CActorA : public CActor
{
public:
    CActorA()
      : CActor("actorA"), nHandled(0) {}
    virtual bool HandleInitialize() override
    {
        RegisterHandler({
            REF_HANDLER(CTestMessageA, boost::bind(&CActorA::HandlerMessageA, this, _1), true),
            REF_HANDLER(CTestMessageC, boost::bind(&CActorA::HandlerMessageC, this, _1), true),
            // REF_HANDLER(CTestMessageE, boost::bind(&CActorA::HandlerMessageE, this, _1), true),
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
    // void HandlerMessageE(const CTestMessageE& msg)
    // {
        // cout << "Actor A handle message C as CTestMessageB: " << msg.strB << endl;
        // CTestMessageC::nHandled++;
        // nHandled++;
        // auto t = msg.spParam->Get();
        // cout << "........ " << std::tuple_size<decltype(t)>::value << endl;
        // cout << "0 param rvalue ref: " << std::is_rvalue_reference<decltype(get<0>(t))>::value << endl;
        // cout << "1 param lvalue ref: " << std::is_lvalue_reference<decltype(get<1>(t))>::value << endl;
        // cout << "2 param rvalue ref: " << std::is_rvalue_reference<decltype(get<2>(t))>::value << endl;
        // get<0>(msg.spParam->Get()) = 11;
        // get<1>(msg.spParam->Get()) = 12;
        // get<2>(msg.spParam->Get()) = 13;
    // }
};

class CActorB : public CActor
{
public:
    CActorB()
      : CActor("actorB"), nHandled(0) {}
    virtual bool HandleInitialize() override
    {
        RegisterHandler({
            PTR_HANDLER(CTestMessageA, boost::bind(&CActorB::HandlerMessage, this, _1), true),
            PTR_HANDLER(CTestMessageB, boost::bind(&CActorB::HandlerMessage, this, _1), true),
            PTR_HANDLER(CTestMessageC, boost::bind(&CActorB::HandlerMessage, this, _1), true),
            PTR_HANDLER(CTestMessageD, boost::bind(&CActorB::HandlerMessage, this, _1), true),
        });

        return true;
    }
    virtual bool HandleInvoke() override
    {
        if (!StartActor())
        {
            return false;
        }

        auto spWorkerA = manager.Add(CTestMessageA::MessageType(), make_shared<CActorWorker>());
        spWorkerA->RegisterHandler(REF_HANDLER(CTestMessageA, boost::bind(&CActorB::HandlerMessageA, this, _1), false));
        spWorkerA->Start();

        auto spWorkerB = manager.Add(CTestMessageB::MessageType(), make_shared<CActorWorker>());
        spWorkerB->RegisterHandler(REF_HANDLER(CTestMessageB, boost::bind(&CActorB::HandlerMessageB, this, _1), false));
        spWorkerB->Start();

        auto spWorkerC = manager.Add(CTestMessageC::MessageType(), make_shared<CActorWorker>());
        spWorkerC->RegisterHandler(REF_HANDLER(CTestMessageC, boost::bind(&CActorB::HandlerMessageC, this, _1), false));
        spWorkerC->Start();

        auto spWorkerD = manager.Add(CTestMessageD::MessageType(), make_shared<CActorWorker>());
        spWorkerD->RegisterHandler(REF_HANDLER(CTestMessageD, boost::bind(&CActorB::HandlerMessageD, this, _1), false));
        spWorkerD->Start();

        return true;
    }
    virtual void HandleHalt() override
    {
        manager.Clear();

        StopActor();
    }
    virtual void HandleDeinitialize() override
    {
        DeregisterHandler();
    }

    virtual bool EnterLoop() override
    {
        if (!CActor::EnterLoop())
        {
            return false;
        }

        return true;
    }

    virtual void LeaveLoop() override
    {
        CActor::LeaveLoop();
    }

    atomic<int> nHandled;
    CWorkerManager<uint32> manager;

protected:
    void HandlerMessage(const shared_ptr<CMessage> spMsg)
    {
        auto spWorker = manager.Get(spMsg->Type());
        if (spWorker)
        {
            spWorker->Publish(spMsg);
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

BOOST_AUTO_TEST_CASE(basic)
{
    CTestMessageA::nHandled = CTestMessageA::nPublish = 0;
    CTestMessageB::nHandled = CTestMessageB::nPublish = 0;
    CTestMessageC::nHandled = CTestMessageC::nPublish = 0;
    CTestMessageD::nHandled = CTestMessageD::nPublish = 0;

    InitLog(boost::filesystem::path(), severity_level::TRACE, true, true);
    CDocker docker;
    docker.Initialize(new CConfig);

    CActorA* pActorA = new CActorA;
    docker.Attach(pActorA);

    CActorB* pActorB = new CActorB;
    docker.Attach(pActorB);

    docker.Run();

    auto Publish = [=](vector<shared_ptr<CMessage>>& vecA, vector<shared_ptr<CMessage>>& vecB,
                       vector<shared_ptr<CMessage>>& vecC, vector<shared_ptr<CMessage>>& vecD) {
        for (auto& spA : vecA)
        {
            PUBLISH_MESSAGE(spA);
            CTestMessageA::nPublish++;
        }

        for (auto& spB : vecB)
        {
            PUBLISH_MESSAGE(spB);
            CTestMessageB::nPublish++;
        }

        for (auto& spC : vecC)
        {
            PUBLISH_MESSAGE(spC);
            CTestMessageC::nPublish++;
        }

        for (auto& spD : vecD)
        {
            PUBLISH_MESSAGE(spD);
            CTestMessageD::nPublish++;
        }
    };

    int nA = 50000, nB = 50000, nC = 50000, nD = 50000;
    vector<shared_ptr<CMessage>> vecA1 = GenerateMessages<CTestMessageA>(nA);
    vector<shared_ptr<CMessage>> vecB1 = GenerateMessages<CTestMessageB>(nB);
    vector<shared_ptr<CMessage>> vecC1 = GenerateMessages<CTestMessageC>(nC);
    vector<shared_ptr<CMessage>> vecD1 = GenerateMessages<CTestMessageD>(nD);
    vector<shared_ptr<CMessage>> vecA2 = GenerateMessages<CTestMessageA>(nA);
    vector<shared_ptr<CMessage>> vecB2 = GenerateMessages<CTestMessageB>(nB);
    vector<shared_ptr<CMessage>> vecC2 = GenerateMessages<CTestMessageC>(nC);
    vector<shared_ptr<CMessage>> vecD2 = GenerateMessages<CTestMessageD>(nD);
    vector<shared_ptr<CMessage>> vecA3 = GenerateMessages<CTestMessageA>(nA);
    vector<shared_ptr<CMessage>> vecB3 = GenerateMessages<CTestMessageB>(nB);
    vector<shared_ptr<CMessage>> vecC3 = GenerateMessages<CTestMessageC>(nC);
    vector<shared_ptr<CMessage>> vecD3 = GenerateMessages<CTestMessageD>(nD);
    vector<shared_ptr<CMessage>> vecA4 = GenerateMessages<CTestMessageA>(nA);
    vector<shared_ptr<CMessage>> vecB4 = GenerateMessages<CTestMessageB>(nB);
    vector<shared_ptr<CMessage>> vecC4 = GenerateMessages<CTestMessageC>(nC);
    vector<shared_ptr<CMessage>> vecD4 = GenerateMessages<CTestMessageD>(nD);

    auto begin = chrono::steady_clock::now();
    thread th1(Publish, ref(vecA1), ref(vecB1), ref(vecC1), ref(vecD1));
    thread th2(Publish, ref(vecA2), ref(vecB2), ref(vecC2), ref(vecD2));
    thread th3(Publish, ref(vecA3), ref(vecB3), ref(vecC3), ref(vecD3));
    thread th4(Publish, ref(vecA4), ref(vecB4), ref(vecC4), ref(vecD4));
    th1.join();
    th2.join();
    th3.join();
    th4.join();

    int shouldHandledA = 4 * (nA + nC);
    int shouldHandledB = 4 * (nA + nB + nC + nD);
    while (pActorA->nHandled != shouldHandledA || pActorB->nHandled != shouldHandledB)
    {
        this_thread::sleep_for(chrono::seconds(1));
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
                 vector<shared_ptr<T>>& vecA, vector<shared_ptr<T>>& vecB,
                 vector<shared_ptr<T>>& vecC, vector<shared_ptr<T>>& vecD)
{
    for (auto& spA : vecA)
    {
        queue.Push(std::move(spA));
        CTestMessageA::nPublish++;
    }

    for (auto& spB : vecB)
    {
        queue.Push(std::move(spB));
        CTestMessageB::nPublish++;
    }

    for (auto& spC : vecC)
    {
        queue.Push(std::move(spC));
        CTestMessageC::nPublish++;
    }

    for (auto& spD : vecD)
    {
        queue.Push(std::move(spD));
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

// shared_ptr<CMessage>
BOOST_AUTO_TEST_CASE(mpscptr)
{
    atomic<bool> fStop(false);
    CTestMessageA::nHandled = CTestMessageA::nPublish = 0;
    CTestMessageB::nHandled = CTestMessageB::nPublish = 0;
    CTestMessageC::nHandled = CTestMessageC::nPublish = 0;
    CTestMessageD::nHandled = CTestMessageD::nPublish = 0;

    ListMPSCQueue<CMessage> spMsgQueue;
    atomic<int> nSpMsgHandled(0);
    int nSpMsgA = 500000, nSpMsgB = 500000, nSpMsgC = 500000, nSpMsgD = 500000;
    vector<shared_ptr<CMessage>> vecSpMsgA1 = GenerateMessages<CTestMessageA>(nSpMsgA);
    vector<shared_ptr<CMessage>> vecSpMsgB1 = GenerateMessages<CTestMessageB>(nSpMsgB);
    vector<shared_ptr<CMessage>> vecSpMsgC1 = GenerateMessages<CTestMessageC>(nSpMsgC);
    vector<shared_ptr<CMessage>> vecSpMsgD1 = GenerateMessages<CTestMessageD>(nSpMsgD);
    vector<shared_ptr<CMessage>> vecSpMsgA2 = GenerateMessages<CTestMessageA>(nSpMsgA);
    vector<shared_ptr<CMessage>> vecSpMsgB2 = GenerateMessages<CTestMessageB>(nSpMsgB);
    vector<shared_ptr<CMessage>> vecSpMsgC2 = GenerateMessages<CTestMessageC>(nSpMsgC);
    vector<shared_ptr<CMessage>> vecSpMsgD2 = GenerateMessages<CTestMessageD>(nSpMsgD);
    vector<shared_ptr<CMessage>> vecSpMsgA3 = GenerateMessages<CTestMessageA>(nSpMsgA);
    vector<shared_ptr<CMessage>> vecSpMsgB3 = GenerateMessages<CTestMessageB>(nSpMsgB);
    vector<shared_ptr<CMessage>> vecSpMsgC3 = GenerateMessages<CTestMessageC>(nSpMsgC);
    vector<shared_ptr<CMessage>> vecSpMsgD3 = GenerateMessages<CTestMessageD>(nSpMsgD);
    vector<shared_ptr<CMessage>> vecSpMsgA4 = GenerateMessages<CTestMessageA>(nSpMsgA);
    vector<shared_ptr<CMessage>> vecSpMsgB4 = GenerateMessages<CTestMessageB>(nSpMsgB);
    vector<shared_ptr<CMessage>> vecSpMsgC4 = GenerateMessages<CTestMessageC>(nSpMsgC);
    vector<shared_ptr<CMessage>> vecSpMsgD4 = GenerateMessages<CTestMessageD>(nSpMsgD);

    auto begin = chrono::steady_clock::now();
    thread spMsgHandler(Handle<CMessage>, ref(spMsgQueue), cref(fStop), ref(nSpMsgHandled));
    thread spMsgTh1(PublishSPtr<CMessage>, ref(spMsgQueue), ref(vecSpMsgA1), ref(vecSpMsgB1), ref(vecSpMsgC1), ref(vecSpMsgD1));
    thread spMsgTh2(PublishSPtr<CMessage>, ref(spMsgQueue), ref(vecSpMsgA2), ref(vecSpMsgB2), ref(vecSpMsgC2), ref(vecSpMsgD2));
    thread spMsgTh3(PublishSPtr<CMessage>, ref(spMsgQueue), ref(vecSpMsgA3), ref(vecSpMsgB3), ref(vecSpMsgC3), ref(vecSpMsgD3));
    thread spMsgTh4(PublishSPtr<CMessage>, ref(spMsgQueue), ref(vecSpMsgA4), ref(vecSpMsgB4), ref(vecSpMsgC4), ref(vecSpMsgD4));
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
    thread SPSCTh(PublishSPtr<CMessage>, ref(SPSCQueue), ref(vecSPSCA), ref(vecSPSCB), ref(vecSPSCC), ref(vecSPSCD));
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
