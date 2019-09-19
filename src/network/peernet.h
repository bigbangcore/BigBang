// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NETWORK_PEERNET_H
#define NETWORK_PEERNET_H

#include "peerevent.h"
#include "proto.h"
#include "xengine.h"

namespace bigbang
{
namespace network
{

class INetChannel : public xengine::IIOModule, virtual public CBbPeerEventListener
{
public:
    INetChannel()
      : IIOModule("netchannel") {}
    virtual int GetPrimaryChainHeight() = 0;
    virtual bool IsForkSynchronized(const uint256& hashFork) const = 0;
    virtual void BroadcastBlockInv(const uint256& hashFork, const uint256& hashBlock) = 0;
    virtual void BroadcastTxInv(const uint256& hashFork) = 0;
    virtual void SubscribeFork(const uint256& hashFork, const uint64& nNonce) = 0;
    virtual void UnsubscribeFork(const uint256& hashFork) = 0;
};

class IDelegatedChannel : public xengine::IIOModule, virtual public CBbPeerEventListener
{
public:
    IDelegatedChannel()
      : IIOModule("delegatedchannel") {}
    virtual void PrimaryUpdate(int nStartHeight,
                               const std::vector<std::pair<int, std::map<CDestination, size_t>>>& vEnrolledWeight,
                               const std::map<CDestination, std::vector<unsigned char>>& mapDistributeData,
                               const std::map<CDestination, std::vector<unsigned char>>& mapPublishData)
        = 0;
};

class CBbPeerNet : public xengine::CPeerNet, virtual public CBbPeerEventListener
{
public:
    CBbPeerNet();
    ~CBbPeerNet();
    virtual void BuildHello(xengine::CPeer* pPeer, xengine::CBufStream& ssPayload);
    void HandlePeerWriten(xengine::CPeer* pPeer) override;
    virtual bool HandlePeerHandshaked(xengine::CPeer* pPeer, uint32 nTimerId);
    virtual bool HandlePeerRecvMessage(xengine::CPeer* pPeer, int nChannel, int nCommand,
                                       xengine::CBufStream& ssPayload);

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleEvent(CEventPeerSubscribe& eventSubscribe) override;
    bool HandleEvent(CEventPeerUnsubscribe& eventUnsubscribe) override;
    bool HandleEvent(CEventPeerInv& eventInv) override;
    bool HandleEvent(CEventPeerGetData& eventGetData) override;
    bool HandleEvent(CEventPeerGetBlocks& eventGetBlocks) override;
    bool HandleEvent(CEventPeerTx& eventTx) override;
    bool HandleEvent(CEventPeerBlock& eventBlock) override;
    bool HandleEvent(CEventPeerBulletin& eventBulletin) override;
    bool HandleEvent(CEventPeerGetDelegated& eventGetDelegated) override;
    bool HandleEvent(CEventPeerDistribute& eventDistribute) override;
    bool HandleEvent(CEventPeerPublish& eventPublish) override;
    xengine::CPeer* CreatePeer(xengine::CIOClient* pClient, uint64 nNonce, bool fInBound) override;
    void DestroyPeer(xengine::CPeer* pPeer) override;
    xengine::CPeerInfo* GetPeerInfo(xengine::CPeer* pPeer, xengine::CPeerInfo* pInfo) override;
    CAddress GetGateWayAddress(const CNetHost& gateWayAddr);
    bool SendDataMessage(uint64 nNonce, int nCommand, xengine::CBufStream& ssPayload);
    bool SendDelegatedMessage(uint64 nNonce, int nCommand, xengine::CBufStream& ssPayload);
    void SetInvTimer(uint64 nNonce, std::vector<CInv>& vInv);
    virtual void ProcessAskFor(xengine::CPeer* pPeer);
    void Configure(uint32 nMagicNumIn, uint32 nVersionIn, uint64 nServiceIn, const std::string& subVersionIn, bool fEnclosedIn)
    {
        nMagicNum = nMagicNumIn;
        nVersion = nVersionIn;
        nService = nServiceIn;
        subVersion = subVersionIn;
        fEnclosed = fEnclosedIn;
    }
    virtual bool CheckPeerVersion(uint32 nVersionIn, uint64 nServiceIn, const std::string& subVersionIn) = 0;

protected:
    INetChannel* pNetChannel;
    IDelegatedChannel* pDelegatedChannel;
    uint32 nMagicNum;
    uint32 nVersion;
    uint64 nService;
    bool fEnclosed;
    std::string subVersion;
    std::set<boost::asio::ip::tcp::endpoint> setDNSeed;
};

} // namespace network
} // namespace bigbang

#endif //NETWORK_PEERNET_H
