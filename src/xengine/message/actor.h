// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_MESSAGE_ACTOR_H
#define XENGINE_MESSAGE_ACTOR_H

#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <map>
#include <memory>
#include <type_traits>

#include "base/controller.h"
#include "message/messagecenter.h"

namespace xengine
{

class CMessage;

/**
 * @brief CIOActor object can handle message.
 */
class CIOActor : public IController
{
    typedef std::function<void(const CMessage&)> HandlerFunction;

public:
    /**
     * @brief Actor constructor
     */
    CIOActor(const std::string& ownKeyIn = "");

    /**
     * @brief Actor destructor
     */
    virtual ~CIOActor();

    /**
     * @brief Publish a message to Actor.
     * @param spMessage a shared_ptr object of CMessage or it's deriv
     * @note Thread safe.
     */
    void Publish(std::shared_ptr<CMessage> spMessage);

protected:
    /**
     * @brief Override IBase::HandleInitialize().
     *        Derived class should call RegisterHandler in this function.
     */
    virtual bool HandleInitialize() override;

    /**
     * @brief Override IBase::HandleInvoke()
     */
    virtual bool HandleInvoke() override;

    /**
     * @brief Override IBase::HandleHalt()
     */
    virtual void HandleHalt() override;

    /**
     * @brief Override IBase::HandleDeinitialize()
     */
    virtual void HandleDeinitialize() override;

    /**
     * @brief Called before message handler thread running.
     */
    virtual void EnterLoop();

    /**
     * @brief Called after message handler thread stopping.
     */
    virtual void LeaveLoop();

    /**
     * @brief Register message handler for derived.
     * @tparam Message Concrete message class to be registered.
     *         Message is equal to or derived from CMessage.
     * @param handler The function (void handler(const Message&)) which handle message.
     * @code
     *     void HandlerDerivedFunction(const DerivedMessage&);
     *     RegisterHandler<DerivedMessage>(HandlerDerivedFunction);
     * 
     *     void HandlerBaseFunction(const CMessage&);
     *     RegisterHandler<DerivedMessage>(HandlerBaseFunction);
     * @endcode
     */
    template <typename Message, typename = typename std::enable_if<std::is_base_of<CMessage, Message>::value, Message>::type>
    void RegisterHandler(boost::function<void(const Message&)> handler)
    {
        CMessageCenter::GetInstance().Subscribe(Message::nType, this);
        mapHandler[Message::nType] = handler;
    }

    /**
     * @brief Deregister message handler for derived.
     * @param nType The message type.
     * @code
     *     DeregisterHandler(DerivedMessage::nType);
     * @endcode
     */
    void DeregisterHandler(const uint32 nType)
    {
        CMessageCenter::GetInstance().Unsubscribe(nType, this);
        mapHandler.erase(nType);
    }

private:
    /// The function of the thread entry contains ioService.run().
    void HandlerThreadFunc();
    /// Message handler callback entry.
    void MessageHandler(std::shared_ptr<CMessage> spMessage);

protected:
    boost::asio::io_service ioService;
    boost::asio::io_service::strand ioStrand;

private:
    boost::asio::io_service::work ioWork;
    CThread thrIOActor;
    std::map<uint32, boost::any> mapHandler;
};

} // namespace xengine

#endif // XENGINE_MESSAGE_ACTOR_H