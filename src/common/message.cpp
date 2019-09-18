// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "message.h"

INITIALIZE_MESSAGE_STATIC_VAR(CPeerBasicMessage, "BasicMessage");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerActiveMessage, "ActiveMessage");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerDeactiveMessage, "DeactiveMessageInBound");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerSubscribeMessageInBound, "SubscribeMessageInBound");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerUnsubscribeMessageInBound, "UnsubscribeMessageInBound");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerGetBlocksMessageInBound, "GetBlocksMessageInBound");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerGetDataMessageInBound, "GetDataMessageInBound");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerInvMessageInBound, "InvMessageInBound");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerTxMessageInBound, "TxMessageInBound");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerBlockMessageInBound, "BlockMessageInBound");

INITIALIZE_MESSAGE_STATIC_VAR(CPeerSubscribeMessageOutBound, "SubscribeMessageOutBound");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerUnsubscribeMessageOutBound, "UnsubscribeMessageOutBound");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerGetBlocksMessageOutBound, "GetBlocksMessageOutBound");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerGetDataMessageOutBound, "GetDataMessageOutBound");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerInvMessageOutBound, "InvMessageOutBound");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerTxMessageOutBound, "TxMessageOutBound");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerBlockMessageOutBound, "BlockMessageOutBound");