// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_MESSAGE_MESSAGECENTER_H
#define XENGINE_MESSAGE_MESSAGECENTER_H

#include <boost/thread/shared_mutex.hpp>
#include <map>
#include <set>

#include "type.h"

namespace xengine
{

class CIOActor;
class CMessage;

/**
 * @brief Save actor objects and their handling message type.
 */
class CMessageCenter
{
public:
    /**
     * @brief Actor subscribe the message with nType.
     * @param nType Message type.
     * @param pActor The pointer of subscriber.
     */
    static void Subscribe(const uint32 nType, CIOActor* pActor);

    /**
     * @brief Publish message to subscribers set of spMessage->Type().
     * @param spMessage Message. 
     */
    static void Publish(std::shared_ptr<CMessage> spMessage);

private:
    static std::map<uint32, std::set<CIOActor*>> mapMessage;
    static boost::shared_mutex mtxMessage;
};

} // namespace xengine

#endif // XENGINE_MESSAGE_MESSAGECENTER_H