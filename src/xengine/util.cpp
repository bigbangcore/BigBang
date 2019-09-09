// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.h"

#include <map>
#if defined(__linux__)
#include <sys/prctl.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif

#ifndef __CYGWIN__
#include <execinfo.h>
#endif

#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/noncopyable.hpp>

namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;

namespace xengine
{
bool STD_DEBUG = false;
void SetThreadName(const char* name)
{
#if defined(__linux__)
    ::prctl(PR_SET_NAME, name);
#elif defined(__APPLE__)
    pthread_setname_np(name);
#endif
}

std::string GetThreadName()
{
    char name[16] = { 0 };
#if defined(__linux__)
    ::prctl(PR_GET_NAME, name);
#elif defined(__APPLE__)
    pthread_getname_np(pthread_self(), name, 16);
#endif
    return name;
}

#define DUMP_STACK_DEPTH_MAX 64

void PrintTrace()
{
#ifndef __CYGWIN__
    void* stack_trace[DUMP_STACK_DEPTH_MAX] = { 0 };
    char** stack_strings = nullptr;
    int stack_depth = 0;
    int i = 0;

    stack_depth = backtrace(stack_trace, DUMP_STACK_DEPTH_MAX);

    stack_strings = (char**)backtrace_symbols(stack_trace, stack_depth);
    if (nullptr == stack_strings)
    {
        printf(" Memory is not enough while dump Stack Trace! \r\n");
        return;
    }

    printf(" Stack Trace: \r\n");
    for (i = 0; i < stack_depth; ++i)
    {
        printf(" [%d] %s \r\n", i, stack_strings[i]);
    }

    free(stack_strings);
    stack_strings = nullptr;
#endif
}

template <typename CharT, typename TraitsT>
inline std::basic_ostream<CharT, TraitsT>& operator<<(
    std::basic_ostream<CharT, TraitsT>& strm, severity_level lvl)
{
    static const char* const str[] = {
        "debug",
        "info",
        "warn",
        "error"
    };
    if (static_cast<std::size_t>(lvl) < (sizeof(str) / sizeof(*str)))
        strm << str[lvl];
    else
        strm << static_cast<int>(lvl);
    return strm;
}

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level)
BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string)

static void console_formatter(logging::record_view const& rec, logging::formatting_ostream& strm)
{
    logging::value_ref<severity_level> level = logging::extract<severity_level>("Severity", rec);

    auto date_time_formatter = expr::stream << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%H:%M:%S.%f");
    date_time_formatter(rec, strm);
    strm << "|" << logging::extract<std::string>("Channel", rec);
    switch (level.get())
    {
    case debug:
        strm << "\033[0m";
        break;
    case info:
        strm << "\033[32m";
        break;
    case warn:
        strm << "\033[33m";
        break;
    case error:
        strm << "\033[31m";
        break;
    default:
        break;
    }
    strm << ":" << logging::extract<severity_level>("Severity", rec);
    strm << "|" << logging::extract<std::string>("ThreadName", rec);
    strm << "|" << rec[expr::smessage];
    strm << "\033[0m";
}

typedef sinks::text_file_backend backend_t;
typedef sinks::unbounded_fifo_queue queue_t;
typedef sinks::asynchronous_sink<backend_t, queue_t> sink_t;

class CBoostLog : public boost::noncopyable
{
public:
    static CBoostLog& getInstance()
    {
        static CBoostLog* singleton = new CBoostLog();
        return *singleton;
    }

    bool IsInited() const
    {
        return fIsInited.load();
    }

    void Init(const boost::filesystem::path& pathData, bool debug_, bool daemon)
    {
        sink = boost::make_shared<sink_t>(
            keywords::open_mode = std::ios::app,
            keywords::file_name = pathData / "logs" / "bigbang_%N.log",
            keywords::rotation_size = 10 * 1024 * 1024,
            keywords::auto_flush = true);

        sink->set_formatter(
            expr::format("%1% : [%2%] <%3%> {%4%} - %5%")
            % expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%H:%M:%S.%f")
            % channel
            % severity
            % expr::attr<std::string>("ThreadName")
            % expr::smessage);

        typedef expr::channel_severity_filter_actor<std::string, severity_level> min_severity_filter;
        min_severity_filter min_severity = expr::channel_severity_filter(channel, severity);
        severity_level sl = debug_ ? debug : info;
        min_severity["bigbang"] = warn;
        min_severity["CDelegate"] = warn;
        min_severity["storage"] = warn;
        auto filter = min_severity || sl <= severity;
        sink->set_filter(filter);

        logging::core::get()->add_sink(sink);

        if (!daemon)
        {
            typedef sinks::synchronous_sink<sinks::text_ostream_backend> text_sink;
            boost::shared_ptr<text_sink> sink_console = boost::make_shared<text_sink>();
            boost::shared_ptr<std::ostream> stream(&std::clog, boost::null_deleter());
            sink_console->locked_backend()->add_stream(stream);
            sink_console->set_formatter(&console_formatter);
            sink_console->set_filter(filter);
            logging::core::get()->add_sink(sink_console);
        }

        logging::add_common_attributes();
        fIsInited = true;
    }

    ~CBoostLog()
    {
        if (sink)
        {
            sink->locked_backend()->flush();
        }
    }

private:
    boost::shared_ptr<sink_t> sink;
    std::atomic<bool> fIsInited = { false };
};

void StdDebug(const char* pszName, const char* pszErr)
{
    if (CBoostLog::getInstance().IsInited())
    {
        std::string str(pszErr);
        if (str[str.length() - 1] == '\n')
        {
            str.resize(str.length() - 1);
        }
        BOOST_LOG_SCOPED_THREAD_TAG("ThreadName", GetThreadName().c_str());
        BOOST_LOG_CHANNEL_SEV(logger::get(), pszName, debug) << pszErr;
    }
}

void StdLog(const char* pszName, const char* pszErr)
{
    if (CBoostLog::getInstance().IsInited())
    {
        std::string str(pszErr);
        if (str[str.length() - 1] == '\n')
        {
            str.resize(str.length() - 1);
        }
        BOOST_LOG_SCOPED_THREAD_TAG("ThreadName", GetThreadName().c_str());
        BOOST_LOG_CHANNEL_SEV(logger::get(), pszName, info) << str;
    }
}

void StdWarn(const char* pszName, const char* pszErr)
{
    if (CBoostLog::getInstance().IsInited())
    {
        std::string str(pszErr);
        if (str[str.length() - 1] == '\n')
        {
            str.resize(str.length() - 1);
        }
        BOOST_LOG_SCOPED_THREAD_TAG("ThreadName", GetThreadName().c_str());
        BOOST_LOG_CHANNEL_SEV(logger::get(), pszName, warn) << pszErr;
    }
}

void StdError(const char* pszName, const char* pszErr)
{
    if (CBoostLog::getInstance().IsInited())
    {
        std::string str(pszErr);
        if (str[str.length() - 1] == '\n')
        {
            str.resize(str.length() - 1);
        }
        BOOST_LOG_SCOPED_THREAD_TAG("ThreadName", GetThreadName().c_str());
        BOOST_LOG_CHANNEL_SEV(logger::get(), pszName, error) << pszErr;
    }
}

bool InitLog(const boost::filesystem::path& pathData, bool debug, bool daemon)
{
    boost::filesystem::path logPath = pathData / "logs";
    if (!boost::filesystem::exists(logPath))
    {
        if (!boost::filesystem::create_directories(logPath))
        {
            return false;
        }
    }

    CBoostLog::getInstance().Init(pathData, debug, daemon);
    return true;
}

} // namespace xengine
