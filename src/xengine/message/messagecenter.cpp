// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "messagecenter.h"

#include <memory>

#include "message/actor.h"

namespace xengine
{

namespace
{
// The message type for subscribing message to CMessageCenter
struct CSubscribeMessage : public CMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CSubscribeMessage);

    uint32 nSubType;
    CIOActor* pActor;
};
INITIALIZE_MESSAGE_STATIC_VAR(CSubscribeMessage, "SubscribeMessage");

// The message type for unsubscribing message from CMessageCenter
struct CUnsubscribeMessage : public CMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CUnsubscribeMessage);

    uint32 nUnsubType;
    CIOActor* pActor;
};
INITIALIZE_MESSAGE_STATIC_VAR(CUnsubscribeMessage, "UnsubscribeMessage");
} // namespace

// CMessageCenter
CMessageCenter::CMessageCenter()
{
    pDistThread = new boost::thread(boost::bind(&CMessageCenter::DistributionThreadFunc, this));
}

CMessageCenter::~CMessageCenter()
{
    pDistThread->interrupt();
    pDistThread->join();
    delete pDistThread;
}

void CMessageCenter::Subscribe(const uint32 nType, CIOActor* pActor)
{
    CSubscribeMessage* pMessage = new CSubscribeMessage();
    pMessage->nSubType = nType;
    pMessage->pActor = pActor;
    Publish(pMessage);
}

void CMessageCenter::Unsubscribe(const uint32 nType, CIOActor* pActor)
{
    CUnsubscribeMessage* pMessage = new CUnsubscribeMessage();
    pMessage->nUnsubType = nType;
    pMessage->pActor = pActor;
    Publish(pMessage);
}

void CMessageCenter::Publish(CMessage* pMessage)
{
    queue.Push(pMessage);
    ++nSize;
}

void CMessageCenter::Publish(std::shared_ptr<CMessage> spMessage)
{
    queue.Push(spMessage);
    ++nSize;
}

uint64 CMessageCenter::Size()
{
    return nSize;
}

void CMessageCenter::DistributionThreadFunc()
{
    std::shared_ptr<CMessage> spMessage = nullptr;
    while (true)
    {
        while ((spMessage = queue.Pop()))
        {
            if (spMessage->Type() == CSubscribeMessage::nType)
            {
                std::shared_ptr<CSubscribeMessage> spSubMessage = std::dynamic_pointer_cast<CSubscribeMessage>(spMessage);
                mapMessage[spSubMessage->nSubType].insert(spSubMessage->pActor);
            }
            else if (spMessage->Type() == CUnsubscribeMessage::nType)
            {
                std::shared_ptr<CUnsubscribeMessage> spUnsubMessage = std::dynamic_pointer_cast<CUnsubscribeMessage>(spMessage);
                mapMessage[spUnsubMessage->nUnsubType].erase(spUnsubMessage->pActor);
            }
            else
            {
                auto it = mapMessage.find(spMessage->Type());
                if (it != mapMessage.end() && !it->second.empty())
                {
                    for (auto& pActor : it->second)
                    {
                        pActor->Publish(spMessage);
                    }
                }
            }
            --nSize;
        }
        boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
    }
}

} // namespace xengine