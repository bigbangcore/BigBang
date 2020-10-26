// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "console.h"

#include <boost/bind.hpp>
#include <cstdlib>
#include <iostream>
#include <readline/history.h>
#include <readline/readline.h>

#ifdef _WIN32
#include <io.h>
#endif

using namespace std;

namespace xengine
{

///////////////////////////////
// CConsole

CConsole* CConsole::pCurrentConsole = nullptr;
boost::mutex CConsole::mutexConsole;

CConsole::CConsole(const string& ownKeyIn, const string& strPromptIn)
  : IBase(ownKeyIn),
    thrConsole(ownKeyIn, boost::bind(&CConsole::ConsoleThreadFunc, this)), strPrompt(strPromptIn),
    ioStrand(ioService),
#ifdef _WIN32
    inStream(ioService, GetStdHandle(STD_INPUT_HANDLE))
#else
    inStream(ioService, ::dup(STDIN_FILENO))
#endif
{
}

CConsole::~CConsole()
{
    UninstallReadline();
}

bool CConsole::DispatchEvent(CEvent* pEvent)
{
    bool fResult = false;
    CIOCompletion complt;
    try
    {
        ioStrand.dispatch(boost::bind(&CConsole::ConsoleHandleEvent, this, pEvent, boost::ref(complt)));
        complt.WaitForComplete(fResult);
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return fResult;
}

void CConsole::DispatchLine(const string& strLine)
{
    ioStrand.dispatch(boost::bind(&CConsole::ConsoleHandleLine, this, strLine));
}

void CConsole::DispatchOutput(const std::string& strOutput)
{
    ioStrand.dispatch(boost::bind(&CConsole::ConsoleHandleOutput, this, strOutput));
}

bool CConsole::HandleInvoke()
{
    if (!InstallReadline(strPrompt))
    {
        Error("Failed to setup readline");
        return false;
    }

    strLastHistory.clear();

    if (!ThreadDelayStart(thrConsole))
    {
        Error("Failed to start console thread");
        return false;
    }
    return true;
}

void CConsole::HandleHalt()
{
    if (!ioService.stopped())
    {
        ioService.stop();
    }
    thrConsole.Interrupt();
    ThreadExit(thrConsole);
    UninstallReadline();
}

bool CConsole::InstallReadline(const string& strPrompt)
{
    boost::unique_lock<boost::mutex> lock(CConsole::mutexConsole);
    if (pCurrentConsole != nullptr && pCurrentConsole != this)
    {
        return false;
    }

    pCurrentConsole = this;
    rl_callback_handler_install(strPrompt.c_str(), CConsole::ReadlineCallback);
    return true;
}

void CConsole::UninstallReadline()
{
    boost::unique_lock<boost::mutex> lock(CConsole::mutexConsole);
    if (pCurrentConsole == this)
    {
        pCurrentConsole = nullptr;
        rl_callback_handler_remove();
        cout << endl;
    }
}

void CConsole::EnterLoop()
{
}

void CConsole::LeaveLoop()
{
}

bool CConsole::HandleLine(const string& strLine)
{
    return true;
}

void CConsole::ReadlineCallback(char* line)
{
    boost::unique_lock<boost::mutex> lock(CConsole::mutexConsole);
    if (pCurrentConsole != nullptr)
    {
        pCurrentConsole->DispatchLine(line);
    }
}

void CConsole::WaitForChars()
{
#ifdef _WIN32
    // TODO: Just to be compilable. It works incorrect.
    inStream.async_read_some(boost::asio::mutable_buffer(), boost::bind(&CConsole::HandleRead, this,
                                                                        boost::asio::placeholders::error,
                                                                        boost::asio::placeholders::bytes_transferred));
#else
    inStream.async_read_some(bufReadNull, boost::bind(&CConsole::HandleRead, this,
                                                      boost::asio::placeholders::error,
                                                      boost::asio::placeholders::bytes_transferred));
#endif
}

void CConsole::HandleRead(boost::system::error_code const& err, size_t nTransferred)
{
    if (err == boost::system::errc::success)
    {
        rl_callback_read_char();
        WaitForChars();
    }
}

void CConsole::ConsoleThreadFunc()
{
    ioService.reset();

    EnterLoop();

    WaitForChars();
    ioService.run();

    LeaveLoop();
}

void CConsole::ConsoleHandleEvent(CEvent* pEvent, CIOCompletion& compltHandle)
{
    compltHandle.Completed(pEvent->Handle(*this));
}

void CConsole::ConsoleHandleLine(const string& strLine)
{
    if (HandleLine(strLine))
    {
        if (!strLine.empty() && strLastHistory != strLine)
        {
            add_history(strLine.c_str());
            strLastHistory = strLine;
        }
    }
}

void CConsole::ConsoleHandleOutput(const string& strOutput)
{
    boost::unique_lock<boost::mutex> lock(CConsole::mutexConsole);
    if (pCurrentConsole == this)
    {
        std::cout << '\n'
                  << strOutput << std::flush << std::endl;
        rl_on_new_line();
        rl_redisplay();
    }
}

} // namespace xengine
