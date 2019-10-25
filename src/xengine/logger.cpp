// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "logger.h"

#include <boost/log/attributes.hpp>
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

template <typename CharT, typename TraitsT>
inline std::basic_ostream<CharT, TraitsT>& operator<<(
    std::basic_ostream<CharT, TraitsT>& strm, severity_level lvl)
{
    static const char* const str[] = {
        "TRACE",
        "DEBUG",
        "INFO",
        "WARN",
        "ERROR"
    };
    if (static_cast<std::size_t>(lvl) < (sizeof(str) / sizeof(*str)))
        strm << str[lvl];
    else
        strm << static_cast<int>(lvl);
    return strm;
}

static void SetColor(logging::formatting_ostream& strm, const severity_level nLevel)
{
#if defined(__linux__) || defined(__APPLE__)
    switch (nLevel)
    {
    case severity_level::TRACE:
        // cyan
        strm << "\033[36m";
        break;
    case severity_level::DEBUG:
        // white
        strm << "\033[37m";
        break;
    case severity_level::INFO:
        // green
        strm << "\033[32m";
        break;
    case severity_level::WARN:
        // yellow
        strm << "\033[33m";
        break;
    case severity_level::ERROR:
        // red
        strm << "\033[31m";
        break;
    default:
        // clear
        strm << "\033[0m";
        break;
    }
#else
#endif
}

static void ResetColor(logging::formatting_ostream& strm)
{
#if defined(__linux__) || defined(__APPLE__)
    strm << "\033[0m";
#else
#endif
}

// INFO format: "%1%%2% : [%3%] <%4%> {%5%} - (%6%:%7%:%8%)%9%%10%"
//  1: Set color by severity
//  2: Date time
//  3: Channel
//  4: Severity string
//  5: Thread name
//  6: __FILE__
//  7: __LINE__
//  8: __FUNC__
//  9: Message
//  10: Reset Color
static void Formatter(logging::record_view const& rec, logging::formatting_ostream& strm, const bool fColor, const bool fDebug)
{
    logging::value_ref<severity_level> level = logging::extract<severity_level>(severity.get_name(), rec);

    //  1: Set color by severity
    if (fColor)
    {
        SetColor(strm, level.get());
    }

    //  2: Date time
    auto date_time_formatter = expr::stream << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%H:%M:%S.%f");
    date_time_formatter(rec, strm);
    strm << " : ";

    //  3: Channel
    strm << "[" << logging::extract<std::string>(channel.get_name(), rec) << "]";
    strm << " ";

    //  4: Severity string
    strm << "<" << logging::extract<severity_level>(severity.get_name(), rec) << ">";
    strm << " ";

    //  5: Thread name
    strm << "{" << logging::extract<std::string>(threadName.get_name(), rec) << "}";
    strm << " - ";

    if (fDebug || level == severity_level::TRACE)
    {
        //  6: __FILE__
        //  7: __LINE__
        auto named_scope_formatter = expr::stream << expr::format_named_scope(scope, boost::log::keywords::format = "(%F:%l:%C)");
        named_scope_formatter(rec, strm);
    }

    //  8: Message
    strm << rec[expr::smessage];

    //  9: Reset Color
    if (fColor)
    {
        ResetColor(strm);
    }
}

class CLogger : public boost::noncopyable
{
    typedef sinks::text_file_backend backend_t;
    typedef sinks::unbounded_fifo_queue queue_t;
    typedef sinks::asynchronous_sink<backend_t, queue_t> sink_t;
    typedef sinks::synchronous_sink<sinks::text_ostream_backend> text_sink;

public:
    static CLogger& getInstance()
    {
        static CLogger singleton;
        return singleton;
    }

    ~CLogger()
    {
        if (sink)
        {
            sink->locked_backend()->flush();
        }
        if (sink_console)
        {
            sink_console->locked_backend()->flush();
        }
    }

    bool Init(const boost::filesystem::path& pathData, const severity_level nLevel,
              const bool fConsole, const bool fDebug)
    {
        if (sink || sink_console)
        {
            return true;
        }

        if (!pathData.empty())
        {
            // Create log directory
            boost::filesystem::path logPath = pathData / "logs";
            if (!boost::filesystem::exists(logPath))
            {
                if (!boost::filesystem::create_directories(logPath))
                {
                    return false;
                }
            }

            // initialize file sink
            sink = boost::make_shared<sink_t>(
                keywords::open_mode = std::ios::app,
                keywords::file_name = pathData / "logs" / "bigbang_%N.log",
                keywords::rotation_size = 10 * 1024 * 1024,
                keywords::auto_flush = true);
            sink->set_formatter(boost::bind(&Formatter, _1, _2, false, fDebug));
            sink->set_filter(severity >= nLevel);
            logging::core::get()->add_sink(sink);
        }

        if (fConsole)
        {
            // initialize console sink
            sink_console = boost::make_shared<text_sink>();
            boost::shared_ptr<std::ostream> stream(&std::clog, boost::null_deleter());
            sink_console->locked_backend()->add_stream(stream);
            sink_console->set_formatter(boost::bind(&Formatter, _1, _2, true, fDebug));
            sink_console->set_filter(severity >= nLevel);
            logging::core::get()->add_sink(sink_console);
        }

        logging::add_common_attributes();
        logging::core::get()->add_global_attribute(scope.get_name(), attrs::named_scope());
        return true;
    }

private:
    boost::shared_ptr<sink_t> sink;
    boost::shared_ptr<text_sink> sink_console;
};

bool InitLog(const boost::filesystem::path& pathData, const severity_level nLevel,
             const bool fConsole, const bool fDebug)
{
    return CLogger::getInstance().Init(pathData, nLevel, fConsole, fDebug);
}

} // namespace xengine
