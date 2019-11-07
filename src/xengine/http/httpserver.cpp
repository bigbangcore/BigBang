// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "httpserver.h"

#include <boost/algorithm/string/trim.hpp>
#include <openssl/rand.h>

using namespace std;
using boost::asio::ip::tcp;

namespace xengine
{

///////////////////////////////
// CHttpClient

CHttpClient::CHttpClient(CHttpServer* pServerIn, CHttpProfile* pProfileIn,
                         CIOClient* pClientIn, uint64 nNonceIn)
  : pServer(pServerIn), pProfile(pProfileIn), pClient(pClientIn),
    nNonce(nNonceIn), fKeepAlive(false), fEventStream(false)
{
}

CHttpClient::~CHttpClient()
{
    if (pClient)
    {
        pClient->Close();
    }
}

CHttpProfile* CHttpClient::GetProfile()
{
    return pProfile;
}

uint64 CHttpClient::GetNonce()
{
    return nNonce;
}

bool CHttpClient::IsKeepAlive()
{
    return fKeepAlive;
}

bool CHttpClient::IsEventStream()
{
    return fEventStream;
}

void CHttpClient::KeepAlive()
{
    fKeepAlive = true;
}

void CHttpClient::SetEventStream()
{
    fEventStream = true;
}

void CHttpClient::Activate()
{
    fKeepAlive = false;
    fEventStream = false;
    ssRecv.Clear();
    ssSend.Clear();
    mapHeader.clear();
    mapQuery.clear();
    mapCookie.clear();

    StartReadHeader();
}

void CHttpClient::SendResponse(string& strResponse)
{
    CBinary binary(&strResponse[0], strResponse.size());
    ssSend.Clear();
    ssSend << binary;
    pClient->Write(ssSend, boost::bind(&CHttpClient::HandleWritenResponse, this, _1));
}

void CHttpClient::StartReadHeader()
{
    pClient->ReadUntil(ssRecv, "\r\n\r\n",
                       boost::bind(&CHttpClient::HandleReadHeader, this, _1));
}

void CHttpClient::StartReadPayload(size_t nLength)
{
    pClient->Read(ssRecv, nLength,
                  boost::bind(&CHttpClient::HandleReadPayload, this, _1));
}

void CHttpClient::HandleReadHeader(size_t nTransferred)
{
    istream is(&ssRecv);
    if (nTransferred != 0 && CHttpUtil().ParseRequestHeader(is, mapHeader, mapQuery, mapCookie))
    {
        size_t nLength = 0;
        MAPIKeyValue::iterator it = mapHeader.find("content-length");
        if (it != mapHeader.end())
        {
            nLength = atoi((*it).second.c_str());
        }
        if (nLength > 0 && ssRecv.GetSize() < nLength)
        {
            StartReadPayload(nLength - ssRecv.GetSize());
        }
        else
        {
            HandleReadCompleted();
        }
    }
    else
    {
        pServer->HandleClientError(this);
    }
}

void CHttpClient::HandleReadPayload(size_t nTransferred)
{
    if (nTransferred != 0)
    {
        HandleReadCompleted();
    }
    else
    {
        pServer->HandleClientError(this);
    }
}

void CHttpClient::HandleReadCompleted()
{
    pServer->HandleClientRecv(this, mapHeader, mapQuery, mapCookie, ssRecv);
}

void CHttpClient::HandleWritenResponse(std::size_t nTransferred)
{
    if (nTransferred != 0)
    {
        pServer->HandleClientSent(this);
    }
    else
    {
        pServer->HandleClientError(this);
    }
}

///////////////////////////////
// CHttpServer

CHttpServer::CHttpServer()
  : CIOProc("httpserver")
{
}

CHttpServer::~CHttpServer()
{
}

void CHttpServer::AddNewHost(const CHttpHostConfig& confHost)
{
    vecHostConfig.push_back(confHost);
}

bool CHttpServer::CreateProfile(const CHttpHostConfig& confHost)
{
    CHttpUtil util;
    CHttpProfile profile;

    if (!GetObject(confHost.strIOModule, profile.pIOModule))
    {
        Error("Failed to request %s", confHost.strIOModule.c_str());
        return false;
    }

    for (map<string, string>::const_iterator it = confHost.mapUserPass.begin();
         it != confHost.mapUserPass.end(); ++it)
    {
        string strAuthBase64;
        util.Base64Encode((*it).first + ":" + (*it).second, strAuthBase64);
        profile.mapAuthrizeUser[string("Basic ") + strAuthBase64] = (*it).first;
    }

    if (confHost.optSSL.fEnable)
    {
        profile.pSSLContext = new boost::asio::ssl::context(boost::asio::ssl::context::sslv23);
        if (profile.pSSLContext == nullptr)
        {
            Error("Failed to alloc ssl context for %s:%u", confHost.epHost.address().to_string().c_str(),
                  confHost.epHost.port());
            return false;
        }
        if (!confHost.optSSL.SetupSSLContext(*profile.pSSLContext))
        {
            Error("Failed to setup ssl context for %s:%u", confHost.epHost.address().to_string().c_str(),
                  confHost.epHost.port());
            delete profile.pSSLContext;
            return false;
        }
    }

    profile.nMaxConnections = confHost.nMaxConnections;
    profile.vAllowMask = confHost.vAllowMask;

    mapProfile[confHost.epHost] = profile;

    return true;
}

bool CHttpServer::HandleInitialize()
{
    for (const CHttpHostConfig& confHost : vecHostConfig)
    {
        if (!CreateProfile(confHost))
        {
            return false;
        }
    }
    return true;
}

void CHttpServer::HandleDeinitialize()
{
    for (map<tcp::endpoint, CHttpProfile>::iterator it = mapProfile.begin();
         it != mapProfile.end(); ++it)
    {
        delete (*it).second.pSSLContext;
    }
    mapProfile.clear();
}

void CHttpServer::EnterLoop()
{
    Log("Http Server start:");
    for (map<tcp::endpoint, CHttpProfile>::iterator it = mapProfile.begin();
         it != mapProfile.end(); ++it)
    {
        if (!StartService((*it).first, (*it).second.nMaxConnections, (*it).second.vAllowMask))
        {
            Error("Setup service %s failed, listen port = %d, connection limit %d",
                  (*it).second.pIOModule->GetOwnKey().c_str(),
                  (*it).first.port(), (*it).second.nMaxConnections);
        }
        else
        {
            Log("Setup service %s sucess, listen port = %d, connection limit %d",
                (*it).second.pIOModule->GetOwnKey().c_str(),
                (*it).first.port(), (*it).second.nMaxConnections);
        }
    }
}

void CHttpServer::LeaveLoop()
{
    vector<CHttpClient*> vClient;
    for (map<uint64, CHttpClient*>::iterator it = mapClient.begin(); it != mapClient.end(); ++it)
    {
        vClient.push_back((*it).second);
    }
    for (CHttpClient* pClient : vClient)
    {
        RemoveClient(pClient);
    }
    Log("Http Server stop");
}

CIOClient* CHttpServer::CreateIOClient(CIOContainer* pContainer)
{
    map<tcp::endpoint, CHttpProfile>::iterator it;
    it = mapProfile.find(pContainer->GetServiceEndpoint());
    if (it != mapProfile.end() && (*it).second.pSSLContext != nullptr)
    {
        return new CSSLClient(pContainer, GetIoService(), *(*it).second.pSSLContext);
    }
    return CIOProc::CreateIOClient(pContainer);
}

void CHttpServer::HandleClientRecv(CHttpClient* pHttpClient, MAPIKeyValue& mapHeader,
                                   MAPKeyValue& mapQuery, MAPIKeyValue& mapCookie,
                                   CBufStream& ssPayload)
{
    CHttpProfile* pHttpProfile = pHttpClient->GetProfile();
    CEventHttpReq* pEventHttpReq = new CEventHttpReq(pHttpClient->GetNonce());
    if (pEventHttpReq == nullptr)
    {
        RespondError(pHttpClient, 500);
        return;
    }

    CHttpReq& req = pEventHttpReq->data;

    req.mapHeader = mapHeader;
    req.mapQuery = mapQuery;
    req.mapCookie = mapCookie;
    ssPayload >> *pEventHttpReq;

    req.strUser = "";
    if (!pHttpProfile->mapAuthrizeUser.empty())
    {
        map<string, string>::iterator it;
        it = pHttpProfile->mapAuthrizeUser.find(mapHeader["authorization"]);
        if (it == pHttpProfile->mapAuthrizeUser.end())
        {
            RespondError(pHttpClient, 401);
            delete pEventHttpReq;
            return;
        }
        req.strUser = (*it).second;
    }

    if (mapHeader["method"] == "POST" || mapHeader["method"] == "GET")
    {
        string& strContentType = mapHeader["content-type"];
        if (strncasecmp("application/x-www-form-urlencoded", strContentType.c_str(),
                        sizeof("application/x-www-form-urlencoded") - 1)
                == 0
            && !CHttpUtil().ParseRequestQuery(req.strContent, req.mapQuery))
        {
            RespondError(pHttpClient, 400);
            delete pEventHttpReq;
            return;
        }
    }
    pHttpProfile->pIOModule->PostEvent(pEventHttpReq);
}

void CHttpServer::HandleClientSent(CHttpClient* pHttpClient)
{
    if (pHttpClient->IsKeepAlive())
    {
        pHttpClient->Activate();
    }
    else
    {
        RemoveClient(pHttpClient);
    }
}

void CHttpServer::HandleClientError(CHttpClient* pHttpClient)
{
    RemoveClient(pHttpClient);
}

bool CHttpServer::ClientAccepted(const tcp::endpoint& epService, CIOClient* pClient, std::string& strFailCause)
{
    map<tcp::endpoint, CHttpProfile>::iterator it = mapProfile.find(epService);
    if (it == mapProfile.end())
    {
        strFailCause = "Query profile of service failed";
        return false;
    }
    if (AddNewClient(pClient, &(*it).second) == nullptr)
    {
        strFailCause = "Add new client failed";
        return false;
    }
    return true;
}

CHttpClient* CHttpServer::AddNewClient(CIOClient* pClient, CHttpProfile* pHttpProfile)
{
    uint64 nNonce = 0;
    RAND_bytes((unsigned char*)&nNonce, sizeof(nNonce));
    while (mapClient.count(nNonce))
    {
        RAND_bytes((unsigned char*)&nNonce, sizeof(nNonce));
    }

    CHttpClient* pHttpClient = new CHttpClient(this, pHttpProfile, pClient, nNonce);
    if (pHttpClient != nullptr)
    {
        mapClient.insert(make_pair(nNonce, pHttpClient));
        pHttpClient->Activate();
    }
    return pHttpClient;
}

void CHttpServer::RemoveClient(CHttpClient* pHttpClient)
{
    mapClient.erase(pHttpClient->GetNonce());
    CEventHttpBroken* pEventHttpBroken = new CEventHttpBroken(pHttpClient->GetNonce());
    if (pEventHttpBroken != nullptr)
    {
        pEventHttpBroken->data.fEventStream = pHttpClient->IsEventStream();
        pHttpClient->GetProfile()->pIOModule->PostEvent(pEventHttpBroken);
    }

    delete pHttpClient;
}

void CHttpServer::RespondError(CHttpClient* pHttpClient, int nStatusCode, const string& strError)
{
    string strContent = strError;
    if (strContent.empty())
    {
        string strStatus;
        if (nStatusCode == 400)
            strStatus = "Bad Request";
        if (nStatusCode == 401)
            strStatus = "Authorization Required";
        if (nStatusCode == 403)
            strStatus = "Forbidden";
        if (nStatusCode == 404)
            strStatus = "Not Found";
        if (nStatusCode == 500)
            strStatus = "Internal Server Error";
        strContent = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">"
                     "<html><head><title>"
                     + strStatus + "</title></head>"
                                   "<body><h1>"
                     + strStatus + "</h1><hr></body></html>";
    }

    MAPIKeyValue mapHeader;
    MAPCookie mapCookie;
    mapHeader["connection"] = "Close";
    if (!strContent.empty())
    {
        mapHeader["content-type"] = "text/html";
    }

    string strRsp = CHttpUtil().BuildResponseHeader(nStatusCode, mapHeader, mapCookie, strContent.size())
                    + strContent;

    pHttpClient->SendResponse(strRsp);
}

bool CHttpServer::HandleEvent(CEventHttpRsp& eventRsp)
{
    map<uint64, CHttpClient*>::iterator it = mapClient.find(eventRsp.nNonce);
    if (it == mapClient.end())
    {
        return false;
    }

    CHttpClient* pHttpClient = (*it).second;

    CHttpRsp& rsp = eventRsp.data;

    string strRsp = CHttpUtil().BuildResponseHeader(rsp.nStatusCode, rsp.mapHeader,
                                                    rsp.mapCookie, rsp.strContent.size())
                    + rsp.strContent;

    if (rsp.mapHeader.count("content-type")
        && rsp.mapHeader["content-type"] == "text/event-stream")
    {
        pHttpClient->SetEventStream();
    }

    if (rsp.mapHeader.count("connection") && rsp.mapHeader["connection"] == "Keep-Alive")
    {
        pHttpClient->KeepAlive();
    }
    pHttpClient->SendResponse(strRsp);
    return true;
}

} // namespace xengine
