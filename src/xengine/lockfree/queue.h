// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_LOCKFREE_QUEUE_H
#define XENGINE_LOCKFREE_QUEUE_H

#include <atomic>
#include <memory>
#include <type_traits>

namespace xengine
{

template <typename T, typename U = typename std::add_pointer<T>::type>
class LockFreeQueue
{
public:
    virtual void Push(T* t) = 0;
    virtual U Pop() = 0;
};

template <typename T>
class ListMPSCQueue : public LockFreeQueue<T, std::shared_ptr<T>>
{
public:
    typedef T* Ptr;
    typedef std::shared_ptr<T> SPtr;

    struct CNode
    {
        CNode(Ptr t)
          : t(t), pNext(nullptr) {}
        CNode(SPtr t)
          : t(t), pNext(nullptr) {}

        SPtr t;
        std::atomic<CNode*> pNext;
    };

    ListMPSCQueue()
    {
        pHead = new CNode(nullptr);
        pTail.store(pHead);
    }

    ~ListMPSCQueue()
    {
        Clear();
    }

    void Push(Ptr t)
    {
        Push(SPtr(t));
    }

    void Push(SPtr t)
    {
        CNode* pNode = new CNode(t);
        CNode* pPrev = pTail.exchange(pNode, std::memory_order_release);
        pPrev->pNext.store(pNode, std::memory_order_relaxed);
    }

    SPtr Pop()
    {
        CNode* pNode = pHead->pNext.load(std::memory_order_relaxed);
        if (pNode)
        {
            delete pHead;
            pHead = pNode;
            return pNode->t;
        }
        return nullptr;
    }

private:
    void Clear()
    {
        while (pHead)
        {
            CNode* pNode = pHead->pNext.load(std::memory_order_relaxed);
            delete pHead;
            pHead = pNode;
        }
    }

private:
    CNode* pHead;
    std::atomic<CNode*> pTail;
};

} // namespace xengine

#endif // XENGINE_LOCKFREE_QUEUE_H
