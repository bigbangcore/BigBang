// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_WALLET_H
#define BIGBANG_WALLET_H

#include <boost/thread/thread.hpp>

#include "base.h"
#include "util.h"
#include "walletdb.h"
#include "wallettx.h"

namespace bigbang
{

using namespace xengine;

class CWalletCoins
{
public:
    CWalletCoins()
      : nTotalValue(0) {}
    void Push(const CWalletTxOut& out)
    {
        if (!out.IsNull())
        {
            if (setCoins.insert(out).second)
            {
                nTotalValue += out.GetAmount();
                out.AddRef();
                StdTrace("CWalletCoins", "Push: insert success, TotalValue: %ld, RefCount: %d, utxo: [%d] %s",
                         nTotalValue, out.spWalletTx->GetRefCount(), out.n, out.spWalletTx->txid.GetHex().c_str());
            }
            else
            {
                StdTrace("CWalletCoins", "Push: insert fail, TotalValue: %ld, RefCount: %d, utxo: [%d] %s",
                         nTotalValue, out.spWalletTx->GetRefCount(), out.n, out.spWalletTx->txid.GetHex().c_str());
            }
        }
        else
        {
            StdTrace("CWalletCoins", "Push: out is null, utxo: [%d] %s", out.n, out.spWalletTx->txid.GetHex().c_str());
        }
    }
    void Pop(const CWalletTxOut& out)
    {
        if (!out.IsNull())
        {
            if (setCoins.erase(out))
            {
                nTotalValue -= out.GetAmount();
                out.Release();
                StdTrace("CWalletCoins", "Pop: erase success, TotalValue: %ld, RefCount: %d, utxo: [%d] %s",
                         nTotalValue, out.spWalletTx->GetRefCount(), out.n, out.spWalletTx->txid.GetHex().c_str());
            }
            else
            {
                StdTrace("CWalletCoins", "Pop: erase fail, TotalValue: %ld, RefCount: %d, utxo: [%d] %s",
                         nTotalValue, out.spWalletTx->GetRefCount(), out.n, out.spWalletTx->txid.GetHex().c_str());
            }
        }
        else
        {
            StdTrace("CWalletCoins", "Pop: out is null, utxo: [%d] %s", out.n, out.spWalletTx->txid.GetHex().c_str());
        }
    }

public:
    int64 nTotalValue;
    std::set<CWalletTxOut> setCoins;
};

class CWalletUnspent
{
public:
    void Clear()
    {
        mapWalletCoins.clear();
    }
    void Push(const uint256& hashFork, std::shared_ptr<CWalletTx>& spWalletTx, int n)
    {
        mapWalletCoins[hashFork].Push(CWalletTxOut(spWalletTx, n));
    }
    void Pop(const uint256& hashFork, std::shared_ptr<CWalletTx>& spWalletTx, int n)
    {
        mapWalletCoins[hashFork].Pop(CWalletTxOut(spWalletTx, n));
    }
    CWalletCoins& GetCoins(const uint256& hashFork)
    {
        return mapWalletCoins[hashFork];
    }
    void Dup(const uint256& hashFrom, const uint256& hashTo)
    {
        std::map<uint256, CWalletCoins>::iterator it = mapWalletCoins.find(hashFrom);
        if (it != mapWalletCoins.end())
        {
            CWalletCoins& coin = mapWalletCoins[hashTo];
            for (const CWalletTxOut& out : (*it).second.setCoins)
            {
                coin.Push(out);
            }
        }
    }

public:
    std::map<uint256, CWalletCoins> mapWalletCoins;
};

class CWalletKeyStore
{
public:
    CWalletKeyStore()
      : nTimerId(0), nAutoLockTime(-1), nAutoDelayTime(0) {}
    CWalletKeyStore(const crypto::CKey& keyIn)
      : key(keyIn), nTimerId(0), nAutoLockTime(-1), nAutoDelayTime(0) {}
    virtual ~CWalletKeyStore() {}

public:
    crypto::CKey key;
    uint32 nTimerId;
    int64 nAutoLockTime;
    int64 nAutoDelayTime;
};

class CWalletFork
{
public:
    CWalletFork(const uint256& hashParentIn = uint64(0), int nOriginHeightIn = -1, bool fIsolatedIn = true)
      : hashParent(hashParentIn), nOriginHeight(nOriginHeightIn), fIsolated(fIsolatedIn)
    {
    }
    void InsertSubline(int nHeight, const uint256& hashSubline)
    {
        mapSubline.insert(std::make_pair(nHeight, hashSubline));
    }

public:
    uint256 hashParent;
    int nOriginHeight;
    bool fIsolated;
    std::multimap<int, uint256> mapSubline;
};

class CWallet : public IWallet
{
public:
    CWallet();
    ~CWallet();
    bool IsMine(const CDestination& dest);
    /* Key store */
    bool AddKey(const crypto::CKey& key) override;
    bool LoadKey(const crypto::CKey& key);
    void GetPubKeys(std::set<crypto::CPubKey>& setPubKey) const override;
    bool Have(const crypto::CPubKey& pubkey, const int32 nVersion = -1) const override;
    bool Export(const crypto::CPubKey& pubkey, std::vector<unsigned char>& vchKey) const override;
    bool Import(const std::vector<unsigned char>& vchKey, crypto::CPubKey& pubkey) override;

    bool Encrypt(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase,
                 const crypto::CCryptoString& strCurrentPassphrase) override;
    bool GetKeyStatus(const crypto::CPubKey& pubkey, int& nVersion, bool& fLocked, int64& nAutoLockTime, bool& fPublic) const override;
    bool IsLocked(const crypto::CPubKey& pubkey) const override;
    bool Lock(const crypto::CPubKey& pubkey) override;
    bool Unlock(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase, int64 nTimeout) override;
    void AutoLock(uint32 nTimerId, const crypto::CPubKey& pubkey);
    bool Sign(const crypto::CPubKey& pubkey, const uint256& hash, std::vector<uint8>& vchSig) override;
    /* Template */
    bool LoadTemplate(CTemplatePtr ptr);
    void GetTemplateIds(std::set<CTemplateId>& setTemplateId) const override;
    bool Have(const CTemplateId& tid) const override;
    bool AddTemplate(CTemplatePtr& ptr) override;
    CTemplatePtr GetTemplate(const CTemplateId& tid) const override;
    /* Destination */
    void GetDestinations(std::set<CDestination>& setDest);
    /* Wallet Tx */
    std::size_t GetTxCount() override;
    bool ListTx(int nOffset, int nCount, std::vector<CWalletTx>& vWalletTx) override;
    bool GetBalance(const CDestination& dest, const uint256& hashFork, int nForkHeight, CWalletBalance& balance) override;
    bool SignTransaction(const CDestination& destIn, CTransaction& tx, bool& fCompleted) override;
    bool ArrangeInputs(const CDestination& destIn, const uint256& hashFork, int nForkHeight, CTransaction& tx) override;
    bool ListForkUnspent(const uint256& hashFork, const CDestination& dest, uint32 nMax, std::vector<CTxUnspent>& vUnspent) override;
    /* Update */
    bool SynchronizeTxSet(const CTxSetChange& change) override;
    bool AddNewTx(const uint256& hashFork, const CAssembledTx& tx) override;
    bool UpdateTx(const uint256& hashFork, const CAssembledTx& tx);
    bool LoadTxUnspent(const CWalletTx& wtx);
    bool LoadTxSpent(const CWalletTx& wtx);
    bool AddNewFork(const uint256& hashFork, const uint256& hashParent, int nOriginHeight) override;
    /* Resync */
    bool SynchronizeWalletTx(const CDestination& destNew) override;
    bool ResynchronizeWalletTx() override;
    bool CompareWithTxOrPool(const CAssembledTx& tx);
    bool CompareWithPoolOrTx(const CWalletTx& wtx, const std::set<CDestination>& setAddr);

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;
    bool LoadDB();
    void Clear();
    bool ClearTx();
    bool InsertKey(const crypto::CKey& key);
    int64 SelectCoins(const CDestination& dest, const uint256& hashFork, int nForkHeight, int64 nTxTime,
                      int64 nTargetValue, std::size_t nMaxInput, std::vector<CTxOutPoint>& vCoins);

    std::shared_ptr<CWalletTx> LoadWalletTx(const uint256& txid);
    std::shared_ptr<CWalletTx> InsertWalletTx(const uint256& txid, const CAssembledTx& tx, const uint256& hashFork, bool fIsMine, bool fFromMe);
    bool SignPubKey(const crypto::CPubKey& pubkey, const uint256& hash, std::vector<uint8>& vchSig);
    bool SignMultiPubKey(const std::set<crypto::CPubKey>& setPubKey, const uint256& seed, const uint256& hash, std::vector<uint8>& vchSig);
    bool SignDestination(const CDestination& destIn, const CTransaction& tx, const uint256& hash, std::vector<uint8>& vchSig, bool& fCompleted);
    void UpdateAutoLock(CWalletKeyStore& keystore);
    bool UpdateFork();
    void GetWalletTxFork(const uint256& hashFork, int nHeight, std::vector<uint256>& vFork);
    void AddNewWalletTx(std::shared_ptr<CWalletTx>& spWalletTx, std::vector<uint256>& vFork);
    void RemoveWalletTx(std::shared_ptr<CWalletTx>& spWalletTx, const uint256& hashFork);
    bool SyncWalletTx(CTxFilter& txFilter);
    bool InspectWalletTx(int nCheckDepth);

protected:
    storage::CWalletDB dbWallet;
    ICoreProtocol* pCoreProtocol;
    IBlockChain* pBlockChain;
    ITxPool* pTxPool;
    mutable boost::shared_mutex rwKeyStore;
    mutable boost::shared_mutex rwWalletTx;
    std::map<crypto::CPubKey, CWalletKeyStore> mapKeyStore;
    std::map<CTemplateId, CTemplatePtr> mapTemplatePtr;
    std::map<uint256, std::shared_ptr<CWalletTx>> mapWalletTx;
    std::map<CDestination, CWalletUnspent> mapWalletUnspent;
    std::map<uint256, CWalletFork> mapFork;
};

// dummy wallet for on wallet server
class CDummyWallet : public IWallet
{
public:
    CDummyWallet() {}
    ~CDummyWallet() {}
    /* Key store */
    virtual bool AddKey(const crypto::CKey& key) override
    {
        return false;
    }
    virtual void GetPubKeys(std::set<crypto::CPubKey>& setPubKey) const override {}
    virtual bool Have(const crypto::CPubKey& pubkey, const int32 nVersion = -1) const override
    {
        return false;
    }
    virtual bool Export(const crypto::CPubKey& pubkey,
                        std::vector<unsigned char>& vchKey) const override
    {
        return false;
    }
    virtual bool Import(const std::vector<unsigned char>& vchKey,
                        crypto::CPubKey& pubkey) override
    {
        return false;
    }
    virtual bool Encrypt(
        const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase,
        const crypto::CCryptoString& strCurrentPassphrase) override
    {
        return false;
    }
    virtual bool GetKeyStatus(const crypto::CPubKey& pubkey, int& nVersion,
                              bool& fLocked, int64& nAutoLockTime, bool& fPublic) const override
    {
        return false;
    }
    virtual bool IsLocked(const crypto::CPubKey& pubkey) const override
    {
        return false;
    }
    virtual bool Lock(const crypto::CPubKey& pubkey) override
    {
        return false;
    }
    virtual bool Unlock(const crypto::CPubKey& pubkey,
                        const crypto::CCryptoString& strPassphrase,
                        int64 nTimeout) override
    {
        return false;
    }
    virtual bool Sign(const crypto::CPubKey& pubkey, const uint256& hash,
                      std::vector<uint8>& vchSig) override
    {
        return false;
    }
    /* Template */
    virtual void GetTemplateIds(std::set<CTemplateId>& setTemplateId) const override {}
    virtual bool Have(const CTemplateId& tid) const override
    {
        return false;
    }
    virtual bool AddTemplate(CTemplatePtr& ptr) override
    {
        return false;
    }
    virtual CTemplatePtr GetTemplate(const CTemplateId& tid) const override
    {
        return nullptr;
    }
    /* Wallet Tx */
    virtual std::size_t GetTxCount() override
    {
        return 0;
    }
    virtual bool ListTx(int nOffset, int nCount, std::vector<CWalletTx>& vWalletTx) override
    {
        return true;
    }
    virtual bool GetBalance(const CDestination& dest, const uint256& hashFork,
                            int nForkHeight, CWalletBalance& balance) override
    {
        return false;
    }
    virtual bool SignTransaction(const CDestination& destIn, CTransaction& tx,
                                 bool& fCompleted) override
    {
        return false;
    }
    virtual bool ArrangeInputs(const CDestination& destIn,
                               const uint256& hashFork, int nForkHeight,
                               CTransaction& tx) override
    {
        return false;
    }
    virtual bool ListForkUnspent(const uint256& hashFork, const CDestination& dest,
                                 uint32 nMax, std::vector<CTxUnspent>& vUnspent) override
    {
        return false;
    }
    /* Update */
    virtual bool SynchronizeTxSet(const CTxSetChange& change) override
    {
        return true;
    }
    virtual bool AddNewTx(const uint256& hashFork, const CAssembledTx& tx) override
    {
        return true;
    }
    virtual bool AddNewFork(const uint256& hashFork, const uint256& hashParent,
                            int nOriginHeight) override
    {
        return true;
    }

    virtual bool SynchronizeWalletTx(const CDestination& destNew) override
    {
        return true;
    }

    virtual bool ResynchronizeWalletTx() override
    {
        return true;
    }
};

} // namespace bigbang

#endif //BIGBANG_WALLET_H
