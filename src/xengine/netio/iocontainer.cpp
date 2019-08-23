// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "iocontainer.h"

#include <boost/bind.hpp>

#include "ioproc.h"

using namespace std;
using boost::asio::ip::tcp;

namespace xengine
{

///////////////////////////////
// CIOContainer

CIOContainer::CIOContainer(CIOProc* pIOProcIn)
  : pIOProc(pIOProcIn)
{
}

CIOContainer::~CIOContainer()
{
}

size_t CIOContainer::GetIdleCount()
{
    return 0;
}

const tcp::endpoint CIOContainer::GetServiceEndpoint()
{
    return (tcp::endpoint());
}

///////////////////////////////
// CIOCachedContainer
CIOCachedContainer::CIOCachedContainer(CIOProc* pIOProcIn)
  : CIOContainer(pIOProcIn)
{
}

CIOCachedContainer::~CIOCachedContainer()
{
    Deactivate();
    FreeClient();
}

void CIOCachedContainer::ClientClose(CIOClient* pClient)
{
    if (setInuseClient.count(pClient))
    {
        setInuseClient.erase(pClient);
        queIdleClient.push(pClient);
    }
}

size_t CIOCachedContainer::GetIdleCount()
{
    return queIdleClient.size();
}

CIOClient* CIOCachedContainer::ClientAlloc()
{
    CIOClient* pClient = nullptr;
    if (!queIdleClient.empty())
    {
        pClient = queIdleClient.front();
        queIdleClient.pop();
        setInuseClient.insert(pClient);
    }
    return pClient;
}

void CIOCachedContainer::Deactivate()
{
    vector<CIOClient*> vecClient(setInuseClient.begin(), setInuseClient.end());
    for (CIOClient* pClient : vecClient)
    {
        pClient->Close();
    }
}

bool CIOCachedContainer::PrepareClient(size_t nPrepare)
{
    while (queIdleClient.size() < nPrepare)
    {
        CIOClient* pClient = pIOProc->CreateIOClient(this);
        if (pClient != nullptr)
        {
            queIdleClient.push(pClient);
        }
        else
        {
            FreeClient();
            return false;
        }
    }

    FreeClient(nPrepare);

    return true;
}

void CIOCachedContainer::FreeClient(size_t nReserve)
{
    while (queIdleClient.size() > nReserve)
    {
        CIOClient* pClient = queIdleClient.front();
        queIdleClient.pop();
        delete pClient;
    }
}

///////////////////////////////
// CIOInBound
CIOInBound::CIOInBound(CIOProc* pIOProcIn)
  : CIOCachedContainer(pIOProcIn), acceptorService(pIOProcIn->GetIoService())
{
}

CIOInBound::~CIOInBound()
{
    acceptorService.close();
}

bool CIOInBound::Invoke(const tcp::endpoint& epListen, size_t nMaxConnection, const vector<string>& vAllowMask)
{
    Halt();
    try
    {
        BuildWhiteList(vAllowMask);
        epService = epListen;

        if (!PrepareClient(nMaxConnection + 2))
        {
            return false;
        }

        acceptorService.open(epListen.protocol());
        acceptorService.set_option(tcp::acceptor::reuse_address(true));
        if (epListen.address().is_v6())
        {
            acceptorService.set_option(boost::asio::ip::v6_only(true));
        }

        //acceptorService.bind(epListen);
        boost::system::error_code ec;
        acceptorService.bind(epListen, ec);
        if (ec)
        {
            throw runtime_error((string("IOInBound tcp bind fail, addr: ") + epListen.address().to_string() + string(":") + to_string(epListen.port()) + string(", cause: ") + ec.message()).c_str());
        }

        acceptorService.listen();

        CIOClient* pClient = ClientAlloc();
        pClient->Accept(acceptorService,
                        boost::bind(&CIOInBound::HandleAccept, this, pClient, _1));
        return true;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
    }

    acceptorService.close();

    return false;
}

void CIOInBound::Halt()
{
    acceptorService.close();

    Deactivate();
}

const tcp::endpoint CIOInBound::GetServiceEndpoint()
{
    return epService;
}

bool CIOInBound::BuildWhiteList(const vector<string>& vAllowMask)
{
    const boost::regex expr("(\\*)|(\\?)|(\\.)");
    const string fmt = "(?1.*)(?2.)(?3\\\\.)";
    vWhiteList.clear();
    try
    {
        for (const string& mask : vAllowMask)
        {
            string strRegex = boost::regex_replace(mask, expr, fmt,
                                                   boost::match_default | boost::format_all);
            vWhiteList.push_back(boost::regex(strRegex));
        }
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

bool CIOInBound::IsAllowedRemote(const tcp::endpoint& ep)
{
    string strAddress = ep.address().to_string();
    try
    {
        for (const boost::regex& expr : vWhiteList)
        {
            if (boost::regex_match(strAddress, expr))
            {
                return true;
            }
        }
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
    }
    return (vWhiteList.empty());
}

void CIOInBound::HandleAccept(CIOClient* pClient, const boost::system::error_code& err)
{
    if (!err)
    {
        if (queIdleClient.size() <= 1 || !IsAllowedRemote(pClient->GetRemote())
            || !pIOProc->ClientAccepted(acceptorService.local_endpoint(), pClient))
        {
            StdError(__PRETTY_FUNCTION__, (string("Accept error ") + epService.address().to_string()
                                           + ". Idle client size: " + to_string(queIdleClient.size())
                                           + ". Is allowed: " + to_string(IsAllowedRemote(pClient->GetRemote()))
                                           + ". Accepted: " + to_string(pIOProc->ClientAccepted(acceptorService.local_endpoint(), pClient)))
                                              .c_str());
            pClient->Close();
        }

        pClient = ClientAlloc();
        pClient->Accept(acceptorService,
                        boost::bind(&CIOInBound::HandleAccept, this, pClient, _1));
    }
    else if (boost::asio::error::get_ssl_category() == err.category())
    {
        pClient->Close();
        pClient = ClientAlloc();
        pClient->Accept(acceptorService,
                        boost::bind(&CIOInBound::HandleAccept, this, pClient, _1));
    }
    else
    {
        StdError(__PRETTY_FUNCTION__, (string("Other error ") + epService.address().to_string() + ". " + err.message()).c_str());
        if (err != boost::asio::error::operation_aborted)
        {
            acceptorService.close();
        }
        pClient->Close();
    }
}

///////////////////////////////
// CIOOutBound
CIOOutBound::CIOOutBound(CIOProc* pIOProcIn)
  : CIOCachedContainer(pIOProcIn)
{
}

CIOOutBound::~CIOOutBound()
{
}

bool CIOOutBound::Invoke(std::size_t nMaxConnection)
{
    return PrepareClient(nMaxConnection);
}

void CIOOutBound::Halt()
{
    Deactivate();
    mapPending.clear();
}

bool CIOOutBound::ConnectTo(const tcp::endpoint& epRemote, int64 nTimeout)
{
    CIOClient* pClient = ClientAlloc();
    if (pClient == nullptr)
    {
        return false;
    }

    uint32 nTimerId = pIOProc->SetTimer(0, nTimeout);
    pClient->Connect(epRemote, boost::bind(&CIOOutBound::HandleConnect, this,
                                           pClient, epRemote, nTimerId, _1));

    mapPending.insert(make_pair(nTimerId, pClient));
    return true;
}

void CIOOutBound::Timeout(uint32 nTimerId)
{
    map<uint32, CIOClient*>::iterator it = mapPending.find(nTimerId);
    if (it != mapPending.end())
    {
        (*it).second->Shutdown();
        mapPending.erase(it);
    }
}

void CIOOutBound::HandleConnect(CIOClient* pClient, const tcp::endpoint epRemote,
                                uint32 nTimerId, const boost::system::error_code& err)
{
    if (!err)
    {
        pIOProc->CancelTimer(nTimerId);
        mapPending.erase(nTimerId);
        if (!pIOProc->ClientConnected(pClient))
        {
            pClient->Close();
        }
    }
    else
    {
        if (err != boost::asio::error::operation_aborted)
        {
            pIOProc->CancelTimer(nTimerId);
            mapPending.erase(nTimerId);
            pClient->Close();
        }
        pIOProc->ClientFailToConnect(epRemote);
    }
}

///////////////////////////////
// CIOSSLOption
bool CIOSSLOption::SetupSSLContext(boost::asio::ssl::context& ctx) const
{
    try
    {
        ctx.set_options(boost::asio::ssl::context::no_sslv2);
        if (fVerifyPeer)
        {
            ctx.set_verify_mode(boost::asio::ssl::verify_peer
                                | boost::asio::ssl::verify_fail_if_no_peer_cert);
            if (strPathCA.empty())
            {
                ctx.set_default_verify_paths();
            }
            else
            {
                ctx.load_verify_file(strPathCA);
            }
        }
        else
        {
            ctx.set_verify_mode(boost::asio::ssl::verify_none);
        }
        if (!strPathCert.empty() && !strPathPK.empty())
        {
            ctx.use_certificate_chain_file(strPathCert);
            ctx.use_private_key_file(strPathPK, boost::asio::ssl::context::pem);
        }
        if (!strCiphers.empty())
        {
            SSL_CTX_set_cipher_list(ctx.native_handle(), strCiphers.c_str());
        }
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

///////////////////////////////
// CIOSSLOutBound

CIOSSLOutBound::CIOSSLOutBound(CIOProc* pIOProcIn)
  : CIOContainer(pIOProcIn)
{
    nMaxConn = 0;
}

CIOSSLOutBound::~CIOSSLOutBound()
{
    Halt();
}

size_t CIOSSLOutBound::GetIdleCount()
{
    return (nMaxConn - setInuseClient.size());
}

CIOClient* CIOSSLOutBound::ClientAlloc(const CIOSSLOption& optSSL)
{
    boost::asio::io_service& ioService = pIOProc->GetIoService();
    if (optSSL.fEnable)
    {
        try
        {
            boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
            ctx.set_options(boost::asio::ssl::context::no_sslv2);
            if (optSSL.fVerifyPeer)
            {
                ctx.set_verify_mode(boost::asio::ssl::verify_peer
                                    | boost::asio::ssl::verify_fail_if_no_peer_cert);
                if (optSSL.strPathCA.empty())
                {
                    ctx.set_default_verify_paths();
                }
                else
                {
                    ctx.load_verify_file(optSSL.strPathCA);
                }
            }
            else
            {
                ctx.set_verify_mode(boost::asio::ssl::verify_none);
            }
            if (!optSSL.strPathCert.empty() && !optSSL.strPathPK.empty())
            {
                ctx.use_certificate_chain_file(optSSL.strPathCert);
                ctx.use_private_key_file(optSSL.strPathPK, boost::asio::ssl::context::pem);
            }

            return new CSSLClient(this, ioService, ctx, optSSL.fVerifyPeer ? optSSL.strPeerName : "");
        }
        catch (exception& e)
        {
            StdError(__PRETTY_FUNCTION__, e.what());
            return nullptr;
        }
    }
    return new CSocketClient(this, ioService);
}

void CIOSSLOutBound::ClientClose(CIOClient* pClient)
{
    set<CIOClient*>::iterator mi = setInuseClient.find(pClient);
    if (mi != setInuseClient.end())
    {
        setInuseClient.erase(mi);
        delete pClient;
    }
}

void CIOSSLOutBound::Invoke(size_t nMaxConnIn)
{
    nMaxConn = nMaxConnIn;
}

void CIOSSLOutBound::Halt()
{
    for (map<uint32, CIOClient*>::iterator it = mapPending.begin();
         it != mapPending.end(); ++it)
    {
        pIOProc->CancelTimer((*it).first);
    }
    mapPending.clear();

    vector<CIOClient*> vecClient(setInuseClient.begin(), setInuseClient.end());
    for (CIOClient* pClient : vecClient)
    {
        pClient->Close();
    }
}

bool CIOSSLOutBound::ConnectTo(const tcp::endpoint& epRemote, int64 nTimeout, const CIOSSLOption& optSSL)
{
    if (setInuseClient.size() >= nMaxConn)
    {
        return false;
    }

    CIOClient* pClient = ClientAlloc(optSSL);
    if (pClient == nullptr)
    {
        return false;
    }

    uint32 nTimerId = pIOProc->SetTimer(0, nTimeout);
    pClient->Connect(epRemote, boost::bind(&CIOSSLOutBound::HandleConnect, this,
                                           pClient, epRemote, nTimerId, _1));
    setInuseClient.insert(pClient);
    mapPending.insert(make_pair(nTimerId, pClient));
    return true;
}

void CIOSSLOutBound::Timeout(uint32 nTimerId)
{
    map<uint32, CIOClient*>::iterator it = mapPending.find(nTimerId);
    if (it != mapPending.end())
    {
        (*it).second->Shutdown();
        mapPending.erase(it);
    }
}

void CIOSSLOutBound::HandleConnect(CIOClient* pClient, const tcp::endpoint epRemote,
                                   uint32 nTimerId, const boost::system::error_code& err)
{

    pIOProc->CancelTimer(nTimerId);
    mapPending.erase(nTimerId);
    if (!err)
    {
        if (!pIOProc->ClientConnected(pClient))
        {
            pClient->Shutdown();
        }
    }
    else
    {
        if (err != boost::asio::error::operation_aborted)
        {
            pClient->Shutdown();
        }
        pIOProc->ClientFailToConnect(epRemote);
    }
}

} // namespace xengine
