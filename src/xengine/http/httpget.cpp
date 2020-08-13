// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "httpget.h"

#include <boost/algorithm/string/trim.hpp>

#include "httputil.h"
#include "netio/netio.h"

#define HTTPGET_CONNET_TIMEOUT 10

using namespace std;
#if BOOST_VERSION >= 106000
using namespace boost::placeholders;
#endif
using boost::asio::ip::tcp;

namespace xengine
{

///////////////////////////////
// CHttpGetClient

CHttpGetClient::CHttpGetClient(const string& strIOModuleIn, const uint64 nNonceIn,
                               CHttpGet* pHttpGetIn, CIOClient* pClientIn)
  : strIOModule(strIOModuleIn), nNonce(nNonceIn), pHttpGet(pHttpGetIn), pClient(pClientIn)
{
    nTimerId = 0;
    fIdle = true;
}

CHttpGetClient::~CHttpGetClient()
{
    if (pClient)
    {
        pClient->Close();
    }
}

const string& CHttpGetClient::GetIOModule()
{
    return strIOModule;
}

uint64 CHttpGetClient::GetNonce()
{
    return nNonce;
}

uint32 CHttpGetClient::GetTimerId()
{
    return nTimerId;
}

CNetHost CHttpGetClient::GetHost()
{
    return CNetHost(pClient->GetRemote());
}

bool CHttpGetClient::IsIdle()
{
    return fIdle;
}

void CHttpGetClient::GetResponse(CHttpRsp& rsp)
{
    rsp.nStatusCode = atoi(mapHeader["status"].c_str());
    if (strChunked.empty())
    {
        ssRecv >> rsp;
    }
    else
    {
        rsp.strContent = strChunked;
    }
    rsp.mapHeader = mapHeader;
    rsp.mapCookie = mapCookie;
    fIdle = true;
}

void CHttpGetClient::Activate(CHttpReqData& httpReqData, uint32 nTimerIdIn)
{
    nTimerId = nTimerIdIn;
    fIdle = false;
    mapHeader.clear();
    mapCookie.clear();
    strChunked.clear();

    ssSend.Clear();
    ssRecv.Clear();

    string strHeader = CHttpUtil().BuildRequestHeader(httpReqData.mapHeader, httpReqData.mapQuery,
                                                      httpReqData.mapCookie, httpReqData.strContent.size());
    ssSend << CBinary(&strHeader[0], strHeader.size()) << httpReqData;
    pClient->Write(ssSend, boost::bind(&CHttpGetClient::HandleWritenRequest, this, _1));
}

void CHttpGetClient::HandleWritenRequest(size_t nTransferred)
{
    if (nTransferred != 0)
    {
        pClient->ReadUntil(ssRecv, "\r\n\r\n",
                           boost::bind(&CHttpGetClient::HandleReadHeader, this, _1));
    }
    else
    {
        pHttpGet->HandleClientError(this);
    }
}

void CHttpGetClient::HandleReadHeader(size_t nTransferred)
{

    istream is(&ssRecv);
    if (nTransferred != 0
        && CHttpUtil().ParseResponseHeader(is, mapHeader, mapCookie))
    {
        size_t nLength = 0;
        MAPIKeyValue::iterator it = mapHeader.find("content-length");
        if (it != mapHeader.end())
        {
            nLength = atoi((*it).second.c_str());
        }
        if (nLength > 0 && ssRecv.GetSize() < nLength)
        {
            pClient->Read(ssRecv, nLength - ssRecv.GetSize(),
                          boost::bind(&CHttpGetClient::HandleReadPayload, this, _1));
        }
        else
        {
            MAPIKeyValue::iterator mi = mapHeader.find("transfer-encoding");
            if (mi != mapHeader.end() && (*mi).second == "chunked")
            {
                if (ssRecv.GetSize() != 0)
                {
                    HandleReadChunked(ssRecv.GetSize());
                }
                else
                {
                    pClient->ReadUntil(ssRecv, "0\r\n\r\n",
                                       boost::bind(&CHttpGetClient::HandleReadChunked, this, _1));
                }
            }
            else
            {
                HandleReadCompleted();
            }
        }
    }
    else
    {
        pHttpGet->HandleClientError(this);
    }
}

void CHttpGetClient::HandleReadPayload(size_t nTransferred)
{
    if (nTransferred != 0)
    {
        HandleReadCompleted();
    }
    else
    {
        pHttpGet->HandleClientError(this);
    }
}

void CHttpGetClient::HandleReadChunked(std::size_t nTransferred)
{
    if (nTransferred != 0)
    {
        istream is(&ssRecv);
        string strResidue;
        bool fContinue;

        if (CHttpUtil().ParseChunked(is, strChunked, strResidue, fContinue))
        {
            is.clear();

            if (fContinue)
            {
                ssRecv << CBinary(&strResidue[0], strResidue.size());
                pClient->ReadUntil(ssRecv, "0\r\n\r\n",
                                   boost::bind(&CHttpGetClient::HandleReadChunked, this, _1));
            }
            else
            {
                HandleReadCompleted();
            }
        }
        else
        {
            pHttpGet->HandleClientError(this);
        }
    }
    else
    {
        pHttpGet->HandleClientError(this);
    }
}

void CHttpGetClient::HandleReadCompleted()
{
    pHttpGet->HandleClientCompleted(this);
}

///////////////////////////////
// CHttpGet

CHttpGet::CHttpGet()
  : CIOProc("httpget")
{
}

CHttpGet::~CHttpGet()
{
}

void CHttpGet::HandleClientCompleted(CHttpGetClient* pGetClient)
{
    CEventHttpGetRsp* pEventGetRsp = new CEventHttpGetRsp(pGetClient->GetNonce());
    if (pEventGetRsp != nullptr)
    {
        CHttpRsp& rsp = pEventGetRsp->data;
        pGetClient->GetResponse(rsp);
        CancelTimer(pGetClient->GetTimerId());
        if (!PostResponse(pGetClient->GetIOModule(), pEventGetRsp))
        {
            CloseConn(pGetClient);
            delete pEventGetRsp;
        }
        else if (strcasecmp(rsp.mapHeader["connection"].c_str(), "Close") == 0)
        {
            CloseConn(pGetClient);
        }
    }
    else
    {
        CloseConn(pGetClient, HTTPGET_INTERNAL_FAILURE);
    }
}

void CHttpGet::HandleClientError(CHttpGetClient* pGetClient)
{
    CloseConn(pGetClient, HTTPGET_INTERRUPTED);
}

void CHttpGet::LeaveLoop()
{
    for (multimap<CNetHost, CEventHttpGet>::iterator it = mapRequest.begin();
         it != mapRequest.end(); ++it)
    {
        PostError((*it).second, HTTPGET_ABORTED);
    }
    mapRequest.clear();

    vector<CHttpGetClient*> vClient;
    for (multimap<uint64, CHttpGetClient*>::iterator it = mapGetClient.begin();
         it != mapGetClient.end(); ++it)
    {
        vClient.push_back((*it).second);
    }
    for (CHttpGetClient* pGetClient : vClient)
    {
        CloseConn(pGetClient, HTTPGET_ABORTED);
    }
}

void CHttpGet::HostResolved(const CNetHost& host, const tcp::endpoint& ep)
{
    multimap<CNetHost, CEventHttpGet>::iterator it = mapRequest.lower_bound(host);
    if (it != mapRequest.upper_bound(host))
    {
        CHttpReqData& httpReqData = (*it).second.data;
        if (SSLConnect(ep, HTTPGET_CONNET_TIMEOUT, GetSSLOption(httpReqData, host.strHost)))
        {
            mapRequest.insert(make_pair(CNetHost(ep), (*it).second));
        }
        else
        {
            PostError((*it).second, HTTPGET_CONNECT_FAILED);
        }
        mapRequest.erase(it);
    }
}

void CHttpGet::HostFailToResolve(const CNetHost& host)
{
    multimap<CNetHost, CEventHttpGet>::iterator it, end;
    it = mapRequest.lower_bound(host);
    end = mapRequest.upper_bound(host);
    while (it != end)
    {
        PostError((*it).second, HTTPGET_RESOLVE_FAILED);
        mapRequest.erase(it++);
    }
}

bool CHttpGet::ClientConnected(CIOClient* pClient)
{
    tcp::endpoint ep = pClient->GetRemote();
    multimap<CNetHost, CEventHttpGet>::iterator it = mapRequest.lower_bound(CNetHost(ep));
    if (it == mapRequest.upper_bound(CNetHost(ep)))
    {
        return false;
    }

    int nErrCode = ActivateConn(pClient, (*it).second);
    if (nErrCode != HTTPGET_OK)
    {
        PostError((*it).second, nErrCode);
    }

    mapRequest.erase(it);
    return (nErrCode == HTTPGET_OK);
}

void CHttpGet::ClientFailToConnect(const tcp::endpoint& epRemote)
{
    multimap<CNetHost, CEventHttpGet>::iterator it = mapRequest.lower_bound(CNetHost(epRemote));
    if (it != mapRequest.upper_bound(CNetHost(epRemote)))
    {
        PostError((*it).second, HTTPGET_CONNECT_FAILED);
        mapRequest.erase(it);
    }
}

int CHttpGet::ActivateConn(CIOClient* pClient, CEventHttpGet& eventGet)
{
    uint64 nNonce = eventGet.nNonce;
    CHttpReqData& httpReqData = eventGet.data;

    CHttpGetClient* pGetClient = new CHttpGetClient(httpReqData.strIOModule, nNonce, this, pClient);
    if (pGetClient == nullptr)
    {
        return HTTPGET_ACTIVATE_FAILED;
    }

    uint32 nTimerId = httpReqData.nTimeout > 0 ? SetTimer(nNonce, httpReqData.nTimeout, "CHttpGet ActivateConn") : 0;
    mapGetClient.insert(make_pair(nNonce, pGetClient));

    pGetClient->Activate(httpReqData, nTimerId);

    return HTTPGET_OK;
}

void CHttpGet::Timeout(uint64 nNonce, uint32 nTimerId, const std::string& strFunctionIn)
{
    for (multimap<uint64, CHttpGetClient*>::iterator it = mapGetClient.lower_bound(nNonce);
         it != mapGetClient.upper_bound(nNonce); ++it)
    {
        if (nTimerId == (*it).second->GetTimerId())
        {
            CloseConn((*it).second, HTTPGET_RESP_TIMEOUT);
            break;
        }
    }
}

bool CHttpGet::PostResponse(const string& strIOModule, CEventHttpGetRsp* pEventResp)
{
    IIOModule* pIOModule;
    if (!GetObject(strIOModule, pIOModule))
    {
        return false;
    }
    pIOModule->PostEvent(pEventResp);
    return true;
}

void CHttpGet::PostError(const string& strIOModule, uint64 nNonce, int nErrCode)
{
    CEventHttpGetRsp* pEventResp = new CEventHttpGetRsp(nNonce);
    if (pEventResp != nullptr)
    {
        pEventResp->data.nStatusCode = nErrCode;
        if (!PostResponse(strIOModule, pEventResp))
        {
            delete pEventResp;
        }
    }
}

void CHttpGet::PostError(const CEventHttpGet& eventGet, int nErrCode)
{
    PostError(eventGet.data.strIOModule, eventGet.nNonce, nErrCode);
}

bool CHttpGet::HandleEvent(CEventHttpGet& eventGet)
{
    CHttpReqData& httpReqData = eventGet.data;

    uint64 nNonce = eventGet.nNonce;
    for (multimap<uint64, CHttpGetClient*>::iterator it = mapGetClient.lower_bound(nNonce);
         it != mapGetClient.upper_bound(nNonce); ++it)
    {
        CHttpGetClient* pGetClient = (*it).second;
        if (httpReqData.strIOModule == pGetClient->GetIOModule())
        {
            if (pGetClient->IsIdle())
            {
                uint32 nTimerId = httpReqData.nTimeout > 0 ? SetTimer(nNonce, httpReqData.nTimeout, "CHttpGet eventGet") : 0;
                pGetClient->Activate(httpReqData, nTimerId);
            }
            else
            {
                PostError(httpReqData.strIOModule, nNonce, HTTPGET_INVALID_NONCE);
            }
            return true;
        }
    }

    CNetHost host(httpReqData.mapHeader["host"], httpReqData.strProtocol == "https" ? 443 : 80);
    tcp::endpoint ep = host.ToEndPoint();
    if (ep != tcp::endpoint())
    {
        if (!SSLConnect(ep, HTTPGET_CONNET_TIMEOUT, GetSSLOption(httpReqData, host.strHost)))
        {
            PostError(httpReqData.strIOModule, nNonce, HTTPGET_CONNECT_FAILED);
            return true;
        }
    }
    else
    {
        ResolveHost(host);
    }

    mapRequest.insert(make_pair(host, eventGet));

    return true;
}

bool CHttpGet::HandleEvent(CEventHttpAbort& eventAbort)
{
    set<uint64> setAbort;
    const string& strIOModule = eventAbort.data.strIOModule;

    for (const uint64 nNonce : eventAbort.data.vNonce)
    {
        for (multimap<uint64, CHttpGetClient*>::iterator it = mapGetClient.lower_bound(nNonce);
             it != mapGetClient.upper_bound(nNonce); ++it)
        {
            CHttpGetClient* pGetClient = (*it).second;
            if (strIOModule == pGetClient->GetIOModule())
            {
                CloseConn(pGetClient, HTTPGET_ABORTED);
                break;
            }
        }
        setAbort.insert(nNonce);
    }

    for (multimap<CNetHost, CEventHttpGet>::iterator it = mapRequest.begin();
         it != mapRequest.end();)
    {
        if (strIOModule == (*it).second.data.strIOModule
            && setAbort.count((*it).second.nNonce))
        {
            PostError((*it).second, HTTPGET_ABORTED);
            mapRequest.erase(it++);
        }
        else
        {
            ++it;
        }
    }
    return true;
}

void CHttpGet::CloseConn(CHttpGetClient* pGetClient, int nErrCode)
{
    CancelTimer(pGetClient->GetTimerId());

    uint64 nNonce = pGetClient->GetNonce();
    if (nErrCode != HTTPGET_OK && !pGetClient->IsIdle())
    {
        PostError(pGetClient->GetIOModule(), nNonce, nErrCode);
    }

    for (multimap<uint64, CHttpGetClient*>::iterator it = mapGetClient.lower_bound(nNonce);
         it != mapGetClient.upper_bound(nNonce); ++it)
    {
        if (pGetClient == (*it).second)
        {
            mapGetClient.erase(it);
            break;
        }
    }
    delete pGetClient;
}

CIOSSLOption CHttpGet::GetSSLOption(CHttpReqData& httpReqData, const string& strHost)
{
    return CIOSSLOption(httpReqData.strProtocol == "https", httpReqData.fVerifyPeer,
                        httpReqData.strPathCA, httpReqData.strPathCert, httpReqData.strPathPK, "", strHost);
}

} // namespace xengine
