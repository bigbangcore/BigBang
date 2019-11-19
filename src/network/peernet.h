// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NETWORK_PEERNET_H
#define NETWORK_PEERNET_H

#include "actor/actor.h"
#include "message.h"
#include "peerevent.h"
#include "proto.h"
#include "xengine.h"

namespace bigbang
{
namespace network
{

class INetChannelController : public xengine::CActor
{
public:
    INetChannelController()
      : xengine::CActor("netchannelcontroller") {}
};

class IDelegatedChannel : public xengine::CActor
{
public:
    IDelegatedChannel()
      : xengine::CActor("delegatedchannel") {}
    virtual void PrimaryUpdate(int nStartHeight,
                               const std::vector<std::pair<uint256, std::map<CDestination, size_t>>>& vEnrolledWeight,
                               const std::map<CDestination, std::vector<unsigned char>>& mapDistributeData,
                               const std::map<CDestination, std::vector<unsigned char>>& mapPublishData) = 0;
};

class CBbPeerNet : public xengine::CPeerNet
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

    void HandleSubscribe(const std::shared_ptr<CPeerSubscribeMessageOutBound> spMsg);
    void HandleUnsubscribe(const std::shared_ptr<CPeerUnsubscribeMessageOutBound> spMsg);
    void HandleInv(const std::shared_ptr<CPeerInvMessageOutBound> spMsg);
    void HandleGetData(const std::shared_ptr<CPeerGetDataMessageOutBound> spMsg);
    void HandleGetBlocks(const std::shared_ptr<CPeerGetBlocksMessageOutBound> spMsg);
    void HandlePeerTx(const std::shared_ptr<CPeerTxMessageOutBound> spMsg);
    void HandlePeerBlock(const std::shared_ptr<CPeerBlockMessageOutBound> spMsg);

    void HandleBulletin(const std::shared_ptr<CPeerBulletinMessageOutBound> spMsg);
    void HandleGetDelegate(const std::shared_ptr<CPeerGetDelegatedMessageOutBound> spMsg);
    void HandleDistribute(const std::shared_ptr<CPeerDistributeMessageOutBound> spMsg);
    void HandlePublish(const std::shared_ptr<CPeerPublishMessageOutBound> spMsg);

    void HandlePrimaryChainHeightUpdate(const std::shared_ptr<CAddedBlockMessage> spMsg);

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
    uint32 nMagicNum;
    uint32 nVersion;
    uint64 nService;
    bool fEnclosed;
    std::string subVersion;
    std::set<boost::asio::ip::tcp::endpoint> setDNSeed;
    int nPrimaryChainHeight;
};

} // namespace network
} // namespace bigbang

#endif //NETWORK_PEERNET_H
