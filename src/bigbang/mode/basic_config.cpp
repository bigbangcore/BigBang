// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mode/basic_config.h"

#include "mode/config_macro.h"

namespace bigbang
{
namespace po = boost::program_options;

CBasicConfig::CBasicConfig()
{
    po::options_description desc("Basic");

    CBasicConfigOption::AddOptionsImpl(desc);

    AddOptions(desc);
}

CBasicConfig::~CBasicConfig() {}

bool CBasicConfig::PostLoad()
{
    if (fHelp)
    {
        return true;
    }

    if (fTestNet)
    {
        pathData /= "testnet";
    }
    nMagicNum = fTestNet ? TESTNET_MAGICNUM : MAINNET_MAGICNUM;

    return true;
}

std::string CBasicConfig::ListConfig() const
{
    std::ostringstream oss;
    oss << CBasicConfigOption::ListConfigImpl();
    oss << "magicNum: " << nMagicNum << "\n";
    return oss.str();
}

std::string CBasicConfig::Help() const
{
    return CBasicConfigOption::HelpImpl();
}

} // namespace bigbang
