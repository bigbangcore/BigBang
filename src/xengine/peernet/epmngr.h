// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_PEERNET_EPMANAGER_H
#define XENGINE_PEERNET_EPMANAGER_H

#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <map>
#include <vector>

#include "peernet/nodemngr.h"
#include "type.h"

namespace xengine
{

class CConnAttempt
{
public:
    CConnAttempt();
    bool Attempt(int64 ts);

protected:
    enum
    {
        ATTEMPT_LIMIT = 8
    };
    enum
    {
        COUNTER_INTERVAL = 10,
        COUNTER_NUM = 8,
        COUNTER_MASK = 7
    };
    int arrayAttempt[COUNTER_NUM];
    int nAttempts;
    int nStartIndex;
    int64 nStartTime;
};

class CAddressStatus
{
public:
    CAddressStatus();
    bool InBoundAttempt(int64 ts, std::string& strFailCause);
    bool AddConnection(bool fInBound, bool fLimitConnect = true);
    void RemoveConnection();
    void Reward(int nPoints, int64 ts);
    void Penalize(int nPoints, int64 ts);

public:
    enum
    {
        CONNECTION_LIMIT = 5
    };
    enum
    {
        MIN_SCORE = -500,
        MAX_SCORE = 100
    };
    enum
    {
        BANTIME_BASE = 20,
        ATTEMPT_PENALTY = 50
    };
    int nScore;
    int64 nLastSeen;
    int64 nBanTo;

protected:
    CConnAttempt connAttempt;
    int nConnections;
};

class CAddressBanned
{
public:
    CAddressBanned(const boost::asio::ip::address addrBannedIn, int nScoreIn, int64 nBanTimeIn)
      : addrBanned(addrBannedIn), nScore(nScoreIn), nBanTime(nBanTimeIn)
    {
    }

public:
    boost::asio::ip::address addrBanned;
    int nScore;
    int64 nBanTime;
};

class CNodeAvail
{
public:
    CNodeAvail() {}
    CNodeAvail(const CNode& node, int64 nTimeIn)
      : ep(node.ep), data(node.data), nTime(nTimeIn) {}

public:
    boost::asio::ip::tcp::endpoint ep;
    boost::any data;
    int64 nTime;
};

class CEndpointManager
{
public:
    typedef enum
    {
        HOST_CLOSE = 0,
        CONNECT_FAILURE,
        NETWORK_ERROR,
        RESPONSE_FAILURE,
        PROTOCOL_INVALID,
        DDOS_ATTACK,
        NUM_CLOSEREASONS
    } CloseReason;
    typedef enum
    {
        OPERATION_DONE = 0,
        MINOR_DATA,
        MAJOR_DATA,
        VITAL_DATA,
        NUM_BONUS
    } Bonus;

public:
    void Clear();
    int GetEndpointScore(const boost::asio::ip::tcp::endpoint& ep);
    void GetBanned(std::vector<CAddressBanned>& vBanned);
    void SetBan(std::vector<boost::asio::ip::address>& vAddrToBan, int64 nBanTime);
    void ClearBanned(std::vector<boost::asio::ip::address>& vAddrToClear);
    void ClearAllBanned();

    void AddNewOutBound(const boost::asio::ip::tcp::endpoint& ep, const std::string& strName,
                        const boost::any& data);
    void RemoveOutBound(const boost::asio::ip::tcp::endpoint& ep);
    std::string GetOutBoundName(const boost::asio::ip::tcp::endpoint& ep);
    bool GetOutBoundData(const boost::asio::ip::tcp::endpoint& ep, boost::any& dataRet);
    bool SetOutBoundData(const boost::asio::ip::tcp::endpoint& ep, const boost::any& dataIn);
    bool FetchOutBound(boost::asio::ip::tcp::endpoint& ep);
    bool AcceptInBound(const boost::asio::ip::tcp::endpoint& ep, std::string& strFailCause);
    void RewardEndpoint(const boost::asio::ip::tcp::endpoint& ep, Bonus bonus);
    void CloseEndpoint(const boost::asio::ip::tcp::endpoint& ep, CloseReason reason);
    void RetrieveGoodNode(std::vector<CNodeAvail>& vGoodNode,
                          int64 nActiveTime, std::size_t nMaxCount);
    int GetCandidateNodeCount()
    {
        return mngrNode.GetCandidateNodeCount();
    }

protected:
    void CleanInactiveAddress();

protected:
    enum
    {
        MAX_ADDRESS_COUNT = 2048,
        MAX_INACTIVE_TIME = 864000
    };
    CNodeManager mngrNode;
    std::map<boost::asio::ip::address, CAddressStatus> mapAddressStatus;
};

} // namespace xengine

#endif //XENGINE_PEERNET_EPMANAGER_H
