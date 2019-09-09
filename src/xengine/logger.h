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

#define STD_DEBUG(Mod, Info) xengine::StdDebug(Mod, xengine::PulsFileLine(__FILE__, __LINE__, Info).c_str())

#define STD_LOG(Mod, Info) xengine::StdLog(Mod, xengine::PulsFileLine(__FILE__, __LINE__, Info).c_str())

#define STD_WARN(Mod, Info) xengine::StdWarn(Mod, xengine::PulsFileLine(__FILE__, __LINE__, Info).c_str())

#define STD_Eerror(Mod, Info) xengine::StdError(Mod, xengine::PulsFileLine(__FILE__, __LINE__, Info).c_str())

enum severity_level
{
    debug,
    info,
    warn,
    error
};

namespace src = boost::log::sources;

typedef src::severity_channel_logger_mt<severity_level, std::string> sclmt_type;
BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(logger, sclmt_type)

void StdDebug(const char* pszName, const char* pszErr);
void StdLog(const char* pszName, const char* pszErr);
void StdWarn(const char* pszName, const char* pszErr);
void StdError(const char* pszName, const char* pszErr);

bool InitLog(const boost::filesystem::path& pathData, bool debug, bool daemon);

} // namespace xengine
#endif // XENGINE_LOGGER_H