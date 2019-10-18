// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_TXPOOL_TXPOOL_H
#define BIGBANG_TXPOOL_TXPOOL_H

#include <boost/thread/shared_mutex.hpp>
#include <map>
#include <unordered_map>

#include "base.h"
#include "txpool/txpoolview.h"
#include "txpool/txset.h"
#include "txpooldata.h"

namespace bigbang
{

class CTxPool : public ITxPool
{
public:
    CTxPool();
    ~CTxPool();
    bool Exists(const uint256& txid) const override;
    std::size_t Count(const uint256& fork) const override;
    bool Get(const uint256& txid, CTransaction& tx) const override;
    void ListTx(const uint256& hashFork, std::vector<std::pair<uint256, std::size_t>>& vTxPool) const override;
    void ListTx(const uint256& hashFork, std::vector<uint256>& vTxPool) const override;
    bool FilterTx(const uint256& hashFork, CTxFilter& filter) const override;
    void ArrangeBlockTx(const uint256& hashFork, int64 nBlockTime, std::size_t nMaxSize,
                        std::vector<CTransaction>& vtx, int64& nTotalTxFee) const override;
    bool FetchInputs(const uint256& hashFork, const CTransaction& tx, std::vector<CTxOut>& vUnspent) const override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;
    bool LoadData();
    bool SaveData();
    Errno AddNew(CTxPoolView& txView, const uint256& txid, const CTransaction& tx, const uint256& hashFork, int nForkHeight);
    std::size_t GetSequenceNumber()
    {
        if (mapTx.empty())
        {
            nLastSequenceNumber = 0;
        }
        return ++nLastSequenceNumber;
    }

    void Clear() override;
    Errno Push(const CTransaction& tx, uint256& hashFork, CDestination& destIn, int64& nValueIn) override;
    void Pop(const uint256& txid) override;
    bool SynchronizeWorldLine(const CWorldLineUpdate& update, CTxSetChange& change) override;

protected:
    storage::CTxPoolData datTxPool;
    mutable boost::shared_mutex rwAccess;
    ICoreProtocol* pCoreProtocol;
    IWorldLineController* pWorldLineCtrl;
    std::map<uint256, CTxPoolView> mapPoolView;
    std::unordered_map<uint256, CPooledTx> mapTx;
    std::size_t nLastSequenceNumber;
};

} // namespace bigbang

#endif //BIGBANG_TXPOOL_TXPOOL_H
