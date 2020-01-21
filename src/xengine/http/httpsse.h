// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_HTTP_HTTPSSE_H
#define XENGINE_HTTP_HTTPSSE_H

#include <boost/ptr_container/ptr_map.hpp>
#include <boost/thread/thread.hpp>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "http/httptype.h"
#include "util.h"

namespace xengine
{

class CHttpSSEData
{
public:
    virtual ~CHttpSSEData() {}
    virtual bool operator==(const CHttpSSEData&) const = 0;
    virtual std::string ToString() = 0;
};

class CHttpSSEGenerator
{
public:
    CHttpSSEGenerator() {}
    virtual ~CHttpSSEGenerator() {}
    virtual void ResetData() = 0;
    virtual bool UpdateData(CHttpSSEData& data, uint64 nEventNewId) = 0;
    virtual void GenerateEventData(uint64 nEventLastId, uint64 nEventCurrentId,
                                   std::vector<std::string>& vEventData)
        = 0;
};

template <typename T>
class CHttpSSEStatusGenerator : public CHttpSSEGenerator
{
public:
    CHttpSSEStatusGenerator()
      : nEventId(0) {}
    virtual void ResetData() override
    {
        status = T();
    }
    virtual bool UpdateData(CHttpSSEData& data, uint64 nEventNewId) override
    {
        try
        {
            T& s = dynamic_cast<T&>(data);
            if (!(s == status))
            {
                status = s;
                nEventId = nEventNewId;
                return true;
            }
        }
        catch (std::exception& e)
        {
            StdError(__PRETTY_FUNCTION__, e.what());
        }
        return false;
    }
    virtual void GenerateEventData(uint64 nEventLastId, uint64 nEventCurrentId,
                                   std::vector<std::string>& vEventData) override
    {
        if (nEventLastId < nEventId && !(status == T()))
        {
            vEventData.push_back(status.ToString());
        }
    }

protected:
    T status;
    uint64 nEventId;
};

template <typename T>
class CHttpSSEQueGenerator : public CHttpSSEGenerator
{
public:
    CHttpSSEQueGenerator() {}
    virtual void ResetData()
    {
        while (!q.empty())
        {
            q.pop();
        }
    }
    virtual bool UpdateData(CHttpSSEData& data, uint64 nEventNewId)
    {
        try
        {
            T& s = dynamic_cast<T&>(data);
            if (q.empty() || !(s == q.back().second))
            {
                q.push(std::make_pair(nEventNewId, s));
                return true;
            }
        }
        catch (std::exception& e)
        {
            StdError(__PRETTY_FUNCTION__, e.what());
        }
        return false;
    }
    virtual void GenerateEventData(uint64 nEventLastId, uint64 nEventCurrentId,
                                   std::vector<std::string>& vEventData)
    {
        while (!q.empty() && q.front().first <= nEventLastId)
        {
            q.pop();
        }
        while (!q.empty() && q.front().first <= nEventCurrentId)
        {
            vEventData.push_back(q.front().second.ToString());
            q.pop();
        }
    }

protected:
    typename std::queue<std::pair<uint64, T>> q;
};

class CHttpEventStream
{
public:
    CHttpEventStream();
    CHttpEventStream(const std::string& strEntryIn);
    CHttpEventStream(const CHttpEventStream& es);
    CHttpEventStream& operator=(const CHttpEventStream& es)
    {
        if (this != &es)
        {
            CHttpEventStream temp(es);
            std::swap(*this, temp);
        }
        return *this;
    }
    virtual ~CHttpEventStream();
    const std::string& GetEntry();
    void RegisterEvent(const std::string& strEventName, CHttpSSEGenerator* pGenerator);
    void UnregisterEvent(const std::string& strEventName);
    void ResetData(const std::string& strEventName);
    bool UpdateEventData(const std::string& strEventName, CHttpSSEData& data);
    bool ConstructResponse(uint64 nLastEventId, CHttpRsp& rsp);

protected:
    boost::mutex mtxEvent;
    std::string strEntry;
    uint64 nEventId;
    //boost::ptr_map<std::string,CHttpSSEGenerator> mapGenerator;
    std::map<std::string, std::unique_ptr<CHttpSSEGenerator>> mapGenerator;
};

} // namespace xengine

#endif //XENGINE_HTTP_HTTPSSE_H
