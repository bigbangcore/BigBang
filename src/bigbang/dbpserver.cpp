// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dbpserver.h"

#include <memory>
#include <algorithm>
#include <chrono>
#include <openssl/rand.h>
 
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>

using namespace xengine;

/*
namespace bigbang
{

static const std::size_t MSG_HEADER_LEN = 4;

CDbpClient::CDbpClient(CDbpServer* pServerIn, CDbpProfile* pProfileIn,
                       CIOClient* pClientIn, uint64 nonce)
    : pServer(pServerIn), pProfile(pProfileIn), pClient(pClientIn), nNonce(nonce)
    , IsReading(false)

{
    ssRecv.Clear();
}

CDbpClient::~CDbpClient()
{
    if (pClient)
    {
        pClient->Close();
    }
}

CDbpProfile* CDbpClient::GetProfile()
{
    return pProfile;
}

uint64 CDbpClient::GetNonce()
{
    return nNonce;
}

std::string CDbpClient::GetSession() const
{
    return strSessionId;
}

void CDbpClient::SetSession(const std::string& session)
{
    strSessionId = session;
}

void CDbpClient::Activate()
{
   // ssRecv.Clear();
    IsReading = true;
    StartReadHeader();
}

void CDbpClient::SendMessage(dbp::Msg type, google::protobuf::Any* any)
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
    pClient->Write(ssSend, boost::bind(&CDbpClient::HandleWritenResponse, this, _1, type));
}

void CDbpClient::SendResponse(CBbDbpConnected& body)
{
    dbp::Connected connectedMsg;
    connectedMsg.set_session(body.session);

    google::protobuf::Any* any = new google::protobuf::Any();
    any->PackFrom(connectedMsg);

    SendMessage(dbp::Msg::CONNECTED,any);
}

void CDbpClient::SendResponse(CBbDbpFailed& body)
{
    dbp::Failed failedMsg;
    for (const int32& version : body.versions)
    {
        failedMsg.add_version(version);
    }

    failedMsg.set_reason(body.reason);
    failedMsg.set_session(body.session);

    google::protobuf::Any* any = new google::protobuf::Any();
    any->PackFrom(failedMsg);

    SendMessage(dbp::Msg::FAILED,any);
}

void CDbpClient::SendResponse(CBbDbpNoSub& body)
{
    dbp::Nosub noSubMsg;
    noSubMsg.set_id(body.id);
    noSubMsg.set_error(body.error);

    google::protobuf::Any* any = new google::protobuf::Any();
    any->PackFrom(noSubMsg);

    SendMessage(dbp::Msg::NOSUB,any);
}

void CDbpClient::SendResponse(CBbDbpReady& body)
{
    dbp::Ready readyMsg;
    readyMsg.set_id(body.id);

    google::protobuf::Any* any = new google::protobuf::Any();
    any->PackFrom(readyMsg);

    SendMessage(dbp::Msg::READY,any);
}

void CDbpClient::SendResponse(const std::string& client, CBbDbpAdded& body)
{
    dbp::Added addedMsg;
    addedMsg.set_id(body.id);
    addedMsg.set_name(body.name);

    if (body.anyAddedObj.type() == typeid(CBbDbpBlock))
    {
        CBbDbpBlock tempBlock = boost::any_cast<CBbDbpBlock>(body.anyAddedObj);

        if(client == "supernode")
        {
            sn::Block block;
            CDbpUtils::DbpToSnBlock(&tempBlock,block);
            google::protobuf::Any* anyBlock = new google::protobuf::Any();
            anyBlock->PackFrom(block);
            addedMsg.set_allocated_object(anyBlock);
        }
        else
        {
            lws::Block block;
            CDbpUtils::DbpToLwsBlock(&tempBlock, block);
            google::protobuf::Any* anyBlock = new google::protobuf::Any();
            anyBlock->PackFrom(block);
            addedMsg.set_allocated_object(anyBlock);
        }
    }
    else if (body.anyAddedObj.type() == typeid(CBbDbpTransaction))
    {
        CBbDbpTransaction tempTx = boost::any_cast<CBbDbpTransaction>(body.anyAddedObj);

        if(client == "supernode")
        {
            std::unique_ptr<sn::Transaction> tx(new sn::Transaction());
            CDbpUtils::DbpToSnTransaction(&tempTx, tx.get());
            google::protobuf::Any* anyTx = new google::protobuf::Any();
            anyTx->PackFrom(*tx);
            addedMsg.set_allocated_object(anyTx);
        }
        else
        {
            std::unique_ptr<lws::Transaction> tx(new lws::Transaction());
            CDbpUtils::DbpToLwsTransaction(&tempTx, tx.get());
            google::protobuf::Any* anyTx = new google::protobuf::Any();
            anyTx->PackFrom(*tx);
            addedMsg.set_allocated_object(anyTx);
        }
    }
    else
    {
    }

    google::protobuf::Any* anyAdded = new google::protobuf::Any();
    anyAdded->PackFrom(addedMsg);

    SendMessage(dbp::Msg::ADDED,anyAdded);
}

void CDbpClient::SendResponse(const std::string& client, CBbDbpMethodResult& body)
{
    dbp::Result resultMsg;
    resultMsg.set_id(body.id);
    resultMsg.set_error(body.error);

    auto dispatchHandler = [&](const boost::any& obj) -> void {
        if (obj.type() == typeid(CBbDbpBlock))
        {
            CBbDbpBlock tempBlock = boost::any_cast<CBbDbpBlock>(obj);
            if(client == "supernode")
            {

            }
            else
            {
                lws::Block block;
                CDbpUtils::DbpToLwsBlock(&tempBlock, block);
                resultMsg.add_result()->PackFrom(block);
            }
        }
        else if (obj.type() == typeid(CBbDbpTransaction))
        {
            CBbDbpTransaction tempTx = boost::any_cast<CBbDbpTransaction>(obj);
            if(client == "supernode")
            {

            }
            else
            {
                std::unique_ptr<lws::Transaction> tx(new lws::Transaction());
                CDbpUtils::DbpToLwsTransaction(&tempTx, tx.get());
                resultMsg.add_result()->PackFrom(*tx);
            }    
        }
        else if (obj.type() == typeid(CBbDbpSendTransactionRet))
        {
            CBbDbpSendTransactionRet txret = boost::any_cast<CBbDbpSendTransactionRet>(obj);
            
            lws::SendTxRet sendTxRet;
            sendTxRet.set_hash(txret.hash);
            sendTxRet.set_result(txret.result);
            sendTxRet.set_reason(txret.reason);
            resultMsg.add_result()->PackFrom(sendTxRet);
          
        }
        else if(obj.type() == typeid(CBbDbpSendBlockRet))
        {
            CBbDbpSendBlockRet blockRet = boost::any_cast<CBbDbpSendBlockRet>(obj);

            sn::SendBlockRet sendBlockRet;
            sendBlockRet.set_hash(blockRet.hash);
            resultMsg.add_result()->PackFrom(sendBlockRet);
        }
        else if(obj.type() == typeid(CBbDbpSendTxRet))
        {
            CBbDbpSendTxRet txRet = boost::any_cast<CBbDbpSendTxRet>(obj);

            sn::SendTxRet sendTxRet;
            sendTxRet.set_hash(txRet.hash);
            resultMsg.add_result()->PackFrom(sendTxRet);
        }
        else if(obj.type() == typeid(CBbDbpRegisterForkIDRet))
        {
            CBbDbpRegisterForkIDRet ret = boost::any_cast<CBbDbpRegisterForkIDRet>(obj);
            sn::RegisterForkIDRet forkRet;
            forkRet.set_id(ret.forkid);
            resultMsg.add_result()->PackFrom(forkRet);
        }
        else
        {
        }
    };

    std::for_each(body.anyResultObjs.begin(), body.anyResultObjs.end(), dispatchHandler);

    google::protobuf::Any* anyResult = new google::protobuf::Any();
    anyResult->PackFrom(resultMsg);

    SendMessage(dbp::Msg::RESULT,anyResult);
}

void CDbpClient::SendPong(const std::string& id)
{
    dbp::Pong msg;
    msg.set_id(id);

    google::protobuf::Any *any = new google::protobuf::Any();
    any->PackFrom(msg);

    SendMessage(dbp::Msg::PONG,any);
}

void CDbpClient::SendPing(const std::string& id)
{
    dbp::Ping msg;
    msg.set_id(id);

    google::protobuf::Any* any = new google::protobuf::Any();
    any->PackFrom(msg);

    SendMessage(dbp::Msg::PING,any);
}

void CDbpClient::SendResponse(const std::string& reason, const std::string& description)
{
    dbp::Error errorMsg;
    errorMsg.set_reason(reason);
    errorMsg.set_explain(description);

    google::protobuf::Any* any = new google::protobuf::Any();
    any->PackFrom(errorMsg);

    SendMessage(dbp::Msg::ERROR,any);
}

void CDbpClient::StartReadHeader()
{
    pClient->Read(ssRecv, MSG_HEADER_LEN,
                  boost::bind(&CDbpClient::HandleReadHeader, this, _1));
}

void CDbpClient::StartReadPayload(std::size_t nLength)
{
    pClient->Read(ssRecv, nLength,
                  boost::bind(&CDbpClient::HandleReadPayload, this, _1, nLength));
}

bool CDbpClient::IsSentComplete()
{
    return (ssSend.GetSize() == 0 && queueMessage.empty());
}

void CDbpClient::HandleReadHeader(std::size_t nTransferred)
{
    if (nTransferred == MSG_HEADER_LEN)
    {
        std::string lenBuffer(ssRecv.GetData(), ssRecv.GetData() + MSG_HEADER_LEN);
        uint32_t nMsgHeaderLen = CDbpUtils::ParseLenFromMsgHeader(&lenBuffer[0], MSG_HEADER_LEN);
        if (nMsgHeaderLen == 0)
        {
            std::cerr << "Msg Base header length is 0" << std::endl;
            pServer->HandleClientError(this);
            return;
        }

        StartReadPayload(nMsgHeaderLen);
    }
    else
    {
        std::cerr << "Msg Base header length is not 4 " << std::endl;
        pServer->HandleClientError(this);
    }
}

void CDbpClient::HandleReadPayload(std::size_t nTransferred, uint32_t len)
{
    if (nTransferred == len)
    {
        HandleReadCompleted(len);
    }
    else
    {
        std::cerr << "pay load is not len. " << std::endl;
        pServer->HandleClientError(this);
    }
}

void CDbpClient::HandleReadCompleted(uint32_t len)
{
    char head[4];
    ssRecv.Read(head,4);
    std::string payloadBuffer(len, 0);
    ssRecv.Read(&payloadBuffer[0], len);
    IsReading = false;
    dbp::Base msgBase;
    if (!msgBase.ParseFromString(payloadBuffer))
    {
        std::cerr << "#######parse payload failed. " << std::endl;
        pServer->RespondError(this, "002", "server recv invalid protobuf object");
        pServer->HandleClientError(this);
        return;
    }

    dbp::Msg currentMsgType = msgBase.msg();
    google::protobuf::Any anyObj = msgBase.object();
    switch (currentMsgType)
    {
    case dbp::CONNECT:
        pServer->HandleClientRecv(this, anyObj);
        break;
    case dbp::SUB:
        pServer->HandleClientRecv(this, anyObj);
        break;
    case dbp::UNSUB:
        pServer->HandleClientRecv(this, anyObj);
        break;
    case dbp::METHOD:
        pServer->HandleClientRecv(this, anyObj);
        break;
    case dbp::PONG:
        pServer->HandleClientRecv(this, anyObj);
        pServer->HandleClientSent(this);
        break;
    case dbp::PING:
        pServer->HandleClientRecv(this, anyObj);
        break;
    default:
        std::cerr << "is not Message Base Type is unknown." << std::endl;
        pServer->RespondError(this, "003", "is not Message Base Type is unknown.");
        pServer->HandleClientError(this);
        break;
    }
}

void CDbpClient::HandleWritenResponse(std::size_t nTransferred, dbp::Msg type)
{
    if (nTransferred != 0)
    {
        if (ssSend.GetSize() != 0)
        {
            pClient->Write(ssSend, boost::bind(&CDbpClient::HandleWritenResponse,
                                               this, _1, type));
            return;
        }

        if (ssSend.GetSize() == 0 && !queueMessage.empty())
        {
            
            auto messagePair = queueMessage.front();
            dbp::Msg messageType = messagePair.first;
            std::string bytes = messagePair.second;
            queueMessage.pop();
            ssSend.Write((char*)bytes.data(),bytes.size());
            pClient->Write(ssSend, boost::bind(&CDbpClient::HandleWritenResponse, this, _1, messageType));
            return;
        }

        if(!IsReading)
        {
            pServer->HandleClientSent(this);
        }
       
    }
    else
    {
        pServer->HandleClientError(this);
    }
}

CDbpServer::CDbpServer()
    : CIOProc("dbpserver")
{
}

CDbpServer::~CDbpServer() noexcept
{
}

CIOClient* CDbpServer::CreateIOClient(CIOContainer* pContainer)
{
    std::map<boost::asio::ip::tcp::endpoint, CDbpProfile>::iterator it;
    it = mapProfile.find(pContainer->GetServiceEndpoint());
    if (it != mapProfile.end() && (*it).second.pSSLContext != NULL)
    {
        return new CSSLClient(pContainer, GetIoService(), *(*it).second.pSSLContext);
    }
    return CIOProc::CreateIOClient(pContainer);
}

void CDbpServer::HandleClientConnect(CDbpClient* pDbpClient, google::protobuf::Any* any)
{

    dbp::Connect connectMsg;
    any->UnpackTo(&connectMsg);

    std::string session = connectMsg.session();

    if (!IsSessionReconnect(session))
    {
        session = GenerateSessionId();
        std::string forkid = GetUdata(&connectMsg, "forkid");
        std::string client = connectMsg.client();
        CreateSession(session, client, forkid, pDbpClient);

        std::string childNodeforks = GetUdata(&connectMsg,"supernode-forks");
        
        CBbEventDbpConnect *pEventDbpConnect = new CBbEventDbpConnect(session);
        if (!pEventDbpConnect)
        {
            return;
        }

        CBbDbpConnect& connectBody = pEventDbpConnect->data;
        connectBody.isReconnect = false;
        connectBody.session = session;
        connectBody.forks = childNodeforks;
        connectBody.version = connectMsg.version();
        connectBody.client = connectMsg.client();

        pDbpClient->GetProfile()->pIOModule->PostEvent(pEventDbpConnect);
    }
    else
    {
        if (IsSessionExist(session))
        {
            UpdateSession(session, pDbpClient);

            CBbEventDbpConnect *pEventDbpConnect = new CBbEventDbpConnect(session);
            if (!pEventDbpConnect)
            {
                return;
            }

            CBbDbpConnect& connectBody = pEventDbpConnect->data;
            connectBody.isReconnect = true;
            connectBody.session = session;
            connectBody.version = connectMsg.version();
            connectBody.client = connectMsg.client();

            pDbpClient->GetProfile()->pIOModule->PostEvent(pEventDbpConnect);
        }
        else
        {
            RespondFailed(pDbpClient, "002");
        }
    }
}

void CDbpServer::HandleClientSub(CDbpClient* pDbpClient, google::protobuf::Any* any)
{
    CBbEventDbpSub* pEventDbpSub = new CBbEventDbpSub(pDbpClient->GetSession());
    if (!pEventDbpSub)
    {
        return;
    }

    dbp::Sub subMsg;
    any->UnpackTo(&subMsg);

    CBbDbpSub& subBody = pEventDbpSub->data;
    subBody.id = subMsg.id();
    subBody.name = subMsg.name();

    pDbpClient->GetProfile()->pIOModule->PostEvent(pEventDbpSub);
}

void CDbpServer::HandleClientUnSub(CDbpClient* pDbpClient, google::protobuf::Any* any)
{
    CBbEventDbpUnSub* pEventDbpUnSub = new CBbEventDbpUnSub(pDbpClient->GetSession());
    if (!pEventDbpUnSub)
    {
        return;
    }

    dbp::Unsub unsubMsg;
    any->UnpackTo(&unsubMsg);

    CBbDbpUnSub& unsubBody = pEventDbpUnSub->data;
    unsubBody.id = unsubMsg.id();

    pDbpClient->GetProfile()->pIOModule->PostEvent(pEventDbpUnSub);
}

void CDbpServer::HandleClientMethod(CDbpClient* pDbpClient, google::protobuf::Any* any)
{
    CBbEventDbpMethod* pEventDbpMethod = new CBbEventDbpMethod(pDbpClient->GetSession());
    if (!pEventDbpMethod)
    {
        return;
    }

    dbp::Method methodMsg;
    any->UnpackTo(&methodMsg);

    CBbDbpMethod& methodBody = pEventDbpMethod->data;
    methodBody.id = methodMsg.id();

    if (methodMsg.method() == "getblocks")
        methodBody.method = CBbDbpMethod::Method::GET_BLOCKS;
    if (methodMsg.method() == "gettransaction")
        methodBody.method = CBbDbpMethod::Method::GET_TRANSACTION;
    if (methodMsg.method() == "sendtransaction")
        methodBody.method = CBbDbpMethod::Method::SEND_TRANSACTION;
    if (methodMsg.method() == "registerforkid")
        methodBody.method = CBbDbpMethod::Method::REGISTER_FORK;
    if (methodMsg.method() == "sendblock")
        methodBody.method = CBbDbpMethod::Method::SEND_BLOCK;
    if (methodMsg.method() == "sendtx")
        methodBody.method = CBbDbpMethod::Method::SEND_TX;


    if (methodBody.method == CBbDbpMethod::Method::GET_BLOCKS && methodMsg.params().Is<lws::GetBlocksArg>())
    {
        lws::GetBlocksArg args;
        methodMsg.params().UnpackTo(&args);

        std::string forkid;
        GetSessionForkId(pDbpClient, forkid);

        methodBody.params.insert(std::make_pair("forkid", forkid));
        methodBody.params.insert(std::make_pair("hash", args.hash()));
        methodBody.params.insert(std::make_pair("number", boost::lexical_cast<std::string>(args.number())));
    }
    else if (methodBody.method == CBbDbpMethod::Method::GET_TRANSACTION && methodMsg.params().Is<lws::GetTxArg>())
    {
        lws::GetTxArg args;
        methodMsg.params().UnpackTo(&args);
        methodBody.params.insert(std::make_pair("hash", args.hash()));
    }
    else if (methodBody.method == CBbDbpMethod::Method::SEND_TRANSACTION && methodMsg.params().Is<lws::SendTxArg>())
    {
        lws::SendTxArg args;
        methodMsg.params().UnpackTo(&args);
        methodBody.params.insert(std::make_pair("data", args.data()));
    }
    else if(methodBody.method == CBbDbpMethod::Method::REGISTER_FORK)
    {
        sn::RegisterForkIDArg args;
        methodMsg.params().UnpackTo(&args);
        
        std::string session = pDbpClient->GetSession();
        mapSessionProfile[session].setChildForks.insert(args.id());
        
        methodBody.params.insert(std::make_pair("forkid",args.id()));
    }
    else if(methodBody.method == CBbDbpMethod::Method::SEND_BLOCK)
    {
        sn::SendBlockArg args;
        methodMsg.params().UnpackTo(&args);

        CBbDbpBlock dbpBlock;
        CDbpUtils::SnToDbpBlock(&(args.block()),dbpBlock);

        methodBody.params.insert(std::make_pair("data", dbpBlock));
    }
    else if(methodBody.method == CBbDbpMethod::Method::SEND_TX)
    {
        sn::SendTxArg args;
        methodMsg.params().UnpackTo(&args);

        CBbDbpTransaction dbptx;
        CDbpUtils::SnToDbpTransaction(&(args.tx()),&dbptx);

        methodBody.params.insert(std::make_pair("data", dbptx));
    }
    else
    {
        delete pEventDbpMethod;
        return;
    }

    pDbpClient->GetProfile()->pIOModule->PostEvent(pEventDbpMethod);
}

void CDbpServer::HandleClientPing(CDbpClient* pDbpClient, google::protobuf::Any* any)
{
    dbp::Ping pingMsg;
    any->UnpackTo(&pingMsg);
    pDbpClient->SendPong(pingMsg.id());
}

void CDbpServer::HandleClientPong(CDbpClient* pDbpClient, google::protobuf::Any* any)
{
    dbp::Pong pongMsg;
    any->UnpackTo(&pongMsg);
    std::string session = bimapSessionClient.right.at(pDbpClient);
    if (IsSessionExist(session))
    {
        mapSessionProfile[session].nTimeStamp = CDbpUtils::CurrentUTC();
    }
}

void CDbpServer::HandleClientRecv(CDbpClient* pDbpClient, const boost::any& anyObj)
{
    if (anyObj.type() != typeid(google::protobuf::Any))
    {
        return;
    }

    google::protobuf::Any any = boost::any_cast<google::protobuf::Any>(anyObj);

    if (any.Is<dbp::Connect>())
    {
        HandleClientConnect(pDbpClient, &any);
    }
    else if (any.Is<dbp::Sub>())
    {
        if (!HaveAssociatedSessionOf(pDbpClient))
        {
            RespondError(pDbpClient, "001", "need to connect first. ");
            return;
        }

        if (IsSessionTimeOut(pDbpClient))
        {
            RespondFailed(pDbpClient, "002");
            return;
        }

        HandleClientSub(pDbpClient, &any);
    }
    else if (any.Is<dbp::Unsub>())
    {
        if (!HaveAssociatedSessionOf(pDbpClient))
        {
            RespondError(pDbpClient, "001", "need to connect first. ");
            return;
        }

        if (IsSessionTimeOut(pDbpClient))
        {
            RespondFailed(pDbpClient, "002");
            return;
        }

        HandleClientUnSub(pDbpClient, &any);
    }
    else if (any.Is<dbp::Method>())
    {
        if (!HaveAssociatedSessionOf(pDbpClient))
        {
            RespondError(pDbpClient, "001", "need to connect first. ");
            return;
        }

        if (IsSessionTimeOut(pDbpClient))
        {
            RespondFailed(pDbpClient, "002");
            return;
        }

        HandleClientMethod(pDbpClient, &any);
    }
    else if (any.Is<dbp::Ping>())
    {
        if (!HaveAssociatedSessionOf(pDbpClient))
        {
            RespondError(pDbpClient, "001", "need to connect first. ");
            return;
        }

        if (IsSessionTimeOut(pDbpClient))
        {
            RespondFailed(pDbpClient, "002");
            return;
        }

        HandleClientPing(pDbpClient, &any);
    }
    else if (any.Is<dbp::Pong>())
    {
        if (!HaveAssociatedSessionOf(pDbpClient))
        {
            RespondError(pDbpClient, "001", "need to connect first. ");
            return;
        }

        if (IsSessionTimeOut(pDbpClient))
        {
            RespondFailed(pDbpClient, "002");
            return;
        }

        HandleClientPong(pDbpClient, &any);
    }
    else
    {
        RespondError(pDbpClient, "003", "unknown dbp message");
        return;
    }
}

void CDbpServer::HandleClientSent(CDbpClient* pDbpClient)
{
    // keep-alive connection,do not remove
    pDbpClient->Activate();
}

void CDbpServer::HandleClientError(CDbpClient* pDbpClient)
{
    std::cerr << "Client Error. " << std::endl;
    
    CBbEventDbpBroken *pEventDbpBroken = new CBbEventDbpBroken(pDbpClient->GetSession());
    if(pEventDbpBroken)
    {
        pDbpClient->GetProfile()->pIOModule->PostEvent(pEventDbpBroken);
    }

    RemoveClient(pDbpClient);
}

void CDbpServer::AddNewHost(const CDbpHostConfig& confHost)
{
    vecHostConfig.push_back(confHost);
}

bool CDbpServer::BlockheadHandleInitialize()
{
    for (const CDbpHostConfig& confHost : vecHostConfig)
    {
        if (!CreateProfile(confHost))
        {
            return false;
        }
    }
    return true;
}

void CDbpServer::BlockheadHandleDeinitialize()
{
    for (std::map<boost::asio::ip::tcp::endpoint, CDbpProfile>::iterator it = mapProfile.begin();
         it != mapProfile.end(); ++it)
    {
        delete (*it).second.pSSLContext;
    }
    mapProfile.clear();
}

void CDbpServer::EnterLoop()
{
    BlockheadLog("Dbp Server starting:\n");

    for (std::map<boost::asio::ip::tcp::endpoint, CDbpProfile>::iterator it = mapProfile.begin();
         it != mapProfile.end(); ++it)
    {
        if (!StartService((*it).first, (*it).second.nMaxConnections, (*it).second.vAllowMask))
        {
            BlockheadError("Setup service %s failed, listen port = %d, connection limit %d\n",
                       (*it).second.pIOModule->BlockheadGetOwnKey().c_str(),
                       (*it).first.port(), (*it).second.nMaxConnections);
        }
        else
        {
            BlockheadLog("Setup service %s success, listen port = %d, connection limit %d\n",
                       (*it).second.pIOModule->BlockheadGetOwnKey().c_str(),
                       (*it).first.port(), (*it).second.nMaxConnections);
        }
    }
}

void CDbpServer::LeaveLoop()
{
    std::vector<CDbpClient*> vClient;
    for (std::map<uint64, CDbpClient*>::iterator it = mapClient.begin(); it != mapClient.end(); ++it)
    {
        vClient.push_back((*it).second);
    }

    for (CDbpClient *pClient : vClient)
    {
        RemoveClient(pClient);
    }
    BlockheadLog("Dbp Server stop\n");
}

bool CDbpServer::ClientAccepted(const boost::asio::ip::tcp::endpoint& epService, CIOClient* pClient)
{
    std::map<boost::asio::ip::tcp::endpoint, CDbpProfile>::iterator it = mapProfile.find(epService);
    if (it == mapProfile.end())
    {
        return false;
    }
    return AddNewClient(pClient, &(*it).second) != NULL;
}

bool CDbpServer::CreateProfile(const CDbpHostConfig& confHost)
{
    CDbpProfile profile;
    if (!BlockheadGetObject(confHost.strIOModule, profile.pIOModule))
    {
        BlockheadError("Failed to request %s\n", confHost.strIOModule.c_str());
        return false;
    }

    if (confHost.optSSL.fEnable)
    {
        profile.pSSLContext = new boost::asio::ssl::context(boost::asio::ssl::context::sslv23);
        if (!profile.pSSLContext)
        {
            BlockheadError("Failed to alloc ssl context for %s:%u\n",
                       confHost.epHost.address().to_string().c_str(),
                       confHost.epHost.port());
            return false;
        }
        if (!confHost.optSSL.SetupSSLContext(*profile.pSSLContext))
        {
            BlockheadError("Failed to setup ssl context for %s:%u\n",
                       confHost.epHost.address().to_string().c_str(),
                       confHost.epHost.port());
            delete profile.pSSLContext;
            return false;
        }
    }

    profile.nMaxConnections = confHost.nMaxConnections;
    profile.nSessionTimeout = confHost.nSessionTimeout;
    profile.vAllowMask = confHost.vAllowMask;
    mapProfile[confHost.epHost] = profile;

    return true;
}

CDbpClient *CDbpServer::AddNewClient(CIOClient* pClient, CDbpProfile* pDbpProfile)
{
    uint64 nNonce = 0;
    RAND_bytes((unsigned char *)&nNonce, sizeof(nNonce));
    while (mapClient.count(nNonce))
    {
        RAND_bytes((unsigned char *)&nNonce, sizeof(nNonce));
    }

    CDbpClient* pDbpClient = new CDbpClient(this, pDbpProfile, pClient, nNonce);
    if (pDbpClient)
    {
        mapClient.insert(std::make_pair(nNonce, pDbpClient));
        pDbpClient->Activate();
    }

    return pDbpClient;
}

void CDbpServer::RemoveSession(CDbpClient* pDbpClient)
{
    mapClient.erase(pDbpClient->GetNonce());

    if (HaveAssociatedSessionOf(pDbpClient))
    {
        std::string assciatedSession = bimapSessionClient.right.at(pDbpClient);
        bimapSessionClient.left.erase(assciatedSession);
        bimapSessionClient.right.erase(pDbpClient);
        mapSessionProfile[assciatedSession].ptrPingTimer->cancel();
        mapSessionProfile.erase(assciatedSession);

        CBbEventDbpRemoveSession* pEvent = new CBbEventDbpRemoveSession("");
        pEvent->data.session = assciatedSession;
        pDbpClient->GetProfile()->pIOModule->PostEvent(pEvent);
    }
}

void CDbpServer::RemoveClient(CDbpClient* pDbpClient)
{
    RemoveSession(pDbpClient);
    delete pDbpClient;
}

void CDbpServer::RespondError(CDbpClient* pDbpClient, const std::string& reason, const std::string& strError)
{
    pDbpClient->SendResponse(reason, strError);
}

void CDbpServer::RespondFailed(CDbpClient* pDbpClient, const std::string& reason)
{
    CBbEventDbpFailed failedEvent(pDbpClient->GetNonce());

    std::string session =
        HaveAssociatedSessionOf(pDbpClient) ? bimapSessionClient.right.at(pDbpClient) : "";
    failedEvent.data.session = session;
    failedEvent.data.reason = reason;
    this->HandleEvent(failedEvent);
}

void CDbpServer::SendPingHandler(const boost::system::error_code& err, const CSessionProfile& sessionProfile)
{
    if (err != boost::system::errc::success)
    {
        return;
    }

    std::string utc = std::to_string(CDbpUtils::CurrentUTC());

    if(IsSessionTimeOut(sessionProfile.pDbpClient))
    {
        std::cerr << "######### session time out ############\n";
        HandleClientError(sessionProfile.pDbpClient);
        return;
    }

    sessionProfile.pDbpClient->SendPing(utc);

    sessionProfile.ptrPingTimer->expires_at(sessionProfile.ptrPingTimer->expires_at() + boost::posix_time::seconds(3));
    sessionProfile.ptrPingTimer->async_wait(boost::bind(&CDbpServer::SendPingHandler,
                                                        this, boost::asio::placeholders::error,
                                                        boost::ref(sessionProfile)));
}

bool CDbpServer::HandleEvent(CBbEventDbpConnected& event)
{
    auto it = mapSessionProfile.find(event.strSessionId);
    if (it == mapSessionProfile.end())
    {
        std::cerr << "cannot find session [Connected] " << event.strSessionId << std::endl;
        return false;
    }

    CDbpClient* pDbpClient = (*it).second.pDbpClient;
    CBbDbpConnected& connectedBody = event.data;

    pDbpClient->SendResponse(connectedBody);

    it->second.ptrPingTimer =
        std::make_shared<boost::asio::deadline_timer>(this->GetIoService(),
                                                      boost::posix_time::seconds(3));

    it->second.ptrPingTimer->expires_at(it->second.ptrPingTimer->expires_at() +
                                       boost::posix_time::seconds(3));
    it->second.ptrPingTimer->async_wait(boost::bind(&CDbpServer::SendPingHandler,
                                                    this, boost::asio::placeholders::error,
                                                    boost::ref(it->second)));

    return true;
}

bool CDbpServer::HandleEvent(CBbEventDbpFailed& event)
{
    std::map<uint64, CDbpClient *>::iterator it = mapClient.find(event.nNonce);
    if (it == mapClient.end())
    {
        std::cerr << "cannot find nonce [failed]" << std::endl;
        return false;
    }

    CDbpClient* pDbpClient = (*it).second;
    CBbDbpFailed& failedBody = event.data;

    pDbpClient->SendResponse(failedBody);

    HandleClientError(pDbpClient);

    return true;
}

bool CDbpServer::HandleEvent(CBbEventDbpNoSub& event)
{
    auto it = mapSessionProfile.find(event.strSessionId);
    if (it == mapSessionProfile.end())
    {
        std::cerr << "cannot find session [NoSub] " << event.strSessionId << std::endl;
        return false;
    }

    CDbpClient* pDbpClient = (*it).second.pDbpClient;
    CBbDbpNoSub& noSubBody = event.data;

    pDbpClient->SendResponse(noSubBody);

    return true;
}

bool CDbpServer::HandleEvent(CBbEventDbpReady& event)
{
    auto it = mapSessionProfile.find(event.strSessionId);
    if (it == mapSessionProfile.end())
    {
        std::cerr << "cannot find session [Ready] " << event.strSessionId << std::endl;
        return false;
    }

    CDbpClient* pDbpClient = (*it).second.pDbpClient;
    CBbDbpReady& readyBody = event.data;

    pDbpClient->SendResponse(readyBody);

    return true;
}

bool CDbpServer::HandleEvent(CBbEventDbpAdded& event)
{
    auto it = mapSessionProfile.find(event.strSessionId);
    if (it == mapSessionProfile.end())
    {
        std::cerr << "cannot find session [Added] " << event.strSessionId << std::endl;
        return false;
    }

    if(it->second.strClient != "supernode" && it->second.strForkId == event.data.forkid)
    {
        CDbpClient* pDbpClient = (*it).second.pDbpClient;
        CBbDbpAdded& addedBody = event.data;

        pDbpClient->SendResponse(it->second.strClient,addedBody);
    }

    if(it->second.strClient == "supernode")
    {
        CDbpClient* pDbpClient = (*it).second.pDbpClient;
        CBbDbpAdded& addedBody = event.data;

        pDbpClient->SendResponse(it->second.strClient,addedBody);
    }
    
    return true;
}

bool CDbpServer::HandleEvent(CBbEventDbpMethodResult& event)
{
    auto it = mapSessionProfile.find(event.strSessionId);
    if (it == mapSessionProfile.end())
    {
        std::cerr << "cannot find session [Method Result] " << event.strSessionId << std::endl;
        return false;
    }

    CDbpClient* pDbpClient = (*it).second.pDbpClient;
    CBbDbpMethodResult& resultBody = event.data;

    pDbpClient->SendResponse(it->second.strClient, resultBody);

    return true;
}

bool CDbpServer::IsSessionTimeOut(CDbpClient* pDbpClient)
{
    if (HaveAssociatedSessionOf(pDbpClient))
    {
        auto timeout = pDbpClient->GetProfile()->nSessionTimeout;
        std::string assciatedSession = bimapSessionClient.right.at(pDbpClient);
        uint64 lastTimeStamp = mapSessionProfile[assciatedSession].nTimeStamp;
        return (CDbpUtils::CurrentUTC() - lastTimeStamp > timeout) ? true : false;
    }
    else
    {
        return false;
    }
}

bool CDbpServer::GetSessionForkId(CDbpClient* pDbpClient, std::string& forkid)
{
    if (HaveAssociatedSessionOf(pDbpClient))
    {
        std::string assciatedSession = bimapSessionClient.right.at(pDbpClient);
        forkid = mapSessionProfile[assciatedSession].strForkId;
        return true;
    }
    else
    {
        return false;
    }
}

bool CDbpServer::IsSessionReconnect(const std::string& session)
{
    return !session.empty();
}

bool CDbpServer::IsSessionExist(const std::string& session)
{
    return mapSessionProfile.find(session) != mapSessionProfile.end();
}

bool CDbpServer::HaveAssociatedSessionOf(CDbpClient* pDbpClient)
{
    return bimapSessionClient.right.find(pDbpClient) != bimapSessionClient.right.end();
}

std::string CDbpServer::GetUdata(dbp::Connect* pConnect, const std::string& keyName)
{
    auto customParamsMap = pConnect->udata();
    google::protobuf::Any paramAny = customParamsMap[keyName];
    lws::ForkID forkidArg;

    if (!paramAny.Is<lws::ForkID>())
    {
        return std::string();
    }

    if (keyName == "forkid")
    {
        paramAny.UnpackTo(&forkidArg);
        if (forkidArg.ids_size() != 0)
            return forkidArg.ids(0);
        else
            return std::string();
       
    }

    if(keyName == "supernode-forks")
    {
        std::string forks;
        paramAny.UnpackTo(&forkidArg);
        for(int i = 0; i < forkidArg.ids_size(); ++i)
        {
            forks.append(forkidArg.ids(i));
            forks.append(";");
        }
        return std::string(forks.begin(),forks.end() - 1);    
    }

    return std::string();
}

std::string CDbpServer::GenerateSessionId()
{
    std::string session = CDbpUtils::RandomString();
    while (IsSessionExist(session))
    {
        session = CDbpUtils::RandomString();
    }

    return session;
}

void CDbpServer::CreateSession(const std::string& session, const std::string& client, const std::string& forkID, CDbpClient* pDbpClient)
{
    CSessionProfile profile;
    profile.strSessionId = session;
    profile.strClient = client;
    profile.strForkId = forkID;
    profile.pDbpClient = pDbpClient;
    profile.nTimeStamp = CDbpUtils::CurrentUTC();

    pDbpClient->SetSession(session);

    mapSessionProfile.insert(std::make_pair(session, profile));
    bimapSessionClient.insert(position_pair(session, pDbpClient));
}

void CDbpServer::UpdateSession(const std::string& session, CDbpClient* pDbpClient)
{
    if (bimapSessionClient.left.find(session) != bimapSessionClient.left.end())
    {
        auto pDbplient = bimapSessionClient.left.at(session);
        bimapSessionClient.left.erase(session);
        bimapSessionClient.right.erase(pDbpClient);
    }

    mapSessionProfile[session].pDbpClient = pDbpClient;
    mapSessionProfile[session].nTimeStamp = CDbpUtils::CurrentUTC();
    bimapSessionClient.insert(position_pair(session, pDbpClient));
}

} //namespace bigbang
*/