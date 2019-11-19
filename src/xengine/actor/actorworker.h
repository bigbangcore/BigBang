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

#include "docker/thread.h"
#include "message/message.h"
#include "type.h"

namespace xengine
{

class CDocker;

#define FUN_HANDLER(Message, function, global) \
    CMessageHandler::CreateFun<Message>(function, global)
#define PTR_HANDLER(Message, function, global) \
    CMessageHandler::CreatePtr<Message>(function, global)

/**
 * @brief Message handler infomation
 */
class CMessageHandler final
{
public:
    /**
     * @brief Create a message handler which accept "const Message&" parameter
     * @tparam Message Concrete message class to be registered
     *         Message must be equal to or derived from CMessage
     * @param handler The function (void handler(const Message&)) which handle message
     * @warning Non-thread-safety. 
     * @code
     *     void HandlerDerivedFunction(const DerivedMessage&);
     *     RegisterHandler(REF_HANDLER(DerivedMessage, HandlerDerivedFunction, true));
     * 
     *     void HandlerBaseFunction(const CMessage&);
     *     RegisterHandler(REF_HANDLER(DerivedMessage, HandlerBaseFunction, true));
     * @endcode
     */
    template <typename Message, typename = typename std::enable_if<std::is_base_of<CMessage, Message>::value, Message>::type>
    static CMessageHandler CreateFun(typename Message::FunHandlerType handlerIn, bool fGlobalIn)
    {
        return CMessageHandler(Message::MessageType(), Message::MessageTag(), handlerIn, fGlobalIn);
    }

    /**
     * @brief Create a message handler which accept "const shared_ptr<Message>" parameter
     * @tparam Message Concrete message class to be registered
     *         Message must be equal to or derived from CMessage
     * @param handler The function (void handler(const std::shared_ptr<Message>)) which handle message
     * @warning Non-thread-safety. 
     * @code
     *     void HandlerDerivedFunction(const std::shared_ptr<DerivedMessage>);
     *     RegisterHandler(REF_HANDLER(DerivedMessage, HandlerDerivedFunction, true));
     * 
     *     void HandlerBaseFunction(const std::shared_ptr<CMessage>);
     *     RegisterHandler(REF_HANDLER(DerivedMessage, HandlerBaseFunction, true));
     * @endcode
     */
    template <typename Message, typename = typename std::enable_if<std::is_base_of<CMessage, Message>::value, Message>::type>
    static CMessageHandler CreatePtr(typename Message::PtrHandlerType handlerIn, bool fGlobalIn)
    {
        return CMessageHandler(Message::MessageType(), Message::MessageTag(), handlerIn, fGlobalIn);
    }

    const uint32 nType;
    const std::string strTag;
    const boost::any handler;
    const bool fGlobal;

protected:
    CMessageHandler(const uint32 nTypeIn, const std::string strTagIn, const boost::any handlerIn, const bool fGlobalIn)
      : nType(nTypeIn), strTag(strTagIn), handler(handlerIn), fGlobal(fGlobalIn)
    {
    }
};

/**
 * @brief CActorWorker object can handle message.
 *        It contains a thread or coroutine.
 *        There may be one or more workers in a CActor for scalability.
 * @note A CActorWorker life cycle:
 *          main thread in constructor:     RegisterHandler();
 *          main thread in constructor:     Start();
 *          worker thread:                  EnterLoop();
 *          worker thread:                  in loop
 *          worker thread:                  LeaveLoop();
 *          main thread in deconstructor:   Stop();
 *          main thread in deconstructor:   DeregisterHandler();
 * 
 *       If create object by "CActorWorker(const std::string&, const vector<CMessageHandler>&)", it will skip "Register()" function.
 */
class CActorWorker
{
public:
    /**
     * @brief Actor worker constructor by tuple(uint32, boost)
     * @param strNameIn worker name
     * @param vecHandler message handlers
     */
    CActorWorker(const std::string& strNameIn = "", const std::vector<CMessageHandler>& vecHandler = std::vector<CMessageHandler>());

    /**
     * @brief Actor worker destructor
     */
    virtual ~CActorWorker();

    /**
     * @brief Register a message handler
     * @param handler CMessageHandler object
     * @warning Non-thread-safety. 
     */
    void RegisterHandler(const CMessageHandler& handler);

    /**
     * @brief Register a batch of message handlers
     * @param vecHandler a vector of CMessageHandler.
     */
    void RegisterHandler(const std::vector<CMessageHandler>& vecHandler);

    /**
     * @brief Start worker thread
     */
    virtual void Start();

    /**
     * @brief Stop worker thread
     */
    virtual void Stop();

    /**
     * @brief Deregister a message handler
     * @param nType The message type.
     * @warning Non-thread-safety. 
     * @code
     *     DeregisterHandler(DerivedMessage::MessageType());
     * @endcode
     */
    void DeregisterHandler(const uint32 nType);

    /**
     * @brief Deregister all messages handler for derived.
     * @warning Non-thread-safety. 
     */
    void DeregisterHandler();

    /**
     * @brief Publish a message to Actor worker.
     * @param spMessage a shared_ptr object of CMessage or it's derived
     * @note Thread safe.
     */
    void Publish(const std::shared_ptr<CMessage> spMessage);

    /**
     * @brief Publish a message to Actor worker.
     * @param spMessage a shared_ptr object of CMessage or it's derived
     * @note Thread safe.
     */
    template <typename T>
    T Call(CCalledMessage<T>&& msg)
    {
        auto spMsg = std::shared_ptr<CCalledMessage<T>>(msg.Move());
        Publish(spMsg);
        return spMsg->Wait();
    }

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
     * @brief Called by worker thread after thread running and before loop
     * @return Successful or not.
     */
    virtual bool EnterLoop();

    /**
     * @brief Called by worker thread after thread loop and before stopping
     */
    virtual void LeaveLoop();

private:
    /// The function of the thread entry contains ioService.run().
    void HandlerThreadFunc();
    /// Message handler callback entry.
    void MessageHandler(const std::shared_ptr<CMessage> spMessage);

protected:
    std::string strName;
    boost::asio::io_service ioService;
    boost::asio::io_service::strand ioStrand;
    CThread thr;

private:
    boost::asio::io_service::work ioWork;
    std::map<uint32, CMessageHandler> mapHandler;
};

} // namespace xengine

#endif // XENGINE_MESSAGE_ACTORWORKER_H