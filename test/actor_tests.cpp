// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "actor/actor.h"

#include <atomic>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <string>
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
    DECLARE_PUBLISHED_MESSAGE_FUNCTION(CTestMessageA);
    string strA;
    static atomic<uint> nPublish;
    static atomic<uint> nHandled;
};
atomic<uint> CTestMessageA::nPublish;
atomic<uint> CTestMessageA::nHandled;

struct CTestMessageB : public CMessage
{
    DECLARE_PUBLISHED_MESSAGE_FUNCTION(CTestMessageB);
    string strB;
    static atomic<uint> nPublish;
    static atomic<uint> nHandled;
};
atomic<uint> CTestMessageB::nPublish;
atomic<uint> CTestMessageB::nHandled;

class CActorA : public CActor
{
public:
    CActorA()
      : CActor("actorA"), nHandled(0) {}
    virtual bool HandleInitialize() override
    {
        RegisterHandler({
            PTR_HANDLER(CTestMessageA, boost::bind(&CActorA::HandlerMessageA, this, _1), true),
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
    void HandlerMessageA(const shared_ptr<CTestMessageA>& spMsg)
    {
        // cout << "Actor A handle message A as CTestMessageA: " << spMsg->strA << endl;
        CTestMessageA::nHandled++;
        nHandled++;
    }
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
        spWorkerA->RegisterHandler(PTR_HANDLER(CTestMessageA, boost::bind(&CActorB::HandlerMessageA, this, _1), false));
        spWorkerA->Start();

        auto spWorkerB = manager.Add(CTestMessageB::MessageType(), make_shared<CActorWorker>());
        spWorkerB->RegisterHandler(PTR_HANDLER(CTestMessageB, boost::bind(&CActorB::HandlerMessageB, this, _1), false));
        spWorkerB->Start();

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
    void HandlerMessage(const shared_ptr<CMessage>& spMsg)
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

    void HandlerMessageA(const shared_ptr<CTestMessageA>& spMsg)
    {
        CTestMessageA::nHandled++;
        nHandled++;
    }

    void HandlerMessageB(const shared_ptr<CTestMessageB>& spMsg)
    {
        CTestMessageB::nHandled++;
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

BOOST_AUTO_TEST_CASE(publish_message_center)
{
    CTestMessageA::nHandled = CTestMessageA::nPublish = 0;
    CTestMessageB::nHandled = CTestMessageB::nPublish = 0;

    InitLog(boost::filesystem::path(), severity_level::TRACE, true, true);
    CDocker docker;
    docker.Initialize(new CConfig);

    CActorA* pActorA = new CActorA;
    docker.Attach(pActorA);

    CActorB* pActorB = new CActorB;
    docker.Attach(pActorB);

    docker.Run();

    auto Publish = [=](vector<shared_ptr<CMessage>>& vecA, vector<shared_ptr<CMessage>>& vecB) {
        for (auto& spA : vecA)
        {
            PUBLISH(spA);
            CTestMessageA::nPublish++;
        }

        for (auto& spB : vecB)
        {
            PUBLISH(spB);
            CTestMessageB::nPublish++;
        }
    };

    int nA = 50000, nB = 50000;
    vector<shared_ptr<CMessage>> vecA1 = GenerateMessages<CTestMessageA>(nA);
    vector<shared_ptr<CMessage>> vecB1 = GenerateMessages<CTestMessageB>(nB);
    vector<shared_ptr<CMessage>> vecA2 = GenerateMessages<CTestMessageA>(nA);
    vector<shared_ptr<CMessage>> vecB2 = GenerateMessages<CTestMessageB>(nB);
    vector<shared_ptr<CMessage>> vecA3 = GenerateMessages<CTestMessageA>(nA);
    vector<shared_ptr<CMessage>> vecB3 = GenerateMessages<CTestMessageB>(nB);
    vector<shared_ptr<CMessage>> vecA4 = GenerateMessages<CTestMessageA>(nA);
    vector<shared_ptr<CMessage>> vecB4 = GenerateMessages<CTestMessageB>(nB);

    auto begin = chrono::steady_clock::now();
    thread th1(Publish, ref(vecA1), ref(vecB1));
    thread th2(Publish, ref(vecA2), ref(vecB2));
    thread th3(Publish, ref(vecA3), ref(vecB3));
    thread th4(Publish, ref(vecA4), ref(vecB4));
    th1.join();
    th2.join();
    th3.join();
    th4.join();

    int shouldHandledA = 4 * nA;
    int shouldHandledB = 4 * (nA + nB);
    while (pActorA->nHandled != shouldHandledA || pActorB->nHandled != shouldHandledB)
    {
        this_thread::sleep_for(chrono::seconds(1));
    }
    auto end = chrono::steady_clock::now();
    uint nTotal = CTestMessageA::nHandled + CTestMessageB::nHandled;
    cout << "4 publishers and 2 actors handled " << nTotal << " times, use time: " << (end - begin).count() << " ns. Average: " << (end - begin).count() / nTotal << "ns." << endl;
    docker.Exit();

    BOOST_CHECK(CTestMessageA::nHandled == 2 * CTestMessageA::nPublish && CTestMessageA::nHandled == 4 * 2 * nA);
    BOOST_CHECK(CTestMessageB::nHandled == CTestMessageB::nPublish && CTestMessageB::nHandled == 4 * nB);
    BOOST_CHECK(CTestMessageA::MessageTag() == CTestMessageA().Tag() && CTestMessageA::MessageTag() == "CTestMessageA");
    BOOST_CHECK(CTestMessageB::MessageTag() == CTestMessageB().Tag() && CTestMessageB::MessageTag() == "CTestMessageB");
}

template <typename T>
void HandleQueue(ListMPSCQueue<T>& queue, const atomic<bool>& fStop, atomic<int>& nHandled)
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
void PublishQueue(ListMPSCQueue<T>& queue,
                  vector<shared_ptr<T>>& vecA, vector<shared_ptr<T>>& vecB)
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
};

// shared_ptr<CMessage>
BOOST_AUTO_TEST_CASE(mpscptr)
{
    atomic<bool> fStop(false);
    CTestMessageA::nHandled = CTestMessageA::nPublish = 0;
    CTestMessageB::nHandled = CTestMessageB::nPublish = 0;

    ListMPSCQueue<CMessage> spMsgQueue;
    atomic<int> nSpMsgHandled(0);
    int nSpMsgA = 500000, nSpMsgB = 500000;
    vector<shared_ptr<CMessage>> vecSpMsgA1 = GenerateMessages<CTestMessageA>(nSpMsgA);
    vector<shared_ptr<CMessage>> vecSpMsgB1 = GenerateMessages<CTestMessageB>(nSpMsgB);
    vector<shared_ptr<CMessage>> vecSpMsgA2 = GenerateMessages<CTestMessageA>(nSpMsgA);
    vector<shared_ptr<CMessage>> vecSpMsgB2 = GenerateMessages<CTestMessageB>(nSpMsgB);
    vector<shared_ptr<CMessage>> vecSpMsgA3 = GenerateMessages<CTestMessageA>(nSpMsgA);
    vector<shared_ptr<CMessage>> vecSpMsgB3 = GenerateMessages<CTestMessageB>(nSpMsgB);
    vector<shared_ptr<CMessage>> vecSpMsgA4 = GenerateMessages<CTestMessageA>(nSpMsgA);
    vector<shared_ptr<CMessage>> vecSpMsgB4 = GenerateMessages<CTestMessageB>(nSpMsgB);

    auto begin = chrono::steady_clock::now();
    thread spMsgHandler(HandleQueue<CMessage>, ref(spMsgQueue), cref(fStop), ref(nSpMsgHandled));
    thread spMsgTh1(PublishQueue<CMessage>, ref(spMsgQueue), ref(vecSpMsgA1), ref(vecSpMsgB1));
    thread spMsgTh2(PublishQueue<CMessage>, ref(spMsgQueue), ref(vecSpMsgA2), ref(vecSpMsgB2));
    thread spMsgTh3(PublishQueue<CMessage>, ref(spMsgQueue), ref(vecSpMsgA3), ref(vecSpMsgB3));
    thread spMsgTh4(PublishQueue<CMessage>, ref(spMsgQueue), ref(vecSpMsgA4), ref(vecSpMsgB4));
    spMsgTh1.join();
    spMsgTh2.join();
    spMsgTh3.join();
    spMsgTh4.join();

    int nSpMsgTotal = 4 * (nSpMsgA + nSpMsgB);
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
}

// single producer and single customer with MPSCQueue
BOOST_AUTO_TEST_CASE(spsc)
{
    atomic<bool> fStop(false);
    CTestMessageA::nHandled = CTestMessageA::nPublish = 0;
    CTestMessageB::nHandled = CTestMessageB::nPublish = 0;

    ListMPSCQueue<CMessage> SPSCQueue;
    atomic<int> nSPSCHandled(0);
    int nSPSCA = 500000, nSPSCB = 500000;
    vector<shared_ptr<CMessage>> vecSPSCA = GenerateMessages<CTestMessageA>(nSPSCA);
    vector<shared_ptr<CMessage>> vecSPSCB = GenerateMessages<CTestMessageB>(nSPSCB);

    auto begin = chrono::steady_clock::now();
    thread SPSCHandler(HandleQueue<CMessage>, ref(SPSCQueue), cref(fStop), ref(nSPSCHandled));
    thread SPSCTh(PublishQueue<CMessage>, ref(SPSCQueue), ref(vecSPSCA), ref(vecSPSCB));
    SPSCTh.join();

    int nSPSCTotal = (nSPSCA + nSPSCB);
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
}

namespace actor_tests
{
DECLARE_CALLED_MESSAGE_CLASS(FunDefaultResult,
                             RETURN(int, -1),
                             PARAM(int, i));

DECLARE_CALLED_MESSAGE_CLASS(FunVoidResult,
                             RETURN(void),
                             PARAM(string&, str),
                             PARAM(const int, i, 15));

DECLARE_CALLED_MESSAGE_CLASS(FunDefaultParam,
                             RETURN(int),
                             PARAM(int, i),
                             PARAM(const string&, str, "123"));

DECLARE_CALLED_MESSAGE_CLASS(FunReference,
                             RETURN(bool),
                             PARAM(int&, i),
                             PARAM(string&, str));
} // namespace actor_tests

class CActorC : public CActor
{
public:
    CActorC()
      : CActor("actorC") {}
    virtual bool HandleInitialize() override
    {
        RegisterHandler({
            FUN_HANDLER(actor_tests::FunDefaultResult, boost::bind(&CActorC::FunDefaultResult, this, _1), true),
            FUN_HANDLER(actor_tests::FunVoidResult, boost::bind(&CActorC::FunVoidResult, this, _1, _2), true),
            FUN_HANDLER(actor_tests::FunDefaultParam, boost::bind(&CActorC::FunDefaultParam, this, _1, _2), true),
            FUN_HANDLER(actor_tests::FunReference, boost::bind(&CActorC::FunReference, this, _1, _2), true),
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

    int FunDefaultResult(int i)
    {
        throw runtime_error("Interrupt");
    }

    void FunVoidResult(string& str, const int i = 15)
    {
        str = to_string(i);
    }

    int FunDefaultParam(int i, const string& str = "123")
    {
        return i + stoi(str);
    }

    bool FunReference(int& i, string& str)
    {
        i = 100;
        str = "abc";
        return true;
    }
};

BOOST_AUTO_TEST_CASE(call_message)
{
    InitLog(boost::filesystem::path(), severity_level::TRACE, true, true);
    CDocker docker;
    docker.Initialize(new CConfig);

    CActorC* pActorC = new CActorC;
    docker.Attach(pActorC);

    docker.Run();

    // FunDefaultResult
    {
        int i = 0;
        int r1 = CALL(actor_tests::FunDefaultResult(i));
        int r2 = pActorC->Call(actor_tests::FunDefaultResult(i));
        BOOST_CHECK(r1 == -1 && r2 == -1);
    }
    // FunVoidResult
    {
        string str1;
        CALL(actor_tests::FunVoidResult(str1, 100));
        string str2;
        pActorC->Call(actor_tests::FunVoidResult(str2));
        BOOST_CHECK(str1 == "100" && str2 == "15");
    }
    // FunDefaultParam
    {
        int i = 1;
        int r1 = CALL(actor_tests::FunDefaultParam(i));
        int r2 = pActorC->Call(actor_tests::FunDefaultParam(i));
        BOOST_CHECK(r1 == 124 && r2 == 124);
    }
    // FunReference
    {
        int i1;
        string str1;
        int b1 = CALL(actor_tests::FunReference(i1, str1));
        int i2;
        string str2;
        int b2 = pActorC->Call(actor_tests::FunReference(i2, str2));
        BOOST_CHECK(i1 == 100 && i2 == 100);
        BOOST_CHECK(str1 == "abc" && str2 == "abc");
        BOOST_CHECK(b1 && b2);
    }

    docker.Exit();
}

BOOST_AUTO_TEST_SUITE_END()
