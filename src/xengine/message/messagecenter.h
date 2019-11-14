// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_MESSAGE_MESSAGECENTER_H
#define XENGINE_MESSAGE_MESSAGECENTER_H

#include <atomic>
#include <boost/thread/thread.hpp>
#include <map>
#include <set>

#include "lockfree/queue.h"
#include "message/message.h"
#include "type.h"

namespace xengine
{

/**
 * @brief A shortcut to publish message.
 * @warning Don't modify members of msg after calling it, unless you know the consequence. See CMessageCenter::Publish()
 */
#define PUBLISH_MESSAGE(msg) CMessageCenter::GetInstance().Publish(std::move(msg))

class CActorWorker;

/**
 * @brief Save actor objects and their handling message type.
 */
class CMessageCenter final
{
public:
    /**
     * @brief Return the singleton instance of CMessageCenter.
     * @return The reference of CMessageCenter object.
     */
    static CMessageCenter& GetInstance()
    {
        static CMessageCenter messageCenter;
        return messageCenter;
    }

    /**
     * @brief Destructor of CMessageCenter.
     */
    ~CMessageCenter();

    /**
     * @brief Subscribe to the nType message.
     * @param nType Message type.
     * @param pActor An Actor pointer as the subscriber.
     */
    void Subscribe(const uint32 nType, CActorWorker* pActor);

    /**
     * @brief Unsubscribe the nType of message.
     * @param nType Message type.
     * @param pActor An Actor pointer as the unsubscriber.
     */
    void Unsubscribe(const uint32 nType, CActorWorker* pActor);

    /**
     * @brief Publish a spMessage->Type() message to subscribers.
     * @param spMessage The rvalue of message smart pointer. It will be seted nullptr when return.
     * @return const shared pointer of spMessage.
     * @warning spMessage will be seted nullptr when return.
     *          Don't modify members of the original pointer of spMessage after published it, unless you know the consequence.
     */
    const std::shared_ptr<CMessage> Publish(std::shared_ptr<CMessage>&& spMessage);

    /**
     * @brief Get the size of undistributed message.
     * @return The size of undistributed message.
     */
    uint64 Size();

protected:
    CMessageCenter();
    CMessageCenter(CMessageCenter&) = delete;
    CMessageCenter(CMessageCenter&&) = delete;

    /// The distribution thread function.
    void DistributionThreadFunc();

private:
    std::map<uint32, std::set<CActorWorker*>> mapMessage;
    ListMPSCQueue<CMessage> queue;
    std::atomic<uint64> nSize;
    boost::thread* pDistThread;
};

} // namespace xengine

#endif // XENGINE_MESSAGE_MESSAGECENTER_H