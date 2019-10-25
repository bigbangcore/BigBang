// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "peer.h"

#include "crypto.h"
#include "peernet.h"

using namespace std;
using namespace xengine;

namespace bigbang
{
namespace network
{

//////////////////////////////
// CBbPeer

CBbPeer::CBbPeer(CPeerNet* pPeerNetIn, CIOClient* pClientIn, uint64 nNonceIn,
                 bool fInBoundIn, uint32 nMsgMagicIn, uint32 nHsTimerIdIn)
  : CPeer(pPeerNetIn, pClientIn, nNonceIn, fInBoundIn), nMsgMagic(nMsgMagicIn), nHsTimerId(nHsTimerIdIn)
{
}

CBbPeer::~CBbPeer()
{
}

void CBbPeer::Activate()
{
    CPeer::Activate();

    nVersion = 0;
    nService = 0;
    nNonceFrom = 0;
    nTimeDelta = 0;
    nTimeHello = 0;
    strSubVer.clear();
    nStartingHeight = 0;

    Read(MESSAGE_HEADER_SIZE, boost::bind(&CBbPeer::HandshakeReadHeader, this));
    if (!fInBound)
    {
        SendHello();
    }
}

bool CBbPeer::IsHandshaked()
{
    return (nHsTimerId == 0);
}

bool CBbPeer::SendMessage(int nChannel, int nCommand, CBufStream& ssPayload)
{
    CPeerMessageHeader hdrSend;
    hdrSend.nMagic = nMsgMagic;
    hdrSend.nType = CPeerMessageHeader::GetMessageType(nChannel, nCommand);
    hdrSend.nPayloadSize = ssPayload.GetSize();
    hdrSend.nPayloadChecksum = bigbang::crypto::CryptoHash(ssPayload.GetData(), ssPayload.GetSize()).Get32();
    hdrSend.nHeaderChecksum = hdrSend.GetHeaderChecksum();

    if (!hdrSend.Verify())
    {
        return false;
    }

    WriteStream() << hdrSend << ssPayload;
    Write();
    return true;
}

uint32 CBbPeer::Request(CInv& inv, uint32 nTimerId)
{
    uint32 nPrevTimerId = 0;
    map<CInv, uint32>::iterator it = mapRequest.find(inv);
    if (it != mapRequest.end())
    {
        nPrevTimerId = (*it).second;
        (*it).second = nTimerId;
    }
    else
    {
        mapRequest.insert(make_pair(inv, nTimerId));
    }
    return nPrevTimerId;
}

uint32 CBbPeer::Responded(CInv& inv)
{
    uint32 nTimerId = 0;
    std::map<CInv, uint32>::iterator it = mapRequest.find(inv);
    if (it != mapRequest.end())
    {
        nTimerId = (*it).second;
        mapRequest.erase(it);
    }
    return nTimerId;
}

void CBbPeer::AskFor(const uint256& hashFork, const vector<CInv>& vInv)
{
    for (const CInv& inv : vInv)
    {
        queAskFor.push(make_pair(hashFork, inv));
    }
}

bool CBbPeer::FetchAskFor(uint256& hashFork, CInv& inv)
{
    if (!queAskFor.empty())
    {
        hashFork = queAskFor.front().first;
        inv = queAskFor.front().second;
        queAskFor.pop();
        return true;
    }
    return false;
}

void CBbPeer::SendHello()
{
    CBufStream ssPayload;
    (static_cast<CBbPeerNet*>(pPeerNet))->BuildHello(this, ssPayload);
    SendMessage(PROTO_CHN_NETWORK, PROTO_CMD_HELLO, ssPayload);
    nTimeHello = GetTime();
}

void CBbPeer::SendHelloAck()
{
    SendMessage(PROTO_CHN_NETWORK, PROTO_CMD_HELLO_ACK);
}

bool CBbPeer::ParseMessageHeader()
{
    try
    {
        ReadStream() >> hdrRecv;
        return (hdrRecv.nMagic == nMsgMagic && hdrRecv.Verify());
    }
    catch (exception& e)
    {
        LOG_ERROR("CBbPeer", "Parse message header error: %s", e.what());
    }
    return false;
}

bool CBbPeer::HandshakeReadHeader()
{
    if (!ParseMessageHeader())
    {
        return false;
    }

    if (hdrRecv.nPayloadSize != 0)
    {
        Read(hdrRecv.nPayloadSize, boost::bind(&CBbPeer::HandshakeReadCompleted, this));
        return true;
    }
    return HandshakeReadCompleted();
}

bool CBbPeer::HandleReadHeader()
{
    if (!ParseMessageHeader())
    {
        return false;
    }

    if (hdrRecv.nPayloadSize != 0)
    {
        Read(hdrRecv.nPayloadSize, boost::bind(&CBbPeer::HandleReadCompleted, this));
        return true;
    }
    return HandleReadCompleted();
}

bool CBbPeer::HandshakeReadCompleted()
{
    CBufStream& ss = ReadStream();
    uint256 hash = bigbang::crypto::CryptoHash(ss.GetData(), ss.GetSize());
    if (hdrRecv.nPayloadChecksum == hash.Get32() && hdrRecv.GetChannel() == PROTO_CHN_NETWORK)
    {
        int64 nTimeRecv = GetTime();
        int nCmd = hdrRecv.GetCommand();
        try
        {
            if (nCmd == PROTO_CMD_HELLO)
            {
                if (nVersion != 0)
                {
                    return false;
                }
                int64 nTime;
                ss >> nVersion >> nService >> nTime >> nNonceFrom >> strSubVer >> nStartingHeight;
                nTimeDelta = nTime - nTimeRecv;
                if (!fInBound)
                {
                    nTimeDelta += (nTimeRecv - nTimeHello) / 2;
                    SendHelloAck();
                    return HandshakeCompleted();
                }
                SendHello();
                Read(MESSAGE_HEADER_SIZE, boost::bind(&CBbPeer::HandshakeReadHeader, this));
                return true;
            }
            else if (nCmd == PROTO_CMD_HELLO_ACK && fInBound)
            {
                if (nVersion == 0)
                {
                    return false;
                }
                nTimeDelta += (nTimeRecv - nTimeHello) / 2;
                return HandshakeCompleted();
            }
        }
        catch (exception& e)
        {
            LOG_ERROR("CBbPeer", "Handshake completed error: %s", e.what());
        }
    }
    return false;
}

bool CBbPeer::HandshakeCompleted()
{
    if (!(static_cast<CBbPeerNet*>(pPeerNet))->HandlePeerHandshaked(this, nHsTimerId))
    {
        return false;
    }
    nHsTimerId = 0;
    Read(MESSAGE_HEADER_SIZE, boost::bind(&CBbPeer::HandleReadHeader, this));
    return true;
}

bool CBbPeer::HandleReadCompleted()
{
    CBufStream& ss = ReadStream();
    uint256 hash = bigbang::crypto::CryptoHash(ss.GetData(), ss.GetSize());
    if (hdrRecv.nPayloadChecksum == hash.Get32())
    {
        try
        {
            if ((static_cast<CBbPeerNet*>(pPeerNet))->HandlePeerRecvMessage(this, hdrRecv.GetChannel(), hdrRecv.GetCommand(), ss))
            {
                Read(MESSAGE_HEADER_SIZE, boost::bind(&CBbPeer::HandleReadHeader, this));
                return true;
            }
        }
        catch (exception& e)
        {
            LOG_ERROR("CBbPeer", "Read completed error: %s", e.what());
        }
    }
    return false;
}

} // namespace network
} // namespace bigbang
