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
INITIALIZE_MESSAGE_TYPE(CSubscribeMessage);

// The message type for unsubscribing message from CMessageCenter
struct CUnsubscribeMessage : public CMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CUnsubscribeMessage);

    uint32 nUnsubType;
    CIOActor* pActor;
};
INITIALIZE_MESSAGE_TYPE(CUnsubscribeMessage);
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

    queue.Clear();
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

uint64 CMessageCenter::Size()
{
    return nSize;
}

void CMessageCenter::DistributionThreadFunc()
{
    CMessage* pMessage = nullptr;
    while (true)
    {
        while ((pMessage = queue.Pop()))
        {
            if (pMessage->Type() == CSubscribeMessage::nType)
            {
                CSubscribeMessage* pSubMessage = static_cast<CSubscribeMessage*>(pMessage);
                mapMessage[pSubMessage->nSubType].insert(pSubMessage->pActor);
                delete pMessage;
            }
            else if (pMessage->Type() == CUnsubscribeMessage::nType)
            {
                CUnsubscribeMessage* pUnsubMessage = static_cast<CUnsubscribeMessage*>(pMessage);
                mapMessage[pUnsubMessage->nUnsubType].erase(pUnsubMessage->pActor);
                delete pMessage;
            }
            else
            {
                auto it = mapMessage.find(pMessage->Type());
                if (it != mapMessage.end() && !it->second.empty())
                {
                    std::shared_ptr<CMessage> spMessage(pMessage);
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