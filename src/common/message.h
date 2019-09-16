// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_MESSAGE_H
#define COMMON_MESSAGE_H

#include <uint256.h>

#include "../network/proto.h"
#include "block.h"
#include "message/message.h"

using namespace xengine;
using namespace bigbang::network;

struct CPeerBasicMessage : public CMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerBasicMessage);
    uint64 nNonce;
    uint256 hashFork;
};

struct CPeerActiveMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerActiveMessage);
    uint64 nService;
    CAddress address;
};

struct CPeerDeactiveMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerDeactiveMessage);
    uint64 nService;
    CAddress address;
};

struct CPeerSubscribeMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerSubscribeMessage);
    std::vector<uint256> vecForks;
};

struct CPeerUnSubscribeMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerUnSubscribeMessage);
    std::vector<uint256> vecForks;
};

struct CPeerGetBlocksMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerGetBlocksMessage);
    CBlockLocator blockLocator;
};

struct CPeerGetDataMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerGetDataMessage);
    std::vector<CInv> vecInv;
};

struct CPeerInvMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerInvMessage);
    std::vector<CInv> vecInv;
};

struct CPeerTxMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerTxMessage);
    CTransaction tx;
};

struct CPeerBlockMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerBlockMessage);
    CBlock block;
};

#endif // COMMON_MESSAGE_H
