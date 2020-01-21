// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_DOCKER_TIMER_H
#define XENGINE_DOCKER_TIMER_H

#include <boost/function.hpp>
#include <boost/thread/thread_time.hpp>
#include <string>

#include "type.h"

namespace xengine
{

typedef boost::function<void(uint64)> TimerCallback;

class CTimer
{
public:
    CTimer() {}
    CTimer(const std::string& keyIn, uint32 nTimerIdIn, boost::system_time& tExpiryAtIn,
           TimerCallback fnCallbackIn)
      : key(keyIn), nTimerId(nTimerIdIn), tExpiryAt(tExpiryAtIn), fnCallback(fnCallbackIn)
    {
    }

public:
    const std::string key;
    uint32 nTimerId;
    boost::system_time const tExpiryAt;
    TimerCallback fnCallback;
};

} // namespace xengine

#endif //XENGINE_DOCKER_TIMER_H
