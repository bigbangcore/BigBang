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

#include "actor/actorworker.h"
#include "base/controller.h"
#include "message/messagecenter.h"

namespace xengine
{

class CMessage;

/**
 * @brief CActor object can handle message.
 */
class CActor : public IController, public CActorWorker
{
public:
    /**
     * @brief Actor constructor
     */
    CActor(const std::string& strOwnKeyIn = "");

    /**
     * @brief Actor destructor
     */
    virtual ~CActor();

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
    virtual bool EnterLoop() override;

    /**
     * @brief Called after message handler thread stopping.
     */
    virtual void LeaveLoop() override;
};

} // namespace xengine

#endif // XENGINE_MESSAGE_ACTOR_H