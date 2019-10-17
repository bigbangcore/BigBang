// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_TXPOOL_CONTROLLER_H
#define BIGBANG_TXPOOL_CONTROLLER_H

#include "base.h"
#include "message.h"

namespace bigbang
{

class CTxPoolController : public ITxPoolController
{
public:
    CTxPoolController();
    ~CTxPoolController();
    void Clear() override;
    Errno Push(const CTransaction& tx, uint256& hashFork, CDestination& destIn, int64& nValueIn) override;
    void Pop(const uint256& txid) override;
    bool SynchronizeWorldLine(const CWorldLineUpdate& update, CTxSetChange& change) override;

public:
    bool Exists(const uint256& txid) override;
    std::size_t Count(const uint256& fork) const override;
    bool Get(const uint256& txid, CTransaction& tx) const override;
    void ListTx(const uint256& hashFork, std::vector<std::pair<uint256, std::size_t>>& vTxPool) override;
    void ListTx(const uint256& hashFork, std::vector<uint256>& vTxPool) override;
    bool FilterTx(const uint256& hashFork, CTxFilter& filter) override;
    void ArrangeBlockTx(const uint256& hashFork, int64 nBlockTime, std::size_t nMaxSize,
                        std::vector<CTransaction>& vtx, int64& nTotalTxFee) override;
    bool FetchInputs(const uint256& hashFork, const CTransaction& tx, std::vector<CTxOut>& vUnspent) override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;

    void HandleAddTx(const CAddTxMessage& msg);
    void HandleRemoveTx(const CRemoveTxMessage& msg);
    void HandleClearTx(const CClearTxMessage& msg);
    void HandleAddedBlock(const CAddedBlockMessage& msg);
};

} // namespace bigbang

#endif //BIGBANG_TXPOOL_CONTROLLER_H
