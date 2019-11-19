// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_DOCKER_TIMER_H
#define XENGINE_DOCKER_TIMER_H

#include <boost/asio/deadline_timer.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/system/error_code.hpp>
#include <boost/thread/thread_time.hpp>
#include <map>

#include "actor/actor.h"
#include "message/message.h"
#include "type.h"

namespace xengine
{

/**
 * @brief Pure virtual base class of time out message. It will be published by CTimer on expiried
 */
struct CTimeoutMessage : public CMessage
{
};

/**
 * @brief Timing message. It will be published to CTimer
 */
struct CSetTimerMessage : public CMessage
{
    DECLARE_PUBLISHED_MESSAGE_FUNCTION(CSetTimerMessage);

    /// Constructor
    CSetTimerMessage() {}
    CSetTimerMessage(std::shared_ptr<CTimeoutMessage> spTimeoutIn, boost::system_time expiryAtIn)
      : spTimeout(spTimeoutIn), expiryAt(expiryAtIn) {}

    /// Expiried time(seconds from 1970-01-01 UTC)
    boost::system_time expiryAt;
    /// Call back message. It must be not nullptr
    std::shared_ptr<CTimeoutMessage> spTimeout;
};

struct CCancelTimerMessage : public CMessage
{
    DECLARE_PUBLISHED_MESSAGE_FUNCTION(CCancelTimerMessage);

    /// Constructor
    CCancelTimerMessage() {}
    CCancelTimerMessage(std::shared_ptr<CTimeoutMessage> spTimeoutIn)
      : spTimeout(spTimeoutIn) {}

    /// The same in CSetTimerMessage. It is the unique key for canceling.
    std::shared_ptr<CTimeoutMessage> spTimeout;
};

/**
 * @brief A timer actor.
 */
class CTimer : public xengine::CActor
{
public:
    /**
     * @brief Constructor
     */
    CTimer();
    /**
     * @brief Destructor
     */
    ~CTimer();

protected:
    bool HandleInitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;
    void HandleDeinitialize() override;
    bool EnterLoop() override;
    void LeaveLoop() override;

    // Timeout callback function
    void TimerCallback(const boost::system::error_code& err);
    // Reset timer expriy time
    void NewTimer(const boost::system_time& expiryAt);

    // Handle set timer message
    virtual void SetTimer(const std::shared_ptr<CSetTimerMessage> spMsg);
    // Handle cancel timer message
    virtual void CancelTimer(const std::shared_ptr<CCancelTimerMessage> spMsg);

protected:
    struct CTagId
    {
    };
    struct CTagTime
    {
    };
    struct CTimerTask
    {
        boost::system_time expiryAt;
        std::shared_ptr<CTimeoutMessage> spTimeout;
    };
    typedef boost::multi_index_container<
        CTimerTask,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<
                boost::multi_index::tag<CTagId>,
                boost::multi_index::member<CTimerTask, std::shared_ptr<CTimeoutMessage>, &CTimerTask::spTimeout>>,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<CTagTime>,
                boost::multi_index::member<CTimerTask, boost::system_time, &CTimerTask::expiryAt>>>>
        CTimerTaskSet;

    CTimerTaskSet setTimerTask;
    boost::asio::deadline_timer timer;
};

} // namespace xengine

#endif //XENGINE_DOCKER_TIMER_H
