// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_CONSENSUS_H
#define BIGBANG_CONSENSUS_H

#include "base.h"
#include "delegate.h"

namespace bigbang
{

class CDelegateTx
{
public:
    uint16 nVersion;
    uint16 nType;
    uint32 nTimeStamp;
    uint32 nLockUntil;
    int64 nAmount;
    int64 nChange;
    int nBlockHeight;

public:
    CDelegateTx()
    {
        SetNull();
    }
    CDelegateTx(const CAssembledTx& tx)
    {
        nVersion = tx.nVersion;
        nType = tx.nType;
        nTimeStamp = tx.nTimeStamp;
        nLockUntil = tx.nLockUntil;
        nAmount = tx.nAmount;
        nChange = tx.GetChange();
        nBlockHeight = tx.nBlockHeight;
    }
    void SetNull()
    {
        nVersion = 0;
        nType = 0;
        nTimeStamp = 0;
        nLockUntil = 0;
        nAmount = 0;
        nChange = 0;
        nBlockHeight = 0;
    }
    bool IsNull() const
    {
        return (nVersion == 0);
    }
    int64 GetTxTime() const
    {
        return ((int64)nTimeStamp);
    };
    bool IsLocked(const uint32 n, const int nBlockHeight) const
    {
        if (n == (nLockUntil >> 31))
        {
            return (nBlockHeight < (nLockUntil & 0x7FFFFFFF));
        }
        return false;
    }
};

class CDelegateContext
{
public:
    CDelegateContext();
    CDelegateContext(const crypto::CKey& keyDelegateIn, const CDestination& destOwnerIn);
    void Clear();
    const CDestination GetDestination() const
    {
        return destDelegate;
    }
    void ChangeTxSet(const CTxSetChange& change);
    void AddNewTx(const CAssembledTx& tx);
    bool BuildEnrollTx(CTransaction& tx, int nBlockHeight, int64 nTime,
                       const uint256& hashAnchor, int64 nTxFee, const std::vector<unsigned char>& vchData);

protected:
    CDestination destDelegate;
    crypto::CKey keyDelegate;
    CDestination destOwner;
    CTemplatePtr templDelegate;
    std::map<uint256, CDelegateTx> mapTx;
    std::map<CTxOutPoint, CDelegateTx*> mapUnspent;
};

class CConsensus : public IConsensus
{
public:
    CConsensus();
    ~CConsensus();
    void PrimaryUpdate(const CBlockChainUpdate& update, const CTxSetChange& change, CDelegateRoutine& routine) override;
    void AddNewTx(const CAssembledTx& tx) override;
    bool AddNewDistribute(int nAnchorHeight, const uint256& hashDistributeAnchor, const CDestination& destFrom, const std::vector<unsigned char>& vchDistribute) override;
    bool AddNewPublish(int nAnchorHeight, const uint256& hashPublishAnchor, const CDestination& destFrom, const std::vector<unsigned char>& vchPublish) override;
    void GetAgreement(int nTargetHeight, uint256& nAgreement, std::size_t& nWeight, std::vector<CDestination>& vBallot) override;
    void GetProof(int nTargetHeight, std::vector<unsigned char>& vchProof) override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;

    bool LoadDelegateTx();
    bool LoadChain();

protected:
    boost::mutex mutex;
    ICoreProtocol* pCoreProtocol;
    IBlockChain* pBlockChain;
    ITxPool* pTxPool;
    delegate::CDelegate delegate;
    std::map<CDestination, CDelegateContext> mapContext;
};

} // namespace bigbang

#endif //BIGBANG_CONSENSUS_H
