// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_BLOCKCHAIN_H
#define BIGBANG_BLOCKCHAIN_H

#include <map>
#include <utility>
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
    Errno AddNewOrigin(const CBlock& block, CBlockChainUpdate& update) override;
    bool GetProofOfWorkTarget(const uint256& hashPrev, int nAlgo, int& nBits, int64& nReward) override;
    bool GetBlockMintReward(const uint256& hashPrev, int64& nReward) override;
    bool GetBlockLocator(const uint256& hashFork, CBlockLocator& locator, uint256& hashDepth, int nIncStep) override;
    bool GetBlockInv(const uint256& hashFork, const CBlockLocator& locator, std::vector<uint256>& vBlockHash, std::size_t nMaxCount) override;
    bool GetBlockDelegateEnrolled(const uint256& hashBlock, CDelegateEnrolled& enrolled) override;
    bool ListForkUnspent(const uint256& hashFork, const CDestination& dest, uint32 nMax, std::vector<CTxUnspent>& vUnspent) override;
    bool ListForkUnspentBatch(const uint256& hashFork, uint32 nMax, std::map<CDestination, std::vector<CTxUnspent>>& mapUnspent) override;
    bool GetVotes(const CDestination& destDelegate, int64& nVotes) override;
    bool ListDelegate(uint32 nCount, std::multimap<int64, CDestination>& mapVotes) override;
    bool VerifyRepeatBlock(const uint256& hashFork, const CBlock& block, const uint256& hashBlockRef) override;
    bool GetBlockDelegateVote(const uint256& hashBlock, std::map<CDestination, int64>& mapVote) override;
    int64 GetDelegateMinEnrollAmount(const uint256& hashBlock) override;
    bool GetDelegateCertTxCount(const uint256& hashLastBlock, std::map<CDestination, int>& mapVoteCert) override;
    int64 GetBlockMoneySupply(const uint256& hashBlock) override;
    bool ListDelegatePayment(uint32 height, CBlock& block, std::multimap<int64, CDestination>& mapVotes) override;
    uint32 DPoSTimestamp(const uint256& hashPrev) override;
    Errno VerifyPowBlock(const CBlock& block, bool& fLongChain) override;

    /////////////    CheckPoints    /////////////////////
    typedef std::map<int, CCheckPoint> MapCheckPointsType;
    typedef std::vector<CCheckPoint> VecCheckPointsType;
    typedef std::pair<MapCheckPointsType, VecCheckPointsType> CheckPointsPairType;

    bool HasCheckPoints(const uint256& hashFork) const override;
    bool GetCheckPointByHeight(const uint256& hashFork, int nHeight, CCheckPoint& point) override;
    std::vector<CCheckPoint> CheckPoints(const uint256& hashFork) const override;
    CCheckPoint LatestCheckPoint(const uint256& hashFork) const override;
    bool VerifyCheckPoint(const uint256& hashFork,int nHeight, const uint256& nBlockHash) override;
    bool FindPreviousCheckPointBlock(const uint256& hashFork, CBlock& block) override;
    bool IsSameBranch(const uint256& hashFork, const CCheckPoint& point, const CBlock& block) override;
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
                                   CDelegateAgreement& agreement, std::size_t& nEnrollTrust);
    bool GetBlockDelegateAgreement(const uint256& hashBlock, CDelegateAgreement& agreement);
    Errno VerifyBlock(const uint256& hashBlock, const CBlock& block, CBlockIndex* pIndexPrev,
                      int64& nReward, CDelegateAgreement& agreement, std::size_t& nEnrollTrust, CBlockIndex** ppIndexRef);
    bool VerifyBlockCertTx(const CBlock& block);

    void InitCheckPoints();
    void InitCheckPoints(const uint256& hashFork, const std::vector<CCheckPoint>& vCheckPoints);
    VecCheckPointsType GenerateCheckPoints(const uint256& hashFork);

protected:
    boost::shared_mutex rwAccess;
    ICoreProtocol* pCoreProtocol;
    ITxPool* pTxPool;
    IForkManager* pForkManager;

    storage::CBlockBase cntrBlock;
    xengine::CCache<uint256, CDelegateEnrolled> cacheEnrolled;
    xengine::CCache<uint256, CDelegateAgreement> cacheAgreement;

    VecCheckPointsType vecGenesisCheckPoints;
    std::map<uint256, CheckPointsPairType> mapForkCheckPoints;
};

} // namespace bigbang

#endif //BIGBANG_BLOCKCHAIN_H
