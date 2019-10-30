// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_MESSAGE_ACTORWORKER_H
#define XENGINE_MESSAGE_ACTORWORKER_H

#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <map>
#include <memory>
#include <type_traits>
#include <atomic>

#include "docker/thread.h"
#include "type.h"

namespace xengine
{

class CMessage;

/**
 * @brief CIOActorWorker object can handle message.
 *        It is the worker of CIOActor.
 *        And There may be one or more workers behind CIOActor for scalability.
 */
class CIOActorWorker
{
    typedef std::function<void(const CMessage&)> HandlerFunction;

public:
    /**
     * @brief Actor worker constructor
     */
    CIOActorWorker(const std::string& strNameIn = "");

    /**
     * @brief Actor worker destructor
     */
    virtual ~CIOActorWorker();

    /**
     * @brief Publish a message to Actor worker.
     * @param spMessage a shared_ptr object of CMessage or it's derived
     * @note Thread safe.
     */
    void Publish(std::shared_ptr<CMessage> spMessage);

    /**
     * @brief Stop service.
     */
    void Stop();

    /**
     * @brief Register message handler for derived.
     * @tparam Message Concrete message class to be registered.
     *         Message is equal to or derived from CMessage.
     * @param handler The function (void handler(const std::shared_ptr<Message>)) which handle message.
     * @warning Non-thread-safety. 
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
        mapHandler[Message::MessageType()] = handler;
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
        mapHandler[Message::MessageType()] = handler;
    }

    /**
     * @brief Deregister message handler for derived.
     * @param nType The message type.
     * @warning Non-thread-safety. 
     * @code
     *     DeregisterHandler(DerivedMessage::MessageType());
     * @endcode
     */
    void DeregisterHandler(const uint32 nType);

    /**
     * @brief Get name of actor worker
     * @return Name of actor worker
     */
    const std::string& GetName() const;

    /**
     * @brief Get reference of io_service
     * @return The reference of io_service
     */
    boost::asio::io_service& GetService();

    /**
     * @brief Get reference of io_service::strand
     * @return The reference of io_service::strand
     */
    boost::asio::io_service::strand& GetStrand();

    /**
     * @brief Get reference of worker thread
     * @return The reference of worker thread
     */
    CThread& GetThread();

protected:
    /**
     * @brief Called before message handler thread running.
     * @return Successful or not.
     */
    virtual bool EnterLoop();

    /**
     * @brief Called after message handler thread stopping.
     */
    virtual void LeaveLoop();

private:
    /// The function of the thread entry contains ioService.run().
    void HandlerThreadFunc();
    /// Message handler callback entry.
    void MessageHandler(std::shared_ptr<CMessage> spMessage);

protected:
    std::string strName;
    boost::asio::io_service ioService;
    boost::asio::io_service::strand ioStrand;
    CThread thrIOActorWorker;

private:
    boost::asio::io_service::work ioWork;
    std::map<uint32, boost::any> mapHandler;
};

} // namespace xengine

#endif // XENGINE_MESSAGE_ACTORWORKER_H