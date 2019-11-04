// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mode/rpc_config.h"

#include <boost/algorithm/algorithm.hpp>

#include "mode/config_macro.h"

namespace bigbang
{
namespace po = boost::program_options;
namespace fs = boost::filesystem;
using tcp = boost::asio::ip::tcp;

/////////////////////////////////////////////////////////////////////
// CRPCBasicConfig

CRPCBasicConfig::CRPCBasicConfig()
{
    po::options_description desc("RPCBasic");

    CRPCBasicConfigOption::AddOptionsImpl(desc);

    AddOptions(desc);
}

CRPCBasicConfig::~CRPCBasicConfig() {}

bool CRPCBasicConfig::PostLoad()
{
    if (fHelp)
    {
        return true;
    }

    if (nRPCPortInt <= 0 || nRPCPortInt > 0xFFFF)
    {
        nRPCPort = (fTestNet ? DEFAULT_TESTNET_RPCPORT : DEFAULT_RPCPORT);
    }
    else
    {
        nRPCPort = (unsigned short)nRPCPortInt;
    }

    if (!strRPCCAFile.empty())
    {
        if (!fs::path(strRPCCAFile).is_complete())
        {
            strRPCCAFile = (pathRoot / strRPCCAFile).string();
        }
    }

    if (!strRPCCertFile.empty())
    {
        if (!fs::path(strRPCCertFile).is_complete())
        {
            strRPCCertFile = (pathRoot / strRPCCertFile).string();
        }
    }

    if (!strRPCPKFile.empty())
    {
        if (!fs::path(strRPCPKFile).is_complete())
        {
            strRPCPKFile = (pathRoot / strRPCPKFile).string();
        }
    }

    return true;
}

std::string CRPCBasicConfig::ListConfig() const
{
    std::ostringstream oss;
    oss << CRPCBasicConfigOption::ListConfigImpl();
    oss << "RPCPort: " << nRPCPort << "\n";
    return oss.str();
}

std::string CRPCBasicConfig::Help() const
{
    return CRPCBasicConfigOption::HelpImpl();
}

/////////////////////////////////////////////////////////////////////
// CRPCServerConfig

CRPCServerConfig::CRPCServerConfig()
{
    po::options_description desc("RPCServer");

    CRPCServerConfigOption::AddOptionsImpl(desc);

    AddOptions(desc);
}

CRPCServerConfig::~CRPCServerConfig() {}

bool CRPCServerConfig::PostLoad()
{
    if (fHelp)
    {
        return true;
    }

    CRPCBasicConfig::PostLoad();

    if (fRPCListen || fRPCListen4 || (!fRPCListen4 && !fRPCListen6))
    {
        epRPC = tcp::endpoint(!vRPCAllowIP.empty()
                                  ? boost::asio::ip::address_v4::any()
                                  : boost::asio::ip::address_v4::loopback(),
                              nRPCPort);
    }

    if (fRPCListen || fRPCListen6)
    {
        epRPC = tcp::endpoint(!vRPCAllowIP.empty()
                                  ? boost::asio::ip::address_v6::any()
                                  : boost::asio::ip::address_v6::loopback(),
                              nRPCPort);
    }

    if (nRPCThread < RPC_THREAD_LOWER_LIMIT)
    {
        nRPCThread = RPC_THREAD_LOWER_LIMIT;
    }
    else if (nRPCThread > RPC_THREAD_UPPER_LIMIT)
    {
        nRPCThread = RPC_THREAD_UPPER_LIMIT;
    }

    return true;
}

std::string CRPCServerConfig::ListConfig() const
{
    std::ostringstream oss;
    oss << CRPCServerConfigOption::ListConfigImpl();
    oss << "epRPC: " << epRPC << "\n";
    return CRPCBasicConfig::ListConfig() + oss.str();
}

std::string CRPCServerConfig::Help() const
{
    return CRPCBasicConfig::Help() + CRPCServerConfigOption::HelpImpl();
}

/////////////////////////////////////////////////////////////////////
// CRPCClientConfig

CRPCClientConfig::CRPCClientConfig()
{
    po::options_description desc("RPCClient");

    CRPCClientConfigOption::AddOptionsImpl(desc);

    AddOptions(desc);
}

CRPCClientConfig::~CRPCClientConfig() {}

bool CRPCClientConfig::PostLoad()
{
    if (fHelp)
    {
        return true;
    }

    CRPCBasicConfig::PostLoad();

    try
    {
        boost::asio::ip::address addr(boost::asio::ip::address::from_string(strRPCConnect));
        if (fRPCListen4 || (!fRPCListen4 && !fRPCListen6))
        {
            if (addr.is_loopback())
            {
                strRPCConnect = boost::asio::ip::address_v4::loopback().to_string();
            }
        }

        if (fRPCListen6)
        {
            if (addr.is_loopback())
            {
                strRPCConnect = boost::asio::ip::address_v6::loopback().to_string();
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return false;
    }

    if (nRPCConnectTimeout == 0)
    {
        nRPCConnectTimeout = 1;
    }

    return true;
}

std::string CRPCClientConfig::ListConfig() const
{
    std::ostringstream oss;
    oss << CRPCClientConfigOption::ListConfigImpl();
    return CRPCBasicConfig::ListConfig() + oss.str();
}

std::string CRPCClientConfig::Help() const
{
    return CRPCBasicConfig::Help() + CRPCClientConfigOption::HelpImpl();
}

} // namespace bigbang
