// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mthbase.h"

#include "util.h"

using namespace std;

namespace xengine
{

//----------------------------------------------------------------------------------------
uint32 CBaseUniqueId::uiIdCreate = 0;
boost::mutex CBaseUniqueId::lockCreate;

uint64 CBaseUniqueId::CreateUniqueId(uint8 ucPortType, uint8 ucDirection, uint8 ucType, uint8 ucSubType)
{
    boost::unique_lock<boost::mutex> writeLock(lockCreate);

    if (uiIdCreate == 0)
    {
        srand(time(NULL));
        uiIdCreate = rand();
        if (uiIdCreate == 0)
        {
            uiIdCreate = 1;
        }
    }

    uint64 ui64UniqueId = 0;
    ((PBASE_UNIQUE_ID)&ui64UniqueId)->uiId = uiIdCreate;
    ((PBASE_UNIQUE_ID)&ui64UniqueId)->ucPortType = ucPortType;
    ((PBASE_UNIQUE_ID)&ui64UniqueId)->ucDirection = ucDirection;
    ((PBASE_UNIQUE_ID)&ui64UniqueId)->ucType = ucType;
    ((PBASE_UNIQUE_ID)&ui64UniqueId)->ucSubType = ucSubType;
    ((PBASE_UNIQUE_ID)&ui64UniqueId)->ucRand = GetTimeMillis() & 0xFF;

    if (++uiIdCreate == 0)
    {
        uiIdCreate = 1;
    }

    return ui64UniqueId;
}

//----------------------------------------------------------------------------------------
void CMthEvent::SetEvent()
{
    boost::unique_lock<boost::mutex> lock(lockEvent);

    if (!fSingleFlag)
    {
        fSingleFlag = true;

        if (fManualReset)
        {
            condEvent.notify_all();
        }
        else
        {
            condEvent.notify_one();
        }

        CMthWait* pWait;
        std::map<uint64, CMthWait*>::iterator it;
        for (it = mapWait.begin(); it != mapWait.end(); it++)
        {
            pWait = it->second;
            if (pWait)
            {
                pWait->SetSignal(this);
            }
        }
    }
}

void CMthEvent::ResetEvent()
{
    boost::unique_lock<boost::mutex> lock(lockEvent);
    fSingleFlag = false;
}

bool CMthEvent::AddWait(CMthWait* pWait)
{
    boost::unique_lock<boost::mutex> lock(lockEvent);
    if (pWait == NULL)
    {
        return false;
    }

    if (mapWait.count(pWait->GetWaitId()) == 0)
    {
        if (!mapWait.insert(make_pair(pWait->GetWaitId(), pWait)).second)
        {
            return false;
        }
    }

    return true;
}

void CMthEvent::DelWait(CMthWait* pWait)
{
    boost::unique_lock<boost::mutex> lock(lockEvent);
    if (pWait == NULL)
    {
        return;
    }

    if (mapWait.count(pWait->GetWaitId()))
    {
        mapWait.erase(pWait->GetWaitId());
    }
}

bool CMthEvent::Wait(uint32 ui32Timeout)
{
    boost::unique_lock<boost::mutex> lock(lockEvent);
    bool fIfHasSig = false;

    if (!fSingleFlag)
    {
        uint64 ui64BeginTime;
        uint64 ui64WaitTime;

        ui64BeginTime = GetTimeMillis();

        do
        {
            if (ui32Timeout > 0)
            {
                ui64WaitTime = ui32Timeout - (GetTimeMillis() - ui64BeginTime);
                if (ui64WaitTime <= 0)
                {
                    return false;
                }

                if (!condEvent.timed_wait(lock, boost::posix_time::milliseconds(ui64WaitTime)))
                {
                    return false;
                }
            }
        } while (ui32Timeout > 0 && !fSingleFlag);
    }

    if (fSingleFlag)
    {
        fIfHasSig = true;
        if (!fManualReset)
        {
            fSingleFlag = false;
        }
    }

    return fIfHasSig;
}

//----------------------------------------------------------------------------------
CMthWait::~CMthWait()
{
    PNM_EVENT pNmEvent;
    std::map<uint64, PNM_EVENT>::iterator it;

    for (it = mapEvent.begin(); it != mapEvent.end(); it++)
    {
        pNmEvent = it->second;
        if (pNmEvent)
        {
            delete pNmEvent;
        }
    }
}

bool CMthWait::AddEvent(CMthEvent* pEvent, int iEventFlag)
{
    boost::unique_lock<boost::mutex> lock(lockWait);
    if (pEvent == NULL || iEventFlag < 0)
    {
        return false;
    }

    if (mapEvent.count(pEvent->GetEventId()) == 0)
    {
        PNM_EVENT pNmEvent;
        std::map<uint64, PNM_EVENT>::iterator it;

        for (it = mapEvent.begin(); it != mapEvent.end(); it++)
        {
            pNmEvent = it->second;
            if (pNmEvent && pNmEvent->iEventFlag == iEventFlag)
            {
                return false;
            }
        }

        if (!pEvent->AddWait(this))
        {
            return false;
        }

        pNmEvent = new NM_EVENT;
        memset(pNmEvent, 0, sizeof(NM_EVENT));

        pNmEvent->iEventFlag = iEventFlag;
        pNmEvent->pEvent = pEvent;
        pNmEvent->fSignalFlag = false;

        if (!mapEvent.insert(make_pair(pEvent->GetEventId(), pNmEvent)).second)
        {
            delete pNmEvent;
            pEvent->DelWait(this);
            return false;
        }
    }

    return true;
}

void CMthWait::DelEvent(CMthEvent* pEvent)
{
    boost::unique_lock<boost::mutex> lock(lockWait);
    if (pEvent == NULL)
    {
        return;
    }

    if (mapEvent.count(pEvent->GetEventId()))
    {
        pEvent->DelWait(this);

        PNM_EVENT pNmEvent = mapEvent[pEvent->GetEventId()];
        if (pNmEvent)
        {
            delete pNmEvent;
        }
        mapEvent.erase(pEvent->GetEventId());
    }
}

void CMthWait::SetSignal(CMthEvent* pEvent)
{
    boost::unique_lock<boost::mutex> lock(lockWait);
    if (pEvent == NULL)
    {
        return;
    }

    if (mapEvent.count(pEvent->GetEventId()))
    {
        PNM_EVENT pNmEvent = mapEvent[pEvent->GetEventId()];
        if (pNmEvent)
        {
            pNmEvent->fSignalFlag = true;
            condWait.notify_one();
        }
    }
}

int CMthWait::Wait(uint32 ui32Timeout)
{
    boost::unique_lock<boost::mutex> lock(lockWait);
    PNM_EVENT pNmEvent;
    std::map<uint64, PNM_EVENT>::iterator it;
    uint32 ui32TestId;
    uint64 ui64BeginTime;
    uint64 ui64WaitTime;

    ui64BeginTime = GetTimeMillis();

    do
    {
        for (it = mapEvent.begin(), ui32TestId = 0; it != mapEvent.end(); it++, ui32TestId++)
        {
            if (ui32TestId >= ui32WaitPos)
            {
                pNmEvent = it->second;
                if (pNmEvent && pNmEvent->fSignalFlag)
                {
                    pNmEvent->fSignalFlag = false;
                    ui32WaitPos = (ui32WaitPos + 1) % mapEvent.size();
                    return pNmEvent->iEventFlag;
                }
            }
        }

        for (it = mapEvent.begin(), ui32TestId = 0; it != mapEvent.end(); it++, ui32TestId++)
        {
            if (ui32TestId < ui32WaitPos)
            {
                pNmEvent = it->second;
                if (pNmEvent && pNmEvent->fSignalFlag)
                {
                    pNmEvent->fSignalFlag = false;
                    ui32WaitPos = (ui32WaitPos + 1) % mapEvent.size();
                    return pNmEvent->iEventFlag;
                }
            }
        }

        if (ui32Timeout > 0)
        {
            ui64WaitTime = ui32Timeout - (GetTimeMillis() - ui64BeginTime);
            if (ui64WaitTime <= 0)
            {
                return -1;
            }

            if (!condWait.timed_wait(lock, boost::posix_time::milliseconds(ui64WaitTime)))
            {
                return -1;
            }
        }

    } while (ui32Timeout > 0);

    return -1;
}

} // namespace xengine
