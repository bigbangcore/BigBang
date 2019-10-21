// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef XENGINE_LOGGER_H
#define XENGINE_LOGGER_H

#include <boost/filesystem.hpp>
#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <cstdarg>
#include <cstdio>
#include <sstream>

#include "util.h"

namespace src = boost::log::sources;
namespace attrs = boost::log::attributes;

namespace xengine
{

enum severity_level : uint8_t
{
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR
};

typedef src::severity_channel_logger_mt<severity_level, std::string> sclmt_type;
BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(logger, sclmt_type)

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level);
BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string);
BOOST_LOG_ATTRIBUTE_KEYWORD(threadName, "ThreadName", std::string);
BOOST_LOG_ATTRIBUTE_KEYWORD(scope, "Scope", attrs::named_scope::value_type);

// Log macros
#define LOG(nLevel, pszName, pszFormat, ...)                                                             \
    {                                                                                                    \
        BOOST_LOG_SCOPED_THREAD_TAG(threadName.get_name(), GetThreadName());                             \
        BOOST_LOG_NAMED_SCOPE("");                                                                       \
        BOOST_LOG_CHANNEL_SEV(logger::get(), pszName, nLevel) << FormatString(pszFormat, ##__VA_ARGS__); \
    }

/**
 * @brief Log trace message
 */
#define LOG_TRACE(pszName, pszFormat, ...) \
    LOG(severity_level::TRACE, pszName, pszFormat, ##__VA_ARGS__)

/**
 * @brief Log debug message
 */
#define LOG_DEBUG(pszName, pszFormat, ...) \
    LOG(severity_level::DEBUG, pszName, pszFormat, ##__VA_ARGS__)

/**
 * @brief Log info message
 */
#define LOG_INFO(pszName, pszFormat, ...) \
    LOG(severity_level::INFO, pszName, pszFormat, ##__VA_ARGS__);

/**
 * @brief Log warn message
 */
#define LOG_WARN(pszName, pszFormat, ...) \
    LOG(severity_level::WARN, pszName, pszFormat, ##__VA_ARGS__);

/**
 * @brief Log error message
 */
#define LOG_ERROR(pszName, pszFormat, ...) \
    LOG(severity_level::ERROR, pszName, pszFormat, ##__VA_ARGS__);

/**
 * @brief Initialize logger
 */
bool InitLog(const boost::filesystem::path& pathData, const int nLevel, const bool fDaemon, const bool fDebug);

void inline DebugLog(const char* pszName, const char* pszErr)
{
    LOG_DEBUG(pszName, pszErr);
}

void inline InfoLog(const char* pszName, const char* pszErr)
{
    LOG_INFO(pszName, pszErr);
}

void inline WarnLog(const char* pszName, const char* pszErr)
{
    LOG_WARN(pszName, pszErr);
}

void inline ErrorLog(const char* pszName, const char* pszErr)
{
    LOG_ERROR(pszName, pszErr);
}

void inline DebugLog(const std::string& pszName, const char* pszFormat, ...)
{
    va_list ap;
    va_start(ap, pszFormat);
    LOG_DEBUG(pszName, pszFormat, ap);
    va_end(ap);
}

void inline InfoLog(const std::string& pszName, const char* pszFormat, ...)
{
    va_list ap;
    va_start(ap, pszFormat);
    LOG_INFO(pszName, pszFormat, ap);
    va_end(ap);
}

void inline WarnLog(const std::string& pszName, const char* pszFormat, ...)
{
    va_list ap;
    va_start(ap, pszFormat);
    LOG_WARN(pszName, pszFormat, ap);
    va_end(ap);
}

void inline ErrorLog(const std::string& pszName, const char* pszFormat, ...)
{
    va_list ap;
    va_start(ap, pszFormat);
    LOG_ERROR(pszName, pszFormat, ap);
    va_end(ap);
}

} // namespace xengine
#endif // XENGINE_LOGGER_H