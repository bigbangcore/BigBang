// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "checkrepair.h"

using namespace std;
using namespace xengine;

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
                    StdLog("check", "Check 2 unspent: find fail, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
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
                    StdLog("check", "Check fork unspent: find utxo fail, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                    vRemove.push_back(it->first);
                    mapForkUnspent.erase(it++);
                    continue;
                }
                else if (mt->second.IsSpent())
                {
                    StdLog("check", "Check fork unspent: is spent, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                    it->second = mt->second;
                    vAddUpdate.push_back(CTxUnspent(it->first, static_cast<const CTxOut&>(it->second)));
                }
                else if (it->second != mt->second)
                {
                    StdLog("check", "Check fork unspent: txout error, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
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
// CCheckWalletForkUnspent

bool CCheckWalletForkUnspent::Exist(const uint256& txid)
{
    return (mapWalletTx.find(txid) != mapWalletTx.end());
}

bool CCheckWalletForkUnspent::AddWalletTx(const CWalletTx& wtx)
{
    if (mapWalletTx.find(wtx.txid) != mapWalletTx.end())
    {
        StdLog("check", "AddWalletTx tx is exist");
        return false;
    }
    map<uint256, CCheckWalletTx>::iterator it = mapWalletTx.insert(make_pair(wtx.txid, CCheckWalletTx(wtx, ++nSeqCreate))).first;
    if (it == mapWalletTx.end())
    {
        StdLog("check", "AddWalletTx insert tx fail");
        return false;
    }
    CWalletTxLinkSetByTxHash& idxTx = setWalletTxLink.get<0>();
    if (idxTx.find(wtx.txid) != idxTx.end())
    {
        setWalletTxLink.erase(wtx.txid);
    }
    if (!setWalletTxLink.insert(CWalletTxLink(&(it->second))).second)
    {
        StdLog("check", "AddWalletTx insert tx link fail");
        return false;
    }
    return true;
}

void CCheckWalletForkUnspent::RemoveWalletTx(const uint256& txid)
{
    mapWalletTx.erase(txid);
    setWalletTxLink.erase(txid);
}

bool CCheckWalletForkUnspent::UpdateUnspent()
{
    CWalletTxLinkSetBySequenceNumber& idxTx = setWalletTxLink.get<1>();
    CWalletTxLinkSetBySequenceNumber::iterator it = idxTx.begin();
    for (; it != idxTx.end(); ++it)
    {
        const CCheckWalletTx& wtx = *(it->ptx);
        for (int n = 0; n < wtx.vInput.size(); n++)
        {
            if (wtx.IsFromMe())
            {
                if (!AddWalletSpent(wtx.vInput[n].prevout, wtx.txid, wtx.sendTo))
                {
                    StdError("check", "AddWalletSpent fail");
                    return false;
                }
            }
        }
        if (wtx.IsMine())
        {
            if (!AddWalletUnspent(CTxOutPoint(wtx.txid, 0), wtx.GetOutput(0)))
            {
                StdError("check", "AddWalletUnspent 0 fail");
                return false;
            }
        }
        if (wtx.IsFromMe())
        {
            if (!AddWalletUnspent(CTxOutPoint(wtx.txid, 1), wtx.GetOutput(1)))
            {
                StdError("check", "AddWalletUnspent 1 fail");
                return false;
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
// CCheckWalletTxWalker

bool CCheckWalletTxWalker::Walk(const CWalletTx& wtx)
{
    if (wtx.hashFork == 0)
    {
        StdError("check", "CCheckWalletTxWalker Walk fork is 0.");
        return false;
    }
    if (!mapWalletFork[wtx.hashFork].AddWalletTx(wtx))
    {
        StdError("check", "CCheckWalletTxWalker AddWalletTx fail.");
        return false;
    }
    nWalletTxCount++;
    return true;
}

bool CCheckWalletTxWalker::Exist(const uint256& hashFork, const uint256& txid)
{
    if (hashFork == 0)
    {
        StdError("check", "CCheckWalletTxWalker Exist hashFork is 0.");
        return false;
    }
    return mapWalletFork[hashFork].Exist(txid);
}

bool CCheckWalletTxWalker::AddWalletTx(const CWalletTx& wtx)
{
    if (wtx.hashFork == 0)
    {
        StdError("check", "CCheckWalletTxWalker AddWalletTx fork is 0.");
        return false;
    }
    if (!mapWalletFork[wtx.hashFork].AddWalletTx(wtx))
    {
        StdError("check", "CCheckWalletTxWalker AddWalletTx fail.");
        return false;
    }
    nWalletTxCount++;
    return true;
}

void CCheckWalletTxWalker::RemoveWalletTx(const uint256& hashFork, const uint256& txid)
{
    if (hashFork != 0)
    {
        nWalletTxCount--;
        mapWalletFork[hashFork].RemoveWalletTx(txid);
    }
    else
    {
        StdError("check", "CCheckWalletTxWalker RemoveWalletTx hashFork is 0.");
    }
}

bool CCheckWalletTxWalker::UpdateUnspent()
{
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.begin();
    for (; it != mapWalletFork.end(); ++it)
    {
        if (!it->second.UpdateUnspent())
        {
            StdError("check", "UpdateUnspent: Update unspent fail, fork: %s.", it->first.GetHex().c_str());
            return false;
        }
    }
    return true;
}

int CCheckWalletTxWalker::GetTxAtBlockHeight(const uint256& hashFork, const uint256& txid)
{
    if (hashFork == 0)
    {
        StdError("check", "CCheckWalletTxWalker GetTxAtBlockHeight hashFork is 0.");
        return -1;
    }
    return mapWalletFork[hashFork].GetTxAtBlockHeight(txid);
}

bool CCheckWalletTxWalker::CheckWalletUnspent(const uint256& hashFork, const CTxOutPoint& point, const CCheckTxOut& out)
{
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
    if (it != mapWalletFork.end())
    {
        return it->second.CheckWalletUnspent(point, out);
    }
    return false;
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
// CCheckFork

void CCheckFork::UpdateMaxTrust(CBlockIndex* pBlockIndex)
{
    if (pBlockIndex->IsOrigin())
    {
        if (pOrigin == nullptr)
        {
            pOrigin = pBlockIndex;
        }
        else
        {
            StdLog("check", "CCheckFork pOrigin is not NULL");
        }
    }
    if (pBlockIndex->nChainTrust > nMaxChainTrust)
    {
        nMaxChainTrust = pBlockIndex->nChainTrust;
        nMaxTrustHeight = pBlockIndex->nHeight;
        hashMaxTrustBlock = pBlockIndex->GetBlockHash();
    }
}

bool CCheckFork::AddBlockTx(const CTransaction& txIn, const CTxContxt& contxtIn, int nHeight, uint32 nFileNoIn, uint32 nOffsetIn)
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

bool CCheckFork::AddBlockSpent(const CTxOutPoint& txPoint, const uint256& txidSpent, const CDestination& sendTo)
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

bool CCheckFork::AddBlockUnspent(const CTxOutPoint& txPoint, const CTxOut& txOut)
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

/////////////////////////////////////////////////////////////////////////
// CCheckBlockWalker

bool CCheckBlockWalker::Walk(const CBlockEx& block, uint32 nFile, uint32 nOffset)
{
    const uint256 hashBlock = block.GetHash();
    if (mapBlock.count(hashBlock) > 0)
    {
        StdError("check", "block exist, hash: %s.", hashBlock.GetHex().c_str());
        return false;
    }

    CBlockEx* pPrevBlock = nullptr;
    if (block.hashPrev != 0)
    {
        map<uint256, CBlockEx>::iterator it = mapBlock.find(block.hashPrev);
        if (it != mapBlock.end())
        {
            pPrevBlock = &(it->second);
        }
    }
    if (!block.IsGenesis() && pPrevBlock == nullptr)
    {
        StdError("check", "Prev block not exist, hash: %s.", hashBlock.GetHex().c_str());
        return false;
    }

    map<uint256, CBlockEx>::iterator mt = mapBlock.insert(make_pair(hashBlock, block)).first;
    if (mt == mapBlock.end())
    {
        StdError("check", "insert block fail, hash: %s.", hashBlock.GetHex().c_str());
        return false;
    }
    CBlockEx& checkBlock = mt->second;

    CBlockIndex* pNewBlockIndex = AddNewIndex(hashBlock, static_cast<const CBlock&>(checkBlock), nFile, nOffset, GetBlockTrust(block));
    if (pNewBlockIndex == nullptr)
    {
        StdError("check", "Add new block index fail, hash: %s.", hashBlock.GetHex().c_str());
        return false;
    }
    if (pNewBlockIndex->GetOriginHash() == 0)
    {
        StdError("check", "Get block origin hash is 0, hash: %s.", hashBlock.GetHex().c_str());
        return false;
    }
    mapCheckFork[pNewBlockIndex->GetOriginHash()].UpdateMaxTrust(pNewBlockIndex);

    if (block.IsGenesis())
    {
        if (hashGenesis != 0)
        {
            StdError("check", "more genesis block, block hash: %s, hashGenesis: %s.",
                     hashBlock.GetHex().c_str(), hashGenesis.GetHex().c_str());
            return false;
        }
        hashGenesis = hashBlock;
    }

    nBlockCount++;

    return true;
}

uint256 CCheckBlockWalker::GetBlockTrust(const CBlockEx& block, const CBlockIndex* pIndexPrev, const CDelegateAgreement& agreement)
{
    if (block.IsOrigin() || block.IsVacant() || block.IsNull())
    {
        return uint64(0);
    }
    else if (block.IsProofOfWork())
    {
        // PoW difficulty = 2 ^ nBits
        CProofOfHashWorkCompact proof;
        proof.Load(block.vchProof);
        uint256 v(1);
        return v << proof.nBits;
    }
    else
    {
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
            return uint256(uint64(agreement.nWeight)) << nBits;
        }
        else
        {
            return uint64(0);
        }
    }
    return uint256();
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
    if (nSpacing > objProofParam.nProofOfWorkUpperTarget && nBits > objProofParam.nProofOfWorkLowerLimit)
    {
        nBits--;
    }
    else if (nSpacing < objProofParam.nProofOfWorkLowerTarget && nBits < objProofParam.nProofOfWorkUpperLimit)
    {
        nBits++;
    }
    return true;
}

bool CCheckBlockWalker::UpdateBlockNext()
{
    map<uint256, CCheckFork>::iterator it = mapCheckFork.begin();
    while (it != mapCheckFork.end())
    {
        CCheckFork& checkFork = it->second;
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

bool CCheckBlockWalker::UpdateBlockTx()
{
    map<uint256, CCheckFork>::iterator it = mapCheckFork.begin();
    for (; it != mapCheckFork.end(); ++it)
    {
        CCheckFork& checkFork = it->second;
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
                break;
            }
            const CBlockEx& block = it->second;

            CBufStream ss;
            CTxContxt txContxt;
            txContxt.destIn = block.txMint.sendTo;
            uint32 nTxOffset = pIndex->nOffset + block.GetTxSerializedOffset();
            if (!checkFork.AddBlockTx(block.txMint, txContxt, block.GetBlockHeight(), pIndex->nFile, nTxOffset))
            {
                break;
            }
            nTxOffset += ss.GetSerializeSize(block.txMint);

            CVarInt var(block.vtx.size());
            nTxOffset += ss.GetSerializeSize(var);
            for (int i = 0; i < block.vtx.size(); i++)
            {
                if (!checkFork.AddBlockTx(block.vtx[i], block.vTxContxt[i], block.GetBlockHeight(), pIndex->nFile, nTxOffset))
                {
                    break;
                }
                nTxOffset += ss.GetSerializeSize(block.vtx[i]);
            }

            pIndex = pIndex->pNext;
        }
        if (it->first == hashGenesis)
        {
            nMainChainTxCount = checkFork.mapBlockTx.size();
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

bool CCheckBlockWalker::CheckTxExist(const uint256& hashFork, const uint256& txid)
{
    map<uint256, CCheckFork>::iterator it = mapCheckFork.find(hashFork);
    if (it == mapCheckFork.end())
    {
        StdError("check", "CheckTxExist: find fork fail, fork: %s, txid: %s", hashFork.GetHex().c_str(), txid.GetHex().c_str());
        return false;
    }
    return it->second.CheckTxExist(txid);
}

bool CCheckBlockWalker::GetBlockWalletTx(const set<CDestination>& setAddress, vector<CWalletTx>& vWalletTx)
{
    map<uint256, CCheckFork>::iterator mt = mapCheckFork.begin();
    for (; mt != mapCheckFork.end(); ++mt)
    {
        const uint256& hashFork = mt->first;
        CCheckFork& checkFork = mt->second;
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

    uint32 nLastFileRet = 0;
    uint32 nLastPosRet = 0;

    StdLog("check", "Fetch block and tx......");
    if (!tsBlock.WalkThrough(objBlockWalker, nLastFileRet, nLastPosRet))
    {
        StdError("check", "Fetch block and tx fail.");
        return false;
    }
    StdLog("check", "Fetch block and tx success, block: %ld.", objBlockWalker.nBlockCount);

    if (objBlockWalker.hashGenesis == 0)
    {
        StdError("check", "Not genesis block");
        return false;
    }

    StdLog("check", "Update blockchain......");
    objBlockWalker.UpdateBlockNext();
    StdLog("check", "Update blockchain success, main chain height: %d.", objBlockWalker.nMainChainHeight);

    StdLog("check", "Update block tx......");
    objBlockWalker.UpdateBlockTx();
    StdLog("check", "Update block tx success, main chain tx count: %ld.", objBlockWalker.nMainChainTxCount);
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

    map<uint256, CCheckFork>::iterator it = objBlockWalker.mapCheckFork.begin();
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
    map<uint256, CCheckFork>::iterator it = objBlockWalker.mapCheckFork.begin();
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

bool CCheckRepairData::CheckBlockUnspent()
{
    bool fCheckResult = true;
    map<uint256, CCheckFork>::iterator mt = objBlockWalker.mapCheckFork.begin();
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
        map<uint256, CCheckWalletForkUnspent>::iterator mt = objWalletTxWalker.mapWalletFork.begin();
        for (; mt != objWalletTxWalker.mapWalletFork.end(); ++mt)
        {
            vector<uint256> vForkRemoveTx;
            CCheckWalletForkUnspent& objWalletFork = mt->second;
            map<uint256, CCheckWalletTx>::iterator it = objWalletFork.mapWalletTx.begin();
            for (; it != objWalletFork.mapWalletTx.end(); ++it)
            {
                const CCheckWalletTx& wtx = it->second;
                if (wtx.nBlockHeight > 0)
                {
                    if (!objBlockWalker.CheckTxExist(wtx.hashFork, wtx.txid))
                    {
                        StdLog("check", "CheckWalletTx: [wallet tx] find block tx fail, txid: %s", wtx.txid.GetHex().c_str());
                        vForkRemoveTx.push_back(wtx.txid);
                        vRemoveTx.push_back(wtx.txid);
                    }
                }
                else
                {
                    if (!objTxPoolData.CheckTxExist(wtx.hashFork, wtx.txid))
                    {
                        StdLog("check", "CheckWalletTx: [wallet tx] find txpool tx fail, txid: %s", wtx.txid.GetHex().c_str());
                        vForkRemoveTx.push_back(wtx.txid);
                        vRemoveTx.push_back(wtx.txid);
                    }
                }
            }
            for (const auto& txid : vForkRemoveTx)
            {
                objWalletFork.RemoveWalletTx(txid);
            }
        }
    }
    {
        map<uint256, CCheckFork>::iterator mt = objBlockWalker.mapCheckFork.begin();
        for (; mt != objBlockWalker.mapCheckFork.end(); ++mt)
        {
            const uint256& hashFork = mt->first;
            CCheckFork& objBlockFork = mt->second;
            map<uint256, CCheckBlockTx>::iterator it = objBlockFork.mapBlockTx.begin();
            for (; it != objBlockFork.mapBlockTx.end(); ++it)
            {
                const uint256& txid = it->first;
                const CCheckBlockTx& cacheTx = it->second;
                bool fIsMine = objWalletAddressWalker.CheckAddress(cacheTx.tx.sendTo);
                bool fFromMe = objWalletAddressWalker.CheckAddress(cacheTx.txContxt.destIn);
                if (fIsMine || fFromMe)
                {
                    if (!objWalletTxWalker.Exist(hashFork, txid))
                    {
                        StdLog("check", "CheckWalletTx: [block tx] find wallet tx fail, txid: %s", txid.GetHex().c_str());
                        //CAssembledTx(const CTransaction& tx, int nBlockHeightIn, const CDestination& destInIn = CDestination(), int64 nValueInIn = 0)
                        //CWalletTx(const uint256& txidIn, const CAssembledTx& tx, const uint256& hashForkIn, bool fIsMine, bool fFromMe)
                        CAssembledTx atx(cacheTx.tx, cacheTx.txIndex.nBlockHeight, cacheTx.txContxt.destIn, cacheTx.txContxt.GetValueIn());
                        CWalletTx wtx(txid, atx, hashFork, fIsMine, fFromMe);
                        objWalletTxWalker.AddWalletTx(wtx);
                        vAddTx.push_back(wtx);
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
                    if (!objWalletTxWalker.Exist(hashFork, txid))
                    {
                        StdLog("check", "CheckWalletTx: [pool tx] find wallet tx fail, txid: %s", txid.GetHex().c_str());
                        //CWalletTx(const uint256& txidIn, const CAssembledTx& tx, const uint256& hashForkIn, bool fIsMine, bool fFromMe)
                        CWalletTx wtx(txid, tx, hashFork, fIsMine, fFromMe);
                        objWalletTxWalker.AddWalletTx(wtx);
                        vAddTx.push_back(wtx);
                    }
                }
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

    map<uint256, CBlockIndex*>::iterator it = objBlockWalker.mapBlockIndex.begin();
    for (; it == objBlockWalker.mapBlockIndex.end(); ++it)
    {
        if (!it->second->IsOrigin())
        {
            if (!walker.CheckBlock(CBlockOutline(it->second)))
            {
                dbBlockIndex.AddNewBlock(CBlockOutline(it->second));
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
            dbBlockIndex.RemoveBlock(blockOut.hashBlock);
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
    map<uint256, CCheckFork>::iterator mt = objBlockWalker.mapCheckFork.begin();
    for (; mt != objBlockWalker.mapCheckFork.end(); ++mt)
    {
        if (!dbTxIndex.LoadFork(mt->first))
        {
            StdLog("check", "dbTxIndex LoadFork fail");
            dbTxIndex.Deinitialize();
            return false;
        }
        CBlockIndex* pBlockIndex = mt->second.pOrigin;
        while (pBlockIndex)
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
            if (!block.IsGenesis())
            {
                CBufStream ss;
                CTxIndex txIndex;

                uint32 nTxOffset = pBlockIndex->nOffset + block.GetTxSerializedOffset();
                if (!dbTxIndex.Retrieve(hashFork, block.txMint.GetHash(), txIndex))
                {
                    StdLog("check", "dbTxIndex Retrieve fail, height: %d, block: %s, mint tx: %s.",
                           block.GetBlockHeight(), block.GetHash().GetHex().c_str(), block.txMint.GetHash().GetHex().c_str());

                    mapTxNew[hashFork].push_back(make_pair(block.txMint.GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                }
                else
                {
                    if (!(txIndex.nFile == pBlockIndex->nFile && txIndex.nOffset == nTxOffset))
                    {
                        StdLog("check", "Check txindex fail, height: %d, block: %s, mint tx: %s, db offset: %d, block offset: %d.",
                               block.GetBlockHeight(), block.GetHash().GetHex().c_str(),
                               block.txMint.GetHash().GetHex().c_str(), txIndex.nOffset, nTxOffset);

                        mapTxNew[hashFork].push_back(make_pair(block.txMint.GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                    }
                }
                nTxOffset += ss.GetSerializeSize(block.txMint);

                CVarInt var(block.vtx.size());
                nTxOffset += ss.GetSerializeSize(var);
                for (int i = 0; i < block.vtx.size(); i++)
                {
                    if (!dbTxIndex.Retrieve(pBlockIndex->GetOriginHash(), block.vtx[i].GetHash(), txIndex))
                    {
                        StdLog("check", "dbTxIndex Retrieve fail, height: %d, block: %s, txid: %s.",
                               block.GetBlockHeight(), block.GetHash().GetHex().c_str(), block.vtx[i].GetHash().GetHex().c_str());

                        mapTxNew[hashFork].push_back(make_pair(block.txMint.GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                    }
                    else
                    {
                        if (!(txIndex.nFile == pBlockIndex->nFile && txIndex.nOffset == nTxOffset))
                        {
                            StdLog("check", "Check txindex fail, height: %d, block: %s, txid: %s, db offset: %d, block offset: %d.",
                                   block.GetBlockHeight(), block.GetHash().GetHex().c_str(), block.vtx[i].GetHash().GetHex().c_str(), txIndex.nOffset, nTxOffset);

                            mapTxNew[hashFork].push_back(make_pair(block.txMint.GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                        }
                    }
                    nTxOffset += ss.GetSerializeSize(block.vtx[i]);
                }
            }
            pBlockIndex = pBlockIndex->pNext;
        }
    }

    // repair
    if (!mapTxNew.empty())
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
    StdLog("check", "Start check, path: %s", strDataPath.c_str());

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

    StdLog("check", "Check block unspent starting");
    if (!CheckBlockUnspent())
    {
        StdLog("check", "Check block unspent fail");
        if (!RepairUnspent())
        {
            StdLog("check", "Repair unspent fail");
            return false;
        }
        StdLog("check", "Repair block unspent success");
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
            StdLog("check", "Check wallet tx success");
        }
    }
    else
    {
        StdLog("check", "Check wallet tx fail, start restructure wallet tx");
        if (!RestructureWalletTx())
        {
            StdLog("check", "Restructure wallet tx fail");
            return false;
        }
        StdLog("check", "Restructure wallet tx success");
    }

    StdLog("check", "Check block index starting");
    if (!CheckBlockIndex())
    {
        StdLog("check", "Check block index fail");
        return false;
    }
    StdLog("check", "Check block index success");

    StdLog("check", "Check tx index starting");
    if (!CheckTxIndex())
    {
        StdLog("check", "Check tx index fail");
        return false;
    }
    StdLog("check", "Check tx index success");
    return true;
}

} // namespace bigbang
