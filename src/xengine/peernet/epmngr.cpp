// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php

#include "epmngr.h"

#include <vector>

#include "util.h"

using namespace std;
using boost::asio::ip::tcp;

namespace xengine
{

///////////////////////////////
// CConnAttempt

CConnAttempt::CConnAttempt()
  : nAttempts(0), nStartIndex(0), nStartTime(0), arrayAttempt{ 0 }
{
}

bool CConnAttempt::Attempt(int64 ts)
{
    int64 offset = (ts - nStartTime) / COUNTER_INTERVAL;
    if (offset >= COUNTER_NUM * 2)
    {
        arrayAttempt[0] = 1;
        for (int i = 1; i < COUNTER_NUM; i++)
        {
            arrayAttempt[i] = 0;
        }
        nAttempts = 1;
        nStartIndex = 0;
        nStartTime = ts;
        return true;
    }

    while (offset >= COUNTER_NUM)
    {
        nAttempts -= arrayAttempt[nStartIndex];
        arrayAttempt[nStartIndex] = 0;
        nStartTime += COUNTER_INTERVAL;
        nStartIndex = (nStartIndex + 1) & COUNTER_MASK;
        offset--;
    }

    arrayAttempt[(nStartIndex + offset) & COUNTER_MASK]++;

    return (++nAttempts <= ATTEMPT_LIMIT);
}

///////////////////////////////
// CAddressStatus

CAddressStatus::CAddressStatus()
  : nScore(0), nLastSeen(), nBanTo(0), nConnections(0)
{
}

bool CAddressStatus::InBoundAttempt(int64 ts)
{
    nLastSeen = ts;
    if (connAttempt.Attempt(ts))
    {
        return (ts > nBanTo);
    }

    Penalize(ATTEMPT_PENALTY, ts);
    return false;
}

bool CAddressStatus::AddConnection(bool fInBound)
{
    if (nConnections >= (fInBound ? CONNECTION_LIMIT : 1))
    {
        return false;
    }
    nConnections++;
    return true;
}

void CAddressStatus::RemoveConnection()
{
    nConnections--;
}

void CAddressStatus::Reward(int nPoints, int64 ts)
{
    nScore += nPoints;
    if (nScore > MAX_SCORE)
    {
        nScore = MAX_SCORE;
    }
    nLastSeen = ts;
}

void CAddressStatus::Penalize(int nPoints, int64 ts)
{
    nScore -= nPoints;
    if (nScore < MIN_SCORE)
    {
        nScore = MIN_SCORE;
    }

    if (nScore < 0)
    {
        int64 bantime = BANTIME_BASE << (11 * nScore / MIN_SCORE);
        nBanTo = (ts > nBanTo ? ts : nBanTo) + bantime;
    }
}

///////////////////////////////
// CEndpointManager

void CEndpointManager::Clear()
{
    mapAddressStatus.clear();
    mngrNode.Clear();
}

int CEndpointManager::GetEndpointScore(const tcp::endpoint& ep)
{
    return mapAddressStatus[ep.address()].nScore;
}

void CEndpointManager::GetBanned(std::vector<CAddressBanned>& vBanned)
{
    int64 now = GetTime();
    map<boost::asio::ip::address, CAddressStatus>::iterator it = mapAddressStatus.begin();
    while (it != mapAddressStatus.end())
    {
        CAddressStatus& status = (*it).second;
        if (now < status.nBanTo)
        {
            vBanned.push_back(CAddressBanned((*it).first, status.nScore, status.nBanTo - now));
        }
        ++it;
    }
}

void CEndpointManager::SetBan(std::vector<boost::asio::ip::address>& vAddrToBan, int64 nBanTime)
{
    int64 now = GetTime();
    for (const boost::asio::ip::address& addr : vAddrToBan)
    {
        CAddressStatus& status = mapAddressStatus[addr];
        status.nBanTo = now + nBanTime;
        status.nLastSeen = now;
        mngrNode.Ban(addr, status.nBanTo);
    }
}

void CEndpointManager::ClearBanned(vector<boost::asio::ip::address>& vAddrToClear)
{
    int64 now = GetTime();
    for (const boost::asio::ip::address& addr : vAddrToClear)
    {
        map<boost::asio::ip::address, CAddressStatus>::iterator it = mapAddressStatus.find(addr);
        if (it != mapAddressStatus.end() && now < (*it).second.nBanTo)
        {
            mapAddressStatus.erase(it);
        }
    }
}

void CEndpointManager::ClearAllBanned()
{
    int64 now = GetTime();
    map<boost::asio::ip::address, CAddressStatus>::iterator it = mapAddressStatus.begin();
    while (it != mapAddressStatus.end())
    {
        if (now < (*it).second.nBanTo)
        {
            mapAddressStatus.erase(it++);
        }
        else
        {
            ++it;
        }
    }
}

void CEndpointManager::AddNewOutBound(const tcp::endpoint& ep, const string& strName,
                                      const boost::any& data)
{
    mngrNode.AddNew(ep, strName, data);
}

void CEndpointManager::RemoveOutBound(const tcp::endpoint& ep)
{
    mngrNode.Remove(ep);
}

string CEndpointManager::GetOutBoundName(const boost::asio::ip::tcp::endpoint& ep)
{
    return mngrNode.GetName(ep);
}

bool CEndpointManager::GetOutBoundData(const tcp::endpoint& ep, boost::any& dataRet)
{
    return mngrNode.GetData(ep, dataRet);
}

bool CEndpointManager::SetOutBoundData(const tcp::endpoint& ep, const boost::any& dataIn)
{
    return mngrNode.SetData(ep, dataIn);
}

bool CEndpointManager::FetchOutBound(tcp::endpoint& ep)
{
    while (mngrNode.Employ(ep))
    {
        CAddressStatus& status = mapAddressStatus[ep.address()];
        if (status.AddConnection(false))
        {
            return true;
        }
        mngrNode.Dismiss(ep, false);
    }
    return false;
}

int CEndpointManager::AcceptInBound(const tcp::endpoint& ep)
{
    int64 now = GetTime();
    CAddressStatus& status = mapAddressStatus[ep.address()];
    if (!status.InBoundAttempt(now))
    {
        return -1;
    }
    if (!status.AddConnection(true))
    {
        return -2;
    }
    return 0;
}

void CEndpointManager::RewardEndpoint(const tcp::endpoint& ep, Bonus bonus)
{
    const int award[NUM_BONUS] = { 1, 2, 3, 5 };
    int index = (int)bonus;
    if (index < 0 || index >= NUM_BONUS)
    {
        index = 0;
    }
    CAddressStatus& status = mapAddressStatus[ep.address()];
    status.Reward(award[index], GetTime());

    CleanInactiveAddress();
}

void CEndpointManager::CloseEndpoint(const tcp::endpoint& ep, CloseReason reason)
{
    int64 now = GetTime();
    const int lost[NUM_CLOSEREASONS] = { 0, 1, 2, 2, 10, 25 };
    int index = (int)reason;
    if (index < 0 || index >= NUM_CLOSEREASONS)
    {
        index = 0;
    }
    CAddressStatus& status = mapAddressStatus[ep.address()];
    status.Penalize(lost[index], now);
    mngrNode.Dismiss(ep, (reason == NETWORK_ERROR));
    status.RemoveConnection();

    if (now < status.nBanTo)
    {
        mngrNode.Ban(ep.address(), status.nBanTo);
    }

    CleanInactiveAddress();
}

void CEndpointManager::RetrieveGoodNode(vector<CNodeAvail>& vGoodNode,
                                        int64 nActiveTime, size_t nMaxCount)
{
    vector<CNode> vNode;
    mngrNode.Retrieve(vNode);
    int64 nActive = GetTime() - nActiveTime;
    multimap<int, CNodeAvail> mapScore;
    for (const CNode& node : vNode)
    {
        const boost::asio::ip::address addr = node.ep.address();
        map<boost::asio::ip::address, CAddressStatus>::iterator it = mapAddressStatus.find(addr);
        if (it != mapAddressStatus.end()
            && (*it).second.nLastSeen > nActive && (*it).second.nScore >= 0)
        {
            mapScore.insert(make_pair(-(*it).second.nScore, CNodeAvail(node, (*it).second.nLastSeen)));
        }
    }

    for (multimap<int, CNodeAvail>::iterator it = mapScore.begin();
         it != mapScore.end() && vGoodNode.size() < nMaxCount; ++it)
    {
        vGoodNode.push_back((*it).second);
    }
}

void CEndpointManager::CleanInactiveAddress()
{
    if (mapAddressStatus.size() <= MAX_ADDRESS_COUNT)
    {
        return;
    }

    int64 inactive = GetTime() - MAX_INACTIVE_TIME;
    multimap<int64, boost::asio::ip::address> mapLastSeen;
    map<boost::asio::ip::address, CAddressStatus>::iterator it = mapAddressStatus.begin();
    while (it != mapAddressStatus.end())
    {
        CAddressStatus& status = (*it).second;
        if (status.nLastSeen > inactive)
        {
            mapLastSeen.insert(make_pair(status.nLastSeen, (*it).first));
            ++it;
        }
        else
        {
            mapAddressStatus.erase(it++);
        }
    }

    multimap<int64, boost::asio::ip::address>::iterator mi = mapLastSeen.begin();
    while (mapAddressStatus.size() > MAX_ADDRESS_COUNT && mi != mapLastSeen.end())
    {
        mapAddressStatus.erase((*mi).second);
        ++mi;
    }
}

} // namespace xengine
