// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mode/mint_config.h"

#include "address.h"
#include "mode/config_macro.h"

namespace bigbang
{
namespace po = boost::program_options;

CMintConfig::CMintConfig()
{
    po::options_description desc("BigBangMint");

    CMintConfigOption::AddOptionsImpl(desc);

    AddOptions(desc);
}

CMintConfig::~CMintConfig() {}

bool CMintConfig::PostLoad()
{
    if (fHelp)
    {
        return true;
    }

    ExtractMintParamPair(strAddressVssMP, strKeyVssMP, destVssMP, keyVssMP);
    ExtractMintParamPair(strAddressCryptonight, strkeyCryptonight, destCryptonight,
                         keyCryptonight);
    return true;
}

std::string CMintConfig::ListConfig() const
{
    std::ostringstream oss;
    oss << CMintConfigOption::ListConfigImpl();
    oss << "destVssMP: " << destVssMP.ToString() << "\n";
    oss << "keyVssMP: " << keyVssMP.GetHex() << "\n";
    oss << "destCryptonight: " << destCryptonight.ToString() << "\n";
    oss << "keyCryptonight: " << keyCryptonight.GetHex() << "\n";
    return oss.str();
}

std::string CMintConfig::Help() const
{
    return CMintConfigOption::HelpImpl();
}

void CMintConfig::ExtractMintParamPair(const std::string& strAddress,
                                       const std::string& strKey,
                                       CDestination& dest, uint256& privkey)
{
    CAddress address;
    if (address.ParseString(strAddress) && !address.IsNull() && strKey.size() == 64)
    {
        dest = address;
        privkey.SetHex(strKey);
    }
}

} // namespace bigbang
