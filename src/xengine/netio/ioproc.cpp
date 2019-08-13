// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ioproc.h"

#include <boost/bind.hpp>

using namespace std;
using boost::asio::ip::tcp;

#define IOPROC_HEARTBEAT (boost::posix_time::seconds(1))
#define DEFAULT_MAX_OUTBOUND (64)

namespace xengine
{

///////////////////////////////
// CIOTimer

CIOTimer::CIOTimer(uint32 nTimerIdIn, uint64 nNonceIn, int64 nExpiryAtIn)
  : nTimerId(nTimerIdIn), nNonce(nNonceIn), nExpiryAt(nExpiryAtIn)
{
}

///////////////////////////////
// CIOCompletion
CIOCompletion::CIOCompletion()
  : fResult(false), fCompleted(false), fAborted(false)
{
}

bool CIOCompletion::WaitForComplete(bool& fResultRet)
{
    boost::unique_lock<boost::mutex> lock(mutex);
    while (!fCompleted && !fAborted)
    {
        cond.wait(lock);
    }
    fResultRet = fResult;
    return fCompleted;
}

void CIOCompletion::Completed(bool fResultIn)
{
    {
        boost::lock_guard<boost::mutex> lock(mutex);
        fResult = fResultIn;
        fCompleted = true;
    }
    cond.notify_all();
}

void CIOCompletion::Abort()
{
    {
        boost::lock_guard<boost::mutex> lock(mutex);
        fAborted = true;
    }
    cond.notify_all();
}

void CIOCompletion::Reset()
{
    boost::lock_guard<boost::mutex> lock(mutex);
    fResult = false;
    fCompleted = false;
    fAborted = false;
}

///////////////////////////////
// CIOProc

CIOProc::CIOProc(const string& ownKeyIn)
  : IIOProc(ownKeyIn),
    thrIOProc(ownKeyIn, boost::bind(&CIOProc::IOThreadFunc, this)),
    ioStrand(ioService), resolverHost(ioService), ioOutBound(this), ioSSLOutBound(this),
    timerHeartbeat(ioService, IOPROC_HEARTBEAT)
{
}

CIOProc::~CIOProc()
{
}

boost::asio::io_service& CIOProc::GetIoService()
{
    return ioService;
}

boost::asio::io_service::strand& CIOProc::GetIoStrand()
{
    return ioStrand;
}

bool CIOProc::DispatchEvent(CEvent* pEvent)
{
    bool fResult = false;
    shared_ptr<CIOCompletion> spComplt(new CIOCompletion);
    try
    {
        ioStrand.dispatch(boost::bind(&CIOProc::IOProcHandleEvent, this, pEvent, spComplt));
        spComplt->WaitForComplete(fResult);
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return fResult;
}

CIOClient* CIOProc::CreateIOClient(CIOContainer* pContainer)
{
    return new CSocketClient(pContainer, ioService);
}

bool CIOProc::HandleInvoke()
{
    if (!ioOutBound.Invoke(GetMaxOutBoundCount()))
    {
        Error("Failed to invoke IOOutBound\n");
        return false;
    }

    ioSSLOutBound.Invoke(GetMaxOutBoundCount());

    if (!ThreadDelayStart(thrIOProc))
    {
        Error("Failed to start iothread\n");
        return false;
    }

    return true;
}

void CIOProc::HandleHalt()
{
    if (!ioService.stopped())
    {
        ioService.stop();
    }
    thrIOProc.Interrupt();
    ThreadExit(thrIOProc);

    ioOutBound.Halt();
    ioSSLOutBound.Halt();

    for (map<tcp::endpoint, CIOInBound*>::iterator it = mapService.begin();
         it != mapService.end(); ++it)
    {
        delete ((*it).second);
    }
    mapService.clear();
}

uint32 CIOProc::SetTimer(uint64 nNonce, int64 nElapse)
{
    static uint32 nTimerId = 0;
    while (nTimerId == 0 || mapTimerById.count(nTimerId))
    {
        nTimerId++;
    }
    map<uint32, CIOTimer>::iterator it;
    it = mapTimerById.insert(make_pair(nTimerId, CIOTimer(nTimerId, nNonce, GetTime() + nElapse))).first;
    CIOTimer* pTimer = &(*it).second;
    mapTimerByExpiry.insert(make_pair(pTimer->nExpiryAt, nTimerId));

    return nTimerId;
}

void CIOProc::CancelTimer(uint32 nTimerId)
{
    if (nTimerId == 0)
    {
        return;
    }

    map<uint32, CIOTimer>::iterator it = mapTimerById.find(nTimerId);
    if (it == mapTimerById.end())
    {
        return;
    }

    CIOTimer* pTimer = &(*it).second;

    multimap<int64, uint32>::iterator li = mapTimerByExpiry.lower_bound(pTimer->nExpiryAt);
    multimap<int64, uint32>::iterator ui = mapTimerByExpiry.upper_bound(pTimer->nExpiryAt);
    for (multimap<int64, uint32>::iterator mi = li; mi != ui && mi != mapTimerByExpiry.end(); ++mi)
    {
        if ((*mi).second == nTimerId)
        {
            mapTimerByExpiry.erase(mi);
            break;
        }
    }

    mapTimerById.erase(it);
}

void CIOProc::CancelClientTimers(uint64 nNonce)
{
    vector<uint32> vTimerId;
    for (map<uint32, CIOTimer>::iterator it = mapTimerById.begin(); it != mapTimerById.end(); ++it)
    {
        if ((*it).second.nNonce == nNonce)
        {
            vTimerId.push_back((*it).first);
        }
    }

    for (const uint32 nTimerId : vTimerId)
    {
        CancelTimer(nTimerId);
    }
}

bool CIOProc::StartService(const tcp::endpoint& epLocal, size_t nMaxConnections, const vector<string>& vAllowMask)
{
    map<tcp::endpoint, CIOInBound*>::iterator it = mapService.find(epLocal);
    if (it == mapService.end())
    {
        it = mapService.insert(make_pair(epLocal, new CIOInBound(this))).first;
    }
    return ((*it).second->Invoke(epLocal, nMaxConnections, vAllowMask));
}

void CIOProc::StopService(const tcp::endpoint& epLocal)
{
    map<tcp::endpoint, CIOInBound*>::iterator it = mapService.find(epLocal);
    if (it != mapService.end())
    {
        delete ((*it).second);
        mapService.erase(it);
    }
}

bool CIOProc::Connect(const tcp::endpoint& epRemote, int64 nTimeout)
{
    return ioOutBound.ConnectTo(epRemote, nTimeout);
}

bool CIOProc::SSLConnect(const tcp::endpoint& epRemote, int64 nTimeout, const CIOSSLOption& optSSL)
{
    return ioSSLOutBound.ConnectTo(epRemote, nTimeout, optSSL);
}

size_t CIOProc::GetOutBoundIdleCount()
{
    return (ioOutBound.GetIdleCount());
}

void CIOProc::ResolveHost(const CNetHost& host)
{
    std::stringstream ss;
    ss << host.nPort;
    tcp::resolver::query query(host.strHost, ss.str());
    resolverHost.async_resolve(query,
                               boost::bind(&CIOProc::IOProcHandleResolved, this, host,
                                           boost::asio::placeholders::error,
                                           boost::asio::placeholders::iterator));
}

void CIOProc::EnterLoop()
{
}

void CIOProc::LeaveLoop()
{
}

void CIOProc::HeartBeat()
{
}

void CIOProc::Timeout(uint64 nNonce, uint32 nTimerId)
{
}

size_t CIOProc::GetMaxOutBoundCount()
{
    return DEFAULT_MAX_OUTBOUND;
}

bool CIOProc::ClientAccepted(const tcp::endpoint& epService, CIOClient* pClient)
{
    return false;
}

bool CIOProc::ClientConnected(CIOClient* pClient)
{
    return false;
}

void CIOProc::ClientFailToConnect(const tcp::endpoint& epRemote)
{
}

void CIOProc::HostResolved(const CNetHost& host, const tcp::endpoint& ep)
{
}

void CIOProc::HostFailToResolve(const CNetHost& host)
{
}

void CIOProc::IOThreadFunc()
{
    ioService.reset();

    timerHeartbeat.async_wait(boost::bind(&CIOProc::IOProcHeartBeat, this, _1));

    EnterLoop();

    ioService.run();

    LeaveLoop();

    timerHeartbeat.cancel();

    mapTimerById.clear();

    mapTimerByExpiry.clear();
}

void CIOProc::IOProcHeartBeat(const boost::system::error_code& err)
{
    if (!err)
    {
        /* restart deadline timer */
        timerHeartbeat.expires_at(timerHeartbeat.expires_at() + IOPROC_HEARTBEAT);
        timerHeartbeat.async_wait(boost::bind(&CIOProc::IOProcHeartBeat, this, _1));

        /* handle io timer */
        IOProcPollTimer();

        /* heartbeat callback */
        HeartBeat();
    }
}

void CIOProc::IOProcPollTimer()
{
    int64 now = GetTime();
    multimap<int64, uint32>::iterator ui = mapTimerByExpiry.upper_bound(now + 1);
    vector<uint32> vecExpires;

    for (multimap<int64, uint32>::iterator it = mapTimerByExpiry.begin(); it != ui; ++it)
    {
        vecExpires.push_back((*it).second);
    }

    mapTimerByExpiry.erase(mapTimerByExpiry.begin(), ui);

    for (uint32& nTimerId : vecExpires)
    {
        map<uint32, CIOTimer>::iterator it = mapTimerById.find(nTimerId);
        if (it != mapTimerById.end())
        {
            uint64 nNonce = (*it).second.nNonce;
            mapTimerById.erase(it);
            if (nNonce == 0)
            {
                ioOutBound.Timeout(nTimerId);
                ioSSLOutBound.Timeout(nTimerId);
            }
            else
            {
                Timeout(nNonce, nTimerId);
            }
        }
    }
}

void CIOProc::IOProcHandleEvent(CEvent* pEvent, shared_ptr<CIOCompletion> spComplt)
{
    spComplt->Completed(pEvent->Handle(*this));
}

void CIOProc::IOProcHandleResolved(const CNetHost& host, const boost::system::error_code& err,
                                   tcp::resolver::iterator endpoint_iterator)
{
    if (!err)
    {
        HostResolved(host, *endpoint_iterator);
    }
    else
    {
        HostFailToResolve(host);
    }
}

} // namespace xengine
