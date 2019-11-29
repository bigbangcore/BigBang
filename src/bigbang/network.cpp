// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "network.h"

#include <boost/any.hpp>
#include <boost/bind.hpp>

#include "version.h"

using namespace std;
using namespace xengine;
using boost::asio::ip::tcp;

namespace bigbang
{

//////////////////////////////
// CNetwork

CNetwork::CNetwork()
  : pCoreProtocol(nullptr)
{
}

CNetwork::~CNetwork()
{
}

bool CNetwork::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol");
        return false;
    }

    Configure(NetworkConfig()->nMagicNum, PROTO_VERSION, network::NODE_NETWORK /*| network::NODE_DELEGATED*/,
              FormatSubVersion(), !NetworkConfig()->vConnectTo.empty(), pCoreProtocol->GetGenesisBlockHash());

    CPeerNetConfig config;
    if (NetworkConfig()->fListen || NetworkConfig()->fListen4)
    {
        config.vecService.push_back(CPeerService(tcp::endpoint(tcp::v4(), NetworkConfig()->nPort),
                                                 NetworkConfig()->nMaxInBounds));
    }
    if (NetworkConfig()->fListen || NetworkConfig()->fListen6)
    {
        config.vecService.push_back(CPeerService(tcp::endpoint(tcp::v6(), NetworkConfig()->nPort),
                                                 NetworkConfig()->nMaxInBounds));
    }
    config.nMaxOutBounds = NetworkConfig()->nMaxOutBounds;
    config.nPortDefault = (NetworkConfig()->fTestNet ? DEFAULT_TESTNET_P2PPORT : DEFAULT_P2PPORT);
    for (const string& conn : NetworkConfig()->vConnectTo)
    {
        config.vecNode.push_back(CNetHost(conn, config.nPortDefault, conn,
                                          boost::any(uint64(network::NODE_NETWORK))));
    }
    if (config.vecNode.empty())
    {
        for (const string& seed : NetworkConfig()->vDNSeed)
        {
            config.vecNode.push_back(CNetHost(seed, DEFAULT_DNSEED_PORT, "dnseed",
                                              boost::any(uint64(network::NODE_NETWORK))));
        }
        for (const string& node : NetworkConfig()->vNode)
        {
            config.vecNode.push_back(CNetHost(node, config.nPortDefault, node,
                                              boost::any(uint64(network::NODE_NETWORK))));
        }
    }

    if ((NetworkConfig()->fListen || NetworkConfig()->fListen4 || NetworkConfig()->fListen6) && !NetworkConfig()->strGateWay.empty())
    {
        config.gateWayAddr.Set(NetworkConfig()->strGateWay, config.nPortDefault, NetworkConfig()->strGateWay,
                               boost::any(uint64(network::NODE_NETWORK)));
    }

    ConfigNetwork(config);

    return network::CBbPeerNet::HandleInitialize();
}

void CNetwork::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    network::CBbPeerNet::HandleDeinitialize();
}

bool CNetwork::CheckPeerVersion(uint32 nVersionIn, uint64 nServiceIn, const string& subVersionIn)
{
    (void)subVersionIn;
    if (nVersionIn < MIN_PROTO_VERSION || (nServiceIn & network::NODE_NETWORK) == 0)
    {
        return false;
    }
    return true;
}

} // namespace bigbang
