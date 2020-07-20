// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_NETIO_IOCLIENT_H
#define XENGINE_NETIO_IOCLIENT_H

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/function.hpp>
#include <string>

#include "stream/stream.h"

namespace xengine
{

class CIOContainer;

class CIOClient
{
public:
    typedef boost::function<void(std::size_t)> CallBackFunc;
    typedef boost::function<void(const boost::system::error_code&)> CallBackConn;

    CIOClient(CIOContainer* pContainerIn);
    virtual ~CIOClient();
    const boost::asio::ip::tcp::endpoint GetRemote();
    const boost::asio::ip::tcp::endpoint GetLocal();
    void Close();
    void Release();
    void Shutdown();
    void Accept(boost::asio::ip::tcp::acceptor& acceptor, CallBackConn fnAccepted);
    void Connect(const boost::asio::ip::tcp::endpoint& epRemote, CallBackConn fnConnected);
    void ConnectByBindAddress(const boost::asio::ip::tcp::endpoint& epLocal, const boost::asio::ip::tcp::endpoint& epRemote, CallBackConn fnConnected);
    void Read(CBufStream& ssRecv, std::size_t nLength, CallBackFunc fnCompleted);
    void ReadUntil(CBufStream& ssRecv, const std::string& delim, CallBackFunc fnCompleted);
    void Write(CBufStream& ssSend, CallBackFunc fnCompleted);

protected:
    void HandleCompleted(CallBackFunc fnCompleted,
                         const boost::system::error_code& err, std::size_t transferred);
    void HandleConnCompleted(CallBackConn fnCompleted, const boost::system::error_code& err);
    virtual const boost::asio::ip::tcp::endpoint SocketGetRemote() = 0;
    virtual const boost::asio::ip::tcp::endpoint SocketGetLocal() = 0;
    virtual void CloseSocket() = 0;
    virtual bool IsSocketOpen() = 0;
    virtual void AsyncAccept(boost::asio::ip::tcp::acceptor& acceptor, CallBackConn fnAccepted) = 0;
    virtual void AsyncConnect(const boost::asio::ip::tcp::endpoint& epRemote, CallBackConn fnConnected) = 0;
    virtual void AsyncConnectByBindAddress(const boost::asio::ip::tcp::endpoint& epLocal, const boost::asio::ip::tcp::endpoint& epRemote, CallBackConn fnConnected) = 0;
    virtual void AsyncRead(CBufStream& ssRecv, std::size_t nLength, CallBackFunc fnCompleted) = 0;
    virtual void AsyncReadUntil(CBufStream& ssRecv, const std::string& delim, CallBackFunc fnCompleted) = 0;
    virtual void AsyncWrite(CBufStream& ssSend, CallBackFunc fnCompleted) = 0;

protected:
    CIOContainer* pContainer;
    boost::asio::ip::tcp::endpoint epRemote;
    int nRefCount;
};

class CSocketClient : public CIOClient
{
public:
    CSocketClient(CIOContainer* pContainerIn, boost::asio::io_service& ioservice);
    ~CSocketClient();

protected:
    void AsyncAccept(boost::asio::ip::tcp::acceptor& acceptor, CallBackConn fnAccepted) override;
    void AsyncConnect(const boost::asio::ip::tcp::endpoint& epRemote, CallBackConn fnConnected) override;
    void AsyncConnectByBindAddress(const boost::asio::ip::tcp::endpoint& epLocal, const boost::asio::ip::tcp::endpoint& epRemote, CallBackConn fnConnected) override;
    void AsyncRead(CBufStream& ssRecv, std::size_t nLength, CallBackFunc fnCompleted) override;
    void AsyncReadUntil(CBufStream& ssRecv, const std::string& delim, CallBackFunc fnCompleted) override;
    void AsyncWrite(CBufStream& ssSend, CallBackFunc fnCompleted) override;
    const boost::asio::ip::tcp::endpoint SocketGetRemote() override;
    const boost::asio::ip::tcp::endpoint SocketGetLocal() override;
    void CloseSocket() override;
    bool IsSocketOpen() override;

protected:
    boost::asio::ip::tcp::socket sockClient;
};

class CSSLClient : public CIOClient
{
public:
    CSSLClient(CIOContainer* pContainerIn, boost::asio::io_service& ioservice,
               boost::asio::ssl::context& context,
               const std::string& strVerifyHost = "");
    ~CSSLClient();

protected:
    void AsyncAccept(boost::asio::ip::tcp::acceptor& acceptor, CallBackConn fnAccepted) override;
    void AsyncConnect(const boost::asio::ip::tcp::endpoint& epRemote, CallBackConn fnConnected) override;
    void AsyncConnectByBindAddress(const boost::asio::ip::tcp::endpoint& epLocal, const boost::asio::ip::tcp::endpoint& epRemote, CallBackConn fnConnected) override;
    void AsyncRead(CBufStream& ssRecv, std::size_t nLength, CallBackFunc fnCompleted) override;
    void AsyncReadUntil(CBufStream& ssRecv, const std::string& delim, CallBackFunc fnCompleted) override;
    void AsyncWrite(CBufStream& ssSend, CallBackFunc fnCompleted) override;
    const boost::asio::ip::tcp::endpoint SocketGetRemote() override;
    const boost::asio::ip::tcp::endpoint SocketGetLocal() override;
    void CloseSocket() override;
    bool IsSocketOpen() override;
    void HandleConnected(CallBackConn fnHandshaked,
                         boost::asio::ssl::stream_base::handshake_type type,
                         const boost::system::error_code& err);
    bool VerifyCertificate(const std::string& strVerifyHost, bool fPreverified,
                           boost::asio::ssl::verify_context& ctx);

protected:
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> sslClient;
};

} // namespace xengine

#endif //XENGINE_NETIO_IOCLIENT_H
