// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_PEERNET_PEER_EVENT_H
#define XENGINE_PEERNET_PEER_EVENT_H

#include <boost/ptr_container/ptr_vector.hpp>

#include "event/event.h"
#include "netio/nethost.h"
#include "peernet/epmngr.h"
#include "peernet/peerinfo.h"

namespace xengine
{

enum
{
    EVENT_PEERNET_IDLE = EVENT_PEER_BASE,
    EVENT_PEERNET_GETIP,
    EVENT_PEERNET_GETCOUNT,
    EVENT_PEERNET_GETPEERS,
    EVENT_PEERNET_ADDNODE,
    EVENT_PEERNET_REMOVENODE,
    EVENT_PEERNET_GETBANNED,
    EVENT_PEERNET_SETBAN,
    EVENT_PEERNET_CLRBANNED,
    EVENT_PEERNET_REWARD,
    EVENT_PEERNET_CLOSE
};

class CPeerEventListener;

#define TYPE_PEERNETEVENT(type, body, res) \
    CEventCategory<type, CPeerEventListener, body, res>

typedef std::pair<std::vector<std::string>, int64> ADDRESSES_TO_BAN;

typedef TYPE_PEERNETEVENT(EVENT_PEERNET_GETIP, int, std::string) CEventPeerNetGetIP;
typedef TYPE_PEERNETEVENT(EVENT_PEERNET_GETCOUNT, int, std::size_t) CEventPeerNetGetCount;
typedef TYPE_PEERNETEVENT(EVENT_PEERNET_GETPEERS, int, boost::ptr_vector<CPeerInfo>) CEventPeerNetGetPeers;
typedef TYPE_PEERNETEVENT(EVENT_PEERNET_ADDNODE, CNetHost, bool) CEventPeerNetAddNode;
typedef TYPE_PEERNETEVENT(EVENT_PEERNET_REMOVENODE, CNetHost, bool) CEventPeerNetRemoveNode;
typedef TYPE_PEERNETEVENT(EVENT_PEERNET_GETBANNED, int, std::vector<CAddressBanned>) CEventPeerNetGetBanned;
typedef TYPE_PEERNETEVENT(EVENT_PEERNET_SETBAN, ADDRESSES_TO_BAN, std::size_t) CEventPeerNetSetBan;
typedef TYPE_PEERNETEVENT(EVENT_PEERNET_CLRBANNED, std::vector<std::string>, std::size_t) CEventPeerNetClrBanned;
typedef TYPE_PEERNETEVENT(EVENT_PEERNET_REWARD, CEndpointManager::Bonus, bool) CEventPeerNetReward;
typedef TYPE_PEERNETEVENT(EVENT_PEERNET_CLOSE, CEndpointManager::CloseReason, bool) CEventPeerNetClose;

class CPeerEventListener : virtual public CEventListener
{
public:
    virtual ~CPeerEventListener() {}
    DECLARE_EVENTHANDLER(CEventPeerNetGetIP);
    DECLARE_EVENTHANDLER(CEventPeerNetGetCount);
    DECLARE_EVENTHANDLER(CEventPeerNetGetPeers);
    DECLARE_EVENTHANDLER(CEventPeerNetAddNode);
    DECLARE_EVENTHANDLER(CEventPeerNetRemoveNode);
    DECLARE_EVENTHANDLER(CEventPeerNetGetBanned);
    DECLARE_EVENTHANDLER(CEventPeerNetSetBan);
    DECLARE_EVENTHANDLER(CEventPeerNetClrBanned);
    DECLARE_EVENTHANDLER(CEventPeerNetReward);
    DECLARE_EVENTHANDLER(CEventPeerNetClose);
};

} // namespace xengine

#endif //XENGINE_PEERNET_PEER_EVENT_H
