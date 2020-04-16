// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_NETIO_IOPROC_H
#define XENGINE_NETIO_IOPROC_H

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <map>
#include <memory>
#include <string>

#include "base/base.h"
#include "netio/ioclient.h"
#include "netio/iocontainer.h"
#include "netio/nethost.h"
#include "netio/netio.h"

namespace xengine
{

class CIOTimer
{
public:
    CIOTimer(uint32 nTimerIdIn, uint64 nNonceIn, const std::string& strFunctionIn, int64 nExpiryAtIn);

public:
    uint32 nTimerId;
    uint64 nNonce;
    std::string strFunction;
    int64 nExpiryAt;
};

class CIOCompletion
{
public:
    CIOCompletion();
    bool WaitForComplete(bool& fResultRet);
    void Completed(bool fResultIn);
    void Abort();
    void Reset();

protected:
    boost::condition_variable cond;
    boost::mutex mutex;
    bool fResult;
    bool fCompleted;
    bool fAborted;
};

class CIOProc : public IIOProc
{
    friend class CIOOutBound;
    friend class CIOSSLOutBound;
    friend class CIOInBound;

public:
    CIOProc(const std::string& ownKeyIn);
    virtual ~CIOProc();
    boost::asio::io_service& GetIoService();
    boost::asio::io_service::strand& GetIoStrand();
    virtual bool DispatchEvent(CEvent* pEvent) override;
    virtual CIOClient* CreateIOClient(CIOContainer* pContainer);

protected:
    bool HandleInvoke() override;
    void HandleHalt() override;

    uint32 SetTimer(uint64 nNonce, int64 nElapse, const std::string& strFunctionIn);
    void CancelTimer(uint32 nTimerId);
    void CancelClientTimers(uint64 nNonce);

    bool StartService(const boost::asio::ip::tcp::endpoint& epLocal, size_t nMaxConnections,
                      const std::vector<std::string>& vAllowMask = std::vector<std::string>());
    void StopService(const boost::asio::ip::tcp::endpoint& epLocal);

    bool Connect(const boost::asio::ip::tcp::endpoint& epRemote, int64 nTimeout);
    bool ConnectByBindAddress(const boost::asio::ip::tcp::endpoint& epLocal, const boost::asio::ip::tcp::endpoint& epRemote, int64 nTimeout);
    bool SSLConnect(const boost::asio::ip::tcp::endpoint& epRemote, int64 nTimeout,
                    const CIOSSLOption& optSSL = CIOSSLOption());
    bool SSLConnectByBindAddress(const boost::asio::ip::tcp::endpoint& epLocal, const boost::asio::ip::tcp::endpoint& epRemote, int64 nTimeout,
                                 const CIOSSLOption& optSSL = CIOSSLOption());
    std::size_t GetOutBoundIdleCount();
    void ResolveHost(const CNetHost& host);
    virtual void EnterLoop();
    virtual void LeaveLoop();
    virtual void HeartBeat();
    virtual void Timeout(uint64 nNonce, uint32 nTimerId, const std::string& strFunctionIn);
    virtual std::size_t GetMaxOutBoundCount();
    virtual bool ClientAccepted(const boost::asio::ip::tcp::endpoint& epService, CIOClient* pClient, std::string& strFailCause);
    virtual bool ClientConnected(CIOClient* pClient);
    virtual void ClientFailToConnect(const boost::asio::ip::tcp::endpoint& epRemote);
    virtual void HostResolved(const CNetHost& host, const boost::asio::ip::tcp::endpoint& ep);
    virtual void HostFailToResolve(const CNetHost& host);

private:
    void IOThreadFunc();
    void IOProcHeartBeat(const boost::system::error_code& err);
    void IOProcPollTimer();
    void IOProcHandleEvent(CEvent* pEvent, std::shared_ptr<CIOCompletion> spComplt);
    void IOProcHandleResolved(const CNetHost& host, const boost::system::error_code& err,
                              boost::asio::ip::tcp::resolver::iterator endpoint_iterator);

private:
    CThread thrIOProc;
    boost::asio::io_service ioService;
    boost::asio::io_service::strand ioStrand;
    boost::asio::ip::tcp::resolver resolverHost;
    std::map<boost::asio::ip::tcp::endpoint, CIOInBound*> mapService;
    CIOOutBound ioOutBound;
    CIOSSLOutBound ioSSLOutBound;

    boost::asio::deadline_timer timerHeartbeat;
    std::map<uint32, CIOTimer> mapTimerById;
    std::multimap<int64, uint32> mapTimerByExpiry;
};

} // namespace xengine

#endif //XENGINE_NETIO_IOPROC_H
