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

/////////// NetChannel /////////////

struct CPeerBasicMessage : public CMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerBasicMessage);
    uint64 nNonce;
    uint256 hashFork;
};

struct CPeerActiveMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerActiveMessage);
    CAddress address;
};

struct CPeerDeactiveMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerDeactiveMessage);
    CAddress address;
};

struct CPeerSubscribeMessageInBound : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerSubscribeMessageInBound);
    std::vector<uint256> vecForks;
};

struct CPeerSubscribeMessageOutBound : public CPeerSubscribeMessageInBound
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerSubscribeMessageOutBound);
};

struct CPeerUnsubscribeMessageInBound : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerUnsubscribeMessageInBound);
    std::vector<uint256> vecForks;
};

struct CPeerUnsubscribeMessageOutBound : public CPeerUnsubscribeMessageInBound
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerUnsubscribeMessageOutBound);
};

struct CPeerGetBlocksMessageInBound : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerGetBlocksMessageInBound);
    CBlockLocator blockLocator;
};

struct CPeerGetBlocksMessageOutBound : public CPeerGetBlocksMessageInBound
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerGetBlocksMessageOutBound);
};

struct CPeerGetDataMessageInBound : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerGetDataMessageInBound);
    std::vector<CInv> vecInv;
};

struct CPeerGetDataMessageOutBound : public CPeerGetDataMessageInBound
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerGetDataMessageOutBound);
};

struct CPeerInvMessageInBound : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerInvMessageInBound);
    std::vector<CInv> vecInv;
};

struct CPeerInvMessageOutBound : public CPeerInvMessageInBound
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerInvMessageOutBound);
};

struct CPeerTxMessageInBound : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerTxMessageInBound);
    CTransaction tx;
};

struct CPeerTxMessageOutBound : public CPeerTxMessageInBound
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerTxMessageOutBound);
};

struct CPeerBlockMessageInBound : public CPeerBasicMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerBlockMessageInBound);
    CBlock block;
};

struct CPeerBlockMessageOutBound : public CPeerBlockMessageInBound
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerBlockMessageOutBound);
};

#endif // COMMON_MESSAGE_H
