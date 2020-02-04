// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "fork_config.h"

#include "config_macro.h"

namespace bigbang
{

CForkConfig::CForkConfig()
{
    boost::program_options::options_description desc("Fork");

    CForkConfigOption::AddOptionsImpl(desc);

    AddOptions(desc);

    fAllowAnyFork = false;
}

CForkConfig::~CForkConfig() {}

bool CForkConfig::PostLoad()
{
    for (const string& strFork : vFork)
    {
        if (strFork == "any")
        {
            fAllowAnyFork = true;
            break;
        }
    }
    return true;
}

std::string CForkConfig::ListConfig() const
{
    std::ostringstream oss;
    oss << CForkConfigOption::ListConfigImpl();
    return oss.str();
}

std::string CForkConfig::Help() const
{
    return CForkConfigOption::HelpImpl();
}

} // namespace bigbang
