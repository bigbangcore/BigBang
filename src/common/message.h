// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_MESSAGE_H
#define COMMON_MESSAGE_H

#include <uint256.h>
#include "message/message.h"
#include "block.h"
#include "proto.h"

using namespace xengine;
using namespace bigbang::network;

struct CPeerBasicMessage : public CMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerBasicMessage);
    uint64 nNonce;
    uint256 hashFork;
};

INITIALIZE_MESSAGE_TYPE(CPeerBasicMessage);

struct CPeerSubscribeMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerSubscribeMessage);
    std::vector<uint256> vecForks;
};

INITIALIZE_MESSAGE_TYPE(CPeerSubscribeMessage);

struct CPeerUnSubscribeMessage : public CPeerSubscribeMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerUnSubscribeMessage);
};

INITIALIZE_MESSAGE_TYPE(CPeerUnSubscribeMessage);

struct CPeerGetBlocksMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerGetBlocksMessage);
    CBlockLocator blockLocator;
};

INITIALIZE_MESSAGE_TYPE(CPeerGetBlocksMessage);

struct CPeerGetDataMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerGetDataMessage);
    std::vector<CInv> vecInv;
};

INITIALIZE_MESSAGE_TYPE(CPeerGetDataMessage);

struct CPeerInvMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerInvMessage);
    std::vector<CInv> vecInv;
};

INITIALIZE_MESSAGE_TYPE(CPeerInvMessage);

struct CPeerTxMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerTxMessage);
    CTransaction tx;
};

INITIALIZE_MESSAGE_TYPE(CPeerTxMessage);

struct CPeerBlockMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerBlockMessage);
    CBlock block;
};

INITIALIZE_MESSAGE_TYPE(CPeerBlockMessage);

#endif // COMMON_MESSAGE_H
