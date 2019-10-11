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

template <typename T>
class LockFreeQueue
{
public:
    virtual const std::shared_ptr<T> Push(std::shared_ptr<T>&& t) = 0;
    virtual const std::shared_ptr<T> Pop() = 0;
};

template <typename T>
class ListMPSCQueue : public LockFreeQueue<T>
{
public:
    struct CNode
    {
        CNode(const std::shared_ptr<T>& t)
          : t(t), pNext(nullptr) {}

        const std::shared_ptr<T> t;
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

    const std::shared_ptr<T> Push(std::shared_ptr<T>&& t)
    {
        std::shared_ptr<T> sp(std::move(t));
        CNode* pNode = new CNode(sp);
        CNode* pPrev = pTail.exchange(pNode, std::memory_order_release);
        pPrev->pNext.store(pNode, std::memory_order_relaxed);
        return sp;
    }

    const std::shared_ptr<T> Pop()
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
