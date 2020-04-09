// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_DBP_CLIENT_H
#define BIGBANG_DBP_CLIENT_H

#include "netio/ioproc.h"
#include "event.h"
#include <boost/bimap.hpp>
#include <boost/any.hpp>
#include <queue>

#include "dbputils.h"

using namespace xengine;

namespace bigbang
{

class CBbDbpClient;

class CDbpClientConfig
{
public:
    CDbpClientConfig(){}
    CDbpClientConfig(const boost::asio::ip::tcp::endpoint& epParentHostIn,
                    const std::string&  SupportForksIn,
                    const CIOSSLOption& optSSLIn, 
                    const std::string& strIOModuleIn)
    : epParentHost(epParentHostIn),
      optSSL(optSSLIn),
      strIOModule(strIOModuleIn)
    {
        vSupportForks = CDbpUtils::Split(SupportForksIn,';');
    }
public:
    boost::asio::ip::tcp::endpoint epParentHost;
    std::vector<std::string> vSupportForks;
    CIOSSLOption optSSL;
    std::string strIOModule;
};

class CDbpClientProfile
{
public:
    CDbpClientProfile() : pIOModule(NULL) {}
public:
    IIOModule* pIOModule;
    CIOSSLOption optSSL;
    boost::asio::ip::tcp::endpoint epParentHost;
    std::vector<std::string> vSupportForks;
};

class CBbDbpClientSocket
{
public:
    CBbDbpClientSocket(IIOModule* pIOModuleIn,const uint64 nNonceIn,
                   CBbDbpClient* pDbpClientIn,CIOClient* pClientIn);
    ~CBbDbpClientSocket();

    IIOModule* GetIOModule();
    uint64 GetNonce();
    CNetHost GetHost();
    std::string GetSession() const;
    void SetSession(const std::string& session);
    
    void ReadMessage();
    void SendPong(const std::string& id);
    void SendPing(const std::string& id);

    void SendForkIds(const std::vector<std::string>& forks);
    void SendSubScribeTopics(const std::vector<std::string>& topics);
    void SendConnectSession(const std::string& session, const std::vector<std::string>& forks);
protected:
    void StartReadHeader();
    void StartReadPayload(std::size_t nLength);
    
    void HandleWritenRequest(std::size_t nTransferred, dbp::Msg type);
    void HandleReadHeader(std::size_t nTransferred);
    void HandleReadPayload(std::size_t nTransferred,uint32_t len);
    void HandleReadCompleted(uint32_t len);

    void SendForkId(const std::string& fork);
    void SendSubscribeTopic(const std::string& topic);
    void SendMessage(dbp::Msg type, google::protobuf::Any* any);

    bool IsSentComplete();

private:
    std::string strSessionId;

protected:
    IIOModule* pIOModule;
    const uint64 nNonce;
    CBbDbpClient* pDbpClient;
    CIOClient* pClient;

    CBufStream ssRecv;
    CBufStream ssSend;

    std::queue<std::pair<dbp::Msg, std::string>> queueMessage;
    bool IsReading;
};

class CBbSessionProfile
{
public:
    CBbDbpClientSocket* pClientSocket;
    std::string strSessionId;
    uint64 nTimeStamp;
    std::shared_ptr<boost::asio::deadline_timer> ptrPingTimer;
};

class CBbDbpClient : public CIOProc, virtual public CDBPEventListener
{
public:
    CBbDbpClient();
    virtual ~CBbDbpClient() noexcept;

    void HandleClientSocketError(CBbDbpClientSocket* pClientSocket);
    void HandleClientSocketSent(CBbDbpClientSocket* pClientSocket);
    void HandleClientSocketRecv(CBbDbpClientSocket* pClientSocket, const boost::any& anyObj);
    void AddNewClient(const CDbpClientConfig& confClient);

    void HandleConnected(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any);
    void HandleFailed(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any);
    void HandlePing(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any);
    void HandlePong(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any);
    void HandleResult(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any);
    void HandleAdded(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any);
    void HandleReady(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any);
    void HandleNoSub(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any);

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    void EnterLoop() override;
    void LeaveLoop() override;

    bool ClientConnected(CIOClient* pClient) override;
    void ClientFailToConnect(const boost::asio::ip::tcp::endpoint& epRemote) override;
    void Timeout(uint64 nNonce,uint32 nTimerId,const std::string& strFunctionIn) override;

    bool CreateProfile(const CDbpClientConfig& confClient);
    bool StartConnection(const boost::asio::ip::tcp::endpoint& epRemote, int64 nTimeout, bool fEnableSSL,
            const CIOSSLOption& optSSL);
    void RegisterDefaultForks(CBbDbpClientSocket* pClientSocket);
    void SubscribeDefaultTopics(CBbDbpClientSocket* pClientSocket);
    void StartPingTimer(const std::string& session);
    void SendPingHandler(const boost::system::error_code& err, const CBbSessionProfile& sessionProfile);
    void CreateSession(const std::string& session, CBbDbpClientSocket* pClientSocket);
    bool HaveAssociatedSessionOf(CBbDbpClientSocket* pClientSocket);
    bool IsSessionExist(const std::string& session);
    bool IsForkNode();    

    bool ActivateConnect(CIOClient* pClient);
    void CloseConnect(CBbDbpClientSocket* pClientSocket);
    void RemoveSession(CBbDbpClientSocket* pClientSocket);
    void RemoveClientSocket(CBbDbpClientSocket* pClientSocket);

protected:
    bool HandleEvent(CBbEventDbpRegisterForkID& event) override;
    bool HandleEvent(CBbEventDbpSendBlock& event) override;
    bool HandleEvent(CBbEventDbpSendTx& event) override;

protected:
    std::vector<CDbpClientConfig> vecClientConfig;
    std::map<boost::asio::ip::tcp::endpoint, CDbpClientProfile> mapProfile;
    std::map<uint64, CBbDbpClientSocket*> mapClientSocket; // nonce => CDbpClientSocket

    typedef boost::bimap<std::string, CBbDbpClientSocket*> SessionClientSocketBimapType;
    typedef SessionClientSocketBimapType::value_type position_pair;
    SessionClientSocketBimapType bimapSessionClientSocket;      // session id <=> CBbDbpClientSocket
    std::map<std::string, CBbSessionProfile> mapSessionProfile; // session id => session profile

private:
    IIOModule* pDbpService;

};

} // namespace bigbang

#endif // BIGBANG_DBP_CLIENT_H

