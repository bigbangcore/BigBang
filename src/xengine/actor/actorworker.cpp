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

void CIOActorWorker::Stop()
{
    if (!ioService.stopped())
    {
        ioService.stop();
    }

    thrIOActorWorker.Exit();
}

void CIOActorWorker::DeregisterHandler(const uint32 nType)
{
    mapHandler.erase(nType);
}

const std::string& CIOActorWorker::GetName() const
{
    return strName;
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

bool CIOActorWorker::EnterLoop()
{
    return true;
}

void CIOActorWorker::LeaveLoop()
{
}

void CIOActorWorker::HandlerThreadFunc()
{
    ioService.reset();

    if (!EnterLoop())
    {
        LOG_ERROR(strName, "Worker enter loop error");
    }
    else
    {
        try
        {
            ioService.run();
        }
        catch (const boost::system::system_error& err)
        {
            LOG_ERROR(strName, "Worker thread error: %s", err.what());
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(strName, "Failed to run CIOActorWorker io_service: %s\n", e.what());
        }
        catch (...)
        {
            LOG_ERROR(strName, "Worker thread unknown error");
        }
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
