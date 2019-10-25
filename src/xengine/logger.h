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
    ERROR,
    IGNORE,
};

typedef src::severity_channel_logger_mt<severity_level, std::string> sclmt_type;
BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(logger, sclmt_type)

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level);
BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string);
BOOST_LOG_ATTRIBUTE_KEYWORD(threadName, "ThreadName", std::string);
BOOST_LOG_ATTRIBUTE_KEYWORD(scope, "Scope", attrs::named_scope::value_type);

// INFO macros
#define LOG(nLevel, pszName, pszFormat, ...)                                                                               \
    {                                                                                                                      \
        BOOST_LOG_SCOPED_THREAD_TAG(xengine::threadName.get_name(), xengine::GetThreadName());                             \
        BOOST_LOG_FUNCTION();                                                                                         \
        BOOST_LOG_CHANNEL_SEV(xengine::logger::get(), pszName, nLevel) << xengine::FormatString(pszFormat, ##__VA_ARGS__); \
    }

/**
 * @brief INFO trace message
 */
#define LOG_TRACE(pszName, pszFormat, ...) \
    LOG(xengine::severity_level::TRACE, pszName, pszFormat, ##__VA_ARGS__)

/**
 * @brief INFO debug message
 */
#define LOG_DEBUG(pszName, pszFormat, ...) \
    LOG(xengine::severity_level::DEBUG, pszName, pszFormat, ##__VA_ARGS__)

/**
 * @brief INFO info message
 */
#define LOG_INFO(pszName, pszFormat, ...) \
    LOG(xengine::severity_level::INFO, pszName, pszFormat, ##__VA_ARGS__);

/**
 * @brief INFO warn message
 */
#define LOG_WARN(pszName, pszFormat, ...) \
    LOG(xengine::severity_level::WARN, pszName, pszFormat, ##__VA_ARGS__);

/**
 * @brief INFO error message
 */
#define LOG_ERROR(pszName, pszFormat, ...) \
    LOG(xengine::severity_level::ERROR, pszName, pszFormat, ##__VA_ARGS__);

/**
 * @brief Save a channel name for logger
 */
class CLogChannel final
{
public:
    CLogChannel(const std::string& strChannelIn)
      : pstrChannelRef(&strChannelIn), fNew(false) {}

    CLogChannel(const char* pszChannelIn)
      : pstrChannelRef(new std::string(pszChannelIn)), fNew(true) {}

    CLogChannel(const CLogChannel& channel)
      : pstrChannelRef(channel.fNew ? new std::string(*channel.pstrChannelRef) : channel.pstrChannelRef), fNew(channel.fNew) {}

    ~CLogChannel()
    {
        if (fNew)
        {
            delete pstrChannelRef;
        }
    }

    const char* Channel() const
    {
        return pstrChannelRef->c_str();
    }

private:
    const std::string* pstrChannelRef;
    const bool fNew;
};

/**
 * @brief Register channel name in a class declaration.
 * @param chan A std::string or const char* variable
 */
#define LOGGER_CHANNEL(chan) \
protected:                   \
    const xengine::CLogChannel __log_channel = chan

/// @brief INFO trace message in a class which has called LOGGER_CHANNEL()
#define TRACE(pszFormat, ...) LOG_TRACE(__log_channel.Channel(), pszFormat, ##__VA_ARGS__)

/// @brief INFO debug message in a class which has called LOGGER_CHANNEL()
#define DEBUG(pszFormat, ...) LOG_DEBUG(__log_channel.Channel(), pszFormat, ##__VA_ARGS__)

/// @brief INFO info message in a class which has called LOGGER_CHANNEL()
#define INFO(pszFormat, ...) LOG_INFO(__log_channel.Channel(), pszFormat, ##__VA_ARGS__)

/// @brief INFO warn message in a class which has called LOGGER_CHANNEL()
#define WARN(pszFormat, ...) LOG_WARN(__log_channel.Channel(), pszFormat, ##__VA_ARGS__)

/// @brief INFO error message in a class which has called LOGGER_CHANNEL()
#define ERROR(pszFormat, ...) LOG_ERROR(__log_channel.Channel(), pszFormat, ##__VA_ARGS__)

/**
 * @brief Initialize logger
 * @param pathData Root directory of log. INFO file wiil be saved in "pathData/logs/".
 * @param nLevel Logger level
 * @param fConsole Output console log or not
 * @param fDebug Output source file name and line or not
 */
bool InitLog(const boost::filesystem::path& pathData, const severity_level nLevel,
             const bool fConsole, const bool fDebug);

} // namespace xengine
#endif // XENGINE_LOGGER_H