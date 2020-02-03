// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_DOCKER_NETTIME_H
#define XENGINE_DOCKER_NETTIME_H

#include <algorithm>
#include <boost/asio/ip/address.hpp>
#include <boost/thread/thread.hpp>
#include <map>
#include <vector>

#include "type.h"
#include "util.h"

namespace xengine
{

#define MAX_TIME_ADJUSTMENT (70 * 60)
#define MAX_SAMPLES_COUNT (200)

class CNetTime
{
    class CDelta
    {
    public:
        int64 nTimeDelta;
        int64 nTimeUpdate;
    };

public:
    CNetTime()
      : nMaxCount(MAX_SAMPLES_COUNT), nTimeOffset(0)
    {
        vecDelta.reserve(nMaxCount);
    }
    int64 GetTimeOffset()
    {
        boost::unique_lock<boost::mutex> lock(mtxNetTime);
        return nTimeOffset;
    }
    void Clear()
    {
        mapNode.clear();
        vecDelta.clear();
        nTimeOffset = 0;
    }
    bool AddNew(const boost::asio::ip::address& address, int64 nTimeDelta)
    {
        boost::unique_lock<boost::mutex> lock(mtxNetTime);

        CDelta& node = mapNode[address];
        if (node.nTimeUpdate != 0)
        {
            if (node.nTimeDelta == nTimeDelta)
            {
                return true;
            }
            EraseDelta(node.nTimeDelta);
        }
        InsertDelta(nTimeDelta);

        node.nTimeUpdate = GetTime();
        node.nTimeDelta = nTimeDelta;

        if (vecDelta.size() > nMaxCount)
        {
            RemoveFirst();
        }

        if (vecDelta.size() > 5)
        {
            int64 nOffset = GetMedian();
            if (nOffset < MAX_TIME_ADJUSTMENT && nOffset > -MAX_TIME_ADJUSTMENT)
            {
                nTimeOffset = nOffset;
            }
            else
            {
                nTimeOffset = 0;
                return false;
            }
        }
        return true;
    }

protected:
    void RemoveFirst()
    {
        std::map<boost::asio::ip::address, CDelta>::iterator it = mapNode.begin();
        std::map<boost::asio::ip::address, CDelta>::iterator iterFirst = it;
        int64 nFirst = (*it).second.nTimeUpdate;

        while (++it != mapNode.end())
        {
            if ((*it).second.nTimeUpdate < nFirst)
            {
                nFirst = (*it).second.nTimeUpdate;
                iterFirst = it;
            }
        }
        EraseDelta((*iterFirst).second.nTimeDelta);
        mapNode.erase(iterFirst);
    }
    void EraseDelta(int64 nTimeDelta)
    {
        vecDelta.erase(std::lower_bound(vecDelta.begin(), vecDelta.end(), nTimeDelta));
    }
    void InsertDelta(int64 nTimeDelta)
    {
        vecDelta.insert(std::lower_bound(vecDelta.begin(), vecDelta.end(), nTimeDelta), nTimeDelta);
    }
    int64 GetMedian()
    {
        std::size_t nMedian = (vecDelta.size() >> 1);
        return ((vecDelta.size() & 1) ? vecDelta[nMedian]
                                      : ((vecDelta[nMedian] + vecDelta[nMedian - 1]) >> 1));
    }

protected:
    boost::mutex mtxNetTime;
    std::map<boost::asio::ip::address, CDelta> mapNode;
    std::vector<int64> vecDelta;
    const std::size_t nMaxCount;
    int64 nTimeOffset;
};

} // namespace xengine

#endif //XENGINE_DOCKER_NETTIME_H
