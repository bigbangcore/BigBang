// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_LOCKFREE_QUEUE_H
#define XENGINE_LOCKFREE_QUEUE_H

#include <atomic>
#include <memory>

namespace xengine
{

template <typename T>
class LockFreeQueue
{
public:
    virtual void Push(T t) = 0;
    virtual T Pop() = 0;
};

template <typename T>
class ListMPSCQueue : public LockFreeQueue<T>
{
public:
    struct CNode
    {
        CNode(T t)
          : t(t), pNext(nullptr)
        {
        }

        T t;
        std::atomic<CNode*> pNext;
    };

    ListMPSCQueue()
    {
        pHead = new CNode(nullptr);
        pTail.store(pHead);
    }

    void Push(T t)
    {
        CNode* pNode = new CNode(t);
        CNode* pPrev = pTail.exchange(pNode, std::memory_order_release);
        pPrev->pNext.store(pNode, std::memory_order_relaxed);
    }

    T Pop()
    {
        CNode* pNode = pHead->pNext.load(std::memory_order_relaxed);
        if (pNode) {
            delete pHead;
            pHead = pNode;
            return pNode->t;
        }

        return nullptr;
    }

private:
    CNode* pHead;
    std::atomic<CNode*> pTail;
};

template <typename T>
class ListSPSCQueue : public LockFreeQueue<T>
{
public:
    struct CNode
    {
        CNode(T t)
          : t(t), pNext(nullptr)
        {
        }

        T t;
        CNode* pNext;
    };

    ListSPSCQueue()
    {
        pTail = new CNode(nullptr);
        pHead = pTail;
    }

    void Push(T t)
    {
        CNode* pNode = new CNode(t);
        CNode* pPrev = pTail;
        pTail = pNode;
        pPrev->pNext = pNode;
        // std::atomic_thread_fence(std::memory_order_release);
    }

    T Pop()
    {
        // std::atomic_thread_fence(std::memory_order_acquire);
        CNode* pNode = pHead->pNext;
        if (pNode) {
            delete pHead;
            pHead = pNode;
            return pNode->t;
        }

        return nullptr;
    }

private:
    CNode* pHead;
    CNode* pTail;
};

} // namespace xengine

#endif // XENGINE_LOCKFREE_QUEUE_H
