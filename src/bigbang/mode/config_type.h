// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_MODE_TEMPLATE_CONFIG_TYPE_H
#define BIGBANG_MODE_TEMPLATE_CONFIG_TYPE_H

#include <tuple>

namespace bigbang
{
class CBasicConfig;
class CForkConfig;
class CMintConfig;
class CNetworkConfig;
class CRPCServerConfig;
class CRPCClientConfig;
class CStorageConfig;

///
// When add new config, add type to EConfigType add class pointer to ___ConfigTypeTemplate
//

// config type
enum class EConfigType
{
    BASIC,     // CBasicConfig
    FORK,      // CForkConfig
    MINT,      // CMintConfig
    NETWORK,   // CNetworkConfig
    RPCSERVER, // CRPCServerConfig
    RPCCLIENT, // CRPCClientConfig
    STORAGE    // CStorageConfig
};

namespace config_type
{
// config type class template
static const auto ___ConfigTypeTemplate = std::make_tuple(
    (CBasicConfig*)NULL,
    (CForkConfig*)NULL,
    (CMintConfig*)NULL,
    (CNetworkConfig*)NULL,
    (CRPCServerConfig*)NULL,
    (CRPCClientConfig*)NULL,
    (CStorageConfig*)NULL);
} // namespace config_type

} // namespace bigbang

#endif // BIGBANG_MODE_TEMPLATE_CONFIG_TYPE_H
