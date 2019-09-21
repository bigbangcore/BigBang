// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <exception>
#include <iostream>

#include "entry.h"
#include "util.h"

using namespace bigbang;

void Shutdown()
{
    CBbEntry::GetInstance().Stop();
}

int is_begin_with(const char* str1, char* str2)
{
    if (str1 == NULL || str2 == NULL)
        return -1;
    int len1 = strlen(str1);
    int len2 = strlen(str2);
    if ((len1 < len2) || (len1 == 0 || len2 == 0))
        return -1;
    char* p = str2;
    int i = 0;
    while (*p != '\0')
    {
        if (*p != str1[i])
            return 0;
        p++;
        i++;
    }
    return 1;
}

int main(int argc, char** argv)
{
    CBbEntry& entry = CBbEntry::GetInstance();
    try
    {
        if (entry.Initialize(argc, argv))
        {
            entry.Run();
        }
    }
    catch (std::exception& e)
    {
        xengine::StdError(__PRETTY_FUNCTION__, e.what());
    }
    catch (...)
    {
        xengine::StdError(__PRETTY_FUNCTION__, "unknown");
    }

    entry.Exit();

    return 0;
}
