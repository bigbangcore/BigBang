// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_DOCKER_LOG_H
#define XENGINE_DOCKER_LOG_H

#include <boost/date_time.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <locale>
#include <sstream>
#include <stdarg.h>
#include <stdio.h>
#include <string>

#include "logger.h"

namespace xengine
{

class CLog
{
public:
    CLog()
    {
    }

    ~CLog()
    {
    }

    bool SetLogFilePath(const std::string& strPathLog)
    {
        boost::filesystem::path p = strPathLog;
        modNmae = p.filename().replace_extension().string();
        return true;
    }

    virtual void operator()(const char* key, const char* strPrefix, const char* pszFormat, va_list ap)
    {
        std::string str("<");
        str.append(key);
        str.append(">");
        char arg_buffer[256] = { 0 };
        vsnprintf(arg_buffer, sizeof(arg_buffer), pszFormat, ap);
        str.append(arg_buffer);

        if (strcmp(strPrefix, "[INFO]") == 0)
        {
            InfoLog(modNmae.c_str(), str.c_str());
        }
        else if (strcmp(strPrefix, "[DEBUG]") == 0)
        {
            DebugLog(modNmae.c_str(), str.c_str());
        }
        else if (strcmp(strPrefix, "[WARN]") == 0)
        {
            WarnLog(modNmae.c_str(), str.c_str());
        }
        else if (strcmp(strPrefix, "[ERROR]") == 0)
        {
            ErrorLog(modNmae.c_str(), str.c_str());
        }
        else
        {
            InfoLog(modNmae.c_str(), str.c_str());
        }
    }

protected:
    std::string modNmae;
};

} // namespace xengine

#endif //XENGINE_DOCKER_LOG_H
