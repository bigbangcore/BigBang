// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_BASE_BASE_H
#define XENGINE_BASE_BASE_H

#include <boost/asio/ip/address.hpp>
#include <stdarg.h>
#include <string>

#include "docker/docker.h"
#include "event/event.h"

namespace xengine
{

typedef enum
{
    STATUS_OUTDOCKER = 0,
    STATUS_ATTACHED,
    STATUS_INITIALIZED,
    STATUS_INVOKED
} Status;

class IBase : virtual public CEventListener
{
public:
    IBase();
    IBase(const std::string& ownKeyIn);
    virtual ~IBase();
    void SetOwnKey(const std::string& ownKeyIn);
    std::string& GetOwnKey();
    Status GetStatus();
    void Connect(CDocker* pDockerIn);
    CDocker* Docker();

    bool Initialize();
    void Deinitialize();
    bool Invoke();
    void Halt();

protected:
    virtual bool HandleInitialize();
    virtual void HandleDeinitialize();
    virtual bool HandleInvoke();
    virtual void HandleHalt();
    template <typename T>
    bool GetObject(const std::string& key, T*& pObj);
    void FatalError();
    void Log(const char* pszFormat, ...);
    void Debug(const char* pszFormat, ...);
    void VDebug(const char* pszFormat, va_list ap);
    void Warn(const char* pszFormat, ...);
    void Error(const char* pszFormat, ...);
    bool ThreadStart(CThread& thr);
    bool ThreadDelayStart(CThread& thr);
    void ThreadExit(CThread& thr);
    uint32 SetTimer(int64 nMilliSeconds, TimerCallback fnCallback);
    void CancelTimer(uint32 nTimerId);
    int64 GetTime();
    int64 GetNetTime();
    void UpdateNetTime(const boost::asio::ip::address& address, int64 nTimeDelta);
    const CConfig* Config();
    const std::string GetWarnings();
    const std::string GetEpString(const boost::asio::ip::tcp::endpoint& ep);

private:
    std::string ownKey;
    CDocker* pDocker;
    Status status;
};

template <typename T>
bool IBase::GetObject(const std::string& key, T*& pObj)
{
    pObj = nullptr;
    if (pDocker != nullptr)
    {
        try
        {
            pObj = dynamic_cast<T*>(pDocker->GetObject(key));
        }
        catch (...)
        {
        }
    }
    return (pObj != nullptr);
}

} // namespace xengine

#endif //XENGINE_BASE_BASE_H
