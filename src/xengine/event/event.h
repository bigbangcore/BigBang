// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_EVENT_EVENT_H
#define XENGINE_EVENT_EVENT_H

#include "logger.h"
#include "stream/stream.h"

namespace xengine
{

#define DECLARE_EVENTHANDLER(Type)    \
    virtual bool HandleEvent(Type& e) \
    {                                 \
        return HandleEventDefault(e); \
    }
enum
{
    EVENT_PEER_BASE = (1 << 8),
    EVENT_HTTP_BASE = (2 << 8),
    EVENT_USER_BASE = (128 << 8)
};

class CEvent;
class CEventListener
{
public:
    virtual ~CEventListener() {}
    virtual bool DispatchEvent(CEvent* pEvent);
    virtual bool HandleEventDefault(CEvent&)
    {
        return true;
    }
    virtual bool HandleEvent(CEvent& e)
    {
        return HandleEventDefault(e);
    }
};

class CEvent
{
    friend class CStream;

public:
    CEvent(uint64 nNonceIn, int nTypeIn)
      : nNonce(nNonceIn), nType(nTypeIn) {}
    CEvent(const std::string& session, int nTypeIn)
      : nType(nTypeIn), strSessionId(session) {}
    virtual ~CEvent() {}
    virtual bool Handle(CEventListener& listener)
    {
        return listener.HandleEvent(*this);
    }
    virtual void Free()
    {
        delete this;
    }

protected:
    virtual void Serialize(CStream& s, SaveType&)
    {
        (void)s;
    }
    virtual void Serialize(CStream& s, LoadType&)
    {
        (void)s;
    }
    virtual void Serialize(CStream& s, std::size_t& serSize)
    {
        (void)s;
        (void)serSize;
    }

public:
    uint64 nNonce;
    int nType;
    std::string strSessionId;
};

template <int type, typename L, typename D, typename R>
class CEventCategory : public CEvent
{
    friend class CStream;

public:
    CEventCategory(uint64 nNonceIn)
      : CEvent(nNonceIn, type) {}
    CEventCategory(const std::string& session)
      : CEvent(session, type) {}
    virtual ~CEventCategory() {}
    virtual bool Handle(CEventListener& listener) override
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
        s.Serialize(data, opt);
    }

public:
    D data;
    R result;
};

} // namespace xengine

#endif //XENGINE_EVENT_EVENT_H
