// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NETWORK_PROTO_H
#define NETWORK_PROTO_H

#include "crc24q.h"
#include "uint256.h"
#include "xengine.h"

namespace bigbang
{

namespace network
{

enum
{
    NODE_NETWORK = (1 << 0),
    NODE_DELEGATED = (1 << 1),
};

enum
{
    PROTO_CHN_NETWORK = 0,
    //PROTO_CHN_DELEGATE = 1,
    PROTO_CHN_DATA = 2,
    PROTO_CHN_USER = 3,
};

enum
{
    PROTO_CMD_HELLO = 1,
    PROTO_CMD_HELLO_ACK = 2,
    PROTO_CMD_GETADDRESS = 3,
    PROTO_CMD_ADDRESS = 4,
    PROTO_CMD_PING = 5,
    PROTO_CMD_PONG = 6,
};

enum
{
    PROTO_CMD_SUBSCRIBE = 1,
    PROTO_CMD_UNSUBSCRIBE = 2,
    PROTO_CMD_GETBLOCKS = 3,
    PROTO_CMD_GETDATA = 4,
    PROTO_CMD_INV = 5,
    PROTO_CMD_TX = 6,
    PROTO_CMD_BLOCK = 7,
    PROTO_CMD_GETFAIL = 8,
    PROTO_CMD_MSGRSP = 9,
};

#define MESSAGE_HEADER_SIZE 16
#define MESSAGE_PAYLOAD_MAX_SIZE 0x400000
#define PING_TIMER_DURATION 120

class CPeerMessageHeader
{
    friend class xengine::CStream;

public:
    uint32 nMagic;
    uint8 nType;
    uint32 nPayloadSize;
    uint32 nPayloadChecksum;
    uint32 nHeaderChecksum;

public:
    int GetChannel() const
    {
        return (nType >> 6);
    }
    int GetCommand() const
    {
        return (nType & 0x3F);
    }
    uint32 GetHeaderChecksum() const
    {
        unsigned char buf[MESSAGE_HEADER_SIZE];
        *(uint32*)&buf[0] = nMagic;
        *(uint8*)&buf[4] = nType;
        *(uint32*)&buf[5] = nPayloadSize;
        *(uint32*)&buf[9] = nPayloadChecksum;
        return bigbang::crypto::crc24q(buf, 13);
    }
    bool Verify() const
    {
        return (nPayloadSize <= MESSAGE_PAYLOAD_MAX_SIZE && nHeaderChecksum == GetHeaderChecksum());
    }
    static uint8 GetMessageType(int nChannel, int nCommand)
    {
        return ((nChannel << 6) | (nCommand & 0x3F));
    }

protected:
    void Serialize(xengine::CStream& s, xengine::SaveType&)
    {
        char buf[MESSAGE_HEADER_SIZE + 1];
        *(uint32*)&buf[0] = nMagic;
        *(uint8*)&buf[4] = nType;
        *(uint32*)&buf[5] = nPayloadSize;
        *(uint32*)&buf[9] = nPayloadChecksum;
        *(uint32*)&buf[13] = nHeaderChecksum;
        s.Write(buf, MESSAGE_HEADER_SIZE);
    }
    void Serialize(xengine::CStream& s, xengine::LoadType&)
    {
        char buf[MESSAGE_HEADER_SIZE + 1];
        s.Read(buf, MESSAGE_HEADER_SIZE);
        nMagic = *(uint32*)&buf[0];
        nType = *(uint8*)&buf[4];
        nPayloadSize = *(uint32*)&buf[5];
        nPayloadChecksum = *(uint32*)&buf[9];
        nHeaderChecksum = *(uint32*)&buf[13] & 0xFFFFFF;
    }
    void Serialize(xengine::CStream& s, std::size_t& serSize)
    {
        (void)s;
        serSize += MESSAGE_HEADER_SIZE;
    }
};

class CMsgRsp
{
    friend class xengine::CStream;

public:
    CMsgRsp()
      : nReqMsgType(0), nRspResult(0), nReqMsgSubType(0) {}
    CMsgRsp(uint32 nType, uint32 nSubType, uint64 nResult)
      : nReqMsgType(nType), nReqMsgSubType(nSubType), nRspResult(nResult) {}
    virtual ~CMsgRsp() {}

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(nReqMsgType, opt);
        s.Serialize(nReqMsgSubType, opt);
        s.Serialize(nRspResult, opt);
    }

public:
    uint32 nReqMsgType;
    uint32 nReqMsgSubType;
    uint64 nRspResult;
};

class CInv
{
    friend class xengine::CStream;

public:
    enum
    {
        MSG_ERROR = 0,
        MSG_TX,
        MSG_BLOCK
    };
    enum
    {
        MAX_INV_COUNT = 1024 * 8,
        MIN_INV_COUNT = 1024
    };
    CInv() {}
    CInv(uint32 nTypeIn, const uint256& nHashIn)
      : nType(nTypeIn), nHash(nHashIn) {}
    friend bool operator<(const CInv& a, const CInv& b)
    {
        return (a.nType < b.nType || (a.nType == b.nType && a.nHash < b.nHash));
    }
    friend bool operator==(const CInv& a, const CInv& b)
    {
        return (a.nType == b.nType && a.nHash == b.nHash);
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(nType, opt);
        s.Serialize(nHash, opt);
    }

public:
    uint32 nType;
    uint256 nHash;
};

class CEndpoint : public xengine::CBinary
{
public:
    enum
    {
        BINSIZE = 18
    };
    CEndpoint();
    CEndpoint(const boost::asio::ip::tcp::endpoint& ep);
    CEndpoint(const CEndpoint& other);
    void GetEndpoint(boost::asio::ip::tcp::endpoint& ep);
    void CopyTo(unsigned char* ssTo) const;
    bool IsRoutable();
    const CEndpoint& operator=(const CEndpoint& other)
    {
        other.CopyTo(ss);
        return (*this);
    }

protected:
    unsigned char ss[BINSIZE];
};

class CAddress
{
    friend class xengine::CStream;

public:
    CAddress() {}
    CAddress(uint64 nServiceIn, const boost::asio::ip::tcp::endpoint& ep)
      : nService(nServiceIn), ssEndpoint(ep) {}
    CAddress(uint64 nServiceIn, const CEndpoint& ssEndpointIn)
      : nService(nServiceIn), ssEndpoint(ssEndpointIn) {}

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(nService, opt);
        s.Serialize(ssEndpoint, opt);
    }

public:
    uint64 nService;
    CEndpoint ssEndpoint;
};

} // namespace network
} // namespace bigbang

#endif // NETWORK_PROTO_H
