// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_MODE_MODULE_TYPE_H
#define BIGBANG_MODE_MODULE_TYPE_H

namespace bigbang
{
// module type
enum class EModuleType
{
    LOCK,                // lock file
    BLOCKMAKER,          // CBlockMaker
    COREPROTOCOL,        // CCoreProtocol
    DISPATCHER,          // CDispatcher
    HTTPGET,             // CHttpGet
    HTTPSERVER,          // CHttpServer
    MINER,               // CMiner
    NETCHANNEL,          // CNetChannel
    DELEGATEDCHANNEL,    // CDelegatedChannel
    NETWORK,             // CNetwork
    RPCCLIENT,           // CRPCClient
    RPCMODE,             // CRPCMod
    SERVICE,             // CService
    TXPOOL,              // CTxPool
    TXPOOLCONTROLLER,    // CTxPoolController
    WALLET,              // CWallet
    WORLDLINE,           // CWorldLine
    WORLDLINECONTROLLER, // CWorldLineController
    CONSENSUS,           // CConsensus
    FORKMANAGER,         // CForkManager
    DATASTAT,            // CDataStat
};

} // namespace bigbang

#endif // BIGBANG_MODE_MODULE_TYPE_H
