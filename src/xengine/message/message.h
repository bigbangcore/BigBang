// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_MESSAGE_MESSAGE_H
#define XENGINE_MESSAGE_MESSAGE_H

#include <boost/any.hpp>
#include <boost/function.hpp>
#include <memory>

#include "type.h"

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
 * @brief Create the virtual function of class derived from CMessage: Type() and destructor.
 * @param cls Derived class name.
 */
#define GENERATE_MESSAGE_FUNCTION(cls)                                                             \
    template <typename... Args>                                                                    \
    static std::shared_ptr<cls> Create(Args&&... args)                                             \
    {                                                                                              \
        return std::make_shared<cls>(std::forward<Args>(args)...);                                 \
    }                                                                                              \
    static uint32 MessageType()                                                                    \
    {                                                                                              \
        static const uint32 nType = CMessage::NewMessageType();                                    \
        return nType;                                                                              \
    }                                                                                              \
    static std::string MessageTag()                                                                \
    {                                                                                              \
        static const std::string strTag = #cls;                                                    \
        return strTag;                                                                             \
    }                                                                                              \
    virtual uint32 Type() const override                                                           \
    {                                                                                              \
        return cls::MessageType();                                                                 \
    }                                                                                              \
    virtual std::string Tag() const override                                                       \
    {                                                                                              \
        return cls::MessageTag();                                                                  \
    }                                                                                              \
    virtual void Handle(boost::any handler) override                                               \
    {                                                                                              \
        try                                                                                        \
        {                                                                                          \
            auto f = boost::any_cast<boost::function<void(const std::shared_ptr<cls>&)>>(handler); \
            f(SharedFromBase<cls>());                                                              \
        }                                                                                          \
        catch (const boost::bad_any_cast&)                                                         \
        {                                                                                          \
            auto f = boost::any_cast<boost::function<void(const cls&)>>(handler);                  \
            f(*this);                                                                              \
        }                                                                                          \
    }

} // namespace xengine

#endif // XENGINE_MESSAGE_MESSAGE_H