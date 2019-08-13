// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "peernet.h"

#include <boost/any.hpp>
#include <boost/bind.hpp>

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
    pNetChannel = NULL;
    pDelegatedChannel = NULL;
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

    return true;
}

void CBbPeerNet::HandleDeinitialize()
{
    setDNSeed.clear();
    pNetChannel = NULL;
    pDelegatedChannel = NULL;
}

bool CBbPeerNet::HandleEvent(CEventPeerSubscribe& eventSubscribe)
{
    CBufStream ssPayload;
    ssPayload << eventSubscribe;
    return SendDataMessage(eventSubscribe.nNonce, PROTO_CMD_SUBSCRIBE, ssPayload);
}

bool CBbPeerNet::HandleEvent(CEventPeerUnsubscribe& eventUnsubscribe)
{
    CBufStream ssPayload;
    ssPayload << eventUnsubscribe;
    return SendDataMessage(eventUnsubscribe.nNonce, PROTO_CMD_UNSUBSCRIBE, ssPayload);
}

bool CBbPeerNet::HandleEvent(CEventPeerInv& eventInv)
{
    CBufStream ssPayload;
    ssPayload << eventInv;
    return SendDataMessage(eventInv.nNonce, PROTO_CMD_INV, ssPayload);
}

bool CBbPeerNet::HandleEvent(CEventPeerGetData& eventGetData)
{
    CBufStream ssPayload;
    ssPayload << eventGetData;
    if (!SendDataMessage(eventGetData.nNonce, PROTO_CMD_GETDATA, ssPayload))
    {
        return false;
    }

    SetInvTimer(eventGetData.nNonce, eventGetData.data);
    return true;
}

bool CBbPeerNet::HandleEvent(CEventPeerGetBlocks& eventGetBlocks)
{
    CBufStream ssPayload;
    ssPayload << eventGetBlocks;
    return SendDataMessage(eventGetBlocks.nNonce, PROTO_CMD_GETBLOCKS, ssPayload);
}

bool CBbPeerNet::HandleEvent(CEventPeerTx& eventTx)
{
    CBufStream ssPayload;
    ssPayload << eventTx;
    return SendDataMessage(eventTx.nNonce, PROTO_CMD_TX, ssPayload);
}

bool CBbPeerNet::HandleEvent(CEventPeerBlock& eventBlock)
{
    CBufStream ssPayload;
    ssPayload << eventBlock;
    return SendDataMessage(eventBlock.nNonce, PROTO_CMD_BLOCK, ssPayload);
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
    if (pPeer == NULL)
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
        if (pEventDeactive != NULL)
        {
            pEventDeactive->data = CAddress(pBbPeer->nService, pBbPeer->GetRemote());

            CEventPeerDeactive* pEventDeactiveDelegated = new CEventPeerDeactive(*pEventDeactive);

            pNetChannel->PostEvent(pEventDeactive);

            if (pEventDeactiveDelegated != NULL)
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
    if (pInfo != NULL)
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
    if (pBbPeer == NULL)
    {
        return false;
    }
    return pBbPeer->SendMessage(PROTO_CHN_DATA, nCommand, ssPayload);
}

bool CBbPeerNet::SendDelegatedMessage(uint64 nNonce, int nCommand, xengine::CBufStream& ssPayload)
{
    CBbPeer* pBbPeer = static_cast<CBbPeer*>(GetPeer(nNonce));
    if (pBbPeer == NULL)
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
    if (pBbPeer != NULL)
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
        CEventPeerGetData* pEventGetData = new CEventPeerGetData(pBbPeer->GetNonce(), hashFork);
        if (pEventGetData != NULL)
        {
            pEventGetData->data.push_back(inv);
            pNetChannel->PostEvent(pEventGetData);
        }
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
        if (GetPeer(pBbPeer->nNonceFrom) != NULL)
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

    CEventPeerActive* pEventActive = new CEventPeerActive(pBbPeer->GetNonce());
    if (pEventActive == NULL)
    {
        return false;
    }

    pEventActive->data = CAddress(pBbPeer->nService, pBbPeer->GetRemote());
    CEventPeerActive* pEventActiveDelegated = new CEventPeerActive(*pEventActive);

    pNetChannel->PostEvent(pEventActive);
    if (pEventActiveDelegated != NULL)
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
            CEventPeerSubscribe* pEvent = new CEventPeerSubscribe(pBbPeer->GetNonce(), hashFork);
            if (pEvent != NULL)
            {
                ssPayload >> pEvent->data;
                pNetChannel->PostEvent(pEvent);
                return true;
            }
        }
        break;
        case PROTO_CMD_UNSUBSCRIBE:
        {
            CEventPeerUnsubscribe* pEvent = new CEventPeerUnsubscribe(pBbPeer->GetNonce(), hashFork);
            if (pEvent != NULL)
            {
                ssPayload >> pEvent->data;
                pNetChannel->PostEvent(pEvent);
                return true;
            }
        }
        break;
        case PROTO_CMD_GETBLOCKS:
        {
            CEventPeerGetBlocks* pEvent = new CEventPeerGetBlocks(pBbPeer->GetNonce(), hashFork);
            if (pEvent != NULL)
            {
                ssPayload >> pEvent->data;
                pNetChannel->PostEvent(pEvent);
                return true;
            }
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
            CEventPeerInv* pEvent = new CEventPeerInv(pBbPeer->GetNonce(), hashFork);
            if (pEvent != NULL)
            {
                ssPayload >> pEvent->data;
                pNetChannel->PostEvent(pEvent);
                return true;
            }
        }
        break;
        case PROTO_CMD_TX:
        {
            CEventPeerTx* pEvent = new CEventPeerTx(pBbPeer->GetNonce(), hashFork);
            if (pEvent != NULL)
            {
                ssPayload >> pEvent->data;
                CInv inv(CInv::MSG_TX, pEvent->data.GetHash());
                CancelTimer(pBbPeer->Responded(inv));
                pNetChannel->PostEvent(pEvent);
                return true;
            }
        }
        break;
        case PROTO_CMD_BLOCK:
        {
            CEventPeerBlock* pEvent = new CEventPeerBlock(pBbPeer->GetNonce(), hashFork);
            if (pEvent != NULL)
            {
                ssPayload >> pEvent->data;
                CInv inv(CInv::MSG_BLOCK, pEvent->data.GetHash());
                CancelTimer(pBbPeer->Responded(inv));
                pNetChannel->PostEvent(pEvent);
                return true;
            }
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
            if (pEvent != NULL)
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
            if (pEvent != NULL)
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
            if (pEvent != NULL)
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
            if (pEvent != NULL)
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
