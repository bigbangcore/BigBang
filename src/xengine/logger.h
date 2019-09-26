// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef XENGINE_LOGGER_H
#define XENGINE_LOGGER_H

#include <boost/filesystem.hpp>
#include <boost/log/common.hpp>
#include <sstream>

#include "util.h"

namespace xengine
{

extern bool STD_DEBUG;

#define STD_DEBUG(Mod, Info) xengine::DebugLog(Mod, xengine::PulsFileLine(__FILE__, __LINE__, Info).c_str())

#define STD_LOG(Mod, Info) xengine::InfoLog(Mod, xengine::PulsFileLine(__FILE__, __LINE__, Info).c_str())

#define STD_WARN(Mod, Info) xengine::WarnLog(Mod, xengine::PulsFileLine(__FILE__, __LINE__, Info).c_str())

#define STD_ERROR(Mod, Info) xengine::ErrorLog(Mod, xengine::PulsFileLine(__FILE__, __LINE__, Info).c_str())

enum severity_level : uint8_t
{
    DEBUG,
    INFO,
    WARN,
    ERROR
};

namespace src = boost::log::sources;

typedef src::severity_channel_logger_mt<severity_level, std::string> sclmt_type;
BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(logger, sclmt_type)

void XLog(const char* pszName, const char* pszErr, severity_level level);

void inline DebugLog(const char* pszName, const char* pszErr)
{
    XLog(pszName, pszErr, severity_level::DEBUG);
}

void inline InfoLog(const char* pszName, const char* pszErr)
{
    XLog(pszName, pszErr, severity_level::INFO);
}

void inline WarnLog(const char* pszName, const char* pszErr)
{
    XLog(pszName, pszErr, severity_level::WARN);
}

void inline ErrorLog(const char* pszName, const char* pszErr)
{
    XLog(pszName, pszErr, severity_level::ERROR);
}

bool InitLog(const boost::filesystem::path& pathData, bool fDebug, bool fDaemon);

} // namespace xengine
#endif // XENGINE_LOGGER_H