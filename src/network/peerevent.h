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
    EVENT_PEER_GETBIZFORKS,
    EVENT_PEER_BIZFORKS,
    EVENT_PEER_SUBSCRIBE,
    EVENT_PEER_UNSUBSCRIBE,
    EVENT_PEER_INV,
    EVENT_PEER_GETDATA,
    EVENT_PEER_GETBLOCKS,
    EVENT_PEER_TX,
    EVENT_PEER_BLOCK,
    EVENT_PEER_GETFAIL,
    EVENT_PEER_MSGRSP,
    EVENT_PEER_BULLETIN,
    EVENT_PEER_GETDELEGATED,
    EVENT_PEER_DISTRIBUTE,
    EVENT_PEER_PUBLISH,
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

template <int type, typename L, typename D>
class CEventPeerDelegated : public xengine::CEvent
{
    friend class xengine::CStream;

public:
    CEventPeerDelegated(uint64 nNonceIn, const int& hashAnchorIn)
      : CEvent(nNonceIn, type), hashAnchor(hashAnchorIn) {}
    virtual ~CEventPeerDelegated() {}
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
        s.Serialize(hashAnchor, opt);
        s.Serialize(data, opt);
    }

public:
    int hashAnchor;
    D data;
};

class CEventPeerDelegatedBulletin
{
    friend class xengine::CStream;

public:
    class CDelegatedBitmap
    {
    public:
        CDelegatedBitmap(const int& hashAnchorIn = uint64(0), uint64 bitmapIn = 0)
          : hashAnchor(hashAnchorIn), bitmap(bitmapIn)
        {
        }
        template <typename O>
        void Serialize(xengine::CStream& s, O& opt)
        {
            s.Serialize(hashAnchor, opt);
            s.Serialize(bitmap, opt);
        }

    public:
        int hashAnchor;
        uint64 bitmap;
    };

public:
    void AddBitmap(const int& hash, uint64 bitmap)
    {
        vBitmap.push_back(CDelegatedBitmap(hash, bitmap));
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(bmDistribute, opt);
        s.Serialize(bmPublish, opt);
        s.Serialize(vBitmap, opt);
    }

public:
    uint64 bmDistribute;
    uint64 bmPublish;
    std::vector<CDelegatedBitmap> vBitmap;
};

class CEventPeerDelegatedGetData
{
    friend class xengine::CStream;

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(nInvType, opt);
        s.Serialize(destDelegate, opt);
    }

public:
    uint32 nInvType;
    CDestination destDelegate;
};

class CEventPeerDelegatedData
{
    friend class xengine::CStream;

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(destDelegate, opt);
        s.Serialize(vchData, opt);
    }

public:
    CDestination destDelegate;
    std::vector<unsigned char> vchData;
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
typedef TYPE_PEEREVENT(EVENT_PEER_GETBIZFORKS, std::vector<uint256>) CEventPeerGetBizForks;
typedef TYPE_PEEREVENT(EVENT_PEER_BIZFORKS, CBizFork) CEventPeerBizForks;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_SUBSCRIBE, std::vector<uint256>) CEventPeerSubscribe;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_UNSUBSCRIBE, std::vector<uint256>) CEventPeerUnsubscribe;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_INV, std::vector<CInv>) CEventPeerInv;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_GETDATA, std::vector<CInv>) CEventPeerGetData;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_GETBLOCKS, CBlockLocator) CEventPeerGetBlocks;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_TX, CTransaction) CEventPeerTx;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_BLOCK, CBlock) CEventPeerBlock;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_GETFAIL, std::vector<CInv>) CEventPeerGetFail;
typedef TYPE_PEERDATAEVENT(EVENT_PEER_MSGRSP, CMsgRsp) CEventPeerMsgRsp;

typedef TYPE_PEERDELEGATEDEVENT(EVENT_PEER_BULLETIN, CEventPeerDelegatedBulletin) CEventPeerBulletin;
typedef TYPE_PEERDELEGATEDEVENT(EVENT_PEER_GETDELEGATED, CEventPeerDelegatedGetData) CEventPeerGetDelegated;
typedef TYPE_PEERDELEGATEDEVENT(EVENT_PEER_DISTRIBUTE, CEventPeerDelegatedData) CEventPeerDistribute;
typedef TYPE_PEERDELEGATEDEVENT(EVENT_PEER_PUBLISH, CEventPeerDelegatedData) CEventPeerPublish;

class CBbPeerEventListener : virtual public xengine::CEventListener
{
public:
    virtual ~CBbPeerEventListener() {}
    DECLARE_EVENTHANDLER(CEventPeerActive);
    DECLARE_EVENTHANDLER(CEventPeerDeactive);
    DECLARE_EVENTHANDLER(CEventPeerGetBizForks);
    DECLARE_EVENTHANDLER(CEventPeerBizForks);
    DECLARE_EVENTHANDLER(CEventPeerSubscribe);
    DECLARE_EVENTHANDLER(CEventPeerUnsubscribe);
    DECLARE_EVENTHANDLER(CEventPeerInv);
    DECLARE_EVENTHANDLER(CEventPeerGetData);
    DECLARE_EVENTHANDLER(CEventPeerGetBlocks);
    DECLARE_EVENTHANDLER(CEventPeerTx);
    DECLARE_EVENTHANDLER(CEventPeerBlock);
    DECLARE_EVENTHANDLER(CEventPeerGetFail);
    DECLARE_EVENTHANDLER(CEventPeerMsgRsp);
    DECLARE_EVENTHANDLER(CEventPeerBulletin);
    DECLARE_EVENTHANDLER(CEventPeerGetDelegated);
    DECLARE_EVENTHANDLER(CEventPeerDistribute);
    DECLARE_EVENTHANDLER(CEventPeerPublish);
};

} // namespace network
} // namespace bigbang

#endif // NETWORK_PEEREVENT_H
