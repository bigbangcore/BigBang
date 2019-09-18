// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_PEERNET_PEERNET_H
#define XENGINE_PEERNET_PEERNET_H

#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "message/actor.h"
#include "netio/ioproc.h"
#include "peernet/epmngr.h"
#include "peernet/peer.h"
#include "peernet/peerevent.h"
#include "peernet/peerinfo.h"

namespace xengine
{

class CPeerService
{
public:
    CPeerService(const boost::asio::ip::tcp::endpoint& epListenIn, std::size_t nMaxInBoundsIn)
      : epListen(epListenIn), nMaxInBounds(nMaxInBoundsIn)
    {
    }

public:
    boost::asio::ip::tcp::endpoint epListen;
    std::size_t nMaxInBounds;
};

class CPeerNetConfig
{
public:
    std::vector<CPeerService> vecService;
    std::vector<CNetHost> vecNode;
    CNetHost gateWayAddr;
    std::size_t nMaxOutBounds;
    unsigned short nPortDefault;
};

class CPeerNet : public CIOProc, virtual public CPeerEventListener
{
public:
    CPeerNet(const std::string& ownKeyIn);
    ~CPeerNet();
    void ConfigNetwork(CPeerNetConfig& config);
    void HandlePeerClose(CPeer* pPeer);
    void HandlePeerViolate(CPeer* pPeer);
    void HandlePeerError(CPeer* pPeer);
    virtual void HandlePeerWriten(CPeer* pPeer);

protected:
    void EnterLoop() override;
    void LeaveLoop() override;
    void HeartBeat() override;
    void Timeout(uint64 nNonce, uint32 nTimerId) override;
    std::size_t GetMaxOutBoundCount() override;
    bool ClientAccepted(const boost::asio::ip::tcp::endpoint& epService, CIOClient* pClient) override;
    bool ClientConnected(CIOClient* pClient) override;
    void ClientFailToConnect(const boost::asio::ip::tcp::endpoint& epRemote) override;
    void HostResolved(const CNetHost& host, const boost::asio::ip::tcp::endpoint& ep) override;
    CPeer* AddNewPeer(CIOClient* pClient, bool fInBound);
    void RewardPeer(CPeer* pPeer, const CEndpointManager::Bonus& bonus);
    void RemovePeer(CPeer* pPeer, const CEndpointManager::CloseReason& reason);
    CPeer* GetPeer(uint64 nNonce);
    CPeer* GetPeer(const boost::asio::ip::tcp::endpoint& epNode);
    void AddNewNode(const CNetHost& host);
    void AddNewNode(const boost::asio::ip::tcp::endpoint& epNode,
                    const std::string& strName = "", const boost::any& data = boost::any());
    void RemoveNode(const CNetHost& host);
    void RemoveNode(const boost::asio::ip::tcp::endpoint& epNode);
    void AddProtocolInvalidPeer(CPeer* pPeer);
    std::string GetNodeName(const boost::asio::ip::tcp::endpoint& epNode);
    bool GetNodeData(const boost::asio::ip::tcp::endpoint& epNode, boost::any& data);
    bool SetNodeData(const boost::asio::ip::tcp::endpoint& epNode, const boost::any& data);
    void RetrieveGoodNode(std::vector<CNodeAvail>& vGoodNode, int64 nActiveTime, std::size_t nMaxCount);
    virtual std::string GetLocalIP();
    virtual CPeer* CreatePeer(CIOClient* pClient, uint64 nNonce, bool fInBound);
    virtual void DestroyPeer(CPeer* pPeer);
    virtual CPeerInfo* GetPeerInfo(CPeer* pPeer, CPeerInfo* pInfo = nullptr);
    bool HandleEvent(CEventPeerNetGetIP& eventGetIP) override;
    bool HandleEvent(CEventPeerNetGetCount& eventGetCount) override;
    bool HandleEvent(CEventPeerNetGetPeers& eventGetPeers) override;
    bool HandleEvent(CEventPeerNetAddNode& eventAddNode) override;
    bool HandleEvent(CEventPeerNetRemoveNode& eventRemoveNode) override;
    bool HandleEvent(CEventPeerNetGetBanned& eventGetBanned) override;
    bool HandleEvent(CEventPeerNetSetBan& eventSetBan) override;
    bool HandleEvent(CEventPeerNetClrBanned& eventClrBanned) override;
    bool HandleEvent(CEventPeerNetReward& eventReward) override;
    bool HandleEvent(CEventPeerNetClose& eventClose) override;
    int GetCandidateNodeCount()
    {
        return epMngr.GetCandidateNodeCount();
    }

protected:
    CPeerNetConfig confNetwork;
    boost::asio::ip::address localIP;
    std::set<boost::asio::ip::address> setProtocolInvalidIP;

private:
    CEndpointManager epMngr;
    std::map<uint64, CPeer*> mapPeer;
};

} // namespace xengine

#endif //XENGINE_PEERNET_PEERNET_H
