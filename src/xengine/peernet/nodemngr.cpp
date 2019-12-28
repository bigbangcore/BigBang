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
        StdLog("CNodeManager", "AddNew: Add idle node by AddNew, peer: %s, interval time: 0 s", GetEpString(ep).c_str());
        AddIdleNode(0, ep);
        if (strName == "dnseed")
        {
            setDnseedAddress.insert(ep.address());
        }
    }
}

void CNodeManager::Remove(const tcp::endpoint& ep)
{
    map<tcp::endpoint, CNode>::iterator mi = mapNode.find(ep);
    if (mi != mapNode.end())
    {
        StdLog("CNodeManager", "Remove: Remove node, node: %s", GetEpString(ep).c_str());
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
            StdLog("CNodeManager", "Ban: erase idle node, peer: %s", GetEpString(it->second).c_str());
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
        StdLog("CNodeManager", "Ban: Add idle node, peer: %s, interval time: %ld s", GetEpString(ep).c_str(), nBanTo - GetTime());
    }
}

bool CNodeManager::Employ(tcp::endpoint& ep)
{
    multimap<int64, tcp::endpoint>::iterator it = mapIdle.begin();
    if (it != mapIdle.upper_bound(GetTime()))
    {
        ep = (*it).second;
        mapIdle.erase(it);
        StdLog("CNodeManager", "Employ node, peer: %s", GetEpString(ep).c_str());
        return true;
    }
    return false;
}

void CNodeManager::Dismiss(const tcp::endpoint& ep, bool fForceRetry, bool fReset)
{
    map<tcp::endpoint, CNode>::iterator it = mapNode.find(ep);
    if (it != mapNode.end())
    {
        CNode& node = (*it).second;
        if (fReset)
        {
            StdLog("CNodeManager", "Dismiss: Add idle node by reset, peer: %s, interval time: %d s",
                   GetEpString(ep).c_str(), RETRY_INTERVAL_BASE);
            AddIdleNode(RETRY_INTERVAL_BASE, node.ep);
        }
        else
        {
            if (node.strName == "dnseed")
            {
                if (node.nRetries >= MAX_RETRIES / 2)
                {
                    node.nRetries = 0;
                }
                else
                {
                    node.nRetries++;
                }
                StdLog("CNodeManager", "Dismiss: Add idle node by dnseed, peer: %s, nRetries: %d, interval time: %ld s",
                       GetEpString(ep).c_str(), node.nRetries, (RETRY_INTERVAL_BASE << node.nRetries));
                AddIdleNode((RETRY_INTERVAL_BASE << node.nRetries), node.ep);
            }
            else
            {
                node.nRetries = (fForceRetry ? 0 : node.nRetries + 1);
                if (node.nRetries <= MAX_RETRIES)
                {
                    StdLog("CNodeManager", "Dismiss: Add idle node, peer: %s, nRetries: %d, interval time: %ld s",
                           GetEpString(ep).c_str(), node.nRetries, (RETRY_INTERVAL_BASE << node.nRetries));
                    AddIdleNode((RETRY_INTERVAL_BASE << node.nRetries), node.ep);
                }
                else
                {
                    StdLog("CNodeManager", "Dismiss: too many reconnections, remove node, peer: %s, nRetries: %d",
                           GetEpString(ep).c_str(), node.nRetries);
                    mapNode.erase(it);
                }
            }
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

bool CNodeManager::IsDnseedAddress(const boost::asio::ip::address& address)
{
    if (setDnseedAddress.find(address) != setDnseedAddress.end())
    {
        return true;
    }
    return false;
}

void CNodeManager::AddIdleNode(int64 nIntervalTime, const boost::asio::ip::tcp::endpoint& ep)
{
    mapIdle.insert(make_pair(GetTime() + nIntervalTime, ep));
    if (mapIdle.size() > MAX_IDLENODES)
    {
        RemoveInactiveNodes();
    }
}

void CNodeManager::RemoveInactiveNodes()
{
    int64 inactive = GetTime() + MAX_IDLETIME;
    int nRemoved = 0;
    int nMaxRemoveCount = mapIdle.size() - MAX_IDLENODES;
    if (nMaxRemoveCount > REMOVE_COUNT)
    {
        nMaxRemoveCount = REMOVE_COUNT;
    }

    multimap<int64, tcp::endpoint>::reverse_iterator rit = mapIdle.rbegin();
    while (rit != mapIdle.rend() && nRemoved < nMaxRemoveCount)
    {
        if ((*rit).first > inactive || nRemoved == 0)
        {
            if (GetName(rit->second) != "dnseed")
            {
                StdLog("CNodeManager", "RemoveInactiveNodes: remove node, peer: %s", GetEpString(rit->second).c_str());
                mapNode.erase((*rit).second);
                mapIdle.erase((++rit).base());
                nRemoved++;
            }
        }
        else
        {
            break;
        }
    }
}

const std::string CNodeManager::GetEpString(const tcp::endpoint& ep)
{
    if (ep.address().is_v6())
    {
        return string("[") + ep.address().to_string() + string("]:") + to_string(ep.port());
    }
    return ep.address().to_string() + string(":") + to_string(ep.port());
}

} // namespace xengine
