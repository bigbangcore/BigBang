// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "actorworker.h"

#include "docker/docker.h"
#include "logger.h"
#include "message/message.h"
#include "message/messagecenter.h"

namespace xengine
{

CActorWorker::CActorWorker(const std::string& strNameIn, const std::vector<CMessageHandler>& vecHandler)
  : strName(strNameIn), ioStrand(ioService), ioWork(ioService),
    thr(strNameIn, boost::bind(&CActorWorker::HandlerThreadFunc, this))
{
    RegisterHandler(vecHandler);
}

CActorWorker::~CActorWorker()
{
    Stop();

    DeregisterHandler();
}

void CActorWorker::RegisterHandler(const CMessageHandler& handler)
{
    auto it = mapHandler.find(handler.nType);
    if (it != mapHandler.end())
    {
        LOG_WARN(strName.c_str(), "Overwrite already registered message: %s", handler.strTag.c_str());
        mapHandler.erase(it);
    }
    mapHandler.insert(std::make_pair(handler.nType, handler));

    // register to message center
    if (handler.fGlobal)
    {
        CMessageCenter::GetInstance().Subscribe(handler.nType, this);
    }
}

void CActorWorker::RegisterHandler(const std::vector<CMessageHandler>& vecHandler)
{
    for (auto& handler : vecHandler)
    {
        RegisterHandler(handler);
    }
}

void CActorWorker::Start()
{
    thr.Run();
}

void CActorWorker::Stop()
{
    if (!ioService.stopped())
    {
        ioService.stop();
    }
    thr.Exit();
}

void CActorWorker::DeregisterHandler(const uint32 nType)
{
    auto it = mapHandler.find(nType);
    if (it != mapHandler.end())
    {
        auto& handler = it->second;
        if (handler.fGlobal)
        {
            CMessageCenter::GetInstance().Unsubscribe(handler.nType, this);
        }
        mapHandler.erase(it);
    }
}

void CActorWorker::DeregisterHandler()
{
    for (auto& e : mapHandler)
    {
        auto& handler = e.second;
        if (handler.fGlobal)
        {
            CMessageCenter::GetInstance().Unsubscribe(handler.nType, this);
        }
    }
    mapHandler.empty();
}

void CActorWorker::Publish(const std::shared_ptr<CMessage> spMessage)
{
    ioStrand.post(boost::bind(&CActorWorker::MessageHandler, this, spMessage));
}

const std::string& CActorWorker::GetName() const
{
    return strName;
}

boost::asio::io_service& CActorWorker::GetService()
{
    return ioService;
}

boost::asio::io_service::strand& CActorWorker::GetStrand()
{
    return ioStrand;
}

CThread& CActorWorker::GetThread()
{
    return thr;
}

bool CActorWorker::EnterLoop()
{
    return true;
}

void CActorWorker::LeaveLoop()
{
}

void CActorWorker::HandlerThreadFunc()
{
    ioService.reset();

    if (!EnterLoop())
    {
        LOG_ERROR(strName.c_str(), "Worker enter loop error");
    }
    else
    {
        try
        {
            ioService.run();
        }
        catch (const boost::system::system_error& err)
        {
            LOG_ERROR(strName.c_str(), "Worker thread error: %s", err.what());
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(strName.c_str(), "Failed to run CActorWorker io_service: %s", e.what());
        }
        catch (...)
        {
            LOG_ERROR(strName.c_str(), "Worker thread unknown error");
        }
    }

    LeaveLoop();
}

void CActorWorker::MessageHandler(const std::shared_ptr<CMessage> spMessage)
{
    auto it = mapHandler.find(spMessage->Type());
    if (it != mapHandler.end())
    {
        spMessage->Handle(it->second.handler);
    }
}

} // namespace xengine
