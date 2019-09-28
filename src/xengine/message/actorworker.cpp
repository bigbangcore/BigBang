// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "actorworker.h"

#include "logger.h"
#include "message/message.h"

namespace xengine
{

CIOActorWorker::CIOActorWorker(const std::string& strNameIn)
  : thrIOActorWorker(strNameIn, boost::bind(&CIOActorWorker::HandlerThreadFunc, this)),
    ioStrand(ioService), ioWork(ioService)
{
}

CIOActorWorker::~CIOActorWorker()
{
    mapHandler.clear();
}

void CIOActorWorker::Publish(const std::shared_ptr<CMessage> spMessage)
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
        // TODO: Replace newer api
        ErrorLog("", "Failed to run CIOActorWorker io_service: %s\n", err.what());
    }
    catch (...)
    {
        // TODO: Replace newer api
        ErrorLog("", "Failed to run CIOActorWorker io_service: unknown error\n");
    }

    LeaveLoop();
}

void CIOActorWorker::MessageHandler(const std::shared_ptr<CMessage> spMessage)
{
    auto it = mapHandler.find(spMessage->Type());
    if (it != mapHandler.end())
    {
        spMessage->Handle(it->second);
    }
}

} // namespace xengine
