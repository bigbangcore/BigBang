// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_UTIL_H
#define XENGINE_UTIL_H

#include <boost/asio/ip/address.hpp>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/common.hpp>

#include "type.h"

namespace xengine
{
extern bool STD_DEBUG;

void SetThreadName(const char* name);

std::string GetThreadName();

void PrintTrace();

inline int64 GetTime()
{
    using namespace boost::posix_time;
    static ptime epoch(boost::gregorian::date(1970, 1, 1));
    return int64((second_clock::universal_time() - epoch).total_seconds());
}

inline int64 GetLocalTimeSeconds()
{
    using namespace boost::posix_time;
    static ptime epoch(boost::gregorian::date(1970, 1, 1));
    return int64((second_clock::local_time() - epoch).total_seconds());
}

inline int64 GetTimeMillis()
{
    using namespace boost::posix_time;
    static ptime epoch(boost::gregorian::date(1970, 1, 1));
    return int64((microsec_clock::universal_time() - epoch).total_milliseconds());
}

inline std::string GetLocalTime()
{
    using namespace boost::posix_time;
    time_facet* facet = new time_facet("%Y-%m-%d %H:%M:%s");
    std::stringstream ss;
    ss.imbue(std::locale(std::locale("C"), facet));
    ss << microsec_clock::local_time();
    return ss.str();
}

inline std::string GetUniversalTime()
{
    using namespace boost::posix_time;
    time_facet* facet = new time_facet("%Y-%m-%d %H:%M:%S");
    std::stringstream ss;
    ss.imbue(std::locale(std::locale("C"), facet));
    ss << second_clock::universal_time();
    return ss.str();
}

class CTicks
{
public:
    CTicks()
      : t(boost::posix_time::microsec_clock::universal_time()) {}
    int64 operator-(const CTicks& ticks) const
    {
        return ((t - ticks.t).ticks());
    }
    int64 Elapse() const
    {
        return ((boost::posix_time::microsec_clock::universal_time() - t).ticks());
    }

public:
    boost::posix_time::ptime t;
};

enum severity_level
{
    debug,
    info,
    warn,
    error
};

namespace src = boost::log::sources;

typedef src::severity_channel_logger_mt<severity_level, std::string> sclmt_type;
BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(lg, sclmt_type)

void StdDebug(const char* pszName, const char* pszErr);
void StdLog(const char* pszName, const char* pszErr);
void StdWarn(const char* pszName, const char* pszErr);
void StdError(const char* pszName, const char* pszErr);

bool InitLog(const boost::filesystem::path& pathData, bool debug, bool daemon);

inline std::string PulsFileLine(const char* file, int line, const char* info)
{
    std::stringstream ss;
    ss << file << "(" << line << ") " << info;
    return ss.str();
}

#define STD_DEBUG(Mod, Info) xengine::StdDebug(Mod, xengine::PulsFileLine(__FILE__, __LINE__, Info).c_str())

#define STD_LOG(Mod, Info) xengine::StdLog(Mod, xengine::PulsFileLine(__FILE__, __LINE__, Info).c_str())

#define STD_WARN(Mod, Info) xengine::StdWarn(Mod, xengine::PulsFileLine(__FILE__, __LINE__, Info).c_str())

#define STD_Eerror(Mod, Info) xengine::StdError(Mod, xengine::PulsFileLine(__FILE__, __LINE__, Info).c_str())

inline bool IsRoutable(const boost::asio::ip::address& address)
{
    if (address.is_loopback() || address.is_unspecified())
    {
        return false;
    }
    if (address.is_v4())
    {
        unsigned long u = address.to_v4().to_ulong();

        // RFC1918 https://tools.ietf.org/html/rfc1918
        // IP address space for private internets
        // 0x0A000000 => 10.0.0.0 prefix
        // 0xC0A80000 => 192.168.0.0 prefix
        // 0x0xAC100000 - 0xAC200000 => 172.16.0.0 - 172.31.0.0 prefix
        if ((u & 0xFF000000) == 0x0A000000 || (u & 0xFFFF0000) == 0xC0A80000
            || (u >= 0xAC100000 && u < 0xAC200000))
        {
            return false;
        }

        // RFC3927 https://tools.ietf.org/html/rfc3927
        // IPv4 Link-Local addresses
        // 0xA9FE0000 => 169.254.0.0 prefix
        if ((u & 0xFFFF0000) == 0xA9FE0000)
        {
            return false;
        }
    }
    else
    {
        boost::asio::ip::address_v6::bytes_type b = address.to_v6().to_bytes();

        // RFC4862 https://tools.ietf.org/html/rfc4862
        // IPv6 Link-local addresses
        // FE80::/64
        if (b[0] == 0xFE && b[1] == 0x80 && b[2] == 0 && b[3] == 0
            && b[4] == 0 && b[5] == 0 && b[6] == 0 && b[7] == 0)
        {
            return false;
        }

        // RFC4193 https://tools.ietf.org/html/rfc4193
        // Local IPv6 Unicast Addresses
        // FC00::/7 prefix
        if ((b[0] & 0xFE) == 0xFC)
        {
            return false;
        }

        // RFC4843 https://tools.ietf.org/html/rfc4843
        // An IPv6 Prefix for Overlay Routable Cryptographic Hash Identifiers
        // 2001:10::/28 prefix
        if (b[0] == 0x20 && b[1] == 0x01 && b[2] == 0x00 && (b[3] & 0xF0) == 0x10)
        {
            return false;
        }
    }
    return true;
}

inline std::string ToHexString(const unsigned char* p, std::size_t size)
{
    const char hexc[17] = "0123456789abcdef";
    char hex[128];
    std::string strHex;
    strHex.reserve(size * 2);

    for (size_t i = 0; i < size; i += 64)
    {
        size_t k;
        for (k = 0; k < 64 && k + i < size; k++)
        {
            int c = *p++;
            hex[k * 2] = hexc[c >> 4];
            hex[k * 2 + 1] = hexc[c & 15];
        }
        strHex.append(hex, k * 2);
    }
    return strHex;
}

inline std::string ToHexString(const std::vector<unsigned char>& vch)
{
    return ToHexString(&vch[0], vch.size());
}

template <typename T>
inline std::string UIntToHexString(const T& t)
{
    const char hexc[17] = "0123456789abcdef";
    char hex[sizeof(T) * 2 + 1];
    for (std::size_t i = 0; i < sizeof(T); i++)
    {
        int byte = (t >> ((sizeof(T) - i - 1)) * 8) & 0xFF;
        hex[i * 2] = hexc[byte >> 4];
        hex[i * 2 + 1] = hexc[byte & 15];
    }
    hex[sizeof(T) * 2] = 0;
    return std::string(hex);
}

inline int CharToHex(char c)
{
    if (c >= '0' && c <= '9')
        return (c - '0');
    if (c >= 'a' && c <= 'f')
        return (c - 'a' + 10);
    if (c >= 'A' && c <= 'F')
        return (c - 'A' + 10);
    return -1;
}

inline std::vector<unsigned char> ParseHexString(const char* psz)
{
    std::vector<unsigned char> vch;
    vch.reserve(128);
    while (*psz)
    {
        int h = CharToHex(*psz++);
        int l = CharToHex(*psz++);
        if (h < 0 || l < 0)
            break;
        vch.push_back((unsigned char)((h << 4) | l));
    }
    return vch;
}

inline std::size_t ParseHexString(const char* psz, unsigned char* p, std::size_t n)
{
    unsigned char* end = p + n;
    while (*psz && p != end)
    {
        int h = CharToHex(*psz++);
        int l = CharToHex(*psz++);
        if (h < 0 || l < 0)
            break;
        *p++ = (unsigned char)((h << 4) | l);
    }
    return (n - (end - p));
}

inline std::vector<unsigned char> ParseHexString(const std::string& str)
{
    return ParseHexString(str.c_str());
}

inline std::size_t ParseHexString(const std::string& str, unsigned char* p, std::size_t n)
{
    return ParseHexString(str.c_str(), p, n);
}

#ifdef __GNUG__
#include <cxxabi.h>
inline const char* TypeName(const std::type_info& info)
{
    int status = 0;
    return abi::__cxa_demangle(info.name(), 0, 0, &status);
}
#else
inline const char* TypeName(const std::type_info& info)
{
    return info.name();
}
#endif

} // namespace xengine

#endif //XENGINE_UTIL_H
