// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "peernet.h"

#include <boost/bind.hpp>
#include <openssl/rand.h>
#include <string>

#include "netio/nethost.h"
#include "util.h"

using namespace std;
using boost::asio::ip::tcp;

#define CONNECT_TIMEOUT 10

namespace xengine
{

///////////////////////////////
// CPeerNet

CPeerNet::CPeerNet(const string& ownKeyIn)
  : CIOProc(ownKeyIn), confNetwork{}
{
}

CPeerNet::~CPeerNet()
{
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

void CPeerNet::EnterLoop()
{
    for (const CPeerService& service : confNetwork.vecService)
    {
        if (StartService(service.epListen, service.nMaxInBounds))
        {
            Log("Listen port %s:%u, connections limit = %u\n",
                service.epListen.address().to_string().c_str(),
                service.epListen.port(), service.nMaxInBounds);
        }
        else
        {
            Error("Failed to listen port %s:%u, disable in-bound connection\n",
                  service.epListen.address().to_string().c_str(), service.epListen.port());
        }
    }

    for (const CNetHost& host : confNetwork.vecNode)
    {
        AddNewNode(host);
    }
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
        if (pPeer->PingTimer(nTimerId))
        {
            return;
        }
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
    RAND_bytes((unsigned char*)&nNonce, sizeof(nNonce));
    while (mapPeer.count(nNonce) || nNonce <= 0xFF)
    {
        RAND_bytes((unsigned char*)&nNonce, sizeof(nNonce));
    }

    CPeer* pPeer = CreatePeer(pClient, nNonce, fInBound);
    if (pPeer != nullptr)
    {
        pPeer->Activate();
        mapPeer.insert(make_pair(nNonce, pPeer));
    }

    Log("Add New Peer : %s %d\n", pPeer->GetRemote().address().to_string().c_str(),
        pPeer->GetRemote().port());

    return pPeer;
}

void CPeerNet::RemovePeer(CPeer* pPeer, const CEndpointManager::CloseReason& reason)
{
    Warn("Remove Peer (%d) : %s\n", reason,
         pPeer->GetRemote().address().to_string().c_str());

    epMngr.CloseEndpoint(pPeer->GetRemote(), reason);

    CancelClientTimers(pPeer->GetNonce());
    mapPeer.erase(pPeer->GetNonce());
    DestroyPeer(pPeer);
}

void CPeerNet::RewardPeer(CPeer* pPeer, const CEndpointManager::Bonus& bonus)
{
    Log("Reward Peer (%d) : %s\n", bonus,
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

bool CPeerNet::HandleEvent(CEventPeerNetGetIP& eventGetIP)
{
    eventGetIP.result = GetLocalIP();
    return true;
}

bool CPeerNet::HandleEvent(CEventPeerNetGetCount& eventGetCount)
{
    eventGetCount.result = mapPeer.size();
    return true;
}

bool CPeerNet::HandleEvent(CEventPeerNetGetPeers& eventGetPeers)
{
    eventGetPeers.result.reserve(mapPeer.size());
    for (map<uint64, CPeer*>::iterator it = mapPeer.begin(); it != mapPeer.end(); ++it)
    {
        CPeerInfo* pInfo = GetPeerInfo((*it).second);
        if (pInfo != nullptr)
        {
            eventGetPeers.result.push_back(pInfo);
        }
    }
    return true;
}

bool CPeerNet::HandleEvent(CEventPeerNetAddNode& eventAddNode)
{
    AddNewNode(eventAddNode.data);
    eventAddNode.result = true;
    return true;
}

bool CPeerNet::HandleEvent(CEventPeerNetRemoveNode& eventRemoveNode)
{
    RemoveNode(eventRemoveNode.data);
    eventRemoveNode.result = true;
    return true;
}

bool CPeerNet::HandleEvent(CEventPeerNetGetBanned& eventGetBanned)
{
    epMngr.GetBanned(eventGetBanned.result);
    return true;
}

bool CPeerNet::HandleEvent(CEventPeerNetSetBan& eventSetBan)
{
    vector<boost::asio::ip::address> vAddrToBan;
    for (const string& strAddress : eventSetBan.data.first)
    {
        boost::system::error_code ec;
        boost::asio::ip::address addr = boost::asio::ip::address::from_string(strAddress, ec);
        if (!ec)
        {
            vAddrToBan.push_back(addr);
        }
    }
    epMngr.SetBan(vAddrToBan, eventSetBan.data.second);
    eventSetBan.result = vAddrToBan.size();
    return true;
}

bool CPeerNet::HandleEvent(CEventPeerNetClrBanned& eventClrBanned)
{
    vector<boost::asio::ip::address> vAddrToClear;
    for (const string& strAddress : eventClrBanned.data)
    {
        boost::system::error_code ec;
        boost::asio::ip::address addr = boost::asio::ip::address::from_string(strAddress, ec);
        if (!ec)
        {
            vAddrToClear.push_back(addr);
        }
    }
    epMngr.ClearBanned(vAddrToClear);
    eventClrBanned.result = vAddrToClear.size();
    return true;
}

bool CPeerNet::HandleEvent(CEventPeerNetReward& eventReward)
{
    CPeer* pPeer = GetPeer(eventReward.nNonce);
    if (pPeer == nullptr)
    {
        return false;
    }
    epMngr.RewardEndpoint(pPeer->GetRemote(), eventReward.data);
    return true;
}

bool CPeerNet::HandleEvent(CEventPeerNetClose& eventClose)
{
    CPeer* pPeer = GetPeer(eventClose.nNonce);
    if (pPeer == nullptr)
    {
        return false;
    }
    RemovePeer(pPeer, eventClose.data);
    return true;
}

} // namespace xengine
