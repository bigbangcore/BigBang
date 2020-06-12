// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_BASE_H
#define BIGBANG_BASE_H

#include <boost/optional.hpp>
#include <map>
#include <xengine.h>

#include "address.h"
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
    virtual const uint256& GetGenesisBlockHash() = 0;
    virtual void GetGenesisBlock(CBlock& block) = 0;
    virtual Errno ValidateTransaction(const CTransaction& tx, int nHeight) = 0;
    virtual Errno ValidateBlock(const CBlock& block) = 0;
    virtual Errno ValidateOrigin(const CBlock& block, const CProfile& parentProfile, CProfile& forkProfile) = 0;
    virtual Errno VerifyProofOfWork(const CBlock& block, const CBlockIndex* pIndexPrev) = 0;
    virtual Errno VerifyDelegatedProofOfStake(const CBlock& block, const CBlockIndex* pIndexPrev,
                                              const CDelegateAgreement& agreement)
        = 0;
    virtual Errno VerifySubsidiary(const CBlock& block, const CBlockIndex* pIndexPrev, const CBlockIndex* pIndexRef,
                                   const CDelegateAgreement& agreement)
        = 0;
    virtual Errno VerifyBlock(const CBlock& block, CBlockIndex* pIndexPrev) = 0;
    virtual Errno VerifyBlockTx(const CTransaction& tx, const CTxContxt& txContxt, CBlockIndex* pIndexPrev, const int nForkHeight,
                                const uint256& hashFork, const CForkSetManager& forkSetMgr, CForkSetManager& unconfirmedForkSetMgr)
        = 0;
    virtual Errno VerifyTransaction(const CTransaction& tx, const std::vector<CTxOut>& vPrevOutput, const int nForkHeight,
                                    const uint256& hashFork, const CForkSetManager& forkSetMgr, CForkSetManager& unconfirmedForkSetMgr)
        = 0;
    virtual bool GetBlockTrust(const CBlock& block, uint256& nChainTrust, const CBlockIndex* pIndexPrev = nullptr, const CDelegateAgreement& agreement = CDelegateAgreement(), const CBlockIndex* pIndexRef = nullptr, std::size_t nEnrollTrust = 0) = 0;
    virtual bool GetProofOfWorkTarget(const CBlockIndex* pIndexPrev, int nAlgo, int& nBits, int64& nReward) = 0;
    virtual bool IsDposHeight(int height) = 0;
    virtual int64 GetPrimaryMintWorkReward(const CBlockIndex* pIndexPrev) = 0;
    virtual void GetDelegatedBallot(const uint256& nAgreement, std::size_t nWeight, const std::map<CDestination, size_t>& mapBallot,
                                    const std::vector<std::pair<CDestination, int64>>& vecAmount, int64 nMoneySupply, std::vector<CDestination>& vBallot, std::size_t& nEnrollTrust, int nBlockHeight)
        = 0;
    virtual int64 MinEnrollAmount() = 0;
    virtual uint32 DPoSTimestamp(const CBlockIndex* pIndexPrev) = 0;
    virtual uint32 GetNextBlockTimeStamp(uint16 nPrevMintType, uint32 nPrevTimeStamp, uint16 nTargetMintType, int nTargetHeight) = 0;
};

class IBlockChain : public xengine::IBase
{
public:
    class CCheckPoint
    {
    public:
        CCheckPoint()
          : nHeight(-1)
        {
        }
        CCheckPoint(int nHeightIn, const uint256& nBlockHashIn)
          : nHeight(nHeightIn), nBlockHash(nBlockHashIn)
        {
        }
        CCheckPoint(const CCheckPoint& point)
          : nHeight(point.nHeight), nBlockHash(point.nBlockHash)
        {
        }
        CCheckPoint& operator=(const CCheckPoint& point)
        {
            nHeight = point.nHeight;
            nBlockHash = point.nBlockHash;
            return *this;
        }

    public:
        int nHeight;
        uint256 nBlockHash;
    };

public:
    IBlockChain()
      : IBase("blockchain") {}
    virtual bool IsForkActive(const uint256& hashFork) = 0;
    virtual void GetForkStatus(std::map<uint256, CForkStatus>& mapForkStatus) = 0;
    virtual bool GetForkProfile(const uint256& hashFork, CProfile& profile) = 0;
    virtual bool GetForkContext(const uint256& hashFork, CForkContext& ctxt) = 0;
    virtual bool GetForkAncestry(const uint256& hashFork, std::vector<std::pair<uint256, uint256>> vAncestry) = 0;
    virtual int GetBlockCount(const uint256& hashFork) = 0;
    virtual bool GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight) = 0;
    virtual bool GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight, uint256& hashNext) = 0;
    virtual bool GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock) = 0;
    virtual bool GetBlockHash(const uint256& hashFork, int nHeight, std::vector<uint256>& vBlockHash) = 0;
    virtual bool GetLastBlockOfHeight(const uint256& hashFork, const int nHeight, uint256& hashBlock, int64& nTime) = 0;
    virtual bool GetLastBlock(const uint256& hashFork, uint256& hashBlock, int& nHeight, int64& nTime, uint16& nMintType) = 0;
    virtual bool GetLastBlockTime(const uint256& hashFork, int nDepth, std::vector<int64>& vTime) = 0;
    virtual bool GetBlock(const uint256& hashBlock, CBlock& block) = 0;
    virtual bool GetBlockEx(const uint256& hashBlock, CBlockEx& block) = 0;
    virtual bool GetOrigin(const uint256& hashFork, CBlock& block) = 0;
    virtual bool Exists(const uint256& hashBlock) = 0;
    virtual bool GetTransaction(const uint256& txid, CTransaction& tx) = 0;
    virtual bool GetTransaction(const uint256& txid, CTransaction& tx, uint256& hashFork, int& nHeight) = 0;
    virtual bool GetTxLocation(const uint256& txid, uint256& hashFork, int& nHeight) = 0;
    virtual bool GetTxUnspent(const uint256& hashFork, const std::vector<CTxIn>& vInput,
                              std::vector<CTxOut>& vOutput)
        = 0;
    virtual bool ExistsTx(const uint256& txid) = 0;
    virtual bool FilterTx(const uint256& hashFork, CTxFilter& filter) = 0;
    virtual bool FilterTx(const uint256& hashFork, int nDepth, CTxFilter& filter) = 0;
    virtual bool ListForkContext(std::vector<std::pair<CForkContext, bool>>& vForkCtxt) = 0;
    virtual bool AddNewForkContext(const CForkContext& ctxt) = 0;
    virtual bool InactivateFork(const uint256& hashFork) = 0;
    virtual Errno AddNewBlock(const CBlock& block, CBlockChainUpdate& update, const CForkSetManager& forkSetMgr) = 0;
    virtual Errno AddNewOrigin(const CBlock& block, CBlockChainUpdate& update) = 0;
    virtual bool GetProofOfWorkTarget(const uint256& hashPrev, int nAlgo, int& nBits, int64& nReward) = 0;
    virtual bool GetBlockMintReward(const uint256& hashPrev, int64& nReward) = 0;
    virtual bool GetBlockLocator(const uint256& hashFork, CBlockLocator& locator, uint256& hashDepth, int nIncStep) = 0;
    virtual bool GetBlockInv(const uint256& hashFork, const CBlockLocator& locator, std::vector<uint256>& vBlockHash, std::size_t nMaxCount) = 0;
    virtual bool ListForkUnspent(const uint256& hashFork, const CDestination& dest, uint32 nMax, std::vector<CTxUnspent>& vUnspent) = 0;

    /////////////    CheckPoints    /////////////////////
    virtual bool HasCheckPoints() const = 0;
    virtual bool GetCheckPointByHeight(int nHeight, CCheckPoint& point) = 0;
    virtual std::vector<CCheckPoint> CheckPoints() const = 0;
    virtual CCheckPoint LatestCheckPoint() const = 0;
    virtual bool VerifyCheckPoint(int nHeight, const uint256& nBlockHash) = 0;
    virtual bool FindPreviousCheckPointBlock(CBlock& block) = 0;

    virtual bool ListForkUnspentBatch(const uint256& hashFork, uint32 nMax, std::map<CDestination, std::vector<CTxUnspent>>& mapUnspent) = 0;
    virtual bool GetVotes(const CDestination& destDelegate, int64& nVotes) = 0;
    virtual bool ListDelegate(uint32 nCount, std::multimap<int64, CDestination>& mapVotes) = 0;
    virtual bool VerifyRepeatBlock(const uint256& hashFork, const CBlock& block, const uint256& hashBlockRef) = 0;
    virtual bool GetBlockDelegateVote(const uint256& hashBlock, std::map<CDestination, int64>& mapVote) = 0;
    virtual int64 GetDelegateMinEnrollAmount(const uint256& hashBlock) = 0;
    virtual bool GetDelegateCertTxCount(const uint256& hashLastBlock, std::map<CDestination, int>& mapVoteCert) = 0;
    virtual bool GetBlockDelegateEnrolled(const uint256& hashBlock, CDelegateEnrolled& enrolled) = 0;
    virtual int64 GetBlockMoneySupply(const uint256& hashBlock) = 0;
    virtual bool ListDelegatePayment(uint32 height, CBlock& block, std::multimap<int64, CDestination>& mapVotes) = 0;
    virtual uint32 DPoSTimestamp(const uint256& hashPrev) = 0;
    virtual Errno VerifyPowBlock(const CBlock& block, bool& fLongChain, const CForkSetManager& forkSetMgr) = 0;

    const CBasicConfig* Config()
    {
        return dynamic_cast<const CBasicConfig*>(xengine::IBase::Config());
    }
    const CStorageConfig* StorageConfig()
    {
        return dynamic_cast<const CStorageConfig*>(xengine::IBase::Config());
    }
};

class ITxPool : public xengine::IBase
{
public:
    ITxPool()
      : IBase("txpool") {}
    virtual bool Exists(const uint256& txid) = 0;
    virtual void Clear() = 0;
    virtual std::size_t Count(const uint256& fork) const = 0;
    virtual Errno Push(const CTransaction& tx, uint256& hashFork, CDestination& destIn, int64& nValueIn, const CForkSetManager& forkSetMgr) = 0;
    virtual void Pop(const uint256& txid) = 0;
    virtual bool Get(const uint256& txid, CTransaction& tx) const = 0;
    virtual bool Get(const uint256& txid, CAssembledTx& tx) const = 0;
    virtual void ListTx(const uint256& hashFork, std::vector<std::pair<uint256, std::size_t>>& vTxPool) = 0;
    virtual void ListTx(const uint256& hashFork, std::vector<uint256>& vTxPool) = 0;
    virtual bool ListForkUnspent(const uint256& hashFork, const CDestination& dest, uint32 nMax, const std::vector<CTxUnspent>& vUnpsentOnChain, std::vector<CTxUnspent>& vUnspent) = 0;
    virtual bool ListForkUnspentBatch(const uint256& hashFork, uint32 nMax, const std::map<CDestination, std::vector<CTxUnspent>>& mapUnspentOnChain, std::map<CDestination, std::vector<CTxUnspent>>& mapUnspent) = 0;
    virtual bool FilterTx(const uint256& hashFork, CTxFilter& filter) = 0;
    virtual bool ArrangeBlockTx(const uint256& hashFork, const uint256& hashPrev, int64 nBlockTime, std::size_t nMaxSize,
                                std::vector<CTransaction>& vtx, int64& nTotalTxFee)
        = 0;
    virtual bool FetchInputs(const uint256& hashFork, const CTransaction& tx, std::vector<CTxOut>& vUnspent) = 0;
    virtual bool SynchronizeBlockChain(const CBlockChainUpdate& update, CTxSetChange& change, const CForkSetManager& forkSetMgr) = 0;
    virtual void AddDestDelegate(const CDestination& destDeleage) = 0;
    const CStorageConfig* StorageConfig()
    {
        return dynamic_cast<const CStorageConfig*>(xengine::IBase::Config());
    }
};

class IForkManager : public xengine::IBase
{
public:
    IForkManager()
      : IBase("forkmanager") {}
    virtual bool IsAllowed(const uint256& hashFork) const = 0;
    virtual bool GetJoint(const uint256& hashFork, uint256& hashParent, uint256& hashJoint, int32& nJointHeight) const = 0;
    virtual bool LoadForkContext(std::vector<uint256>& vActive) = 0;
    virtual void ForkUpdate(const CBlockChainUpdate& update, std::vector<uint256>& vActive, std::vector<uint256>& vDeactive) = 0;
    virtual void GetForkList(std::vector<uint256>& vFork) const = 0;
    virtual bool GetSubline(const uint256& hashFork, std::vector<std::pair<int32, uint256>>& vSubline) const = 0;
    virtual bool GetCreatedHeight(const uint256& hashFork, int32& nCreatedHeight) const = 0;
    virtual const CForkSetManager& GetForkSetManager() const = 0;
    const CForkConfig* ForkConfig()
    {
        return dynamic_cast<const CForkConfig*>(xengine::IBase::Config());
    }
};

class IConsensus : public xengine::IBase
{
public:
    IConsensus()
      : IBase("consensus") {}
    const CMintConfig* MintConfig()
    {
        return dynamic_cast<const CMintConfig*>(xengine::IBase::Config());
    }
    virtual void PrimaryUpdate(const CBlockChainUpdate& update, const CTxSetChange& change, CDelegateRoutine& routine) = 0;
    virtual void AddNewTx(const CAssembledTx& tx) = 0;
    virtual bool AddNewDistribute(const uint256& hashDistributeAnchor, const CDestination& destFrom, const std::vector<unsigned char>& vchDistribute) = 0;
    virtual bool AddNewPublish(const uint256& hashDistributeAnchor, const CDestination& destFrom, const std::vector<unsigned char>& vchPublish) = 0;
    virtual void GetAgreement(int nTargetHeight, uint256& nAgreement, std::size_t& nWeight, std::vector<CDestination>& vBallot) = 0;
    virtual void GetProof(int nTargetHeight, std::vector<unsigned char>& vchProof) = 0;
    virtual bool GetNextConsensus(CAgreementBlock& consParam) = 0;
    virtual bool LoadConsensusData(int& nStartHeight, CDelegateRoutine& routine) = 0;
};

class IBlockMaker : public xengine::CEventProc
{
public:
    IBlockMaker()
      : CEventProc("blockmaker") {}
    const CMintConfig* MintConfig()
    {
        return dynamic_cast<const CMintConfig*>(xengine::IBase::Config());
    }
};

class IWallet : public xengine::IBase
{
public:
    IWallet()
      : IBase("wallet") {}
    /* Key store */
    virtual boost::optional<std::string> AddKey(const crypto::CKey& key) = 0;
    virtual void GetPubKeys(std::set<crypto::CPubKey>& setPubKey) const = 0;
    virtual bool Have(const crypto::CPubKey& pubkey, const int32 nVersion = -1) const = 0;
    virtual bool Export(const crypto::CPubKey& pubkey, std::vector<unsigned char>& vchKey) const = 0;
    virtual bool Import(const std::vector<unsigned char>& vchKey, crypto::CPubKey& pubkey) = 0;
    virtual bool Encrypt(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase,
                         const crypto::CCryptoString& strCurrentPassphrase)
        = 0;
    virtual bool GetKeyStatus(const crypto::CPubKey& pubkey, int& nVersion, bool& fLocked, int64& nAutoLockTime, bool& fPublic) const = 0;
    virtual bool IsLocked(const crypto::CPubKey& pubkey) const = 0;
    virtual bool Lock(const crypto::CPubKey& pubkey) = 0;
    virtual bool Unlock(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase, int64 nTimeout) = 0;
    virtual bool Sign(const crypto::CPubKey& pubkey, const uint256& hash, std::vector<uint8>& vchSig) = 0;
    /* Template */
    virtual void GetTemplateIds(std::set<CTemplateId>& setTemplateId) const = 0;
    virtual bool Have(const CTemplateId& tid) const = 0;
    virtual bool AddTemplate(CTemplatePtr& ptr) = 0;
    virtual CTemplatePtr GetTemplate(const CTemplateId& tid) const = 0;
    /* Wallet Tx */
    virtual std::size_t GetTxCount() = 0;
    virtual bool ListTx(const uint256& hashFork, const CDestination& dest, int nOffset, int nCount, std::vector<CWalletTx>& vWalletTx) = 0;
    virtual bool GetBalance(const CDestination& dest, const uint256& hashFork, int nForkHeight, CWalletBalance& balance) = 0;
    virtual bool SignTransaction(const CDestination& destIn, CTransaction& tx, const vector<uint8>& vchSendToData, const int32 nForkHeight, bool& fCompleted) = 0;
    virtual bool ArrangeInputs(const CDestination& destIn, const uint256& hashFork, int nForkHeight, CTransaction& tx) = 0;
    virtual bool ListForkUnspent(const uint256& hashFork, const CDestination& dest, uint32 nMax, std::vector<CTxUnspent>& vUnspent) = 0;
    /* Update */
    virtual bool SynchronizeTxSet(const CTxSetChange& change) = 0;
    virtual bool AddNewTx(const uint256& hashFork, const CAssembledTx& tx) = 0;
    virtual bool AddNewFork(const uint256& hashFork, const uint256& hashParent, int nOriginHeight) = 0;
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
};

class IDispatcher : public xengine::IBase
{
public:
    IDispatcher()
      : IBase("dispatcher") {}
    virtual Errno AddNewBlock(const CBlock& block, uint64 nNonce = 0) = 0;
    virtual Errno AddNewTx(const CTransaction& tx, uint64 nNonce = 0) = 0;
    virtual bool AddNewDistribute(const uint256& hashAnchor, const CDestination& dest,
                                  const std::vector<unsigned char>& vchDistribute)
        = 0;
    virtual bool AddNewPublish(const uint256& hashAnchor, const CDestination& dest,
                               const std::vector<unsigned char>& vchPublish)
        = 0;
    virtual void SetConsensus(const CAgreementBlock& agreeBlock) = 0;
};

class IService : public xengine::IBase
{
public:
    IService()
      : IBase("service") {}
    /* Notify */
    virtual void NotifyBlockChainUpdate(const CBlockChainUpdate& update) = 0;
    virtual void NotifyNetworkPeerUpdate(const CNetworkPeerUpdate& update) = 0;
    virtual void NotifyTransactionUpdate(const CTransactionUpdate& update) = 0;
    /* System */
    virtual void Stop() = 0;
    /* Network */
    virtual int GetPeerCount() = 0;
    virtual void GetPeers(std::vector<network::CBbPeerInfo>& vPeerInfo) = 0;
    virtual bool AddNode(const xengine::CNetHost& node) = 0;
    virtual bool RemoveNode(const xengine::CNetHost& node) = 0;
    /* Blockchain & Tx Pool*/
    virtual int GetForkCount() = 0;
    virtual bool HaveFork(const uint256& hashFork) = 0;
    virtual int GetForkHeight(const uint256& hashFork) = 0;
    virtual void ListFork(std::vector<std::pair<uint256, CProfile>>& vFork, bool fAll = false) = 0;
    virtual bool GetForkGenealogy(const uint256& hashFork, std::vector<std::pair<uint256, int>>& vAncestry,
                                  std::vector<std::pair<int, uint256>>& vSubline)
        = 0;
    virtual bool GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight) = 0;
    virtual int GetBlockCount(const uint256& hashFork) = 0;
    virtual bool GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock) = 0;
    virtual bool GetBlockHash(const uint256& hashFork, int nHeight, std::vector<uint256>& vBlockHash) = 0;
    virtual bool GetBlock(const uint256& hashBlock, CBlock& block, uint256& hashFork, int& nHeight) = 0;
    virtual bool GetBlockEx(const uint256& hashBlock, CBlockEx& block, uint256& hashFork, int& nHeight) = 0;
    virtual bool GetLastBlockOfHeight(const uint256& hashFork, const int nHeight, uint256& hashBlock, int64& nTime) = 0;
    virtual void GetTxPool(const uint256& hashFork, std::vector<std::pair<uint256, std::size_t>>& vTxPool) = 0;
    virtual bool GetTransaction(const uint256& txid, CTransaction& tx, uint256& hashFork, int& nHeight, uint256& hashBlock, CDestination& destIn) = 0;
    virtual Errno SendTransaction(CTransaction& tx) = 0;
    virtual bool RemovePendingTx(const uint256& txid) = 0;
    virtual bool ListForkUnspent(const uint256& hashFork, const CDestination& dest, uint32 nMax, std::vector<CTxUnspent>& vUnspent) = 0;
    virtual bool ListForkUnspentBatch(const uint256& hashFork, uint32 nMax, std::map<CDestination, std::vector<CTxUnspent>>& mapUnspent) = 0;
    virtual bool GetVotes(const CDestination& destDelegate, int64& nVotes, string& strFailCause) = 0;
    virtual bool ListDelegate(uint32 nCount, std::multimap<int64, CDestination>& mapVotes) = 0;

    /* Wallet */
    virtual bool HaveKey(const crypto::CPubKey& pubkey, const int32 nVersion = -1) = 0;
    virtual void GetPubKeys(std::set<crypto::CPubKey>& setPubKey) = 0;
    virtual bool GetKeyStatus(const crypto::CPubKey& pubkey, int& nVersion, bool& fLocked, int64& nAutoLockTime, bool& fPublic) = 0;
    virtual boost::optional<std::string> MakeNewKey(const crypto::CCryptoString& strPassphrase, crypto::CPubKey& pubkey) = 0;
    virtual boost::optional<std::string> AddKey(const crypto::CKey& key) = 0;
    virtual bool ImportKey(const std::vector<unsigned char>& vchKey, crypto::CPubKey& pubkey) = 0;
    virtual bool ExportKey(const crypto::CPubKey& pubkey, std::vector<unsigned char>& vchKey) = 0;
    virtual bool EncryptKey(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase,
                            const crypto::CCryptoString& strCurrentPassphrase)
        = 0;
    virtual bool Lock(const crypto::CPubKey& pubkey) = 0;
    virtual bool Unlock(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase, int64 nTimeout) = 0;
    virtual bool SignSignature(const crypto::CPubKey& pubkey, const uint256& hash, std::vector<unsigned char>& vchSig) = 0;
    virtual bool SignTransaction(CTransaction& tx, const vector<uint8>& vchSendToData, bool& fCompleted) = 0;
    virtual bool HaveTemplate(const CTemplateId& tid) = 0;
    virtual void GetTemplateIds(std::set<CTemplateId>& setTid) = 0;
    virtual bool AddTemplate(CTemplatePtr& ptr) = 0;
    virtual CTemplatePtr GetTemplate(const CTemplateId& tid) = 0;
    virtual bool GetBalance(const CDestination& dest, const uint256& hashFork, CWalletBalance& balance) = 0;
    virtual bool ListWalletTx(const uint256& hashFork, const CDestination& dest, int nOffset, int nCount, std::vector<CWalletTx>& vWalletTx) = 0;
    virtual boost::optional<std::string> CreateTransaction(const uint256& hashFork, const CDestination& destFrom,
                                                           const CDestination& destSendTo, int64 nAmount, int64 nTxFee,
                                                           const std::vector<unsigned char>& vchData, CTransaction& txNew)
        = 0;
    virtual bool SynchronizeWalletTx(const CDestination& destNew) = 0;
    virtual bool ResynchronizeWalletTx() = 0;
    virtual bool SignOfflineTransaction(const CDestination& destIn, CTransaction& tx, bool& fCompleted) = 0;
    virtual Errno SendOfflineSignedTransaction(CTransaction& tx) = 0;
    /* Mint */
    virtual bool GetWork(std::vector<unsigned char>& vchWorkData, int& nPrevBlockHeight,
                         uint256& hashPrev, uint32& nPrevTime, int& nAlgo, int& nBits,
                         const CTemplateMintPtr& templMint)
        = 0;
    virtual Errno SubmitWork(const std::vector<unsigned char>& vchWorkData, const CTemplateMintPtr& templMint,
                             crypto::CKey& keyMint, uint256& hashBlock)
        = 0;
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

class IRecovery : public xengine::IBase
{
public:
    IRecovery()
      : IBase("recovery") {}
    const CStorageConfig* StorageConfig()
    {
        return dynamic_cast<const CStorageConfig*>(xengine::IBase::Config());
    }
};

} // namespace bigbang

#endif //BIGBANG_BASE_H
