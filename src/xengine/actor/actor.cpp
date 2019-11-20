// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "actor.h"

#include "message/message.h"

namespace xengine
{

CActor::CActor(const std::string& strOwnKeyIn)
  : IController(strOwnKeyIn),
    CActorWorker(strOwnKeyIn)
{
}

CActor::~CActor()
{
}

bool CActor::StartActor()
{
    if (!ThreadDelayStart(thr))
    {
        ERROR("Failed to start thread");
        return false;
    }

    return true;
}

void CActor::StopActor()
{
    Stop();
}

bool CActor::EnterLoop()
{
    return true;
}

void CActor::LeaveLoop()
{
}

} // namespace xengine
