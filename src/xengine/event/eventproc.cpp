// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "eventproc.h"

#include <boost/bind.hpp>

using namespace std;

namespace xengine
{

///////////////////////////////
// CEventProc

CEventProc::CEventProc(const string& ownKeyIn)
  : IBase(ownKeyIn),
    thrEventQue(ownKeyIn + "-eventq", boost::bind(&CEventProc::EventThreadFunc, this))
{
}

bool CEventProc::HandleInvoke()
{
    queEvent.Reset();

    return ThreadStart(thrEventQue);
}

void CEventProc::HandleHalt()
{
    queEvent.Interrupt();

    ThreadExit(thrEventQue);
}

void CEventProc::PostEvent(CEvent* pEvent)
{
    queEvent.AddNew(pEvent);
}

void CEventProc::EventThreadFunc()
{
    CEvent* pEvent = nullptr;
    while ((pEvent = queEvent.Fetch()) != nullptr)
    {
        if (!pEvent->Handle(*this))
        {
            queEvent.Reset();
        }
        pEvent->Free();
    }
}

} // namespace xengine
