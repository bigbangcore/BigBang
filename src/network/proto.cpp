// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "proto.h"

#include <boost/asio.hpp>

using namespace std;
using namespace xengine;
using boost::asio::ip::tcp;

namespace bigbang
{
namespace network
{

//////////////////////////////
// CPeerMessageHeader

///////////////////////////////
// CEndpoint

static const unsigned char epipv4[CEndpoint::BINSIZE] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0xff,
    0xff,
};
CEndpoint::CEndpoint()
  : CBinary((char*)ss, BINSIZE)
{
    memmove(ss, epipv4, BINSIZE);
}

CEndpoint::CEndpoint(const tcp::endpoint& ep)
  : CBinary((char*)ss, BINSIZE)
{
    memmove(ss, epipv4, BINSIZE);
    if (ep.address().is_v4())
    {
        //TODO
        memmove(&ss[12], ep.address().to_v4().to_bytes().data(), 4);
    }
    else
    {
        //TODO
        memmove(&ss[0], ep.address().to_v6().to_bytes().data(), 16);
    }
    ss[16] = ep.port() >> 8;
    ss[17] = ep.port() & 0xFF;
}

CEndpoint::CEndpoint(const CEndpoint& other)
  : CBinary((char*)ss, BINSIZE)
{
    other.CopyTo(ss);
}

void CEndpoint::GetEndpoint(tcp::endpoint& ep)
{
    using namespace boost::asio;

    if (memcmp(ss, epipv4, 12) == 0)
    {
        ip::address_v4::bytes_type bytes;
        //TODO
        memmove(bytes.data(), &ss[12], 4);
        ep.address(ip::address(ip::address_v4(bytes)));
    }
    else
    {
        ip::address_v6::bytes_type bytes;
        //TODO
        memmove(bytes.data(), &ss[0], 16);
        ep.address(ip::address(ip::address_v6(bytes)));
    }
    ep.port((((unsigned short)ss[16]) << 8) + ss[17]);
}

void CEndpoint::CopyTo(unsigned char* ssTo) const
{
    memmove(ssTo, ss, BINSIZE);
}

bool CEndpoint::IsRoutable()
{
    if (memcmp(epipv4, ss, 12) == 0)
    {
        // IPV4
        //RFC1918
        if (ss[12] == 10 || (ss[12] == 192 && ss[13] == 168)
            || (ss[12] == 172 && ss[13] >= 16 && ss[13] <= 31))
        {
            return false;
        }

        //RFC3927
        if (ss[12] == 169 && ss[13] == 254)
        {
            return false;
        }

        //Local
        if (ss[12] == 127 || ss[12] == 0)
        {
            return false;
        }
    }
    else
    {
        // IPV6
        //RFC4862
        const unsigned char pchRFC4862[] = { 0xFE, 0x80, 0, 0, 0, 0, 0, 0 };
        if (memcmp(ss, pchRFC4862, sizeof(pchRFC4862)) == 0)
        {
            return false;
        }

        //RFC4193
        if ((ss[0] & 0xFE) == 0xFC)
        {
            return false;
        }

        //RFC4843
        if (ss[0] == 0x20 && ss[1] == 0x01 && ss[2] == 0x00 && (ss[3] & 0xF0) == 0x10)
        {
            return false;
        }

        //Local
        const unsigned char pchLocal[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
        if (memcmp(ss, pchLocal, sizeof(pchLocal)) == 0)
        {
            return false;
        }
    }
    return true;
}

} // namespace network
} // namespace bigbang
