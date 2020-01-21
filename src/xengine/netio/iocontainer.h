// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_NETIO_IOCONTAINER_H
#define XENGINE_NETIO_IOCONTAINER_H

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/function.hpp>
#include <boost/regex.hpp>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "netio/ioclient.h"

namespace xengine
{

class CIOProc;

class CIOContainer
{
public:
    CIOContainer(CIOProc* pIOProcIn);
    virtual ~CIOContainer();
    virtual void ClientClose(CIOClient* pClient) = 0;
    virtual std::size_t GetIdleCount();
    virtual const boost::asio::ip::tcp::endpoint GetServiceEndpoint();

protected:
    CIOProc* pIOProc;
};

class CIOCachedContainer : public CIOContainer
{
public:
    CIOCachedContainer(CIOProc* pIOProcIn);
    virtual ~CIOCachedContainer();
    void ClientClose(CIOClient* pClient) override;
    std::size_t GetIdleCount() override;
    virtual void IOInAccept() {}

protected:
    CIOClient* ClientAlloc();
    void Deactivate();
    bool PrepareClient(std::size_t nPrepare);
    void FreeClient(std::size_t nReserve = 0);

protected:
    std::queue<CIOClient*> queIdleClient;
    std::set<CIOClient*> setInuseClient;
};

class CIOInBound : public CIOCachedContainer
{
public:
    CIOInBound(CIOProc* pIOProcIn);
    ~CIOInBound();
    bool Invoke(const boost::asio::ip::tcp::endpoint& epListen, std::size_t nMaxConnection,
                const std::vector<std::string>& vAllowMask = std::vector<std::string>());
    void Halt();
    const boost::asio::ip::tcp::endpoint GetServiceEndpoint() override;
    void IOInAccept() override;

protected:
    bool BuildWhiteList(const std::vector<std::string>& vAllowMask);
    bool IsAllowedRemote(const boost::asio::ip::tcp::endpoint& ep);
    void HandleAccept(CIOClient* pClient, const boost::system::error_code& err);

protected:
    boost::asio::ip::tcp::acceptor acceptorService;
    boost::asio::ip::tcp::endpoint epService;
    std::vector<boost::regex> vWhiteList;
    int nAcceptCount;
};

class CIOOutBound : public CIOCachedContainer
{
public:
    CIOOutBound(CIOProc* pIOProcIn);
    ~CIOOutBound();
    bool Invoke(std::size_t nMaxConnection);
    void Halt();
    bool ConnectTo(const boost::asio::ip::tcp::endpoint& epRemote, int64 nTimeout);
    void Timeout(uint32 nTimerId);

protected:
    void HandleConnect(CIOClient* pClient, const boost::asio::ip::tcp::endpoint epRemote,
                       uint32 nTimerId, const boost::system::error_code& err);

protected:
    std::map<uint32, CIOClient*> mapPending;
};

class CIOSSLOption
{
public:
    CIOSSLOption(bool fEnableIn = false, bool fVerifyPeerIn = false,
                 const std::string& strPathCAIn = "",
                 const std::string& strPathCertIn = "",
                 const std::string& strPathPKIn = "",
                 const std::string& strCiphersIn = "",
                 const std::string& strPeerNameIn = "")
      : fEnable(fEnableIn), fVerifyPeer(fVerifyPeerIn), strPathCA(strPathCAIn),
        strPathCert(strPathCertIn), strPathPK(strPathPKIn),
        strCiphers(strCiphersIn), strPeerName(strPeerNameIn)
    {
    }
    bool SetupSSLContext(boost::asio::ssl::context& ctx) const;

public:
    bool fEnable;
    bool fVerifyPeer;
    std::string strPathCA;
    std::string strPathCert;
    std::string strPathPK;
    std::string strCiphers;
    std::string strPeerName;
};

class CIOSSLOutBound : public CIOContainer
{
public:
    CIOSSLOutBound(CIOProc* pIOProcIn);
    virtual ~CIOSSLOutBound();
    std::size_t GetIdleCount();
    void ClientClose(CIOClient* pClient);
    void Invoke(std::size_t nMaxConnIn);
    void Halt();
    bool ConnectTo(const boost::asio::ip::tcp::endpoint& epRemote, int64 nTimeout,
                   const CIOSSLOption& optSSL = CIOSSLOption());
    void Timeout(uint32 nTimerId);

protected:
    CIOClient* ClientAlloc(const CIOSSLOption& optSSL);
    void HandleConnect(CIOClient* pClient, const boost::asio::ip::tcp::endpoint epRemote,
                       uint32 nTimerId, const boost::system::error_code& err);

protected:
    size_t nMaxConn;
    std::map<uint32, CIOClient*> mapPending;
    std::set<CIOClient*> setInuseClient;
    CIOClient* pGarbageClient;
};

} // namespace xengine

#endif //XENGINE_NETIO_IOCONTAINER_H
