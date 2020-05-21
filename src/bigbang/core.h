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

    virtual Errno VerifyBlock(const CBlock& block, CBlockIndex* pIndexPrev) override;
    virtual Errno VerifyBlockTx(const CTransaction& tx, const CTxContxt& txContxt, CBlockIndex* pIndexPrev, int nForkHeight, const uint256& fork) override;
    virtual Errno VerifyTransaction(const CTransaction& tx, const std::vector<CTxOut>& vPrevOutput, int nForkHeight, const uint256& fork) override;

    virtual Errno VerifyProofOfWork(const CBlock& block, const CBlockIndex* pIndexPrev) override;
    virtual Errno VerifyDelegatedProofOfStake(const CBlock& block, const CBlockIndex* pIndexPrev,
                                              const CDelegateAgreement& agreement) override;
    virtual Errno VerifySubsidiary(const CBlock& block, const CBlockIndex* pIndexPrev, const CBlockIndex* pIndexRef,
                                   const CDelegateAgreement& agreement) override;
    virtual bool GetBlockTrust(const CBlock& block, uint256& nChainTrust, const CBlockIndex* pIndexPrev = nullptr, const CDelegateAgreement& agreement = CDelegateAgreement(), const CBlockIndex* pIndexRef = nullptr, std::size_t nEnrollTrust = 0) override;
    virtual bool GetProofOfWorkTarget(const CBlockIndex* pIndexPrev, int nAlgo, int& nBits, int64& nReward) override;
    virtual bool IsDposHeight(int height) override;
    virtual int64 GetPrimaryMintWorkReward(const CBlockIndex* pIndexPrev) override;
    virtual void GetDelegatedBallot(const uint256& nAgreement, std::size_t nWeight, const std::map<CDestination, size_t>& mapBallot,
                                    const std::vector<std::pair<CDestination, int64>>& vecAmount, int64 nMoneySupply, std::vector<CDestination>& vBallot, std::size_t& nEnrollTrust, int nBlockHeight) override;
    virtual int64 MinEnrollAmount() override;
    virtual uint32 DPoSTimestamp(const CBlockIndex* pIndexPrev) override;
    virtual uint32 GetNextBlockTimeStamp(uint16 nPrevMintType, uint32 nPrevTimeStamp, uint16 nTargetMintType, int nTargetHeight) override;

protected:
    bool HandleInitialize() override;
    Errno Debug(const Errno& err, const char* pszFunc, const char* pszFormat, ...);
    bool CheckBlockSignature(const CBlock& block);
    Errno ValidateVacantBlock(const CBlock& block);
    bool VerifyDestRecorded(const CTransaction& tx, vector<uint8>& vchSigOut);

protected:
    uint256 hashGenesisBlock;
    int nProofOfWorkLowerLimit;
    int nProofOfWorkUpperLimit;
    int nProofOfWorkInit;
    int64 nProofOfWorkUpperTarget;
    int64 nProofOfWorkLowerTarget;
    int64 nProofOfWorkUpperTargetOfDpos;
    int64 nProofOfWorkLowerTargetOfDpos;
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
    int nProofOfWorkLowerLimit;
    int nProofOfWorkUpperLimit;
    int nProofOfWorkInit;
    int64 nProofOfWorkUpperTarget;
    int64 nProofOfWorkLowerTarget;
    int64 nProofOfWorkUpperTargetOfDpos;
    int64 nProofOfWorkLowerTargetOfDpos;
    int nProofOfWorkAdjustCount;
    int64 nDelegateProofOfStakeEnrollMinimumAmount;
    int64 nDelegateProofOfStakeEnrollMaximumAmount;
    uint32 nDelegateProofOfStakeHeight;

public:
    bool IsDposHeight(int height);
};

} // namespace bigbang

#endif //BIGBANG_BASE_H
