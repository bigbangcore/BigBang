// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_MQEVENT_H
#define BIGBANG_MQEVENT_H

#include "xengine.h"
#include "struct.h"

using namespace xengine;

namespace bigbang
{

enum
{
    EVENT_MQ_BASE = EVENT_USER_BASE,
    EVENT_MQ_SYNCBLOCK,
    EVENT_MQ_UPDATEBLOCK,
    EVENT_MQ_AGREEMENT
};

class CMQEventListener;

#define TYPE_MQEVENT(type, body, res) \
    CEventCategory<type, CMQEventListener, body, res>



typedef TYPE_MQEVENT(EVENT_MQ_SYNCBLOCK, int, std::string) CEventMQSyncBlock;
typedef TYPE_MQEVENT(EVENT_MQ_UPDATEBLOCK, CDelegateAgreement, int) CEventMQUpdateBlock;
typedef TYPE_MQEVENT(EVENT_MQ_AGREEMENT, CDelegateAgreement, int) CEventMQAgreement;

class CMQEventListener : virtual public CEventListener
{
public:
    virtual ~CMQEventListener() {}
    DECLARE_EVENTHANDLER(CEventMQSyncBlock);
    DECLARE_EVENTHANDLER(CEventMQUpdateBlock);
    DECLARE_EVENTHANDLER(CEventMQAgreement);
};

} // namespace bigbang

#endif //BIGBANG_MQEVENT_H
