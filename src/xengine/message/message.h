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
 *        GENERATE_MESSAGE_VIRTUAL_FUNCTION(CDerivedMessage);
 *        // self member variables
 *        string str;
 *        int n;
 *        ...
 *     };
 *     INITIALIZE_MESSAGE_TYPE(CDerivedMessage);
 * @endcode
 */
class CMessage
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
};

/**
 * @brief Create the virtual function of class derived from CMessage: Type() and destructor.
 * @param cls The name of derived class.
 */
#define GENERATE_MESSAGE_VIRTUAL_FUNCTION(cls)                                  \
    virtual uint32 Type() const override                                        \
    {                                                                           \
        return cls::nType;                                                      \
    }                                                                           \
    virtual void Handle(boost::any handler) override                            \
    {                                                                           \
        boost::any_cast<boost::function<void(const cls& msg)>>(handler)(*this); \
    }                                                                           \
    static const uint32 nType

/**
 * @brief Initialze the variable 'static const uint32 cls::nType' of cls.
 * @param cls The name of derived class.
 */
#define INITIALIZE_MESSAGE_TYPE(cls) \
    const uint32 cls::nType = CMessage::NewMessageType()

} // namespace xengine

#endif // XENGINE_MESSAGE_MESSAGE_H