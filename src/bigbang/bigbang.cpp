// Copyright (c) 2019-2020 The Bigbang developers
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
