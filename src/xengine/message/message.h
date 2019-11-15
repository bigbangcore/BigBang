// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_MESSAGE_MESSAGE_H
#define XENGINE_MESSAGE_MESSAGE_H

#include <boost/any.hpp>
#include <boost/function.hpp>
#include <exception>
#include <future>
#include <memory>
#include <string>
#include <tuple>

#include "message/macro.h"
#include "type.h"
#include "util.h"

namespace xengine
{

/**
 * @brief CMessage is the basic class of messages which are delivered between actors.
 *        Define a concrete message as following.
 * @code
 *    struct CDerivedMessage : public CMessage
 *    {
 *        GENERATE_MESSAGE_FUNCTION(CDerivedMessage);
 *        // self member variables
 *        string str;
 *        int n;
 *        ...
 *     };
 * @endcode
 */
class CMessage : public std::enable_shared_from_this<CMessage>
{
public:
    /// Send time(seconds)
    int64 nTime;

public:
    /**
     * @brief Constructor
     */
    CMessage()
      : nTime(GetTime()) {}

    /**
     * @brief Virtual destructor
     */
    virtual ~CMessage() {}

    /**
     * @brief Return message type.
     * @return The message type.
     */
    virtual uint32 Type() const = 0;

    /**
     * @brief Return message tag.
     * @return The message tag.
     */
    virtual std::string Tag() const = 0;

    /**
     * @brief Handle self by handler
     * @param handler The handler function
     */
    virtual void Handle(boost::any handler) = 0;

protected:
    /**
     * @brief Generate a new message type which is globally unique.
     * @return New message type.
     */
    static uint32 NewMessageType()
    {
        static uint32 nType = 0;
        return ++nType;
    }

    template <typename Derived>
    std::shared_ptr<Derived> SharedFromBase()
    {
        return std::static_pointer_cast<Derived>(shared_from_this());
    }
};

/**
 * @brief Send a message which is handled by functional-like function and wait(sync or async) a result
 */
template <const char* Name, typename R, typename... Args>
struct CCalledMessage final : public CMessage
{
    using ResultType = R;
    using Self = CCalledMessage<Name, R, Args...>;
    using HandlerType = typename boost::function<ResultType(Args...)>;
    using ParamType = std::tuple<Args...>;

    std::promise<ResultType> result;
    ParamType param;
    ResultType failure;

public:
    CCalledMessage(R failure, Args... args)
      : failure(failure), param(std::forward_as_tuple(args...)) {}

    static uint32 MessageType()
    {
        static const uint32 nType = CMessage::NewMessageType();
        return nType;
    }
    static std::string MessageTag()
    {
        static const std::string strTag = Name;
        return strTag;
    }
    virtual uint32 Type() const override
    {
        return Self::MessageType();
    }
    virtual std::string Tag() const override
    {
        return Self::MessageTag();
    }
    virtual void Handle(boost::any handler) override
    {
        using PtrHandlerType = boost::function<void(const std::shared_ptr<Self>)>;
        try
        {
            if (handler.type() == typeid(HandlerType))
            {
                auto f = boost::any_cast<HandlerType>(handler);
                auto ret = Apply(f, Self::param);
                Self::result.set_value(ret);
            }
            else if (handler.type() == typeid(PtrHandlerType))
            {
                auto f = boost::any_cast<PtrHandlerType>(handler);
                f(SharedFromBase<Self>());
            }
            else
            {
                throw std::runtime_error("Unknown type");
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(Tag().c_str(), "Message handler type error: %s", e.what());
        }
    }
};

template <const char* Name, typename... Args>
struct CCalledMessage<Name, void, Args...> : public CMessage
{
    using Self = CCalledMessage<Name, void, Args...>;
    using HandlerType = typename boost::function<void(Args...)>;
    using ParamType = std::tuple<Args...>;

    std::promise<void> result;
    ParamType param;

public:
    CCalledMessage(Args... args)
      : param(std::forward_as_tuple(args...)) {}

    static uint32 MessageType()
    {
        static const uint32 nType = CMessage::NewMessageType();
        return nType;
    }
    static std::string MessageTag()
    {
        static const std::string strTag = Name;
        return strTag;
    }
    virtual uint32 Type() const override
    {
        return Self::MessageType();
    }
    virtual std::string Tag() const override
    {
        return Self::MessageTag();
    }
    virtual void Handle(boost::any handler) override
    {
        using PtrHandlerType = boost::function<void(const std::shared_ptr<Self>)>;
        try
        {
            if (handler.type() == typeid(HandlerType))
            {
                auto f = boost::any_cast<HandlerType>(handler);
                Apply(f, Self::param);
                Self::result.set_value();
            }
            else if (handler.type() == typeid(PtrHandlerType))
            {
                auto f = boost::any_cast<PtrHandlerType>(handler);
                f(SharedFromBase<Self>());
            }
            else
            {
                throw std::runtime_error("Unknown type");
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(Tag().c_str(), "Message handler type error: %s", e.what());
        }
    }
};

/**
 * @brief Create the virtual function of class derived from CMessage: Type() and destructor.
 * @param cls Derived class name.
 */
#define GENERATE_MESSAGE_FUNCTION(cls)                                            \
    template <typename... Args>                                                   \
    static std::shared_ptr<cls> Create(Args&&... args)                            \
    {                                                                             \
        return std::make_shared<cls>(std::forward<Args>(args)...);                \
    }                                                                             \
    static uint32 MessageType()                                                   \
    {                                                                             \
        static const uint32 nType = CMessage::NewMessageType();                   \
        return nType;                                                             \
    }                                                                             \
    static std::string MessageTag()                                               \
    {                                                                             \
        static const std::string strTag = #cls;                                   \
        return strTag;                                                            \
    }                                                                             \
    virtual uint32 Type() const override                                          \
    {                                                                             \
        return cls::MessageType();                                                \
    }                                                                             \
    virtual std::string Tag() const override                                      \
    {                                                                             \
        return cls::MessageTag();                                                 \
    }                                                                             \
    virtual void Handle(boost::any handler) override                              \
    {                                                                             \
        using PtrHandlerType = boost::function<void(const std::shared_ptr<cls>)>; \
        try                                                                       \
        {                                                                         \
            if (handler.type() == typeid(PtrHandlerType))                         \
            {                                                                     \
                auto f = boost::any_cast<PtrHandlerType>(handler);                \
                f(SharedFromBase<cls>());                                         \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                throw std::runtime_error("Unknown type");                         \
            }                                                                     \
        }                                                                         \
        catch (const exception& e)                                                \
        {                                                                         \
            LOG_ERROR(Tag().c_str(), "Message handler type error: %s", e.what()); \
        }                                                                         \
    }

} // namespace xengine

#endif // XENGINE_MESSAGE_MESSAGE_H