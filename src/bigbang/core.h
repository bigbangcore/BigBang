// Copyright (c) 2019 The Bigbang developers
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
    virtual Errno ValidateTransaction(const CTransaction& tx) override;
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
    virtual bool GetProofOfWorkTarget(const CBlockIndex* pIndexPrev, int nAlgo, int& nBits, int64& nReward) override;
    virtual int GetProofOfWorkRunTimeBits(int nBits, int64 nTime, int64 nPrevTime) override;
    virtual int64 GetPrimaryMintWorkReward(const CBlockIndex* pIndexPrev) override;
    virtual void GetDelegatedBallot(const uint256& nAgreement, std::size_t nWeight,
                                    const std::map<CDestination, size_t>& mapBallot, std::vector<CDestination>& vBallot, int nBlockHeight) override;
    virtual bool CheckFirstPow(int nBlockHeight) override;

protected:
    bool HandleInitialize() override;
    Errno Debug(const Errno& err, const char* pszFunc, const char* pszFormat, ...);
    bool CheckBlockSignature(const CBlock& block);
    Errno ValidateVacantBlock(const CBlock& block);

protected:
    uint256 hashGenesisBlock;
    int nProofOfWorkLimit;
    int nProofOfWorkInit;
    int64 nProofOfWorkUpperTarget;
    int64 nProofOfWorkLowerTarget;
};

class CTestNetCoreProtocol : public CCoreProtocol
{
public:
    CTestNetCoreProtocol();
    void GetGenesisBlock(CBlock& block) override;
};

} // namespace bigbang

#endif //BIGBANG_BASE_H
