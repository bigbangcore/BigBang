// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet.h"

#include "../common/template/exchange.h"
#include "address.h"
#include "defs.h"
#include "template/delegate.h"
#include "template/mint.h"
#include "template/vote.h"
#include "template/payment.h"

using namespace std;
using namespace xengine;

namespace bigbang
{

#define MAX_TXIN_SELECTIONS 128
//#define MAX_SIGNATURE_SIZE 2048

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
      : fRes(true), pWallet(pWalletIn), setDest(setDestIn) {}
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
    pBlockChain = nullptr;
    pTxPool = nullptr;
}

CWallet::~CWallet()
{
}

bool CWallet::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("blockchain", pBlockChain))
    {
        Error("Failed to request blockchain");
        return false;
    }

    if (!GetObject("txpool", pTxPool))
    {
        Error("Failed to request txpool");
        return false;
    }

    return true;
}

void CWallet::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pTxPool = nullptr;
}

bool CWallet::HandleInvoke()
{
    if (!dbWallet.Initialize(Config()->pathData / "wallet"))
    {
        Error("Failed to initialize wallet database");
        return false;
    }

    if (!LoadDB())
    {
        Error("Failed to load wallet database");
        return false;
    }

    if (!InspectWalletTx(StorageConfig()->nCheckDepth))
    {
        Log("Failed to inspect wallet transactions");
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
        return (!!mapKeyStore.count(pubkey));
    }
    else if (dest.GetTemplateId(nTemplateId))
    {
        return (!!mapTemplatePtr.count(nTemplateId));
    }
    return false;
}

boost::optional<std::string> CWallet::AddKey(const crypto::CKey& key)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwKeyStore);
    if (!InsertKey(key))
    {
        Warn("AddKey : invalid or duplicated key");
        return std::string("AddKey : invalid or duplicated key");
    }

    if (!dbWallet.UpdateKey(key.GetPubKey(), key.GetVersion(), key.GetCipher()))
    {
        mapKeyStore.erase(key.GetPubKey());
        Warn("AddKey : failed to save key");
        return std::string("AddKey : failed to save key");
    }
    return boost::optional<std::string>{};
}

bool CWallet::LoadKey(const crypto::CKey& key)
{
    if (!InsertKey(key))
    {
        Error("LoadKey : invalid or duplicated key");
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

bool CWallet::Have(const crypto::CPubKey& pubkey, const int32 nVersion) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);
    auto it = mapKeyStore.find(pubkey);
    if (nVersion < 0)
    {
        return it != mapKeyStore.end();
    }
    else
    {
        return (it != mapKeyStore.end() && it->second.key.GetVersion() == nVersion);
    }
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
    return AddKey(key) ? false : true;
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
            Error("AddKey : Encrypt fail");
            return false;
        }
        if (!dbWallet.UpdateKey(key.GetPubKey(), keyTemp.GetVersion(), keyTemp.GetCipher()))
        {
            Error("AddKey : failed to update key");
            return false;
        }
        key.Encrypt(strPassphrase, strCurrentPassphrase);
        key.Lock();
        return true;
    }
    return false;
}

bool CWallet::GetKeyStatus(const crypto::CPubKey& pubkey, int& nVersion, bool& fLocked, int64& nAutoLockTime, bool& fPublic) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);
    map<crypto::CPubKey, CWalletKeyStore>::const_iterator it = mapKeyStore.find(pubkey);
    if (it != mapKeyStore.end())
    {
        const CWalletKeyStore& keystore = (*it).second;
        nVersion = keystore.key.GetVersion();
        fLocked = keystore.key.IsLocked();
        fPublic = keystore.key.IsPubKey();
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
    if (it != mapKeyStore.end() && it->second.key.IsPrivKey())
    {
        CWalletKeyStore& keystore = (*it).second;
        keystore.key.Lock();
        if (keystore.nTimerId)
        {
            CancelTimer(keystore.nTimerId);
            keystore.nTimerId = 0;
            keystore.nAutoLockTime = -1;
            keystore.nAutoDelayTime = 0;
        }
        return true;
    }
    return false;
}

bool CWallet::Unlock(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase, int64 nTimeout)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwKeyStore);
    map<crypto::CPubKey, CWalletKeyStore>::iterator it = mapKeyStore.find(pubkey);
    if (it != mapKeyStore.end() && it->second.key.IsPrivKey())
    {
        CWalletKeyStore& keystore = (*it).second;
        if (!keystore.key.IsLocked() || !keystore.key.Unlock(strPassphrase))
        {
            return false;
        }

        if (nTimeout > 0)
        {
            keystore.nAutoDelayTime = nTimeout;
            keystore.nAutoLockTime = GetTime() + keystore.nAutoDelayTime;
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
    if (it != mapKeyStore.end() && it->second.key.IsPrivKey())
    {
        CWalletKeyStore& keystore = (*it).second;
        if (keystore.nTimerId == nTimerId)
        {
            keystore.key.Lock();
            keystore.nTimerId = 0;
            keystore.nAutoLockTime = -1;
            keystore.nAutoDelayTime = 0;
        }
    }
}

bool CWallet::Sign(const crypto::CPubKey& pubkey, const uint256& hash, vector<uint8>& vchSig)
{
    set<crypto::CPubKey> setSignedKey;
    bool ret;
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);
        ret = SignPubKey(pubkey, hash, vchSig, setSignedKey);
    }

    if (ret)
    {
        UpdateAutoLock(setSignedKey);
    }
    return ret;
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
    return (!!mapTemplatePtr.count(tid));
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

bool CWallet::ListTx(const uint256& hashFork, const CDestination& dest, int nOffset, int nCount, vector<CWalletTx>& vWalletTx)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwWalletTx);
    return dbWallet.ListTx(hashFork, dest, nOffset, nCount, vWalletTx);
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

bool CWallet::SignTransaction(const CDestination& destIn, CTransaction& tx, const int32 nForkHeight, bool& fCompleted)
{
    vector<uint8> vchSig;
    CDestination sendToDelegate;
    CDestination sendToOwner;
    bool fDestInRecorded = false;
    CTemplateId tid;
    if (tx.sendTo.GetTemplateId(tid) && tid.GetType() == TEMPLATE_VOTE)
    {
        CTemplatePtr tempPtr = GetTemplate(tid);
        if (tempPtr != nullptr)
        {
            boost::dynamic_pointer_cast<CSendToRecordedTemplate>(tempPtr)->GetDelegateOwnerDestination(sendToDelegate, sendToOwner);
        }
        if (sendToDelegate.IsNull() || sendToOwner.IsNull())
        {
            StdError("CWallet", "SignTransaction: sendTo does not load template, sendTo: %s, txid: %s",
                     CAddress(tx.sendTo).ToString().c_str(), tx.GetHash().GetHex().c_str());
            return false;
        }
        fDestInRecorded = true;
    }
    if (destIn.GetTemplateId(tid) && tid.GetType() == TEMPLATE_PAYMENT)
    {
        CTemplatePtr tempPtr = GetTemplate(tid);
        if (tempPtr != nullptr)
        {
            auto payment = boost::dynamic_pointer_cast<CTemplatePayment>(tempPtr);
            if (nForkHeight < payment->m_height_end && nForkHeight >= (payment->m_height_exec + payment->SafeHeight))
            {
                CBlock block;
                std::multimap<int64, CDestination> mapVotes;
                if (!pBlockChain->ListDelegatePayment(payment->m_height_exec,block,mapVotes))
                {
                    return false;
                }
                CProofOfSecretShare dpos;
                dpos.Load(block.vchProof);
                uint32 n = dpos.nAgreement.Get32() % mapVotes.size();
                std::vector<CDestination> votes;
                for (const auto& d : mapVotes)
                {
                    votes.push_back(d.second);
                }
                tx.sendTo = votes[n];
            }
        }
        else
        {
            return false;
        }
    }
    if (tx.sendTo.GetTemplateId(tid) && tid.GetType() == TEMPLATE_PAYMENT)
    {
        CTemplatePtr tempPtr = GetTemplate(tid);
        if (tempPtr != nullptr)
        {
            auto payment = boost::dynamic_pointer_cast<CTemplatePayment>(tempPtr);
            if (tx.nAmount != (payment->m_amount + payment->m_pledge))
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    /*bool fDestInRecorded = CTemplate::IsDestInRecorded(tx.sendTo);
    if (!tx.vchSig.empty())
    {
        if (fDestInRecorded)
        {
            CDestination preDestIn;
            if (!CSendToRecordedTemplate::ParseDestIn(tx.vchSig, preDestIn, vchSig) || preDestIn != destIn)
            {
                StdError("CWallet", "SignTransaction: ParseDestIn fail, destIn: %s, txid: %s",
                         destIn.ToString().c_str(), tx.GetHash().GetHex().c_str());
                return false;
            }
        }
        else
        {
            vchSig = move(tx.vchSig);
        }
    }*/

    set<crypto::CPubKey> setSignedKey;
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwKeyStore);
        if (!SignDestination(destIn, tx, tx.GetSignatureHash(), vchSig, nForkHeight, setSignedKey, fCompleted))
        {
            StdError("CWallet", "SignTransaction: SignDestination fail, destIn: %s, txid: %s",
                     destIn.ToString().c_str(), tx.GetHash().GetHex().c_str());
            return false;
        }
    }

    UpdateAutoLock(setSignedKey);

    if (fDestInRecorded)
    {
        CSendToRecordedTemplate::RecordDest(sendToDelegate, sendToOwner, vchSig, tx.vchSig);
        //CSendToRecordedTemplate::RecordDestIn(destDelegate, destOwner, vchSig, tx.vchSig);
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
    //int nMaxInput = (MAX_TX_SIZE - MAX_SIGNATURE_SIZE - 4) / 33;
    int64 nTargetValue = tx.nAmount + tx.nTxFee;

    // locked coin template
    if (CTemplate::IsLockedCoin(destIn))
    {
        CTemplatePtr ptr = GetTemplate(destIn.GetTemplateId());
        if (!ptr)
        {
            StdError("CWallet", "ArrangeInputs: GetTemplate fail, destIn: %s", destIn.ToString().c_str());
            return false;
        }
        nTargetValue += boost::dynamic_pointer_cast<CLockedCoinTemplate>(ptr)->LockedCoin(tx.sendTo, nForkHeight);
    }

    vector<CTxOutPoint> vCoins;
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwWalletTx);
        int64 nValueIn = SelectCoins(destIn, hashFork, nForkHeight, tx.GetTxTime(), nTargetValue, MAX_TX_INPUT_COUNT, vCoins);
        if (nValueIn < nTargetValue)
        {
            StdError("CWallet", "ArrangeInputs: SelectCoins coin not enough, destIn: %s, nValueIn: %ld < nTargeValue: %ld",
                     destIn.ToString().c_str(), nValueIn, nTargetValue);
            return false;
        }
    }
    tx.vInput.reserve(vCoins.size());
    for (const CTxOutPoint& out : vCoins)
    {
        tx.vInput.emplace_back(CTxIn(out));
    }
    return true;
}

bool CWallet::ListForkUnspent(const uint256& hashFork, const CDestination& dest, uint32 nMax, std::vector<CTxUnspent>& vUnspent)
{
    auto it = mapWalletUnspent.find(dest);
    if (it == mapWalletUnspent.end())
    {
        return false;
    }
    auto& setCoins = it->second.GetCoins(hashFork).setCoins;

    vUnspent.clear();
    if (nMax > 0)
    {
        vUnspent.reserve(min(static_cast<size_t>(nMax), setCoins.size()));
    }
    else
    {
        vUnspent.reserve(max(static_cast<size_t>(nMax), setCoins.size()));
    }
    uint32 nCounter = 0;
    for (auto& out : setCoins)
    {
        vUnspent.push_back(CTxUnspent(out.GetTxOutPoint(), out.GetTxOutput()));

        if (nMax != 0 && ++nCounter >= nMax)
        {
            break;
        }
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
        StdTrace("CWallet", "UpdateTx: txid: %s", tx.GetHash().GetHex().c_str());
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
    StdTrace("CWallet", "LoadTxUnspent: txid: %s", wtx.txid.GetHex().c_str());
    std::shared_ptr<CWalletTx> spWalletTx(new CWalletTx(wtx));
    mapWalletTx.insert(make_pair(wtx.txid, spWalletTx));

    vector<uint256> vFork;
    GetWalletTxFork(spWalletTx->hashFork, spWalletTx->nBlockHeight, vFork);

    if (spWalletTx->IsFromMe())
    {
        if (!AddWalletTxOut(CTxOutPoint(wtx.txid, 1)))
        {
            StdError("CWallet", "LoadTxUnspent: Txout added, txout: [1] %s", wtx.txid.GetHex().c_str());
            return false;
        }
        for (const uint256& hashFork : vFork)
        {
            mapWalletUnspent[spWalletTx->destIn].Push(hashFork, spWalletTx, 1);
        }
    }
    if (spWalletTx->IsMine())
    {
        if (!AddWalletTxOut(CTxOutPoint(wtx.txid, 0)))
        {
            StdError("CWallet", "LoadTxUnspent: Txout added, txout: [0] %s", wtx.txid.GetHex().c_str());
            return false;
        }
        for (const uint256& hashFork : vFork)
        {
            mapWalletUnspent[spWalletTx->sendTo].Push(hashFork, spWalletTx, 0);
        }
    }
    return true;
}

bool CWallet::LoadTxSpent(const CWalletTx& wtx)
{
    StdTrace("CWallet", "LoadTxSpent: txid: %s", wtx.txid.GetHex().c_str());
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
    if (!pBlockChain->GetForkProfile(hashFork, profile))
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
            StdLog("CWallet", "SyncWalletTx: Find fork fail, fork: %s.", hashFork.GetHex().c_str());
            return false;
        }

        for (multimap<int, uint256>::iterator mi = (*it).second.mapSubline.begin();
             mi != (*it).second.mapSubline.end(); ++mi)
        {
            vFork.push_back((*mi).second);
        }

        if (!pBlockChain->FilterTx(hashFork, txFilter))
        {
            StdLog("CWallet", "SyncWalletTx: BlockChain filter fail, fork: %s.", hashFork.GetHex().c_str());
            return false;
        }
        if (!pTxPool->FilterTx(hashFork, txFilter))
        {
            StdLog("CWallet", "SyncWalletTx: TxPool filter fail, fork: %s.", hashFork.GetHex().c_str());
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
            StdLog("CWallet", "InspectWalletTx: Address is empty, But tx is greater than 0.");
            return false;
        }
    }

    map<uint256, CForkStatus> mapForkStatus;
    pBlockChain->GetForkStatus(mapForkStatus);
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
            if (!pTxPool->FilterTx(it, filterPool)) //condition: fork/dest's
            {
                StdLog("CWallet", "InspectWalletTx: Filter txpool fail.");
                return false;
            }
        }

        //set of wallet transactions must be equal to set of transactions in the whole block
        CInspectWtxFilter filterTx(this, setAddr);
        for (const auto& it : vFork)
        {
            if (!pBlockChain->FilterTx(it, nDepth, filterTx)) //condition: fork/depth/dest's
            {
                StdLog("CWallet", "InspectWalletTx: Filter blockchain fail.");
                return false;
            }
        }
    }

    CInspectDBTxWalker walker(this, setAddr);
    if (!dbWallet.WalkThroughTx(walker) || !walker.fRes)
    {
        StdLog("CWallet", "InspectWalletTx: Inspect db tx fail.");
        return false;
    }

    return true;
}

bool CWallet::CompareWithTxOrPool(const CAssembledTx& tx)
{
    CWalletTx wtx;
    if (!dbWallet.RetrieveTx(tx.GetHash(), wtx))
    {
        StdLog("CWallet", "CompareWithTxOrPool: Retrieve tx fail, txid: %s.", tx.GetHash().GetHex().c_str());
        return false;
    }

    if (tx.nTimeStamp != wtx.nTimeStamp || tx.nVersion != wtx.nVersion
        || tx.nType != wtx.nType || tx.nLockUntil != wtx.nLockUntil
        || tx.vInput != wtx.vInput || tx.sendTo != wtx.sendTo
        || tx.nAmount != wtx.nAmount || tx.nTxFee != wtx.nTxFee
        || tx.nBlockHeight != wtx.nBlockHeight)
    {
        std::string strFailCause;
        if (tx.nTimeStamp != wtx.nTimeStamp)
        {
            strFailCause += std::string("nTimeStamp error, tx.nTimeStamp=") + std::to_string(tx.nTimeStamp) + std::string(",wtx.nTimeStamp=") + std::to_string(wtx.nTimeStamp);
        }
        if (tx.nVersion != wtx.nVersion)
        {
            if (!strFailCause.empty())
            {
                strFailCause += std::string(",");
            }
            strFailCause += std::string("nVersion error, tx.nVersion=") + std::to_string(tx.nVersion) + std::string(",wtx.nVersion=") + std::to_string(wtx.nVersion);
        }
        if (tx.nType != wtx.nType)
        {
            if (!strFailCause.empty())
            {
                strFailCause += std::string(",");
            }
            strFailCause += std::string("nType error, tx.nType=") + std::to_string(tx.nType) + std::string(",wtx.nType=") + std::to_string(wtx.nType);
        }
        if (tx.nLockUntil != wtx.nLockUntil)
        {
            if (!strFailCause.empty())
            {
                strFailCause += std::string(",");
            }
            strFailCause += std::string("nLockUntil error, tx.nLockUntil=") + std::to_string(tx.nLockUntil) + std::string(",wtx.nLockUntil=") + std::to_string(wtx.nLockUntil);
        }
        if (tx.vInput != wtx.vInput)
        {
            if (!strFailCause.empty())
            {
                strFailCause += std::string(",");
            }
            if (tx.vInput.size() != wtx.vInput.size())
            {
                strFailCause += std::string("vInput size error, tx.vInput.size()=") + std::to_string(tx.vInput.size()) + std::string(",wtx.vInput.size()=") + std::to_string(wtx.vInput.size());
            }
            else
            {
                strFailCause += std::string("vInput data error");
            }
            StdLog("CWallet", "vInput error:");
            for (int i = 0; i < tx.vInput.size(); i++)
            {
                StdLog("CWallet", "tx.vInput[%d]: [%d] %s", i, tx.vInput[i].prevout.n, tx.vInput[i].prevout.hash.GetHex().c_str());
            }
            for (int i = 0; i < wtx.vInput.size(); i++)
            {
                StdLog("CWallet", "wtx.vInput[%d]: [%d] %s", i, wtx.vInput[i].prevout.n, wtx.vInput[i].prevout.hash.GetHex().c_str());
            }
        }
        if (tx.sendTo != wtx.sendTo)
        {
            if (!strFailCause.empty())
            {
                strFailCause += std::string(",");
            }
            strFailCause += std::string("sendTo error, tx.sendTo=") + CAddress(tx.sendTo).ToString() + std::string(",wtx.sendTo=") + CAddress(wtx.sendTo).ToString();
        }
        if (tx.nAmount != wtx.nAmount)
        {
            if (!strFailCause.empty())
            {
                strFailCause += std::string(",");
            }
            strFailCause += std::string("nAmount error, tx.nAmount=") + std::to_string(tx.nAmount) + std::string(",wtx.nAmount=") + std::to_string(wtx.nAmount);
        }
        if (tx.nTxFee != wtx.nTxFee)
        {
            if (!strFailCause.empty())
            {
                strFailCause += std::string(",");
            }
            strFailCause += std::string("nTxFee error, tx.nTxFee=") + std::to_string(tx.nTxFee) + std::string(",wtx.nTxFee=") + std::to_string(wtx.nTxFee);
        }
        if (tx.nBlockHeight != wtx.nBlockHeight)
        {
            if (!strFailCause.empty())
            {
                strFailCause += std::string(",");
            }
            strFailCause += std::string("nBlockHeight error, tx.nBlockHeight=") + std::to_string(tx.nBlockHeight) + std::string(",wtx.nBlockHeight=") + std::to_string(wtx.nBlockHeight);
        }
        StdLog("CWallet", "CompareWithTxOrPool: Check fail, txid: %s, cause: %s.", tx.GetHash().GetHex().c_str(), strFailCause.c_str());
        return false;
    }

    return true;
}

bool CWallet::CompareWithPoolOrTx(const CWalletTx& wtx, const std::set<CDestination>& setAddr)
{
    //wallet transactions must be only owned by addresses in the wallet of the node
    if (!setAddr.count(wtx.destIn) && !setAddr.count(wtx.sendTo))
    {
        StdLog("CWallet", "CompareWithPoolOrTx: Address error, wtx.destIn: %s, wtx.sendTo: %s.",
               CAddress(wtx.destIn).ToString().c_str(), CAddress(wtx.sendTo).ToString().c_str());
        return false;
    }

    if (wtx.nBlockHeight < 0)
    { //compare wtx with txpool
        CTransaction tx;
        if (!pTxPool->Get(wtx.txid, tx))
        {
            StdLog("CWallet", "CompareWithPoolOrTx: TxPool get fail, wtx.txid: %s.",
                   wtx.txid.GetHex().c_str());
            return false;
        }
        if (tx.nTimeStamp != wtx.nTimeStamp || tx.nVersion != wtx.nVersion
            || tx.nType != wtx.nType || tx.nLockUntil != wtx.nLockUntil
            || tx.vInput != wtx.vInput || tx.sendTo != wtx.sendTo
            || tx.nAmount != wtx.nAmount || tx.nTxFee != wtx.nTxFee)
        {
            StdLog("CWallet", "CompareWithPoolOrTx: TxPool check fail, wtx.txid: %s.",
                   wtx.txid.GetHex().c_str());
            return false;
        }
    }
    else
    { //compare wtx with vtx of block
        CTransaction tx;
        if (!pBlockChain->GetTransaction(wtx.txid, tx))
        {
            StdLog("CWallet", "CompareWithPoolOrTx: BlockChain GetTransaction fail, wtx.txid: %s.",
                   wtx.txid.GetHex().c_str());
            return false;
        }
        if (tx.nTimeStamp != wtx.nTimeStamp || tx.nVersion != wtx.nVersion
            || tx.nType != wtx.nType || tx.nLockUntil != wtx.nLockUntil
            || tx.vInput != wtx.vInput || tx.sendTo != wtx.sendTo
            || tx.nAmount != wtx.nAmount || tx.nTxFee != wtx.nTxFee)
        {
            StdLog("CWallet", "CompareWithPoolOrTx: BlockChain check fail, wtx.txid: %s.",
                   wtx.txid.GetHex().c_str());
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
            StdLog("CWallet", "LoadDB: WalkThroughAddress fail.");
            return false;
        }
    }

    {
        boost::unique_lock<boost::shared_mutex> wlock(rwWalletTx);
        if (!UpdateFork())
        {
            StdLog("CWallet", "LoadDB: UpdateFork fail.");
            return false;
        }

        CDBTxWalker walker(this);
        if (!dbWallet.WalkThroughTx(walker))
        {
            StdLog("CWallet", "LoadDB: WalkThroughTx fail.");
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
        auto it = mapKeyStore.find(key.GetPubKey());
        if (it != mapKeyStore.end())
        {
            // privkey update pubkey
            if (it->second.key.IsPubKey() && key.IsPrivKey())
            {
                it->second = CWalletKeyStore(key);
                it->second.key.Lock();
                return true;
            }
            return false;
        }
        else
        {
            auto ret = mapKeyStore.insert(make_pair(key.GetPubKey(), CWalletKeyStore(key)));
            ret.first->second.key.Lock();
            return true;
        }
    }
    return false;
}

bool CWallet::SynchronizeTxSet(const CTxSetChange& change)
{
    StdTrace("CWallet", "SynchronizeTxSet: add: %ld, remove: %ld, udpate: %ld", change.vTxAddNew.size(), change.vTxRemove.size(), change.mapTxUpdate.size());
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

    for (map<uint256, int>::const_iterator it = change.mapTxUpdate.begin(); it != change.mapTxUpdate.end(); ++it)
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
        StdTrace("CWallet", "AddNewTx: txid: %s", tx.GetHash().GetHex().c_str());
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

    auto it = mapWalletUnspent.find(dest);
    if (it == mapWalletUnspent.end())
    {
        StdLog("CWallet", "SelectCoins: Find dest fail, dest: %s.", CAddress(dest).ToString().c_str());
        return 0;
    }

    CWalletCoins& walletCoins = it->second.GetCoins(hashFork);
    if (walletCoins.nTotalValue < nTargetValue)
    {
        StdLog("CWallet", "SelectCoins: Coins not enough, dest: %s, nTotalValue: %ld, nTargetValue: %ld.",
               CAddress(dest).ToString().c_str(), walletCoins.nTotalValue, nTargetValue);
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

bool CWallet::SignPubKey(const crypto::CPubKey& pubkey, const uint256& hash, vector<uint8>& vchSig, std::set<crypto::CPubKey>& setSignedKey)
{
    auto it = mapKeyStore.find(pubkey);
    if (it == mapKeyStore.end())
    {
        StdError("CWallet", "SignPubKey: find privkey fail, pubkey: %s", pubkey.GetHex().c_str());
        return false;
    }
    if (!it->second.key.IsPrivKey())
    {
        StdError("CWallet", "SignPubKey: can't sign tx by non-privkey, pubkey: %s", pubkey.GetHex().c_str());
        return false;
    }
    if (!it->second.key.Sign(hash, vchSig))
    {
        StdError("CWallet", "SignPubKey: sign fail, pubkey: %s", pubkey.GetHex().c_str());
        return false;
    }

    setSignedKey.insert(pubkey);
    return true;
}

bool CWallet::SignMultiPubKey(const set<crypto::CPubKey>& setPubKey, const uint256& hash, const uint256& hashAnchor,
                              const int32 nForkHeight, vector<uint8>& vchSig, std::set<crypto::CPubKey>& setSignedKey)
{
    bool fSigned = false;
    for (auto& pubkey : setPubKey)
    {
        auto it = mapKeyStore.find(pubkey);
        if (it != mapKeyStore.end() && it->second.key.IsPrivKey())
        {
            if (nForkHeight > 0 && nForkHeight < HEIGHT_HASH_MULTI_SIGNER)
            {
                fSigned |= it->second.key.MultiSignDefect(setPubKey, hashAnchor, hash, vchSig);
            }
            else
            {
                fSigned |= it->second.key.MultiSign(setPubKey, hash, vchSig);
            }
            setSignedKey.insert(pubkey);
        }
    }
    return fSigned;
}

bool CWallet::SignDestination(const CDestination& destIn, const CTransaction& tx,
                              const uint256& hash, vector<uint8>& vchSig,
                              const int32 nForkHeight, std::set<crypto::CPubKey>& setSignedKey, bool& fCompleted)
{
    if (destIn.IsPubKey())
    {
        fCompleted = SignPubKey(destIn.GetPubKey(), hash, vchSig, setSignedKey);
        if (!fCompleted)
        {
            StdError("CWallet", "SignDestination: PubKey SignPubKey fail, txid: %s, destIn: %s", tx.GetHash().GetHex().c_str(), destIn.ToString().c_str());
            return false;
        }
        return fCompleted;
    }
    else if (destIn.IsTemplate())
    {
        CTemplatePtr ptr = GetTemplate(destIn.GetTemplateId());
        if (!ptr)
        {
            StdError("CWallet", "SignDestination: GetTemplate fail, txid: %s, destIn: %s", tx.GetHash().GetHex().c_str(), destIn.ToString().c_str());
            return false;
        }

        set<CDestination> setSubDest;
        vector<uint8> vchSubSig;
        if (!ptr->GetSignDestination(tx, vchSig, setSubDest, vchSubSig))
        {
            StdError("CWallet", "SignDestination: GetSignDestination fail, txid: %s", tx.GetHash().GetHex().c_str());
            return false;
        }

        if (setSubDest.empty())
        {
            StdError("CWallet", "SignDestination: setSubDest is empty, txid: %s", tx.GetHash().GetHex().c_str());
            return false;
        }
        else if (setSubDest.size() == 1)
        {
            if (!SignDestination(*setSubDest.begin(), tx, hash, vchSubSig, nForkHeight, setSignedKey, fCompleted))
            {
                StdError("CWallet", "SignDestination: SignDestination fail, txid: %s", tx.GetHash().GetHex().c_str());
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
                    StdError("CWallet", "SignDestination: dest not is pubkey, txid: %s, dest: %s", tx.GetHash().GetHex().c_str(), dest.ToString().c_str());
                    return false;
                }
                setPubKey.insert(dest.GetPubKey());
            }

            if (!SignMultiPubKey(setPubKey, hash, tx.hashAnchor, nForkHeight, vchSubSig, setSignedKey))
            {
                StdError("CWallet", "SignDestination: SignMultiPubKey fail, txid: %s", tx.GetHash().GetHex().c_str());
                return false;
            }
        }
        if (ptr->GetTemplateType() == TEMPLATE_EXCHANGE)
        {
            CTemplateExchangePtr pe = boost::dynamic_pointer_cast<CTemplateExchange>(ptr);
            vchSig = tx.vchSig;
            return pe->BuildTxSignature(hash, tx.nType, tx.hashAnchor, tx.sendTo, vchSubSig, vchSig);
        }
        else
        {
            if (!ptr->BuildTxSignature(hash, tx.nType, tx.hashAnchor, tx.sendTo, nForkHeight, vchSubSig, vchSig, fCompleted))
            {
                StdError("CWallet", "SignDestination: BuildTxSignature fail, txid: %s", tx.GetHash().GetHex().c_str());
                return false;
            }
        }
    }
    else
    {
        StdError("CWallet", "SignDestination: destIn type error, txid: %s, destIn: %s", tx.GetHash().GetHex().c_str(), destIn.ToString().c_str());
        return false;
    }
    return true;
}

void CWallet::UpdateAutoLock(const std::set<crypto::CPubKey>& setSignedKey)
{
    if (setSignedKey.empty())
    {
        return;
    }

    boost::unique_lock<boost::shared_mutex> wlock(rwKeyStore);
    for (auto& key: setSignedKey)
    {
        map<crypto::CPubKey, CWalletKeyStore>::iterator it = mapKeyStore.find(key);
        if (it != mapKeyStore.end() && it->second.key.IsPrivKey())
        {
            CWalletKeyStore& keystore = (*it).second;
            if (keystore.nAutoDelayTime > 0)
            {
                CancelTimer(keystore.nTimerId);
                keystore.nAutoLockTime = GetTime() + keystore.nAutoDelayTime;
                keystore.nTimerId = SetTimer(keystore.nAutoDelayTime * 1000, boost::bind(&CWallet::AutoLock, this, _1, keystore.key.GetPubKey()));
            }
        }
    }
}

bool CWallet::UpdateFork()
{
    map<uint256, CForkStatus> mapForkStatus;
    multimap<uint256, pair<int, uint256>> mapSubline;

    pBlockChain->GetForkStatus(mapForkStatus);

    for (map<uint256, CForkStatus>::iterator it = mapForkStatus.begin(); it != mapForkStatus.end(); ++it)
    {
        const uint256 hashFork = (*it).first;
        const CForkStatus& status = (*it).second;
        if (!mapFork.count(hashFork))
        {
            CProfile profile;
            if (!pBlockChain->GetForkProfile(hashFork, profile))
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

bool CWallet::AddWalletTxOut(const CTxOutPoint& txout)
{
    if (setWalletTxOut.find(txout) == setWalletTxOut.end())
    {
        setWalletTxOut.insert(txout);
        return true;
    }
    return false;
}

void CWallet::RemoveWalletTxOut(const CTxOutPoint& txout)
{
    setWalletTxOut.erase(txout);
}

void CWallet::AddNewWalletTx(std::shared_ptr<CWalletTx>& spWalletTx, vector<uint256>& vFork)
{
    StdTrace("CWallet", "Add new wallet tx: txid: %s", spWalletTx->txid.GetHex().c_str());
    if (spWalletTx->IsFromMe())
    {
        if (AddWalletTxOut(CTxOutPoint(spWalletTx->txid, 1)))
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
                else
                {
                    StdError("CWallet", "Add new wallet tx: find prev tx fail, txid: %s", txin.prevout.hash.GetHex().c_str());
                }
            }
            for (const uint256& hashFork : vFork)
            {
                mapWalletUnspent[spWalletTx->destIn].Push(hashFork, spWalletTx, 1);
            }
        }
        else
        {
            StdTrace("CWallet", "Add new wallet tx: Txout added, txout: [1] %s", spWalletTx->txid.GetHex().c_str());
        }
    }
    if (spWalletTx->IsMine())
    {
        if (AddWalletTxOut(CTxOutPoint(spWalletTx->txid, 0)))
        {
            for (const uint256& hashFork : vFork)
            {
                mapWalletUnspent[spWalletTx->sendTo].Push(hashFork, spWalletTx, 0);
            }
        }
        else
        {
            StdTrace("CWallet", "Add new wallet tx: Txout added, txout: [0] %s", spWalletTx->txid.GetHex().c_str());
        }
    }
}

void CWallet::RemoveWalletTx(std::shared_ptr<CWalletTx>& spWalletTx, const uint256& hashFork)
{
    StdTrace("CWallet", "Remove wallet tx: txid: %s", spWalletTx->txid.GetHex().c_str());
    if (spWalletTx->IsFromMe())
    {
        RemoveWalletTxOut(CTxOutPoint(spWalletTx->txid, 1));
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
        RemoveWalletTxOut(CTxOutPoint(spWalletTx->txid, 0));
        mapWalletUnspent[spWalletTx->sendTo].Pop(hashFork, spWalletTx, 0);
    }
}

} // namespace bigbang
