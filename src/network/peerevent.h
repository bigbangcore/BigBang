// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NETWORK_PEEREVENT_H
#define NETWORK_PEEREVENT_H

#include "block.h"
#include "proto.h"
#include "transaction.h"
#include "xengine.h"

using namespace xengine;

namespace bigbang
{
namespace network
{

enum
{
    EVENT_PEER_BASE = xengine::EVENT_USER_BASE,
    //PEER
    EVENT_PEER_ACTIVE,
    EVENT_PEER_DEACTIVE,
    EVENT_PEER_SUBSCRIBE,
    EVENT_PEER_UNSUBSCRIBE,
    EVENT_PEER_INV,
    EVENT_PEER_GETDATA,
    EVENT_PEER_GETBLOCKS,
    EVENT_PEER_TX,
    EVENT_PEER_BLOCK,
    EVENT_PEER_GETFAIL,
    EVENT_PEER_MSGRSP,
    EVENT_PEER_MAX,
};

template <int type, typename L, typename D>
class CEventPeerData : public xengine::CEvent
{
    friend class xengine::CStream;

public:
    CEventPeerData(uint64 nNonceIn, const uint256& hashForkIn)
      : CEvent(nNonceIn, type), hashFork(hashForkIn) {}
    virtual ~CEventPeerData() {}
    virtual bool Handle(xengine::CEventListener& listener)
    {
        try
        {
            return (dynamic_cast<L&>(listener)).HandleEvent(*this);
        }
        catch (std::bad_cast&)
        {
            return listener.HandleEvent(*this);
        }
        catch (std::exception& e)
        {
            StdError(__PRETTY_FUNCTION__, e.what());
        }
        return false;
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(hashFork, opt);
        s.Serialize(data, opt);
    }

public:
    uint256 hashFork;
    D data;
};

class CBbPeerEventListener;

#define TYPE_PEEREVENT(type, body) \
    xengine::CEventCategory<type, CBbPeerEventListener, body, bool>

#define TYPE_PEERDATAEVENT(type, body) \
    CEventPeerData<type, CBbPeerEventListener, body>

#define TYPE_PEERDELEGATEDEVENT(type, body) \
    CEventPeerDelegated<type, CBbPeerEventListener, body>

typedef TYPE_PEEREVENT(EVENT_PEER_ACTIVE, CAddress) CEventPeerActive;
typedef TYPE_PEEREVENT(EVENT_PEER_DEACTIVE, CAddress) CEventPeerDeactive;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_SUBSCRIBE, std::vector<uint256>) CEventPeerSubscribe;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_UNSUBSCRIBE, std::vector<uint256>) CEventPeerUnsubscribe;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_INV, std::vector<CInv>) CEventPeerInv;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_GETDATA, std::vector<CInv>) CEventPeerGetData;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_GETBLOCKS, CBlockLocator) CEventPeerGetBlocks;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_TX, CTransaction) CEventPeerTx;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_BLOCK, CBlock) CEventPeerBlock;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_GETFAIL, std::vector<CInv>) CEventPeerGetFail;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_MSGRSP, CMsgRsp) CEventPeerMsgRsp;

class CBbPeerEventListener : virtual public xengine::CEventListener
{
public:
    virtual ~CBbPeerEventListener() {}
    DECLARE_EVENTHANDLER(CEventPeerActive);
    DECLARE_EVENTHANDLER(CEventPeerDeactive);
    DECLARE_EVENTHANDLER(CEventPeerSubscribe);
    DECLARE_EVENTHANDLER(CEventPeerUnsubscribe);
    DECLARE_EVENTHANDLER(CEventPeerInv);
    DECLARE_EVENTHANDLER(CEventPeerGetData);
    DECLARE_EVENTHANDLER(CEventPeerGetBlocks);
    DECLARE_EVENTHANDLER(CEventPeerTx);
    DECLARE_EVENTHANDLER(CEventPeerBlock);
    DECLARE_EVENTHANDLER(CEventPeerGetFail);
    DECLARE_EVENTHANDLER(CEventPeerMsgRsp);
};

} // namespace network
} // namespace bigbang

#endif // NETWORK_PEEREVENT_H
