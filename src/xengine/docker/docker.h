// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_DOCKER_DOCKER_H
#define XENGINE_DOCKER_DOCKER_H

#include <boost/asio/ip/address.hpp>
#include <boost/thread/thread.hpp>
#include <list>
#include <map>
#include <stdarg.h>
#include <string>

#include "docker/config.h"
#include "docker/log.h"
#include "docker/nettime.h"
#include "docker/thread.h"
#include "docker/timer.h"

namespace xengine
{

class IBase;

class CDocker
{
public:
    CDocker();
    ~CDocker();
    bool Initialize(CConfig* pConfigIn, CLog* pLogIn = nullptr);

    bool Attach(IBase* pBase);
    void Detach(std::string& key);

    bool Run();
    bool Run(std::string key);
    void Halt();
    void Halt(std::string key);

    void Exit();

    IBase* GetObject(const std::string& key);
    void FatalError(const std::string& key);
    void LogOutput(const char* key, const char* strPrefix, const char* pszFormat, va_list ap);
    bool ThreadStart(CThread& thr);
    bool ThreadDelayStart(CThread& thr);
    void ThreadExit(CThread& thr);
    void ThreadRun(CThread& thr);
    void ThreadDelayRun(CThread& thr);
    uint32 SetTimer(const std::string& key, int64 nMilliSeconds, TimerCallback fnCallback);
    void CancelTimer(const uint32 nTimerId);
    void CancelTimers(const std::string& key);
    int64 GetSystemTime();
    int64 GetNetTime();
    void UpdateNetTime(const boost::asio::ip::address& address, int64 nTimeDelta);
    const CConfig* GetConfig();
    const std::string GetWarnings();

protected:
    void Halt(std::vector<IBase*>& vWorkQueue);
    void Log(const char* pszFormat, ...);
    void LogException(const char* pszThread, std::exception* pex);
    void ListConfig();
    void TimerProc();

protected:
    boost::mutex mtxDocker;
    boost::condition_variable condDocker;
    std::map<std::string, IBase*> mapObj;
    std::list<IBase*> listInstalled;

    CNetTime tmNet;
    CConfig* pConfig;
    CLog* pLog;

    bool fActived;
    bool fShutdown;

    boost::thread* pThreadTimer;
    boost::mutex mtxTimer;
    boost::condition_variable condTimer;
    std::map<uint32, CTimer> mapTimerById;
    std::multimap<boost::system_time, uint32> mapTimerByExpiry;
};

} // namespace xengine

#endif //XENGINE_DOCKER_DOCKER_H
