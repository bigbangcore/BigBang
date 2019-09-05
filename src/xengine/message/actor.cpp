// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "actor.h"

#include "message/message.h"

namespace xengine
{

CIOActor::CIOActor(const std::string& ownKeyIn)
  : IBase(ownKeyIn),
    thrIOActor(ownKeyIn, boost::bind(&CIOActor::HandlerThreadFunc, this)),
    ioStrand(ioService), ioWork(ioService)
{
}

CIOActor::~CIOActor()
{
}

void CIOActor::Publish(std::shared_ptr<CMessage> spMessage)
{
    ioStrand.post(boost::bind(&CIOActor::MessageHandler, this, spMessage));
}

bool CIOActor::HandleInitialize()
{
    return true;
}

bool CIOActor::HandleInvoke()
{
    if (!ThreadDelayStart(thrIOActor))
    {
        Error("Failed to start iothread\n");
        return false;
    }

    return true;
}

void CIOActor::HandleHalt()
{
    if (!ioService.stopped())
    {
        ioService.stop();
    }

    thrIOActor.Interrupt();
    ThreadExit(thrIOActor);

    mapHandler.clear();
}

void CIOActor::HandleDeinitialize()
{
}

void CIOActor::EnterLoop()
{
}

void CIOActor::LeaveLoop()
{
}

void CIOActor::HandlerThreadFunc()
{
    ioService.reset();

    EnterLoop();

    try
    {
        ioService.run();
    }
    catch (const boost::system::system_error& err)
    {
        Error("Failed to run CIOActor io_service: %s\n", err.what());
    }
    catch (...)
    {
        Error("Failed to run CIOActor io_service: unknown error\n");
    }

    LeaveLoop();
}

void CIOActor::MessageHandler(std::shared_ptr<CMessage> spMessage)
{
    auto it = mapHandler.find(spMessage->Type());
    if (it != mapHandler.end())
    {
        spMessage->Handle(it->second);
    }
}

} // namespace xengine
