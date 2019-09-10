// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config.h"

#include <algorithm>
#include <boost/filesystem.hpp>
#include <exception>
#include <iostream>
#include <numeric>

#include "rpc/rpc_error.h"
#include "util.h"

using namespace xengine;

namespace po = boost::program_options;
namespace fs = boost::filesystem;

namespace bigbang
{

CConfig::CConfig()
  : emMode(EModeType::ERROR), pImpl(nullptr) {}

CConfig::~CConfig()
{
    if (pImpl)
    {
        delete pImpl;
    }
}

bool CConfig::Load(int argc, char* argv[], const fs::path& pathDefault,
                   const std::string& strConfile)
{
    if (argc <= 0)
    {
        return false;
    }

    // call parse cmd
    std::vector<std::string> vecCmd = xengine::CConfig::ParseCmd(argc, argv);

    // determine mode type
    std::string exec = fs::path(argv[0]).filename().string();
    std::string cmd = (vecCmd.size() > 0) ? vecCmd[0] : "";

    int ignoreCmd = 0;
    if (exec == "bigbang-server")
    {
        emMode = EModeType::SERVER;
    }
    else if (exec == "bigbang-miner" || cmd == "miner")
    {
        emMode = EModeType::MINER;
    }
    else if (exec == "bigbang-cli")
    {
        emMode = EModeType::CONSOLE;

        if (vecCmd.size() > 0)
        {
            subCmd = vecCmd[0];
        }
    }
    else
    {
        if (cmd == "server" || cmd == "")
        {
            emMode = EModeType::SERVER;

            if (cmd == "server")
            {
                ignoreCmd = 1;
            }
        }
        else if (cmd == "miner")
        {
            emMode = EModeType::MINER;
            ignoreCmd = 1;
        }
        else
        {
            emMode = EModeType::CONSOLE;
            if (cmd == "console")
            {
                ignoreCmd = 1;
            }

            if (vecCmd.size() > ignoreCmd)
            {
                subCmd = vecCmd[ignoreCmd];
            }
        }
    }

    if (emMode == EModeType::ERROR)
    {
        return false;
    }

    pImpl = CMode::CreateConfig(emMode, subCmd);
    pImpl->SetIgnoreCmd(ignoreCmd);

    // Load
    return pImpl->Load(argc, argv, pathDefault, strConfile);
}

bool CConfig::PostLoad()
{
    if (pImpl)
    {
        try
        {
            return pImpl->PostLoad();
        }
        catch (rpc::CRPCException& e)
        {
            ErrorLog(__PRETTY_FUNCTION__, (e.strMessage + rpc::strHelpTips).c_str());
            return false;
        }
        catch (std::exception& e)
        {
            ErrorLog(__PRETTY_FUNCTION__, e.what());
            return false;
        }
    }
    return false;
}

void CConfig::ListConfig() const
{
    if (pImpl)
    {
        std::cout << pImpl->ListConfig() << std::endl;
    }
}

std::string CConfig::Help() const
{
    if (pImpl)
    {
        return CMode::Help(emMode, subCmd, pImpl->Help());
    }
    else
    {
        return CMode::Help(emMode, subCmd);
    }
}

} // namespace bigbang
