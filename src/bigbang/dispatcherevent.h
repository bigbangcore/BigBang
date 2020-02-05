// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_DISPATCHEREVENT_H
#define BIGBANG_DISPATCHEREVENT_H

//#include "event/event.h"
#include "xengine.h"

using namespace xengine;

namespace bigbang
{

enum
{
    EVENT_DISPATCHER_BASE = xengine::EVENT_USER_BASE,
    EVENT_DISPATCHER_AGREEMENT,
    EVENT_DISPATCHER_MAX,
};

class CDispatcherEventListener;

#define TYPE_DISPATCHEREVENT(type, body) \
    xengine::CEventCategory<type, CDispatcherEventListener, body, bool>

typedef TYPE_DISPATCHEREVENT(EVENT_DISPATCHER_AGREEMENT, CDelegateAgreement) CEventDispatcherAgreement;

class CDispatcherEventListener : virtual public xengine::CEventListener
{
public:
    virtual ~CDispatcherEventListener() {}
    DECLARE_EVENTHANDLER(CEventDispatcherAgreement);
};



}





#endif //BIGBANG_DISPATCHEREVENT_H
