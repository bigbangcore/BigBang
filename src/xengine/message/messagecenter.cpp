// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "messagecenter.h"

#include "message/actor.h"
#include "message/message.h"

namespace xengine
{

std::map<uint32, std::set<CIOActor*>> CMessageCenter::mapMessage;
boost::shared_mutex CMessageCenter::mtxMessage;

void CMessageCenter::Subscribe(const uint32 nType, CIOActor* pActor)
{
    boost::unique_lock<boost::shared_mutex> wlock(mtxMessage);
    mapMessage[nType].insert(pActor);
}

void CMessageCenter::Publish(std::shared_ptr<CMessage> spMessage)
{
    boost::shared_lock<boost::shared_mutex> rlock(mtxMessage);
    auto it = mapMessage.find(spMessage->Type());
    if (it != mapMessage.end())
    {
        for (auto& pActor : it->second)
        {
            pActor->Publish(spMessage);
        }
    }
}

} // namespace xengine