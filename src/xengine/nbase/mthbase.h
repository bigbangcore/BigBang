// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_NBASE_MTHBASE_H
#define XENGINE_NBASE_MTHBASE_H

#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <queue>

#include "type.h"
#include "util.h"

namespace xengine
{

inline unsigned long GetCurThreadId()
{
    std::string threadId = boost::lexical_cast<std::string>(boost::this_thread::get_id());
    unsigned long threadNumber = 0;
    threadNumber = std::stoul(threadId, nullptr, 16);
    return threadNumber;
}

class CBaseUniqueId : public boost::noncopyable
{
public:
    CBaseUniqueId(uint64 ui64Id)
      : ui64UniqueId(ui64Id) {}
    //CBaseUniqueId(CBaseUniqueId& uid)
    //  : ui64UniqueId(uid.ui64UniqueId) {}

    uint8 GetPortType() const
    {
        return ((PBASE_UNIQUE_ID)&ui64UniqueId)->ucPortType;
    }
    uint8 GetDirection() const
    {
        return ((PBASE_UNIQUE_ID)&ui64UniqueId)->ucDirection;
    }
    uint8 GetType() const
    {
        return ((PBASE_UNIQUE_ID)&ui64UniqueId)->ucType;
    }
    uint8 GetSubType() const
    {
        return ((PBASE_UNIQUE_ID)&ui64UniqueId)->ucSubType;
    }
    uint32 GetId() const
    {
        return ((PBASE_UNIQUE_ID)&ui64UniqueId)->uiId;
    }
    uint64 GetUniqueId() const
    {
        return ui64UniqueId;
    }

    CBaseUniqueId& operator=(const CBaseUniqueId& uid)
    {
        ui64UniqueId = uid.ui64UniqueId;
        return *this;
    }

    static uint64 CreateUniqueId(uint8 ucPortType, uint8 ucDirection, uint8 ucType, uint8 ucSubType = 0);

private:
    static uint32 uiIdCreate;
    static boost::mutex lockCreate;

#pragma pack(1)
    typedef struct _BASE_UNIQUE_ID
    {
        uint32 uiId;
        uint8 ucPortType : 1;  //0: tcp, 1: udp
        uint8 ucDirection : 1; //0: in, 1: out
        uint8 ucReserve : 6;
        uint8 ucType;
        uint8 ucSubType;
        uint8 ucRand;
    } BASE_UNIQUE_ID, *PBASE_UNIQUE_ID;
#pragma pack()

    uint64 ui64UniqueId;
};

class CMthWait;

class CMthEvent
{
public:
    CMthEvent(bool bManualResetIn = false, bool bInitSigStateIn = false)
      : fManualReset(bManualResetIn), fSingleFlag(bInitSigStateIn)
    {
        ui64EventId = CBaseUniqueId::CreateUniqueId(0, 0, 0xEF);
    }
    ~CMthEvent()
    {
        mapWait.clear();
    }

    uint64 GetEventId() const
    {
        return ui64EventId;
    }

    void SetEvent();
    void ResetEvent();

    bool AddWait(CMthWait* pWait);
    void DelWait(CMthWait* pWait);

    bool Wait(uint32 ui32Timeout); /* ui32Timeout is milliseconds */

    bool QuerySetWaitObj(void* pWaitObj);
    void ClearWaitObj(void* pWaitObj);
    void ClearAllWaitObj();

private:
    boost::mutex lockEvent;
    boost::condition_variable_any condEvent;

    bool fSingleFlag;
    bool fManualReset;

    uint64 ui64EventId;

    std::map<uint64, CMthWait*> mapWait;
};

class CMthWait
{
public:
    CMthWait()
      : ui32WaitPos(0)
    {
        ui64WaitId = CBaseUniqueId::CreateUniqueId(0, 0, 0xEF);
    }
    ~CMthWait();

    uint64 GetWaitId() const
    {
        return ui64WaitId;
    }
    bool AddEvent(CMthEvent* pEvent, int iEventFlag);
    void DelEvent(CMthEvent* pEvent);
    void SetSignal(CMthEvent* pEvent);
    int Wait(uint32 ui32Timeout); /* ui32Timeout is milliseconds */

private:
    boost::mutex lockWait;
    boost::condition_variable_any condWait;
    uint64 ui64WaitId;

    typedef struct _NM_EVENT
    {
        int iEventFlag;
        bool fSignalFlag;
        CMthEvent* pEvent;
    } NM_EVENT, *PNM_EVENT;

    std::map<uint64, PNM_EVENT> mapEvent;
    uint32 ui32WaitPos;
};

class CMthDataBuf
{
public:
    CMthDataBuf()
      : pDataBuf(NULL), ui32BufSize(0), ui32DataLen(0) {}
    CMthDataBuf(uint32 ui32AllocSize)
    {
        ui32BufSize = ui32AllocSize;
        ui32DataLen = 0;
        pDataBuf = new char[ui32BufSize];
    }
    CMthDataBuf(const char* pInBuf, const uint32 ui32InLen)
    {
        if (pInBuf == NULL || ui32InLen == 0)
        {
            pDataBuf = NULL;
            ui32BufSize = 0;
            ui32DataLen = 0;
        }
        else
        {
            ui32BufSize = ui32InLen;
            ui32DataLen = ui32InLen;
            pDataBuf = new char[ui32BufSize];
            memcpy(pDataBuf, pInBuf, ui32InLen);
        }
    }
    CMthDataBuf(const std::string& strIn)
    {
        if (strIn.empty())
        {
            pDataBuf = NULL;
            ui32BufSize = 0;
            ui32DataLen = 0;
        }
        else
        {
            ui32DataLen = strIn.size();
            ui32BufSize = ui32DataLen;
            pDataBuf = new char[ui32BufSize];
            memcpy(pDataBuf, strIn.c_str(), ui32DataLen);
        }
    }
    CMthDataBuf(const CMthDataBuf& mbuf)
    {
        ui32BufSize = mbuf.GetBufSize();
        ui32DataLen = mbuf.GetDataLen();
        pDataBuf = NULL;
        if (ui32DataLen > ui32BufSize)
        {
            ui32BufSize = ui32DataLen;
        }
        if (ui32BufSize > 0)
        {
            pDataBuf = new char[ui32BufSize];
        }
        if (mbuf.GetDataBuf() && ui32DataLen > 0)
        {
            memcpy(pDataBuf, mbuf.GetDataBuf(), ui32DataLen);
        }
    }
    ~CMthDataBuf()
    {
        clear();
    }

    inline char* GetDataBuf() const
    {
        return pDataBuf;
    }
    inline uint32 GetBufSize() const
    {
        return ui32BufSize;
    }
    inline uint32 GetDataLen() const
    {
        return ui32DataLen;
    }

    CMthDataBuf& operator=(const CMthDataBuf& mbuf)
    {
        ui32BufSize = mbuf.GetBufSize();
        ui32DataLen = mbuf.GetDataLen();
        pDataBuf = NULL;
        if (ui32DataLen > ui32BufSize)
        {
            ui32BufSize = ui32DataLen;
        }
        if (ui32BufSize > 0)
        {
            pDataBuf = new char[ui32BufSize];
        }
        if (mbuf.GetDataBuf() && ui32DataLen > 0)
        {
            memcpy(pDataBuf, mbuf.GetDataBuf(), ui32DataLen);
        }
        return *this;
    }
    CMthDataBuf& operator+=(const CMthDataBuf& mbuf)
    {
        if (mbuf.GetBufSize() > 0)
        {
            char* pNewBuf;
            uint32 uimlen;
            ui32BufSize += mbuf.GetBufSize();
            pNewBuf = new char[ui32BufSize];
            if (pDataBuf && ui32DataLen > 0)
            {
                memcpy(pNewBuf, pDataBuf, ui32DataLen);
            }
            uimlen = mbuf.GetDataLen();
            if (uimlen > 0)
            {
                memcpy(pNewBuf + ui32DataLen, mbuf.GetDataBuf(), uimlen);
                ui32DataLen += uimlen;
            }
            if (pDataBuf)
            {
                delete[] pDataBuf;
            }
            pDataBuf = pNewBuf;
        }
        return *this;
    }

    void erase(uint32 ui32Pos, uint32 ui32Len)
    {
        if (pDataBuf && ui32DataLen > 0 && ui32Len > 0 && ui32Pos < ui32DataLen)
        {
            if (ui32Pos + ui32Len >= ui32DataLen)
            {
                ui32DataLen = ui32Pos;
            }
            else
            {
                memmove(pDataBuf + ui32Pos,
                        pDataBuf + ui32Pos + ui32Len,
                        ui32DataLen - (ui32Pos + ui32Len));
                ui32DataLen -= ui32Len;
            }
        }
    }
    void erase(uint32 ui32Len)
    {
        erase(0, ui32Len);
    }
    void reserve(uint32 ui32Size)
    {
        if (ui32Size > ui32BufSize)
        {
            ui32BufSize = ui32Size;
            char* pNewBuf = new char[ui32BufSize];
            if (pDataBuf && ui32DataLen > 0)
            {
                memcpy(pNewBuf, pDataBuf, ui32DataLen);
            }
            if (pDataBuf)
            {
                delete[] pDataBuf;
            }
            pDataBuf = pNewBuf;
        }
    }
    void clear()
    {
        if (pDataBuf)
        {
            delete[] pDataBuf;
            pDataBuf = NULL;
        }
        ui32BufSize = 0;
        ui32DataLen = 0;
    }

protected:
    char* pDataBuf;
    uint32 ui32BufSize;
    uint32 ui32DataLen;
};

template <typename T>
class CMthNvDataBuf : public CMthDataBuf
{
public:
    CMthNvDataBuf() {}
    CMthNvDataBuf(T v, const char* p, const uint32 n)
      : CMthDataBuf(p, n), tNv(v) {}
    CMthNvDataBuf(T v, const std::string s)
      : CMthDataBuf(s), tNv(v) {}

    T& SetNvData(T& v)
    {
        tNv = v;
    }
    T& GetNvData()
    {
        return tNv;
    }

private:
    T tNv;
};

template <typename T>
class CMthQueue
{
public:
    CMthQueue()
      : eventRead(true, false), eventWrite(true, true), ui32MaxQueueSize(10000), fAbort(false) {}
    CMthQueue(uint32 ui32QueueSize)
      : eventRead(true, false), eventWrite(true, true), ui32MaxQueueSize(ui32QueueSize), fAbort(false) {}
    ~CMthQueue() {}

    /* ui32Timeout is milliseconds */
    virtual bool SetData(const T& data, uint32 ui32Timeout = 0)
    {
        boost::unique_lock<boost::mutex> lock(lockQueue);
        return PrtSetData(lock, data, ui32Timeout);
    }

    /* ui32Timeout is milliseconds */
    virtual bool GetData(T& data, uint32 ui32Timeout = 0)
    {
        boost::unique_lock<boost::mutex> lock(lockQueue);
        return PrtGetData(lock, data, ui32Timeout);
    }

    uint32 GetCount()
    {
        boost::unique_lock<boost::mutex> lock(lockQueue);
        return qData.size();
    }

    CMthEvent& GetReadEvent()
    {
        return eventRead;
    }
    CMthEvent& GetWriteEvent()
    {
        return eventWrite;
    }

    void SetQueueSize(uint32 ui32QueueSize)
    {
        if (ui32QueueSize > 0)
        {
            boost::unique_lock<boost::mutex> lock(lockQueue);
            ui32MaxQueueSize = ui32QueueSize;
        }
    }
    void Interrupt()
    {
        {
            boost::unique_lock<boost::mutex> lock(lockQueue);
            /*while (!qData.empty())
            {
                qData.pop();
            }*/
            fAbort = true;
        }
        eventRead.SetEvent();
        eventWrite.SetEvent();
        condQueueRead.notify_all();
        condQueueWrite.notify_all();
    }

protected:
    inline bool PrtSetData(boost::unique_lock<boost::mutex>& lock, const T& data, uint32 ui32Timeout)
    {
        uint64 ui64BeginTime;
        uint64 ui64WaitTime;

        ui64BeginTime = GetTimeMillis();
        do
        {
            if (fAbort)
            {
                break;
            }
            if (qData.size() < ui32MaxQueueSize)
            {
                qData.push(data);
                condQueueRead.notify_one();
                eventRead.SetEvent();
                if (qData.size() >= ui32MaxQueueSize)
                {
                    eventWrite.ResetEvent();
                }
                return true;
            }
            if (ui32Timeout)
            {
                ui64WaitTime = ui32Timeout - (GetTimeMillis() - ui64BeginTime);
                if (ui64WaitTime <= 0 || !condQueueWrite.timed_wait(lock, boost::posix_time::milliseconds(ui64WaitTime)))
                {
                    break;
                }
            }
        } while (ui32Timeout);

        return false;
    }

    inline bool PrtGetData(boost::unique_lock<boost::mutex>& lock, T& data, uint32 ui32Timeout)
    {
        uint64 ui64BeginTime;
        uint64 ui64WaitTime;

        ui64BeginTime = GetTimeMillis();
        do
        {
            if (fAbort)
            {
                break;
            }
            if (!qData.empty())
            {
                data = qData.front();
                qData.pop();
                condQueueWrite.notify_one();
                eventWrite.SetEvent();
                if (qData.empty())
                {
                    eventRead.ResetEvent();
                }
                return true;
            }
            if (ui32Timeout)
            {
                ui64WaitTime = ui32Timeout - (GetTimeMillis() - ui64BeginTime);
                if (ui64WaitTime <= 0 || !condQueueRead.timed_wait(lock, boost::posix_time::milliseconds(ui64WaitTime)))
                {
                    break;
                }
            }
        } while (ui32Timeout);

        return false;
    }

protected:
    boost::mutex lockQueue;
    boost::condition_variable_any condQueueRead;
    boost::condition_variable_any condQueueWrite;

    CMthEvent eventRead;
    CMthEvent eventWrite;

    std::queue<T> qData;
    uint32 ui32MaxQueueSize;
    bool fAbort;
};

} // namespace xengine

#endif // XENGINE_NBASE_MTHBASE_H
