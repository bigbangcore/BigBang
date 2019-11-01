// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "peernet.h"

#include <boost/bind.hpp>
#include <openssl/rand.h>
#include <string>

#include "netio/nethost.h"
#include "util.h"
#include "nonce.h"

using namespace std;
using boost::asio::ip::tcp;

#define CONNECT_TIMEOUT 10

namespace xengine
{

///////////////////////////////
// CPeerNet

CPeerNet::CPeerNet(const string& strOwnKeyIn)
  : CIOProc(strOwnKeyIn), confNetwork{}
{
    RegisterRefHandler<CPeerNetCloseMessage>(boost::bind(&CPeerNet::HandlePeerNetClose, this, _1));
    RegisterRefHandler<CPeerNetRewardMessage>(boost::bind(&CPeerNet::HandlePeerNetReward, this, _1));

    RegisterPtrHandler<CPeerNetGetIPMessage>(boost::bind(&CPeerNet::HandlePeerGetIP, this, _1));
    RegisterPtrHandler<CPeerNetGetCountMessage>(boost::bind(&CPeerNet::HandlePeerGetCount, this, _1));
    RegisterPtrHandler<CPeerNetGetPeersMessage>(boost::bind(&CPeerNet::HandlePeerGetPeers, this, _1));
    RegisterPtrHandler<CPeerNetAddNodeMessage>(boost::bind(&CPeerNet::HandlePeerAddNode, this, _1));
    RegisterPtrHandler<CPeerNetRemoveNodeMessage>(boost::bind(&CPeerNet::HandlePeerRemoveNode, this, _1));
    RegisterPtrHandler<CPeerNetGetBannedMessage>(boost::bind(&CPeerNet::HandlePeerGetBanned, this, _1));
    RegisterPtrHandler<CPeerNetSetBanMessage>(boost::bind(&CPeerNet::HandlePeerSetBan, this, _1));
    RegisterPtrHandler<CPeerNetClrBannedMessage>(boost::bind(&CPeerNet::HandlePeerClrBanned, this, _1));
}

CPeerNet::~CPeerNet()
{
    DeregisterHandler(CPeerNetCloseMessage::MessageType());
    DeregisterHandler(CPeerNetRewardMessage::MessageType());

    DeregisterHandler(CPeerNetGetIPMessage::MessageType());
    DeregisterHandler(CPeerNetGetCountMessage::MessageType());
    DeregisterHandler(CPeerNetGetPeersMessage::MessageType());
    DeregisterHandler(CPeerNetAddNodeMessage::MessageType());
    DeregisterHandler(CPeerNetRemoveNodeMessage::MessageType());
    DeregisterHandler(CPeerNetGetBannedMessage::MessageType());
    DeregisterHandler(CPeerNetSetBanMessage::MessageType());
    DeregisterHandler(CPeerNetClrBannedMessage::MessageType());
}

void CPeerNet::ConfigNetwork(CPeerNetConfig& config)
{
    confNetwork = config;
}

void CPeerNet::HandlePeerClose(CPeer* pPeer)
{
    RemovePeer(pPeer, CEndpointManager::HOST_CLOSE);
}
void CPeerNet::HandlePeerViolate(CPeer* pPeer)
{
    AddProtocolInvalidPeer(pPeer);
    RemovePeer(pPeer, CEndpointManager::PROTOCOL_INVALID);
}

void CPeerNet::HandlePeerError(CPeer* pPeer)
{
    RemovePeer(pPeer, CEndpointManager::NETWORK_ERROR);
}

void CPeerNet::HandlePeerWriten(CPeer* pPeer)
{
}

bool CPeerNet::EnterLoop()
{
    if (!CIOProc::EnterLoop())
    {
        return false;
    }

    for (const CPeerService& service : confNetwork.vecService)
    {
        if (StartService(service.epListen, service.nMaxInBounds))
        {
            INFO("Listen port %s:%u, connections limit = %u",
                 service.epListen.address().to_string().c_str(),
                 service.epListen.port(), service.nMaxInBounds);
        }
        else
        {
            ERROR("Failed to listen port %s:%u, disable in-bound connection",
                  service.epListen.address().to_string().c_str(), service.epListen.port());
            return false;
        }
    }

    for (const CNetHost& host : confNetwork.vecNode)
    {
        AddNewNode(host);
    }

    return true;
}

void CPeerNet::LeaveLoop()
{
    vector<CPeer*> vPeer;
    for (map<uint64, CPeer*>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
    {
        vPeer.push_back((*it).second);
    }

    for (CPeer* pPeer : vPeer)
    {
        RemovePeer(pPeer, CEndpointManager::HOST_CLOSE);
    }

    for (const CPeerService& service : confNetwork.vecService)
    {
        StopService(service.epListen);
    }

    epMngr.Clear();

    CIOProc::LeaveLoop();
}

void CPeerNet::HeartBeat()
{
    if (GetOutBoundIdleCount() != 0)
    {
        tcp::endpoint ep;
        if (epMngr.FetchOutBound(ep))
        {
            if (!Connect(ep, CONNECT_TIMEOUT))
            {
                epMngr.CloseEndpoint(ep, CEndpointManager::CONNECT_FAILURE);
            }
        }
    }
}

void CPeerNet::Timeout(uint64 nNonce, uint32 nTimerId)
{
    CPeer* pPeer = GetPeer(nNonce);
    if (pPeer != nullptr)
    {
        RemovePeer(pPeer, CEndpointManager::RESPONSE_FAILURE);
    }
}

std::size_t CPeerNet::GetMaxOutBoundCount()
{
    return confNetwork.nMaxOutBounds;
}

bool CPeerNet::ClientAccepted(const tcp::endpoint& epService, CIOClient* pClient)
{
    if (epMngr.AcceptInBound(pClient->GetRemote()))
    {
        CPeer* pPeer = AddNewPeer(pClient, true);
        if (pPeer != nullptr)
        {
            return true;
        }
        epMngr.CloseEndpoint(pClient->GetRemote(), CEndpointManager::HOST_CLOSE);
    }
    return false;
}

bool CPeerNet::ClientConnected(CIOClient* pClient)
{
    CPeer* pPeer = AddNewPeer(pClient, false);
    if (pPeer != nullptr)
    {
        localIP = pClient->GetLocal().address();
        return true;
    }

    epMngr.CloseEndpoint(pClient->GetRemote(), CEndpointManager::HOST_CLOSE);
    return false;
}

void CPeerNet::ClientFailToConnect(const tcp::endpoint& epRemote)
{
    epMngr.CloseEndpoint(epRemote, CEndpointManager::CONNECT_FAILURE);
}

void CPeerNet::HostResolved(const CNetHost& host, const tcp::endpoint& ep)
{
    if (host.strName == "toremove")
    {
        epMngr.RemoveOutBound(ep);
        CPeer* pPeer = GetPeer(ep);
        if (pPeer)
        {
            HandlePeerClose(pPeer);
        }
    }
    else
    {
        epMngr.AddNewOutBound(ep, host.strName, host.data);
    }
}

CPeer* CPeerNet::AddNewPeer(CIOClient* pClient, bool fInBound)
{
    uint64 nNonce;
    do
    {
        nNonce = CreateNonce(PEER_NONCE_TYPE);
    } while (mapPeer.count(nNonce));

    CPeer* pPeer = CreatePeer(pClient, nNonce, fInBound);
    if (pPeer != nullptr)
    {
        pPeer->Activate();
        mapPeer.insert(make_pair(nNonce, pPeer));
    }

    INFO("Add New Peer : %s %d", pPeer->GetRemote().address().to_string().c_str(),
         pPeer->GetRemote().port());

    return pPeer;
}

void CPeerNet::RemovePeer(CPeer* pPeer, const CEndpointManager::CloseReason& reason)
{
    WARN("Remove Peer (%d) : %s", reason,
         pPeer->GetRemote().address().to_string().c_str());

    epMngr.CloseEndpoint(pPeer->GetRemote(), reason);

    CancelClientTimers(pPeer->GetNonce());
    mapPeer.erase(pPeer->GetNonce());
    DestroyPeer(pPeer);
}

void CPeerNet::RewardPeer(CPeer* pPeer, const CEndpointManager::Bonus& bonus)
{
    INFO("Reward Peer (%d) : %s", bonus,
         pPeer->GetRemote().address().to_string().c_str());
    epMngr.RewardEndpoint(pPeer->GetRemote(), bonus);
}

CPeer* CPeerNet::GetPeer(uint64 nNonce)
{
    map<uint64, CPeer*>::iterator it = mapPeer.find(nNonce);
    if (it != mapPeer.end())
    {
        return (*it).second;
    }
    return nullptr;
}

CPeer* CPeerNet::GetPeer(const boost::asio::ip::tcp::endpoint& epNode)
{
    for (const auto& peer : mapPeer)
    {
        CPeer* pPeer = peer.second;
        if (pPeer->GetRemote() == epNode)
        {
            return pPeer;
        }
    }
    return nullptr;
}

void CPeerNet::AddNewNode(const CNetHost& host)
{
    const tcp::endpoint ep = host.ToEndPoint();
    if (ep != tcp::endpoint())
    {
        epMngr.AddNewOutBound(ep, host.strName, host.data);
    }
    else
    {
        ResolveHost(host);
    }
}

void CPeerNet::AddNewNode(const tcp::endpoint& epNode, const string& strName, const boost::any& data)
{
    epMngr.AddNewOutBound(epNode, strName, data);
}

void CPeerNet::RemoveNode(const CNetHost& host)
{
    const tcp::endpoint ep = host.ToEndPoint();
    if (ep != tcp::endpoint())
    {
        epMngr.RemoveOutBound(ep);
        CPeer* pPeer = GetPeer(ep);
        if (pPeer)
        {
            HandlePeerClose(pPeer);
        }
    }
    else
    {
        ResolveHost(CNetHost(host.strHost, host.nPort, "toremove"));
    }
}

void CPeerNet::RemoveNode(const tcp::endpoint& epNode)
{
    epMngr.RemoveOutBound(epNode);
}

void CPeerNet::AddProtocolInvalidPeer(CPeer* pPeer)
{
    setProtocolInvalidIP.insert(pPeer->GetRemote().address());
}

string CPeerNet::GetNodeName(const tcp::endpoint& epNode)
{
    return epMngr.GetOutBoundName(epNode);
}

bool CPeerNet::GetNodeData(const tcp::endpoint& epNode, boost::any& data)
{
    return epMngr.GetOutBoundData(epNode, data);
}

bool CPeerNet::SetNodeData(const tcp::endpoint& epNode, const boost::any& data)
{
    return epMngr.SetOutBoundData(epNode, data);
}

void CPeerNet::RetrieveGoodNode(vector<CNodeAvail>& vGoodNode, int64 nActiveTime, size_t nMaxCount)
{
    return epMngr.RetrieveGoodNode(vGoodNode, nActiveTime, nMaxCount);
}

std::string CPeerNet::GetLocalIP()
{
    if (IsRoutable(localIP))
    {
        return localIP.to_string();
    }
    return "0.0.0.0";
}

CPeer* CPeerNet::CreatePeer(CIOClient* pClient, uint64 nNonce, bool fInBound)
{
    return (new CPeer(this, pClient, nNonce, fInBound));
}

void CPeerNet::DestroyPeer(CPeer* pPeer)
{
    delete pPeer;
}

CPeerInfo* CPeerNet::GetPeerInfo(CPeer* pPeer, CPeerInfo* pInfo)
{
    if (pInfo == nullptr)
    {
        pInfo = new CPeerInfo();
    }

    if (pInfo != nullptr)
    {
        tcp::endpoint ep = pPeer->GetRemote();

        if (ep.address().is_v6())
        {
            pInfo->strAddress = string("[") + ep.address().to_string() + string("]:") + to_string(ep.port());
        }
        else
        {
            pInfo->strAddress = ep.address().to_string() + string(":") + to_string(ep.port());
        }
        pInfo->fInBound = pPeer->IsInBound();
        pInfo->nActive = pPeer->nTimeActive;
        pInfo->nLastRecv = pPeer->nTimeRecv;
        pInfo->nLastSend = pPeer->nTimeSend;

        pInfo->nScore = epMngr.GetEndpointScore(ep);
    }

    return pInfo;
}

void CPeerNet::HandlePeerGetIP(std::shared_ptr<CPeerNetGetIPMessage> getIPMsg)
{
    getIPMsg->ip.set_value(GetLocalIP());
}

void CPeerNet::HandlePeerGetCount(std::shared_ptr<CPeerNetGetCountMessage> getCountMsg)
{
    getCountMsg->count.set_value(mapPeer.size());
}

void CPeerNet::HandlePeerGetPeers(std::shared_ptr<CPeerNetGetPeersMessage> getPeersMsg)
{
    boost::ptr_vector<CPeerInfo> results;
    for (map<uint64, CPeer*>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
    {
        CPeerInfo* pInfo = GetPeerInfo((*it).second);
        if (pInfo)
        {
            results.push_back(pInfo);
        }
    }
    getPeersMsg->results.set_value(results);
}

void CPeerNet::HandlePeerAddNode(std::shared_ptr<CPeerNetAddNodeMessage> addNodeMsg)
{
    AddNewNode(addNodeMsg->host);
    addNodeMsg->fSuccess.set_value(true);
}

void CPeerNet::HandlePeerRemoveNode(std::shared_ptr<CPeerNetRemoveNodeMessage> removeNodeMsg)
{
    RemoveNode(removeNodeMsg->host);
    removeNodeMsg->fSuccess.set_value(true);
}

void CPeerNet::HandlePeerGetBanned(std::shared_ptr<CPeerNetGetBannedMessage> getBannedMsg)
{
    std::vector<CAddressBanned> bannedAddresses;
    epMngr.GetBanned(bannedAddresses);
    getBannedMsg->results.set_value(bannedAddresses);
}

void CPeerNet::HandlePeerSetBan(std::shared_ptr<CPeerNetSetBanMessage> setBanMsg)
{
    vector<boost::asio::ip::address> vAddrToBan;
    for (const string& strAddress : setBanMsg->addresses.first)
    {
        boost::system::error_code ec;
        boost::asio::ip::address addr = boost::asio::ip::address::from_string(strAddress, ec);
        if (!ec)
        {
            vAddrToBan.push_back(addr);
        }
    }
    epMngr.SetBan(vAddrToBan, setBanMsg->addresses.second);
    setBanMsg->count.set_value(vAddrToBan.size());
}

void CPeerNet::HandlePeerClrBanned(std::shared_ptr<CPeerNetClrBannedMessage> clrBannedMsg)
{
    vector<boost::asio::ip::address> vAddrToClear;
    for (const string& strAddress : clrBannedMsg->addresses)
    {
        boost::system::error_code ec;
        boost::asio::ip::address addr = boost::asio::ip::address::from_string(strAddress, ec);
        if (!ec)
        {
            vAddrToClear.push_back(addr);
        }
    }
    epMngr.ClearBanned(vAddrToClear);
    clrBannedMsg->count.set_value(vAddrToClear.size());
}

void CPeerNet::HandlePeerNetClose(const CPeerNetCloseMessage& netCloseMsg)
{
    CPeer* pPeer = GetPeer(netCloseMsg.nNonce);
    if (!pPeer)
    {
        return;
    }
    RemovePeer(pPeer, netCloseMsg.closeReason);
}

void CPeerNet::HandlePeerNetReward(const CPeerNetRewardMessage& netRewardMsg)
{
    CPeer* pPeer = GetPeer(netRewardMsg.nNonce);
    if (!pPeer)
    {
        return;
    }
    epMngr.RewardEndpoint(pPeer->GetRemote(), netRewardMsg.bonus);
}

} // namespace xengine
