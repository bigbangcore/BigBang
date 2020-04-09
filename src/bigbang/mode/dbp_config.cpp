// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mode/dbp_config.h"

#include <boost/algorithm/algorithm.hpp>
#include "mode/config_macro.h"

namespace bigbang
{
namespace po = boost::program_options;
namespace fs = boost::filesystem;
using tcp = boost::asio::ip::tcp;

/////////////////////////////////////////////////////////////////////
// CBbDbpBasicConfig

CBbDbpBasicConfig::CBbDbpBasicConfig()
{
    po::options_description desc("DbpBasic");

    CBbDbpBasicConfigOption::AddOptionsImpl(desc);

    AddOptions(desc);
}

CBbDbpBasicConfig::~CBbDbpBasicConfig() {}

bool CBbDbpBasicConfig::PostLoad()
{
    if (fHelp)
    {
        return true;
    }

    if (nDbpPortInt <= 0 || nDbpPortInt > 0xFFFF)
    {
        nDbpPort = (fTestNet ? DEFAULT_TESTNET_DBPPORT : DEFAULT_DBPPORT);
    }
    else
    {
        nDbpPort = (unsigned short)nDbpPortInt;
    }

    if (!fs::path(strDbpCAFile).is_complete())
    {
        strDbpCAFile = (pathRoot / strDbpCAFile).string();
    }

    if (!fs::path(strDbpCertFile).is_complete())
    {
        strDbpCertFile = (pathRoot / strDbpCertFile).string();
    }

    if (!fs::path(strDbpPKFile).is_complete())
    {
        strDbpPKFile = (pathRoot / strDbpPKFile).string();
    }

    return true;
}

std::string CBbDbpBasicConfig::ListConfig() const
{
    std::ostringstream oss;
    oss << CBbDbpBasicConfigOption::ListConfigImpl();
    oss << "DbpPort: " << nDbpPort << "\n";
    return oss.str();
}

std::string CBbDbpBasicConfig::Help() const
{
    return CBbDbpBasicConfigOption::HelpImpl();
}

/////////////////////////////////////////////////////////////////////
// CBbRPCServerConfig

CBbDbpServerConfig::CBbDbpServerConfig()
{
    po::options_description desc("DbpServer");

    CBbDbpServerConfigOption::AddOptionsImpl(desc);

    AddOptions(desc);
}

CBbDbpServerConfig::~CBbDbpServerConfig() {}

bool CBbDbpServerConfig::PostLoad()
{
    if (fHelp)
    {
        return true;
    }

    CBbDbpBasicConfig::PostLoad();

    if(fDbpListen4 || (!fDbpListen4 && !fDbpListen6))
    {
        epDbp = tcp::endpoint(!vDbpAllowIP.empty()
                              ? boost::asio::ip::address_v4::any()
                              : boost::asio::ip::address_v4::loopback(),
                          nDbpPort);
    }

    if(fDbpListen6)
    {
        epDbp = tcp::endpoint(!vDbpAllowIP.empty()
                              ? boost::asio::ip::address_v6::any()
                              : boost::asio::ip::address_v6::loopback(),
                          nDbpPort);
    }

    
    return true;
}

std::string CBbDbpServerConfig::ListConfig() const
{
    std::ostringstream oss;
    oss << CBbDbpServerConfigOption::ListConfigImpl();
    oss << "epDbp: " << epDbp << "\n";
    return CBbDbpBasicConfig::ListConfig() + oss.str();
}

std::string CBbDbpServerConfig::Help() const
{
    return CBbDbpBasicConfig::Help() + CBbDbpServerConfigOption::HelpImpl();
}

CBbDbpClientConfig::CBbDbpClientConfig()
{
    po::options_description desc("DbpClient");

    CBbDbpClientConfigOption::AddOptionsImpl(desc);

    AddOptions(desc);
}

CBbDbpClientConfig::~CBbDbpClientConfig()
{

}
    
bool CBbDbpClientConfig::PostLoad()
{
    if (fHelp)
    {
        return true;
    }

    CBbDbpBasicConfig::PostLoad();

    epParentHost = 
    tcp::endpoint(!strParentHost.empty()
                    ? boost::asio::ip::address::from_string(strParentHost)
                    : boost::asio::ip::address_v4::loopback(),
                    nDbpPort);
    
    return true;
}

std::string CBbDbpClientConfig::ListConfig() const
{
    std::ostringstream oss;
    oss << CBbDbpClientConfigOption::ListConfigImpl();
    oss << "epParentDbp: " << epParentHost << "\n";
    return oss.str();
}

std::string CBbDbpClientConfig::Help() const
{
    return CBbDbpClientConfigOption::HelpImpl();
}

} // namepspace bigbang
