// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet.h"

#include "template/exchange.h"
using namespace std;
using namespace xengine;

namespace bigbang
{

#define MAX_TXIN_SELECTIONS 128
#define MAX_SIGNATURE_SIZE 2048

//////////////////////////////
// CDBAddressWalker

class CDBAddrWalker : public storage::CWalletDBAddrWalker
{
public:
    CDBAddrWalker(CWallet* pWalletIn)
      : pWallet(pWalletIn) {}
    bool WalkPubkey(const crypto::CPubKey& pubkey, int version, const crypto::CCryptoCipher& cipher) override
    {
        crypto::CKey key;
        key.Load(pubkey, version, cipher);
        return pWallet->LoadKey(key);
    }
    bool WalkTemplate(const CTemplateId& tid, const std::vector<unsigned char>& vchData) override
    {
        CTemplatePtr ptr = CTemplate::CreateTemplatePtr(tid.GetType(), vchData);
        if (ptr)
        {
            return pWallet->LoadTemplate(ptr);
        }
        return false;
    }

protected:
    CWallet* pWallet;
};

//////////////////////////////
// CDBTxWalker

class CDBTxWalker : public storage::CWalletDBTxWalker
{
public:
    CDBTxWalker(CWallet* pWalletIn)
      : pWallet(pWalletIn) {}
    bool Walk(const CWalletTx& wtx) override
    {
        std::shared_ptr<CWalletTx> spWalletTx(new CWalletTx(wtx));
        mapWalkTx.insert(make_pair(wtx.txid, spWalletTx));
        return pWallet->LoadTxUnspent(wtx);
    }
    void LoadSpentTx()
    {
        std::map<uint256, std::shared_ptr<CWalletTx>>::iterator it = mapWalkTx.begin();
        for (; it != mapWalkTx.end(); ++it)
        {
            pWallet->LoadTxSpent(*(*it).second);
        }
        mapWalkTx.clear();
    }

protected:
    CWallet* pWallet;
    std::map<uint256, std::shared_ptr<CWalletTx>> mapWalkTx;
};

//////////////////////////////
// CWalletTxFilter

class CWalletTxFilter : public CTxFilter
{
public:
    CWalletTxFilter(CWallet* pWalletIn, const CDestination& destNew)
      : CTxFilter(destNew), pWallet(pWalletIn)
    {
    }
    CWalletTxFilter(CWallet* pWalletIn, const set<CDestination>& setDestIn)
      : CTxFilter(setDestIn), pWallet(pWalletIn)
    {
    }
    bool FoundTx(const uint256& hashFork, const CAssembledTx& tx) override
    {
        return pWallet->UpdateTx(hashFork, tx);
    }

public:
    CWallet* pWallet;
};

//////////////////////////////
// CInspectWtxFilter

class CInspectWtxFilter : public CTxFilter
{
public:
    CInspectWtxFilter(CWallet* pWalletIn, const CDestination& destNew)
      : CTxFilter(destNew), pWallet(pWalletIn)
    {
    }
    CInspectWtxFilter(CWallet* pWalletIn, const set<CDestination>& setDestIn)
      : CTxFilter(setDestIn), pWallet(pWalletIn)
    {
    }
    bool FoundTx(const uint256& hashFork, const CAssembledTx& tx) override
    {
        return pWallet->CompareWithTxOrPool(tx);
    }

public:
    CWallet* pWallet;
};

//////////////////////////////
// CInspectDBTxWalker

class CInspectDBTxWalker : public storage::CWalletDBTxWalker
{
public:
    CInspectDBTxWalker(CWallet* pWalletIn, const set<CDestination>& setDestIn)
      : fRes(false), pWallet(pWalletIn), setDest(setDestIn) {}
    bool Walk(const CWalletTx& wtx) override
    {
        fRes = pWallet->CompareWithPoolOrTx(wtx, setDest);
        return fRes;
    }
    bool fRes;

protected:
    CWallet* pWallet;
    set<CDestination> setDest;
};

//////////////////////////////
// CWallet

CWallet::CWallet()
{
    pCoreProtocol = nullptr;
    pWorldLineCtrl = nullptr;
    pTxPoolCtrl = nullptr;
}

CWallet::~CWallet()
{
}

bool CWallet::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        ERROR("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("worldlinecontroller", pWorldLineCtrl))
    {
        ERROR("Failed to request worldline");
        return false;
    }

    if (!GetObject("txpoolcontroller", pTxPoolCtrl))
    {
        ERROR("Failed to request txpool");
        return false;
    }

    return true;
}

void CWallet::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pWorldLineCtrl = nullptr;
    pTxPoolCtrl = nullptr;
}

bool CWallet::HandleInvoke()
{
    if (!dbWallet.Initialize(Config() ? (Config()->pathData / "wallet") : "./"))
    {
        ERROR("Failed to initialize wallet database");
        return false;
    }

    if (!LoadDB())
    {
        ERROR("Failed to load wallet database");
        return false;
    }

    if (!InspectWalletTx(StorageConfig() ? StorageConfig()->nCheckDepth : 1))
    {
        INFO("Failed to inspect wallet transactions");
        return false;
    }

    return true;
}

void CWallet::HandleHalt()
{
    dbWallet.Deinitialize();
    Clear();
}

bool CWallet::IsMine(const CDestination& dest)
{
    crypto::CPubKey pubkey;
    CTemplateId nTemplateId;
    if (dest.GetPubKey(pubkey))
    {
        return mapKeyStore.count(pubkey) != 0;
    }
    else if (dest.GetTemplateId(nTemplateId))
    {
        return mapTemplatePtr.count(nTemplateId) != 0;
    }
    return false;
}

bool CWallet::AddKey(const crypto::CKey& key)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwKeyStore);
    if (!InsertKey(key))
    {
        WARN("AddKey : invalid or duplicated key");
        return false;
    }

    if (!dbWallet.UpdateKey(key.GetPubKey(), key.GetVersion(), key.GetCipher()))
    {
        mapKeyStore.erase(key.GetPubKey());
        WARN("AddKey : failed to save key");
        return false;
    }
    return true;
}

bool CWallet::LoadKey(const crypto::CKey& key)
{
    if (!InsertKey(key))
    {
        ERROR("LoadKey : invalid or duplicated key");
        return false;
    }
    return true;
}

void CWallet::GetPubKeys(set<crypto::CPubKey>& setPubKey) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);

    for (map<crypto::CPubKey, CWalletKeyStore>::const_iterator it = mapKeyStore.begin();
         it != mapKeyStore.end(); ++it)
    {
        setPubKey.insert((*it).first);
    }
}

bool CWallet::Have(const crypto::CPubKey& pubkey) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);
    return mapKeyStore.count(pubkey) != 0;
}

bool CWallet::Export(const crypto::CPubKey& pubkey, vector<unsigned char>& vchKey) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);
    map<crypto::CPubKey, CWalletKeyStore>::const_iterator it = mapKeyStore.find(pubkey);
    if (it != mapKeyStore.end())
    {
        (*it).second.key.Save(vchKey);
        return true;
    }
    return false;
}

bool CWallet::Import(const vector<unsigned char>& vchKey, crypto::CPubKey& pubkey)
{
    crypto::CKey key;
    if (!key.Load(vchKey))
    {
        return false;
    }
    pubkey = key.GetPubKey();
    return AddKey(key);
}

bool CWallet::Encrypt(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase,
                      const crypto::CCryptoString& strCurrentPassphrase)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwKeyStore);
    map<crypto::CPubKey, CWalletKeyStore>::iterator it = mapKeyStore.find(pubkey);
    if (it != mapKeyStore.end())
    {
        crypto::CKey& key = (*it).second.key;
        crypto::CKey keyTemp(key);
        if (!keyTemp.Encrypt(strPassphrase, strCurrentPassphrase))
        {
            return false;
        }
        if (!dbWallet.UpdateKey(key.GetPubKey(), keyTemp.GetVersion(), keyTemp.GetCipher()))
        {
            ERROR("AddKey : failed to update key");
            return false;
        }
        key.Encrypt(strPassphrase, strCurrentPassphrase);
        key.Lock();
        return true;
    }
    return false;
}

bool CWallet::GetKeyStatus(const crypto::CPubKey& pubkey, int& nVersion, bool& fLocked, int64& nAutoLockTime) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);
    map<crypto::CPubKey, CWalletKeyStore>::const_iterator it = mapKeyStore.find(pubkey);
    if (it != mapKeyStore.end())
    {
        const CWalletKeyStore& keystore = (*it).second;
        nVersion = keystore.key.GetVersion();
        fLocked = keystore.key.IsLocked();
        nAutoLockTime = (!fLocked && keystore.nAutoLockTime > 0) ? keystore.nAutoLockTime : 0;
        return true;
    }
    return false;
}

bool CWallet::IsLocked(const crypto::CPubKey& pubkey) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);
    map<crypto::CPubKey, CWalletKeyStore>::const_iterator it = mapKeyStore.find(pubkey);
    if (it != mapKeyStore.end())
    {
        return (*it).second.key.IsLocked();
    }
    return false;
}

bool CWallet::Lock(const crypto::CPubKey& pubkey)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwKeyStore);
    map<crypto::CPubKey, CWalletKeyStore>::iterator it = mapKeyStore.find(pubkey);
    if (it != mapKeyStore.end())
    {
        CWalletKeyStore& keystore = (*it).second;
        keystore.key.Lock();
        if (keystore.nTimerId)
        {
            CancelTimer(keystore.nTimerId);
            keystore.nTimerId = 0;
            keystore.nAutoLockTime = -1;
        }
        return true;
    }
    return false;
}

bool CWallet::Unlock(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase, int64 nTimeout)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwKeyStore);
    map<crypto::CPubKey, CWalletKeyStore>::iterator it = mapKeyStore.find(pubkey);
    if (it != mapKeyStore.end())
    {
        CWalletKeyStore& keystore = (*it).second;
        if (!keystore.key.IsLocked() || !keystore.key.Unlock(strPassphrase))
        {
            return false;
        }

        if (nTimeout > 0)
        {
            keystore.nAutoLockTime = GetTime() + nTimeout;
            keystore.nTimerId = SetTimer(nTimeout * 1000, boost::bind(&CWallet::AutoLock, this, _1, (*it).first));
        }
        return true;
    }
    return false;
}

void CWallet::AutoLock(uint32 nTimerId, const crypto::CPubKey& pubkey)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwKeyStore);
    map<crypto::CPubKey, CWalletKeyStore>::iterator it = mapKeyStore.find(pubkey);
    if (it != mapKeyStore.end())
    {
        CWalletKeyStore& keystore = (*it).second;
        if (keystore.nTimerId == nTimerId)
        {
            keystore.key.Lock();
            keystore.nTimerId = 0;
            keystore.nAutoLockTime = -1;
        }
    }
}

bool CWallet::Sign(const crypto::CPubKey& pubkey, const uint256& hash, vector<uint8>& vchSig) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);
    return SignPubKey(pubkey, hash, vchSig);
}

bool CWallet::LoadTemplate(CTemplatePtr ptr)
{
    if (ptr != nullptr)
    {
        return mapTemplatePtr.insert(make_pair(ptr->GetTemplateId(), ptr)).second;
    }
    return false;
}

void CWallet::GetTemplateIds(set<CTemplateId>& setTemplateId) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);
    for (map<CTemplateId, CTemplatePtr>::const_iterator it = mapTemplatePtr.begin();
         it != mapTemplatePtr.end(); ++it)
    {
        setTemplateId.insert((*it).first);
    }
}

bool CWallet::Have(const CTemplateId& tid) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);
    return mapTemplatePtr.count(tid) != 0;
}

bool CWallet::AddTemplate(CTemplatePtr& ptr)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwKeyStore);
    if (ptr != nullptr)
    {
        CTemplateId tid = ptr->GetTemplateId();
        if (mapTemplatePtr.insert(make_pair(tid, ptr)).second)
        {
            const vector<unsigned char>& vchData = ptr->GetTemplateData();
            return dbWallet.UpdateTemplate(tid, vchData);
        }
    }
    return false;
}

CTemplatePtr CWallet::GetTemplate(const CTemplateId& tid) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);
    map<CTemplateId, CTemplatePtr>::const_iterator it = mapTemplatePtr.find(tid);
    if (it != mapTemplatePtr.end())
    {
        return (*it).second;
    }
    return nullptr;
}

void CWallet::GetDestinations(set<CDestination>& setDest)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);

    for (map<crypto::CPubKey, CWalletKeyStore>::const_iterator it = mapKeyStore.begin();
         it != mapKeyStore.end(); ++it)
    {
        setDest.insert(CDestination((*it).first));
    }

    for (map<CTemplateId, CTemplatePtr>::const_iterator it = mapTemplatePtr.begin();
         it != mapTemplatePtr.end(); ++it)
    {
        setDest.insert(CDestination((*it).first));
    }
}

size_t CWallet::GetTxCount()
{
    boost::shared_lock<boost::shared_mutex> rlock(rwWalletTx);
    return dbWallet.GetTxCount();
}

bool CWallet::ListTx(int nOffset, int nCount, vector<CWalletTx>& vWalletTx)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwWalletTx);
    return dbWallet.ListTx(nOffset, nCount, vWalletTx);
}

bool CWallet::GetBalance(const CDestination& dest, const uint256& hashFork, int nForkHeight, CWalletBalance& balance)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwWalletTx);
    map<CDestination, CWalletUnspent>::iterator it = mapWalletUnspent.find(dest);
    if (it == mapWalletUnspent.end())
    {
        return false;
    }
    CWalletCoins& coins = (*it).second.GetCoins(hashFork);
    balance.SetNull();
    for (const CWalletTxOut& txout : coins.setCoins)
    {
        if (txout.IsLocked(nForkHeight))
        {
            balance.nLocked += txout.GetAmount();
        }
        else
        {
            if (txout.GetDepth(nForkHeight) == 0)
            {
                balance.nUnconfirmed += txout.GetAmount();
            }
        }
    }

    // locked coin template
    if (CTemplate::IsLockedCoin(dest))
    {
        CTemplatePtr ptr = GetTemplate(dest.GetTemplateId());
        if (!ptr)
        {
            return false;
        }
        int64 nLockedCoin = boost::dynamic_pointer_cast<CLockedCoinTemplate>(ptr)->LockedCoin(CDestination(), nForkHeight);
        if (balance.nLocked < nLockedCoin)
        {
            balance.nLocked = nLockedCoin;
        }
        if (balance.nLocked > coins.nTotalValue)
        {
            balance.nLocked = coins.nTotalValue;
        }
    }
    balance.nAvailable = coins.nTotalValue - balance.nLocked;
    return true;
}

bool CWallet::SignTransaction(const CDestination& destIn, CTransaction& tx, bool& fCompleted) const
{
    vector<uint8> vchSig;

    bool fDestInRecorded = CTemplate::IsDestInRecorded(tx.sendTo);
    if (!tx.vchSig.empty())
    {
        if (fDestInRecorded)
        {
            CDestination preDestIn;
            if (!CDestInRecordedTemplate::ParseDestIn(tx.vchSig, preDestIn, vchSig) || preDestIn != destIn)
            {
                return false;
            }
        }
        else
        {
            vchSig = move(tx.vchSig);
        }
    }

    {
        boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);
        if (!SignDestination(destIn, tx, tx.GetSignatureHash(), vchSig, fCompleted))
        {
            return false;
        }
    }

    if (fDestInRecorded)
    {
        CDestInRecordedTemplate::RecordDestIn(destIn, vchSig, tx.vchSig);
    }
    else
    {
        tx.vchSig = move(vchSig);
    }
    return true;
}

bool CWallet::ArrangeInputs(const CDestination& destIn, const uint256& hashFork, int nForkHeight, CTransaction& tx)
{
    tx.vInput.clear();
    int nMaxInput = (MAX_TX_SIZE - MAX_SIGNATURE_SIZE - 4) / 33;
    int64 nTargeValue = tx.nAmount + tx.nTxFee;

    // locked coin template
    if (CTemplate::IsLockedCoin(destIn))
    {
        CTemplatePtr ptr = GetTemplate(destIn.GetTemplateId());
        if (!ptr)
        {
            return false;
        }
        nTargeValue += boost::dynamic_pointer_cast<CLockedCoinTemplate>(ptr)->LockedCoin(tx.sendTo, nForkHeight);
    }

    vector<CTxOutPoint> vCoins;
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwWalletTx);
        int64 nValueIn = SelectCoins(destIn, hashFork, nForkHeight, tx.GetTxTime(), nTargeValue, nMaxInput, vCoins);
        if (nValueIn < nTargeValue)
        {
            return false;
        }
    }
    tx.vInput.reserve(vCoins.size());
    for (const CTxOutPoint& out : vCoins)
    {
        tx.vInput.push_back(CTxIn(out));
    }
    return true;
}

bool CWallet::UpdateTx(const uint256& hashFork, const CAssembledTx& tx)
{
    vector<CWalletTx> vWalletTx;
    bool fIsMine = IsMine(tx.sendTo);
    bool fFromMe = IsMine(tx.destIn);
    if (fFromMe || fIsMine)
    {
        uint256 txid = tx.GetHash();
        std::shared_ptr<CWalletTx> spWalletTx = InsertWalletTx(txid, tx, hashFork, fIsMine, fFromMe);
        if (spWalletTx != nullptr)
        {
            vector<uint256> vFork;
            GetWalletTxFork(hashFork, tx.nBlockHeight, vFork);
            AddNewWalletTx(spWalletTx, vFork);
            vWalletTx.push_back(*spWalletTx);
            if (!spWalletTx->GetRefCount())
            {
                mapWalletTx.erase(txid);
            }
        }
    }
    if (!vWalletTx.empty())
    {
        return dbWallet.UpdateTx(vWalletTx);
    }
    return true;
}

bool CWallet::LoadTxUnspent(const CWalletTx& wtx)
{
    std::shared_ptr<CWalletTx> spWalletTx(new CWalletTx(wtx));
    mapWalletTx.insert(make_pair(wtx.txid, spWalletTx));

    vector<uint256> vFork;
    GetWalletTxFork(spWalletTx->hashFork, spWalletTx->nBlockHeight, vFork);

    if (spWalletTx->IsFromMe())
    {
        for (const uint256& hashFork : vFork)
        {
            mapWalletUnspent[spWalletTx->destIn].Push(hashFork, spWalletTx, 1);
        }
    }
    if (spWalletTx->IsMine())
    {
        for (const uint256& hashFork : vFork)
        {
            mapWalletUnspent[spWalletTx->sendTo].Push(hashFork, spWalletTx, 0);
        }
    }
    return true;
}

bool CWallet::LoadTxSpent(const CWalletTx& wtx)
{
    vector<uint256> vFork;
    GetWalletTxFork(wtx.hashFork, wtx.nBlockHeight, vFork);
    if (wtx.IsFromMe())
    {
        for (const CTxIn& txin : wtx.vInput)
        {
            map<uint256, std::shared_ptr<CWalletTx>>::iterator it = mapWalletTx.find(txin.prevout.hash);
            if (it != mapWalletTx.end())
            {
                std::shared_ptr<CWalletTx>& spPrevWalletTx = (*it).second;
                for (const uint256& hashFork : vFork)
                {
                    mapWalletUnspent[wtx.destIn].Pop(hashFork, spPrevWalletTx, txin.prevout.n);
                }
                if (!spPrevWalletTx->GetRefCount())
                {
                    mapWalletTx.erase(it);
                }
            }
        }
    }
    return true;
}

bool CWallet::AddNewFork(const uint256& hashFork, const uint256& hashParent, int nOriginHeight)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwWalletTx);

    CProfile profile;
    if (!pWorldLineCtrl->GetForkProfile(hashFork, profile))
    {
        return false;
    }
    if (mapFork.insert(make_pair(hashFork, CWalletFork(hashParent, nOriginHeight, profile.IsIsolated()))).second)
    {
        if (hashParent != 0)
        {
            mapFork[hashParent].InsertSubline(nOriginHeight, hashFork);
        }
        if (!profile.IsIsolated())
        {
            for (map<CDestination, CWalletUnspent>::iterator it = mapWalletUnspent.begin();
                 it != mapWalletUnspent.end(); ++it)
            {
                CWalletUnspent& unspent = (*it).second;
                unspent.Dup(hashParent, hashFork);
            }
            vector<uint256> vForkTx;
            if (!dbWallet.ListRollBackTx(hashParent, nOriginHeight + 1, vForkTx))
            {
                return false;
            }
            for (int i = vForkTx.size() - 1; i >= 0; --i)
            {
                std::shared_ptr<CWalletTx> spWalletTx = LoadWalletTx(vForkTx[i]);
                if (spWalletTx != nullptr)
                {
                    RemoveWalletTx(spWalletTx, hashFork);
                    if (!spWalletTx->GetRefCount())
                    {
                        mapWalletTx.erase(vForkTx[i]);
                    }
                }
                else
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool CWallet::ResynchronizeWalletTx()
{
    boost::unique_lock<boost::shared_mutex> wlock(rwWalletTx);

    if (!ClearTx())
    {
        return false;
    }
    set<CDestination> setDest;
    GetDestinations(setDest);

    CWalletTxFilter txFilter(this, setDest);

    return SyncWalletTx(txFilter);
}

bool CWallet::SynchronizeWalletTx(const CDestination& destNew)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwWalletTx);

    CWalletTxFilter txFilter(this, destNew);

    return SyncWalletTx(txFilter);
}

bool CWallet::SyncWalletTx(CTxFilter& txFilter)
{
    vector<uint256> vFork;
    vFork.reserve(mapFork.size());

    vFork.push_back(pCoreProtocol->GetGenesisBlockHash());

    for (int i = 0; i < vFork.size(); i++)
    {
        const uint256& hashFork = vFork[i];

        map<uint256, CWalletFork>::iterator it = mapFork.find(hashFork);
        if (it == mapFork.end())
        {
            return false;
        }

        for (multimap<int, uint256>::iterator mi = (*it).second.mapSubline.begin();
             mi != (*it).second.mapSubline.end(); ++mi)
        {
            vFork.push_back((*mi).second);
        }

        if (!pWorldLineCtrl->FilterTx(hashFork, txFilter) || !pTxPoolCtrl->FilterTx(hashFork, txFilter))
        {
            return false;
        }
    }

    return true;
}

bool CWallet::InspectWalletTx(int nCheckDepth)
{
    set<CDestination> setAddr;
    GetDestinations(setAddr);
    if (setAddr.empty())
    {
        if (dbWallet.GetTxCount() == 0)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    map<uint256, CForkStatus> mapForkStatus;
    pWorldLineCtrl->GetForkStatus(mapForkStatus);
    for (const auto& it : mapForkStatus)
    {
        const auto& hashFork = it.first;
        const auto& status = it.second;
        int nDepth = nCheckDepth;
        if (nDepth > status.nLastBlockHeight || nDepth <= 0)
        {
            nDepth = status.nLastBlockHeight;
        }

        vector<uint256> vFork;
        GetWalletTxFork(hashFork, status.nLastBlockHeight - nDepth, vFork);

        //set of wallet pooled transactions must be equal to set of transactions in txpool
        CInspectWtxFilter filterPool(this, setAddr);
        for (const auto& it : vFork)
        {
            if (!pTxPoolCtrl->FilterTx(it, filterPool)) //condition: fork/dest's
            {
                return false;
            }
        }

        //set of wallet transactions must be equal to set of transactions in the whole block
        CInspectWtxFilter filterTx(this, setAddr);
        for (const auto& it : vFork)
        {
            if (!pWorldLineCtrl->FilterTx(it, nDepth, filterTx)) //condition: fork/depth/dest's
            {
                return false;
            }
        }
    }

    CInspectDBTxWalker walker(this, setAddr);
    if (!dbWallet.WalkThroughTx(walker) && !walker.fRes)
    {
        return false;
    }

    return true;
}

bool CWallet::CompareWithTxOrPool(const CAssembledTx& tx)
{
    CWalletTx wtx;
    if (!dbWallet.RetrieveTx(tx.GetHash(), wtx))
    {
        return false;
    }

    if (tx.nTimeStamp != wtx.nTimeStamp || tx.nVersion != wtx.nVersion
        || tx.nType != wtx.nType || tx.nLockUntil != wtx.nLockUntil
        || tx.vInput != wtx.vInput || tx.sendTo != wtx.sendTo
        || tx.nAmount != wtx.nAmount || tx.nTxFee != wtx.nTxFee
        || tx.nBlockHeight != wtx.nBlockHeight)
    {
        return false;
    }

    return true;
}

bool CWallet::CompareWithPoolOrTx(const CWalletTx& wtx, const std::set<CDestination>& setAddr)
{
    //wallet transactions must be only owned by addresses in the wallet of the node
    if (!setAddr.count(wtx.destIn) || !setAddr.count(wtx.sendTo))
    {
        return false;
    }

    if (wtx.nBlockHeight < 0)
    { //compare wtx with txpool
        CTransaction tx;
        if (!pTxPoolCtrl->Get(wtx.txid, tx))
        {
            return false;
        }
        if (tx.nTimeStamp != wtx.nTimeStamp || tx.nVersion != wtx.nVersion
            || tx.nType != wtx.nType || tx.nLockUntil != wtx.nLockUntil
            || tx.vInput != wtx.vInput || tx.sendTo != wtx.sendTo
            || tx.nAmount != wtx.nAmount || tx.nTxFee != wtx.nTxFee)
        {
            return false;
        }
    }
    else
    { //compare wtx with vtx of block
        CTransaction tx;
        if (!pWorldLineCtrl->GetTransaction(wtx.txid, tx))
        {
            return false;
        }
        if (tx.nTimeStamp != wtx.nTimeStamp || tx.nVersion != wtx.nVersion
            || tx.nType != wtx.nType || tx.nLockUntil != wtx.nLockUntil
            || tx.vInput != wtx.vInput || tx.sendTo != wtx.sendTo
            || tx.nAmount != wtx.nAmount || tx.nTxFee != wtx.nTxFee)
        {
            return false;
        }
    }

    return true;
}

bool CWallet::LoadDB()
{
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwKeyStore);

        CDBAddrWalker walker(this);
        if (!dbWallet.WalkThroughAddress(walker))
        {
            return false;
        }
    }

    {
        boost::unique_lock<boost::shared_mutex> wlock(rwWalletTx);
        if (!UpdateFork())
        {
            return false;
        }

        CDBTxWalker walker(this);
        if (!dbWallet.WalkThroughTx(walker))
        {
            return false;
        }
        walker.LoadSpentTx();
    }
    return true;
}

void CWallet::Clear()
{
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwKeyStore);
        mapKeyStore.clear();
        mapTemplatePtr.clear();
    }
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwWalletTx);
        mapWalletUnspent.clear();
        mapWalletTx.clear();
    }
}

bool CWallet::ClearTx()
{
    mapWalletUnspent.clear();
    mapWalletTx.clear();
    return dbWallet.ClearTx();
}

bool CWallet::InsertKey(const crypto::CKey& key)
{
    if (!key.IsNull())
    {
        pair<map<crypto::CPubKey, CWalletKeyStore>::iterator, bool> ret;
        ret = mapKeyStore.insert(make_pair(key.GetPubKey(), CWalletKeyStore(key)));
        if (ret.second)
        {
            (*(ret.first)).second.key.Lock();
            return true;
        }
    }
    return false;
}

bool CWallet::SynchronizeTxSet(const CTxSetChange& change)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwWalletTx);

    vector<CWalletTx> vWalletTx;
    vector<uint256> vRemove;

    for (std::size_t i = 0; i < change.vTxRemove.size(); i++)
    {
        const uint256& txid = change.vTxRemove[i].first;
        std::shared_ptr<CWalletTx> spWalletTx = LoadWalletTx(txid);
        if (spWalletTx != nullptr)
        {
            RemoveWalletTx(spWalletTx, change.hashFork);
            mapWalletTx.erase(txid);
            vRemove.push_back(txid);
        }
    }

    for (unordered_map<uint256, int>::const_iterator it = change.mapTxUpdate.begin(); it != change.mapTxUpdate.end(); ++it)
    {
        const uint256& txid = (*it).first;
        std::shared_ptr<CWalletTx> spWalletTx = LoadWalletTx(txid);
        if (spWalletTx != nullptr)
        {
            spWalletTx->nBlockHeight = (*it).second;
            vWalletTx.push_back(*spWalletTx);
        }
    }

    map<int, vector<uint256>> mapPreFork;
    for (const CAssembledTx& tx : change.vTxAddNew)
    {
        bool fIsMine = IsMine(tx.sendTo);
        bool fFromMe = IsMine(tx.destIn);
        if (fFromMe || fIsMine)
        {
            uint256 txid = tx.GetHash();
            std::shared_ptr<CWalletTx> spWalletTx = InsertWalletTx(txid, tx, change.hashFork, fIsMine, fFromMe);
            if (spWalletTx != nullptr)
            {
                vector<uint256>& vFork = mapPreFork[spWalletTx->nBlockHeight];
                if (vFork.empty())
                {
                    GetWalletTxFork(change.hashFork, spWalletTx->nBlockHeight, vFork);
                }
                AddNewWalletTx(spWalletTx, vFork);
                vWalletTx.push_back(*spWalletTx);
            }
        }
    }

    for (const CWalletTx& wtx : vWalletTx)
    {
        map<uint256, std::shared_ptr<CWalletTx>>::iterator it = mapWalletTx.find(wtx.txid);
        if (it != mapWalletTx.end() && !(*it).second->GetRefCount())
        {
            mapWalletTx.erase(it);
        }
    }

    if (!vWalletTx.empty() || !vRemove.empty())
    {
        return dbWallet.UpdateTx(vWalletTx, vRemove);
    }

    return true;
}

bool CWallet::AddNewTx(const uint256& hashFork, const CAssembledTx& tx)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwWalletTx);

    bool fIsMine = IsMine(tx.sendTo);
    bool fFromMe = IsMine(tx.destIn);
    if (fFromMe || fIsMine)
    {
        uint256 txid = tx.GetHash();
        std::shared_ptr<CWalletTx> spWalletTx = InsertWalletTx(txid, tx, hashFork, fIsMine, fFromMe);
        if (spWalletTx != nullptr)
        {
            vector<uint256> vFork;
            GetWalletTxFork(hashFork, tx.nBlockHeight, vFork);
            AddNewWalletTx(spWalletTx, vFork);
            if (!spWalletTx->GetRefCount())
            {
                mapWalletTx.erase(txid);
            }
            return dbWallet.AddNewTx(*spWalletTx);
        }
    }
    return true;
}

std::shared_ptr<CWalletTx> CWallet::LoadWalletTx(const uint256& txid)
{
    std::shared_ptr<CWalletTx> spWalletTx;
    map<uint256, std::shared_ptr<CWalletTx>>::iterator it = mapWalletTx.find(txid);
    if (it == mapWalletTx.end())
    {
        CWalletTx wtx;
        if (!dbWallet.RetrieveTx(txid, wtx))
        {
            return nullptr;
        }
        spWalletTx = std::shared_ptr<CWalletTx>(new CWalletTx(wtx));
        mapWalletTx.insert(make_pair(txid, spWalletTx));
    }
    else
    {
        spWalletTx = (*it).second;
    }
    return (!spWalletTx->IsNull() ? spWalletTx : nullptr);
}

std::shared_ptr<CWalletTx> CWallet::InsertWalletTx(const uint256& txid, const CAssembledTx& tx, const uint256& hashFork,
                                                   bool fIsMine, bool fFromMe)
{
    std::shared_ptr<CWalletTx> spWalletTx;
    map<uint256, std::shared_ptr<CWalletTx>>::iterator it = mapWalletTx.find(txid);
    if (it == mapWalletTx.end())
    {
        spWalletTx = std::shared_ptr<CWalletTx>(new CWalletTx(txid, tx, hashFork, fIsMine, fFromMe));
        mapWalletTx.insert(make_pair(txid, spWalletTx));
    }
    else
    {
        spWalletTx = (*it).second;
        spWalletTx->nBlockHeight = tx.nBlockHeight;
        spWalletTx->SetFlags(fIsMine, fFromMe);
    }
    return spWalletTx;
}

int64 CWallet::SelectCoins(const CDestination& dest, const uint256& hashFork, int nForkHeight,
                           int64 nTxTime, int64 nTargetValue, size_t nMaxInput, vector<CTxOutPoint>& vCoins)
{
    vCoins.clear();

    CWalletCoins& walletCoins = mapWalletUnspent[dest].GetCoins(hashFork);
    if (walletCoins.nTotalValue < nTargetValue)
    {
        return 0;
    }

    pair<int64, CWalletTxOut> coinLowestLarger;
    coinLowestLarger.first = std::numeric_limits<int64>::max();
    int64 nTotalLower = 0;

    multimap<int64, CWalletTxOut> mapValue;

    for (const CWalletTxOut& out : walletCoins.setCoins)
    {
        if (out.IsLocked(nForkHeight) || out.GetTxTime() > nTxTime)
        {
            continue;
        }

        int64 nValue = out.GetAmount();
        pair<int64, CWalletTxOut> coin = make_pair(nValue, out);

        if (nValue == nTargetValue)
        {
            vCoins.push_back(out.GetTxOutPoint());
            return nValue;
        }
        else if (nValue < nTargetValue)
        {
            mapValue.insert(coin);
            nTotalLower += nValue;
            while (mapValue.size() > nMaxInput)
            {
                multimap<int64, CWalletTxOut>::iterator mi = mapValue.begin();
                nTotalLower -= (*mi).first;
                mapValue.erase(mi);
            }
            if (nTotalLower >= nTargetValue)
            {
                break;
            }
        }
        else if (nValue < coinLowestLarger.first)
        {
            coinLowestLarger = coin;
        }
    }

    int64 nValueRet = 0;
    if (nTotalLower >= nTargetValue)
    {
        while (nValueRet < nTargetValue)
        {
            int64 nShortage = nTargetValue - nValueRet;
            multimap<int64, CWalletTxOut>::iterator it = mapValue.lower_bound(nShortage);
            if (it == mapValue.end())
            {
                --it;
            }
            vCoins.push_back((*it).second.GetTxOutPoint());
            nValueRet += (*it).first;
            mapValue.erase(it);
        }
    }
    else if (!coinLowestLarger.second.IsNull())
    {
        vCoins.push_back(coinLowestLarger.second.GetTxOutPoint());
        nValueRet += coinLowestLarger.first;
        multimap<int64, CWalletTxOut>::iterator it = mapValue.begin();
        for (int i = 0; i < 3 && it != mapValue.end(); i++, ++it)
        {
            vCoins.push_back((*it).second.GetTxOutPoint());
            nValueRet += (*it).first;
        }
    }
    return nValueRet;
}

bool CWallet::SignPubKey(const crypto::CPubKey& pubkey, const uint256& hash, vector<uint8>& vchSig) const
{
    map<crypto::CPubKey, CWalletKeyStore>::const_iterator it = mapKeyStore.find(pubkey);
    if (it != mapKeyStore.end())
    {
        return (*it).second.key.Sign(hash, vchSig);
    }
    return false;
}

bool CWallet::SignMultiPubKey(const set<crypto::CPubKey>& setPubKey, const uint256& seed, const uint256& hash, vector<uint8>& vchSig) const
{
    bool fSigned = false;
    for (auto& pubkey : setPubKey)
    {
        map<crypto::CPubKey, CWalletKeyStore>::const_iterator it = mapKeyStore.find(pubkey);
        if (it != mapKeyStore.end())
        {
            fSigned |= (*it).second.key.MultiSign(setPubKey, seed, hash, vchSig);
        }
    }
    return fSigned;
}

bool CWallet::SignDestination(const CDestination& destIn, const CTransaction& tx, const uint256& hash, vector<uint8>& vchSig, bool& fCompleted) const
{
    if (destIn.IsPubKey())
    {
        fCompleted = SignPubKey(destIn.GetPubKey(), hash, vchSig);
        return fCompleted;
    }
    else if (destIn.IsTemplate())
    {
        CTemplatePtr ptr = GetTemplate(destIn.GetTemplateId());
        if (!ptr)
        {
            return false;
        }

        set<CDestination> setSubDest;
        vector<uint8> vchSubSig;
        if (!ptr->GetSignDestination(tx, vchSig, setSubDest, vchSubSig))
        {
            return false;
        }

        if (setSubDest.empty())
        {
            return false;
        }
        else if (setSubDest.size() == 1)
        {
            if (!SignDestination(*setSubDest.begin(), tx, hash, vchSubSig, fCompleted))
            {
                return false;
            }
        }
        else
        {
            set<crypto::CPubKey> setPubKey;
            for (const CDestination& dest : setSubDest)
            {
                if (!dest.IsPubKey())
                {
                    return false;
                }
                setPubKey.insert(dest.GetPubKey());
            }

            if (!SignMultiPubKey(setPubKey, tx.hashAnchor, hash, vchSubSig))
            {
                return false;
            }
        }
        if (ptr->GetTemplateType() == TEMPLATE_EXCHANGE)
        {
            CTemplateExchangePtr pe = boost::dynamic_pointer_cast<CTemplateExchange>(ptr);
            return pe->BuildTxSignature(hash, tx.hashAnchor, tx.sendTo, vchSubSig, vchSig);
        }
        else
        {
            return ptr->BuildTxSignature(hash, tx.hashAnchor, tx.sendTo, vchSubSig, vchSig, fCompleted);
        }
    }

    return false;
}

bool CWallet::UpdateFork()
{
    map<uint256, CForkStatus> mapForkStatus;
    multimap<uint256, pair<int, uint256>> mapSubline;

    pWorldLineCtrl->GetForkStatus(mapForkStatus);

    for (map<uint256, CForkStatus>::iterator it = mapForkStatus.begin(); it != mapForkStatus.end(); ++it)
    {
        const uint256 hashFork = (*it).first;
        const CForkStatus& status = (*it).second;
        if (!mapFork.count(hashFork))
        {
            CProfile profile;
            if (!pWorldLineCtrl->GetForkProfile(hashFork, profile))
            {
                return false;
            }
            mapFork.insert(make_pair(hashFork, CWalletFork(status.hashParent, status.nOriginHeight, profile.IsIsolated())));
            if (status.hashParent != 0)
            {
                mapSubline.insert(make_pair(status.hashParent, make_pair(status.nOriginHeight, hashFork)));
            }
        }
    }

    for (multimap<uint256, pair<int, uint256>>::iterator it = mapSubline.begin(); it != mapSubline.end(); ++it)
    {
        mapFork[(*it).first].InsertSubline((*it).second.first, (*it).second.second);
    }
    return true;
}

void CWallet::GetWalletTxFork(const uint256& hashFork, int nHeight, vector<uint256>& vFork)
{
    vector<pair<uint256, CWalletFork*>> vForkPtr;
    {
        map<uint256, CWalletFork>::iterator it = mapFork.find(hashFork);
        if (it != mapFork.end())
        {
            vForkPtr.push_back(make_pair(hashFork, &(*it).second));
        }
    }
    if (nHeight >= 0)
    {
        for (size_t i = 0; i < vForkPtr.size(); i++)
        {
            CWalletFork* pFork = vForkPtr[i].second;
            for (multimap<int, uint256>::iterator mi = pFork->mapSubline.lower_bound(nHeight); mi != pFork->mapSubline.end(); ++mi)
            {
                map<uint256, CWalletFork>::iterator it = mapFork.find((*mi).second);
                if (it != mapFork.end() && !(*it).second.fIsolated)
                {
                    vForkPtr.push_back(make_pair((*it).first, &(*it).second));
                }
            }
        }
    }
    vFork.reserve(vForkPtr.size());
    for (size_t i = 0; i < vForkPtr.size(); i++)
    {
        vFork.push_back(vForkPtr[i].first);
    }
}

void CWallet::AddNewWalletTx(std::shared_ptr<CWalletTx>& spWalletTx, vector<uint256>& vFork)
{
    if (spWalletTx->IsFromMe())
    {
        for (const CTxIn& txin : spWalletTx->vInput)
        {
            map<uint256, std::shared_ptr<CWalletTx>>::iterator it = mapWalletTx.find(txin.prevout.hash);
            if (it != mapWalletTx.end())
            {
                std::shared_ptr<CWalletTx>& spPrevWalletTx = (*it).second;
                for (const uint256& hashFork : vFork)
                {
                    mapWalletUnspent[spWalletTx->destIn].Pop(hashFork, spPrevWalletTx, txin.prevout.n);
                }

                if (!spPrevWalletTx->GetRefCount())
                {
                    mapWalletTx.erase(it);
                }
            }
        }
        for (const uint256& hashFork : vFork)
        {
            mapWalletUnspent[spWalletTx->destIn].Push(hashFork, spWalletTx, 1);
        }
    }
    if (spWalletTx->IsMine())
    {
        for (const uint256& hashFork : vFork)
        {
            mapWalletUnspent[spWalletTx->sendTo].Push(hashFork, spWalletTx, 0);
        }
    }
}

void CWallet::RemoveWalletTx(std::shared_ptr<CWalletTx>& spWalletTx, const uint256& hashFork)
{
    if (spWalletTx->IsFromMe())
    {
        for (const CTxIn& txin : spWalletTx->vInput)
        {
            std::shared_ptr<CWalletTx> spPrevWalletTx = LoadWalletTx(txin.prevout.hash);
            if (spPrevWalletTx != nullptr)
            {
                mapWalletUnspent[spWalletTx->destIn].Push(hashFork, spPrevWalletTx, txin.prevout.n);
            }
        }
        mapWalletUnspent[spWalletTx->destIn].Pop(hashFork, spWalletTx, 1);
    }
    if (spWalletTx->IsMine())
    {
        mapWalletUnspent[spWalletTx->sendTo].Pop(hashFork, spWalletTx, 0);
    }
}

CWalletController::CWalletController()
{
}

CWalletController::~CWalletController()
{
}

bool CWalletController::HandleInitialize()
{
    if (!GetObject("wallet", pWallet))
    {
        ERROR("Failed to request wallet");
        return false;
    }

    RegisterHandler({
        PTR_HANDLER(CAddedBlockMessage, boost::bind(&CWalletController::HandleNewFork, this, _1), true),
        PTR_HANDLER(CAddedTxMessage, boost::bind(&CWalletController::HandleAddedTx, this, _1), true),
        PTR_HANDLER(CSyncTxChangeMessage, boost::bind(&CWalletController::HandleSyncTxChange, this, _1), true),
    });

    return true;
}

void CWalletController::HandleDeinitialize()
{
    DeregisterHandler();

    pWallet = nullptr;
}

bool CWalletController::HandleInvoke()
{
    if (!StartActor())
    {
        ERROR("Failed to start actor");
        return false;
    }
    return true;
}

void CWalletController::HandleHalt()
{
    StopActor();
}

void CWalletController::HandleNewFork(const shared_ptr<CAddedBlockMessage>& spMsg)
{
    TRACE("Wallet received new block message");
    if (spMsg->nErrno == OK && !spMsg->update.IsNull() && spMsg->block.IsOrigin())
    {
        AddNewFork(spMsg->update.hashFork, spMsg->update.hashParent, spMsg->update.nOriginHeight);
    }
}

void CWalletController::HandleAddedTx(const shared_ptr<CAddedTxMessage>& spMsg)
{
    TRACE("Wallet received new tx message");
    AddNewTx(spMsg->hashFork, spMsg->tx);
}

void CWalletController::HandleSyncTxChange(const shared_ptr<CSyncTxChangeMessage>& spMsg)
{
    TRACE("Wallet received sync tx change message");
    SynchronizeTxSet(spMsg->change);
}

} // namespace bigbang
