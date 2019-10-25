// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "docker.h"

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <vector>

#include "base/base.h"
#include "util.h"

using namespace std;
using namespace xengine;

namespace xengine
{

///////////////////////////////
// CDocker

CDocker::CDocker()
{
    pThreadTimer = nullptr;
    fActived = false;
    fShutdown = false;
}

CDocker::~CDocker()
{
    Exit();
}

bool CDocker::Initialize(CConfig* pConfigIn)
{
    fActived = false;
    fShutdown = false;

    pConfig = pConfigIn;

    if (pConfig == nullptr)
    {
        return false;
    }

    mapTimerById.clear();
    mapTimerByExpiry.clear();
    pThreadTimer = new boost::thread(boost::bind(&CDocker::TimerProc, this));
    if (pThreadTimer == nullptr)
    {
        return false;
    }

    tmNet.Clear();

    INFO("\n\n\n\n\n\n\n");
    INFO("WALL-E is being activatied..");

    INFO("#### Configuration : \n%s", pConfig->ListConfig().c_str());
    INFO("##################");

    return true;
}

bool CDocker::Attach(IBase* pBase)
{
    if (pBase == nullptr)
    {
        return false;
    }

    string& key = pBase->GetOwnKey();

    boost::unique_lock<boost::mutex> lock(mtxDocker);
    if (mapObj.count(key))
    {
        return false;
    }
    pBase->Connect(this);
    mapObj.insert(make_pair(key, pBase));
    listInstalled.push_back(pBase);

    return true;
}

void CDocker::Detach(string& key)
{
    IBase* pBase = nullptr;
    {
        boost::unique_lock<boost::mutex> lock(mtxDocker);
        map<string, IBase*>::iterator mi = mapObj.find(key);
        if (mi != mapObj.end())
        {
            pBase = (*mi).second;
            listInstalled.remove(pBase);
            mapObj.erase(mi);
        }
    }

    if (pBase != nullptr)
    {
        if (pBase->GetStatus() == STATUS_INVOKED)
        {
            pBase->Halt();
        }
        if (pBase->GetStatus() == STATUS_INITIALIZED)
        {
            pBase->Deinitialize();
        }
        delete pBase;
    }
}

bool CDocker::Run()
{
    vector<IBase*> vWorkQueue;
    {
        boost::unique_lock<boost::mutex> lock(mtxDocker);
        list<IBase*>::iterator it;
        for (it = listInstalled.begin(); it != listInstalled.end(); ++it)
        {
            Status status = (*it)->GetStatus();
            if (status == STATUS_ATTACHED)
            {
                vWorkQueue.push_back(*it);
            }
        }
    }

    vector<IBase*>::iterator it;
    for (it = vWorkQueue.begin(); it != vWorkQueue.end(); ++it)
    {
        if (!(*it)->Initialize())
        {
            Halt(vWorkQueue);
            return false;
        }
    }

    {
        boost::unique_lock<boost::mutex> lock(mtxDocker);
        list<IBase*>::iterator it;
        for (it = listInstalled.begin(); it != listInstalled.end(); ++it)
        {
            Status status = (*it)->GetStatus();
            if (status == STATUS_INITIALIZED
                && !count(vWorkQueue.begin(), vWorkQueue.end(), (*it)))
            {
                vWorkQueue.push_back(*it);
            }
        }
    }

    for (it = vWorkQueue.begin(); it != vWorkQueue.end(); ++it)
    {
        if (!(*it)->Invoke())
        {
            Halt(vWorkQueue);
            return false;
        }
    }

    {
        boost::unique_lock<boost::mutex> lock(mtxDocker);
        fActived = true;
        condDocker.notify_all();
    }

    return true;
}

bool CDocker::Run(string key)
{
    IBase* pBase = nullptr;
    {
        boost::unique_lock<boost::mutex> lock(mtxDocker);
        map<string, IBase*>::iterator mi = mapObj.find(key);
        if (mi != mapObj.end())
        {
            pBase = (*mi).second;
        }
    }

    if (pBase == nullptr)
    {
        return false;
    }

    if (pBase->GetStatus() == STATUS_ATTACHED)
    {
        pBase->Initialize();
    }

    if (pBase->GetStatus() == STATUS_INITIALIZED)
    {
        pBase->Invoke();
    }

    if (pBase->GetStatus() == STATUS_INVOKED)
    {
        return true;
    }

    if (pBase->GetStatus() != STATUS_ATTACHED)
    {
        Halt(key);
    }
    return false;
}

void CDocker::Halt()
{
    {
        boost::unique_lock<boost::mutex> lock(mtxDocker);
        fActived = false;
        condDocker.notify_all();
    }

    vector<IBase*> vWorkQueue;
    {
        boost::unique_lock<boost::mutex> lock(mtxDocker);
        vWorkQueue.assign(listInstalled.begin(), listInstalled.end());
    }

    Halt(vWorkQueue);
}

void CDocker::Halt(string key)
{
    IBase* pBase = nullptr;
    {
        boost::unique_lock<boost::mutex> lock(mtxDocker);
        map<string, IBase*>::iterator mi = mapObj.find(key);
        if (mi != mapObj.end())
        {
            pBase = (*mi).second;
        }
    }

    if (pBase->GetStatus() == STATUS_INVOKED)
    {
        pBase->Halt();
    }

    if (pBase->GetStatus() == STATUS_INITIALIZED)
    {
        pBase->Deinitialize();
    }

    CancelTimers(key);
}

void CDocker::Halt(vector<IBase*>& vWorkQueue)
{
    vector<IBase*>::reverse_iterator it;
    for (it = vWorkQueue.rbegin(); it != vWorkQueue.rend(); ++it)
    {
        if ((*it)->GetStatus() == STATUS_INVOKED)
        {
            (*it)->Halt();
        }
    }

    for (it = vWorkQueue.rbegin(); it != vWorkQueue.rend(); ++it)
    {
        if ((*it)->GetStatus() == STATUS_INITIALIZED)
        {
            (*it)->Deinitialize();
        }
    }

    for (it = vWorkQueue.rbegin(); it != vWorkQueue.rend(); ++it)
    {
        CancelTimers((*it)->GetOwnKey());
    }
}

void CDocker::Exit()
{
    if (fShutdown)
    {
        return;
    }

    fShutdown = true;

    Halt();

    {
        boost::unique_lock<boost::mutex> lock(mtxDocker);
        while (!listInstalled.empty())
        {
            IBase* pBase = listInstalled.back();
            listInstalled.pop_back();
            delete pBase;
        }
    }

    if (pThreadTimer)
    {
        {
            boost::unique_lock<boost::mutex> lock(mtxTimer);
            condTimer.notify_all();
        }
        pThreadTimer->join();
        delete pThreadTimer;
        pThreadTimer = nullptr;
    }

    INFO("WALL-E is deactivatied");
}

IBase* CDocker::GetObject(const string& key)
{
    {
        boost::unique_lock<boost::mutex> lock(mtxDocker);
        map<string, IBase*>::iterator mi = mapObj.find(key);
        if (mi != mapObj.end())
        {
            return (*mi).second;
        }
    }
    return nullptr;
}

void CDocker::FatalError(const std::string& key)
{
}

bool CDocker::ThreadStart(CThread& thr)
{
    try
    {
        boost::thread::attributes attr;
        attr.set_stack_size(16 * 1024 * 1024);
        thr.pThread = new boost::thread(attr, boost::bind(&CDocker::ThreadRun, this, boost::ref(thr)));
        return (thr.pThread != nullptr);
    }
    catch (exception& e)
    {
        ERROR("Thread (%s) start error: %s", thr.strThreadName.c_str(), e.what());
    }
    return false;
}

bool CDocker::ThreadDelayStart(CThread& thr)
{
    try
    {
        thr.pThread = new boost::thread(boost::bind(&CDocker::ThreadDelayRun, this, boost::ref(thr)));
        return (thr.pThread != nullptr);
    }
    catch (exception& e)
    {
        ERROR("Thread (%s) delay start error: %s", thr.strThreadName.c_str(), e.what());
    }
    return false;
}

void CDocker::ThreadExit(CThread& thr)
{
    thr.Exit();
}

void CDocker::ThreadRun(CThread& thr)
{
    INFO("Thread %s started", thr.strThreadName.c_str());
    SetThreadName(thr.strThreadName.c_str());
    try
    {
        thr.fRunning = true;
        thr.fnCallback();
        thr.fRunning = false;
    }
    catch (std::exception& e)
    {
        thr.fRunning = false;
        ERROR("Thread (%s) run error: %s", thr.strThreadName.c_str(), e.what());
    }
    catch (...)
    {
        thr.fRunning = false;
        throw; // support pthread_cancel()
    }
    INFO("Thread %s exiting", thr.strThreadName.c_str());
}

void CDocker::ThreadDelayRun(CThread& thr)
{
    {
        boost::unique_lock<boost::mutex> lock(mtxDocker);
        if (!fActived)
        {
            INFO("Thread %s delay to invoke", thr.strThreadName.c_str());
        }
        while (!fActived && !fShutdown)
        {
            condDocker.wait(lock);
        }
    }
    if (!fShutdown)
    {
        ThreadRun(thr);
    }
    else
    {
        INFO("Thread %s is not running before shutdown", thr.strThreadName.c_str());
    }
}

uint32 CDocker::SetTimer(const string& key, int64 nMilliSeconds, TimerCallback fnCallback)
{
    static uint32 nTimerIdAlloc = 0;
    uint32 nTimerId = 0;
    bool fUpdate = false;
    {
        boost::unique_lock<boost::mutex> lock(mtxTimer);

        boost::system_time tExpiryAt = boost::get_system_time()
                                       + boost::posix_time::milliseconds(nMilliSeconds);
        pair<map<uint32, CTimerTask>::iterator, bool> r;
        do
        {
            nTimerId = ++nTimerIdAlloc;
            if (nTimerId == 0)
            {
                continue;
            }
            r = mapTimerById.insert(make_pair(nTimerId, CTimerTask(key, nTimerId, tExpiryAt, fnCallback)));
        } while (!r.second);

        multimap<boost::system_time, uint32>::iterator it;
        it = mapTimerByExpiry.insert(make_pair(tExpiryAt, nTimerId));
        fUpdate = (it == mapTimerByExpiry.begin());
    }
    if (fUpdate)
    {
        condTimer.notify_all();
    }
    return nTimerId;
}

void CDocker::CancelTimer(const uint32 nTimerId)
{
    bool fUpdate = false;
    {
        boost::unique_lock<boost::mutex> lock(mtxTimer);
        map<uint32, CTimerTask>::iterator it = mapTimerById.find(nTimerId);
        if (it != mapTimerById.end())
        {
            CTimerTask& tmr = (*it).second;
            multimap<boost::system_time, uint32>::iterator mi;
            for (mi = mapTimerByExpiry.lower_bound(tmr.tExpiryAt);
                 mi != mapTimerByExpiry.upper_bound(tmr.tExpiryAt); ++mi)
            {
                if ((*mi).second == nTimerId)
                {
                    fUpdate = (mi == mapTimerByExpiry.begin());
                    mapTimerByExpiry.erase(mi);
                    break;
                }
            }
            mapTimerById.erase(it);
        }
    }
    if (fUpdate)
    {
        condTimer.notify_all();
    }
}

void CDocker::CancelTimers(const string& key)
{
    bool fUpdate = false;
    {
        boost::unique_lock<boost::mutex> lock(mtxTimer);
        multimap<boost::system_time, uint32>::iterator mi = mapTimerByExpiry.begin();
        while (mi != mapTimerByExpiry.end())
        {
            map<uint32, CTimerTask>::iterator it = mapTimerById.find((*mi).second);
            if (it != mapTimerById.end() && (*it).second.key == key)
            {
                if (mi == mapTimerByExpiry.begin())
                {
                    fUpdate = true;
                }
                mapTimerByExpiry.erase(mi++);
                mapTimerById.erase(it);
            }
            else
            {
                ++mi;
            }
        }
    }
    if (fUpdate)
    {
        condTimer.notify_all();
    }
}

int64 CDocker::GetSystemTime()
{
    return GetTime();
}

int64 CDocker::GetNetTime()
{
    return (GetTime() + tmNet.GetTimeOffset());
}

void CDocker::UpdateNetTime(const boost::asio::ip::address& address, int64 nTimeDelta)
{
    if (!tmNet.AddNew(address, nTimeDelta))
    {
        // warning..
    }
}

const CConfig* CDocker::GetConfig()
{
    return pConfig;
}

const string CDocker::GetWarnings()
{
    return "";
}

void CDocker::TimerProc()
{
    SetThreadName("Docker_Timer");
    while (!fShutdown)
    {
        vector<uint32> vWorkQueue;
        {
            boost::unique_lock<boost::mutex> lock(mtxTimer);
            if (fShutdown)
            {
                break;
            }
            multimap<boost::system_time, uint32>::iterator mi = mapTimerByExpiry.begin();
            if (mi == mapTimerByExpiry.end())
            {
                condTimer.wait(lock);
                continue;
            }
            boost::system_time const now = boost::get_system_time();
            if ((*mi).first > now)
            {
                condTimer.timed_wait(lock, (*mi).first);
            }
            else
            {
                while (mi != mapTimerByExpiry.end() && (*mi).first <= now)
                {
                    vWorkQueue.push_back((*mi).second);
                    mapTimerByExpiry.erase(mi++);
                }
            }
        }
        for (const uint32& nTimerId : vWorkQueue)
        {
            if (fShutdown)
            {
                break;
            }
            TimerCallback fnCallback;
            {
                boost::unique_lock<boost::mutex> lock(mtxTimer);
                map<uint32, CTimerTask>::iterator it = mapTimerById.find(nTimerId);
                if (it != mapTimerById.end())
                {
                    fnCallback = (*it).second.fnCallback;
                    mapTimerById.erase(it);
                }
            }
            if (fnCallback)
            {
                fnCallback(nTimerId);
            }
        }
    }
}

} // namespace xengine
