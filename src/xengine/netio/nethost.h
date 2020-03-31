// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_NETIO_NETHOST_H
#define XENGINE_NETIO_NETHOST_H

#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <string>

namespace xengine
{

class CNetHost
{
public:
    enum
    {
        NH_EMPTY = 0,
        NH_DOMAINNAME = 1,
        NH_IPV4 = 4,
        NH_IPV6 = 6
    };

    CNetHost();
    CNetHost(const std::string& strHostIn, unsigned short nPortIn,
             const std::string& strNameIn = "", const boost::any& dataIn = boost::any());
    CNetHost(const boost::asio::ip::tcp::endpoint& ep,
             const std::string& strNameIn = "", const boost::any& dataIn = boost::any());

    friend bool operator==(const CNetHost& a, const CNetHost& b)
    {
        return (a.strHost == b.strHost && a.nPort == b.nPort);
    }
    friend bool operator<(const CNetHost& a, const CNetHost& b)
    {
        return (a.strHost < b.strHost
                || (a.strHost == b.strHost && a.nPort < b.nPort));
    }

    bool IsEmpty() const;
    int GetHostType() const;
    bool SetHostPort(const std::string& strHostIn, unsigned short nDefPortIn);
    bool Set(const std::string& strHostIn, unsigned short nDefPortIn,
             const std::string& strNameIn = "", const boost::any& dataIn = boost::any());
    bool Set(const boost::asio::ip::tcp::endpoint& ep,
             const std::string& strNameIn = "", const boost::any& dataIn = boost::any());
    const std::string ToString() const;
    const boost::asio::ip::tcp::endpoint ToEndPoint() const;

    static bool SplitHostPort(const char* pAddr, char* pHost, int iHostBufSize, unsigned short* pPort);

public:
    std::string strHost;
    unsigned short nPort;
    std::string strName;
    boost::any data;
};

} // namespace xengine

#endif //XENGINE_NETIO_NETHOST_H
