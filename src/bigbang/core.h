// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_CORE_H
#define BIGBANG_CORE_H

#include "base.h"

namespace bigbang
{

class CCoreProtocol : public ICoreProtocol
{
public:
    CCoreProtocol();
    virtual ~CCoreProtocol();
    virtual const uint256& GetGenesisBlockHash() override;
    virtual void GetGenesisBlock(CBlock& block) override;
    virtual Errno ValidateTransaction(const CTransaction& tx, int nHeight) override;
    virtual Errno ValidateBlock(const CBlock& block) override;
    virtual Errno ValidateOrigin(const CBlock& block, const CProfile& parentProfile, CProfile& forkProfile) override;

    virtual Errno VerifyBlockTx(const CTransaction& tx, const CTxContxt& txContxt, CBlockIndex* pIndexPrev, int nForkHeight, const uint256& fork) override;
    virtual Errno VerifyTransaction(const CTransaction& tx, const std::vector<CTxOut>& vPrevOutput, int nForkHeight, const uint256& fork) override;

    virtual Errno VerifyProofOfWork(const CBlock& block, const CBlockIndex* pIndexPrev) override;
    virtual bool GetBlockTrust(const CBlock& block, uint256& nChainTrust) override;
    virtual bool GetProofOfWorkTarget(const CBlockIndex* pIndexPrev, int nAlgo, uint32_t& nBits, int64& nReward) override;
    virtual int64 GetPrimaryMintWorkReward(const CBlockIndex* pIndexPrev) override;

protected:
    bool HandleInitialize() override;
    Errno Debug(const Errno& err, const char* pszFunc, const char* pszFormat, ...);
    bool CheckBlockSignature(const CBlock& block);

protected:
    uint256 hashGenesisBlock;
    uint256 nProofOfWorkLowerLimit;
    uint256 nProofOfWorkUpperLimit;
    uint256 nProofOfWorkInit;
    uint32 nProofOfWorkDifficultyInterval;
    IBlockChain* pBlockChain;
};

class CTestNetCoreProtocol : public CCoreProtocol
{
public:
    CTestNetCoreProtocol();
    void GetGenesisBlock(CBlock& block) override;
};

class CProofOfWorkParam
{
public:
    CProofOfWorkParam(bool fTestnet);

public:
    uint256 hashGenesisBlock;
    uint256 nProofOfWorkLowerLimit;
    uint256 nProofOfWorkUpperLimit;
    uint256 nProofOfWorkInit;
    uint32 nProofOfWorkDifficultyInterval;
    int64 nDelegateProofOfStakeEnrollMinimumAmount;
    int64 nDelegateProofOfStakeEnrollMaximumAmount;
    uint32 nDelegateProofOfStakeHeight;

public:
    bool IsDposHeight(int height);
};

} // namespace bigbang

#endif //BIGBANG_BASE_H
