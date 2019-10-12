// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_MESSAGE_ACTOR_H
#define XENGINE_MESSAGE_ACTOR_H

#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <memory>
#include <set>
#include <type_traits>

#include "base/controller.h"
#include "message/actorworker.h"
#include "message/messagecenter.h"

namespace xengine
{

class CMessage;

/**
 * @brief CIOActor object can handle message.
 */
class CIOActor : public IController, public CIOActorWorker
{
    typedef std::function<void(const CMessage&)> HandlerFunction;

public:
    /**
     * @brief Actor constructor
     */
    CIOActor(const std::string& strOwnKeyIn = "");

    /**
     * @brief Actor destructor
     */
    virtual ~CIOActor();

protected:
    /**
     * @brief Start actor worker thread.
     *        Call it in the beginning of HandleInvoke() commonly.
     */
    bool StartActor();

    /**
     * @brief Stop actor worker thread.
     *        Call it in the beginning of HandleHalt() commonly.
     */
    void StopActor();

    /**
     * @brief Called before message handler thread running.
     */
    virtual void EnterLoop() override;

    /**
     * @brief Called after message handler thread stopping.
     */
    virtual void LeaveLoop() override;

    /**
     * @brief Register message handler for derived.
     * @tparam Message Concrete message class to be registered.
     *         Message is equal to or derived from CMessage.
     * @param handler The function (void handler(const std::shared_ptr<Message>)) which handle message.
     * @code
     *     void HandlerDerivedFunction(const std::shared_ptr<DerivedMessage>&);
     *     RegisterPtrHandler<DerivedMessage>(HandlerDerivedFunction);
     * 
     *     void HandlerBaseFunction(const std::shared_ptr<CMessage>&);
     *     RegisterPtrHandler<DerivedMessage>(HandlerBaseFunction);
     * @endcode
     */
    template <typename Message, typename = typename std::enable_if<std::is_base_of<CMessage, Message>::value, Message>::type>
    void RegisterPtrHandler(boost::function<void(const std::shared_ptr<Message>&)> handler)
    {
        CMessageCenter::GetInstance().Subscribe(Message::MessageType(), this);
        CIOActorWorker::RegisterPtrHandler<Message>(handler);
    }

    /**
     * @brief Register message handler for derived.
     * @tparam Message Concrete message class to be registered.
     *         Message is equal to or derived from CMessage.
     * @param handler The function (void handler(const Message&)) which handle message.
     * @code
     *     void HandlerDerivedFunction(const DerivedMessage&);
     *     RegisterRefHandler<DerivedMessage>(HandlerDerivedFunction);
     * 
     *     void HandlerBaseFunction(const CMessage&);
     *     RegisterRefHandler<DerivedMessage>(HandlerBaseFunction);
     * @endcode
     */
    template <typename Message, typename = typename std::enable_if<std::is_base_of<CMessage, Message>::value, Message>::type>
    void RegisterRefHandler(boost::function<void(const Message&)> handler)
    {
        CMessageCenter::GetInstance().Subscribe(Message::MessageType(), this);
        CIOActorWorker::RegisterRefHandler<Message>(handler);
    }

    /**
     * @brief Deregister message handler for derived.
     * @param nType The message type.
     * @code
     *     DeregisterHandler(DerivedMessage::MessageType());
     * @endcode
     */
    void DeregisterHandler(const uint32 nType);

private:
    /// The function of the thread entry contains ioService.run().
    void HandlerThreadFunc();
    /// Message handler callback entry.
    void MessageHandler(std::shared_ptr<CMessage> spMessage);
};

} // namespace xengine

#endif // XENGINE_MESSAGE_ACTOR_H