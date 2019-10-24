// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.h"

#include <cstdarg>
#include <map>
#if defined(__linux__)
#include <sys/prctl.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif

#ifndef __CYGWIN__
#include <execinfo.h>
#endif

namespace xengine
{

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
    char name[32] = { 0 };
#if defined(__linux__)
    ::prctl(PR_GET_NAME, name);
#elif defined(__APPLE__)
    pthread_getname_np(pthread_self(), name, 32);
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

std::string FormatString(const char* pszFormat, ...)
{
    va_list ap;

    std::string str;
    size_t size = (strlen(pszFormat) / 128 + 2) * 128;
    int len = -1;
    do
    {
        str.resize(size);
        va_start(ap, pszFormat);
        len = vsnprintf(const_cast<char*>(str.data()), size, pszFormat, ap);
        va_end(ap);
    } while ((len < 0 || len >= size) && (size *= 2));

    return str.substr(0, len + 1);
}

} // namespace xengine
