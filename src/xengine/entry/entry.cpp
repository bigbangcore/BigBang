// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "entry.h"

#include <boost/bind.hpp>
#include <signal.h>
#include <stdio.h>
using namespace std;

namespace xengine
{

///////////////////////////////
// CEntry

CEntry::CEntry()
  : ioService(), ipcSignals(ioService)
{
    ipcSignals.add(SIGINT);
    ipcSignals.add(SIGTERM);
#if defined(SIGQUIT)
    ipcSignals.add(SIGQUIT);
#endif // defined(SIGQUIT)
#if defined(SIGHUP)
    ipcSignals.add(SIGHUP);
#endif // defined(SIGHUP)

    ipcSignals.async_wait(boost::bind(&CEntry::HandleSignal, this, _1, _2));
}

CEntry::~CEntry()
{
}

bool CEntry::TryLockFile(const string& strLockFile)
{
    FILE* fp = fopen(strLockFile.c_str(), "a");
    if (fp)
        fclose(fp);
    lockFile = boost::interprocess::file_lock(strLockFile.c_str());
    return lockFile.try_lock();
}

bool CEntry::Run()
{
    try
    {
        ioService.run();
    }
    catch (...)
    {
        return false;
    }
    return true;
}

void CEntry::Stop()
{
    ioService.stop();
}

void CEntry::HandleSignal(const boost::system::error_code& error, int signal_number)
{
    if (signal_number == SIGINT || signal_number == SIGTERM)
    {
        Stop();
    }
}

} // namespace xengine
