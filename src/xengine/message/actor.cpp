// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "actor.h"

#include "message/message.h"

namespace xengine
{

CIOActor::CIOActor(const std::string& strOwnKeyIn)
  : IController(strOwnKeyIn),
    CIOActorWorker(strOwnKeyIn)
{
}

CIOActor::~CIOActor()
{
}

bool CIOActor::StartActor()
{
    if (!ThreadDelayStart(thrIOActorWorker))
    {
        ERROR("Failed to start thread");
        return false;
    }

    return true;
}

void CIOActor::StopActor()
{
    Stop();
}

void CIOActor::EnterLoop()
{
    CIOActorWorker::EnterLoop();
}

void CIOActor::LeaveLoop()
{
    CIOActorWorker::LeaveLoop();
}

void CIOActor::DeregisterHandler(const uint32 nType)
{
    CMessageCenter::GetInstance().Unsubscribe(nType, this);
    CIOActorWorker::DeregisterHandler(nType);
}

} // namespace xengine
