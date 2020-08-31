// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_BLOCKDB_H
#define STORAGE_BLOCKDB_H

#include "block.h"
#include "blockindexdb.h"
#include "delegatedb.h"
#include "forkcontext.h"
#include "forkdb.h"
#include "transaction.h"
#include "txindexdb.h"
#include "unspentdb.h"

namespace bigbang
{
namespace storage
{

class CBlockDB
{
public:
    CBlockDB();
    ~CBlockDB();
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool RemoveAll();
    bool AddNewForkContext(const CForkContext& ctxt);
    bool RetrieveForkContext(const uint256& hash, CForkContext& ctxt);
    bool ListForkContext(std::vector<CForkContext>& vForkCtxt);
    bool AddNewFork(const uint256& hash);
    bool RemoveFork(const uint256& hash);
    bool ListFork(std::vector<std::pair<uint256, uint256>>& vFork);
    bool UpdateFork(const uint256& hash, const uint256& hashRefBlock, const uint256& hashForkBased,
                    const std::vector<std::pair<uint256, CTxIndex>>& vTxNew, const std::vector<uint256>& vTxDel,
                    const std::vector<CTxUnspent>& vAddNew, const std::vector<CTxOutPoint>& vRemove);
    bool AddNewBlock(const CBlockOutline& outline);
    bool RemoveBlock(const uint256& hash);
    bool UpdateDelegateContext(const uint256& hash, const CDelegateContext& ctxtDelegate);
    bool WalkThroughBlock(CBlockDBWalker& walker);
    bool RetrieveTxIndex(const uint256& txid, CTxIndex& txIndex, uint256& fork);
    bool RetrieveTxIndex(const uint256& fork, const uint256& txid, CTxIndex& txIndex);
    bool RetrieveTxUnspent(const uint256& fork, const CTxOutPoint& out, CTxOut& unspent);
    bool WalkThroughUnspent(const uint256& hashFork, CForkUnspentDBWalker& walker);
    bool RetrieveDelegate(const uint256& hash, std::map<CDestination, int64>& mapDelegate);
    bool RetrieveEnroll(const uint256& hash, std::map<int, std::map<CDestination, CDiskPos>>& mapEnrollTxPos);
    bool RetrieveEnroll(int height, const std::vector<uint256>& vBlockRange,
                        std::map<CDestination, CDiskPos>& mapEnrollTxPos);
    bool AddForkIncreaseCoin(const uint256& hashFork, int nHeight, int64 nAmount, int64 nMint, const uint256& hashTx);
    bool RetrieveForkIncreaseCoin(const uint256& hashFork, int nHeight, int64& nAmount, int64& nMint, uint256& hashTx);
    bool RemoveForkIncreaseCoin(const uint256& hashFork, int nHeight);
    bool ListForkIncreaseCoin(const uint256& hashFork, CForkIncreaseCoin& incCoin);

protected:
    bool LoadFork();

protected:
    CForkDB dbFork;
    CBlockIndexDB dbBlockIndex;
    CTxIndexDB dbTxIndex;
    CUnspentDB dbUnspent;
    //CDelegateDB dbDelegate;
};

} // namespace storage
} // namespace bigbang

#endif //STORAGE_BLOCKDB_H
