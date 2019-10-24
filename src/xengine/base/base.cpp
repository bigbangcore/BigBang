// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base/base.h"

#include <boost/bind.hpp>
#include <vector>

#include "event/eventproc.h"

using namespace std;

namespace xengine
{

///////////////////////////////
// IBase

IBase::IBase()
{
    status = STATUS_OUTDOCKER;
}

IBase::IBase(const string& strOwnKeyIn)
{
    status = STATUS_OUTDOCKER;
    strOwnKey = strOwnKeyIn;
}

IBase::~IBase()
{
}

void IBase::SetOwnKey(const string& strOwnKeyIn)
{
    strOwnKey = strOwnKeyIn;
}

string& IBase::GetOwnKey()
{
    return strOwnKey;
}

Status IBase::GetStatus()
{
    return status;
}

void IBase::Connect(CDocker* pDockerIn)
{
    pDocker = pDockerIn;
    status = STATUS_ATTACHED;
}

CDocker* IBase::Docker()
{
    return pDocker;
}

bool IBase::Initialize()
{
    INFO("Initializing..");
    if (!HandleInitialize())
    {
        INFO("Failed to initialize");
        return false;
    }
    INFO("Initialized");
    status = STATUS_INITIALIZED;
    return true;
}

void IBase::Deinitialize()
{
    INFO("Deinitializing..");

    HandleDeinitialize();

    INFO("Deinitialized..");
    status = STATUS_ATTACHED;
}

bool IBase::Invoke()
{
    INFO("Invoking..");
    if (!HandleInvoke())
    {
        INFO("Failed to invoke");
        return false;
    }
    INFO("Invoked");
    status = STATUS_INVOKED;
    return true;
}

void IBase::Halt()
{
    INFO("Halting..");

    HandleHalt();

    INFO("Halted");
    status = STATUS_INITIALIZED;
}

bool IBase::HandleInitialize()
{
    return true;
}

void IBase::HandleDeinitialize()
{
}

bool IBase::HandleInvoke()
{
    return true;
}

void IBase::HandleHalt()
{
}

void IBase::FatalError()
{
    INFO("Fatal Error!!!!!");
    pDocker->FatalError(strOwnKey.c_str());
}

bool IBase::ThreadStart(CThread& thr)
{
    if (pDocker == nullptr)
    {
        return false;
    }
    return pDocker->ThreadStart(thr);
}

bool IBase::ThreadDelayStart(CThread& thr)
{
    if (pDocker == nullptr)
    {
        return false;
    }
    return pDocker->ThreadDelayStart(thr);
}

void IBase::ThreadExit(CThread& thr)
{
    if (pDocker != nullptr)
    {
        pDocker->ThreadExit(thr);
    }
}

uint32 IBase::SetTimer(int64 nMilliSeconds, TimerCallback fnCallback)
{
    if (pDocker != nullptr)
    {
        return pDocker->SetTimer(strOwnKey, nMilliSeconds, fnCallback);
    }
    return 0;
}

void IBase::CancelTimer(uint32 nTimerId)
{
    if (pDocker != nullptr)
    {
        pDocker->CancelTimer(nTimerId);
    }
}

int64 IBase::GetTime()
{
    return (pDocker != nullptr ? pDocker->GetSystemTime() : 0);
}

int64 IBase::GetNetTime()
{
    return (pDocker != nullptr ? pDocker->GetNetTime() : 0);
}

void IBase::UpdateNetTime(const boost::asio::ip::address& address, int64 nTimeDelta)
{
    if (pDocker != nullptr)
    {
        pDocker->UpdateNetTime(address, nTimeDelta);
    }
}

const CConfig* IBase::Config()
{
    return (pDocker != nullptr ? pDocker->GetConfig() : nullptr);
}

const string IBase::GetWarnings()
{
    return (pDocker != nullptr ? pDocker->GetWarnings() : "");
}

} // namespace xengine
