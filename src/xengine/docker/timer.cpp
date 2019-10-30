// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "timer.h"

#include <boost/function.hpp>
#include <boost/thread/thread_time.hpp>
#include <string>

#include "actor/actor.h"
#include "message/message.h"
#include "type.h"

namespace xengine
{

CTimer::CTimer()
  : CIOActor("timer"), timer(ioService)
{
}

CTimer::~CTimer()
{
    setTimerTask.clear();
}

bool CTimer::HandleInitialize()
{
    RegisterRefHandler<CSetTimerMessage>(boost::bind(&CTimer::SetTimer, this, _1));
    RegisterRefHandler<CCancelTimerMessage>(boost::bind(&CTimer::CancelTimer, this, _1));
    return true;
}

bool CTimer::HandleInvoke()
{
    return StartActor();
}

void CTimer::HandleHalt()
{
    StopActor();
}

void CTimer::HandleDeinitialize()
{
    DeregisterHandler(CSetTimerMessage::MessageType());
    DeregisterHandler(CCancelTimerMessage::MessageType());
}

bool CTimer::EnterLoop()
{
    return true;
}

void CTimer::LeaveLoop()
{
    timer.cancel();
}

void CTimer::TimerCallback(const boost::system::error_code& err)
{
    auto& index = setTimerTask.get<CTagTime>();
    const boost::system_time now = boost::get_system_time();
    for (auto it = index.begin(); it != index.end() && it->expiryAt <= now;)
    {
        PUBLISH_MESSAGE(it->spTimeout);
        index.erase(it++);
    }

    if (!index.empty())
    {
        NewTimer(index.begin()->expiryAt);
    }
}

void CTimer::SetTimer(const CSetTimerMessage& message)
{
    if (message.spTimeout)
    {
        auto& index = setTimerTask.get<CTagId>();
        auto it = index.find(message.spTimeout);
        if (it == index.end())
        {
            index.insert(CTimerTask{ message.expiryAt, message.spTimeout });
        }
        else
        {
            index.modify(it, [&message](CTimerTask& task) { task.expiryAt = message.expiryAt; });
        }

        if (timer.expires_at() > message.expiryAt)
        {
            NewTimer(message.expiryAt);
        }
    }
}

void CTimer::CancelTimer(const CCancelTimerMessage& message)
{
    setTimerTask.get<CTagId>().erase(message.spTimeout);
}

void CTimer::NewTimer(const boost::system_time& expiryAt)
{
    timer.expires_at(expiryAt);
    timer.async_wait(boost::bind(&CTimer::TimerCallback, this, _1));
}

} // namespace xengine
