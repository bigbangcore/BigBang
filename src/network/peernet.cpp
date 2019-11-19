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
    nPrimaryChainHeight = 0;
    fEnclosed = false;
}

CBbPeerNet::~CBbPeerNet()
{
}

bool CBbPeerNet::HandleInitialize()
{
    RegisterHandler({
        PTR_HANDLER(CPeerSubscribeMessageOutBound, boost::bind(&CBbPeerNet::HandleSubscribe, this, _1), true),
        PTR_HANDLER(CPeerUnsubscribeMessageOutBound, boost::bind(&CBbPeerNet::HandleUnsubscribe, this, _1), true),
        PTR_HANDLER(CPeerInvMessageOutBound, boost::bind(&CBbPeerNet::HandleInv, this, _1), true),
        PTR_HANDLER(CPeerGetDataMessageOutBound, boost::bind(&CBbPeerNet::HandleGetData, this, _1), true),
        PTR_HANDLER(CPeerGetBlocksMessageOutBound, boost::bind(&CBbPeerNet::HandleGetBlocks, this, _1), true),
        PTR_HANDLER(CPeerTxMessageOutBound, boost::bind(&CBbPeerNet::HandlePeerTx, this, _1), true),
        PTR_HANDLER(CPeerBlockMessageOutBound, boost::bind(&CBbPeerNet::HandlePeerBlock, this, _1), true),

        PTR_HANDLER(CPeerBulletinMessageOutBound, boost::bind(&CBbPeerNet::HandleBulletin, this, _1), true),
        PTR_HANDLER(CPeerGetDelegatedMessageOutBound, boost::bind(&CBbPeerNet::HandleGetDelegate, this, _1), true),
        PTR_HANDLER(CPeerDistributeMessageOutBound, boost::bind(&CBbPeerNet::HandleDistribute, this, _1), true),
        PTR_HANDLER(CPeerPublishMessageOutBound, boost::bind(&CBbPeerNet::HandlePublish, this, _1), true),
        PTR_HANDLER(CAddedBlockMessage, boost::bind(&CBbPeerNet::HandlePrimaryChainHeightUpdate, this, _1), true),
    });

    return true;
}

void CBbPeerNet::HandleDeinitialize()
{
    setDNSeed.clear();

    DeregisterHandler();
}

void CBbPeerNet::HandleSubscribe(const shared_ptr<CPeerSubscribeMessageOutBound> spMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CBufStream ssPayload;
    CEventPeerSubscribe eventSubscribe(spMsg->nNonce, spMsg->hashFork);
    eventSubscribe.data = spMsg->vecForks;
    ssPayload << eventSubscribe;
    SendDataMessage(eventSubscribe.nNonce, PROTO_CMD_SUBSCRIBE, ssPayload);
}

void CBbPeerNet::HandleUnsubscribe(const shared_ptr<CPeerUnsubscribeMessageOutBound> spMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CBufStream ssPayload;
    CEventPeerUnsubscribe eventUnsubscribe(spMsg->nNonce, spMsg->hashFork);
    eventUnsubscribe.data = spMsg->vecForks;
    ssPayload << eventUnsubscribe;
    SendDataMessage(eventUnsubscribe.nNonce, PROTO_CMD_UNSUBSCRIBE, ssPayload);
}

void CBbPeerNet::HandleInv(const shared_ptr<CPeerInvMessageOutBound> spMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CBufStream ssPayload;
    CEventPeerInv eventInv(spMsg->nNonce, spMsg->hashFork);
    eventInv.data = spMsg->vecInv;
    ssPayload << eventInv;
    SendDataMessage(eventInv.nNonce, PROTO_CMD_INV, ssPayload);
}

void CBbPeerNet::HandleGetData(const shared_ptr<CPeerGetDataMessageOutBound> spMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CBufStream ssPayload;
    CEventPeerGetData eventGetData(spMsg->nNonce, spMsg->hashFork);
    eventGetData.data = spMsg->vecInv;
    ssPayload << eventGetData;
    if (!SendDataMessage(eventGetData.nNonce, PROTO_CMD_GETDATA, ssPayload))
    {
        return;
    }

    SetInvTimer(eventGetData.nNonce, eventGetData.data);
}

void CBbPeerNet::HandleGetBlocks(const shared_ptr<CPeerGetBlocksMessageOutBound> spMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CBufStream ssPayload;
    CEventPeerGetBlocks eventGetBlocks(spMsg->nNonce, spMsg->hashFork);
    eventGetBlocks.data = spMsg->blockLocator;
    ssPayload << eventGetBlocks;
    SendDataMessage(eventGetBlocks.nNonce, PROTO_CMD_GETBLOCKS, ssPayload);
}

void CBbPeerNet::HandlePeerTx(const shared_ptr<CPeerTxMessageOutBound> spMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CBufStream ssPayload;
    CEventPeerTx eventTx(spMsg->nNonce, spMsg->hashFork);
    eventTx.data = spMsg->tx;
    ssPayload << eventTx;
    SendDataMessage(eventTx.nNonce, PROTO_CMD_TX, ssPayload);
}

void CBbPeerNet::HandlePeerBlock(const shared_ptr<CPeerBlockMessageOutBound> spMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CBufStream ssPayload;
    CEventPeerBlock eventBlock(spMsg->nNonce, spMsg->hashFork);
    eventBlock.data = spMsg->block;
    ssPayload << eventBlock;
    SendDataMessage(eventBlock.nNonce, PROTO_CMD_BLOCK, ssPayload);
}

void CBbPeerNet::HandleBulletin(const shared_ptr<CPeerBulletinMessageOutBound> spMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CEventPeerBulletin eventBulletin(spMsg->nNonce, spMsg->hashAnchor);
    eventBulletin.data = spMsg->deletegatedBulletin;
    CBufStream ssPayload;
    ssPayload << eventBulletin;
    SendDelegatedMessage(eventBulletin.nNonce, PROTO_CMD_BULLETIN, ssPayload);
}

void CBbPeerNet::HandleGetDelegate(const shared_ptr<CPeerGetDelegatedMessageOutBound> spMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CEventPeerGetDelegated eventGetDelegated(spMsg->nNonce, spMsg->hashAnchor);
    eventGetDelegated.data = spMsg->delegatedGetData;
    CBufStream ssPayload;
    ssPayload << eventGetDelegated;
    if (!SendDelegatedMessage(eventGetDelegated.nNonce, PROTO_CMD_GETDELEGATED, ssPayload))
    {
        return;
    }

    CBufStream ss;
    ss << eventGetDelegated.hashAnchor << eventGetDelegated.data.destDelegate;
    uint256 hash = crypto::CryptoHash(ss.GetData(), ss.GetSize());

    vector<CInv> vInv;
    vInv.push_back(CInv(eventGetDelegated.data.nInvType, hash));

    SetInvTimer(eventGetDelegated.nNonce, vInv);
}

void CBbPeerNet::HandleDistribute(const shared_ptr<CPeerDistributeMessageOutBound> spMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CEventPeerDistribute eventDistribute(spMsg->nNonce, spMsg->hashAnchor);
    eventDistribute.data = spMsg->delegatedData;
    CBufStream ssPayload;
    ssPayload << eventDistribute;
    SendDelegatedMessage(eventDistribute.nNonce, PROTO_CMD_DISTRIBUTE, ssPayload);
}

void CBbPeerNet::HandlePublish(const shared_ptr<CPeerPublishMessageOutBound> spMsg)
{
    // TODO: All Message Type need to serilize to CBufStream
    CEventPeerPublish eventPublish(spMsg->nNonce, spMsg->hashAnchor);
    eventPublish.data = spMsg->delegatedData;
    CBufStream ssPayload;
    ssPayload << eventPublish;
    SendDelegatedMessage(eventPublish.nNonce, PROTO_CMD_PUBLISH, ssPayload);
}

void CBbPeerNet::HandlePrimaryChainHeightUpdate(const shared_ptr<CAddedBlockMessage> spMsg)
{
    if (spMsg->block.nType == CBlock::BLOCK_PRIMARY)
    {
        nPrimaryChainHeight = spMsg->update.nLastBlockHeight;
    }
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

        PUBLISH(spDeactiveMsg);
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
        PUBLISH(spGetDataMsg);
    }
}

void CBbPeerNet::BuildHello(CPeer* pPeer, CBufStream& ssPayload)
{
    uint64 nNonce = pPeer->GetNonce();
    int64 nTime = GetNetTime();
    ssPayload << nVersion << nService << nTime << nNonce << subVersion << nPrimaryChainHeight;
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
    PUBLISH(spActiveMsg);

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
            PUBLISH(spSubscribeMsg);
            return true;
        }
        break;
        case PROTO_CMD_UNSUBSCRIBE:
        {
            auto spUnsubscribeMsg = CPeerUnsubscribeMessageInBound::Create();
            spUnsubscribeMsg->nNonce = pBbPeer->GetNonce();
            spUnsubscribeMsg->hashFork = hashFork;
            ssPayload >> spUnsubscribeMsg->vecForks;
            PUBLISH(spUnsubscribeMsg);
            return true;
        }
        break;
        case PROTO_CMD_GETBLOCKS:
        {
            auto spGetBlocksMsg = CPeerGetBlocksMessageInBound::Create();
            spGetBlocksMsg->nNonce = pBbPeer->GetNonce();
            spGetBlocksMsg->hashFork = hashFork;
            ssPayload >> spGetBlocksMsg->blockLocator;
            PUBLISH(spGetBlocksMsg);
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
            PUBLISH(spInvMsg);
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
            PUBLISH(spTxMsg);
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
            PUBLISH(spBlockMsg);
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
            PUBLISH(spBulletinMsg);
        }
        break;
        case PROTO_CMD_GETDELEGATED:
        {
            auto spGetDelegatedMsg = CPeerGetDelegatedMessageInBound::Create();
            spGetDelegatedMsg->nNonce = pBbPeer->GetNonce();
            spGetDelegatedMsg->hashAnchor = hashAnchor;
            ssPayload >> spGetDelegatedMsg->delegatedGetData;
            PUBLISH(spGetDelegatedMsg);
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

            PUBLISH(spDistributeMsg);
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

            PUBLISH(spPublishMsh);
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
