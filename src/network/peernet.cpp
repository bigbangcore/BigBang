// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "peernet.h"

#include <boost/any.hpp>
#include <boost/bind.hpp>

#include "message.h"
#include "peer.h"

#define HANDSHAKE_TIMEOUT (5)
#define RESPONSE_TX_TIMEOUT (15)
#define RESPONSE_BLOCK_TIMEOUT (120)
#define RESPONSE_DISTRIBUTE_TIMEOUT (5)
#define RESPONSE_PUBLISH_TIMEOUT (10)
#define NODE_ACTIVE_TIME (3 * 60 * 60)

#define NODE_DEFAULT_GATEWAY "0.0.0.0"

using namespace std;
using namespace xengine;
using boost::asio::ip::tcp;

namespace bigbang
{
namespace network
{

//////////////////////////////
// CBbPeerNet

CBbPeerNet::CBbPeerNet()
  : CPeerNet("peernet")
{
    nMagicNum = 0;
    nVersion = 0;
    nService = 0;
    fEnclosed = false;
    pNetChannel = nullptr;
}

CBbPeerNet::~CBbPeerNet()
{
}

bool CBbPeerNet::HandleInitialize()
{
    if (!GetObject("netchannel", pNetChannel))
    {
        Error("Failed to request peer net datachannel\n");
        return false;
    }

    RegisterHandler<CPeerSubscribeMessageOutBound>(boost::bind(&CBbPeerNet::HandleSubscribe, this, _1));
    RegisterHandler<CPeerUnsubscribeMessageOutBound>(boost::bind(&CBbPeerNet::HandleUnsubscribe, this, _1));
    RegisterHandler<CPeerInvMessageOutBound>(boost::bind(&CBbPeerNet::HandleInv, this, _1));
    RegisterHandler<CPeerGetDataMessageOutBound>(boost::bind(&CBbPeerNet::HandleGetData, this, _1));
    RegisterHandler<CPeerGetBlocksMessageOutBound>(boost::bind(&CBbPeerNet::HandleGetBlocks, this, _1));
    RegisterHandler<CPeerTxMessageOutBound>(boost::bind(&CBbPeerNet::HandlePeerTx, this, _1));
    RegisterHandler<CPeerBlockMessageOutBound>(boost::bind(&CBbPeerNet::HandlePeerBlock, this, _1));

    return true;
}

void CBbPeerNet::HandleDeinitialize()
{
    setDNSeed.clear();
    pNetChannel = nullptr;
}

void CBbPeerNet::HandleSubscribe(const CPeerSubscribeMessageOutBound& subscribeMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CBufStream ssPayload;
    CEventPeerSubscribe eventSubscribe(subscribeMsg.nNonce, subscribeMsg.hashFork);
    eventSubscribe.data = subscribeMsg.vecForks;
    ssPayload << eventSubscribe;
    SendDataMessage(eventSubscribe.nNonce, PROTO_CMD_SUBSCRIBE, ssPayload);
}

void CBbPeerNet::HandleUnsubscribe(const CPeerUnsubscribeMessageOutBound& unsubscribeMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CBufStream ssPayload;
    CEventPeerUnsubscribe eventUnsubscribe(unsubscribeMsg.nNonce, unsubscribeMsg.hashFork);
    eventUnsubscribe.data = unsubscribeMsg.vecForks;
    ssPayload << eventUnsubscribe;
    SendDataMessage(eventUnsubscribe.nNonce, PROTO_CMD_UNSUBSCRIBE, ssPayload);
}

void CBbPeerNet::HandleInv(const CPeerInvMessageOutBound& invMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CBufStream ssPayload;
    CEventPeerInv eventInv(invMsg.nNonce, invMsg.hashFork);
    eventInv.data = invMsg.vecInv;
    ssPayload << eventInv;
    SendDataMessage(eventInv.nNonce, PROTO_CMD_INV, ssPayload);
}

void CBbPeerNet::HandleGetData(const CPeerGetDataMessageOutBound& getDataMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CBufStream ssPayload;
    CEventPeerGetData eventGetData(getDataMsg.nNonce, getDataMsg.hashFork);
    eventGetData.data = getDataMsg.vecInv;
    ssPayload << eventGetData;
    if (!SendDataMessage(eventGetData.nNonce, PROTO_CMD_GETDATA, ssPayload))
    {
        return;
    }

    SetInvTimer(eventGetData.nNonce, eventGetData.data);
}

void CBbPeerNet::HandleGetBlocks(const CPeerGetBlocksMessageOutBound& getBlocksMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CBufStream ssPayload;
    CEventPeerGetBlocks eventGetBlocks(getBlocksMsg.nNonce, getBlocksMsg.hashFork);
    eventGetBlocks.data = getBlocksMsg.blockLocator;
    ssPayload << eventGetBlocks;
    SendDataMessage(eventGetBlocks.nNonce, PROTO_CMD_GETBLOCKS, ssPayload);
}

void CBbPeerNet::HandlePeerTx(const CPeerTxMessageOutBound& txMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CBufStream ssPayload;
    CEventPeerTx eventTx(txMsg.nNonce, txMsg.hashFork);
    eventTx.data = txMsg.tx;
    ssPayload << eventTx;
    SendDataMessage(eventTx.nNonce, PROTO_CMD_TX, ssPayload);
}

void CBbPeerNet::HandlePeerBlock(const CPeerBlockMessageOutBound& blockMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CBufStream ssPayload;
    CEventPeerBlock eventBlock(blockMsg.nNonce, blockMsg.hashFork);
    eventBlock.data = blockMsg.block;
    ssPayload << eventBlock;
    SendDataMessage(eventBlock.nNonce, PROTO_CMD_BLOCK, ssPayload);
}

bool CBbPeerNet::HandleEvent(CEventPeerBulletin& eventBulletin)
{
    CBufStream ssPayload;
    ssPayload << eventBulletin;
    return SendDelegatedMessage(eventBulletin.nNonce, PROTO_CMD_BULLETIN, ssPayload);
}

bool CBbPeerNet::HandleEvent(CEventPeerGetDelegated& eventGetDelegated)
{
    CBufStream ssPayload;
    ssPayload << eventGetDelegated;
    if (!SendDelegatedMessage(eventGetDelegated.nNonce, PROTO_CMD_GETDELEGATED, ssPayload))
    {
        return false;
    }

    CBufStream ss;
    ss << eventGetDelegated.hashAnchor << eventGetDelegated.data.destDelegate;
    uint256 hash = crypto::CryptoHash(ss.GetData(), ss.GetSize());

    vector<CInv> vInv;
    vInv.push_back(CInv(eventGetDelegated.data.nInvType, hash));

    SetInvTimer(eventGetDelegated.nNonce, vInv);
    return true;
}

bool CBbPeerNet::HandleEvent(CEventPeerDistribute& eventDistribute)
{
    CBufStream ssPayload;
    ssPayload << eventDistribute;
    return SendDelegatedMessage(eventDistribute.nNonce, PROTO_CMD_DISTRIBUTE, ssPayload);
}

bool CBbPeerNet::HandleEvent(CEventPeerPublish& eventPublish)
{
    CBufStream ssPayload;
    ssPayload << eventPublish;
    return SendDelegatedMessage(eventPublish.nNonce, PROTO_CMD_PUBLISH, ssPayload);
}

CPeer* CBbPeerNet::CreatePeer(CIOClient* pClient, uint64 nNonce, bool fInBound)
{
    uint32_t nTimerId = SetTimer(nNonce, HANDSHAKE_TIMEOUT);
    CBbPeer* pPeer = new CBbPeer(this, pClient, nNonce, fInBound, nMagicNum, nTimerId);
    if (pPeer == nullptr)
    {
        CancelTimer(nTimerId);
    }
    return pPeer;
}

void CBbPeerNet::DestroyPeer(CPeer* pPeer)
{
    CBbPeer* pBbPeer = static_cast<CBbPeer*>(pPeer);
    if (pBbPeer->IsHandshaked())
    {
        auto spDeactiveMsg = CPeerDeactiveMessage::Create();
        spDeactiveMsg->address = CAddress(pBbPeer->nService, pBbPeer->GetRemote());
        spDeactiveMsg->nNonce = pBbPeer->GetNonce();

        PUBLISH_MESSAGE(spDeactiveMsg);
    }
    CPeerNet::DestroyPeer(pPeer);
}

CPeerInfo* CBbPeerNet::GetPeerInfo(CPeer* pPeer, CPeerInfo* pInfo)
{
    pInfo = new CBbPeerInfo();
    if (pInfo != nullptr)
    {
        CBbPeer* pBbPeer = static_cast<CBbPeer*>(pPeer);
        CBbPeerInfo* pBbInfo = static_cast<CBbPeerInfo*>(pInfo);
        CPeerNet::GetPeerInfo(pPeer, pInfo);
        pBbInfo->nVersion = pBbPeer->nVersion;
        pBbInfo->nService = pBbPeer->nService;
        pBbInfo->strSubVer = pBbPeer->strSubVer;
        pBbInfo->nStartingHeight = pBbPeer->nStartingHeight;
    }
    return pInfo;
}

CAddress CBbPeerNet::GetGateWayAddress(const CNetHost& gateWayAddr)
{
    if (!gateWayAddr.IsEmpty() && !gateWayAddr.ToEndPoint().address().is_unspecified())
    {
        if (gateWayAddr.data.type() == typeid(uint64) && IsRoutable(gateWayAddr.ToEndPoint().address()))
        {
            uint64 nService = boost::any_cast<uint64>(gateWayAddr.data);
            return CAddress(nService, gateWayAddr.ToEndPoint());
        }
    }

    CNetHost defaultGateWay(NODE_DEFAULT_GATEWAY, confNetwork.nPortDefault, "",
                            boost::any(uint64(network::NODE_NETWORK)));
    uint64 nService = boost::any_cast<uint64>(defaultGateWay.data);
    return CAddress(nService, defaultGateWay.ToEndPoint());
}

bool CBbPeerNet::SendDataMessage(uint64 nNonce, int nCommand, CBufStream& ssPayload)
{
    CBbPeer* pBbPeer = static_cast<CBbPeer*>(GetPeer(nNonce));
    if (pBbPeer == nullptr)
    {
        return false;
    }
    return pBbPeer->SendMessage(PROTO_CHN_DATA, nCommand, ssPayload);
}

bool CBbPeerNet::SendDelegatedMessage(uint64 nNonce, int nCommand, xengine::CBufStream& ssPayload)
{
    CBbPeer* pBbPeer = static_cast<CBbPeer*>(GetPeer(nNonce));
    if (pBbPeer == nullptr)
    {
        return false;
    }
    return pBbPeer->SendMessage(PROTO_CHN_DELEGATE, nCommand, ssPayload);
}

void CBbPeerNet::SetInvTimer(uint64 nNonce, vector<CInv>& vInv)
{
    const int64 nTimeout[] = { 0, RESPONSE_TX_TIMEOUT, RESPONSE_BLOCK_TIMEOUT,
                               RESPONSE_DISTRIBUTE_TIMEOUT, RESPONSE_PUBLISH_TIMEOUT };
    CBbPeer* pBbPeer = static_cast<CBbPeer*>(GetPeer(nNonce));
    if (pBbPeer != nullptr)
    {
        int64 nElapse = 0;
        for (CInv& inv : vInv)
        {
            if (inv.nType >= CInv::MSG_TX && inv.nType <= CInv::MSG_PUBLISH)
            {
                nElapse += nTimeout[inv.nType];
                uint32 nTimerId = SetTimer(nNonce, nElapse);
                CancelTimer(pBbPeer->Request(inv, nTimerId));
            }
        }
    }
}

void CBbPeerNet::ProcessAskFor(CPeer* pPeer)
{
    uint256 hashFork;
    CInv inv;
    CBbPeer* pBbPeer = static_cast<CBbPeer*>(pPeer);
    if (pBbPeer->FetchAskFor(hashFork, inv))
    {
        auto spGetDataMsg = CPeerGetDataMessageInBound::Create();
        spGetDataMsg->nNonce = pBbPeer->GetNonce();
        spGetDataMsg->hashFork = hashFork;
        spGetDataMsg->vecInv.push_back(inv);
        PUBLISH_MESSAGE(spGetDataMsg);
    }
}

void CBbPeerNet::BuildHello(CPeer* pPeer, CBufStream& ssPayload)
{
    uint64 nNonce = pPeer->GetNonce();
    int64 nTime = GetNetTime();
    int nHeight = pNetChannel->GetPrimaryChainHeight();
    ssPayload << nVersion << nService << nTime << nNonce << subVersion << nHeight;
}

void CBbPeerNet::HandlePeerWriten(CPeer* pPeer)
{
    ProcessAskFor(pPeer);
}

bool CBbPeerNet::HandlePeerHandshaked(CPeer* pPeer, uint32 nTimerId)
{
    CBbPeer* pBbPeer = static_cast<CBbPeer*>(pPeer);
    CancelTimer(nTimerId);
    if (!CheckPeerVersion(pBbPeer->nVersion, pBbPeer->nService, pBbPeer->strSubVer))
    {
        return false;
    }
    if (!pBbPeer->IsInBound())
    {
        tcp::endpoint ep = pBbPeer->GetRemote();
        if (GetPeer(pBbPeer->nNonceFrom) != nullptr)
        {
            RemoveNode(ep);
            return false;
        }

        string strName = GetNodeName(ep);
        if (strName == "dnseed")
        {
            setDNSeed.insert(ep);
        }

        SetNodeData(ep, boost::any(pBbPeer->nService));
    }

    UpdateNetTime(pBbPeer->GetRemote().address(), pBbPeer->nTimeDelta);

    auto spActiveMsg = CPeerActiveMessage::Create();
    spActiveMsg->nNonce = pBbPeer->GetNonce();
    spActiveMsg->address = CAddress(pBbPeer->nService, pBbPeer->GetRemote());
    PUBLISH_MESSAGE(spActiveMsg);

    if (!fEnclosed)
    {
        pBbPeer->SendMessage(PROTO_CHN_NETWORK, PROTO_CMD_GETADDRESS);
    }
    return true;
}

bool CBbPeerNet::HandlePeerRecvMessage(CPeer* pPeer, int nChannel, int nCommand, CBufStream& ssPayload)
{
    CBbPeer* pBbPeer = static_cast<CBbPeer*>(pPeer);
    if (nChannel == PROTO_CHN_NETWORK)
    {
        switch (nCommand)
        {
        case PROTO_CMD_GETADDRESS:
        {
            vector<CNodeAvail> vNode;
            RetrieveGoodNode(vNode, NODE_ACTIVE_TIME, 499);

            vector<CAddress> vAddr;
            vAddr.push_back(GetGateWayAddress(confNetwork.gateWayAddr));

            for (const CNodeAvail& node : vNode)
            {
                if (node.data.type() == typeid(uint64)
                    && IsRoutable(node.ep.address()))
                {
                    uint64 nService = boost::any_cast<uint64>(node.data);
                    vAddr.push_back(CAddress(nService, node.ep));
                }
            }

            CBufStream ss;
            ss << vAddr;
            return pBbPeer->SendMessage(PROTO_CHN_NETWORK, PROTO_CMD_ADDRESS, ss);
        }
        break;
        case PROTO_CMD_ADDRESS:
            if (!fEnclosed)
            {
                vector<CAddress> vAddr;
                ssPayload >> vAddr;
                if (vAddr.size() > 500)
                {
                    return false;
                }

                bool fHaveGateway = false;
                tcp::endpoint epGateway;
                if (!confNetwork.gateWayAddr.IsEmpty())
                {
                    epGateway = confNetwork.gateWayAddr.ToEndPoint();
                    fHaveGateway = true;
                }

                for (int i = 0; i < vAddr.size(); ++i)
                {
                    CAddress& addr = vAddr[i];
                    tcp::endpoint ep;
                    addr.ssEndpoint.GetEndpoint(ep);
                    if ((i == 0 && ep.address().to_string() == NODE_DEFAULT_GATEWAY) || (fHaveGateway && ep == epGateway))
                    {
                        continue;
                    }
                    if ((addr.nService & NODE_NETWORK) == NODE_NETWORK
                        && IsRoutable(ep.address())
                        && !setDNSeed.count(ep)
                        && !setProtocolInvalidIP.count(ep.address()))
                    {
                        AddNewNode(ep, ep.address().to_string(), boost::any(addr.nService));
                    }
                }

                if (!pBbPeer->IsInBound() && setDNSeed.count(pBbPeer->GetRemote()))
                {
                    RemoveNode(pBbPeer->GetRemote());
                }
                return true;
            }
            break;
        case PROTO_CMD_PING:
            return pBbPeer->SendMessage(PROTO_CHN_NETWORK, PROTO_CMD_PONG, ssPayload);
            break;
        case PROTO_CMD_PONG:
            return true;
            break;
        default:
            break;
        }
    }
    else if (nChannel == PROTO_CHN_DATA)
    {
        uint256 hashFork;
        ssPayload >> hashFork;
        switch (nCommand)
        {
        case PROTO_CMD_SUBSCRIBE:
        {
            auto spSubscribeMsg = CPeerSubscribeMessageInBound::Create();
            spSubscribeMsg->nNonce = pBbPeer->GetNonce();
            spSubscribeMsg->hashFork = hashFork;
            ssPayload >> spSubscribeMsg->vecForks;
            PUBLISH_MESSAGE(spSubscribeMsg);
            return true;
        }
        break;
        case PROTO_CMD_UNSUBSCRIBE:
        {
            auto spUnsubscribeMsg = CPeerUnsubscribeMessageInBound::Create();
            spUnsubscribeMsg->nNonce = pBbPeer->GetNonce();
            spUnsubscribeMsg->hashFork = hashFork;
            ssPayload >> spUnsubscribeMsg->vecForks;
            PUBLISH_MESSAGE(spUnsubscribeMsg);
            return true;
        }
        break;
        case PROTO_CMD_GETBLOCKS:
        {
            auto spGetBlocksMsg = CPeerGetBlocksMessageInBound::Create();
            spGetBlocksMsg->nNonce = pBbPeer->GetNonce();
            spGetBlocksMsg->hashFork = hashFork;
            ssPayload >> spGetBlocksMsg->blockLocator;
            PUBLISH_MESSAGE(spGetBlocksMsg);
            return true;
        }
        break;
        case PROTO_CMD_GETDATA:
        {
            vector<CInv> vInv;
            ssPayload >> vInv;
            pBbPeer->AskFor(hashFork, vInv);
            ProcessAskFor(pPeer);
            return true;
        }
        break;
        case PROTO_CMD_INV:
        {
            auto spInvMsg = CPeerInvMessageInBound::Create();
            spInvMsg->nNonce = pBbPeer->GetNonce();
            spInvMsg->hashFork = hashFork;
            ssPayload >> spInvMsg->vecInv;
            PUBLISH_MESSAGE(spInvMsg);
            return true;
        }
        break;
        case PROTO_CMD_TX:
        {
            auto spTxMsg = CPeerTxMessageInBound::Create();
            spTxMsg->nNonce = pBbPeer->GetNonce();
            spTxMsg->hashFork = hashFork;
            ssPayload >> spTxMsg->tx;
            CInv inv(CInv::MSG_TX, spTxMsg->tx.GetHash());
            CancelTimer(pBbPeer->Responded(inv));
            PUBLISH_MESSAGE(spTxMsg);
            return true;
        }
        break;
        case PROTO_CMD_BLOCK:
        {
            auto spBlockMsg = CPeerBlockMessageInBound::Create();
            spBlockMsg->nNonce = pBbPeer->GetNonce();
            spBlockMsg->hashFork = hashFork;
            ssPayload >> spBlockMsg->block;
            CInv inv(CInv::MSG_BLOCK, spBlockMsg->block.GetHash());
            CancelTimer(pBbPeer->Responded(inv));
            PUBLISH_MESSAGE(spBlockMsg);
            return true;
        }
        break;
        default:
            break;
        }
    }
    else if (nChannel == PROTO_CHN_DELEGATE)
    {
        uint256 hashAnchor;
        ssPayload >> hashAnchor;
        switch (nCommand)
        {
        case PROTO_CMD_BULLETIN:
        {
            auto spBulletinMsg = CPeerBulletinMessageInBound::Create();
            spBulletinMsg->nNonce = pBbPeer->GetNonce();
            spBulletinMsg->hashAnchor = hashAnchor;
            ssPayload >> spBulletinMsg->deletegatedBulletin;
            PUBLISH_MESSAGE(spBulletinMsg);
        }
        break;
        case PROTO_CMD_GETDELEGATED:
        {
            auto spGetDelegatedMsg = CPeerGetDelegatedMessageInBound::Create();
            spGetDelegatedMsg->nNonce = pBbPeer->GetNonce();
            spGetDelegatedMsg->hashAnchor = hashAnchor;
            ssPayload >> spGetDelegatedMsg->delegatedGetData;
            PUBLISH_MESSAGE(spGetDelegatedMsg);
        }
        break;
        case PROTO_CMD_DISTRIBUTE:
        {
            auto spDistributeMsg = CPeerDistributeMessageInBound::Create();
            spDistributeMsg->nNonce = pBbPeer->GetNonce();
            spDistributeMsg->hashAnchor = hashAnchor;
            ssPayload >> spDistributeMsg->delegatedData;

            CBufStream ss;
            ss << hashAnchor << (spDistributeMsg->delegatedData.destDelegate);
            uint256 hash = crypto::CryptoHash(ss.GetData(), ss.GetSize());
            CInv inv(CInv::MSG_DISTRIBUTE, hash);
            CancelTimer(pBbPeer->Responded(inv));

            PUBLISH_MESSAGE(spDistributeMsg);
        }
        break;
        case PROTO_CMD_PUBLISH:
        {
            auto spPublishMsh = CPeerPublishMessageInBound::Create();
            spPublishMsh->nNonce = pBbPeer->GetNonce();
            spPublishMsh->hashAnchor = hashAnchor;
            ssPayload >> spPublishMsh->delegatedData;

            CBufStream ss;
            ss << hashAnchor << (spPublishMsh->delegatedData.destDelegate);
            uint256 hash = crypto::CryptoHash(ss.GetData(), ss.GetSize());
            CInv inv(CInv::MSG_PUBLISH, hash);
            CancelTimer(pBbPeer->Responded(inv));

            PUBLISH_MESSAGE(spPublishMsh);
        }
        break;
        default:
            break;
        }
    }
    return false;
}

} // namespace network
} // namespace bigbang
