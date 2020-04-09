// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_EVENT_H
#define BIGBANG_EVENT_H

#include <map>
#include <set>
#include <vector>

#include "dbptype.h"
#include "block.h"
#include "peerevent.h"
#include "struct.h"
#include "transaction.h"
#include "xengine.h"

namespace bigbang
{

enum
{
    EVENT_BASE = network::EVENT_PEER_MAX,
    EVENT_BLOCKMAKER_UPDATE,
    EVENT_BLOCKMAKER_ENROLL,
    EVENT_BLOCKMAKER_DISTRIBUTE,
    EVENT_BLOCKMAKER_PUBLISH,
    EVENT_BLOCKMAKER_AGREE,
    BB_EVENT_DBP_SOCKET_ADD_NEW_BLOCK,
    BB_EVENT_DBP_SOCKET_ADD_NEW_TX,

    BB_EVENT_DBP_REQ,
    BB_EVENT_DBP_RSP,
    BB_EVENT_DBP_CONNECT,
    BB_EVENT_DBP_CONNECTED,
    BB_EVENT_DBP_FAILED,
    BB_EVENT_DBP_SUB,
    BB_EVENT_DBP_UNSUB,
    BB_EVENT_DBP_NOSUB,
    BB_EVENT_DBP_READY,
    BB_EVENT_DBP_ADDED,
    BB_EVENT_DBP_METHOD,
    BB_EVENT_DBP_RESULT,

    /*super node*/
    BB_EVENT_DBP_REGISTER_FORKID,
    BB_EVENT_DBP_SEND_BLOCK,
    BB_EVENT_DBP_SEND_TX,

    BB_EVENT_DBP_PING,
    BB_EVENT_DBP_PONG,

    BB_EVENT_DBP_BROKEN,
    BB_EVENT_DBP_REMOVE_SESSION
};

class CBlockMakerEventListener;
#define TYPE_BLOCKMAKEREVENT(type, body) \
    xengine::CEventCategory<type, CBlockMakerEventListener, body, CNil>

typedef TYPE_BLOCKMAKEREVENT(EVENT_BLOCKMAKER_UPDATE, CBlockMakerUpdate) CEventBlockMakerUpdate;
typedef TYPE_BLOCKMAKEREVENT(EVENT_BLOCKMAKER_AGREE, CDelegateAgreement) CEventBlockMakerAgree;

class CBlockMakerEventListener : virtual public xengine::CEventListener
{
public:
    virtual ~CBlockMakerEventListener() {}
    DECLARE_EVENTHANDLER(CEventBlockMakerUpdate);
    DECLARE_EVENTHANDLER(CEventBlockMakerAgree);
};

template <int type, typename L, typename D>
class CBbEventDbpSocketData : public CEvent
{
    friend class CStream;

public:
    CBbEventDbpSocketData(uint64 nNonceIn, const uint256& hashForkIn, int64 nChangeIn)
        : CEvent(nNonceIn, type), hashFork(hashForkIn), nChange(nChangeIn) {}
    virtual ~CBbEventDbpSocketData() {}
    virtual bool Handle(CEventListener& listener)
    {
        try
        {
            return (dynamic_cast<L &>(listener)).HandleEvent(*this);
        }
        catch (std::bad_cast& )
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
    void BlockheadSerialize(CStream& s, O& opt)
    {
        s.Serialize(hashFork, opt);
        s.Serialize(data, opt);
    }

public:
    uint256 hashFork;
    int64 nChange;
    D data;
};

class CBbDBPEventListener;
class CDBPEventListener;
#define TYPE_DBPEVENT(type, body) \
    CBbEventDbpSocketData<type, CBbDBPEventListener, body>

#define TYPE_DBP_EVENT(type, body) \
    CEventCategory<type, CDBPEventListener, body, bool>


typedef TYPE_DBPEVENT(BB_EVENT_DBP_SOCKET_ADD_NEW_BLOCK, CBlockEx) CBbEventDbpUpdateNewBlock;
typedef TYPE_DBPEVENT(BB_EVENT_DBP_SOCKET_ADD_NEW_TX, CTransaction) CBbEventDbpUpdateNewTx;

typedef TYPE_DBP_EVENT(BB_EVENT_DBP_REQ, CBbDbpRequest) CBbEventDbpRequest;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_RSP, CBbDbpRespond) CBbEventDbpRespond;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_CONNECT, CBbDbpConnect) CBbEventDbpConnect;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_CONNECTED, CBbDbpConnected) CBbEventDbpConnected;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_FAILED, CBbDbpFailed) CBbEventDbpFailed;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_SUB, CBbDbpSub) CBbEventDbpSub;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_UNSUB, CBbDbpUnSub) CBbEventDbpUnSub;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_NOSUB, CBbDbpNoSub) CBbEventDbpNoSub;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_READY, CBbDbpReady) CBbEventDbpReady;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_ADDED, CBbDbpAdded) CBbEventDbpAdded;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_METHOD, CBbDbpMethod) CBbEventDbpMethod;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_RESULT, CBbDbpMethodResult) CBbEventDbpMethodResult;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_BROKEN, CBbDbpBroken) CBbEventDbpBroken;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_REMOVE_SESSION, CBbDbpRemoveSession) CBbEventDbpRemoveSession;

typedef TYPE_DBP_EVENT(BB_EVENT_DBP_REGISTER_FORKID, CBbDbpRegisterForkID) CBbEventDbpRegisterForkID;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_SEND_BLOCK, CBbDbpSendBlock) CBbEventDbpSendBlock;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_SEND_TX, CBbDbpSendTx) CBbEventDbpSendTx;

// HeartBeats
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_PING, CBbDbpPing) CBbEventDbpPing;
typedef TYPE_DBP_EVENT(BB_EVENT_DBP_PONG, CBbDbpPong) CBbEventDbpPong;


class CBbDBPEventListener : virtual public CEventListener
{
public:
    virtual ~CBbDBPEventListener() {}
    DECLARE_EVENTHANDLER(CBbEventDbpUpdateNewBlock);
    DECLARE_EVENTHANDLER(CBbEventDbpUpdateNewTx);
};

class CDBPEventListener : virtual public CEventListener
{
public:
    virtual ~CDBPEventListener() {}
    DECLARE_EVENTHANDLER(CBbEventDbpRequest);
    DECLARE_EVENTHANDLER(CBbEventDbpRespond);
    DECLARE_EVENTHANDLER(CBbEventDbpConnect);
    DECLARE_EVENTHANDLER(CBbEventDbpConnected);
    DECLARE_EVENTHANDLER(CBbEventDbpFailed);
    DECLARE_EVENTHANDLER(CBbEventDbpSub);
    DECLARE_EVENTHANDLER(CBbEventDbpUnSub);
    DECLARE_EVENTHANDLER(CBbEventDbpNoSub);
    DECLARE_EVENTHANDLER(CBbEventDbpReady);
    DECLARE_EVENTHANDLER(CBbEventDbpAdded);
    DECLARE_EVENTHANDLER(CBbEventDbpMethod);
    DECLARE_EVENTHANDLER(CBbEventDbpMethodResult);
    DECLARE_EVENTHANDLER(CBbEventDbpBroken);
    DECLARE_EVENTHANDLER(CBbEventDbpRemoveSession);

    DECLARE_EVENTHANDLER(CBbEventDbpPing);
    DECLARE_EVENTHANDLER(CBbEventDbpPong);

    DECLARE_EVENTHANDLER(CBbEventDbpRegisterForkID);
    DECLARE_EVENTHANDLER(CBbEventDbpSendBlock);
    DECLARE_EVENTHANDLER(CBbEventDbpSendTx);
};

} // namespace bigbang

#endif //BIGBANG_EVENT_H
