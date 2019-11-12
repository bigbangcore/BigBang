// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_WORLDLINE_H
#define BIGBANG_WORLDLINE_H

#include <map>

#include "base.h"
#include "blockbase.h"
#include "message.h"

namespace bigbang
{

class CWorldLine : public IWorldLine
{
public:
    CWorldLine();
    ~CWorldLine();
    void GetForkStatus(std::map<uint256, CForkStatus>& mapForkStatus) override;
    bool GetForkProfile(const uint256& hashFork, CProfile& profile) override;
    bool GetForkContext(const uint256& hashFork, CForkContext& ctxt) override;
    bool GetForkAncestry(const uint256& hashFork, std::vector<std::pair<uint256, uint256>> vAncestry) override;
    int GetBlockCount(const uint256& hashFork) override;
    bool GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight) override;
    bool GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock) override;
    bool GetBlockHash(const uint256& hashFork, int nHeight, std::vector<uint256>& vBlockHash) override;
    bool GetLastBlock(const uint256& hashFork, uint256& hashBlock, int& nHeight, int64& nTime) override;
    bool GetLastBlockTime(const uint256& hashFork, int nDepth, std::vector<int64>& vTime) override;
    bool GetBlock(const uint256& hashBlock, CBlock& block) override;
    bool GetBlockEx(const uint256& hashBlock, CBlockEx& block) override;
    bool GetOrigin(const uint256& hashFork, CBlock& block) override;
    bool Exists(const uint256& hashBlock) override;
    bool GetTransaction(const uint256& txid, CTransaction& tx) override;
    bool ExistsTx(const uint256& txid) override;
    bool GetTxLocation(const uint256& txid, uint256& hashFork, int& nHeight) override;
    bool GetTxUnspent(const uint256& hashFork, const std::vector<CTxIn>& vInput,
                      std::vector<CTxOut>& vOutput) override;
    bool FilterTx(const uint256& hashFork, CTxFilter& filter) override;
    bool FilterTx(const uint256& hashFork, int nDepth, CTxFilter& filter) override;
    bool ListForkContext(std::vector<CForkContext>& vForkCtxt) override;
    Errno AddNewForkContext(const CTransaction& txFork, CForkContext& ctxt) override;
    Errno AddNewBlock(const CBlock& block, CWorldLineUpdate& update) override;
    Errno AddNewOrigin(const CBlock& block, CWorldLineUpdate& update) override;
    bool GetProofOfWorkTarget(const uint256& hashPrev, int nAlgo, int& nBits, int64& nReward) override;
    bool GetBlockMintReward(const uint256& hashPrev, int64& nReward) override;
    bool GetBlockLocator(const uint256& hashFork, CBlockLocator& locator) override;
    bool GetBlockLocatorFromHash(const uint256& hashFork, const uint256& blockHash, CBlockLocator& locator) override;
    bool GetBlockInv(const uint256& hashFork, const CBlockLocator& locator, std::vector<uint256>& vBlockHash, std::size_t nMaxCount) override;
    bool GetBlockDelegateEnrolled(const uint256& hashBlock, CDelegateEnrolled& enrolled) override;
    bool GetBlockDelegateAgreement(const uint256& hashBlock, CDelegateAgreement& agreement) override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;
    bool CheckContainer();
    bool RebuildContainer();
    bool InsertGenesisBlock(CBlock& block);
    Errno GetTxContxt(storage::CBlockView& view, const CTransaction& tx, CTxContxt& txContxt);
    bool GetBlockChanges(const CBlockIndex* pIndexNew, const CBlockIndex* pIndexFork,
                         std::vector<CBlockEx>& vBlockAddNew, std::vector<CBlockEx>& vBlockRemove);
    bool GetBlockDelegateAgreement(const uint256& hashBlock, const CBlock& block, const CBlockIndex* pIndexPrev,
                                   CDelegateAgreement& agreement);
    Errno VerifyBlock(const uint256& hashBlock, const CBlock& block, CBlockIndex* pIndexPrev, int64& nReward);

protected:
    ICoreProtocol* pCoreProtocol;
    ITxPoolController* pTxPoolCtrl;
    storage::CBlockBase blockBase;
    xengine::CCache<uint256, CDelegateEnrolled> cacheEnrolled;
    xengine::CCache<uint256, CDelegateAgreement> cacheAgreement;
};

class CWorldLineController : public IWorldLineController
{
public:
    CWorldLineController();
    ~CWorldLineController();
    Errno AddNewForkContext(const CTransaction& txFork, CForkContext& ctxt) override;
    Errno AddNewBlock(const CBlock& block, CWorldLineUpdate& update, uint64 nNonce) override;
    Errno AddNewOrigin(const CBlock& block, CWorldLineUpdate& update, uint64 nNonce) override;

public:
    void GetForkStatus(std::map<uint256, CForkStatus>& mapForkStatus) override;
    bool GetForkProfile(const uint256& hashFork, CProfile& profile) override;
    bool GetForkContext(const uint256& hashFork, CForkContext& ctxt) override;
    bool GetForkAncestry(const uint256& hashFork, std::vector<std::pair<uint256, uint256>> vAncestry) override;
    int GetBlockCount(const uint256& hashFork) override;
    bool GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight) override;
    bool GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock) override;
    bool GetBlockHash(const uint256& hashFork, int nHeight, std::vector<uint256>& vBlockHash) override;
    bool GetLastBlock(const uint256& hashFork, uint256& hashBlock, int& nHeight, int64& nTime) override;
    bool GetLastBlockTime(const uint256& hashFork, int nDepth, std::vector<int64>& vTime) override;
    bool GetBlock(const uint256& hashBlock, CBlock& block) override;
    bool GetBlockEx(const uint256& hashBlock, CBlockEx& block) override;
    bool GetOrigin(const uint256& hashFork, CBlock& block) override;
    bool Exists(const uint256& hashBlock) override;
    bool GetTransaction(const uint256& txid, CTransaction& tx) override;
    bool ExistsTx(const uint256& txid) override;
    bool GetTxLocation(const uint256& txid, uint256& hashFork, int& nHeight) override;
    bool GetTxUnspent(const uint256& hashFork, const std::vector<CTxIn>& vInput,
                      std::vector<CTxOut>& vOutput) override;
    bool FilterTx(const uint256& hashFork, CTxFilter& filter) override;
    bool FilterTx(const uint256& hashFork, int nDepth, CTxFilter& filter) override;
    bool ListForkContext(std::vector<CForkContext>& vForkCtxt) override;
    bool GetProofOfWorkTarget(const uint256& hashPrev, int nAlgo, int& nBits, int64& nReward) override;
    bool GetBlockMintReward(const uint256& hashPrev, int64& nReward) override;
    bool GetBlockLocator(const uint256& hashFork, CBlockLocator& locator) override;
    bool GetBlockLocatorFromHash(const uint256& hashFork, const uint256& blockHash, CBlockLocator& locator) override;
    bool GetBlockInv(const uint256& hashFork, const CBlockLocator& locator, std::vector<uint256>& vBlockHash, std::size_t nMaxCount) override;
    bool GetBlockDelegateEnrolled(const uint256& hashBlock, CDelegateEnrolled& enrolled) override;
    bool GetBlockDelegateAgreement(const uint256& hashBlock, CDelegateAgreement& agreement) override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;

    void HandleAddBlock(std::shared_ptr<CAddBlockMessage> msg);

protected:
    IForkManager* pForkManager;
};

} // namespace bigbang

#endif //BIGBANG_WORLDLINE_H
