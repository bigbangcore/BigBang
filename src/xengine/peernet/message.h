// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_PEERNET_MESSAGE_H
#define XENGINE_PEERNET_MESSAGE_H

#include <boost/ptr_container/ptr_vector.hpp>
#include <future>

#include "epmngr.h"
#include "message/message.h"
#include "netio/nethost.h"
#include "peernet/peerinfo.h"

namespace xengine
{
struct CPeerNetCloseMessage : public CMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerNetCloseMessage);
    uint64 nNonce;
    CEndpointManager::CloseReason closeReason;
};

struct CPeerNetRewardMessage : public CMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerNetRewardMessage);
    uint64 nNonce;
    CEndpointManager::Bonus bonus;
};

////////////////////   PeerNet  ////////////////////////

// Message that contains std::promise must be consume once
struct CPeerNetGetIPMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerNetGetIPMessage);
    CPeerNetGetIPMessage(std::promise<std::string>&& ipIn)
      : ip(std::move(ipIn)), data(0) {}
    int data;
    std::promise<std::string> ip;
};

struct CPeerNetGetCountMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerNetGetCountMessage);
    CPeerNetGetCountMessage(std::promise<std::size_t>&& countIn)
      : count(std::move(countIn)), data(0) {}
    int data;
    std::promise<std::size_t> count;
};

struct CPeerNetGetPeersMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerNetGetPeersMessage);
    CPeerNetGetPeersMessage(std::promise<boost::ptr_vector<CPeerInfo>>&& resultsIn)
      : results(std::move(resultsIn)), data(0) {}
    int data;
    std::promise<boost::ptr_vector<CPeerInfo>> results;
};

struct CPeerNetAddNodeMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerNetAddNodeMessage);
    CPeerNetAddNodeMessage(std::promise<bool>&& fSuccessIn)
      : fSuccess(std::move(fSuccessIn)) {}
    CNetHost host;
    std::promise<bool> fSuccess;
};

struct CPeerNetRemoveNodeMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerNetRemoveNodeMessage);
    CPeerNetRemoveNodeMessage(std::promise<bool>&& fSuccessIn)
      : fSuccess(std::move(fSuccessIn)) {}
    CNetHost host;
    std::promise<bool> fSuccess;
};

struct CPeerNetGetBannedMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerNetGetBannedMessage);
    CPeerNetGetBannedMessage(std::promise<std::vector<CAddressBanned>>&& resultIn)
      : results(std::move(resultIn)), data(0) {}
    int data;
    std::promise<std::vector<CAddressBanned>> results;
};

typedef std::pair<std::vector<std::string>, int64> ADDRESSES_TO_BAN;

struct CPeerNetSetBanMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerNetSetBanMessage);
    CPeerNetSetBanMessage(std::promise<std::size_t>&& countIn)
      : count(std::move(countIn)) {}
    ADDRESSES_TO_BAN addresses;
    std::promise<std::size_t> count;
};

struct CPeerNetClrBannedMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerNetClrBannedMessage);
    CPeerNetClrBannedMessage(std::promise<std::size_t>&& countIn)
      : count(std::move(countIn)) {}
    std::vector<std::string> addresses;
    std::promise<std::size_t> count;
};

} // namespace xengine

#endif // XENGINE_PEERNET_MESSAGE_H