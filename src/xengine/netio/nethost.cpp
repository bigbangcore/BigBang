// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "nethost.h"

#include <sstream>

using namespace std;
using boost::asio::ip::tcp;

namespace xengine
{

// IPv6 [host]:port
// IPv4 host:port
// domain name domain:port
// refer to https://stackoverflow.com/questions/186829/how-do-ports-work-with-ipv6

///////////////////////////////
// CNetHost
CNetHost::CNetHost()
  : strHost(""), nPort(0), strName("")
{
}

CNetHost::CNetHost(const string& strHostIn, unsigned short nPortIn, const string& strNameIn,
                   const boost::any& dataIn)
  : strName(!strNameIn.empty() ? strNameIn : strHost),
    data(dataIn)
{
    SetHostPort(strHostIn, nPortIn);
}

CNetHost::CNetHost(const tcp::endpoint& ep, const string& strNameIn, const boost::any& dataIn)
  : strHost(ep.address().to_string()), nPort(ep.port()),
    strName(!strNameIn.empty() ? strNameIn : strHost),
    data(dataIn)
{
}

bool CNetHost::IsEmpty() const
{
    if (strHost.empty() || nPort == 0)
    {
        return true;
    }
    return false;
}

int CNetHost::GetHostType() const
{
    if (strHost.empty())
    {
        return NH_EMPTY;
    }

    boost::system::error_code ec;
    boost::asio::ip::address addr(boost::asio::ip::address::from_string(strHost, ec));
    if (!ec)
    {
        if (addr.is_v6())
        {
            return NH_IPV6;
        }
        else
        {
            return NH_IPV4;
        }
    }
    else
    {
        return NH_DOMAINNAME;
    }
}

bool CNetHost::SetHostPort(const std::string& strHostIn, unsigned short nDefPortIn)
{
    if (strHostIn.empty())
    {
        return false;
    }

    char sTempIpBuf[128] = { 0 };
    unsigned short usTempPort = 0;
    if (!SplitHostPort(strHostIn.c_str(), sTempIpBuf, sizeof(sTempIpBuf), &usTempPort))
    {
        return false;
    }

    strHost = sTempIpBuf;
    if (usTempPort == 0)
    {
        nPort = nDefPortIn;
    }
    else
    {
        nPort = usTempPort;
    }
    return true;
}

bool CNetHost::Set(const std::string& strHostIn, unsigned short nDefPortIn,
                   const std::string& strNameIn, const boost::any& dataIn)
{
    if (!SetHostPort(strHostIn, nDefPortIn))
    {
        return false;
    }
    strName = (!strNameIn.empty() ? strNameIn : strHost);
    data = dataIn;
    return true;
}

bool CNetHost::Set(const boost::asio::ip::tcp::endpoint& ep,
                   const std::string& strNameIn, const boost::any& dataIn)
{
    strHost = ep.address().to_string();
    nPort = ep.port();
    strName = (!strNameIn.empty() ? strNameIn : strHost);
    data = dataIn;
    return true;
}

const string CNetHost::ToString() const
{
    if (strHost.empty())
    {
        return string("");
    }

    boost::system::error_code ec;
    boost::asio::ip::address addr(boost::asio::ip::address::from_string(strHost, ec));
    if (!ec)
    {
        if (nPort)
        {
            if (addr.is_v6())
            {
                return (string("[") + strHost + string("]:") + to_string(nPort));
            }
            else
            {
                return (strHost + string(":") + to_string(nPort));
            }
        }
        else
        {
            return strHost;
        }
    }
    else
    {
        if (nPort)
        {
            return (strHost + string(":") + to_string(nPort));
        }
        else
        {
            return strHost;
        }
    }
}

const tcp::endpoint CNetHost::ToEndPoint() const
{
    if (IsEmpty())
    {
        return tcp::endpoint();
    }

    boost::system::error_code ec;
    tcp::endpoint ep(boost::asio::ip::address::from_string(strHost, ec), nPort);
    return ((!ec && nPort) ? ep : tcp::endpoint());
}

bool CNetHost::SplitHostPort(const char* pAddr, char* pHost, int iHostBufSize, unsigned short* pPort)
{
    char *pos, *findpos;
    int len;

    if (pAddr == nullptr || pHost == nullptr || iHostBufSize <= 0 || pPort == nullptr)
    {
        return false;
    }

    pos = (char*)pAddr;
    while (*pos && *pos == ' ')
        pos++;
    if (*pos == 0)
        return false;

    if (*pos == '[')
    {
        pos++;
        findpos = strchr(pos, ']');
        if (findpos == nullptr)
            return false;
        len = findpos - pos;
        if (len <= 0 || len >= iHostBufSize)
            return false;
        memcpy(pHost, pos, len);
        pHost[len] = 0;

        pos = findpos + 1;
        while (*pos && *pos == ' ')
            pos++;
        if (*pos == 0)
        {
            *pPort = 0;
            return true;
        }
        if (*pos != ':')
            return false;

        pos++;
        while (*pos && *pos == ' ')
            pos++;
        if (*pos == 0)
            *pPort = 0;
        else
            *pPort = atoi(pos);
    }
    else
    {
        findpos = strchr(pos, ':');
        if (findpos == nullptr)
        {
            len = strlen(pos);
            if (len <= 0 || len >= iHostBufSize)
                return false;
            memcpy(pHost, pos, len);
            pHost[len] = 0;
            *pPort = 0;
        }
        else
        {
            len = findpos - pos;
            if (len <= 0 || len >= iHostBufSize)
                return false;
            memcpy(pHost, pos, len);
            pHost[len] = 0;

            pos = findpos + 1;
            while (*pos && *pos == ' ')
                pos++;
            if (*pos == 0)
                *pPort = 0;
            else
                *pPort = atoi(pos);
        }
    }
    return true;
}

} // namespace xengine
