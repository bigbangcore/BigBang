// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ioclient.h"

#include <boost/asio/ssl/rfc2818_verification.hpp>
#include <boost/bind.hpp>

#include "iocontainer.h"
#include "util.h"

#ifdef __CYGWIN__
#ifndef TCP_KEEPIDLE
#define TCP_KEEPIDLE 4     /* Start keeplives after this period */
#endif
 
#ifndef TCP_KEEPINTVL
#define TCP_KEEPINTVL 5    /* Interval between keepalives */
#endif
 
#ifndef TCP_KEEPCNT
#define TCP_KEEPCNT 6      /* Number of keepalives before death */
#endif
#endif

using namespace std;
using boost::asio::ip::tcp;

namespace xengine
{

///////////////////////////////
// CIOClient
CIOClient::CIOClient(CIOContainer* pContainerIn)
  : pContainer(pContainerIn)
{
    nRefCount = 0;
}

CIOClient::~CIOClient()
{
}

const tcp::endpoint CIOClient::GetRemote()
{
    if (epRemote == tcp::endpoint())
    {
        try
        {
            epRemote = SocketGetRemote();
        }
        catch (exception& e)
        {
            StdError(__PRETTY_FUNCTION__, e.what());
        }
    }
    return epRemote;
}

const tcp::endpoint CIOClient::GetLocal()
{
    try
    {
        return SocketGetLocal();
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
    }
    return (tcp::endpoint());
}

void CIOClient::Close()
{
    if (IsSocketOpen())
    {
        CloseSocket();
        epRemote = tcp::endpoint();
        Release();
    }
}

void CIOClient::Release()
{
    if (!(--nRefCount))
    {
        pContainer->ClientClose(this);
    }
}

void CIOClient::Shutdown()
{
    CloseSocket();
    epRemote = tcp::endpoint();
}

void CIOClient::Accept(tcp::acceptor& acceptor, CallBackConn fnAccepted)
{
    ++nRefCount;
    AsyncAccept(acceptor, fnAccepted);
}

void CIOClient::Connect(const tcp::endpoint& epRemote, CallBackConn fnConnected)
{
    ++nRefCount;
    AsyncConnect(epRemote, fnConnected);
}

void CIOClient::Read(CBufStream& ssRecv, size_t nLength, CallBackFunc fnCompleted)
{
    ++nRefCount;
    AsyncRead(ssRecv, nLength, fnCompleted);
}

void CIOClient::ReadUntil(CBufStream& ssRecv, const string& delim, CallBackFunc fnCompleted)
{
    ++nRefCount;
    AsyncReadUntil(ssRecv, delim, fnCompleted);
}

void CIOClient::Write(CBufStream& ssSend, CallBackFunc fnCompleted)
{
    ++nRefCount;
    AsyncWrite(ssSend, fnCompleted);
}

void CIOClient::HandleCompleted(CallBackFunc fnCompleted,
                                const boost::system::error_code& err, size_t transferred)
{
    if (err != boost::asio::error::operation_aborted && IsSocketOpen())
    {
        fnCompleted(!err ? transferred : 0);
    }
    Release();
}

void CIOClient::HandleConnCompleted(CallBackConn fnCompleted, const boost::system::error_code& err)
{
    fnCompleted(IsSocketOpen() ? err : boost::asio::error::operation_aborted);

    if (!IsSocketOpen() && nRefCount)
    {
        Release();
    }
}

///////////////////////////////
// CSocketClient
CSocketClient::CSocketClient(CIOContainer* pContainerIn, boost::asio::io_service& ioservice)
  : CIOClient(pContainerIn), sockClient(ioservice)
{
}

CSocketClient::~CSocketClient()
{
    CloseSocket();
}

void CSocketClient::AsyncAccept(tcp::acceptor& acceptor, CallBackConn fnAccepted)
{
    acceptor.async_accept(sockClient, boost::bind(&CSocketClient::HandleConnCompleted, this,
                                                  fnAccepted, boost::asio::placeholders::error));
}

void CSocketClient::AsyncConnect(const tcp::endpoint& epRemote, CallBackConn fnConnected)
{
    sockClient.async_connect(epRemote, boost::bind(&CSocketClient::HandleConnCompleted, this,
                                                   fnConnected, boost::asio::placeholders::error));
}

void CSocketClient::AsyncRead(CBufStream& ssRecv, size_t nLength, CallBackFunc fnCompleted)
{
    boost::asio::async_read(sockClient,
                            (boost::asio::streambuf&)ssRecv,
                            boost::asio::transfer_exactly(nLength),
                            boost::bind(&CSocketClient::HandleCompleted, this, fnCompleted,
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));
}

void CSocketClient::AsyncReadUntil(CBufStream& ssRecv, const string& delim, CallBackFunc fnCompleted)
{
    boost::asio::async_read_until(sockClient,
                                  (boost::asio::streambuf&)ssRecv,
                                  delim,
                                  boost::bind(&CSocketClient::HandleCompleted, this, fnCompleted,
                                              boost::asio::placeholders::error,
                                              boost::asio::placeholders::bytes_transferred));
}

void CSocketClient::AsyncWrite(CBufStream& ssSend, CallBackFunc fnCompleted)
{
    boost::asio::async_write(sockClient,
                             (boost::asio::streambuf&)ssSend,
                             boost::asio::transfer_all(),
                             boost::bind(&CSocketClient::HandleCompleted, this, fnCompleted,
                                         boost::asio::placeholders::error,
                                         boost::asio::placeholders::bytes_transferred));
}

const tcp::endpoint CSocketClient::SocketGetRemote()
{
    return sockClient.remote_endpoint();
}

const tcp::endpoint CSocketClient::SocketGetLocal()
{
    return sockClient.local_endpoint();
}

void CSocketClient::CloseSocket()
{
    sockClient.close();
}

bool CSocketClient::IsSocketOpen()
{
    return sockClient.is_open();
}

///////////////////////////////
// CSSLClient
CSSLClient::CSSLClient(CIOContainer* pContainerIn, boost::asio::io_service& ioserivce,
                       boost::asio::ssl::context& context,
                       const string& strVerifyHost)
  : CIOClient(pContainerIn), sslClient(ioserivce, context)
{
    /*if (!strVerifyHost.empty())
    {
        //        sslClient.set_verify_callback(boost::bind(&CSSLClient::VerifyCertificate,this,
        //                                                   strVerifyHost,_1,_2));
        sslClient.set_verify_callback(boost::asio::ssl::rfc2818_verification(strVerifyHost));
    }*/
    sslClient.set_verify_callback(boost::bind(&CSSLClient::VerifyCertificate, this,
                                              strVerifyHost, _1, _2));
}

CSSLClient::~CSSLClient()
{
    CloseSocket();
}

void CSSLClient::AsyncAccept(tcp::acceptor& acceptor, CallBackConn fnAccepted)
{
    acceptor.async_accept(sslClient.lowest_layer(),
                          boost::bind(&CSSLClient::HandleConnected, this, fnAccepted,
                                      boost::asio::ssl::stream_base::server,
                                      boost::asio::placeholders::error));
}

void CSSLClient::AsyncConnect(const tcp::endpoint& epRemote, CallBackConn fnConnected)
{
    sslClient.lowest_layer().async_connect(epRemote,
                                           boost::bind(&CSSLClient::HandleConnected, this, fnConnected,
                                                       boost::asio::ssl::stream_base::client,
                                                       boost::asio::placeholders::error));
}

void CSSLClient::AsyncRead(CBufStream& ssRecv, size_t nLength, CallBackFunc fnCompleted)
{
    boost::asio::async_read(sslClient,
                            (boost::asio::streambuf&)ssRecv,
                            boost::asio::transfer_exactly(nLength),
                            boost::bind(&CSSLClient::HandleCompleted, this, fnCompleted,
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));
}

void CSSLClient::AsyncReadUntil(CBufStream& ssRecv, const string& delim, CallBackFunc fnCompleted)
{
    boost::asio::async_read_until(sslClient,
                                  (boost::asio::streambuf&)ssRecv,
                                  delim,
                                  boost::bind(&CSSLClient::HandleCompleted, this, fnCompleted,
                                              boost::asio::placeholders::error,
                                              boost::asio::placeholders::bytes_transferred));
}

void CSSLClient::AsyncWrite(CBufStream& ssSend, CallBackFunc fnCompleted)
{
    boost::asio::async_write(sslClient,
                             (boost::asio::streambuf&)ssSend,
                             boost::bind(&CSSLClient::HandleCompleted, this, fnCompleted,
                                         boost::asio::placeholders::error,
                                         boost::asio::placeholders::bytes_transferred));
}

const tcp::endpoint CSSLClient::SocketGetRemote()
{
    return sslClient.lowest_layer().remote_endpoint();
}

const tcp::endpoint CSSLClient::SocketGetLocal()
{
    return sslClient.lowest_layer().local_endpoint();
}

void CSSLClient::CloseSocket()
{
    sslClient.lowest_layer().close();
}

bool CSSLClient::IsSocketOpen()
{
    return sslClient.lowest_layer().is_open();
}

void CSSLClient::HandleConnected(CallBackConn fnHandshaked,
                                 boost::asio::ssl::stream_base::handshake_type type,
                                 const boost::system::error_code& err)
{
    if (!err)
    {
        sslClient.async_handshake(type, boost::bind(&CSSLClient::HandleConnCompleted, this, fnHandshaked,
                                                    boost::asio::placeholders::error));
    }
    else
    {
        HandleConnCompleted(fnHandshaked, err);
    }
}

bool CSSLClient::VerifyCertificate(const string& strVerifyHost, bool fPreverified,
                                   boost::asio::ssl::verify_context& ctx)
{
    char subject_name[256];
    X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
    X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
    //std::cout << "Verifying " << (fPreverified ? "success" : "fail") << ": " << subject_name << "\n";

    X509_STORE_CTX* cts = ctx.native_handle();

    //int32_t length = 0;
    //X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
    int cts_error;

#ifdef USE_SSL_110
    cts_error = X509_STORE_CTX_get_error(cts);
#else
    cts_error = cts->error;
#endif
    //std::cout << "CTX ERROR : " << cts_error << std::endl;

    int32_t depth = X509_STORE_CTX_get_error_depth(cts);
    //std::cout << "CTX DEPTH : " << depth << std::endl;

    if (fPreverified)
    {
        xengine::StdDebug("SSLVERIFY", (string("SSL verify success, subject: ") + subject_name).c_str());
    }
    else
    {
        string sErrorDesc;

        switch (cts_error)
        {
        case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
            sErrorDesc = "CA certificate issued for this certificate could not be found.";
            break;
        case X509_V_ERR_UNABLE_TO_GET_CRL:
            sErrorDesc = "The CRL associated with the certificate could not be found.";
            break;
        case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
            sErrorDesc = "Unable to unscramble the signature in the certificate.";
            break;
        case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:
            sErrorDesc = "Cannot unscramble CRLs signature.";
            break;
        case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
            sErrorDesc = "The public key information in the certificate could not be obtained.";
            break;
        case X509_V_ERR_CERT_SIGNATURE_FAILURE:
            sErrorDesc = "Certificate signature is invalid.";
            break;
        case X509_V_ERR_CRL_SIGNATURE_FAILURE:
            sErrorDesc = "Certificate-related CRL signatures are invalid.";
            break;
        case X509_V_ERR_CERT_NOT_YET_VALID:
            sErrorDesc = "Certificates are not valid yet.";
            break;
        case X509_V_ERR_CRL_NOT_YET_VALID:
            sErrorDesc = "Certificate-related CRLs do not yet have a valid start time.";
            break;
        case X509_V_ERR_CERT_HAS_EXPIRED:
            sErrorDesc = "Certificate has expired.";
            break;
        case X509_V_ERR_CRL_HAS_EXPIRED:
            sErrorDesc = "Certificate-related CRL expiration.";
            break;
        case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
            sErrorDesc = "The notBeforefield of the certificate is not in the correct format, that is, the time is in the illegal format.";
            break;
        case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
            sErrorDesc = "The notAfter field of the certificate is not in the correct format, that is, the time is in the illegal format.";
            break;
        case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
            sErrorDesc = "The first certificate to be validated is the word signature certificate, which is not in the trust CA certificate list.";
            break;
        case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
            sErrorDesc = "Certificate chains can be established, but their roots cannot be found locally.";
            break;
        case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
            sErrorDesc = "One certificate issued by CA could not be found.";
            break;
        case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
            sErrorDesc = "The certificate chain has only one item, but it is not a signed certificate.";
            break;
        case X509_V_ERR_CERT_CHAIN_TOO_LONG:
            sErrorDesc = "Certificate chain is too long.";
            break;
        case X509_V_ERR_CERT_REVOKED:
            sErrorDesc = "The certificate has been declared withdrawn by CA.";
            break;
        case X509_V_ERR_INVALID_CA:
            sErrorDesc = "Certificate of a CA is invalid.";
            break;
        case X509_V_ERR_CERT_UNTRUSTED:
            sErrorDesc = "Root CA certificates are not trusted if they are used for the purpose of requests.";
            break;
        case X509_V_ERR_CERT_REJECTED:
            sErrorDesc = "CA certificates can not be used for requests at all.";
            break;
        case X509_V_ERR_SUBJECT_ISSUER_MISMATCH:
            sErrorDesc = "The certificate issuer name is different from its CA owner name.";
            break;
        case X509_V_ERR_AKID_SKID_MISMATCH:
            sErrorDesc = "The key flag of a certificate is different from the key flag designated for it by the CA issued by the certificate.";
            break;
        case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH:
            sErrorDesc = "Certificate serial number is different from the serial number designated by CA.";
            break;
        case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
            sErrorDesc = "Certificate purposes of a CA do not include signing other certificates.";
            break;
        default:
            sErrorDesc = "Other error.";
            break;
        }

        xengine::StdDebug("SSLVERIFY", (string("SSL verify fail, subject: ") + subject_name + string(", cts_error: [") + to_string(cts_error) + string("]<") + to_string(depth) + string(">  ") + string(sErrorDesc)).c_str());
    }

    return fPreverified;
}

} // namespace xengine
