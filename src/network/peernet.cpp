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
    pDelegatedChannel = nullptr;
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

    if (!GetObject("delegatedchannel", pDelegatedChannel))
    {
        Error("Failed to request delegated datachannel\n");
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
    pDelegatedChannel = nullptr;
}

/*
bool CBbPeerNet::HandleEvent(CEventPeerBlock& eventBlock)
{
    CBufStream ssPayload;
    ssPayload << eventBlock;
    return SendDataMessage(eventBlock.nNonce, PROTO_CMD_BLOCK, ssPayload);
}*/

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
        CEventPeerDeactive* pEventDeactive = new CEventPeerDeactive(pBbPeer->GetNonce());
        if (pEventDeactive != nullptr)
        {
            pEventDeactive->data = CAddress(pBbPeer->nService, pBbPeer->GetRemote());

            CEventPeerDeactive* pEventDeactiveDelegated = new CEventPeerDeactive(*pEventDeactive);

            CPeerDeactiveMessage* pDeactiveMsg = new CPeerDeactiveMessage();
            pDeactiveMsg->address = pEventDeactive->data;
            pDeactiveMsg->nNonce = pEventDeactive->nNonce;

            PUBLISH_MESSAGE(pDeactiveMsg);

            if (pEventDeactiveDelegated != nullptr)
            {
                pDelegatedChannel->PostEvent(pEventDeactiveDelegated);
            }
        }
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
        CPeerGetDataMessageInBound* pGetDataMsg = new CPeerGetDataMessageInBound();
        pGetDataMsg->nNonce = pBbPeer->GetNonce();
        pGetDataMsg->hashFork = hashFork;
        pGetDataMsg->vecInv.push_back(inv);
        PUBLISH_MESSAGE(pGetDataMsg);
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

    CEventPeerActive* pEventActiveDelegated = new CEventPeerActive(pBbPeer->GetNonce());
    pEventActiveDelegated->data = CAddress(pBbPeer->nService, pBbPeer->GetRemote());

    CPeerActiveMessage* pActiveMsg = new CPeerActiveMessage();
    pActiveMsg->nNonce = pEventActiveDelegated->nNonce;
    pActiveMsg->address = pEventActiveDelegated->data;
    PUBLISH_MESSAGE(pActiveMsg);

    if (pEventActiveDelegated != nullptr)
    {
        pDelegatedChannel->PostEvent(pEventActiveDelegated);
    }

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
            CPeerSubscribeMessageInBound* pSubscribeMsg = new CPeerSubscribeMessageInBound();
            pSubscribeMsg->nNonce = pBbPeer->GetNonce();
            pSubscribeMsg->hashFork = hashFork;
            ssPayload >> pSubscribeMsg->vecForks;
            PUBLISH_MESSAGE(pSubscribeMsg);
            return true;
        }
        break;
        case PROTO_CMD_UNSUBSCRIBE:
        {
            CPeerUnsubscribeMessageInBound* pUnsubscribeMsg = new CPeerUnsubscribeMessageInBound();
            pUnsubscribeMsg->nNonce = pBbPeer->GetNonce();
            pUnsubscribeMsg->hashFork = hashFork;
            ssPayload >> pUnsubscribeMsg->vecForks;
            PUBLISH_MESSAGE(pUnsubscribeMsg);
            return true;
        }
        break;
        case PROTO_CMD_GETBLOCKS:
        {
            CPeerGetBlocksMessageInBound* pGetBlocksMsg = new CPeerGetBlocksMessageInBound();
            pGetBlocksMsg->nNonce = pBbPeer->GetNonce();
            pGetBlocksMsg->hashFork = hashFork;
            ssPayload >> pGetBlocksMsg->blockLocator;
            PUBLISH_MESSAGE(pGetBlocksMsg);
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
            CPeerInvMessageInBound* pInvMsg = new CPeerInvMessageInBound();
            pInvMsg->nNonce = pBbPeer->GetNonce();
            pInvMsg->hashFork = hashFork;
            ssPayload >> pInvMsg->vecInv;
            PUBLISH_MESSAGE(pInvMsg);
            return true;
        }
        break;
        case PROTO_CMD_TX:
        {
            CPeerTxMessageInBound* pTxMsg = new CPeerTxMessageInBound();
            pTxMsg->nNonce = pBbPeer->GetNonce();
            pTxMsg->hashFork = hashFork;
            ssPayload >> pTxMsg->tx;
            CInv inv(CInv::MSG_TX, pTxMsg->tx.GetHash());
            CancelTimer(pBbPeer->Responded(inv));
            PUBLISH_MESSAGE(pTxMsg);
            return true;
        }
        break;
        case PROTO_CMD_BLOCK:
        {
            CPeerBlockMessageInBound* pBlockMsg = new CPeerBlockMessageInBound();
            pBlockMsg->nNonce = pBbPeer->GetNonce();
            pBlockMsg->hashFork = hashFork;
            ssPayload >> pBlockMsg->block;
            CInv inv(CInv::MSG_BLOCK, pBlockMsg->block.GetHash());
            CancelTimer(pBbPeer->Responded(inv));
            PUBLISH_MESSAGE(pBlockMsg);
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
            CEventPeerBulletin* pEvent = new CEventPeerBulletin(pBbPeer->GetNonce(), hashAnchor);
            if (pEvent != nullptr)
            {
                ssPayload >> pEvent->data;
                pDelegatedChannel->PostEvent(pEvent);
                return true;
            }
        }
        break;
        case PROTO_CMD_GETDELEGATED:
        {
            CEventPeerGetDelegated* pEvent = new CEventPeerGetDelegated(pBbPeer->GetNonce(), hashAnchor);
            if (pEvent != nullptr)
            {
                ssPayload >> pEvent->data;
                pDelegatedChannel->PostEvent(pEvent);
                return true;
            }
        }
        break;
        case PROTO_CMD_DISTRIBUTE:
        {
            CEventPeerDistribute* pEvent = new CEventPeerDistribute(pBbPeer->GetNonce(), hashAnchor);
            if (pEvent != nullptr)
            {
                ssPayload >> pEvent->data;

                CBufStream ss;
                ss << hashAnchor << (pEvent->data.destDelegate);
                uint256 hash = crypto::CryptoHash(ss.GetData(), ss.GetSize());
                CInv inv(CInv::MSG_DISTRIBUTE, hash);
                CancelTimer(pBbPeer->Responded(inv));

                pDelegatedChannel->PostEvent(pEvent);

                return true;
            }
        }
        break;
        case PROTO_CMD_PUBLISH:
        {
            CEventPeerPublish* pEvent = new CEventPeerPublish(pBbPeer->GetNonce(), hashAnchor);
            if (pEvent != nullptr)
            {
                ssPayload >> pEvent->data;

                CBufStream ss;
                ss << hashAnchor << (pEvent->data.destDelegate);
                uint256 hash = crypto::CryptoHash(ss.GetData(), ss.GetSize());
                CInv inv(CInv::MSG_PUBLISH, hash);
                CancelTimer(pBbPeer->Responded(inv));

                pDelegatedChannel->PostEvent(pEvent);
                return true;
            }
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
