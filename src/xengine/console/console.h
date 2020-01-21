// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_CONSOLE_CONSOLE_H
#define XENGINE_CONSOLE_CONSOLE_H

#include <boost/asio.hpp>
#include <string>

#include "base/base.h"
#include "netio/ioproc.h"

#ifdef WIN32
typedef boost::asio::windows::stream_handle stream_desc;
#else
typedef boost::asio::posix::stream_descriptor stream_desc;
#endif

namespace xengine
{

class CConsole : public IBase
{
public:
    CConsole(const std::string& ownKeyIn, const std::string& strPromptIn);
    virtual ~CConsole();
    bool DispatchEvent(CEvent* pEvent) override;
    void DispatchLine(const std::string& strLine);
    void DispatchOutput(const std::string& strOutput);

protected:
    bool HandleInvoke() override;
    void HandleHalt() override;
    bool InstallReadline(const std::string& strPrompt);
    void UninstallReadline();
    static void ReadlineCallback(char* line);
    virtual void EnterLoop();
    virtual void LeaveLoop();
    virtual bool HandleLine(const std::string& strLine);

private:
    void WaitForChars();
    void HandleRead(boost::system::error_code const& err, size_t nTransferred);
    void ConsoleThreadFunc();
    void ConsoleHandleEvent(CEvent* pEvent, CIOCompletion& compltHandle);
    void ConsoleHandleLine(const std::string& strLine);
    void ConsoleHandleOutput(const std::string& strOutput);

public:
    static CConsole* pCurrentConsole;
    static boost::mutex mutexConsole;

private:
    CThread thrConsole;
    std::string strPrompt;
    std::string strLastHistory;
    boost::asio::io_service ioService;
    boost::asio::io_service::strand ioStrand;
    stream_desc inStream;
    boost::asio::null_buffers bufReadNull;
};

} // namespace xengine

#endif //XENGINE_CONSOLE_CONSOLE_H
