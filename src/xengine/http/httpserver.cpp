// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "httpserver.h"

#include <boost/algorithm/string/trim.hpp>
#include <openssl/rand.h>
#include "nonce.h"

using namespace std;
using boost::asio::ip::tcp;

namespace xengine
{

///////////////////////////////
// CHttpClient

CHttpClient::CHttpClient(CHttpServer* pServerIn, CHttpProfile* pProfileIn,
                         CIOClient* pClientIn, CNoncePtr spNonceIn)
  : pServer(pServerIn), pProfile(pProfileIn), pClient(pClientIn),
    spNonce(spNonceIn), fKeepAlive(false), fEventStream(false)
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

CNoncePtr CHttpClient::GetNonce()
{
    return spNonce;
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
            ERROR("Failed to alloc ssl context for %s:%u", confHost.epHost.address().to_string().c_str(),
                  confHost.epHost.port());
            return false;
        }
        if (!confHost.optSSL.SetupSSLContext(*profile.pSSLContext))
        {
            ERROR("Failed to setup ssl context for %s:%u", confHost.epHost.address().to_string().c_str(),
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

    RegisterRefHandler<CHttpRspMessage>(boost::bind(&CHttpServer::HandleHttpRsp, this, _1));

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

    DeregisterHandler(CHttpRspMessage::MessageType());
}

bool CHttpServer::EnterLoop()
{
    INFO("Http Server start:");
    if (!CIOProc::EnterLoop())
    {
        return false;
    }

    for (map<tcp::endpoint, CHttpProfile>::iterator it = mapProfile.begin();
         it != mapProfile.end(); ++it)
    {
        if (!StartService((*it).first, (*it).second.nMaxConnections, (*it).second.vAllowMask))
        {
            ERROR("Setup http service failed, listen port = %d, connection limit %d",
                  (*it).first.port(), (*it).second.nMaxConnections);
        }
        else
        {
            INFO("Setup http service sucess, listen port = %d, connection limit %d",
                 (*it).first.port(), (*it).second.nMaxConnections);
        }
    }

    return true;
}

void CHttpServer::LeaveLoop()
{
    vector<CHttpClient*> vClient;
    for (auto it = mapClient.begin(); it != mapClient.end(); ++it)
    {
        vClient.push_back((*it).second);
    }
    for (CHttpClient* pClient : vClient)
    {
        RemoveClient(pClient);
    }

    for (map<tcp::endpoint, CHttpProfile>::iterator it = mapProfile.begin();
         it != mapProfile.end(); ++it)
    {
        StopService(it->first);
    }

    CIOProc::LeaveLoop();
    INFO("Http Server stop");
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
    auto spHttpReqMsg = CHttpReqMessage::Create();
    spHttpReqMsg->spNonce = pHttpClient->GetNonce();

    spHttpReqMsg->mapHeader = mapHeader;
    spHttpReqMsg->mapQuery = mapQuery;
    spHttpReqMsg->mapCookie = mapCookie;
    ssPayload >> *spHttpReqMsg;

    spHttpReqMsg->strUser = "";
    if (!pHttpProfile->mapAuthrizeUser.empty())
    {
        map<string, string>::iterator it;
        it = pHttpProfile->mapAuthrizeUser.find(mapHeader["authorization"]);
        if (it == pHttpProfile->mapAuthrizeUser.end())
        {
            RespondError(pHttpClient, 401);
            return;
        }
        spHttpReqMsg->strUser = (*it).second;
    }

    if (mapHeader["method"] == "POST" || mapHeader["method"] == "GET")
    {
        string& strContentType = mapHeader["content-type"];
        if (strncasecmp("application/x-www-form-urlencoded", strContentType.c_str(),
                        sizeof("application/x-www-form-urlencoded") - 1)
                == 0
            && !CHttpUtil().ParseRequestQuery(spHttpReqMsg->strContent, spHttpReqMsg->mapQuery))
        {
            RespondError(pHttpClient, 400);
            return;
        }
    }
    PUBLISH_MESSAGE(spHttpReqMsg);
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

bool CHttpServer::ClientAccepted(const tcp::endpoint& epService, CIOClient* pClient)
{
    map<tcp::endpoint, CHttpProfile>::iterator it = mapProfile.find(epService);
    if (it == mapProfile.end())
    {
        return false;
    }
    return (AddNewClient(pClient, &(*it).second) != nullptr);
}

CHttpClient* CHttpServer::AddNewClient(CIOClient* pClient, CHttpProfile* pHttpProfile)
{
    CNoncePtr spNonce = CNonce::Create();
    do
    {
        spNonce->nNonce = CreateNonce(HTTP_NONCE_TYPE);
    } while (mapClient.count(spNonce));

    CHttpClient* pHttpClient = new CHttpClient(this, pHttpProfile, pClient, spNonce);
    if (pHttpClient != nullptr)
    {
        mapClient.insert(make_pair(spNonce, pHttpClient));
        pHttpClient->Activate();
    }
    return pHttpClient;
}

void CHttpServer::RemoveClient(CHttpClient* pHttpClient)
{
    mapClient.erase(pHttpClient->GetNonce());

    auto spHttpBrokenMsg = CHttpBrokenMessage::Create();
    spHttpBrokenMsg->spNonce = pHttpClient->GetNonce();
    spHttpBrokenMsg->fEventStream = pHttpClient->IsEventStream();
    PUBLISH_MESSAGE(spHttpBrokenMsg);
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

void CHttpServer::HandleHttpRsp(const CHttpRspMessage& msg)
{
    auto it = mapClient.find(msg.spNonce);
    if (it == mapClient.end())
    {
        return;
    }

    CHttpClient* pHttpClient = (*it).second;

    auto mapHeader = msg.mapHeader;
    auto mapCookie = msg.mapCookie;
    string strRsp = CHttpUtil().BuildResponseHeader(msg.nStatusCode, mapHeader,
                                                    mapCookie, msg.strContent.size())
                    + msg.strContent;

    if (msg.mapHeader.count("content-type")
        && msg.mapHeader.at("content-type") == "text/event-stream")
    {
        pHttpClient->SetEventStream();
    }

    if (msg.mapHeader.count("connection") && msg.mapHeader.at("connection") == "Keep-Alive")
    {
        pHttpClient->KeepAlive();
    }
    pHttpClient->SendResponse(strRsp);
}

} // namespace xengine
