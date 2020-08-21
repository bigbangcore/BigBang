// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_BLOCKCHAIN_H
#define BIGBANG_BLOCKCHAIN_H

#include <map>
#include <uint256.h>

#include "base.h"
#include "blockbase.h"
namespace bigbang
{

class CBlockChain : public IBlockChain
{
public:
    CBlockChain();
    ~CBlockChain();
    void GetForkStatus(std::map<uint256, CForkStatus>& mapForkStatus) override;
    bool GetForkProfile(const uint256& hashFork, CProfile& profile) override;
    bool GetForkContext(const uint256& hashFork, CForkContext& ctxt) override;
    bool GetForkAncestry(const uint256& hashFork, std::vector<std::pair<uint256, uint256>> vAncestry) override;
    int GetBlockCount(const uint256& hashFork) override;
    bool GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight) override;
    bool GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight, uint256& hashNext) override;
    bool GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock) override;
    bool GetBlockHash(const uint256& hashFork, int nHeight, std::vector<uint256>& vBlockHash) override;
    bool GetLastBlockOfHeight(const uint256& hashFork, const int nHeight, uint256& hashBlock, int64& nTime) override;
    bool GetLastBlock(const uint256& hashFork, uint256& hashBlock, int& nHeight, int64& nTime, uint16& nMintType) override;
    bool GetLastBlockTime(const uint256& hashFork, int nDepth, std::vector<int64>& vTime) override;
    bool GetBlock(const uint256& hashBlock, CBlock& block) override;
    bool GetBlockEx(const uint256& hashBlock, CBlockEx& block) override;
    bool GetOrigin(const uint256& hashFork, CBlock& block) override;
    bool Exists(const uint256& hashBlock) override;
    bool GetTransaction(const uint256& txid, CTransaction& tx) override;
    bool GetTransaction(const uint256& txid, CTransaction& tx, uint256& hashFork, int& nHeight) override;
    bool ExistsTx(const uint256& txid) override;
    bool GetTxLocation(const uint256& txid, uint256& hashFork, int& nHeight) override;
    bool GetTxUnspent(const uint256& hashFork, const std::vector<CTxIn>& vInput,
                      std::vector<CTxOut>& vOutput) override;
    bool FilterTx(const uint256& hashFork, CTxFilter& filter) override;
    bool FilterTx(const uint256& hashFork, int nDepth, CTxFilter& filter) override;
    bool ListForkContext(std::vector<CForkContext>& vForkCtxt) override;
    Errno AddNewForkContext(const CTransaction& txFork, CForkContext& ctxt) override;
    Errno AddNewBlock(const CBlock& block, CBlockChainUpdate& update) override;
    bool GetProofOfWorkTarget(const uint256& hashPrev, int nAlgo, uint32_t& nBits, int64& nReward) override;
    bool GetBlockMintReward(const uint256& hashPrev, int64& nReward) override;
    bool GetBlockLocator(const uint256& hashFork, CBlockLocator& locator, uint256& hashDepth, int nIncStep) override;
    bool GetBlockInv(const uint256& hashFork, const CBlockLocator& locator, std::vector<uint256>& vBlockHash, std::size_t nMaxCount) override;
    bool ListForkUnspent(const uint256& hashFork, const CDestination& dest, uint32 nMax, std::vector<CTxUnspent>& vUnspent) override;
    bool ListForkUnspentBatch(const uint256& hashFork, uint32 nMax, std::map<CDestination, std::vector<CTxUnspent>>& mapUnspent) override;
    bool VerifyRepeatBlock(const uint256& hashFork, const CBlock& block) override;
    int64 GetBlockMoneySupply(const uint256& hashBlock) override;

    /////////////    CheckPoints    /////////////////////
    bool HasCheckPoints() const override;
    bool GetCheckPointByHeight(int nHeight, CCheckPoint& point) override;
    std::vector<CCheckPoint> CheckPoints() const override;
    CCheckPoint LatestCheckPoint() const override;
    bool VerifyCheckPoint(int nHeight, const uint256& nBlockHash) override;
    bool FindPreviousCheckPointBlock(CBlock& block) override;

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
    Errno VerifyBlock(const uint256& hashBlock, const CBlock& block, CBlockIndex* pIndexPrev, int64& nReward);

    void InitCheckPoints();

protected:
    boost::shared_mutex rwAccess;
    ICoreProtocol* pCoreProtocol;
    ITxPool* pTxPool;
    storage::CBlockBase cntrBlock;

    std::map<int, CCheckPoint> mapCheckPoints;
    std::vector<CCheckPoint> vecCheckPoints;
};

} // namespace bigbang

#endif //BIGBANG_BLOCKCHAIN_H
