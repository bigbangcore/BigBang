// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "actorworker.h"

#include "logger.h"
#include "message/message.h"

namespace xengine
{

CIOActorWorker::CIOActorWorker(const std::string& strNameIn)
  : strName(strNameIn), ioStrand(ioService), ioWork(ioService),
    thrIOActorWorker(strNameIn, boost::bind(&CIOActorWorker::HandlerThreadFunc, this))
{
}

CIOActorWorker::~CIOActorWorker()
{
    mapHandler.clear();
}

void CIOActorWorker::Publish(std::shared_ptr<CMessage> spMessage)
{
    ioStrand.post(boost::bind(&CIOActorWorker::MessageHandler, this, spMessage));
}

void CIOActorWorker::EnterLoop()
{
}

void CIOActorWorker::LeaveLoop()
{
}

void CIOActorWorker::DeregisterHandler(const uint32 nType)
{
    mapHandler.erase(nType);
}

void CIOActorWorker::Stop()
{
    if (!ioService.stopped())
    {
        ioService.stop();
    }

    thrIOActorWorker.Interrupt();
    thrIOActorWorker.Exit();
}

boost::asio::io_service& CIOActorWorker::GetService()
{
    return ioService;
}

boost::asio::io_service::strand& CIOActorWorker::GetStrand()
{
    return ioStrand;
}

CThread& CIOActorWorker::GetThread()
{
    return thrIOActorWorker;
}

void CIOActorWorker::HandlerThreadFunc()
{
    ioService.reset();

    EnterLoop();

    try
    {
        ioService.run();
    }
    catch (const boost::system::system_error& err)
    {
        LOG_ERROR(strName, "Worker thread error: %s", err.what());
    }
    catch (...)
    {
        LOG_ERROR(strName, "Worker thread unknown error");
    }

    LeaveLoop();
}

void CIOActorWorker::MessageHandler(std::shared_ptr<CMessage> spMessage)
{
    auto it = mapHandler.find(spMessage->Type());
    if (it != mapHandler.end())
    {
        spMessage->Handle(it->second);
    }
}

} // namespace xengine
