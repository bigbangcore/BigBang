// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "messagecenter.h"

#include <memory>

#include "logger.h"
#include "message/actor.h"

namespace xengine
{

namespace
{
// The message type for subscribing message to CMessageCenter
struct CSubscribeMessage : public CMessage
{
    GENERATE_MESSAGE_FUNCTION(CSubscribeMessage);

    uint32 nSubType;
    CIOActor* pActor;
};

// The message type for unsubscribing message from CMessageCenter
struct CUnsubscribeMessage : public CMessage
{
    GENERATE_MESSAGE_FUNCTION(CUnsubscribeMessage);

    uint32 nUnsubType;
    CIOActor* pActor;
};

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
    Publish(std::shared_ptr<CSubscribeMessage>(pMessage));
}

void CMessageCenter::Unsubscribe(const uint32 nType, CIOActor* pActor)
{
    CUnsubscribeMessage* pMessage = new CUnsubscribeMessage();
    pMessage->nUnsubType = nType;
    pMessage->pActor = pActor;
    Publish(std::shared_ptr<CUnsubscribeMessage>(pMessage));
}

const std::shared_ptr<CMessage> CMessageCenter::Publish(std::shared_ptr<CMessage>&& spMessage)
{
    // DebugLog("message-center", "Publish tag {%s} message to center\n", spMessage->Tag().c_str());
    ++nSize;
    return queue.Push(std::move(spMessage));
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
            if (spMessage->Type() == CSubscribeMessage::MessageType())
            {
                std::shared_ptr<CSubscribeMessage> spSubMessage = std::dynamic_pointer_cast<CSubscribeMessage>(spMessage);
                mapMessage[spSubMessage->nSubType].insert(spSubMessage->pActor);
            }
            else if (spMessage->Type() == CUnsubscribeMessage::MessageType())
            {
                std::shared_ptr<CUnsubscribeMessage> spUnsubMessage = std::dynamic_pointer_cast<CUnsubscribeMessage>(spMessage);
                mapMessage[spUnsubMessage->nUnsubType].erase(spUnsubMessage->pActor);
            }
            else
            {
                auto it = mapMessage.find(spMessage->Type());
                if (it != mapMessage.end() && !it->second.empty())
                {
                    // DebugLog("message-center", "Dispatch tag {%s} message to actors, number is {%d}\n", spMessage->Tag().c_str(), it->second.size());
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