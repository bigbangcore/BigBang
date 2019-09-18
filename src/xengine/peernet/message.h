// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_PEERNET_MESSAGE_H
#define XENGINE_PEERNET_MESSAGE_H

#include "epmngr.h"
#include "message/message.h"

namespace xengine
{
struct CPeerNetCloseMessage : public CMessage
{
    GENERATE_MESSAGE_VIRTUAL_FUNCTION(CPeerNetCloseMessage);
    uint64 nNonce;
    CEndpointManager::CloseReason closeReason;
};
} // namespace xengine

#endif // XENGINE_PEERNET_MESSAGE_H