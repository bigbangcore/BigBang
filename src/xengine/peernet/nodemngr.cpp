// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php

#include "nodemngr.h"

#include <vector>

#include "util.h"

using namespace std;
using boost::asio::ip::tcp;

namespace xengine
{

///////////////////////////////
// CNodeManager

CNodeManager::CNodeManager()
{
}

void CNodeManager::AddNew(const tcp::endpoint& ep, const string& strName, const boost::any& data)
{
    if (mapNode.insert(make_pair(ep, CNode(ep, strName, data))).second)
    {
        mapIdle.insert(make_pair(GetTime(), ep));
        if (mapIdle.size() > MAX_IDLENODES)
        {
            RemoveInactiveNodes();
        }
    }
}

void CNodeManager::Remove(const tcp::endpoint& ep)
{
    map<tcp::endpoint, CNode>::iterator mi = mapNode.find(ep);
    if (mi != mapNode.end())
    {
        mapNode.erase(mi);

        for (multimap<int64, tcp::endpoint>::iterator it = mapIdle.begin();
             it != mapIdle.end(); ++it)
        {
            if ((*it).second == ep)
            {
                mapIdle.erase(it);
                break;
            }
        }
    }
}

string CNodeManager::GetName(const tcp::endpoint& ep)
{
    map<tcp::endpoint, CNode>::iterator mi = mapNode.find(ep);
    if (mi != mapNode.end())
    {
        return (*mi).second.strName;
    }
    return "";
}

bool CNodeManager::GetData(const tcp::endpoint& ep, boost::any& dataRet)
{
    map<tcp::endpoint, CNode>::iterator mi = mapNode.find(ep);
    if (mi != mapNode.end())
    {
        CNode& node = (*mi).second;
        if (!node.data.empty())
        {
            dataRet = node.data;
            return true;
        }
    }
    return false;
}

bool CNodeManager::SetData(const tcp::endpoint& ep, const boost::any& dataIn)
{
    map<tcp::endpoint, CNode>::iterator mi = mapNode.find(ep);
    if (mi != mapNode.end())
    {
        CNode& node = (*mi).second;
        node.data = dataIn;
        return true;
    }
    return false;
}

void CNodeManager::Clear()
{
    mapNode.clear();
    mapIdle.clear();
}

void CNodeManager::Ban(const boost::asio::ip::address& address, int64 nBanTo)
{
    vector<tcp::endpoint> vNode;
    multimap<int64, tcp::endpoint>::iterator it = mapIdle.begin();
    while (it != mapIdle.upper_bound(nBanTo))
    {
        if (address == (*it).second.address())
        {
            vNode.push_back((*it).second);
            mapIdle.erase(it++);
        }
        else
        {
            ++it;
        }
    }

    for (const tcp::endpoint& ep : vNode)
    {
        mapIdle.insert(make_pair(nBanTo, ep));
    }
}

bool CNodeManager::Employ(tcp::endpoint& ep)
{
    multimap<int64, tcp::endpoint>::iterator it = mapIdle.begin();
    if (it != mapIdle.upper_bound(GetTime()))
    {
        ep = (*it).second;
        mapIdle.erase(it);
        return true;
    }
    return false;
}

void CNodeManager::Dismiss(const tcp::endpoint& ep, bool fForceRetry)
{
    map<tcp::endpoint, CNode>::iterator it = mapNode.find(ep);
    if (it != mapNode.end())
    {
        CNode& node = (*it).second;
        node.nRetries = (fForceRetry ? 0 : node.nRetries + 1);
        if (node.nRetries <= MAX_RETRIES)
        {
            int64 nIdleTo = GetTime() + (RETRY_INTERVAL_BASE << node.nRetries);
            mapIdle.insert(make_pair(nIdleTo, node.ep));
            if (mapIdle.size() > MAX_IDLENODES)
            {
                RemoveInactiveNodes();
            }
        }
        else
        {
            mapNode.erase(it);
        }
    }
}

void CNodeManager::Retrieve(vector<CNode>& vNode)
{
    for (map<tcp::endpoint, CNode>::iterator it = mapNode.begin(); it != mapNode.end(); ++it)
    {
        vNode.push_back((*it).second);
    }
}

void CNodeManager::RemoveInactiveNodes()
{
    int64 inactive = GetTime() + MAX_IDLETIME;
    int nRemoved = 0;

    multimap<int64, tcp::endpoint>::reverse_iterator rit = mapIdle.rbegin();
    while (rit != mapIdle.rend() && nRemoved < REMOVE_COUNT)
    {
        if ((*rit).first > inactive || nRemoved == 0)
        {
            mapNode.erase((*rit).second);
            mapIdle.erase((++rit).base());
            nRemoved++;
        }
        else
        {
            break;
        }
    }
}

} // namespace xengine
