// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NETWORK_PEER_H
#define NETWORK_PEER_H

#include <boost/bind.hpp>

#include "proto.h"
#include "xengine.h"

namespace bigbang
{

namespace network
{

class CBbPeer : public xengine::CPeer
{
public:
    CBbPeer(xengine::CPeerNet* pPeerNetIn, xengine::CIOClient* pClientIn, uint64 nNonceIn,
            bool fInBoundIn, uint32 nMsgMagicIn, uint32 nHsTimerIdIn);
    ~CBbPeer();
    void Activate() override;
    bool IsHandshaked();
    bool SendMessage(int nChannel, int nCommand, xengine::CBufStream& ssPayload);
    bool SendMessage(int nChannel, int nCommand)
    {
        xengine::CBufStream ssPayload;
        return SendMessage(nChannel, nCommand, ssPayload);
    }
    uint32 Request(const CInv& inv, uint32 nTimerId);
    uint32 Responded(const CInv& inv);
    void AskFor(const uint256& hashFork, const std::vector<CInv>& vInv);
    bool FetchAskFor(uint256& hashFork, CInv& inv);
    bool PingTimer(uint32 nTimerId) override;

protected:
    void SendHello();
    void SendHelloAck();
    void SendPing();
    bool ParseMessageHeader();
    bool HandshakeReadHeader();
    bool HandshakeReadCompleted();
    virtual bool HandshakeCompleted();
    bool HandleReadHeader();
    bool HandleReadCompleted();

public:
    int nVersion;
    uint64 nService;
    uint64 nNonceFrom;
    int64 nTimeDelta;
    int64 nTimeHello;
    std::string strSubVer;
    int nStartingHeight;
    int nPingPongTimeDelta;

    uint32 nPingTimerId;
    int64 nPingMillisTime;
    uint32 nPingSeq;

protected:
    uint32 nMsgMagic;
    uint32 nHsTimerId;
    CPeerMessageHeader hdrRecv;

    std::map<CInv, uint32> mapRequest;
    std::queue<std::pair<uint256, CInv>> queAskFor;
};

class CBbPeerInfo : public xengine::CPeerInfo
{
public:
    int nVersion;
    uint64 nService;
    std::string strSubVer;
    int nStartingHeight;
    int nPingPongTimeDelta;
};

} // namespace network
} // namespace bigbang

#endif //NETWORK_PEER_H
