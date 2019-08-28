// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txpool.h"

#include <boost/range/adaptor/reversed.hpp>

using namespace std;
using namespace xengine;

namespace bigbang
{

//////////////////////////////
// CTxPoolCandidate
class CTxPoolCandidate
{
public:
    CTxPoolCandidate()
      : nTxSeq(-1), nTotalTxFee(0), nTotalSize(0) {}
    CTxPoolCandidate(const pair<uint256, CPooledTx*>& ptx)
      : nTxSeq(ptx.second->nSequenceNumber), nTotalTxFee(0), nTotalSize(0)
    {
        AddNewTx(ptx);
    }
    void AddNewTx(const pair<uint256, CPooledTx*>& ptx)
    {
        if (mapPoolTx.insert(ptx).second)
        {
            nTotalTxFee += ptx.second->nTxFee;
            nTotalSize += ptx.second->nSerializeSize;
        }
    }
    bool Have(const uint256& txid) const
    {
        return (!!mapPoolTx.count(txid));
    }
    int64 GetTxFeePerKB() const
    {
        return (nTotalSize != 0 ? (nTotalTxFee << 10) / nTotalSize : 0);
    }

public:
    size_t nTxSeq;
    int64 nTotalTxFee;
    size_t nTotalSize;
    map<uint256, CPooledTx*> mapPoolTx;
};

//////////////////////////////
// CTxPoolView
void CTxPoolView::InvalidateSpent(const CTxOutPoint& out, vector<uint256>& vInvolvedTx)
{
    vector<CTxOutPoint> vOutPoint;
    vOutPoint.push_back(out);
    for (std::size_t i = 0; i < vOutPoint.size(); i++)
    {
        uint256 txidNextTx;
        if (GetSpent(vOutPoint[i], txidNextTx))
        {
            CPooledTx* pNextTx = nullptr;
            if ((pNextTx = Get(txidNextTx)) != nullptr)
            {
                for (const CTxIn& txin : pNextTx->vInput)
                {
                    SetUnspent(txin.prevout);
                }
                CTxOutPoint out0(txidNextTx, 0);
                if (IsSpent(out0))
                {
                    vOutPoint.push_back(out0);
                }
                else
                {
                    mapSpent.erase(out0);
                }
                CTxOutPoint out1(txidNextTx, 1);
                if (IsSpent(out1))
                {
                    vOutPoint.push_back(out1);
                }
                else
                {
                    mapSpent.erase(out1);
                }
                setTxLinkIndex.erase(txidNextTx);
                vInvolvedTx.push_back(txidNextTx);
            }
        }
    }
}

void CTxPoolView::ArrangeBlockTx(vector<CTransaction>& vtx, int64& nTotalTxFee, int64 nBlockTime, size_t nMaxSize)
{
    size_t nTotalSize = 0;
    nTotalTxFee = 0;

    const CPooledTxLinkSetBySequenceNumber& idxTxLinkSeq = setTxLinkIndex.get<1>();
    CPooledTxLinkSetBySequenceNumber::iterator it = idxTxLinkSeq.begin();
    for (; it != idxTxLinkSeq.end(); ++it)
    {
        if ((*it).ptx && (*it).ptx->GetTxTime() <= nBlockTime)
        {
            if (nTotalSize + (*it).ptx->nSerializeSize > nMaxSize)
            {
                break;
            }
            vtx.push_back(*static_cast<CTransaction*>((*it).ptx));
            nTotalSize += (*it).ptx->nSerializeSize;
            nTotalTxFee += (*it).ptx->nTxFee;
        }
    }
}

//////////////////////////////
// CTxPool

CTxPool::CTxPool()
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    nLastSequenceNumber = 0;
}

CTxPool::~CTxPool()
{
}

bool CTxPool::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol\n");
        return false;
    }

    if (!GetObject("blockchain", pBlockChain))
    {
        Error("Failed to request blockchain\n");
        return false;
    }

    return true;
}

void CTxPool::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
}

bool CTxPool::HandleInvoke()
{
    if (!datTxPool.Initialize(Config()->pathData))
    {
        Error("Failed to initialize txpool data\n");
        return false;
    }

    if (!LoadData())
    {
        Error("Failed to load txpool data\n");
        return false;
    }

    return true;
}

void CTxPool::HandleHalt()
{
    if (!SaveData())
    {
        Error("Failed to save txpool data\n");
    }
    Clear();
}

bool CTxPool::Exists(const uint256& txid)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    return (!!mapTx.count(txid));
}

void CTxPool::Clear()
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
    mapPoolView.clear();
    mapTx.clear();
}

size_t CTxPool::Count(const uint256& fork) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    map<uint256, CTxPoolView>::const_iterator it = mapPoolView.find(fork);
    if (it != mapPoolView.end())
    {
        return ((*it).second.Count());
    }
    return 0;
}

Errno CTxPool::Push(const CTransaction& tx, uint256& hashFork, CDestination& destIn, int64& nValueIn)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
    uint256 txid = tx.GetHash();

    if (mapTx.count(txid))
    {
        return ERR_ALREADY_HAVE;
    }

    if (tx.IsMintTx())
    {
        return ERR_TRANSACTION_INVALID;
    }

    int nHeight;
    if (!pBlockChain->GetBlockLocation(tx.hashAnchor, hashFork, nHeight))
    {
        return ERR_TRANSACTION_INVALID;
    }

    CTxPoolView& txView = mapPoolView[hashFork];
    Errno err = AddNew(txView, txid, tx, hashFork, nHeight);
    if (err == OK)
    {
        CPooledTx* pPooledTx = txView.Get(txid);
        if (pPooledTx == nullptr)
        {
            return ERR_NOT_FOUND;
        }
        destIn = pPooledTx->destIn;
        nValueIn = pPooledTx->nValueIn;
    }

    return err;
}

void CTxPool::Pop(const uint256& txid)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
    map<uint256, CPooledTx>::iterator it = mapTx.find(txid);
    if (it == mapTx.end())
    {
        return;
    }
    CPooledTx& tx = (*it).second;
    uint256 hashFork;
    int nHeight;
    if (!pBlockChain->GetBlockLocation(tx.hashAnchor, hashFork, nHeight))
    {
        return;
    }
    vector<uint256> vInvalidTx;
    CTxPoolView& txView = mapPoolView[hashFork];
    txView.Remove(txid);
    txView.InvalidateSpent(CTxOutPoint(txid, 0), vInvalidTx);
    txView.InvalidateSpent(CTxOutPoint(txid, 1), vInvalidTx);
    for (const uint256& txidInvalid : vInvalidTx)
    {
        mapTx.erase(txidInvalid);
    }
}

bool CTxPool::Get(const uint256& txid, CTransaction& tx) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    map<uint256, CPooledTx>::const_iterator it = mapTx.find(txid);
    if (it != mapTx.end())
    {
        tx = (*it).second;
        return true;
    }
    return false;
}

void CTxPool::ListTx(const uint256& hashFork, vector<pair<uint256, size_t>>& vTxPool)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    map<uint256, CTxPoolView>::iterator it = mapPoolView.find(hashFork);
    if (it != mapPoolView.end())
    {
        CPooledTxLinkSetBySequenceNumber& idxTx = (*it).second.setTxLinkIndex.get<1>();
        for (CPooledTxLinkSetBySequenceNumber::iterator mi = idxTx.begin(); mi != idxTx.end(); ++mi)
        {
            vTxPool.push_back(make_pair((*mi).hashTX, (*mi).ptx->nSerializeSize));
        }
    }
}

void CTxPool::ListTx(const uint256& hashFork, vector<uint256>& vTxPool)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    map<uint256, CTxPoolView>::iterator it = mapPoolView.find(hashFork);
    if (it != mapPoolView.end())
    {
        CPooledTxLinkSetBySequenceNumber& idxTx = (*it).second.setTxLinkIndex.get<1>();
        for (CPooledTxLinkSetBySequenceNumber::iterator mi = idxTx.begin(); mi != idxTx.end(); ++mi)
        {
            vTxPool.push_back((*mi).hashTX);
        }
    }
}

void CTxPool::ListTx(const uint256& hashFork, vector<pair<uint256, uint256>>& vTxPool)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    map<uint256, CTxPoolView>::iterator it = mapPoolView.find(hashFork);
    if (it != mapPoolView.end())
    {
        CPooledTxLinkSetBySequenceNumber& idxTx = it->second.setTxLinkIndex.get<1>();
        for (CPooledTxLinkSetBySequenceNumber::iterator mi = idxTx.begin(); mi != idxTx.end(); ++mi)
        {
            vTxPool.push_back(make_pair(mi->hashTX, mi->ptx->hashAnchor));
        }
    }
}

bool CTxPool::FilterTx(const uint256& hashFork, CTxFilter& filter)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);

    map<uint256, CTxPoolView>::iterator it = mapPoolView.find(hashFork);
    if (it == mapPoolView.end())
    {
        return true;
    }

    CPooledTxLinkSetByTxHash& idxTx = (*it).second.setTxLinkIndex.get<0>();
    for (CPooledTxLinkSetByTxHash::iterator mi = idxTx.begin(); mi != idxTx.end(); ++mi)
    {
        if ((*mi).ptx && (filter.setDest.count((*mi).ptx->sendTo) || filter.setDest.count((*mi).ptx->destIn)))
        {
            if (!filter.FoundTx(hashFork, *static_cast<CAssembledTx*>((*mi).ptx)))
            {
                return false;
            }
        }
    }

    return true;
}

void CTxPool::ArrangeBlockTx(const uint256& hashFork, int64 nBlockTime, size_t nMaxSize,
                             vector<CTransaction>& vtx, int64& nTotalTxFee)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    CTxPoolView& txView = mapPoolView[hashFork];
    txView.ArrangeBlockTx(vtx, nTotalTxFee, nBlockTime, nMaxSize);
}

bool CTxPool::FetchInputs(const uint256& hashFork, const CTransaction& tx, vector<CTxOut>& vUnspent)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    CTxPoolView& txView = mapPoolView[hashFork];

    vUnspent.resize(tx.vInput.size());

    for (std::size_t i = 0; i < tx.vInput.size(); i++)
    {
        if (txView.IsSpent(tx.vInput[i].prevout))
        {
            return false;
        }
        txView.GetUnspent(tx.vInput[i].prevout, vUnspent[i]);
    }

    if (!pBlockChain->GetTxUnspent(hashFork, tx.vInput, vUnspent))
    {
        return false;
    }

    CDestination destIn;
    for (std::size_t i = 0; i < tx.vInput.size(); i++)
    {
        if (vUnspent[i].IsNull())
        {
            return false;
        }

        if (destIn.IsNull())
        {
            destIn = vUnspent[i].destTo;
        }
        else if (destIn != vUnspent[i].destTo)
        {
            return false;
        }
    }

    return true;
}

bool CTxPool::SynchronizeBlockChain(const CBlockChainUpdate& update, CTxSetChange& change)
{
    change.hashFork = update.hashFork;

    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);

    vector<uint256> vInvalidTx;
    CTxPoolView& txView = mapPoolView[update.hashFork];

    int nHeight = update.nLastBlockHeight - update.vBlockAddNew.size() + 1;
    for (const CBlockEx& block : boost::adaptors::reverse(update.vBlockAddNew))
    {
        if (block.txMint.nAmount != 0)
        {
            change.vTxAddNew.push_back(CAssembledTx(block.txMint, nHeight));
        }
        for (std::size_t i = 0; i < block.vtx.size(); i++)
        {
            const CTransaction& tx = block.vtx[i];
            const CTxContxt& txContxt = block.vTxContxt[i];
            uint256 txid = tx.GetHash();
            if (!update.setTxUpdate.count(txid))
            {
                if (txView.Exists(txid))
                {
                    txView.Remove(txid);
                    mapTx.erase(txid);
                    change.mapTxUpdate.insert(make_pair(txid, nHeight));
                }
                else
                {
                    for (const CTxIn& txin : tx.vInput)
                    {
                        txView.InvalidateSpent(txin.prevout, vInvalidTx);
                    }
                    change.vTxAddNew.push_back(CAssembledTx(tx, nHeight, txContxt.destIn, txContxt.GetValueIn()));
                }
            }
            else
            {
                change.mapTxUpdate.insert(make_pair(txid, nHeight));
            }
        }
        nHeight++;
    }

    vector<pair<uint256, vector<CTxIn>>> vTxRemove;
    for (const CBlockEx& block : update.vBlockRemove)
    {
        for (int i = block.vtx.size() - 1; i >= 0; i--)
        {
            const CTransaction& tx = block.vtx[i];
            uint256 txid = tx.GetHash();
            if (!update.setTxUpdate.count(txid))
            {
                uint256 spent0, spent1;

                txView.GetSpent(CTxOutPoint(txid, 0), spent0);
                txView.GetSpent(CTxOutPoint(txid, 1), spent1);
                if (AddNew(txView, txid, tx, update.hashFork, update.nLastBlockHeight) == OK)
                {
                    if (spent0 != 0)
                        txView.SetSpent(CTxOutPoint(txid, 0), spent0);
                    if (spent1 != 0)
                        txView.SetSpent(CTxOutPoint(txid, 1), spent0);

                    change.mapTxUpdate.insert(make_pair(txid, -1));
                }
                else
                {
                    txView.InvalidateSpent(CTxOutPoint(txid, 0), vInvalidTx);
                    txView.InvalidateSpent(CTxOutPoint(txid, 1), vInvalidTx);
                    vTxRemove.push_back(make_pair(txid, tx.vInput));
                }
            }
        }
        if (block.txMint.nAmount != 0)
        {
            uint256 txidMint = block.txMint.GetHash();
            CTxOutPoint outMint(txidMint, 0);
            txView.InvalidateSpent(outMint, vInvalidTx);

            vTxRemove.push_back(make_pair(txidMint, block.txMint.vInput));
        }
    }

    change.vTxRemove.reserve(vInvalidTx.size() + vTxRemove.size());
    for (const uint256& txid : boost::adaptors::reverse(vInvalidTx))
    {
        map<uint256, CPooledTx>::iterator it = mapTx.find(txid);
        if (it != mapTx.end())
        {
            change.vTxRemove.push_back(make_pair(txid, (*it).second.vInput));
            mapTx.erase(it);
        }
    }
    change.vTxRemove.insert(change.vTxRemove.end(), vTxRemove.begin(), vTxRemove.end());

    return true;
}

bool CTxPool::LoadData()
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);

    vector<pair<uint256, pair<uint256, CAssembledTx>>> vTx;
    if (!datTxPool.Load(vTx))
    {
        return false;
    }

    for (int i = 0; i < vTx.size(); i++)
    {
        const uint256& hashFork = vTx[i].first;
        const uint256& txid = vTx[i].second.first;
        const CAssembledTx& tx = vTx[i].second.second;

        map<uint256, CPooledTx>::iterator mi = mapTx.insert(make_pair(txid, CPooledTx(tx, GetSequenceNumber()))).first;
        mapPoolView[hashFork].AddNew(txid, (*mi).second);
    }
    return true;
}

bool CTxPool::SaveData()
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);

    map<size_t, pair<uint256, pair<uint256, CAssembledTx>>> mapSortTx;
    for (map<uint256, CTxPoolView>::iterator it = mapPoolView.begin(); it != mapPoolView.end(); ++it)
    {
        CPooledTxLinkSetByTxHash& idxTx = (*it).second.setTxLinkIndex.get<0>();
        for (CPooledTxLinkSetByTxHash::iterator mi = idxTx.begin(); mi != idxTx.end(); ++mi)
        {
            mapSortTx[(*mi).nSequenceNumber] = make_pair((*it).first, make_pair((*mi).hashTX, static_cast<CAssembledTx&>(*(*mi).ptx)));
        }
    }

    vector<pair<uint256, pair<uint256, CAssembledTx>>> vTx;
    vTx.reserve(mapSortTx.size());
    for (map<size_t, pair<uint256, pair<uint256, CAssembledTx>>>::iterator it = mapSortTx.begin();
         it != mapSortTx.end(); ++it)
    {
        vTx.push_back((*it).second);
    }

    return datTxPool.Save(vTx);
}

Errno CTxPool::AddNew(CTxPoolView& txView, const uint256& txid, const CTransaction& tx, const uint256& hashFork, int nForkHeight)
{
    vector<CTxOut> vPrevOutput;
    vPrevOutput.resize(tx.vInput.size());
    for (int i = 0; i < tx.vInput.size(); i++)
    {
        if (txView.IsSpent(tx.vInput[i].prevout))
        {
            return ERR_TRANSACTION_CONFLICTING_INPUT;
        }
        txView.GetUnspent(tx.vInput[i].prevout, vPrevOutput[i]);
    }

    if (!pBlockChain->GetTxUnspent(hashFork, tx.vInput, vPrevOutput))
    {
        return ERR_SYS_STORAGE_ERROR;
    }

    int64 nValueIn = 0;
    for (int i = 0; i < tx.vInput.size(); i++)
    {
        if (vPrevOutput[i].IsNull())
        {
            return ERR_TRANSACTION_CONFLICTING_INPUT;
        }
        nValueIn += vPrevOutput[i].nAmount;
    }

    Errno err = pCoreProtocol->VerifyTransaction(tx, vPrevOutput, nForkHeight, hashFork);
    if (err != OK)
    {
        return err;
    }

    CDestination destIn = vPrevOutput[0].destTo;
    map<uint256, CPooledTx>::iterator mi;
    mi = mapTx.insert(make_pair(txid, CPooledTx(tx, -1, GetSequenceNumber(), destIn, nValueIn))).first;
    txView.AddNew(txid, (*mi).second);

    return OK;
}

} // namespace bigbang
