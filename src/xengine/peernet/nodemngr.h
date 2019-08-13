// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_PEERNET_NODEMANAGER_H
#define XENGINE_PEERNET_NODEMANAGER_H

#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <map>
#include <string>

#include "type.h"

namespace xengine
{

class CNode
{
public:
    CNode() {}
    CNode(const boost::asio::ip::tcp::endpoint& epIn, const std::string& strNameIn,
          const boost::any& dataIn)
      : ep(epIn), strName(strNameIn), data(dataIn), nRetries(0) {}

public:
    boost::asio::ip::tcp::endpoint ep;
    std::string strName;
    boost::any data;
    int nRetries;
};

class CNodeManager
{
public:
    CNodeManager();
    void AddNew(const boost::asio::ip::tcp::endpoint& ep, const std::string& strName,
                const boost::any& data);
    void Remove(const boost::asio::ip::tcp::endpoint& ep);
    std::string GetName(const boost::asio::ip::tcp::endpoint& ep);
    bool GetData(const boost::asio::ip::tcp::endpoint& ep, boost::any& dataRet);
    bool SetData(const boost::asio::ip::tcp::endpoint& ep, const boost::any& dataIn);
    void Clear();
    void Ban(const boost::asio::ip::address& address, int64 nBanTo);
    bool Employ(boost::asio::ip::tcp::endpoint& ep);
    void Dismiss(const boost::asio::ip::tcp::endpoint& ep, bool fForceRetry);
    void Retrieve(std::vector<CNode>& vNode);
    int GetCandidateNodeCount()
    {
        return mapNode.size();
    }

protected:
    void RemoveInactiveNodes();

protected:
    enum
    {
        MAX_IDLENODES = 512,
        REMOVE_COUNT = 16
    };
    enum
    {
        RETRY_INTERVAL_BASE = 30,
        MAX_RETRIES = 16,
        MAX_IDLETIME = 28800
    };
    std::map<boost::asio::ip::tcp::endpoint, CNode> mapNode;
    std::multimap<int64, boost::asio::ip::tcp::endpoint> mapIdle;
};

} // namespace xengine

#endif //XENGINE_PEERNET_NODEMANAGER_H
