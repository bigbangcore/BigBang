// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "checkrepair.h"

using namespace std;
using namespace xengine;
using namespace boost::filesystem;

#define BLOCKFILE_PREFIX "block"

namespace bigbang
{

/////////////////////////////////////////////////////////////////////////
// CCheckForkUnspentWalker

bool CCheckForkUnspentWalker::Walk(const CTxOutPoint& txout, const CTxOut& output)
{
    if (!output.IsNull())
    {
        if (!mapForkUnspent.insert(make_pair(txout, CCheckTxOut(output))).second)
        {
            StdError("check", "Insert leveldb unspent fail, unspent: [%d] %s.", txout.n, txout.hash.GetHex().c_str());
            return false;
        }
    }
    else
    {
        StdLog("check", "Leveldb unspent is NULL, unspent: [%d] %s.", txout.n, txout.hash.GetHex().c_str());
    }
    return true;
}

bool CCheckForkUnspentWalker::CheckForkUnspent(map<CTxOutPoint, CCheckTxOut>& mapBlockForkUnspent)
{
    {
        map<CTxOutPoint, CCheckTxOut>::iterator it = mapBlockForkUnspent.begin();
        for (; it != mapBlockForkUnspent.end(); ++it)
        {
            if (!it->second.IsSpent())
            {
                map<CTxOutPoint, CCheckTxOut>::iterator mt = mapForkUnspent.find(it->first);
                if (mt == mapForkUnspent.end())
                {
                    StdLog("check", "Check block unspent: find utxo fail, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                    mapForkUnspent.insert(*it);
                    vAddUpdate.push_back(CTxUnspent(it->first, static_cast<const CTxOut&>(it->second)));
                }
                else if (mt->second.IsSpent())
                {
                    StdLog("check", "Check block unspent: is spent, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                    mt->second = it->second;
                    vAddUpdate.push_back(CTxUnspent(it->first, static_cast<const CTxOut&>(it->second)));
                }
                else if (it->second != mt->second)
                {
                    StdLog("check", "Check block unspent: txout error, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                    mt->second = it->second;
                    vAddUpdate.push_back(CTxUnspent(it->first, static_cast<const CTxOut&>(it->second)));
                }
            }
        }
    }

    {
        map<CTxOutPoint, CCheckTxOut>::iterator it = mapForkUnspent.begin();
        for (; it != mapForkUnspent.end();)
        {
            if (!it->second.IsSpent())
            {
                map<CTxOutPoint, CCheckTxOut>::iterator mt = mapBlockForkUnspent.find(it->first);
                if (mt == mapBlockForkUnspent.end())
                {
                    StdLog("check", "Check db unspent: find utxo fail, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                    vRemove.push_back(it->first);
                    mapForkUnspent.erase(it++);
                    continue;
                }
                else if (mt->second.IsSpent())
                {
                    StdLog("check", "Check db unspent: is spent, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                    vRemove.push_back(it->first);
                    mapForkUnspent.erase(it++);
                    continue;
                }
                else if (it->second != mt->second)
                {
                    StdLog("check", "Check db unspent: txout error, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                    it->second = mt->second;
                    vAddUpdate.push_back(CTxUnspent(it->first, static_cast<const CTxOut&>(it->second)));
                }
            }
            ++it;
        }
    }
    return !(vAddUpdate.size() > 0 || vRemove.size() > 0);
}

/////////////////////////////////////////////////////////////////////////
// CCheckForkManager

bool CCheckForkManager::FetchForkStatus(const string& strDataPath)
{
    CForkDB dbFork;
    if (!dbFork.Initialize(path(strDataPath)))
    {
        StdError("check", "Fetch fork status: Initialize fail");
        return false;
    }

    vector<pair<uint256, uint256>> vFork;
    if (!dbFork.ListFork(vFork))
    {
        StdError("check", "Fetch fork status: ListFork fail");
        dbFork.Deinitialize();
        return false;
    }

    for (const auto& fork : vFork)
    {
        const uint256 hashFork = fork.first;
        CCheckForkStatus& status = mapForkStatus[hashFork];
        status.hashLastBlock = fork.second;

        if (!dbFork.RetrieveForkContext(hashFork, status.ctxt))
        {
            StdError("check", "Fetch fork status: RetrieveForkContext fail");
            dbFork.Deinitialize();
            return false;
        }

        if (status.ctxt.hashParent != 0)
        {
            mapForkStatus[status.ctxt.hashParent].InsertSubline(status.ctxt.nJointHeight, hashFork);
        }
    }

    dbFork.Deinitialize();
    return true;
}

void CCheckForkManager::GetForkList(const uint256& hashGenesis, vector<uint256>& vForkList)
{
    set<uint256> setFork;

    vForkList.clear();
    vForkList.push_back(hashGenesis);
    setFork.insert(hashGenesis);

    for (int i = 0; i < vForkList.size(); i++)
    {
        map<uint256, CCheckForkStatus>::iterator it = mapForkStatus.find(vForkList[i]);
        if (it != mapForkStatus.end())
        {
            for (auto mt = it->second.mapSubline.begin(); mt != it->second.mapSubline.end(); ++mt)
            {
                if (setFork.count(mt->second) == 0 && mapForkStatus.count(mt->second) > 0)
                {
                    vForkList.push_back(mt->second);
                    setFork.insert(mt->second);
                }
            }
        }
        else
        {
            StdLog("check", "Fork manager get fork list: find fork fail, fork: %s", vForkList[i].GetHex().c_str());
        }
    }
}

void CCheckForkManager::GetTxFork(const uint256& hashFork, int nHeight, vector<uint256>& vFork)
{
    vector<pair<uint256, CCheckForkStatus*>> vForkPtr;
    {
        map<uint256, CCheckForkStatus>::iterator it = mapForkStatus.find(hashFork);
        if (it != mapForkStatus.end())
        {
            vForkPtr.push_back(make_pair(hashFork, &(*it).second));
        }
    }
    if (nHeight >= 0)
    {
        for (size_t i = 0; i < vForkPtr.size(); i++)
        {
            CCheckForkStatus* pFork = vForkPtr[i].second;
            for (multimap<int, uint256>::iterator mi = pFork->mapSubline.lower_bound(nHeight); mi != pFork->mapSubline.end(); ++mi)
            {
                map<uint256, CCheckForkStatus>::iterator it = mapForkStatus.find((*mi).second);
                if (it != mapForkStatus.end() && !(*it).second.ctxt.IsIsolated())
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

bool CCheckForkManager::UpdateForkLast(const string& strDataPath, const vector<pair<uint256, uint256>>& vForkLast)
{
    CForkDB dbFork;
    if (!dbFork.Initialize(path(strDataPath)))
    {
        StdError("check", "Update fork last: Initialize fail");
        return false;
    }
    for (const auto& data : vForkLast)
    {
        if (!dbFork.UpdateFork(data.first, data.second))
        {
            StdError("check", "Update fork last: update fail, fork: %s, last: %s",
                     data.first.GetHex().c_str(), data.second.GetHex().c_str());
            dbFork.Deinitialize();
            return false;
        }
    }
    dbFork.Deinitialize();
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckWalletForkUnspent

bool CCheckWalletForkUnspent::LocalTxExist(const uint256& txid)
{
    map<uint256, CCheckWalletTx>::iterator it = mapWalletTx.find(txid);
    if (it == mapWalletTx.end())
    {
        StdLog("check", "Wallet fork LocalTxExist: find tx fail, txid: %s", txid.GetHex().c_str());
        return false;
    }
    if (it->second.hashFork != hashFork)
    {
        StdLog("check", "Wallet fork LocalTxExist: fork error, txid: %s, wtx fork: %s, wallet fork: %s",
               txid.GetHex().c_str(), it->second.hashFork.GetHex().c_str(), hashFork.GetHex().c_str());
        return false;
    }
    return true;
}

CCheckWalletTx* CCheckWalletForkUnspent::GetLocalWalletTx(const uint256& txid)
{
    map<uint256, CCheckWalletTx>::iterator it = mapWalletTx.find(txid);
    if (it == mapWalletTx.end())
    {
        StdLog("check", "Wallet fork GetLocalWalletTx: find tx fail, txid: %s", txid.GetHex().c_str());
        return nullptr;
    }
    if (it->second.hashFork != hashFork)
    {
        StdLog("check", "Wallet fork GetLocalWalletTx: fork error, txid: %s, wtx fork: %s, wallet fork: %s",
               txid.GetHex().c_str(), it->second.hashFork.GetHex().c_str(), hashFork.GetHex().c_str());
        return nullptr;
    }
    return &(it->second);
}

bool CCheckWalletForkUnspent::AddTx(const CWalletTx& wtx)
{
    if (mapWalletTx.find(wtx.txid) != mapWalletTx.end())
    {
        StdLog("check", "Add wallet tx: tx is exist, txid: %s", wtx.txid.GetHex().c_str());
        return false;
    }
    map<uint256, CCheckWalletTx>::iterator it = mapWalletTx.insert(make_pair(wtx.txid, CCheckWalletTx(wtx, ++nSeqCreate))).first;
    if (it == mapWalletTx.end())
    {
        StdLog("check", "Add wallet tx: add fail, txid: %s", wtx.txid.GetHex().c_str());
        return false;
    }
    CWalletTxLinkSetByTxHash& idxTx = setWalletTxLink.get<0>();
    if (idxTx.find(wtx.txid) != idxTx.end())
    {
        setWalletTxLink.erase(wtx.txid);
    }
    if (!setWalletTxLink.insert(CWalletTxLink(&(it->second))).second)
    {
        StdLog("check", "Add wallet tx: add link fail, txid: %s", wtx.txid.GetHex().c_str());
        return false;
    }
    return true;
}

void CCheckWalletForkUnspent::RemoveTx(const uint256& txid)
{
    mapWalletTx.erase(txid);
    setWalletTxLink.erase(txid);
}

bool CCheckWalletForkUnspent::UpdateUnspent()
{
    CWalletTxLinkSetBySequenceNumber& idxTx = setWalletTxLink.get<1>();
    {
        CWalletTxLinkSetBySequenceNumber::iterator it = idxTx.begin();
        for (; it != idxTx.end(); ++it)
        {
            const CCheckWalletTx& wtx = *(it->ptx);
            if (wtx.IsMine())
            {
                if (!AddWalletUnspent(CTxOutPoint(wtx.txid, 0), wtx.GetOutput(0)))
                {
                    StdError("check", "Wallet update unspent: add unspent 0 fail");
                    return false;
                }
            }
            if (wtx.IsFromMe())
            {
                if (!AddWalletUnspent(CTxOutPoint(wtx.txid, 1), wtx.GetOutput(1)))
                {
                    StdError("check", "Wallet update unspent: add unspent 1 fail");
                    return false;
                }
            }
        }
    }
    {
        CWalletTxLinkSetBySequenceNumber::iterator it = idxTx.begin();
        for (; it != idxTx.end(); ++it)
        {
            const CCheckWalletTx& wtx = *(it->ptx);
            if (wtx.IsFromMe())
            {
                for (int n = 0; n < wtx.vInput.size(); n++)
                {
                    if (!AddWalletSpent(wtx.vInput[n].prevout, wtx.txid, wtx.sendTo))
                    {
                        StdError("check", "Wallet update unspent: spent fail");
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

bool CCheckWalletForkUnspent::AddWalletSpent(const CTxOutPoint& txPoint, const uint256& txidSpent, const CDestination& sendTo)
{
    map<CTxOutPoint, CCheckTxOut>::iterator it = mapWalletUnspent.find(txPoint);
    if (it == mapWalletUnspent.end())
    {
        StdError("check", "AddWalletSpent find fail, utxo: [%d] %s.", txPoint.n, txPoint.hash.GetHex().c_str());
        return false;
    }
    if (it->second.IsSpent())
    {
        StdError("check", "AddWalletSpent spent, utxo: [%d] %s.", txPoint.n, txPoint.hash.GetHex().c_str());
        return false;
    }
    it->second.SetSpent(txidSpent, sendTo);
    return true;
}

bool CCheckWalletForkUnspent::AddWalletUnspent(const CTxOutPoint& txPoint, const CTxOut& txOut)
{
    if (!txOut.IsNull())
    {
        if (!mapWalletUnspent.insert(make_pair(txPoint, CCheckTxOut(txOut))).second)
        {
            StdError("check", "insert wallet unspent fail, utxo: [%d] %s.", txPoint.n, txPoint.hash.GetHex().c_str());
            return false;
        }
    }
    return true;
}

int CCheckWalletForkUnspent::GetTxAtBlockHeight(const uint256& txid)
{
    map<uint256, CCheckWalletTx>::iterator it = mapWalletTx.find(txid);
    if (it == mapWalletTx.end())
    {
        return -1;
    }
    return it->second.nBlockHeight;
}

bool CCheckWalletForkUnspent::CheckWalletUnspent(const CTxOutPoint& point, const CCheckTxOut& out)
{
    map<CTxOutPoint, CCheckTxOut>::iterator mt = mapWalletUnspent.find(point);
    if (mt == mapWalletUnspent.end())
    {
        StdLog("check", "CheckWalletUnspent: find unspent fail, utxo: [%d] %s.", point.n, point.hash.GetHex().c_str());
        return false;
    }
    if (mt->second.IsSpent())
    {
        StdLog("check", "CheckWalletUnspent: spented, utxo: [%d] %s.", point.n, point.hash.GetHex().c_str());
        return false;
    }
    if (mt->second != out)
    {
        StdLog("check", "CheckWalletUnspent: out error, utxo: [%d] %s.", point.n, point.hash.GetHex().c_str());
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckDelegateDB

bool CCheckDelegateDB::CheckDelegate(const uint256& hashBlock)
{
    CDelegateContext ctxtDelegate;
    return Retrieve(hashBlock, ctxtDelegate);
}

bool CCheckDelegateDB::UpdateDelegate(const uint256& hashBlock, CBlockEx& block, uint32 nBlockFile, uint32 nBlockOffset)
{
    if (block.IsGenesis())
    {
        CDelegateContext ctxtDelegate;
        if (!AddNew(hashBlock, ctxtDelegate))
        {
            StdTrace("check", "Update genesis delegate fail, block: %s", hashBlock.ToString().c_str());
            return false;
        }
        return true;
    }

    CDelegateContext ctxtDelegate;
    map<CDestination, int64>& mapDelegate = ctxtDelegate.mapVote;
    map<int, map<CDestination, CDiskPos>>& mapEnrollTx = ctxtDelegate.mapEnrollTx;
    if (!RetrieveDelegatedVote(block.hashPrev, mapDelegate))
    {
        StdError("check", "Update delegate vote: RetrieveDelegatedVote fail, hashPrev: %s", block.hashPrev.GetHex().c_str());
        return false;
    }

    {
        CTemplateId tid;
        if (block.txMint.sendTo.GetTemplateId(tid) && tid.GetType() == TEMPLATE_DELEGATE)
        {
            mapDelegate[block.txMint.sendTo] += block.txMint.nAmount;
        }
    }

    CBufStream ss;
    CVarInt var(block.vtx.size());
    uint32 nOffset = nBlockOffset + block.GetTxSerializedOffset()
                     + ss.GetSerializeSize(block.txMint)
                     + ss.GetSerializeSize(var);
    for (int i = 0; i < block.vtx.size(); i++)
    {
        CTransaction& tx = block.vtx[i];
        CDestination destInDelegateTemplate;
        CDestination sendToDelegateTemplate;
        CTxContxt& txContxt = block.vTxContxt[i];
        if (!CTemplate::ParseDelegateDest(txContxt.destIn, tx.sendTo, tx.vchSig, destInDelegateTemplate, sendToDelegateTemplate))
        {
            StdLog("check", "Update delegate vote: parse delegate dest fail, destIn: %s, sendTo: %s, block: %s, txid: %s",
                   CAddress(txContxt.destIn).ToString().c_str(), CAddress(tx.sendTo).ToString().c_str(), hashBlock.GetHex().c_str(), tx.GetHash().GetHex().c_str());
            return false;
        }
        if (!sendToDelegateTemplate.IsNull())
        {
            mapDelegate[sendToDelegateTemplate] += tx.nAmount;
        }
        if (!destInDelegateTemplate.IsNull())
        {
            mapDelegate[destInDelegateTemplate] -= (tx.nAmount + tx.nTxFee);
        }
        if (tx.nType == CTransaction::TX_CERT)
        {
            if (destInDelegateTemplate.IsNull())
            {
                StdLog("check", "Update delegate vote: TX_CERT destInDelegate is null, destInDelegate: %s, block: %s, txid: %s",
                       CAddress(destInDelegateTemplate).ToString().c_str(), hashBlock.GetHex().c_str(), tx.GetHash().GetHex().c_str());
                return false;
            }
            int nCertAnchorHeight = 0;
            try
            {
                CIDataStream is(tx.vchData);
                is >> nCertAnchorHeight;
            }
            catch (...)
            {
                StdLog("check", "Update delegate vote: TX_CERT vchData error, destInDelegate: %s, block: %s, txid: %s",
                       CAddress(destInDelegateTemplate).ToString().c_str(), hashBlock.GetHex().c_str(), tx.GetHash().GetHex().c_str());
                return false;
            }
            mapEnrollTx[nCertAnchorHeight].insert(make_pair(destInDelegateTemplate, CDiskPos(nBlockFile, nOffset)));
        }
        nOffset += ss.GetSerializeSize(tx);
    }

    if (!AddNew(hashBlock, ctxtDelegate))
    {
        StdError("check", "Update delegate context failed, block: %s", hashBlock.ToString().c_str());
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckWalletTxWalker

bool CCheckWalletTxWalker::Walk(const CWalletTx& wtx)
{
    if (wtx.hashFork == 0)
    {
        StdError("check", "Wallet Walk: fork is 0.");
        return false;
    }
    return AddWalletTx(wtx);
}

bool CCheckWalletTxWalker::Exist(const uint256& hashFork, const uint256& txid)
{
    if (hashFork == 0)
    {
        StdError("check", "Wallet Exist: hashFork is 0.");
        return false;
    }
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
    if (it == mapWalletFork.end())
    {
        StdError("check", "Wallet Exist: hashFork find fail, fork: %s.", hashFork.GetHex().c_str());
        return false;
    }
    return it->second.LocalTxExist(txid);
}

CCheckWalletTx* CCheckWalletTxWalker::GetWalletTx(const uint256& hashFork, const uint256& txid)
{
    if (hashFork == 0)
    {
        StdError("check", "Wallet get tx: hashFork is 0.");
        return nullptr;
    }
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
    if (it == mapWalletFork.end())
    {
        StdError("check", "Wallet get tx: hashFork find fail, fork: %s.", hashFork.GetHex().c_str());
        return nullptr;
    }
    return it->second.GetLocalWalletTx(txid);
}

bool CCheckWalletTxWalker::AddWalletTx(const CWalletTx& wtx)
{
    if (pForkManager == nullptr)
    {
        StdError("check", "Wallet add tx: pForkManager is null.");
        return false;
    }
    if (wtx.hashFork == 0)
    {
        StdError("check", "Wallet add tx: wtx fork is 0.");
        return false;
    }

    vector<uint256> vFork;
    pForkManager->GetTxFork(wtx.hashFork, wtx.nBlockHeight, vFork);
    for (const uint256& hashFork : vFork)
    {
        map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
        if (it == mapWalletFork.end())
        {
            it = mapWalletFork.insert(make_pair(hashFork, CCheckWalletForkUnspent(hashFork))).first;
            if (it == mapWalletFork.end())
            {
                StdError("check", "Wallet add tx: add fork fail.");
                return false;
            }
        }
        if (!it->second.AddTx(wtx))
        {
            StdError("check", "Wallet add tx: add fail.");
            return false;
        }
    }

    nWalletTxCount++;
    return true;
}

void CCheckWalletTxWalker::RemoveWalletTx(const uint256& hashFork, int nHeight, const uint256& txid)
{
    if (hashFork == 0)
    {
        StdError("check", "Wallet remove tx: hashFork is 0.");
        return;
    }

    vector<uint256> vFork;
    pForkManager->GetTxFork(hashFork, nHeight, vFork);
    for (const uint256& hash : vFork)
    {
        map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hash);
        if (it == mapWalletFork.end())
        {
            StdError("check", "Wallet remove tx: fork find fail, fork: %s.", hash.GetHex().c_str());
        }
        else
        {
            it->second.RemoveTx(txid);
        }
    }

    nWalletTxCount--;
}

bool CCheckWalletTxWalker::UpdateUnspent()
{
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.begin();
    for (; it != mapWalletFork.end(); ++it)
    {
        if (!it->second.UpdateUnspent())
        {
            StdError("check", "Wallet update unspent fail, fork: %s.", it->first.GetHex().c_str());
            return false;
        }
    }
    return true;
}

int CCheckWalletTxWalker::GetTxAtBlockHeight(const uint256& hashFork, const uint256& txid)
{
    if (hashFork == 0)
    {
        StdError("check", "Wallet get block height: hashFork is 0.");
        return -1;
    }
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
    if (it == mapWalletFork.end())
    {
        StdError("check", "Wallet get block height: fork find fail, fork: %s.", hashFork.GetHex().c_str());
        return -1;
    }
    return it->second.GetTxAtBlockHeight(txid);
}

bool CCheckWalletTxWalker::CheckWalletUnspent(const uint256& hashFork, const CTxOutPoint& point, const CCheckTxOut& out)
{
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
    if (it == mapWalletFork.end())
    {
        StdError("check", "Wallet check unspent: fork find fail, fork: %s.", hashFork.GetHex().c_str());
        return false;
    }
    return it->second.CheckWalletUnspent(point, out);
}

/////////////////////////////////////////////////////////////////////////
// CCheckForkTxPool

bool CCheckForkTxPool::AddTx(const uint256& txid, const CAssembledTx& tx)
{
    vTx.push_back(make_pair(txid, tx));

    if (!mapTxPoolTx.insert(make_pair(txid, tx)).second)
    {
        StdError("check", "TxPool AddTx: Add tx failed, txid: %s.", txid.GetHex().c_str());
        return false;
    }
    for (int i = 0; i < tx.vInput.size(); i++)
    {
        if (!Spent(tx.vInput[i].prevout, txid, tx.sendTo))
        {
            StdError("check", "TxPool AddTx: Spent fail, txid: %s.", txid.GetHex().c_str());
            return false;
        }
    }
    if (!Unspent(CTxOutPoint(txid, 0), tx.GetOutput(0)))
    {
        StdError("check", "TxPool AddTx: Add unspent 0 fail, txid: %s.", txid.GetHex().c_str());
        return false;
    }
    if (!tx.IsMintTx())
    {
        if (!Unspent(CTxOutPoint(txid, 1), tx.GetOutput(1)))
        {
            StdError("check", "TxPool AddTx: Add unspent 1 fail, txid: %s.", txid.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckForkTxPool::Spent(const CTxOutPoint& point, const uint256& txidSpent, const CDestination& sendTo)
{
    map<CTxOutPoint, CCheckTxOut>::iterator it = mapTxPoolUnspent.find(point);
    if (it == mapTxPoolUnspent.end())
    {
        StdError("check", "TxPool Spent: find fail, utxo: [%d] %s", point.n, point.hash.GetHex().c_str());
        return false;
    }
    if (it->second.IsSpent())
    {
        StdError("check", "TxPool Spent: spented, utxo: [%d] %s", point.n, point.hash.GetHex().c_str());
        return false;
    }
    it->second.SetSpent(txidSpent, sendTo);
    return true;
}

bool CCheckForkTxPool::Unspent(const CTxOutPoint& point, const CTxOut& out)
{
    if (!out.IsNull())
    {
        if (!mapTxPoolUnspent.insert(make_pair(point, CCheckTxOut(out))).second)
        {
            StdError("check", "TxPool Unspent: insert unspent fail, utxo: [%d] %s.", point.n, point.hash.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckForkTxPool::GetWalletTx(const uint256& hashFork, const set<CDestination>& setAddress, vector<CWalletTx>& vWalletTx)
{
    for (int i = 0; i < vTx.size(); i++)
    {
        const CAssembledTx& atx = vTx[i].second;
        bool fIsMine = (setAddress.find(atx.sendTo) != setAddress.end());
        bool fFromMe = (setAddress.find(atx.destIn) != setAddress.end());
        if (fIsMine || fFromMe)
        {
            CWalletTx wtx(vTx[i].first, atx, hashFork, fIsMine, fFromMe);
            vWalletTx.push_back(wtx);
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckTxPoolData

bool CCheckTxPoolData::AddForkUnspent(const uint256& hashFork, const map<CTxOutPoint, CCheckTxOut>& mapUnspent)
{
    if (hashFork == 0)
    {
        StdLog("check", "TxPool: AddForkUnspent hashFork is 0");
        return false;
    }
    mapForkTxPool[hashFork].AddBlockUnspentList(mapUnspent);
    return true;
}

bool CCheckTxPoolData::FetchTxPool(const string& strPath)
{
    CTxPoolData datTxPool;
    if (!datTxPool.Initialize(path(strPath)))
    {
        StdLog("check", "TxPool: Failed to initialize txpool data");
        return false;
    }

    vector<pair<uint256, pair<uint256, CAssembledTx>>> vTx;
    if (!datTxPool.LoadCheck(vTx))
    {
        StdLog("check", "TxPool: Load txpool data failed");
        return false;
    }

    for (int i = 0; i < vTx.size(); i++)
    {
        if (vTx[i].first == 0)
        {
            StdError("check", "TxPool: tx fork hash is 0, txid: %s", vTx[i].second.first.GetHex().c_str());
            return false;
        }
        mapForkTxPool[vTx[i].first].AddTx(vTx[i].second.first, vTx[i].second.second);
    }
    return true;
}

bool CCheckTxPoolData::CheckTxExist(const uint256& hashFork, const uint256& txid)
{
    map<uint256, CCheckForkTxPool>::iterator it = mapForkTxPool.find(hashFork);
    if (it == mapForkTxPool.end())
    {
        return false;
    }
    return it->second.CheckTxExist(txid);
}

bool CCheckTxPoolData::CheckTxPoolUnspent(const uint256& hashFork, const CTxOutPoint& point, const CCheckTxOut& out)
{
    map<uint256, CCheckForkTxPool>::iterator it = mapForkTxPool.find(hashFork);
    if (it == mapForkTxPool.end())
    {
        return false;
    }
    return it->second.CheckTxPoolUnspent(point, out);
}

bool CCheckTxPoolData::GetTxPoolWalletTx(const set<CDestination>& setAddress, vector<CWalletTx>& vWalletTx)
{
    map<uint256, CCheckForkTxPool>::iterator it = mapForkTxPool.begin();
    for (; it != mapForkTxPool.end(); ++it)
    {
        if (!it->second.GetWalletTx(it->first, setAddress, vWalletTx))
        {
            return false;
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckBlockFork

void CCheckBlockFork::UpdateMaxTrust(CBlockIndex* pBlockIndex)
{
    if (pBlockIndex->IsOrigin())
    {
        if (pOrigin != nullptr)
        {
            StdLog("check", "CCheckBlockFork pOrigin is not NULL");
            return;
        }
        pOrigin = pBlockIndex;
    }
    if (pBlockIndex->nChainTrust > nMaxChainTrust
        || (pBlockIndex->nChainTrust == nMaxChainTrust && pBlockIndex->nTimeStamp > nMaxTrustTimeStamp))
    {
        nMaxChainTrust = pBlockIndex->nChainTrust;
        nMaxTrustHeight = pBlockIndex->nHeight;
        nMaxTrustTimeStamp = pBlockIndex->nTimeStamp;
        hashMaxTrustBlock = pBlockIndex->GetBlockHash();
    }
}

bool CCheckBlockFork::AddBlockTx(const CTransaction& txIn, const CTxContxt& contxtIn, int nHeight, uint32 nFileNoIn, uint32 nOffsetIn)
{
    const uint256 txid = txIn.GetHash();
    map<uint256, CCheckBlockTx>::iterator mt = mapBlockTx.insert(make_pair(txid, CCheckBlockTx(txIn, contxtIn, nHeight, nFileNoIn, nOffsetIn))).first;
    if (mt == mapBlockTx.end())
    {
        StdLog("check", "AddBlockTx: add block tx fail, txid: %s.", txid.GetHex().c_str());
        return false;
    }
    for (int i = 0; i < txIn.vInput.size(); i++)
    {
        if (!AddBlockSpent(txIn.vInput[i].prevout, txid, txIn.sendTo))
        {
            StdLog("check", "AddBlockTx: block spent fail, txid: %s.", txid.GetHex().c_str());
            return false;
        }
    }
    if (!AddBlockUnspent(CTxOutPoint(txid, 0), CTxOut(txIn)))
    {
        StdLog("check", "AddBlockTx: add block unspent 0 fail, txid: %s.", txid.GetHex().c_str());
        return false;
    }
    if (!txIn.IsMintTx())
    {
        if (!AddBlockUnspent(CTxOutPoint(txid, 1), CTxOut(txIn, contxtIn.destIn, contxtIn.GetValueIn())))
        {
            StdLog("check", "AddBlockTx: add block unspent 1 fail, txid: %s.", txid.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckBlockFork::AddBlockSpent(const CTxOutPoint& txPoint, const uint256& txidSpent, const CDestination& sendTo)
{
    map<CTxOutPoint, CCheckTxOut>::iterator it = mapBlockUnspent.find(txPoint);
    if (it == mapBlockUnspent.end())
    {
        StdLog("check", "AddBlockSpent: utxo find fail, utxo: [%d] %s.", txPoint.n, txPoint.hash.GetHex().c_str());
        return false;
    }
    if (it->second.IsSpent())
    {
        StdLog("check", "AddBlockSpent: utxo spented, utxo: [%d] %s.", txPoint.n, txPoint.hash.GetHex().c_str());
        return false;
    }
    it->second.SetSpent(txidSpent, sendTo);
    return true;
}

bool CCheckBlockFork::AddBlockUnspent(const CTxOutPoint& txPoint, const CTxOut& txOut)
{
    if (!txOut.IsNull())
    {
        if (!mapBlockUnspent.insert(make_pair(txPoint, CCheckTxOut(txOut))).second)
        {
            StdLog("check", "AddBlockUnspent: Add block unspent fail, utxo: [%d] %s.", txPoint.n, txPoint.hash.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckBlockFork::CheckTxExist(const uint256& txid, int& nHeight)
{
    map<uint256, CCheckBlockTx>::iterator it = mapBlockTx.find(txid);
    if (it == mapBlockTx.end())
    {
        StdLog("check", "CheckBlockFork: Check tx exist, find tx fail, txid: %s", txid.GetHex().c_str());
        return false;
    }
    nHeight = it->second.txIndex.nBlockHeight;
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckBlockWalker

CCheckBlockWalker::~CCheckBlockWalker()
{
    ClearBlockIndex();
    dbBlockIndex.Deinitialize();
}

bool CCheckBlockWalker::Initialize(const string& strPath)
{
    if (!objDelegateDB.Initialize(path(strPath)))
    {
        StdError("check", "Block walker: Delegate db initialize fail, path: %s.", strPath.c_str());
        return false;
    }

    if (!objTsBlock.Initialize(path(strPath) / "block", BLOCKFILE_PREFIX))
    {
        StdError("check", "Block walker: TsBlock initialize fail, path: %s.", strPath.c_str());
        return false;
    }

    if (!dbBlockIndex.Initialize(path(strPath)))
    {
        StdLog("check", "dbBlockIndex Initialize fail");
        return false;
    }
    dbBlockIndex.WalkThroughBlock(objBlockIndexWalker);
    StdLog("check", "Fetch block index success, count: %ld", objBlockIndexWalker.mapBlockIndex.size());
    return true;
}

bool CCheckBlockWalker::Walk(const CBlockEx& block, uint32 nFile, uint32 nOffset)
{
    const uint256 hashBlock = block.GetHash();
    if (mapBlock.count(hashBlock) > 0)
    {
        StdError("check", "Block walk: block exist, hash: %s.", hashBlock.GetHex().c_str());
        return true;
    }

    CBlockEx* pPrevBlock = nullptr;
    CBlockIndex* pIndexPrev = nullptr;
    if (block.hashPrev != 0)
    {
        map<uint256, CBlockEx>::iterator it = mapBlock.find(block.hashPrev);
        if (it != mapBlock.end())
        {
            pPrevBlock = &(it->second);
        }
        map<uint256, CBlockIndex*>::iterator mt = mapBlockIndex.find(block.hashPrev);
        if (mt != mapBlockIndex.end())
        {
            pIndexPrev = mt->second;
        }
    }
    if (!block.IsGenesis() && pPrevBlock == nullptr)
    {
        StdError("check", "Block walk: Prev block not exist, block: %s.", hashBlock.GetHex().c_str());
        return false;
    }

    map<uint256, CBlockEx>::iterator mt = mapBlock.insert(make_pair(hashBlock, block)).first;
    if (mt == mapBlock.end())
    {
        StdError("check", "Block walk: Insert block fail, block: %s.", hashBlock.GetHex().c_str());
        return false;
    }
    CBlockEx& checkBlock = mt->second;

    if (block.IsPrimary() && !objDelegateDB.CheckDelegate(hashBlock))
    {
        StdError("check", "Block walk: Check delegate vote fail, block: %s", hashBlock.GetHex().c_str());
        if (!fOnlyCheck)
        {
            if (!objDelegateDB.UpdateDelegate(hashBlock, checkBlock, nFile, nOffset))
            {
                StdError("check", "Block walk: Update delegate fail, block: %s.", hashBlock.GetHex().c_str());
                return false;
            }
        }
    }

    CBlockIndex* pNewBlockIndex = nullptr;
    CBlockOutline* pBlockOutline = objBlockIndexWalker.GetBlockOutline(hashBlock);
    if (pBlockOutline == nullptr)
    {
        uint256 nChainTrust;
        if (pIndexPrev != nullptr && !(block.IsOrigin() || block.IsVacant() || block.IsNull()))
        {
            if (block.IsProofOfWork())
            {
                if (!GetBlockTrust(block, nChainTrust))
                {
                    StdError("check", "Block walk: Get block trust fail 1, block: %s.", hashBlock.GetHex().c_str());
                    return false;
                }
            }
            else
            {
                CBlockIndex* pIndexRef = nullptr;
                CDelegateAgreement agreement;
                size_t nEnrollTrust = 0;
                if (block.IsPrimary())
                {
                    if (!GetBlockDelegateAgreement(hashBlock, checkBlock, pIndexPrev, agreement, nEnrollTrust))
                    {
                        StdError("check", "Block walk: GetBlockDelegateAgreement fail, block: %s.", hashBlock.GetHex().c_str());
                        return false;
                    }
                }
                else if (block.IsSubsidiary() || block.IsExtended())
                {
                    CProofOfPiggyback proof;
                    proof.Load(block.vchProof);
                    if (proof.hashRefBlock != 0)
                    {
                        map<uint256, CBlockIndex*>::iterator mt = mapBlockIndex.find(proof.hashRefBlock);
                        if (mt != mapBlockIndex.end())
                        {
                            pIndexRef = mt->second;
                        }
                    }
                    if (pIndexRef == nullptr)
                    {
                        StdError("check", "Block walk: refblock find fail, refblock: %s, block: %s.", proof.hashRefBlock.GetHex().c_str(), hashBlock.GetHex().c_str());
                        return false;
                    }
                    if (pIndexRef->pPrev == nullptr)
                    {
                        StdError("check", "Block walk: pIndexRef->pPrev is null, refblock: %s, block: %s.", proof.hashRefBlock.GetHex().c_str(), hashBlock.GetHex().c_str());
                        return false;
                    }
                }
                if (!GetBlockTrust(block, nChainTrust, pIndexPrev, agreement, pIndexRef, nEnrollTrust))
                {
                    StdError("check", "Block walk: Get block trust fail 2, block: %s.", hashBlock.GetHex().c_str());
                    return false;
                }
            }
        }
        pNewBlockIndex = AddNewIndex(hashBlock, static_cast<const CBlock&>(checkBlock), nFile, nOffset, nChainTrust);
        if (pNewBlockIndex == nullptr)
        {
            StdError("check", "Block walk: Add new block index fail 1, block: %s.", hashBlock.GetHex().c_str());
            return false;
        }
    }
    else
    {
        pNewBlockIndex = AddNewIndex(hashBlock, *pBlockOutline);
        if (pNewBlockIndex == nullptr)
        {
            StdError("check", "Block walk: Add new block index fail 2, block: %s.", hashBlock.GetHex().c_str());
            return false;
        }
        if (pNewBlockIndex->nFile != nFile || pNewBlockIndex->nOffset != nOffset)
        {
            StdLog("check", "Block walk: Block index param error, block: %s, File: %d - %d, Offset: %d - %d.",
                   hashBlock.GetHex().c_str(), pNewBlockIndex->nFile, nFile, pNewBlockIndex->nOffset, nOffset);
            pNewBlockIndex->nFile = nFile;
            pNewBlockIndex->nOffset = nOffset;
        }
    }
    if (pNewBlockIndex->GetOriginHash() == 0)
    {
        StdError("check", "Block walk: Get block origin hash is 0, block: %s.", hashBlock.GetHex().c_str());
        return false;
    }
    mapCheckFork[pNewBlockIndex->GetOriginHash()].UpdateMaxTrust(pNewBlockIndex);

    if (block.IsGenesis())
    {
        if (hashGenesis != 0)
        {
            StdError("check", "Block walk: more genesis block, block hash: %s, hashGenesis: %s.",
                     hashBlock.GetHex().c_str(), hashGenesis.GetHex().c_str());
            return false;
        }
        hashGenesis = hashBlock;
    }

    nBlockCount++;

    return true;
}

bool CCheckBlockWalker::GetBlockTrust(const CBlockEx& block, uint256& nChainTrust, const CBlockIndex* pIndexPrev, const CDelegateAgreement& agreement, const CBlockIndex* pIndexRef, size_t nEnrollTrust)
{
    if (block.IsGenesis())
    {
        nChainTrust = uint64(0);
    }
    else if (block.IsVacant())
    {
        nChainTrust = uint64(0);
    }
    else if (block.IsPrimary())
    {
        if (block.IsProofOfWork())
        {
            // PoW difficulty = 2 ^ nBits
            CProofOfHashWorkCompact proof;
            proof.Load(block.vchProof);
            uint256 v(1);
            nChainTrust = v << proof.nBits;
        }
        else if (pIndexPrev != nullptr)
        {
            if (!objProofParam.IsDposHeight(block.GetBlockHeight()))
            {
                StdError("check", "GetBlockTrust: not dpos height, height: %d", block.GetBlockHeight());
                return false;
            }

            // Get the last PoW block nAlgo
            int nAlgo;
            const CBlockIndex* pIndex = pIndexPrev;
            while (!pIndex->IsProofOfWork() && (pIndex->pPrev != nullptr))
            {
                pIndex = pIndex->pPrev;
            }
            if (!pIndex->IsProofOfWork())
            {
                nAlgo = CM_CRYPTONIGHT;
            }
            else
            {
                nAlgo = pIndex->nProofAlgo;
            }

            // DPoS difficulty = weight * (2 ^ nBits)
            int nBits;
            if (GetProofOfWorkTarget(pIndexPrev, nAlgo, nBits))
            {
                if (agreement.nWeight == 0 || nBits <= 0)
                {
                    StdError("check", "GetBlockTrust: nWeight or nBits error, nWeight: %lu, nBits: %d", agreement.nWeight, nBits);
                    return false;
                }
                if (nEnrollTrust <= 0)
                {
                    StdError("check", "GetBlockTrust: nEnrollTrust error, nEnrollTrust: %lu", nEnrollTrust);
                    return false;
                }
                nChainTrust = uint256(uint64(nEnrollTrust)) << nBits;
            }
            else
            {
                StdError("check", "GetBlockTrust: GetProofOfWorkTarget fail");
                return false;
            }
        }
        else
        {
            StdError("check", "GetBlockTrust: Primary pIndexPrev is null");
            return false;
        }
    }
    else if (block.IsOrigin())
    {
        nChainTrust = uint64(0);
    }
    else if ((block.IsSubsidiary() || block.IsExtended()) && (pIndexRef != nullptr))
    {
        if (pIndexRef->pPrev == nullptr)
        {
            StdError("check", "GetBlockTrust: Subsidiary or Extended block pPrev is null, block: %s", block.GetHash().GetHex().c_str());
            return false;
        }
        nChainTrust = pIndexRef->nChainTrust - pIndexRef->pPrev->nChainTrust;
    }
    else
    {
        StdError("check", "GetBlockTrust: block type error");
        return false;
    }
    return true;
}

bool CCheckBlockWalker::GetProofOfWorkTarget(const CBlockIndex* pIndexPrev, int nAlgo, int& nBits)
{
    if (nAlgo <= 0 || nAlgo >= CM_MAX || !pIndexPrev->IsPrimary())
    {
        return false;
    }

    const CBlockIndex* pIndex = pIndexPrev;
    while ((!pIndex->IsProofOfWork() || pIndex->nProofAlgo != nAlgo) && pIndex->pPrev != nullptr)
    {
        pIndex = pIndex->pPrev;
    }

    // first
    if (!pIndex->IsProofOfWork())
    {
        nBits = objProofParam.nProofOfWorkInit;
        return true;
    }

    nBits = pIndex->nProofBits;
    int64 nSpacing = 0;
    int64 nWeight = 0;
    int nWIndex = objProofParam.nProofOfWorkAdjustCount - 1;
    while (pIndex->IsProofOfWork())
    {
        nSpacing += (pIndex->GetBlockTime() - pIndex->pPrev->GetBlockTime()) << nWIndex;
        nWeight += (1ULL) << nWIndex;
        if (!nWIndex--)
        {
            break;
        }
        pIndex = pIndex->pPrev;
        while ((!pIndex->IsProofOfWork() || pIndex->nProofAlgo != nAlgo) && pIndex->pPrev != nullptr)
        {
            pIndex = pIndex->pPrev;
        }
    }
    nSpacing /= nWeight;
    if (objProofParam.IsDposHeight(pIndexPrev->GetBlockHeight() + 1))
    {
        if (nSpacing > objProofParam.nProofOfWorkUpperTargetOfDpos && nBits > objProofParam.nProofOfWorkLowerLimit)
        {
            nBits--;
        }
        else if (nSpacing < objProofParam.nProofOfWorkLowerTargetOfDpos && nBits < objProofParam.nProofOfWorkUpperLimit)
        {
            nBits++;
        }
    }
    else
    {
        if (nSpacing > objProofParam.nProofOfWorkUpperTarget && nBits > objProofParam.nProofOfWorkLowerLimit)
        {
            nBits--;
        }
        else if (nSpacing < objProofParam.nProofOfWorkLowerTarget && nBits < objProofParam.nProofOfWorkUpperLimit)
        {
            nBits++;
        }
    }
    return true;
}

bool CCheckBlockWalker::GetBlockDelegateAgreement(const uint256& hashBlock, const CBlock& block, CBlockIndex* pIndexPrev, CDelegateAgreement& agreement, size_t& nEnrollTrust)
{
    agreement.Clear();
    if (pIndexPrev->GetBlockHeight() < CONSENSUS_INTERVAL - 1)
    {
        return true;
    }

    CBlockIndex* pIndex = pIndexPrev;
    for (int i = 0; i < CONSENSUS_DISTRIBUTE_INTERVAL; i++)
    {
        if (pIndex == nullptr)
        {
            StdLog("check", "GetBlockDelegateAgreement : pIndex is null, block: %s", hashBlock.ToString().c_str());
            return false;
        }
        pIndex = pIndex->pPrev;
    }

    CDelegateEnrolled enrolled;
    if (!GetBlockDelegateEnrolled(pIndex->GetBlockHash(), pIndex, enrolled))
    {
        StdLog("check", "GetBlockDelegateAgreement : GetBlockDelegateEnrolled fail, block: %s", hashBlock.ToString().c_str());
        return false;
    }

    delegate::CDelegateVerify verifier(enrolled.mapWeight, enrolled.mapEnrollData);
    map<CDestination, size_t> mapBallot;
    if (!verifier.VerifyProof(block.vchProof, agreement.nAgreement, agreement.nWeight, mapBallot))
    {
        StdLog("check", "GetBlockDelegateAgreement : Invalid block proof, block: %s", hashBlock.ToString().c_str());
        return false;
    }

    nEnrollTrust = 0;
    for (auto& amount : enrolled.vecAmount)
    {
        if (mapBallot.find(amount.first) != mapBallot.end())
        {
            nEnrollTrust += (size_t)(min(amount.second, objProofParam.nDelegateProofOfStakeEnrollMaximumAmount));
        }
    }
    nEnrollTrust /= objProofParam.nDelegateProofOfStakeEnrollMinimumAmount;
    return true;
}

bool CCheckBlockWalker::GetBlockDelegateEnrolled(const uint256& hashBlock, CBlockIndex* pIndex, CDelegateEnrolled& enrolled)
{
    enrolled.Clear();
    if (pIndex == nullptr)
    {
        StdLog("check", "GetBlockDelegateEnrolled : pIndex is null, block: %s", hashBlock.ToString().c_str());
        return false;
    }

    if (pIndex->GetBlockHeight() < CONSENSUS_ENROLL_INTERVAL)
    {
        return true;
    }

    vector<uint256> vBlockRange;
    for (int i = 0; i < CONSENSUS_ENROLL_INTERVAL; i++)
    {
        if (pIndex == nullptr)
        {
            StdLog("check", "GetBlockDelegateEnrolled : pIndex is null 2, block: %s", hashBlock.ToString().c_str());
            return false;
        }
        vBlockRange.push_back(pIndex->GetBlockHash());
        pIndex = pIndex->pPrev;
    }

    if (!RetrieveAvailDelegate(hashBlock, pIndex->GetBlockHeight(), vBlockRange, objProofParam.nDelegateProofOfStakeEnrollMinimumAmount,
                               enrolled.mapWeight, enrolled.mapEnrollData, enrolled.vecAmount))
    {
        StdLog("check", "GetBlockDelegateEnrolled : Retrieve Avail Delegate Error, block: %s", hashBlock.ToString().c_str());
        return false;
    }
    return true;
}

bool CCheckBlockWalker::RetrieveAvailDelegate(const uint256& hash, int height, const vector<uint256>& vBlockRange,
                                              int64 nMinEnrollAmount,
                                              map<CDestination, size_t>& mapWeight,
                                              map<CDestination, vector<unsigned char>>& mapEnrollData,
                                              vector<pair<CDestination, int64>>& vecAmount)
{
    map<CDestination, int64> mapVote;
    if (!objDelegateDB.RetrieveDelegatedVote(hash, mapVote))
    {
        StdLog("check", "RetrieveAvailDelegate: RetrieveDelegatedVote fail, block: %s", hash.ToString().c_str());
        return false;
    }

    map<CDestination, CDiskPos> mapEnrollTxPos;
    if (!objDelegateDB.RetrieveEnrollTx(height, vBlockRange, mapEnrollTxPos))
    {
        StdLog("check", "RetrieveAvailDelegate: RetrieveEnrollTx fail, block: %s, height: %d", hash.ToString().c_str(), height);
        return false;
    }

    map<pair<int64, CDiskPos>, pair<CDestination, vector<uint8>>> mapSortEnroll;
    for (map<CDestination, int64>::iterator it = mapVote.begin(); it != mapVote.end(); ++it)
    {
        if ((*it).second >= nMinEnrollAmount)
        {
            const CDestination& dest = (*it).first;
            map<CDestination, CDiskPos>::iterator mi = mapEnrollTxPos.find(dest);
            if (mi != mapEnrollTxPos.end())
            {
                CTransaction tx;
                if (!objTsBlock.Read(tx, (*mi).second))
                {
                    StdLog("check", "RetrieveAvailDelegate: Read tx fail, txid: %s", tx.GetHash().ToString().c_str());
                    return false;
                }

                if (tx.vchData.size() <= sizeof(int))
                {
                    StdLog("check", "RetrieveAvailDelegate: tx.vchData error, txid: %s", tx.GetHash().ToString().c_str());
                    return false;
                }
                std::vector<uint8> vchCertData;
                vchCertData.assign(tx.vchData.begin() + sizeof(int), tx.vchData.end());

                mapSortEnroll.insert(make_pair(make_pair(it->second, mi->second), make_pair(dest, vchCertData)));
            }
        }
    }

    // first 23 destination sorted by amount and sequence
    for (auto it = mapSortEnroll.rbegin(); it != mapSortEnroll.rend() && mapWeight.size() < MAX_DELEGATE_THRESH; it++)
    {
        mapWeight.insert(make_pair(it->second.first, 1));
        mapEnrollData.insert(make_pair(it->second.first, it->second.second));
        vecAmount.push_back(make_pair(it->second.first, it->first.first));
    }
    return true;
}

bool CCheckBlockWalker::UpdateBlockNext()
{
    map<uint256, CCheckBlockFork>::iterator it = mapCheckFork.begin();
    while (it != mapCheckFork.end())
    {
        CCheckBlockFork& checkFork = it->second;
        if (checkFork.hashMaxTrustBlock == 0 || checkFork.pOrigin == nullptr)
        {
            if (checkFork.pOrigin == nullptr)
            {
                StdError("check", "UpdateBlockNext: pOrigin is null, fork: %s", it->first.GetHex().c_str());
            }
            else
            {
                StdError("check", "UpdateBlockNext: hashMaxTrustBlock is 0, fork: %s", it->first.GetHex().c_str());
            }
            mapCheckFork.erase(it++);
            continue;
        }
        CBlockIndex* pBlockIndex = mapBlockIndex[checkFork.hashMaxTrustBlock];
        if (pBlockIndex == nullptr)
        {
            StdError("check", "UpdateBlockNext: Last block index is null, fork: %s", it->first.GetHex().c_str());
            mapCheckFork.erase(it++);
            continue;
        }
        checkFork.pLast = pBlockIndex;
        while (pBlockIndex && !pBlockIndex->IsOrigin())
        {
            if (pBlockIndex->pPrev)
            {
                pBlockIndex->pPrev->pNext = pBlockIndex;
            }
            pBlockIndex = pBlockIndex->pPrev;
        }
        if (it->first == hashGenesis)
        {
            nMainChainHeight = checkFork.nMaxTrustHeight;
        }
        ++it;
    }
    return true;
}

bool CCheckBlockWalker::UpdateBlockTx(CCheckForkManager& objForkMn)
{
    if (hashGenesis == 0)
    {
        StdError("check", "UpdateBlockTx: hashGenesis is 0");
        return false;
    }

    vector<uint256> vForkList;
    objForkMn.GetForkList(hashGenesis, vForkList);

    for (const uint256& hashFork : vForkList)
    {
        map<uint256, CCheckBlockFork>::iterator it = mapCheckFork.find(hashFork);
        if (it != mapCheckFork.end())
        {
            CCheckBlockFork& checkFork = it->second;
            if (checkFork.pOrigin == nullptr)
            {
                StdError("check", "UpdateBlockTx: pOrigin is null, fork: %s", it->first.GetHex().c_str());
                continue;
            }
            CBlockIndex* pIndex = checkFork.pOrigin;
            while (pIndex)
            {
                map<uint256, CBlockEx>::iterator it = mapBlock.find(pIndex->GetBlockHash());
                if (it == mapBlock.end())
                {
                    StdError("check", "UpdateBlockTx: Find block fail, block: %s", pIndex->GetBlockHash().GetHex().c_str());
                    return false;
                }
                const CBlockEx& block = it->second;
                if (!(block.IsVacant() || block.IsNull()))
                {
                    vector<uint256> vFork;
                    objForkMn.GetTxFork(hashFork, pIndex->GetBlockHeight(), vFork);

                    CBufStream ss;
                    CTxContxt txContxt;
                    txContxt.destIn = block.txMint.sendTo;
                    uint32 nTxOffset = pIndex->nOffset + block.GetTxSerializedOffset();
                    if (!AddBlockTx(block.txMint, txContxt, block.GetBlockHeight(), pIndex->nFile, nTxOffset, vFork))
                    {
                        StdError("check", "UpdateBlockTx: Add mint tx fail, txid: %s, block: %s",
                                 block.txMint.GetHash().GetHex().c_str(), pIndex->GetBlockHash().GetHex().c_str());
                        return false;
                    }
                    nTxOffset += ss.GetSerializeSize(block.txMint);

                    CVarInt var(block.vtx.size());
                    nTxOffset += ss.GetSerializeSize(var);
                    for (int i = 0; i < block.vtx.size(); i++)
                    {
                        if (!AddBlockTx(block.vtx[i], block.vTxContxt[i], block.GetBlockHeight(), pIndex->nFile, nTxOffset, vFork))
                        {
                            StdError("check", "UpdateBlockTx: Add tx fail, txid: %s, block: %s",
                                     block.vtx[i].GetHash().GetHex().c_str(), pIndex->GetBlockHash().GetHex().c_str());
                            return false;
                        }
                        nTxOffset += ss.GetSerializeSize(block.vtx[i]);
                    }
                }
                pIndex = pIndex->pNext;
            }
            if (it->first == hashGenesis)
            {
                nMainChainTxCount = checkFork.mapBlockTx.size();
            }
        }
    }
    return true;
}

bool CCheckBlockWalker::AddBlockTx(const CTransaction& txIn, const CTxContxt& contxtIn, int nHeight, uint32 nFileNoIn, uint32 nOffsetIn, const vector<uint256>& vFork)
{
    for (const uint256& hashFork : vFork)
    {
        map<uint256, CCheckBlockFork>::iterator it = mapCheckFork.find(hashFork);
        if (it != mapCheckFork.end())
        {
            if (!it->second.AddBlockTx(txIn, contxtIn, nHeight, nFileNoIn, nOffsetIn))
            {
                StdError("check", "Block add tx: Add fail, txid: %s, fork: %s",
                         txIn.GetHash().GetHex().c_str(), hashFork.GetHex().c_str());
                return false;
            }
        }
    }
    return true;
}

CBlockIndex* CCheckBlockWalker::AddNewIndex(const uint256& hash, const CBlock& block, uint32 nFile, uint32 nOffset, uint256 nChainTrust)
{
    CBlockIndex* pIndexNew = new CBlockIndex(block, nFile, nOffset);
    if (pIndexNew != nullptr)
    {
        map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.insert(make_pair(hash, pIndexNew)).first;
        if (mi == mapBlockIndex.end())
        {
            StdError("check", "AddNewIndex: insert fail, block: %s", hash.GetHex().c_str());
            return nullptr;
        }
        pIndexNew->phashBlock = &((*mi).first);

        int64 nMoneySupply = block.GetBlockMint();
        uint64 nRandBeacon = block.GetBlockBeacon();
        CBlockIndex* pIndexPrev = nullptr;
        map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(block.hashPrev);
        if (miPrev != mapBlockIndex.end())
        {
            pIndexPrev = (*miPrev).second;
            pIndexNew->pPrev = pIndexPrev;
            if (!pIndexNew->IsOrigin())
            {
                pIndexNew->pOrigin = pIndexPrev->pOrigin;
                nRandBeacon ^= pIndexNew->pOrigin->nRandBeacon;
            }
            nMoneySupply += pIndexPrev->nMoneySupply;
            nChainTrust += pIndexPrev->nChainTrust;
        }
        pIndexNew->nMoneySupply = nMoneySupply;
        pIndexNew->nChainTrust = nChainTrust;
        pIndexNew->nRandBeacon = nRandBeacon;
    }
    else
    {
        StdError("check", "AddNewIndex: new fail, block: %s", hash.GetHex().c_str());
    }
    return pIndexNew;
}

CBlockIndex* CCheckBlockWalker::AddNewIndex(const uint256& hash, const CBlockOutline& objBlockOutline)
{
    CBlockIndex* pIndexNew = new CBlockIndex(static_cast<const CBlockIndex&>(objBlockOutline));
    if (pIndexNew != nullptr)
    {
        map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.insert(make_pair(hash, pIndexNew)).first;
        if (mi == mapBlockIndex.end())
        {
            StdError("check", "AddNewIndex: insert fail, block: %s", hash.GetHex().c_str());
            delete pIndexNew;
            return nullptr;
        }
        pIndexNew->phashBlock = &((*mi).first);

        if (objBlockOutline.hashPrev != 0)
        {
            map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(objBlockOutline.hashPrev);
            if (miPrev == mapBlockIndex.end())
            {
                StdError("check", "AddNewIndex: find prev fail, prev: %s", objBlockOutline.hashPrev.GetHex().c_str());
                mapBlockIndex.erase(hash);
                delete pIndexNew;
                return nullptr;
            }
            pIndexNew->pPrev = miPrev->second;
        }

        if (objBlockOutline.hashOrigin != 0)
        {
            map<uint256, CBlockIndex*>::iterator miOri = mapBlockIndex.find(objBlockOutline.hashOrigin);
            if (miOri == mapBlockIndex.end())
            {
                StdError("check", "AddNewIndex: find origin fail, origin: %s", objBlockOutline.hashOrigin.GetHex().c_str());
                mapBlockIndex.erase(hash);
                delete pIndexNew;
                return nullptr;
            }
            pIndexNew->pOrigin = miOri->second;
        }
    }
    else
    {
        StdError("check", "AddNewIndex: new fail, block: %s", hash.GetHex().c_str());
    }
    return pIndexNew;
}

void CCheckBlockWalker::ClearBlockIndex()
{
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.begin();
    for (; mi != mapBlockIndex.end(); ++mi)
    {
        delete (*mi).second;
    }
    mapBlockIndex.clear();
    mapBlock.clear();
}

bool CCheckBlockWalker::CheckTxExist(const uint256& hashFork, const uint256& txid, int& nHeight)
{
    map<uint256, CCheckBlockFork>::iterator it = mapCheckFork.find(hashFork);
    if (it == mapCheckFork.end())
    {
        StdError("check", "CheckTxExist: find fork fail, fork: %s, txid: %s", hashFork.GetHex().c_str(), txid.GetHex().c_str());
        return false;
    }
    return it->second.CheckTxExist(txid, nHeight);
}

bool CCheckBlockWalker::GetBlockWalletTx(const set<CDestination>& setAddress, vector<CWalletTx>& vWalletTx)
{
    map<uint256, CCheckBlockFork>::iterator mt = mapCheckFork.begin();
    for (; mt != mapCheckFork.end(); ++mt)
    {
        const uint256& hashFork = mt->first;
        CCheckBlockFork& checkFork = mt->second;
        if (checkFork.pOrigin == nullptr)
        {
            StdError("check", "GetBlockWalletTx: pOrigin is null, fork: %s", hashFork.GetHex().c_str());
            return false;
        }
        CBlockIndex* pBlockIndex = checkFork.pOrigin;
        while (pBlockIndex)
        {
            map<uint256, CBlockEx>::iterator it = mapBlock.find(pBlockIndex->GetBlockHash());
            if (it == mapBlock.end())
            {
                StdError("check", "GetBlockWalletTx: find block fail, block: %s, fork: %s", pBlockIndex->GetBlockHash().GetHex().c_str(), hashFork.GetHex().c_str());
                return false;
            }
            const CBlockEx& block = it->second;
            for (int i = 0; i < block.vtx.size(); i++)
            {
                //CAssembledTx(const CTransaction& tx, int nBlockHeightIn, const CDestination& destInIn = CDestination(), int64 nValueInIn = 0)
                //CWalletTx(const uint256& txidIn, const CAssembledTx& tx, const uint256& hashForkIn, bool fIsMine, bool fFromMe)

                bool fIsMine = (setAddress.find(block.vtx[i].sendTo) != setAddress.end());
                bool fFromMe = (setAddress.find(block.vTxContxt[i].destIn) != setAddress.end());
                if (fIsMine || fFromMe)
                {
                    CAssembledTx atx(block.vtx[i], block.GetBlockHeight(), block.vTxContxt[i].destIn, block.vTxContxt[i].GetValueIn());
                    CWalletTx wtx(block.vtx[i].GetHash(), atx, hashFork, fIsMine, fFromMe);
                    vWalletTx.push_back(wtx);
                }
            }
            if (!block.txMint.IsNull() && setAddress.find(block.txMint.sendTo) != setAddress.end())
            {
                CAssembledTx atx(block.txMint, block.GetBlockHeight(), CDestination(), 0);
                CWalletTx wtx(block.txMint.GetHash(), atx, hashFork, true, false);
                vWalletTx.push_back(wtx);
            }
            pBlockIndex = pBlockIndex->pNext;
        }
    }
    return true;
}

bool CCheckBlockWalker::CheckBlockIndex()
{
    for (map<uint256, CBlockIndex*>::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); ++it)
    {
        if (!objBlockIndexWalker.CheckBlock(CBlockOutline(it->second)))
        {
            StdLog("check", "CheckBlockIndex: Find block index fail, add block index, block: %s.", it->first.GetHex().c_str());
            if (!fOnlyCheck)
            {
                dbBlockIndex.AddNewBlock(CBlockOutline(it->second));
            }
        }
    }
    for (map<uint256, CBlockOutline>::iterator it = objBlockIndexWalker.mapBlockIndex.begin(); it != objBlockIndexWalker.mapBlockIndex.end(); ++it)
    {
        const CBlockOutline& blockOut = it->second;
        if (mapBlockIndex.find(blockOut.hashBlock) == mapBlockIndex.end())
        {
            StdLog("check", "CheckBlockIndex: Find block hash fail, remove block index, block: %s.", blockOut.hashBlock.GetHex().c_str());
            if (!fOnlyCheck)
            {
                dbBlockIndex.RemoveBlock(blockOut.hashBlock);
            }
        }
    }
    return true;
}
/////////////////////////////////////////////////////////////////////////
// CCheckRepairData

bool CCheckRepairData::FetchBlockData()
{
    CTimeSeriesCached tsBlock;
    if (!tsBlock.Initialize(path(strDataPath) / "block", BLOCKFILE_PREFIX))
    {
        StdError("check", "tsBlock Initialize fail");
        return false;
    }

    if (!objBlockWalker.Initialize(strDataPath))
    {
        StdError("check", "objBlockWalker Initialize fail");
        return false;
    }

    uint32 nLastFileRet = 0;
    uint32 nLastPosRet = 0;

    StdLog("check", "Fetch block and tx......");
    if (!tsBlock.WalkThrough(objBlockWalker, nLastFileRet, nLastPosRet, !fOnlyCheck))
    {
        StdError("check", "Fetch block and tx fail.");
        return false;
    }
    StdLog("check", "Fetch block and tx success, block: %ld.", objBlockWalker.nBlockCount);

    if (objBlockWalker.nBlockCount > 0)
    {
        if (objBlockWalker.hashGenesis == 0)
        {
            StdError("check", "Not genesis block");
            return false;
        }

        StdLog("check", "Update blockchain......");
        if (!objBlockWalker.UpdateBlockNext())
        {
            StdError("check", "Fetch block and tx, update block next fail");
            return false;
        }
        StdLog("check", "Update blockchain success, main chain height: %d.", objBlockWalker.nMainChainHeight);

        StdLog("check", "Update block tx......");
        if (!objBlockWalker.UpdateBlockTx(objForkManager))
        {
            StdError("check", "Fetch block and tx, update block tx fail");
            return false;
        }
        StdLog("check", "Update block tx success, main chain tx count: %ld.", objBlockWalker.nMainChainTxCount);
    }
    return true;
}

bool CCheckRepairData::FetchUnspent()
{
    CUnspentDB dbUnspent;
    if (!dbUnspent.Initialize(path(strDataPath)))
    {
        StdError("check", "FetchUnspent: dbUnspent Initialize fail");
        return false;
    }

    map<uint256, CCheckBlockFork>::iterator it = objBlockWalker.mapCheckFork.begin();
    for (; it != objBlockWalker.mapCheckFork.end(); ++it)
    {
        if (!dbUnspent.AddNewFork(it->first))
        {
            StdError("check", "FetchUnspent: dbUnspent AddNewFork fail.");
            dbUnspent.Deinitialize();
            return false;
        }
        if (!dbUnspent.WalkThrough(it->first, mapForkUnspentWalker[it->first]))
        {
            StdError("check", "FetchUnspent: dbUnspent WalkThrough fail.");
            dbUnspent.Deinitialize();
            return false;
        }
    }

    dbUnspent.Deinitialize();
    return true;
}

bool CCheckRepairData::FetchTxPool()
{
    map<uint256, CCheckBlockFork>::iterator it = objBlockWalker.mapCheckFork.begin();
    for (; it != objBlockWalker.mapCheckFork.end(); ++it)
    {
        if (!objTxPoolData.AddForkUnspent(it->first, it->second.mapBlockUnspent))
        {
            StdError("check", "FetchTxPool: AddForkUnspent fail.");
            return false;
        }
    }
    if (!objTxPoolData.FetchTxPool(strDataPath))
    {
        StdError("check", "FetchTxPool: Fetch tx pool fail.");
        return false;
    }
    return true;
}

bool CCheckRepairData::FetchWalletAddress()
{
    CWalletDB dbWallet;
    if (!dbWallet.Initialize(path(strDataPath) / "wallet"))
    {
        StdLog("check", "dbWallet Initialize fail");
        return false;
    }
    if (!dbWallet.WalkThroughAddress(objWalletAddressWalker))
    {
        StdLog("check", "WalkThroughAddress fail");
        dbWallet.Deinitialize();
        return false;
    }
    dbWallet.Deinitialize();
    return true;
}

bool CCheckRepairData::FetchWalletTx()
{
    CWalletDB dbWallet;
    if (!dbWallet.Initialize(path(strDataPath) / "wallet"))
    {
        StdLog("check", "dbWallet Initialize fail");
        return false;
    }
    if (!dbWallet.WalkThroughTx(objWalletTxWalker))
    {
        StdLog("check", "WalkThroughTx fail");
        dbWallet.Deinitialize();
        return false;
    }
    dbWallet.Deinitialize();
    return true;
}

bool CCheckRepairData::CheckRepairFork()
{
    vector<pair<uint256, uint256>> vForkUpdate;
    map<uint256, CCheckForkStatus>::iterator it = objForkManager.mapForkStatus.begin();
    for (; it != objForkManager.mapForkStatus.end(); ++it)
    {
        map<uint256, CCheckBlockFork>::iterator mt = objBlockWalker.mapCheckFork.find(it->first);
        if (mt != objBlockWalker.mapCheckFork.end() && mt->second.pLast != nullptr)
        {
            if (it->second.hashLastBlock != mt->second.pLast->GetBlockHash())
            {
                StdLog("check", "Check fork last: last block error, fork: %s, fork last: %s, block last: %s",
                       it->first.GetHex().c_str(),
                       it->second.hashLastBlock.GetHex().c_str(),
                       mt->second.pLast->GetBlockHash().GetHex().c_str());
                vForkUpdate.push_back(make_pair(it->first, mt->second.pLast->GetBlockHash()));
            }
        }
    }
    if (!fOnlyCheck && !vForkUpdate.empty())
    {
        return objForkManager.UpdateForkLast(strDataPath, vForkUpdate);
    }
    return true;
}

bool CCheckRepairData::CheckBlockUnspent()
{
    bool fCheckResult = true;
    map<uint256, CCheckBlockFork>::iterator mt = objBlockWalker.mapCheckFork.begin();
    for (; mt != objBlockWalker.mapCheckFork.end(); ++mt)
    {
        if (!mapForkUnspentWalker[mt->first].CheckForkUnspent(mt->second.mapBlockUnspent))
        {
            fCheckResult = false;
        }
    }
    return fCheckResult;
}

bool CCheckRepairData::CheckWalletTx(vector<CWalletTx>& vAddTx, vector<uint256>& vRemoveTx)
{
    //tx check
    {
        map<uint256, CCheckBlockFork>::iterator mt = objBlockWalker.mapCheckFork.begin();
        for (; mt != objBlockWalker.mapCheckFork.end(); ++mt)
        {
            const uint256& hashFork = mt->first;
            CCheckBlockFork& objBlockFork = mt->second;
            map<uint256, CCheckBlockTx>::iterator it = objBlockFork.mapBlockTx.begin();
            for (; it != objBlockFork.mapBlockTx.end(); ++it)
            {
                const uint256& txid = it->first;
                const CCheckBlockTx& cacheTx = it->second;
                bool fIsMine = objWalletAddressWalker.CheckAddress(cacheTx.tx.sendTo);
                bool fFromMe = objWalletAddressWalker.CheckAddress(cacheTx.txContxt.destIn);
                if (fIsMine || fFromMe)
                {
                    CCheckWalletTx* pWalletTx = objWalletTxWalker.GetWalletTx(hashFork, txid);
                    if (pWalletTx == nullptr)
                    {
                        StdLog("check", "CheckWalletTx: [block tx] find wallet tx fail, txid: %s, block height: %d, fork: %s",
                               txid.GetHex().c_str(), cacheTx.txIndex.nBlockHeight, hashFork.GetHex().c_str());
                        //CAssembledTx(const CTransaction& tx, int nBlockHeightIn, const CDestination& destInIn = CDestination(), int64 nValueInIn = 0)
                        //CWalletTx(const uint256& txidIn, const CAssembledTx& tx, const uint256& hashForkIn, bool fIsMine, bool fFromMe)
                        CAssembledTx atx(cacheTx.tx, cacheTx.txIndex.nBlockHeight, cacheTx.txContxt.destIn, cacheTx.txContxt.GetValueIn());
                        CWalletTx wtx(txid, atx, hashFork, fIsMine, fFromMe);
                        objWalletTxWalker.AddWalletTx(wtx);
                        vAddTx.push_back(wtx);
                    }
                    else
                    {
                        if (pWalletTx->nBlockHeight != cacheTx.txIndex.nBlockHeight)
                        {
                            StdLog("check", "CheckWalletTx: [block tx] wallet tx height error, wtx height: %d, block height: %d, txid: %s",
                                   pWalletTx->nBlockHeight, cacheTx.txIndex.nBlockHeight, txid.GetHex().c_str());
                            pWalletTx->nBlockHeight = cacheTx.txIndex.nBlockHeight;
                            vAddTx.push_back(*pWalletTx);
                        }
                    }
                }
            }
        }
    }
    {
        map<uint256, CCheckForkTxPool>::iterator mt = objTxPoolData.mapForkTxPool.begin();
        for (; mt != objTxPoolData.mapForkTxPool.end(); ++mt)
        {
            const uint256& hashFork = mt->first;
            CCheckForkTxPool& objTxPoolFork = mt->second;
            map<uint256, CAssembledTx>::iterator it = objTxPoolFork.mapTxPoolTx.begin();
            for (; it != objTxPoolFork.mapTxPoolTx.end(); ++it)
            {
                const uint256& txid = it->first;
                const CAssembledTx& tx = it->second;
                bool fIsMine = objWalletAddressWalker.CheckAddress(tx.sendTo);
                bool fFromMe = objWalletAddressWalker.CheckAddress(tx.destIn);
                if (fIsMine || fFromMe)
                {
                    CCheckWalletTx* pWalletTx = objWalletTxWalker.GetWalletTx(hashFork, txid);
                    if (pWalletTx == nullptr)
                    {
                        StdLog("check", "CheckWalletTx: [pool tx] find wallet tx fail, txid: %s", txid.GetHex().c_str());
                        //CWalletTx(const uint256& txidIn, const CAssembledTx& tx, const uint256& hashForkIn, bool fIsMine, bool fFromMe)
                        CWalletTx wtx(txid, tx, hashFork, fIsMine, fFromMe);
                        objWalletTxWalker.AddWalletTx(wtx);
                        vAddTx.push_back(wtx);
                    }
                    else
                    {
                        if (pWalletTx->nBlockHeight != -1)
                        {
                            StdLog("check", "CheckWalletTx: [pool tx] wallet tx height error, wtx height: %d, txid: %s",
                                   pWalletTx->nBlockHeight, txid.GetHex().c_str());
                            pWalletTx->nBlockHeight = -1;
                            vAddTx.push_back(*pWalletTx);
                        }
                    }
                }
            }
        }
    }
    {
        map<uint256, CCheckWalletForkUnspent>::iterator mt = objWalletTxWalker.mapWalletFork.begin();
        for (; mt != objWalletTxWalker.mapWalletFork.end(); ++mt)
        {
            vector<pair<int, uint256>> vForkRemoveTx;
            const uint256& hashWalletFork = mt->first;
            CCheckWalletForkUnspent& objWalletFork = mt->second;
            map<uint256, CCheckWalletTx>::iterator it = objWalletFork.mapWalletTx.begin();
            for (; it != objWalletFork.mapWalletTx.end(); ++it)
            {
                CCheckWalletTx& wtx = it->second;
                if (wtx.hashFork == hashWalletFork)
                {
                    if (wtx.nBlockHeight >= 0)
                    {
                        int nBlockHeight = 0;
                        if (!objBlockWalker.CheckTxExist(wtx.hashFork, wtx.txid, nBlockHeight))
                        {
                            StdLog("check", "CheckWalletTx: [wallet tx] find block tx fail, txid: %s", wtx.txid.GetHex().c_str());
                            vForkRemoveTx.push_back(make_pair(wtx.nBlockHeight, wtx.txid));
                            vRemoveTx.push_back(wtx.txid);
                        }
                        if (wtx.nBlockHeight != nBlockHeight)
                        {
                            StdLog("check", "CheckWalletTx: [wallet tx] block tx height error, wtx height: %d, block height: %d, txid: %s",
                                   wtx.nBlockHeight, nBlockHeight, wtx.txid.GetHex().c_str());
                            wtx.nBlockHeight = nBlockHeight;
                            vAddTx.push_back(wtx);
                        }
                    }
                    else
                    {
                        if (!objTxPoolData.CheckTxExist(wtx.hashFork, wtx.txid))
                        {
                            StdLog("check", "CheckWalletTx: [wallet tx] find txpool tx fail, txid: %s", wtx.txid.GetHex().c_str());
                            vForkRemoveTx.push_back(make_pair(wtx.nBlockHeight, wtx.txid));
                            vRemoveTx.push_back(wtx.txid);
                        }
                    }
                }
            }
            for (const auto& txd : vForkRemoveTx)
            {
                objWalletTxWalker.RemoveWalletTx(mt->first, txd.first, txd.second);
            }
        }
    }

    //update unspent
    if (!objWalletTxWalker.UpdateUnspent())
    {
        StdError("check", "CheckWalletTx UpdateUnspent fail.");
        return false;
    }

    //unspent check
    {
        map<uint256, CCheckWalletForkUnspent>::iterator mt = objWalletTxWalker.mapWalletFork.begin();
        for (; mt != objWalletTxWalker.mapWalletFork.end(); ++mt)
        {
            uint256 hashFork = mt->first;
            CCheckWalletForkUnspent& fork = mt->second;
            map<CTxOutPoint, CCheckTxOut>::iterator it = fork.mapWalletUnspent.begin();
            for (; it != fork.mapWalletUnspent.end(); ++it)
            {
                if (!it->second.IsSpent() && !objTxPoolData.CheckTxPoolUnspent(hashFork, it->first, it->second))
                {
                    StdLog("check", "Check wallet unspent 1: spent fail, height: %d, utxo: [%d] %s.",
                           objWalletTxWalker.GetTxAtBlockHeight(hashFork, it->first.hash), it->first.n, it->first.hash.GetHex().c_str());
                    return false;
                }
            }
        }
    }
    {
        map<uint256, CCheckForkTxPool>::iterator mt = objTxPoolData.mapForkTxPool.begin();
        for (; mt != objTxPoolData.mapForkTxPool.end(); ++mt)
        {
            uint256 hashFork = mt->first;
            CCheckForkTxPool& fork = mt->second;
            map<CTxOutPoint, CCheckTxOut>::iterator it = fork.mapTxPoolUnspent.begin();
            for (; it != fork.mapTxPoolUnspent.end(); ++it)
            {
                if (!it->second.IsSpent() && objWalletAddressWalker.CheckAddress(it->second.destTo))
                {
                    if (!objWalletTxWalker.CheckWalletUnspent(hashFork, it->first, it->second))
                    {
                        StdLog("check", "Check wallet unspent 2: check unspent fail, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

bool CCheckRepairData::CheckBlockIndex()
{
    StdLog("check", "Start check block index.");
    CBlockIndexDB dbBlockIndex;
    if (!dbBlockIndex.Initialize(path(strDataPath)))
    {
        StdLog("check", "dbBlockIndex Initialize fail");
        return false;
    }

    CCheckBlockIndexWalker walker;
    dbBlockIndex.WalkThroughBlock(walker);
    StdLog("check", "Fetch block index success, count: %ld", walker.mapBlockIndex.size());

    for (map<uint256, CBlockIndex*>::iterator it = objBlockWalker.mapBlockIndex.begin(); it != objBlockWalker.mapBlockIndex.end(); ++it)
    {
        if (!it->second->IsOrigin())
        {
            if (!walker.CheckBlock(CBlockOutline(it->second)))
            {
                if (!fOnlyCheck)
                {
                    dbBlockIndex.AddNewBlock(CBlockOutline(it->second));
                }
            }
        }
    }

    for (map<uint256, CBlockOutline>::iterator it = walker.mapBlockIndex.begin(); it != walker.mapBlockIndex.end(); ++it)
    {
        const CBlockOutline& blockOut = it->second;
        map<uint256, CBlockEx>::iterator mt = objBlockWalker.mapBlock.find(blockOut.hashBlock);
        if (mt == objBlockWalker.mapBlock.end())
        {
            StdLog("check", "CheckBlockIndex: Find block hash fail, remove block index, block: %s.", blockOut.hashBlock.GetHex().c_str());
            if (!fOnlyCheck)
            {
                dbBlockIndex.RemoveBlock(blockOut.hashBlock);
            }
        }
    }

    dbBlockIndex.Deinitialize();
    return true;
}

bool CCheckRepairData::CheckTxIndex()
{
    CTxIndexDB dbTxIndex;
    if (!dbTxIndex.Initialize(path(strDataPath)))
    {
        StdLog("check", "dbTxIndex Initialize fail");
        return false;
    }

    map<uint256, vector<pair<uint256, CTxIndex>>> mapTxNew;
    map<uint256, CCheckBlockFork>::iterator mt = objBlockWalker.mapCheckFork.begin();
    for (; mt != objBlockWalker.mapCheckFork.end(); ++mt)
    {
        if (!dbTxIndex.LoadFork(mt->first))
        {
            StdLog("check", "dbTxIndex LoadFork fail");
            dbTxIndex.Deinitialize();
            return false;
        }
        int nCheckOkCount = 0;
        CBlockIndex* pBlockIndex = mt->second.pLast;
        while (pBlockIndex && pBlockIndex != mt->second.pOrigin)
        {
            const uint256& hashBlock = pBlockIndex->GetBlockHash();
            uint256 hashFork = pBlockIndex->GetOriginHash();
            if (hashFork == 0)
            {
                StdLog("check", "CheckTxIndex: fork is 0");
                pBlockIndex = pBlockIndex->pNext;
                continue;
            }
            map<uint256, CBlockEx>::iterator at = objBlockWalker.mapBlock.find(hashBlock);
            if (at == objBlockWalker.mapBlock.end())
            {
                StdLog("check", "CheckTxIndex: find block fail");
                return false;
            }
            const CBlockEx& block = at->second;
            if (!(block.IsOrigin() || block.IsVacant() || block.IsNull()))
            {
                CBufStream ss;
                CTxIndex txIndex;
                bool bCheckFail = false;

                uint32 nTxOffset = pBlockIndex->nOffset + block.GetTxSerializedOffset();
                if (!dbTxIndex.Retrieve(hashFork, block.txMint.GetHash(), txIndex))
                {
                    StdLog("check", "Retrieve db tx index fail, height: %d, block: %s, mint tx: %s.",
                           block.GetBlockHeight(), block.GetHash().GetHex().c_str(), block.txMint.GetHash().GetHex().c_str());

                    mapTxNew[hashFork].push_back(make_pair(block.txMint.GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                    bCheckFail = true;
                }
                else
                {
                    if (!(txIndex.nFile == pBlockIndex->nFile && txIndex.nOffset == nTxOffset))
                    {
                        StdLog("check", "Check tx index fail, height: %d, block: %s, mint tx: %s, db offset: %d, block offset: %d.",
                               block.GetBlockHeight(), block.GetHash().GetHex().c_str(),
                               block.txMint.GetHash().GetHex().c_str(), txIndex.nOffset, nTxOffset);

                        mapTxNew[hashFork].push_back(make_pair(block.txMint.GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                        bCheckFail = true;
                    }
                }
                nTxOffset += ss.GetSerializeSize(block.txMint);

                CVarInt var(block.vtx.size());
                nTxOffset += ss.GetSerializeSize(var);
                for (int i = 0; i < block.vtx.size(); i++)
                {
                    if (!dbTxIndex.Retrieve(pBlockIndex->GetOriginHash(), block.vtx[i].GetHash(), txIndex))
                    {
                        StdLog("check", "Retrieve db tx index fail, height: %d, block: %s, txid: %s.",
                               block.GetBlockHeight(), block.GetHash().GetHex().c_str(), block.vtx[i].GetHash().GetHex().c_str());

                        mapTxNew[hashFork].push_back(make_pair(block.vtx[i].GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                        bCheckFail = true;
                    }
                    else
                    {
                        if (!(txIndex.nFile == pBlockIndex->nFile && txIndex.nOffset == nTxOffset))
                        {
                            StdLog("check", "Check tx index fail, height: %d, block: %s, txid: %s, db offset: %d, block offset: %d.",
                                   block.GetBlockHeight(), block.GetHash().GetHex().c_str(), block.vtx[i].GetHash().GetHex().c_str(), txIndex.nOffset, nTxOffset);

                            mapTxNew[hashFork].push_back(make_pair(block.vtx[i].GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                            bCheckFail = true;
                        }
                    }
                    nTxOffset += ss.GetSerializeSize(block.vtx[i]);
                }
                if (!bCheckFail && ++nCheckOkCount >= 128)
                {
                    break;
                }
            }
            pBlockIndex = pBlockIndex->pPrev;
        }
    }

    // repair
    if (!fOnlyCheck && !mapTxNew.empty())
    {
        StdLog("check", "Repair tx index starting");
        for (const auto& fork : mapTxNew)
        {
            if (!dbTxIndex.Update(fork.first, fork.second, vector<uint256>()))
            {
                StdLog("check", "Repair tx index update fail");
            }
            dbTxIndex.Flush(fork.first);
            dbTxIndex.Flush(fork.first);
        }
        StdLog("check", "Repair tx index success");
    }

    dbTxIndex.Deinitialize();
    return true;
}

bool CCheckRepairData::RepairUnspent()
{
    CUnspentDB dbUnspent;
    if (!dbUnspent.Initialize(path(strDataPath)))
    {
        StdLog("check", "dbUnspent Initialize fail");
        return false;
    }

    map<uint256, CCheckForkUnspentWalker>::iterator it = mapForkUnspentWalker.begin();
    for (; it != mapForkUnspentWalker.end(); ++it)
    {
        if (!it->second.vAddUpdate.empty() || !it->second.vRemove.empty())
        {
            if (!dbUnspent.AddNewFork(it->first))
            {
                StdLog("check", "dbUnspent AddNewFork fail.");
                dbUnspent.Deinitialize();
                return false;
            }
            dbUnspent.RepairUnspent(it->first, it->second.vAddUpdate, it->second.vRemove);
        }
    }

    dbUnspent.Deinitialize();
    return true;
}

bool CCheckRepairData::RepairWalletTx(const vector<CWalletTx>& vAddTx, const vector<uint256>& vRemoveTx)
{
    CWalletDB dbWallet;
    if (!dbWallet.Initialize(path(strDataPath) / "wallet"))
    {
        StdLog("check", "dbWallet Initialize fail");
        return false;
    }
    if (!dbWallet.UpdateTx(vAddTx, vRemoveTx))
    {
        StdLog("check", "Wallet UpdateTx fail");
        dbWallet.Deinitialize();
        return false;
    }
    dbWallet.Deinitialize();
    return true;
}

bool CCheckRepairData::RestructureWalletTx()
{
    vector<CWalletTx> vAddTx;
    if (!objBlockWalker.GetBlockWalletTx(objWalletAddressWalker.setAddress, vAddTx))
    {
        StdLog("check", "Restructure wallet tx: Get block wallet tx fail");
        return false;
    }
    int64 nBlockWalletTxCount = vAddTx.size();
    StdLog("check", "Restructure wallet tx, block tx count: %ld", nBlockWalletTxCount);

    if (!objTxPoolData.GetTxPoolWalletTx(objWalletAddressWalker.setAddress, vAddTx))
    {
        StdLog("check", "Restructure wallet tx: Get txpool wallet tx fail");
        return false;
    }
    StdLog("check", "Restructure wallet tx, txpool tx count: %ld, total tx count: %ld",
           vAddTx.size() - nBlockWalletTxCount, vAddTx.size());

    CWalletDB dbWallet;
    if (!dbWallet.Initialize(path(strDataPath) / "wallet"))
    {
        StdLog("check", "Restructure wallet tx: dbWallet Initialize fail");
        return false;
    }

    dbWallet.ClearTx();
    if (!dbWallet.UpdateTx(vAddTx, vector<uint256>()))
    {
        StdLog("check", "Restructure wallet tx: Wallet UpdateTx fail");
        dbWallet.Deinitialize();
        return false;
    }

    dbWallet.Deinitialize();
    return true;
}

////////////////////////////////////////////////////////////////
bool CCheckRepairData::CheckRepairData()
{
    StdLog("check", "Start check and repair, path: %s", strDataPath.c_str());

    objWalletTxWalker.SetForkManager(&objForkManager);

    if (!objForkManager.FetchForkStatus(strDataPath))
    {
        StdLog("check", "Fetch fork status fail");
        return false;
    }
    StdLog("check", "Fetch fork status success");

    if (!FetchBlockData())
    {
        StdLog("check", "Fetch block data fail");
        return false;
    }
    StdLog("check", "Fetch block data success");

    if (!FetchUnspent())
    {
        StdLog("check", "Fetch unspent fail");
        return false;
    }
    StdLog("check", "Fetch unspent success");

    if (!FetchTxPool())
    {
        StdLog("check", "Fetch txpool data fail");
        return false;
    }
    StdLog("check", "Fetch txpool data success");

    if (!FetchWalletAddress())
    {
        StdLog("check", "Fetch wallet address fail");
        return false;
    }
    StdLog("check", "Fetch wallet address success");

    if (!FetchWalletTx())
    {
        StdLog("check", "Fetch wallet tx fail");
        return false;
    }
    StdLog("check", "Fetch wallet tx success, wallet tx count: %ld", objWalletTxWalker.nWalletTxCount);

    if (!CheckRepairFork())
    {
        StdLog("check", "Fetch wallet tx fail");
        return false;
    }
    StdLog("check", "Check and repair fork success");

    StdLog("check", "Check block unspent starting");
    if (!CheckBlockUnspent())
    {
        StdLog("check", "Check block unspent fail");
        if (!fOnlyCheck)
        {
            if (!RepairUnspent())
            {
                StdLog("check", "Repair unspent fail");
                return false;
            }
            StdLog("check", "Repair block unspent success");
        }
    }
    else
    {
        StdLog("check", "Check block unspent success");
    }

    StdLog("check", "Check wallet tx starting");
    vector<CWalletTx> vAddTx;
    vector<uint256> vRemoveTx;
    if (CheckWalletTx(vAddTx, vRemoveTx))
    {
        if (!vAddTx.empty() || !vRemoveTx.empty())
        {
            if (!fOnlyCheck)
            {
                StdLog("check", "Check wallet tx fail, start repair wallet tx, add tx: %ld, remove tx: %ld", vAddTx.size(), vRemoveTx.size());
                if (!RepairWalletTx(vAddTx, vRemoveTx))
                {
                    StdLog("check", "Repair wallet tx fail");
                    return false;
                }
                StdLog("check", "Repair wallet tx success");
            }
            else
            {
                StdLog("check", "Check wallet tx fail, add tx: %ld, remove tx: %ld", vAddTx.size(), vRemoveTx.size());
            }
        }
        else
        {
            StdLog("check", "Check wallet tx success");
        }
    }
    else
    {
        if (!fOnlyCheck)
        {
            StdLog("check", "Check wallet tx fail, start restructure wallet tx");
            if (!RestructureWalletTx())
            {
                StdLog("check", "Restructure wallet tx fail");
                return false;
            }
            StdLog("check", "Restructure wallet tx success");
        }
        else
        {
            StdLog("check", "Check wallet tx fail, need to restructure wallet tx");
        }
    }

    StdLog("check", "Check block index starting");
    if (!objBlockWalker.CheckBlockIndex())
    {
        StdLog("check", "Check block index fail");
        return false;
    }
    StdLog("check", "Check block index complete");

    StdLog("check", "Check tx index starting");
    if (!CheckTxIndex())
    {
        StdLog("check", "Check tx index fail");
        return false;
    }
    StdLog("check", "Check tx index complete");
    return true;
}

} // namespace bigbang
