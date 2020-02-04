// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_ENTRY_ENTRY_H
#define XENGINE_ENTRY_ENTRY_H

#include <boost/asio.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <string>

namespace xengine
{

class CEntry
{
public:
    CEntry();
    ~CEntry();
    bool TryLockFile(const std::string& strLockFile);
    virtual bool Run();
    virtual void Stop();

protected:
    void HandleSignal(const boost::system::error_code& error, int signal_number);

protected:
    boost::asio::io_service ioService;
    boost::asio::signal_set ipcSignals;
    boost::interprocess::file_lock lockFile;
};

} // namespace xengine

#endif //XENGINE_ENTRY_ENTRY_H
