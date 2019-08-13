// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "event.h"

using namespace std;

namespace xengine
{

///////////////////////////////
// CEventListener

bool CEventListener::DispatchEvent(CEvent* pEvent)
{
    return pEvent->Handle(*this);
}

} // namespace xengine
