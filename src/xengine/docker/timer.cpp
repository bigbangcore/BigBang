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
  : CActor("timer"), timer(ioService)
{
}

CTimer::~CTimer()
{
    setTimerTask.clear();
}

bool CTimer::HandleInitialize()
{
    RegisterHandler({
        PTR_HANDLER(CSetTimerMessage, boost::bind(&CTimer::SetTimer, this, _1), true),
        PTR_HANDLER(CCancelTimerMessage, boost::bind(&CTimer::CancelTimer, this, _1), true),
    });
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
    DeregisterHandler();
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
        PUBLISH(it->spTimeout);
        index.erase(it++);
    }

    if (!index.empty())
    {
        NewTimer(index.begin()->expiryAt);
    }
}

void CTimer::SetTimer(const std::shared_ptr<CSetTimerMessage> spMsg)
{
    if (spMsg->spTimeout)
    {
        auto& index = setTimerTask.get<CTagId>();
        auto it = index.find(spMsg->spTimeout);
        if (it == index.end())
        {
            index.insert(CTimerTask{ spMsg->expiryAt, spMsg->spTimeout });
        }
        else
        {
            index.modify(it, [&spMsg](CTimerTask& task) { task.expiryAt = spMsg->expiryAt; });
        }

        if (timer.expires_at() > spMsg->expiryAt)
        {
            NewTimer(spMsg->expiryAt);
        }
    }
}

void CTimer::CancelTimer(const std::shared_ptr<CCancelTimerMessage> spMsg)
{
    setTimerTask.get<CTagId>().erase(spMsg->spTimeout);
}

void CTimer::NewTimer(const boost::system_time& expiryAt)
{
    timer.expires_at(expiryAt);
    timer.async_wait(boost::bind(&CTimer::TimerCallback, this, _1));
}

} // namespace xengine
