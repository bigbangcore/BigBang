// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "message.h"

INITIALIZE_MESSAGE_TYPE(CPeerBasicMessage);
INITIALIZE_MESSAGE_TYPE(CPeerActiveMessage);
INITIALIZE_MESSAGE_TYPE(CPeerDeactiveMessage);
INITIALIZE_MESSAGE_TYPE(CPeerSubscribeMessage);
INITIALIZE_MESSAGE_TYPE(CPeerUnSubscribeMessage);
INITIALIZE_MESSAGE_TYPE(CPeerGetBlocksMessage);
INITIALIZE_MESSAGE_TYPE(CPeerGetDataMessage);
INITIALIZE_MESSAGE_TYPE(CPeerInvMessage);
INITIALIZE_MESSAGE_TYPE(CPeerTxMessage);
INITIALIZE_MESSAGE_TYPE(CPeerBlockMessage);