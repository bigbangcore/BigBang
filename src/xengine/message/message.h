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

#include "logger.h"
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
 *        DECLARE_PUBLISHED_MESSAGE_FUNCTION(CDerivedMessage);
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

/// Apply a function with params in a tuple
template <typename R, typename F, typename Tuple>
R Apply(F f, Tuple&& t)
{
    using Indices = MakeIndexSequence<std::tuple_size<typename std::decay<Tuple>::type>::value>;
    return Apply_impl<R>(f, std::forward<Tuple>(t), Indices());
}

/**
 * @brief Send a message which is handled by functional-like function and wait(sync or async) a result
 */
template <typename R>
class CCalledMessage : public CMessage
{
public:
    /// handle result
    mutable std::promise<R> result;
    /// failure result
    R failure;

    /**
     * @brief Default constructor
     */
    CCalledMessage() {}
    /**
     * @brief Construct with failure value
     */
    CCalledMessage(R failure)
      : failure(failure) {}
    /**
     * @brief Default move constructor
     */
    CCalledMessage(CCalledMessage&&) = default;

    /**
     * @brief New a object of moved self.
     */
    virtual CCalledMessage<R>* Move() = 0;

    /**
     * @brief Apply the handler. Called by the executor
     */
    template <typename F, typename Tuple>
    void ApplyHandler(F f, Tuple&& t)
    {
        try
        {
            auto ret = Apply<R>(f, std::forward<Tuple>(t));
            result.set_value(ret);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(Tag().c_str(), "Apply Handler error: %s", e.what());
            result.set_value(failure);
        }
    }

    /**
     * @brief Wait the result. Called by the caller
     */
    R Wait() const
    {
        try
        {
            return result.get_future().get();
        }
        catch (...)
        {
            return failure;
        }
    }
};

/**
 * @brief The specialization CCalledMessage<void> of CCalledMessage<R> 
 */
template <>
class CCalledMessage<void> : public CMessage
{
public:
    mutable std::promise<void> result;

    /**
     * @brief Default constructor
     */
    CCalledMessage() {}
    /**
     * @brief Default move constructor
     */
    CCalledMessage(CCalledMessage&&) = default;

    /**
     * @brief New a object of moved self.
     */
    virtual CCalledMessage<void>* Move() = 0;

    /**
     * @brief Apply the handler. Called by the executor
     */
    template <typename F, typename Tuple>
    void ApplyHandler(F f, Tuple&& t)
    {
        try
        {
            Apply<void>(f, std::forward<Tuple>(t));
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(Tag().c_str(), "Apply Handler error: %s", e.what());
        }
        result.set_value();
    }

    /**
     * @brief Wait the result. Called by the caller
     */
    void Wait() const
    {
        try
        {
            result.get_future().get();
        }
        catch (...)
        {
        }
    }
};

/**
 * @brief Create the  function of class derived from CMessage: Type() and destructor.
 * @param cls Derived class name.
 */
#define DECLARE_CALLED_MESSAGE_CLASS(cls, ...) CALLED_MESSAGE(cls, __VA_ARGS__)

/**
 * @brief Create the virtual functions of the class derived from CMessage: Type() and destructor.
 * @param cls Derived class name.
 */
#define DECLARE_PUBLISHED_MESSAGE_FUNCTION(cls) PUBLISHED_MESSAGE(cls)

} // namespace xengine

#endif // XENGINE_MESSAGE_MESSAGE_H