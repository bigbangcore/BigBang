// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dbpclient.h"
#include "dbputils.h"
#include "netio/netio.h"
#include "uint256.h"

#include <thread>
#include <chrono>
#include <memory>
#include <algorithm>
#include <openssl/rand.h>
 
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>


static std::size_t MSG_HEADER_LEN = 4;

#define DBPCLIENT_CONNECT_TIMEOUT 10 

namespace bigbang
{

CBbDbpClientSocket::CBbDbpClientSocket(IIOModule* pIOModuleIn,const uint64 nNonceIn,
                   CBbDbpClient* pDbpClientIn,CIOClient* pClientIn)
: pIOModule(pIOModuleIn),
  nNonce(nNonceIn),
  pDbpClient(pDbpClientIn),
  pClient(pClientIn),
  IsReading(false)
{
    ssRecv.Clear();
}
    
CBbDbpClientSocket::~CBbDbpClientSocket()
{
    if(pClient)
    {
        pClient->Close();
    }
}

IIOModule* CBbDbpClientSocket::GetIOModule()
{
    return pIOModule;
}
    
uint64 CBbDbpClientSocket::GetNonce()
{
    return nNonce;
}
    
CNetHost CBbDbpClientSocket::GetHost()
{
    return CNetHost(pClient->GetRemote());
}

std::string CBbDbpClientSocket::GetSession() const
{
    return strSessionId;
}

void CBbDbpClientSocket::SetSession(const std::string& session)
{
    strSessionId = session;
}

void CBbDbpClientSocket::ReadMessage()
{
    StartReadHeader();
}

void CBbDbpClientSocket::SendPong(const std::string& id)
{
    dbp::Pong pong;
    pong.set_id(id);

    google::protobuf::Any *any = new google::protobuf::Any();
    any->PackFrom(pong);

    SendMessage(dbp::Msg::PONG,any);
}

void CBbDbpClientSocket::SendPing(const std::string& id)
{
    dbp::Ping ping;
    ping.set_id(id);

    google::protobuf::Any *any = new google::protobuf::Any();
    any->PackFrom(ping);

    SendMessage(dbp::Msg::PING,any);
}

void CBbDbpClientSocket::SendForkId(const std::string& fork)
{
    sn::RegisterForkIDArg forkArg;
    forkArg.set_id(fork);

    google::protobuf::Any *fork_any = new google::protobuf::Any();
    fork_any->PackFrom(forkArg);
    
    dbp::Method method;
    method.set_method("registerforkid");
    std::string id(CDbpUtils::RandomString());  
    method.set_id(id);
    method.set_allocated_params(fork_any);

    google::protobuf::Any *any = new google::protobuf::Any();
    any->PackFrom(method);

    SendMessage(dbp::Msg::METHOD,any);
}

void CBbDbpClientSocket::SendSubscribeTopic(const std::string& topic)
{
    dbp::Sub sub;
    sub.set_name(topic);
    std::string id(CDbpUtils::RandomString());
    sub.set_id(id);
    google::protobuf::Any *any = new google::protobuf::Any();
    any->PackFrom(sub);

    SendMessage(dbp::Msg::SUB,any);
}

void CBbDbpClientSocket::SendForkIds(const std::vector<std::string>& forks)
{
    for(const auto& fork : forks)
    {
        SendForkId(fork);
    }
}

void CBbDbpClientSocket::SendSubScribeTopics(const std::vector<std::string>& topics)
{
    for(const auto& topic : topics)
    {
        SendSubscribeTopic(topic);
    }
}

void CBbDbpClientSocket::SendConnectSession(const std::string& session, const std::vector<std::string>& forks)
{
    dbp::Connect connect;
    connect.set_session(session);
    connect.set_client("supernode");
    connect.set_version(1);

    google::protobuf::Any anyFork;
    sn::ForkID forkidMsg;

    for(const auto& fork : forks)
    {
        forkidMsg.add_ids(fork);
    }
    
    anyFork.PackFrom(forkidMsg);
    (*connect.mutable_udata())["supernode-forks"] = anyFork;

    google::protobuf::Any *any = new google::protobuf::Any();
    any->PackFrom(connect);

    SendMessage(dbp::Msg::CONNECT,any);
}

bool CBbDbpClientSocket::IsSentComplete()
{
    return (ssSend.GetSize() == 0 && queueMessage.empty());
}

void CBbDbpClientSocket::SendMessage(dbp::Msg type, google::protobuf::Any* any)
{
    dbp::Base base;
    base.set_msg(type);
    base.set_allocated_object(any);

    std::string bytes;
    base.SerializeToString(&bytes);

    uint32_t len;
    char len_buffer[4];
    len = bytes.size();
    len = htonl(len);
    std::memcpy(&len_buffer[0], &len, 4);
    bytes.insert(0, len_buffer, 4);

    if(!IsSentComplete())
    {
        queueMessage.push(std::make_pair(type,bytes));
        return;
    }
    
    ssSend.Write((char*)bytes.data(),bytes.size());
    pClient->Write(ssSend,boost::bind(&CBbDbpClientSocket::HandleWritenRequest,this,_1, type));
    
}

void CBbDbpClientSocket::StartReadHeader()
{
    IsReading = true;
    pClient->Read(ssRecv, MSG_HEADER_LEN,
                  boost::bind(&CBbDbpClientSocket::HandleReadHeader, this, _1));
}

void CBbDbpClientSocket::StartReadPayload(std::size_t nLength)
{
    pClient->Read(ssRecv, nLength,
                  boost::bind(&CBbDbpClientSocket::HandleReadPayload, this, _1, nLength));
}

void CBbDbpClientSocket::HandleWritenRequest(std::size_t nTransferred, dbp::Msg type)
{ 
    if(nTransferred != 0)
    {
        if(ssSend.GetSize() != 0)
        {
            pClient->Write(ssSend,boost::bind(&CBbDbpClientSocket::HandleWritenRequest,this,_1, type));
            return;
        }

        if(ssSend.GetSize() == 0 && !queueMessage.empty())
        {
            auto messagePair = queueMessage.front();
            dbp::Msg msgType = messagePair.first;
            std::string bytes = messagePair.second;
            queueMessage.pop();
            ssSend.Write((char*)bytes.data(),bytes.size());
            pClient->Write(ssSend,boost::bind(&CBbDbpClientSocket::HandleWritenRequest,this,_1, msgType));
            return;
        }

        if(!IsReading)
        {
            pDbpClient->HandleClientSocketSent(this);
        }
    
    }
    else
    {
        pDbpClient->HandleClientSocketError(this);
    }
}
    
void CBbDbpClientSocket::HandleReadHeader(std::size_t nTransferred)
{   
    if (nTransferred == MSG_HEADER_LEN)
    {
        std::string lenBuffer(ssRecv.GetData(), ssRecv.GetData() + MSG_HEADER_LEN);
        uint32_t nMsgHeaderLen = CDbpUtils::ParseLenFromMsgHeader(&lenBuffer[0], MSG_HEADER_LEN);
        if (nMsgHeaderLen == 0)
        {
            std::cerr << "Msg Base header length is 0 [dbpclient]" << std::endl;
            pDbpClient->HandleClientSocketError(this);
            return;
        }

        StartReadPayload(nMsgHeaderLen);
    }
    else
    {
        std::cerr << "Msg Base header length is not 4 [dbpclient]" << std::endl;
        pDbpClient->HandleClientSocketError(this);
    }
}
    
void CBbDbpClientSocket::HandleReadPayload(std::size_t nTransferred,uint32_t len)
{
    if (nTransferred == len)
    {
        HandleReadCompleted(len);
    }
    else
    {
        std::cerr << "pay load is not len. [dbpclient]" << std::endl;
        pDbpClient->HandleClientSocketError(this);
    }
}

void CBbDbpClientSocket::HandleReadCompleted(uint32_t len)
{ 
    char head[4];
    ssRecv.Read(head,4);
    std::string payloadBuffer(len, 0);
    ssRecv.Read(&payloadBuffer[0], len);
    IsReading = false;

    dbp::Base msgBase;
    if (!msgBase.ParseFromString(payloadBuffer))
    {
        std::cerr << "parse payload failed. [dbpclient]" << std::endl;
        pDbpClient->HandleClientSocketError(this);
        return;
    }

    dbp::Msg currentMsgType = msgBase.msg();
    google::protobuf::Any anyObj = msgBase.object();
    switch(currentMsgType)
    {
    case dbp::CONNECTED:
        pDbpClient->HandleClientSocketRecv(this,anyObj);
        break;
    case dbp::FAILED:
        pDbpClient->HandleClientSocketRecv(this,anyObj);
        break;
    case dbp::PING:
        pDbpClient->HandleClientSocketRecv(this,anyObj);
        break;
    case dbp::PONG:
        pDbpClient->HandleClientSocketRecv(this,anyObj);
        pDbpClient->HandleClientSocketSent(this);
        break;
    case dbp::RESULT:
        pDbpClient->HandleClientSocketRecv(this,anyObj);
        pDbpClient->HandleClientSocketSent(this);
        break;
    case dbp::NOSUB:
        pDbpClient->HandleClientSocketRecv(this,anyObj);
        break;
    case dbp::READY:
        pDbpClient->HandleClientSocketRecv(this,anyObj);
        pDbpClient->HandleClientSocketSent(this);
        break;
    case dbp::ADDED:
        pDbpClient->HandleClientSocketRecv(this,anyObj);
        pDbpClient->HandleClientSocketSent(this);
        break;
    default:
        std::cerr << "is not Message Base Type is unknown. [dbpclient]" << std::endl;
        pDbpClient->HandleClientSocketError(this);
        break;
    }
}

CBbDbpClient::CBbDbpClient()
  : CIOProc("dbpclient")
{
    pDbpService = NULL;
}

CBbDbpClient::~CBbDbpClient() noexcept
{
}


void CBbDbpClient::HandleClientSocketError(CBbDbpClientSocket* pClientSocket)
{
    std::cerr << "Client Socket Error." << std::endl; 
    
    CBbEventDbpBroken *pEventDbpBroken = new CBbEventDbpBroken(pClientSocket->GetSession());
    if(pEventDbpBroken)
    {
        pClientSocket->GetIOModule()->PostEvent(pEventDbpBroken);
    }

    auto epRemote = pClientSocket->GetHost().ToEndPoint();
    RemoveClientSocket(pClientSocket);

    auto it = mapProfile.find(epRemote);
    if(it != mapProfile.end())
    {
        StartConnection(epRemote,DBPCLIENT_CONNECT_TIMEOUT, 
            it->second.optSSL.fEnable,it->second.optSSL);
    }
    else
    {
        std::cerr << "cannot find reconnect parent node " << 
            epRemote.address().to_string() << " failed, " 
            << "port " << epRemote.port() << std::endl;
    }
}

void CBbDbpClient::HandleClientSocketSent(CBbDbpClientSocket* pClientSocket)
{
    pClientSocket->ReadMessage();
}

void CBbDbpClient::HandleClientSocketRecv(CBbDbpClientSocket* pClientSocket, const boost::any& anyObj)
{
    if(anyObj.type() != typeid(google::protobuf::Any))
    {
        return;
    }

    google::protobuf::Any any = boost::any_cast<google::protobuf::Any>(anyObj);

    if(any.Is<dbp::Connected>())
    {
        HandleConnected(pClientSocket,&any);
    }
    else if(any.Is<dbp::Failed>())
    {
        HandleFailed(pClientSocket,&any);
    }
    else if(any.Is<dbp::Ping>())
    {
        if(!HaveAssociatedSessionOf(pClientSocket))
        {
            return;
        }
        
        HandlePing(pClientSocket,&any);
    }
    else if(any.Is<dbp::Pong>())
    {
        if(!HaveAssociatedSessionOf(pClientSocket))
        {
            return;
        }

        HandlePong(pClientSocket,&any);
    }
    else if(any.Is<dbp::Result>())
    {
        if(!HaveAssociatedSessionOf(pClientSocket))
        {
            return;
        }

        HandleResult(pClientSocket,&any);
    }
    else if(any.Is<dbp::Added>())
    {
        if(!HaveAssociatedSessionOf(pClientSocket))
        {
            return;
        }

        HandleAdded(pClientSocket,&any);
    }
    else if(any.Is<dbp::Ready>())
    {
        if(!HaveAssociatedSessionOf(pClientSocket))
        {
            return;
        }

        HandleReady(pClientSocket,&any);
    }
    else if(any.Is<dbp::Nosub>())
    {
        if(!HaveAssociatedSessionOf(pClientSocket))
        {
            return;
        }

        HandleNoSub(pClientSocket,&any);
    }
    else
    {

    }

}

void CBbDbpClient::AddNewClient(const CDbpClientConfig& confClient)
{
    vecClientConfig.push_back(confClient);
}

void CBbDbpClient::HandleConnected(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any)
{
    dbp::Connected connected;
    any->UnpackTo(&connected);
    CreateSession(connected.session(),pClientSocket);
    
    if(IsSessionExist(connected.session()))
    {
        StartPingTimer(connected.session());
        RegisterDefaultForks(pClientSocket);
        SubscribeDefaultTopics(pClientSocket);
    }
}

void CBbDbpClient::HandleFailed(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any)
{   
    dbp::Failed failed;
    any->UnpackTo(&failed);
    
    if(failed.reason() == "002")
    {
        std::cerr << "[<] session timeout: " << failed.reason() << std::endl;    
    }
    
   
    auto epRemote = pClientSocket->GetHost().ToEndPoint();
    RemoveClientSocket(pClientSocket);
    
    auto it = mapProfile.find(epRemote);
    if(it != mapProfile.end())
    {
        StartConnection(epRemote,DBPCLIENT_CONNECT_TIMEOUT, 
            it->second.optSSL.fEnable,it->second.optSSL);
    }
    else
    {
        std::cerr << "cannot find reconnect parent node " << 
            epRemote.address().to_string() << " failed, " 
            << "port " << epRemote.port() << std::endl;
    }
}
    
void CBbDbpClient::HandlePing(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any)
{
    dbp::Ping ping;
    any->UnpackTo(&ping);
    pClientSocket->SendPong(ping.id());
}
    
void CBbDbpClient::HandlePong(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any)
{
    dbp::Pong pong;
    any->UnpackTo(&pong);

    std::string session = bimapSessionClientSocket.right.at(pClientSocket);
    if(IsSessionExist(session))
    {
        mapSessionProfile[session].nTimeStamp = CDbpUtils::CurrentUTC();
    }
}

void CBbDbpClient::HandleResult(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any)
{
    dbp::Result result;
    any->UnpackTo(&result);

    if (!result.error().empty())
    {
        std::cerr << "[<]method error:" << result.error()  <<  " [dbpclient]" << std::endl;    
        return;
    }

    int size = result.result_size();
    for (int i = 0; i < size; i++)
    {
        google::protobuf::Any any = result.result(i);
        
        if(any.Is<sn::RegisterForkIDRet>())
        {
            sn::RegisterForkIDRet ret;
            any.UnpackTo(&ret);
        }
        
    }
}

void CBbDbpClient::HandleAdded(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any)
{
    dbp::Added added;
    any->UnpackTo(&added);

    if (added.name() == "all-block")
    {
        sn::Block block;
        added.object().UnpackTo(&block);

        CBbDbpBlock dbpBlock;
        CDbpUtils::SnToDbpBlock(&block,dbpBlock);
        
        CBbEventDbpAdded* pEventAdded =  new CBbEventDbpAdded(pClientSocket->GetSession());
        pEventAdded->data.id = added.id();
        pEventAdded->data.name = added.name();
        pEventAdded->data.anyAddedObj = dbpBlock;

        pDbpService->PostEvent(pEventAdded);
        
    }

    if (added.name() == "all-tx")
    {
        sn::Transaction tx;
        added.object().UnpackTo(&tx);

        CBbDbpTransaction dbpTx;
        CDbpUtils::SnToDbpTransaction(&tx,&dbpTx);

        CBbEventDbpAdded* pEventAdded =  new CBbEventDbpAdded(pClientSocket->GetSession());
        pEventAdded->data.id = added.id();
        pEventAdded->data.name = added.name();
        pEventAdded->data.anyAddedObj = dbpTx;

        pDbpService->PostEvent(pEventAdded);
    }
}

void CBbDbpClient::HandleReady(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any)
{
    dbp::Ready ready;
    any->UnpackTo(&ready);
}

void CBbDbpClient::HandleNoSub(CBbDbpClientSocket* pClientSocket, google::protobuf::Any* any)
{
    dbp::Nosub nosub;
    any->UnpackTo(&nosub);
}

bool CBbDbpClient::HandleInitialize()
{
    // init client config
    for(const auto & confClient : vecClientConfig)
    {
        if(!CreateProfile(confClient))
        {
            return false;
        }
    }

    if(!GetObject("dbpservice",pDbpService))
    {
        //BlockheadError("request dbpservice failed in dbpclient.");
        return false;
    }

    return true;
}

void CBbDbpClient::HandleDeinitialize()
{
    pDbpService = NULL;
    // delete client config
    mapProfile.clear();
}

void CBbDbpClient::EnterLoop()
{
    // start resource
    //BlockheadLog("Dbp Client starting:\n");
    
    for(std::map<boost::asio::ip::tcp::endpoint, CDbpClientProfile>::iterator it = mapProfile.begin();
         it != mapProfile.end(); ++it)
    {
        bool fEnableSSL = (*it).second.optSSL.fEnable;
        if(it->first.address().is_loopback()) continue;
        if(!StartConnection(it->first,DBPCLIENT_CONNECT_TIMEOUT,fEnableSSL,it->second.optSSL))
        {
            //BlockheadWarn("Start to connect parent node %s failed,  port = %d\n",
            //           (*it).first.address().to_string().c_str(),
            //           (*it).first.port());
        }
        else
        {
            //BlockheadLog("Start to connect parent node %s,  port = %d\n",
            //           (*it).first.address().to_string().c_str(),
            //           (*it).first.port());
        }  
    }
}

void CBbDbpClient::LeaveLoop()
{
    // destory resource
    std::vector<CBbSessionProfile> vProfile;
    for(auto it = mapSessionProfile.begin(); it != mapSessionProfile.end(); ++it)
    {
        vProfile.push_back((*it).second);
    }

    for(const auto& profile : vProfile)
    {
        RemoveClientSocket(profile.pClientSocket);
    }

    //BlockheadLog("Dbp Client stop\n");
}

bool CBbDbpClient::ClientConnected(CIOClient* pClient)
{
    auto it = mapProfile.find(pClient->GetRemote());
    if(it == mapProfile.end())
    {
        return false;
    }

    //BlockheadLog("Connect parent node %s success,  port = %d\n",
    //                   (*it).first.address().to_string().c_str(),
    //                   (*it).first.port());

    return ActivateConnect(pClient);
}

void CBbDbpClient::ClientFailToConnect(const boost::asio::ip::tcp::endpoint& epRemote)
{
    //BlockheadWarn("Connect parent node %s failed,  port = %d\n reconnectting\n",
    //                   epRemote.address().to_string().c_str(),
    //                   epRemote.port());

    std::cerr << "Connect parent node " << 
        epRemote.address().to_string() << " failed, " 
        << "port " << epRemote.port() << " and reconnectting." << std::endl;
    
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    auto it = mapProfile.find(epRemote);
    if(it != mapProfile.end())
    {
        StartConnection(epRemote,DBPCLIENT_CONNECT_TIMEOUT, 
            it->second.optSSL.fEnable,it->second.optSSL);
    }
    else
    {
        std::cerr << "cannot find reconnect parent node " << 
            epRemote.address().to_string() << " failed, " 
            << "port " << epRemote.port() << std::endl;
    }
}
    
void CBbDbpClient::Timeout(uint64 nNonce,uint32 nTimerId,const std::string& strFunctionIn)
{
    std::cerr << "time out" << std::endl;

    //BlockheadWarn("Connect parent node %s timeout,  nonce = %d  timerid = %d\n",
    //                   nNonce,
    //                   nTimerId);
}

bool CBbDbpClient::CreateProfile(const CDbpClientConfig& confClient)
{
    CDbpClientProfile profile;
    if(!GetObject(confClient.strIOModule,profile.pIOModule))
    {
        //BlockheadError("Failed to request %s\n", confClient.strIOModule.c_str());
        return false;
    }

    if(confClient.optSSL.fEnable)
        profile.optSSL = confClient.optSSL;
    
    profile.vSupportForks = confClient.vSupportForks;
    profile.epParentHost = confClient.epParentHost;
    mapProfile[confClient.epParentHost] = profile;

    return true;
}

bool CBbDbpClient::StartConnection(const boost::asio::ip::tcp::endpoint& epRemote, int64 nTimeout,bool fEnableSSL,
    const CIOSSLOption& optSSL)
{
    if(fEnableSSL)
    {
        return Connect(epRemote,nTimeout) ? true : false;
    }
    else
    {
        return SSLConnect(epRemote,nTimeout,optSSL) ? true : false;
    }
}

void CBbDbpClient::SendPingHandler(const boost::system::error_code& err, const CBbSessionProfile& sessionProfile)
{
    if (err != boost::system::errc::success)
    {
        return;
    }

    if(!HaveAssociatedSessionOf(sessionProfile.pClientSocket))
    {
        return;
    }

    std::string utc = std::to_string(CDbpUtils::CurrentUTC());
    sessionProfile.pClientSocket->SendPing(utc);
    
    sessionProfile.ptrPingTimer->expires_at(sessionProfile.ptrPingTimer->expires_at() + boost::posix_time::seconds(3));
    sessionProfile.ptrPingTimer->async_wait(boost::bind(&CBbDbpClient::SendPingHandler,
                                                        this, boost::asio::placeholders::error,
                                                        boost::ref(sessionProfile)));
}
 
void CBbDbpClient::StartPingTimer(const std::string& session)
{
    auto& profile = mapSessionProfile[session];
    
    profile.ptrPingTimer = 
        std::make_shared<boost::asio::deadline_timer>(this->GetIoService(),
                                                      boost::posix_time::seconds(3));
    profile.ptrPingTimer->expires_at(profile.ptrPingTimer->expires_at() +
                                        boost::posix_time::seconds(3));
    profile.ptrPingTimer->async_wait(boost::bind(&CBbDbpClient::SendPingHandler,
                                                    this, boost::asio::placeholders::error,
                                                    boost::ref(profile)));
}

void CBbDbpClient::RegisterDefaultForks(CBbDbpClientSocket* pClientSocket)
{
    std::vector<std::string> vSupportForks = mapProfile[pClientSocket->GetHost().ToEndPoint()].vSupportForks;
    pClientSocket->SendForkIds(vSupportForks);
    
    for(const auto& fork : vSupportForks)
    {
        CBbEventDbpRegisterForkID *pEvent = new CBbEventDbpRegisterForkID("");
        pEvent->data.forkid = fork;
        pDbpService->PostEvent(pEvent);
    }
}

void CBbDbpClient::SubscribeDefaultTopics(CBbDbpClientSocket* pClientSocket)
{
    std::vector<std::string> vTopics{"all-block","all-tx"};
    pClientSocket->SendSubScribeTopics(vTopics);
}

void CBbDbpClient::CreateSession(const std::string& session, CBbDbpClientSocket* pClientSocket)
{
    CBbSessionProfile profile;
    profile.strSessionId = session;
    profile.nTimeStamp = CDbpUtils::CurrentUTC();
    profile.pClientSocket = pClientSocket;

    pClientSocket->SetSession(session);

    mapSessionProfile.insert(std::make_pair(session,profile));
    bimapSessionClientSocket.insert(position_pair(session,pClientSocket));
}

bool CBbDbpClient::HaveAssociatedSessionOf(CBbDbpClientSocket* pClientSocket)
{
    return bimapSessionClientSocket.right.find(pClientSocket) != bimapSessionClientSocket.right.end();
}

bool CBbDbpClient::IsSessionExist(const std::string& session)
{
  return mapSessionProfile.find(session) != mapSessionProfile.end();
}

bool CBbDbpClient::IsForkNode()
{
    if(mapProfile.size() > 0)
    {
        return mapProfile.begin()->second.epParentHost.address().to_string().empty() ? false : true; 
    }
    else
    {
        return false;
    }
}

bool CBbDbpClient::ActivateConnect(CIOClient* pClient)
{
    uint64 nNonce = 0;
    RAND_bytes((unsigned char *)&nNonce, sizeof(nNonce));
    while (mapClientSocket.count(nNonce))
    {
        RAND_bytes((unsigned char *)&nNonce, sizeof(nNonce));
    }
    
    IIOModule* pIOModule = mapProfile[pClient->GetRemote()].pIOModule;
    CBbDbpClientSocket* pDbpClientSocket = new CBbDbpClientSocket(pIOModule,nNonce,this,pClient);
    if(!pDbpClientSocket)
    {
        std::cerr << "Create Client Socket error\n";
        return false;
    }

    mapClientSocket.insert(std::make_pair(nNonce,pDbpClientSocket));
    
    std::vector<std::string> vSupportForks = mapProfile[pClient->GetRemote()].vSupportForks;
    
    pDbpClientSocket->SendConnectSession("",vSupportForks);
    
    return true;
}

void CBbDbpClient::CloseConnect(CBbDbpClientSocket* pClientSocket)
{
    delete pClientSocket;
}

void CBbDbpClient::RemoveSession(CBbDbpClientSocket* pClientSocket)
{
    if (HaveAssociatedSessionOf(pClientSocket))
    {
        std::string assciatedSession = bimapSessionClientSocket.right.at(pClientSocket);
        bimapSessionClientSocket.left.erase(assciatedSession);
        bimapSessionClientSocket.right.erase(pClientSocket);
        
        mapSessionProfile[assciatedSession].ptrPingTimer->cancel();
        mapSessionProfile.erase(assciatedSession);
    }
}

void CBbDbpClient::RemoveClientSocket(CBbDbpClientSocket* pClientSocket)
{
    RemoveSession(pClientSocket);
    CloseConnect(pClientSocket);
}

bool CBbDbpClient::HandleEvent(CBbEventDbpRegisterForkID& event)
{
    if(!event.strSessionId.empty() || event.data.forkid.empty() || !IsForkNode())
    {
        std::cerr << "cannot handle Register fork event." << std::endl;
        return false;
    }

    // pick one session to sendforks
    CBbDbpClientSocket* pClientSocket = NULL;
    if(mapSessionProfile.size() > 0)
    {
        pClientSocket = mapSessionProfile.begin()->second.pClientSocket;
    }
    else
    {
        std::cerr << "mapSessionProfile is empty\n";
        return false;
    }

    if(!pClientSocket)
    {
        std::cerr << "Client Socket is invalid\n";
        return false;
    }

    // TODO


    // std::vector<std::string> forks{event.data.forkid};
    // pClientSocket->SendForkIds(forks);

    return true;
}

bool CBbDbpClient::HandleEvent(CBbEventDbpSendBlock& event)
{
    if(!event.strSessionId.empty() || event.data.block.type() != typeid(CBbDbpBlock)
        || !IsForkNode())
    {
        std::cerr << "cannot handle SendBlock event." << std::endl;
        return false;
    }

    // pick one session to sendblock
    CBbDbpClientSocket* pClientSocket = NULL;
    if(mapSessionProfile.size() > 0)
    {
        pClientSocket = mapSessionProfile.begin()->second.pClientSocket;
    }
    else
    {
        std::cerr << "mapSessionProfile is empty\n";
        return false;
    }

    if(!pClientSocket)
    {
        std::cerr << "Client Socket is invalid\n";
        return false;
    }

    

    // TODO
    
    return true;
}

bool CBbDbpClient::HandleEvent(CBbEventDbpSendTx& event)
{
    if(!event.strSessionId.empty() || event.data.tx.type() != typeid(CBbDbpTransaction)
        || !IsForkNode())
    {
        std::cerr << "cannot handle SendTx event." << std::endl;
        return false;
    }
    
    // pick one session to sendtx
    CBbDbpClientSocket* pClientSocket = NULL;
    if(mapSessionProfile.size() > 0)
    {
        pClientSocket = mapSessionProfile.begin()->second.pClientSocket;
    }
    else
    {
        std::cerr << "mapSessionProfile is empty\n";
        return false;
    }

    if(!pClientSocket)
    {
        std::cerr << "Client Socket is invalid\n";
        return false;
    }

    // TODO
    
    return false;
}

} // namespace bigbang
