// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mode/network_config.h"

#include "mode/config_macro.h"

namespace bigbang
{
namespace po = boost::program_options;

CNetworkConfig::CNetworkConfig()
{
    po::options_description desc("BigBangNetwork");

    CNetworkConfigOption::AddOptionsImpl(desc);

    AddOptions(desc);
}
CNetworkConfig::~CNetworkConfig() {}

bool CNetworkConfig::PostLoad()
{
    if (fHelp)
    {
        return true;
    }

    if (nPortInt <= 0 || nPortInt > 0xFFFF)
    {
        nPort = (fTestNet ? DEFAULT_TESTNET_P2PPORT : DEFAULT_P2PPORT);
    }
    else
    {
        nPort = (unsigned short)nPortInt;
    }

    nMaxOutBounds = DEFAULT_MAX_OUTBOUNDS;
    if (nMaxConnection <= DEFAULT_MAX_OUTBOUNDS)
    {
        nMaxInBounds = (fListen || fListen4 || fListen6) ? 1 : 0;
    }
    else
    {
        nMaxInBounds = nMaxConnection - DEFAULT_MAX_OUTBOUNDS;
    }

    if (nConnectTimeout == 0)
    {
        nConnectTimeout = 1;
    }

    return true;
}

std::string CNetworkConfig::ListConfig() const
{
    std::ostringstream oss;
    oss << CNetworkConfigOption::ListConfigImpl();
    oss << "port: " << nPort << "\n";
    oss << "maxOutBounds: " << nMaxOutBounds << "\n";
    oss << "maxInBounds: " << nMaxInBounds << "\n";
    oss << "dnseed: ";
    for (auto& s : vDNSeed)
    {
        oss << s << " ";
    }
    oss << "\n";
    return oss.str();
}

std::string CNetworkConfig::Help() const
{
    return CNetworkConfigOption::HelpImpl();
}

} // namespace bigbang
