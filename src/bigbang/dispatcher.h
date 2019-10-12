// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_DISPATCHER_H
#define BIGBANG_DISPATCHER_H

#include "base.h"
#include "peernet.h"

namespace bigbang
{

class CDispatcher : public IDispatcher
{
public:
    CDispatcher();
    ~CDispatcher();
    Errno AddNewBlock(const CBlock& block, uint64 nNonce = 0) override;
    Errno AddNewTx(const CTransaction& tx, uint64 nNonce = 0) override;
    bool AddNewDistribute(const uint256& hashAnchor, const CDestination& dest,
                          const std::vector<unsigned char>& vchDistribute) override;
    bool AddNewPublish(const uint256& hashAnchor, const CDestination& dest,
                       const std::vector<unsigned char>& vchPublish) override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;
    void UpdatePrimaryBlock(const CBlock& block, const CWorldLineUpdate& updateWorldLine, const CTxSetChange& changeTxSet, const uint64& nNonce);
    void ActivateFork(const uint256& hashFork, const uint64& nNonce);
    bool ProcessForkTx(const uint256& txid, const CTransaction& tx);
    void SyncForkHeight(int nPrimaryHeight);

protected:
    ICoreProtocol* pCoreProtocol;
    IWorldLineController* pWorldLineCtrl;
    ITxPoolController* pTxPoolCtrl;
    IForkManager* pForkManager;
    IConsensus* pConsensus;
    IWallet* pWallet;
    IService* pService;
    IBlockMaker* pBlockMaker;
    network::INetChannelController* pNetChannelCtrl;
    network::IDelegatedChannel* pDelegatedChannel;
    IDataStat* pDataStat;
};

} // namespace bigbang

#endif //BIGBANG_DISPATCHER_H
