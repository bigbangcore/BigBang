// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_EVENT_H
#define BIGBANG_EVENT_H

#include <map>
#include <set>
#include <vector>

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
    EVENT_BLOCKMAKER_AGREEMENT
};

class CBlockMakerEventListener;
#define TYPE_BLOCKMAKEREVENT(type, body) \
    xengine::CEventCategory<type, CBlockMakerEventListener, body, CNil>

typedef TYPE_BLOCKMAKEREVENT(EVENT_BLOCKMAKER_UPDATE, CBlockMakerUpdate) CEventBlockMakerUpdate;
typedef TYPE_BLOCKMAKEREVENT(EVENT_BLOCKMAKER_AGREEMENT, CDelegateAgreement) CEventBlockMakerAgreement;

class CBlockMakerEventListener : virtual public xengine::CEventListener
{
public:
    virtual ~CBlockMakerEventListener() {}
    DECLARE_EVENTHANDLER(CEventBlockMakerUpdate);
    DECLARE_EVENTHANDLER(CEventBlockMakerAgreement);
};

} // namespace bigbang

#endif //BIGBANG_EVENT_H
