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

#include "util.h"

namespace xengine
{

class CLog
{
public:
    CLog()
      : pFile(NULL), fNewLine(false)
    {
    }

    ~CLog()
    {
        if (pFile != NULL)
        {
            fclose(pFile);
        }
    }

    bool SetLogFilePath(const std::string& strPathLog)
    {
        if (pFile != NULL)
        {
            fclose(pFile);
        }
        pFile = fopen(strPathLog.c_str(), "a");
        fNewLine = true;
        return (pFile != NULL);
    }

    virtual void operator()(const char* key, const char* strPrefix, const char* pszFormat, va_list ap)
    {
        if (pFile != NULL)
        {
            boost::mutex::scoped_lock scoped_lock(mutex);
            if (fNewLine && pszFormat[0] != '\n')
            {
                fprintf(pFile, "%s %s <%s> {%s} ", GetLocalTime().c_str(), strPrefix, key, GetThreadName().c_str());
            }

            fNewLine = (pszFormat[strlen(pszFormat) - 1] == '\n');

            vfprintf(pFile, pszFormat, ap);
            fflush(pFile);
        }
    }

protected:
    FILE* pFile;
    boost::mutex mutex;
    bool fNewLine;
};

} // namespace xengine

#endif //XENGINE_DOCKER_LOG_H
