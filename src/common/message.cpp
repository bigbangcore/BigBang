// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "message.h"

INITIALIZE_MESSAGE_STATIC_VAR(CPeerBasicMessage, "BasicMessage");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerActiveMessage, "ActiveMessage");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerDeactiveMessage, "DeactiveMessage");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerSubscribeMessage, "SubscribeMessage");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerUnSubscribeMessage, "UnsubscribeMessage");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerGetBlocksMessage, "GetBlocksMessage");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerGetDataMessage, "GetDataMessage");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerInvMessage, "InvMessage");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerTxMessage, "TxMessage");
INITIALIZE_MESSAGE_STATIC_VAR(CPeerBlockMessage, "BlockMessage");