// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_HTTP_HTTPEVENT_H
#define XENGINE_HTTP_HTTPEVENT_H

#include "event/event.h"
#include "http/httptype.h"

namespace xengine
{

enum
{
    EVENT_HTTP_IDLE = EVENT_HTTP_BASE,
    EVENT_HTTP_REQ,
    EVENT_HTTP_RSP,
    EVENT_HTTP_GET,
    EVENT_HTTP_GETRSP,
    EVENT_HTTP_ABORT,
    EVENT_HTTP_BROKEN
};

class CHttpEventListener;

#define TYPE_HTTPEVENT(type, body) \
    CEventCategory<type, CHttpEventListener, body, bool>

typedef TYPE_HTTPEVENT(EVENT_HTTP_REQ, CHttpReq) CEventHttpReq;
typedef TYPE_HTTPEVENT(EVENT_HTTP_RSP, CHttpRsp) CEventHttpRsp;
typedef TYPE_HTTPEVENT(EVENT_HTTP_GET, CHttpReqData) CEventHttpGet;
typedef TYPE_HTTPEVENT(EVENT_HTTP_GETRSP, CHttpRsp) CEventHttpGetRsp;
typedef TYPE_HTTPEVENT(EVENT_HTTP_ABORT, CHttpAbort) CEventHttpAbort;
typedef TYPE_HTTPEVENT(EVENT_HTTP_BROKEN, CHttpBroken) CEventHttpBroken;

class CHttpEventListener : virtual public CEventListener
{
public:
    virtual ~CHttpEventListener() {}
    DECLARE_EVENTHANDLER(CEventHttpReq);
    DECLARE_EVENTHANDLER(CEventHttpRsp);
    DECLARE_EVENTHANDLER(CEventHttpAbort);
    DECLARE_EVENTHANDLER(CEventHttpGet);
    DECLARE_EVENTHANDLER(CEventHttpGetRsp);
    DECLARE_EVENTHANDLER(CEventHttpBroken);
};

} // namespace xengine

#endif //XENGINE_HTTP_HTTPEVENT_H
