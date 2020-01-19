// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_HTTP_HTTPSERVER_H
#define XENGINE_HTTP_HTTPSERVER_H

#include "http/httpevent.h"
#include "http/httputil.h"
#include "netio/ioproc.h"

namespace xengine
{

class CHttpServer;

class CHttpHostConfig
{
public:
    CHttpHostConfig() {}
    CHttpHostConfig(const boost::asio::ip::tcp::endpoint& epHostIn, unsigned int nMaxConnectionsIn,
                    const CIOSSLOption& optSSLIn, const std::map<std::string, std::string>& mapUserPassIn,
                    const std::vector<std::string>& vAllowMaskIn, const std::string& strIOModuleIn)
      : epHost(epHostIn), nMaxConnections(nMaxConnectionsIn), optSSL(optSSLIn),
        mapUserPass(mapUserPassIn), vAllowMask(vAllowMaskIn), strIOModule(strIOModuleIn)
    {
    }

public:
    boost::asio::ip::tcp::endpoint epHost;
    unsigned int nMaxConnections;
    CIOSSLOption optSSL;
    std::map<std::string, std::string> mapUserPass;
    std::vector<std::string> vAllowMask;
    std::string strIOModule;
};

class CHttpProfile
{
public:
    CHttpProfile()
      : pIOModule(nullptr), pSSLContext(nullptr), nMaxConnections(0) {}

public:
    IIOModule* pIOModule;
    boost::asio::ssl::context* pSSLContext;
    std::map<std::string, std::string> mapAuthrizeUser;
    std::vector<std::string> vAllowMask;
    unsigned int nMaxConnections;
};

class CHttpClient
{
public:
    CHttpClient(CHttpServer* pServerIn, CHttpProfile* pProfileIn,
                CIOClient* pClientIn, uint64 nNonceIn);
    ~CHttpClient();
    CHttpProfile* GetProfile();
    uint64 GetNonce();
    bool IsKeepAlive();
    bool IsEventStream();
    void KeepAlive();
    void SetEventStream();
    void Activate();
    void SendResponse(std::string& strResponse);

protected:
    void StartReadHeader();
    void StartReadPayload(std::size_t nLength);

    void HandleReadHeader(std::size_t nTransferred);
    void HandleReadPayload(std::size_t nTransferred);
    void HandleReadCompleted();
    void HandleWritenResponse(std::size_t nTransferred);

protected:
    CHttpServer* pServer;
    CHttpProfile* pProfile;
    CIOClient* pClient;
    uint64 nNonce;
    bool fKeepAlive;
    bool fEventStream;
    CBufStream ssRecv;
    CBufStream ssSend;
    MAPIKeyValue mapHeader;
    MAPKeyValue mapQuery;
    MAPIKeyValue mapCookie;
};

class CHttpServer : public CIOProc, virtual public CHttpEventListener
{
public:
    CHttpServer();
    virtual ~CHttpServer();
    CIOClient* CreateIOClient(CIOContainer* pContainer) override;
    void HandleClientRecv(CHttpClient* pHttpClient, MAPIKeyValue& mapHeader,
                          MAPKeyValue& mapQuery, MAPIKeyValue& mapCookie,
                          CBufStream& ssPayload);
    void HandleClientSent(CHttpClient* pHttpClient);
    void HandleClientError(CHttpClient* pHttpClient);
    void AddNewHost(const CHttpHostConfig& confHost);

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    void EnterLoop() override;
    void LeaveLoop() override;

    bool ClientAccepted(const boost::asio::ip::tcp::endpoint& epService, CIOClient* pClient, std::string& strFailCause) override;

    bool CreateProfile(const CHttpHostConfig& confHost);
    CHttpClient* AddNewClient(CIOClient* pClient, CHttpProfile* pHttpProfile);
    void RemoveClient(CHttpClient* pHttpClient);
    void RespondError(CHttpClient* pHttpClient, int nStatusCode, const std::string& strError = "");
    bool HandleEvent(CEventHttpRsp& eventRsp) override;

protected:
    std::vector<CHttpHostConfig> vecHostConfig;
    std::map<boost::asio::ip::tcp::endpoint, CHttpProfile> mapProfile;
    std::map<uint64, CHttpClient*> mapClient;
};

} // namespace xengine

#endif //XENGINE_HTTP_HTTPSERVER_H
