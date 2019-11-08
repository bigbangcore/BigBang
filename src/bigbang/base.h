// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_BASE_H
#define BIGBANG_BASE_H

#include <xengine.h>

#include "block.h"
#include "blockbase.h"
#include "config.h"
#include "crypto.h"
#include "destination.h"
#include "error.h"
#include "key.h"
#include "param.h"
#include "peer.h"
#include "profile.h"
#include "struct.h"
#include "template/mint.h"
#include "template/template.h"
#include "transaction.h"
#include "uint256.h"
#include "wallettx.h"

namespace bigbang
{

class ICoreProtocol : public xengine::IBase
{
public:
    ICoreProtocol()
      : IBase("coreprotocol") {}
    virtual const uint256& GetGenesisBlockHash() const = 0;
    virtual void GetGenesisBlock(CBlock& block) const = 0;
    virtual Errno ValidateTransaction(const CTransaction& tx) = 0;
    virtual Errno ValidateBlock(const CBlock& block) = 0;
    virtual Errno ValidateOrigin(const CBlock& block, const CProfile& parentProfile, CProfile& forkProfile) = 0;
    virtual Errno VerifyProofOfWork(const CBlock& block, const CBlockIndex* pIndexPrev) = 0;
    virtual Errno VerifyDelegatedProofOfStake(const CBlock& block, const CBlockIndex* pIndexPrev,
                                              const CDelegateAgreement& agreement) = 0;
    virtual Errno VerifySubsidiary(const CBlock& block, const CBlockIndex* pIndexPrev, const CBlockIndex* pIndexRef,
                                   const CDelegateAgreement& agreement) = 0;
    virtual Errno VerifyBlock(const CBlock& block, CBlockIndex* pIndexPrev) = 0;
    virtual Errno VerifyBlockTx(const CTransaction& tx, const CTxContxt& txContxt, CBlockIndex* pIndexPrev, int nForkHeight, const uint256& fork) = 0;
    virtual Errno VerifyTransaction(const CTransaction& tx, const std::vector<CTxOut>& vPrevOutput, int nForkHeight, const uint256& fork) = 0;
    virtual bool GetProofOfWorkTarget(const CBlockIndex* pIndexPrev, int nAlgo, int& nBits, int64& nReward) = 0;
    virtual int GetProofOfWorkRunTimeBits(int nBits, int64 nTime, int64 nPrevTime) = 0;
    virtual int64 GetPrimaryMintWorkReward(const CBlockIndex* pIndexPrev) = 0;
    virtual void GetDelegatedBallot(const uint256& nAgreement, std::size_t nWeight,
                                    const std::map<CDestination, size_t>& mapBallot, std::vector<CDestination>& vBallot, int nBlockHeight) = 0;
    virtual bool CheckFirstPow(int nBlockHeight) = 0;
};

class IWorldLineController;
class IWorldLine : public xengine::IModel
{
    friend class IWorldLineController;

public:
    IWorldLine()
      : IModel("worldline") {}

    virtual void GetForkStatus(std::map<uint256, CForkStatus>& mapForkStatus) = 0;
    virtual bool GetForkProfile(const uint256& hashFork, CProfile& profile) = 0;
    virtual bool GetForkContext(const uint256& hashFork, CForkContext& ctxt) = 0;
    virtual bool GetForkAncestry(const uint256& hashFork, std::vector<std::pair<uint256, uint256>> vAncestry) = 0;
    virtual int GetBlockCount(const uint256& hashFork) = 0;
    virtual bool GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight) = 0;
    virtual bool GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock) = 0;
    virtual bool GetBlockHash(const uint256& hashFork, int nHeight, std::vector<uint256>& vBlockHash) = 0;
    virtual bool GetLastBlock(const uint256& hashFork, uint256& hashBlock, int& nHeight, int64& nTime) = 0;
    virtual bool GetLastBlockTime(const uint256& hashFork, int nDepth, std::vector<int64>& vTime) = 0;
    virtual bool GetBlock(const uint256& hashBlock, CBlock& block) = 0;
    virtual bool GetBlockEx(const uint256& hashBlock, CBlockEx& block) = 0;
    virtual bool GetOrigin(const uint256& hashFork, CBlock& block) = 0;
    virtual bool Exists(const uint256& hashBlock) = 0;
    virtual bool GetTransaction(const uint256& txid, CTransaction& tx) = 0;
    virtual bool GetTxLocation(const uint256& txid, uint256& hashFork, int& nHeight) = 0;
    virtual bool GetTxUnspent(const uint256& hashFork, const std::vector<CTxIn>& vInput,
                              std::vector<CTxOut>& vOutput) = 0;
    virtual bool ExistsTx(const uint256& txid) = 0;
    virtual bool FilterTx(const uint256& hashFork, CTxFilter& filter) = 0;
    virtual bool FilterTx(const uint256& hashFork, int nDepth, CTxFilter& filter) = 0;
    virtual bool ListForkContext(std::vector<CForkContext>& vForkCtxt) = 0;
    virtual bool GetProofOfWorkTarget(const uint256& hashPrev, int nAlgo, int& nBits, int64& nReward) = 0;
    virtual bool GetBlockMintReward(const uint256& hashPrev, int64& nReward) = 0;
    virtual bool GetBlockLocator(const uint256& hashFork, CBlockLocator& locator) = 0;
    virtual bool GetBlockLocatorFromHash(const uint256& hashFork, const uint256& blockHash, CBlockLocator& locator) = 0;
    virtual bool GetBlockInv(const uint256& hashFork, const CBlockLocator& locator, std::vector<uint256>& vBlockHash, std::size_t nMaxCount) = 0;
    virtual bool GetBlockDelegateEnrolled(const uint256& hashBlock, CDelegateEnrolled& enrolled) = 0;
    virtual bool GetBlockDelegateAgreement(const uint256& hashRefBlock, CDelegateAgreement& agreement) = 0;

protected:
    virtual Errno AddNewForkContext(const CTransaction& txFork, CForkContext& ctxt) = 0;
    virtual Errno AddNewBlock(const CBlock& block, CWorldLineUpdate& update) = 0;
    virtual Errno AddNewOrigin(const CBlock& block, CWorldLineUpdate& update) = 0;

protected:
    const CBasicConfig* Config()
    {
        return dynamic_cast<const CBasicConfig*>(xengine::IBase::Config());
    }
    const CStorageConfig* StorageConfig()
    {
        return dynamic_cast<const CStorageConfig*>(xengine::IBase::Config());
    }
};

class IWorldLineController : public xengine::CIOActor
{
public:
    IWorldLineController()
      : CIOActor("worldlinecontroller"), pWorldLine(nullptr) {}

    // TODO: remove these functions because of existed in IWorldLine
    virtual Errno AddNewForkContext(const CTransaction& txFork, CForkContext& ctxt) = 0;
    virtual Errno AddNewBlock(const CBlock& block, CWorldLineUpdate& update, uint64 nNonce) = 0;
    virtual Errno AddNewOrigin(const CBlock& block, CWorldLineUpdate& update, uint64 nNonce) = 0;

    virtual void GetForkStatus(std::map<uint256, CForkStatus>& mapForkStatus) = 0;
    virtual bool GetForkProfile(const uint256& hashFork, CProfile& profile) = 0;
    virtual bool GetForkContext(const uint256& hashFork, CForkContext& ctxt) = 0;
    virtual bool GetForkAncestry(const uint256& hashFork, std::vector<std::pair<uint256, uint256>> vAncestry) = 0;
    virtual int GetBlockCount(const uint256& hashFork) = 0;
    virtual bool GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight) = 0;
    virtual bool GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock) = 0;
    virtual bool GetBlockHash(const uint256& hashFork, int nHeight, std::vector<uint256>& vBlockHash) = 0;
    virtual bool GetLastBlock(const uint256& hashFork, uint256& hashBlock, int& nHeight, int64& nTime) = 0;
    virtual bool GetLastBlockTime(const uint256& hashFork, int nDepth, std::vector<int64>& vTime) = 0;
    virtual bool GetBlock(const uint256& hashBlock, CBlock& block) = 0;
    virtual bool GetBlockEx(const uint256& hashBlock, CBlockEx& block) = 0;
    virtual bool GetOrigin(const uint256& hashFork, CBlock& block) = 0;
    virtual bool Exists(const uint256& hashBlock) = 0;
    virtual bool GetTransaction(const uint256& txid, CTransaction& tx) = 0;
    virtual bool GetTxLocation(const uint256& txid, uint256& hashFork, int& nHeight) = 0;
    virtual bool GetTxUnspent(const uint256& hashFork, const std::vector<CTxIn>& vInput,
                              std::vector<CTxOut>& vOutput) = 0;
    virtual bool ExistsTx(const uint256& txid) = 0;
    virtual bool FilterTx(const uint256& hashFork, CTxFilter& filter) = 0;
    virtual bool FilterTx(const uint256& hashFork, int nDepth, CTxFilter& filter) = 0;
    virtual bool ListForkContext(std::vector<CForkContext>& vForkCtxt) = 0;
    virtual bool GetProofOfWorkTarget(const uint256& hashPrev, int nAlgo, int& nBits, int64& nReward) = 0;
    virtual bool GetBlockMintReward(const uint256& hashPrev, int64& nReward) = 0;
    virtual bool GetBlockLocator(const uint256& hashFork, CBlockLocator& locator) = 0;
    virtual bool GetBlockLocatorFromHash(const uint256& hashFork, const uint256& hashBlock, CBlockLocator& locator) = 0;
    virtual bool GetBlockInv(const uint256& hashFork, const CBlockLocator& locator, std::vector<uint256>& vBlockHash, std::size_t nMaxCount) = 0;
    virtual bool GetBlockDelegateEnrolled(const uint256& hashBlock, CDelegateEnrolled& enrolled) = 0;
    virtual bool GetBlockDelegateAgreement(const uint256& hashRefBlock, CDelegateAgreement& agreement) = 0;

protected:
    Errno AddNewForkContextIntoWorldLine(const CTransaction& txFork, CForkContext& ctxt)
    {
        return pWorldLine->AddNewForkContext(txFork, ctxt);
    }
    Errno AddNewBlockIntoWorldLine(const CBlock& block, CWorldLineUpdate& update)
    {
        return pWorldLine->AddNewBlock(block, update);
    }
    Errno AddNewOriginIntoWorldLine(const CBlock& block, CWorldLineUpdate& update)
    {
        return pWorldLine->AddNewOrigin(block, update);
    }

protected:
    IWorldLine* pWorldLine;
};

class ITxPoolController;
class ITxPool : public xengine::IModel
{
    friend class ITxPoolController;

public:
    ITxPool()
      : IModel("txpool") {}

    // TODO: remove these functions because of existed in IWorldLine
    virtual bool Exists(const uint256& txid) const = 0;
    virtual std::size_t Count(const uint256& fork) const = 0;
    virtual bool Get(const uint256& txid, CTransaction& tx) const = 0;
    virtual void ListTx(const uint256& hashFork, std::vector<std::pair<uint256, std::size_t>>& vTxPool) const = 0;
    virtual void ListTx(const uint256& hashFork, std::vector<uint256>& vTxPool) const = 0;
    virtual bool FilterTx(const uint256& hashFork, CTxFilter& filter) const = 0;
    virtual void ArrangeBlockTx(const uint256& hashFork, int64 nBlockTime, std::size_t nMaxSize,
                                std::vector<CTransaction>& vtx, int64& nTotalTxFee) const = 0;
    virtual bool FetchInputs(const uint256& hashFork, const CTransaction& tx, std::vector<CTxOut>& vUnspent) const = 0;

protected:
    virtual void Clear() = 0;
    virtual Errno Push(const CTransaction& tx, uint256& hashFork, CDestination& destIn, int64& nValueIn) = 0;
    virtual void Pop(const uint256& txid) = 0;
    virtual bool SynchronizeWorldLine(const CWorldLineUpdate& update, CTxSetChange& change) = 0;
};

class ITxPoolController : public xengine::CIOActor
{
public:
    ITxPoolController()
      : CIOActor("txpoolcontroller"), pTxPool(nullptr) {}
    virtual void Clear() = 0;
    virtual Errno Push(const CTransaction& tx, uint256& hashFork, CDestination& destIn, int64& nValueIn) = 0;
    virtual void Pop(const uint256& txid) = 0;
    virtual bool SynchronizeWorldLine(const CWorldLineUpdate& update, CTxSetChange& change) = 0;
    const CStorageConfig* StorageConfig()
    {
        return dynamic_cast<const CStorageConfig*>(xengine::IBase::Config());
    }

    virtual bool Exists(const uint256& txid) = 0;
    virtual std::size_t Count(const uint256& fork) const = 0;
    virtual bool Get(const uint256& txid, CTransaction& tx) const = 0;
    virtual void ListTx(const uint256& hashFork, std::vector<std::pair<uint256, std::size_t>>& vTxPool) = 0;
    virtual void ListTx(const uint256& hashFork, std::vector<uint256>& vTxPool) = 0;
    virtual bool FilterTx(const uint256& hashFork, CTxFilter& filter) = 0;
    virtual void ArrangeBlockTx(const uint256& hashFork, int64 nBlockTime, std::size_t nMaxSize,
                                std::vector<CTransaction>& vtx, int64& nTotalTxFee) = 0;
    virtual bool FetchInputs(const uint256& hashFork, const CTransaction& tx, std::vector<CTxOut>& vUnspent) = 0;

protected:
    void ClearTxPool()
    {
        pTxPool->Clear();
    }
    Errno PushIntoTxPool(const CTransaction& tx, uint256& hashFork, CDestination& destIn, int64& nValueIn)
    {
        return pTxPool->Push(tx, hashFork, destIn, nValueIn);
    }
    void PopFromTxPool(const uint256& txid)
    {
        pTxPool->Pop(txid);
    }
    bool SynchronizeWorldLineWithTxPool(const CWorldLineUpdate& update, CTxSetChange& change)
    {
        return pTxPool->SynchronizeWorldLine(update, change);
    }

protected:
    ITxPool* pTxPool;
};

class IForkManager : public xengine::IBase
{
public:
    IForkManager()
      : IBase("forkmanager") {}
    virtual bool IsAllowed(const uint256& hashFork) const = 0;
    virtual bool GetJoint(const uint256& hashFork, uint256& hashParent, uint256& hashJoint, int& nHeight) const = 0;
    virtual bool LoadForkContext(std::vector<uint256>& vActive) = 0;
    virtual void ForkUpdate(const CWorldLineUpdate& update, std::vector<uint256>& vActive, std::vector<uint256>& vDeactive) = 0;
    virtual void ForkUpdate(const CWorldLineUpdate& update, std::vector<CTransaction>& vForkTx,
                            std::vector<uint256>& vActive, std::vector<uint256>& vDeactive) = 0;
    virtual bool AddNewForkContext(const CForkContext& ctxt, std::vector<uint256>& vActive) = 0;
    virtual void GetForkList(std::vector<uint256>& vFork) const = 0;
    virtual bool GetSubline(const uint256& hashFork, std::vector<std::pair<int, uint256>>& vSubline) const = 0;
    const CForkConfig* ForkConfig()
    {
        return dynamic_cast<const CForkConfig*>(xengine::IBase::Config());
    }
};

class IConsensusController;
class IConsensus : public xengine::IModel
{
    friend class IConsensusController;

public:
    IConsensus()
      : IModel("consensus") {}
    const CMintConfig* MintConfig()
    {
        return dynamic_cast<const CMintConfig*>(xengine::IBase::Config());
    }
    virtual void GetAgreement(int nTargetHeight, uint256& nAgreement, std::size_t& nWeight,
                              std::vector<CDestination>& vBallot) = 0;
    virtual void GetProof(int nTargetHeight, std::vector<unsigned char>& vchProof) = 0;

protected:
    virtual void PrimaryUpdate(const CWorldLineUpdate& update, const CTxSetChange& change, CDelegateRoutine& routine) = 0;
    virtual void AddNewTx(const CAssembledTx& tx) = 0;
    virtual bool AddNewDistribute(int nAnchorHeight, const CDestination& destFrom, const std::vector<unsigned char>& vchDistribute) = 0;
    virtual bool AddNewPublish(int nAnchorHeight, const CDestination& destFrom, const std::vector<unsigned char>& vchPublish) = 0;
};

class IConsensusController : public xengine::CIOActor
{
public:
    IConsensusController()
      : CIOActor("consensuscontroller"), pConsensus(nullptr) {}

protected:
    void PrimaryUpdate(const CWorldLineUpdate& update, const CTxSetChange& change, CDelegateRoutine& routine)
    {
        pConsensus->PrimaryUpdate(update, change, routine);
    }

    void AddNewTx(const CAssembledTx& tx)
    {
        pConsensus->AddNewTx(tx);
    }

    bool AddNewDistribute(int nAnchorHeight, const CDestination& destFrom, const std::vector<unsigned char>& vchDistribute)
    {
        return pConsensus->AddNewDistribute(nAnchorHeight, destFrom, vchDistribute);
    }

    bool AddNewPublish(int nAnchorHeight, const CDestination& destFrom, const std::vector<unsigned char>& vchPublish)
    {
        return pConsensus->AddNewPublish(nAnchorHeight, destFrom, vchPublish);
    }

protected:
    IConsensus* pConsensus;
};

class IBlockMaker : public xengine::CIOActor
{
public:
    IBlockMaker()
      : CIOActor("blockmaker") {}
    const CMintConfig* MintConfig()
    {
        return dynamic_cast<const CMintConfig*>(xengine::IBase::Config());
    }
};

class IWalletController;
class IWallet : public xengine::IModel
{
    friend class IWalletController;

public:
    IWallet()
      : IModel("wallet") {}
    /* Key store */
    virtual bool AddKey(const crypto::CKey& key) = 0;
    virtual void GetPubKeys(std::set<crypto::CPubKey>& setPubKey) const = 0;
    virtual bool Have(const crypto::CPubKey& pubkey) const = 0;
    virtual bool Export(const crypto::CPubKey& pubkey, std::vector<unsigned char>& vchKey) const = 0;
    virtual bool Import(const std::vector<unsigned char>& vchKey, crypto::CPubKey& pubkey) = 0;
    virtual bool Encrypt(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase,
                         const crypto::CCryptoString& strCurrentPassphrase) = 0;
    virtual bool GetKeyStatus(const crypto::CPubKey& pubkey, int& nVersion, bool& fLocked, int64& nAutoLockTime) const = 0;
    virtual bool IsLocked(const crypto::CPubKey& pubkey) const = 0;
    virtual bool Lock(const crypto::CPubKey& pubkey) = 0;
    virtual bool Unlock(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase, int64 nTimeout) = 0;
    virtual bool Sign(const crypto::CPubKey& pubkey, const uint256& hash, std::vector<uint8>& vchSig) const = 0;
    /* Template */
    virtual void GetTemplateIds(std::set<CTemplateId>& setTemplateId) const = 0;
    virtual bool Have(const CTemplateId& tid) const = 0;
    virtual bool AddTemplate(CTemplatePtr& ptr) = 0;
    virtual CTemplatePtr GetTemplate(const CTemplateId& tid) const = 0;
    /* Wallet Tx */
    virtual std::size_t GetTxCount() = 0;
    virtual bool ListTx(int nOffset, int nCount, std::vector<CWalletTx>& vWalletTx) = 0;
    virtual bool GetBalance(const CDestination& dest, const uint256& hashFork, int nForkHeight, CWalletBalance& balance) = 0;
    virtual bool SignTransaction(const CDestination& destIn, CTransaction& tx, bool& fCompleted) const = 0;
    virtual bool ArrangeInputs(const CDestination& destIn, const uint256& hashFork, int nForkHeight, CTransaction& tx) = 0;
    /* Sync */
    virtual bool SynchronizeWalletTx(const CDestination& destNew) = 0;
    virtual bool ResynchronizeWalletTx() = 0;

    const CBasicConfig* Config()
    {
        return dynamic_cast<const CBasicConfig*>(xengine::IBase::Config());
    }
    const CStorageConfig* StorageConfig()
    {
        return dynamic_cast<const CStorageConfig*>(xengine::IBase::Config());
    }

protected:
    /* Update */
    virtual bool SynchronizeTxSet(const CTxSetChange& change) = 0;
    virtual bool AddNewTx(const uint256& hashFork, const CAssembledTx& tx) = 0;
    virtual bool AddNewFork(const uint256& hashFork, const uint256& hashParent, int nOriginHeight) = 0;
};

class IWalletController : public xengine::CIOActor
{
public:
    IWalletController()
      : CIOActor("walletcontroller"), pWallet(nullptr) {}

protected:
    bool SynchronizeTxSet(const CTxSetChange& change)
    {
        return pWallet->SynchronizeTxSet(change);
    }

    bool AddNewTx(const uint256& hashFork, const CAssembledTx& tx)
    {
        return pWallet->AddNewTx(hashFork, tx);
    }

    bool AddNewFork(const uint256& hashFork, const uint256& hashParent, int nOriginHeight)
    {
        return pWallet->AddNewFork(hashFork, hashParent, nOriginHeight);
    }

protected:
    IWallet* pWallet;
};

class IDispatcher : public xengine::IBase
{
public:
    IDispatcher()
      : IBase("dispatcher") {}
    virtual Errno AddNewBlock(const CBlock& block, uint64 nNonce = 0) = 0;
    virtual Errno AddNewTx(const CTransaction& tx, uint64 nNonce = 0) = 0;
    virtual bool AddNewDistribute(const uint256& hashAnchor, const CDestination& dest,
                                  const std::vector<unsigned char>& vchDistribute) = 0;
    virtual bool AddNewPublish(const uint256& hashAnchor, const CDestination& dest,
                               const std::vector<unsigned char>& vchPublish) = 0;
};

class IServiceController;
class IService : public xengine::IModel
{
    friend class IServiceController;

public:
    IService()
      : IModel("service") {}
    /* System */
    virtual void Stop() = 0;
    /* Network */
    virtual int GetPeerCount() = 0;
    virtual void GetPeers(std::vector<network::CBbPeerInfo>& vPeerInfo) = 0;
    virtual bool AddNode(const xengine::CNetHost& node) = 0;
    virtual bool RemoveNode(const xengine::CNetHost& node) = 0;
    /* WorldLine & Tx Pool*/
    virtual int GetForkCount() = 0;
    virtual bool HaveFork(const uint256& hashFork) = 0;
    virtual int GetForkHeight(const uint256& hashFork) = 0;
    virtual void ListFork(std::vector<std::pair<uint256, CProfile>>& vFork, bool fAll = false) = 0;
    virtual bool GetForkGenealogy(const uint256& hashFork, std::vector<std::pair<uint256, int>>& vAncestry,
                                  std::vector<std::pair<int, uint256>>& vSubline) = 0;
    virtual bool GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight) = 0;
    virtual int GetBlockCount(const uint256& hashFork) = 0;
    virtual bool GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock) = 0;
    virtual bool GetBlockHash(const uint256& hashFork, int nHeight, std::vector<uint256>& vBlockHash) = 0;
    virtual bool GetBlock(const uint256& hashBlock, CBlock& block, uint256& hashFork, int& nHeight) = 0;
    virtual bool GetBlockEx(const uint256& hashBlock, CBlockEx& block, uint256& hashFork, int& nHeight) = 0;
    virtual void GetTxPool(const uint256& hashFork, std::vector<std::pair<uint256, std::size_t>>& vTxPool) = 0;
    virtual bool GetTransaction(const uint256& txid, CTransaction& tx, uint256& hashFork, int& nHeight) = 0;
    virtual bool SendTransaction(xengine::CNoncePtr spNonce, uint256& hashFork, const CTransaction& tx) = 0;
    virtual bool RemovePendingTx(const uint256& txid) = 0;
    /* Wallet */
    virtual bool HaveKey(const crypto::CPubKey& pubkey) = 0;
    virtual void GetPubKeys(std::set<crypto::CPubKey>& setPubKey) = 0;
    virtual bool GetKeyStatus(const crypto::CPubKey& pubkey, int& nVersion, bool& fLocked, int64& nAutoLockTime) = 0;
    virtual bool MakeNewKey(const crypto::CCryptoString& strPassphrase, crypto::CPubKey& pubkey) = 0;
    virtual bool AddKey(const crypto::CKey& key) = 0;
    virtual bool ImportKey(const std::vector<unsigned char>& vchKey, crypto::CPubKey& pubkey) = 0;
    virtual bool ExportKey(const crypto::CPubKey& pubkey, std::vector<unsigned char>& vchKey) = 0;
    virtual bool EncryptKey(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase,
                            const crypto::CCryptoString& strCurrentPassphrase) = 0;
    virtual bool Lock(const crypto::CPubKey& pubkey) = 0;
    virtual bool Unlock(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase, int64 nTimeout) = 0;
    virtual bool SignSignature(const crypto::CPubKey& pubkey, const uint256& hash, std::vector<unsigned char>& vchSig) = 0;
    virtual bool SignTransaction(CTransaction& tx, bool& fCompleted) = 0;
    virtual bool HaveTemplate(const CTemplateId& tid) = 0;
    virtual void GetTemplateIds(std::set<CTemplateId>& setTid) = 0;
    virtual bool AddTemplate(CTemplatePtr& ptr) = 0;
    virtual CTemplatePtr GetTemplate(const CTemplateId& tid) = 0;
    virtual bool GetBalance(const CDestination& dest, const uint256& hashFork, CWalletBalance& balance) = 0;
    virtual bool ListWalletTx(int nOffset, int nCount, std::vector<CWalletTx>& vWalletTx) = 0;
    virtual bool CreateTransaction(const uint256& hashFork, const CDestination& destFrom,
                                   const CDestination& destSendTo, int64 nAmount, int64 nTxFee,
                                   const std::vector<unsigned char>& vchData, CTransaction& txNew) = 0;
    virtual bool SynchronizeWalletTx(const CDestination& destNew) = 0;
    virtual bool ResynchronizeWalletTx() = 0;
    /* Mint */
    virtual Errno SubmitWork(const std::vector<unsigned char>& vchWorkData, CTemplateMintPtr& templMint, crypto::CKey& keyMint, CBlock& block) = 0;
    virtual bool GetWork(const uint256& hashBlockPrev, std::vector<unsigned char>& vchWorkData, int& nPrevBlockHeight, uint256& hashPrev, uint32& nPrevTime, int& nAlgo, int& nBits, CTemplateMintPtr& templMint) = 0;
    virtual bool SendBlock(xengine::CNoncePtr spNonce, const uint256& hashFork, const uint256 blockHash, const CBlock& block) = 0;

protected:
    /* Notify */
    virtual void NotifyWorldLineUpdate(const CWorldLineUpdate& update) = 0;
    virtual void NotifyNetworkPeerUpdate(const CNetworkPeerUpdate& update) = 0;
    virtual void NotifyTransactionUpdate(const CTransactionUpdate& update) = 0;
};

class IServiceController : public xengine::CIOActor
{
public:
    IServiceController()
      : CIOActor("servicecontroller"), pService(nullptr) {}

protected:
    void NotifyWorldLineUpdate(const CWorldLineUpdate& update)
    {
        pService->NotifyWorldLineUpdate(update);
    }

    void NotifyNetworkPeerUpdate(const CNetworkPeerUpdate& update)
    {
        pService->NotifyNetworkPeerUpdate(update);
    }

    void NotifyTransactionUpdate(const CTransactionUpdate& update)
    {
        pService->NotifyTransactionUpdate(update);
    }

protected:
    IService* pService;
};

class IDataStat : public xengine::IIOModule
{
public:
    IDataStat()
      : IIOModule("datastat") {}
    virtual bool AddBlockMakerStatData(const uint256& hashFork, bool fPOW, uint64 nTxCountIn) = 0;
    virtual bool AddP2pSynRecvStatData(const uint256& hashFork, uint64 nBlockCountIn, uint64 nTxCountIn) = 0;
    virtual bool AddP2pSynSendStatData(const uint256& hashFork, uint64 nBlockCountIn, uint64 nTxCountIn) = 0;
    virtual bool AddP2pSynTxSynStatData(const uint256& hashFork, bool fRecv) = 0;
    virtual bool GetBlockMakerStatData(const uint256& hashFork, uint32 nBeginTime, uint32 nGetCount, std::vector<CStatItemBlockMaker>& vStatData) = 0;
    virtual bool GetP2pSynStatData(const uint256& hashFork, uint32 nBeginTime, uint32 nGetCount, std::vector<CStatItemP2pSyn>& vStatData) = 0;
};

} // namespace bigbang

#endif //BIGBANG_BASE_H
