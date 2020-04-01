// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txpool.h"

#include <algorithm>
#include <boost/range/adaptor/reversed.hpp>
#include <deque>

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

bool CTxPoolView::AddTxIndex(const uint256& txid, CPooledTx& tx)
{
    CPooledTxLinkSetByTxHash& idxTx = setTxLinkIndex.get<0>();

    if (idxTx.find(txid) != idxTx.end())
    {
        setTxLinkIndex.erase(txid);
    }

    CPooledTx* pMinNextPooledTx = nullptr;
    for (int i = 0; i < 2; i++)
    {
        uint256 txidNextTx;
        if (GetSpent(CTxOutPoint(txid, i), txidNextTx))
        {
            CPooledTx* pTx = Get(txidNextTx);
            if (pTx == nullptr)
            {
                StdError("CTxPoolView", "AddNew: find next tx fail, txid: %s", txidNextTx.GetHex().c_str());
                return false;
            }
            if (pMinNextPooledTx == nullptr || pMinNextPooledTx->nSequenceNumber > pTx->nSequenceNumber)
            {
                pMinNextPooledTx = pTx;
            }
        }
    }
    if (pMinNextPooledTx != nullptr)
    {
        CPooledTx* pRootPooledTx = nullptr;
        if ((pMinNextPooledTx->nSequenceNumber & 0xFFFFFFL) != 0)
        {
            pRootPooledTx = Get(((pMinNextPooledTx->nSequenceNumber >> 24) + 1) << 24);
            if (pRootPooledTx == nullptr)
            {
                StdLog("CTxPoolView", "AddNew: find root sequence number fail, nSequenceNumber: %ld", pMinNextPooledTx->nSequenceNumber);
            }
        }
        else
        {
            pRootPooledTx = pMinNextPooledTx;
        }
        if (pRootPooledTx != nullptr)
        {
            if (pRootPooledTx->nNextSequenceNumber == 0)
            {
                pRootPooledTx->nNextSequenceNumber = pRootPooledTx->nSequenceNumber - 1;
            }
            tx.nSequenceNumber = pRootPooledTx->nNextSequenceNumber--;
            if (Get(tx.nSequenceNumber) != nullptr)
            {
                StdError("CTxPoolView", "AddNew: new sequence used (1), nSequenceNumber: %ld", tx.nSequenceNumber);
                return false;
            }
        }
        else
        {
            uint64 nIdleNextSeq = pMinNextPooledTx->nSequenceNumber - 1;
            while ((nIdleNextSeq & 0xFFFFFFL) != 0)
            {
                if (Get(nIdleNextSeq) == nullptr)
                {
                    tx.nSequenceNumber = nIdleNextSeq;
                    break;
                }
                --nIdleNextSeq;
            }
            if ((nIdleNextSeq & 0xFFFFFFL) == 0)
            {
                StdError("CTxPoolView", "AddNew: find idle next sequence fail, nSequenceNumber: %ld", pMinNextPooledTx->nSequenceNumber);
                return false;
            }
        }
    }
    else
    {
        if (Get(tx.nSequenceNumber) != nullptr)
        {
            StdError("CTxPoolView", "AddNew: new sequence used (2), nSequenceNumber: %ld", tx.nSequenceNumber);
            return false;
        }
    }

    if (!setTxLinkIndex.insert(CPooledTxLink(&tx)).second)
    {
        StdError("CTxPoolView", "AddNew: setTxLinkIndex insert fail, txid: %s, nSequenceNumber: %ld",
                 txid.GetHex().c_str(), tx.nSequenceNumber);
        return false;
    }

    return true;
}

bool CTxPoolView::AddNew(const uint256& txid, CPooledTx& tx)
{
    if (!AddTxIndex(txid, tx))
    {
        StdError("CTxPoolView", "AddNew: Add tx index fail, txid: %s, nSequenceNumber: %ld",
                 txid.GetHex().c_str(), tx.nSequenceNumber);
        return false;
    }

    for (std::size_t i = 0; i < tx.vInput.size(); i++)
    {
        mapSpent[tx.vInput[i].prevout].SetSpent(txid);
    }

    CTxOut output;
    output = tx.GetOutput(0);
    if (!output.IsNull())
    {
        mapSpent[CTxOutPoint(txid, 0)].SetUnspent(output);
    }
    output = tx.GetOutput(1);
    if (!output.IsNull())
    {
        mapSpent[CTxOutPoint(txid, 1)].SetUnspent(output);
    }

    vector<pair<uint256, uint64>> vPrevTxid;
    for (size_t i = 0; i < tx.vInput.size(); i++)
    {
        vPrevTxid.push_back(make_pair(tx.vInput[i].prevout.hash, tx.nSequenceNumber));
    }
    for (size_t i = 0; i < vPrevTxid.size(); i++)
    {
        const uint256& txidPrev = vPrevTxid[i].first;
        CPooledTx* ptx = Get(txidPrev);
        if (ptx != nullptr)
        {
            if (ptx->nSequenceNumber > vPrevTxid[i].second)
            {
                if (!AddTxIndex(txidPrev, *ptx))
                {
                    StdError("CTxPoolView", "AddNew: Add prev tx index fail, txidPrev: %s, nSequenceNumber: %ld",
                             txidPrev.GetHex().c_str(), ptx->nSequenceNumber);
                    return false;
                }
                for (size_t j = 0; j < ptx->vInput.size(); j++)
                {
                    vPrevTxid.push_back(make_pair(ptx->vInput[j].prevout.hash, ptx->nSequenceNumber));
                }
            }
        }
    }

    return true;
}

// 把输入参数outpoint所关联的Tx链条的所有后序都标记为未花费，并拿到这个链条的TxPoolView
void CTxPoolView::InvalidateSpent(const CTxOutPoint& out, CTxPoolView& viewInvolvedTx)
{
    vector<CTxOutPoint> vOutPoint;
    vOutPoint.push_back(out);
    for (std::size_t i = 0; i < vOutPoint.size(); i++)
    {
        uint256 txidNextTx;
        // 通过前序输出拿到输出的花费，也就是花费这个输出的tx，也就是下一个（后序）tx的id
        if (GetSpent(vOutPoint[i], txidNextTx))
        {
            CPooledTx* pNextTx = nullptr;
            // 通过后序Tx id得到后序Tx本身
            if ((pNextTx = Get(txidNextTx)) != nullptr)
            {
                // 把后序tx的所有前序输出都标记为未花费
                for (const CTxIn& txin : pNextTx->vInput)
                {
                    SetUnspent(txin.prevout);
                }
                // 后序tx的输出是否有花费，一旦有花费，就压入队列，广度优先，去标记后序tx的前序输出为未花费
                CTxOutPoint out0(txidNextTx, 0);
                if (IsSpent(out0))
                {
                    vOutPoint.push_back(out0);
                }
                else
                {
                    mapSpent.erase(out0);
                }
                // 找零
                CTxOutPoint out1(txidNextTx, 1);
                if (IsSpent(out1))
                {
                    vOutPoint.push_back(out1);
                }
                else
                {
                    mapSpent.erase(out1);
                }
                // 把关联的Tx后序链条的所有Tx添加到输出参数的viewInvolvedTx中
                viewInvolvedTx.AddNew(txidNextTx, *pNextTx);
            }
        }
    }
}

void CTxPoolView::GetAllPrevTxLink(const CPooledTxLink& link, std::vector<CPooledTxLink>& prevLinks)
{
    std::deque<CPooledTxLink> queueBFS;
    queueBFS.push_back(link);

    while (!queueBFS.empty())
    {
        const CPooledTxLink& tempLink = queueBFS.front();
        for (int i = 0; i < tempLink.ptx->vInput.size(); ++i)
        {
            const CTxIn& txin = tempLink.ptx->vInput[i];
            const uint256& prevHash = txin.prevout.hash;
            auto iter = setTxLinkIndex.find(prevHash);
            if (iter != setTxLinkIndex.end())
            {
                prevLinks.push_back(*iter);
                queueBFS.push_back(*iter);
            }
        }
        queueBFS.pop_front();
    }
}

bool CTxPoolView::AddArrangeBlockTx(vector<CTransaction>& vtx, int64& nTotalTxFee, int64 nBlockTime, size_t nMaxSize, size_t& nTotalSize,
                                    map<CDestination, int>& mapVoteCert, set<uint256>& setUnTx, CPooledTx* ptx, map<CDestination, int64>& mapVote, int64 nWeightRatio)
{
    if (ptx->GetTxTime() <= nBlockTime)
    {
        if (!setUnTx.empty())
        {
            bool fMissPrev = false;
            for (const auto& d : ptx->vInput)
            {
                if (setUnTx.find(d.prevout.hash) != setUnTx.end())
                {
                    fMissPrev = true;
                    break;
                }
            }
            if (fMissPrev)
            {
                setUnTx.insert(ptx->GetHash());
                return true;
            }
        }
        if (ptx->nType == CTransaction::TX_CERT && !mapVoteCert.empty())
        {
            std::map<CDestination, int>::iterator it = mapVoteCert.find(ptx->sendTo);
            if (it != mapVoteCert.end())
            {
                if (it->second <= 0)
                {
                    setUnTx.insert(ptx->GetHash());
                    return true;
                }
                it->second--;
            }
        }
        if (ptx->nType == CTransaction::TX_CERT && !mapVote.empty())
        {
            std::map<CDestination, int64>::iterator iter = mapVote.find(ptx->sendTo);
            if (iter != mapVote.end())
            {
                if (iter->second < nWeightRatio)
                {
                    setUnTx.insert(ptx->GetHash());
                    return true;
                }
            }
            else
            {
                setUnTx.insert(ptx->GetHash());
                return true;
            }
        }
        if (nTotalSize + ptx->nSerializeSize > nMaxSize)
        {
            return false;
        }
        vtx.push_back(*static_cast<CTransaction*>(ptx));
        nTotalSize += ptx->nSerializeSize;
        nTotalTxFee += ptx->nTxFee;
    }
    else
    {
        setUnTx.insert(ptx->GetHash());
    }
    return true;
}

void CTxPoolView::ArrangeBlockTx(vector<CTransaction>& vtx, int64& nTotalTxFee, int64 nBlockTime, size_t nMaxSize, map<CDestination, int>& mapVoteCert, map<CDestination, int64>& mapVote, int64 nWeightRatio)
{
    size_t nTotalSize = 0;
    set<uint256> setUnTx;
    CPooledCertTxLinkSet setCertRelativesIndex;
    nTotalTxFee = 0;

    // Collect all cert related tx
    const CPooledTxLinkSetByTxType& idxTxLinkType = setTxLinkIndex.get<2>();
    const auto iterBegin = idxTxLinkType.lower_bound((uint16)(CTransaction::TX_CERT));
    const auto iterEnd = idxTxLinkType.upper_bound((uint16)(CTransaction::TX_CERT));
    for (auto iter = iterBegin; iter != iterEnd; ++iter)
    {
        if (iter->ptx && iter->nType == CTransaction::TX_CERT)
        {
            setCertRelativesIndex.insert(*iter);

            std::vector<CPooledTxLink> prevLinks;
            GetAllPrevTxLink(*iter, prevLinks);
            setCertRelativesIndex.insert(prevLinks.begin(), prevLinks.end());
        }
    }

    // process all cert related tx by seqnum
    const CPooledCertTxLinkSetBySequenceNumber& idxCertTxLinkSeq = setCertRelativesIndex.get<1>();
    for (auto& i : idxCertTxLinkSeq)
    {
        if (i.ptx)
        {
            StdDebug("CTxPoolView", "Cert tx related tx, tx seqnum: %llu, type: %d, tx hash: %s", i.nSequenceNumber, i.ptx->nType, i.hashTX.ToString().c_str());
            if (!AddArrangeBlockTx(vtx, nTotalTxFee, nBlockTime, nMaxSize, nTotalSize, mapVoteCert, setUnTx, i.ptx, mapVote, nWeightRatio))
            {
                return;
            }
        }
    }

    // process all tx in tx pool by seqnum
    const CPooledTxLinkSetBySequenceNumber& idxTxLinkSeq = setTxLinkIndex.get<1>();
    for (auto& i : idxTxLinkSeq)
    {
        // skip cert related tx
        if (setCertRelativesIndex.find(i.hashTX) != setCertRelativesIndex.end())
        {
            continue;
        }
        if (i.ptx)
        {
            if (!AddArrangeBlockTx(vtx, nTotalTxFee, nBlockTime, nMaxSize, nTotalSize, mapVoteCert, setUnTx, i.ptx, mapVote, nWeightRatio))
            {
                return;
            }
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
        Error("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("blockchain", pBlockChain))
    {
        Error("Failed to request blockchain");
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
        Error("Failed to initialize txpool data");
        return false;
    }

    if (!LoadData())
    {
        Error("Failed to load txpool data");
        return false;
    }

    return true;
}

void CTxPool::HandleHalt()
{
    if (!SaveData())
    {
        Error("Failed to save txpool data");
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
        StdError("CTxPool", "Push: tx existed, txid: %s", txid.GetHex().c_str());
        return ERR_ALREADY_HAVE;
    }

    if (tx.IsMintTx())
    {
        StdError("CTxPool", "Push: tx is mint, txid: %s", txid.GetHex().c_str());
        return ERR_TRANSACTION_INVALID;
    }

    int nHeight;
    if (!pBlockChain->GetBlockLocation(tx.hashAnchor, hashFork, nHeight))
    {
        StdError("CTxPool", "Push: GetBlockLocation fail, txid: %s, hashAnchor: %s",
                 txid.GetHex().c_str(), tx.hashAnchor.GetHex().c_str());
        return ERR_TRANSACTION_INVALID;
    }

    uint256 hashLast;
    int64 nTime;
    uint16 nMintType;
    if (!pBlockChain->GetLastBlock(hashFork, hashLast, nHeight, nTime, nMintType))
    {
        StdError("CTxPool", "Push: GetLastBlock fail, txid: %s, hashFork: %s",
                 txid.GetHex().c_str(), hashFork.GetHex().c_str());
        return ERR_TRANSACTION_INVALID;
    }

    CTxPoolView& txView = mapPoolView[hashFork];
    Errno err = AddNew(txView, txid, tx, hashFork, nHeight);
    if (err == OK)
    {
        CPooledTx* pPooledTx = txView.Get(txid);
        if (pPooledTx == nullptr)
        {
            StdError("CTxPool", "Push: txView Get fail, txid: %s", txid.GetHex().c_str());
            return ERR_NOT_FOUND;
        }
        destIn = pPooledTx->destIn;
        nValueIn = pPooledTx->nValueIn;
        StdTrace("CTxPool", "Push success, txid: %s", txid.GetHex().c_str());
    }
    else
    {
        StdTrace("CTxPool", "Push fail, err: [%d] %s, txid: %s", err, ErrorString(err), txid.GetHex().c_str());
    }

    return err;
}

void CTxPool::Pop(const uint256& txid)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
    map<uint256, CPooledTx>::iterator it = mapTx.find(txid);
    if (it == mapTx.end())
    {
        StdError("CTxPool", "Pop: find fail, txid: %s", txid.GetHex().c_str());
        return;
    }

    CPooledTx& tx = (*it).second;
    uint256 hashFork;
    int nHeight;
    if (!pBlockChain->GetBlockLocation(tx.hashAnchor, hashFork, nHeight))
    {
        StdError("CTxPool", "Pop: GetBlockLocation fail, txid: %s", txid.GetHex().c_str());
        return;
    }

    CTxPoolView& txView = mapPoolView[hashFork];
    txView.Remove(txid);

    CTxPoolView viewInvolvedTx;
    txView.InvalidateSpent(CTxOutPoint(txid, 0), viewInvolvedTx);
    txView.InvalidateSpent(CTxOutPoint(txid, 1), viewInvolvedTx);

    const CPooledTxLinkSetBySequenceNumber& idxTx = viewInvolvedTx.setTxLinkIndex.get<1>();
    for (CPooledTxLinkSetBySequenceNumber::const_iterator mi = idxTx.begin(); mi != idxTx.end(); ++mi)
    {
        mapTx.erase((*mi).hashTX);
    }

    StdTrace("CTxPool", "Pop success, txid: %s", txid.GetHex().c_str());
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
    map<uint256, CTxPoolView>::const_iterator it = mapPoolView.find(hashFork);
    if (it != mapPoolView.end())
    {
        const CPooledTxLinkSetBySequenceNumber& idxTx = (*it).second.setTxLinkIndex.get<1>();
        for (CPooledTxLinkSetBySequenceNumber::iterator mi = idxTx.begin(); mi != idxTx.end(); ++mi)
        {
            vTxPool.push_back(make_pair((*mi).hashTX, (*mi).ptx->nSerializeSize));
        }
    }
}

void CTxPool::ListTx(const uint256& hashFork, vector<uint256>& vTxPool)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    map<uint256, CTxPoolView>::const_iterator it = mapPoolView.find(hashFork);
    if (it != mapPoolView.end())
    {
        const CPooledTxLinkSetBySequenceNumber& idxTx = (*it).second.setTxLinkIndex.get<1>();
        for (CPooledTxLinkSetBySequenceNumber::const_iterator mi = idxTx.begin(); mi != idxTx.end(); ++mi)
        {
            vTxPool.push_back((*mi).hashTX);
        }
    }
}

bool CTxPool::FilterTx(const uint256& hashFork, CTxFilter& filter)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);

    map<uint256, CTxPoolView>::const_iterator it = mapPoolView.find(hashFork);
    if (it == mapPoolView.end())
    {
        return true;
    }

    const CPooledTxLinkSetByTxHash& idxTx = (*it).second.setTxLinkIndex.get<0>();
    for (CPooledTxLinkSetByTxHash::const_iterator mi = idxTx.begin(); mi != idxTx.end(); ++mi)
    {
        if ((*mi).ptx && (filter.setDest.count((*mi).ptx->sendTo) || filter.setDest.count((*mi).ptx->destIn)))
        {
            if (!filter.FoundTx(hashFork, *static_cast<CAssembledTx*>((*mi).ptx)))
            {
                StdLog("CTxPool", "FilterTx: FoundTx fail, txid: %s", (*mi).ptx->GetHash().GetHex().c_str());
                return false;
            }
        }
    }

    return true;
}

bool CTxPool::ArrangeBlockTx(const uint256& hashFork, const uint256& hashPrev, int64 nBlockTime, size_t nMaxSize,
                             vector<CTransaction>& vtx, int64& nTotalTxFee)
{

    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    if (mapTxCache.find(hashFork) == mapTxCache.end())
    {
        StdError("CTxPool", "ArrangeBlockTx: find hashFork failed");
        return false;
    }

    auto& cache = mapTxCache[hashFork];
    if (!cache.Exists(hashPrev))
    {
        StdError("CTxPool", "ArrangeBlockTx: find hashPrev in cache failed");
        return false;
    }

    std::vector<CTransaction> vCacheTx;
    cache.Retrieve(hashPrev, vCacheTx);

    nTotalTxFee = 0;
    size_t currentSize = 0;
    for (const auto& tx : vCacheTx)
    {
        size_t nSerializeSize = xengine::GetSerializeSize(tx);
        currentSize += nSerializeSize;
        if (currentSize > nMaxSize)
        {
            break;
        }

        nTotalTxFee += tx.nTxFee;
        vtx.push_back(tx);
    }

    return true;
}

void CTxPool::ArrangeBlockTx(const uint256& hashFork, int64 nBlockTime, const uint256& hashBlock, std::size_t nMaxSize,
                             std::vector<CTransaction>& vtx, int64& nTotalTxFee)
{
    map<CDestination, int> mapVoteCert;
    std::map<CDestination, int64> mapVote;
    int64 nWeightRatio = 0;
    if (hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        if (!pBlockChain->GetDelegateCertTxCount(hashBlock, mapVoteCert))
        {
            StdError("CTxPool", "ArrangeBlockTx: GetDelegateCertTxCount fail");
            return;
        }

        if (!pBlockChain->GetBlockDelegateVote(hashBlock, mapVote))
        {
            StdError("CTxPool", "ArrangeBlockTx: GetBlockDelegateVote fail");
            return;
        }

        nWeightRatio = pBlockChain->GetDelegateWeightRatio(hashBlock);
        if (nWeightRatio < 0)
        {
            StdError("CTxPool", "ArrangeBlockTx: GetDelegateWeightRatio fail");
            return;
        }
    }

    mapPoolView[hashFork].ArrangeBlockTx(vtx, nTotalTxFee, nBlockTime, nMaxSize, mapVoteCert, mapVote, nWeightRatio);
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
            StdError("CTxPool", "FetchInputs: prevout is spent, txid: %s, prevout: [%d]:%s",
                     tx.GetHash().GetHex().c_str(), tx.vInput[i].prevout.n, tx.vInput[i].prevout.hash.GetHex().c_str());
            return false;
        }
        txView.GetUnspent(tx.vInput[i].prevout, vUnspent[i]);
    }

    if (!pBlockChain->GetTxUnspent(hashFork, tx.vInput, vUnspent))
    {
        StdError("CTxPool", "FetchInputs: GetTxUnspent fail, txid: %s", tx.GetHash().GetHex().c_str());
        return false;
    }

    CDestination destIn;
    for (std::size_t i = 0; i < tx.vInput.size(); i++)
    {
        if (vUnspent[i].IsNull())
        {
            StdError("CTxPool", "FetchInputs: not find unspent, txid: %s, prevout: [%d]:%s",
                     tx.GetHash().GetHex().c_str(), tx.vInput[i].prevout.n, tx.vInput[i].prevout.hash.GetHex().c_str());
            return false;
        }

        if (destIn.IsNull())
        {
            destIn = vUnspent[i].destTo;
        }
        else if (destIn != vUnspent[i].destTo)
        {
            StdError("CTxPool", "FetchInputs: destIn error, destIn: %s, destTo: %s",
                     destIn.ToString().c_str(), vUnspent[i].destTo.ToString().c_str());
            return false;
        }
    }

    return true;
}

// 通过链的某个fORK最新更新的状态(回滚，最新高度等信息)来回滚更新某个Fork的TxPool，并同时得到某个Fork的Tx集合的变化
bool CTxPool::SynchronizeBlockChain(const CBlockChainUpdate& update, CTxSetChange& change)
{
    // 更新对应的Fork
    change.hashFork = update.hashFork;

    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);

    CTxPoolView viewInvolvedTx;
    // 拿到对应的Fork的TxPoolView
    CTxPoolView& txView = mapPoolView[update.hashFork];

    //int nHeight = update.nLastBlockHeight - update.vBlockAddNew.size() + 1;
    // 对于长链，高度从低到高遍历处理
    for (const CBlockEx& block : boost::adaptors::reverse(update.vBlockAddNew))
    {
        int nBlockHeight = block.GetBlockHeight();
        if (block.txMint.nAmount != 0)
        {
            // 长链Block的 mint tx如果有效，就放到txChange的txAddNew中
            change.vTxAddNew.push_back(CAssembledTx(block.txMint, nBlockHeight /*nHeight*/));
        }
        // 遍历处理Block中被打包的Tx列表，从前序到后续
        for (std::size_t i = 0; i < block.vtx.size(); i++)
        {
            const CTransaction& tx = block.vtx[i];
            const CTxContxt& txContxt = block.vTxContxt[i];
            uint256 txid = tx.GetHash();
            // 不在BlockView中删除回滚的Tx集合里面，setTxUpdate是被待删除回滚的tx集合
            if (!update.setTxUpdate.count(txid))
            {
                // 没被回滚删除的Tx，如果存在TxView中就需要从View中删除，如果存在也意味着Tx在没被上链的时候被同步过来了，既然现在tx已经上链，就需要删除txpool中的tx
                if (txView.Exists(txid))
                {
                    // 也就是把该Tx的前序设置为未花费
                    txView.Remove(txid);
                    mapTx.erase(txid);
                    // 因为打包上链的tx存在txpool中，所以是加入到maptxupdate字段
                    change.mapTxUpdate.insert(make_pair(txid, nBlockHeight));
                }
                else
                {
                    // 打包上链的Tx，不存在TxPool中的，就把它的前序输出所关联的所有后序Tx链条都标记为未花费，并拿到这个后序链条的TxPoolView
                    for (const CTxIn& txin : tx.vInput)
                    {
                        txView.InvalidateSpent(txin.prevout, viewInvolvedTx);
                    }
                    // 因为打包上链的tx不存在txpool中，所以是加入到vTxAddNew字段
                    change.vTxAddNew.push_back(CAssembledTx(tx, nBlockHeight /*nHeight*/, txContxt.destIn, txContxt.GetValueIn()));
                }
            }
            else
            {
                // 被BlocView中删除回滚的tx也加入到mapTxUpdate字段
                change.mapTxUpdate.insert(make_pair(txid, nBlockHeight /*nHeight*/));
            }
        }
        //nHeight++;
    }

    vector<pair<uint256, vector<CTxIn>>> vTxRemove;
    //std::vector<CBlockEx> vBlockRemove = update.vBlockRemove;
    //std::reverse(vBlockRemove.begin(), vBlockRemove.end());
    //for (const CBlockEx& block : vBlockRemove)
    // 对于被回滚的短链，高度从低到高的遍历处理
    for (const CBlockEx& block : boost::adaptors::reverse(update.vBlockRemove))
    {
        // 遍历处理Block中被打包的Tx，前后序的顺序列        
        for (int i = 0; i < block.vtx.size(); ++i)
        {
            const CTransaction& tx = block.vtx[i];
            uint256 txid = tx.GetHash();
            // 不在BlockView中删除回滚的Tx集合里面，setTxUpdate是被待删除回滚的tx集合
            if (!update.setTxUpdate.count(txid))
            {
                uint256 spent0, spent1;
                // 得到被回滚Block打包的Tx的输出所对应的后序Tx id（包含找零）
                txView.GetSpent(CTxOutPoint(txid, 0), spent0);
                txView.GetSpent(CTxOutPoint(txid, 1), spent1);
                // 回滚的Tx重新添加到对应的Fork的TxPoolView中
                if (AddNew(txView, txid, tx, update.hashFork, update.nLastBlockHeight) == OK)
                {
                    if (spent0 != 0)
                        txView.SetSpent(CTxOutPoint(txid, 0), spent0);
                    if (spent1 != 0)
                        txView.SetSpent(CTxOutPoint(txid, 1), spent1);

                    change.mapTxUpdate.insert(make_pair(txid, -1));
                }
                else
                {
                    txView.InvalidateSpent(CTxOutPoint(txid, 0), viewInvolvedTx);
                    txView.InvalidateSpent(CTxOutPoint(txid, 1), viewInvolvedTx);
                    vTxRemove.push_back(make_pair(txid, tx.vInput));
                }
            }
        }
        if (block.txMint.nAmount != 0)
        {
            uint256 txidMint = block.txMint.GetHash();
            CTxOutPoint outMint(txidMint, 0);
            txView.InvalidateSpent(outMint, viewInvolvedTx);

            vTxRemove.push_back(make_pair(txidMint, block.txMint.vInput));
        }
    }

    const CPooledTxLinkSetBySequenceNumber& idxInvolvedTx = viewInvolvedTx.setTxLinkIndex.get<1>();
    change.vTxRemove.reserve(idxInvolvedTx.size() + vTxRemove.size());
    for (const auto& txseq : boost::adaptors::reverse(idxInvolvedTx))
    {
        map<uint256, CPooledTx>::iterator it = mapTx.find(txseq.hashTX);
        if (it != mapTx.end())
        {
            change.vTxRemove.push_back(make_pair(txseq.hashTX, (*it).second.vInput));
            mapTx.erase(it);
        }
    }
    change.vTxRemove.insert(change.vTxRemove.end(), vTxRemove.rbegin(), vTxRemove.rend());

    // ArrangeBlockTx to cache
    if (mapTxCache.find(update.hashFork) == mapTxCache.end())
    {
        mapTxCache.insert(std::make_pair(update.hashFork, CTxCache(CACHE_HEIGHT_INTERVAL)));
    }

    std::vector<CTransaction> vtx;
    int64 nTotalFee = 0;
    const CBlockEx& lastBlockEx = update.vBlockAddNew[0];
    ArrangeBlockTx(update.hashFork, lastBlockEx.GetBlockTime(), lastBlockEx.GetHash(), MAX_BLOCK_SIZE, vtx, nTotalFee);

    auto& cache = mapTxCache[update.hashFork];
    cache.AddNew(lastBlockEx.GetHash(), vtx);

    return true;
}

bool CTxPool::LoadData()
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);

    vector<pair<uint256, pair<uint256, CAssembledTx>>> vTx;
    if (!datTxPool.Load(vTx))
    {
        StdTrace("CTxPool", "Load Data failed");
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

    std::map<uint256, CForkStatus> mapForkStatus;
    pBlockChain->GetForkStatus(mapForkStatus);

    for (const auto& kv : mapForkStatus)
    {
        const uint256& hashFork = kv.first;
        mapTxCache.insert(make_pair(hashFork, CTxCache(CACHE_HEIGHT_INTERVAL)));

        uint256 hashBlock;
        int nHeight = 0;
        int64 nTime = 0;
        uint16 nMintType = 0;
        if (!pBlockChain->GetLastBlock(hashFork, hashBlock, nHeight, nTime, nMintType))
        {
            return false;
        }

        std::vector<CTransaction> vtx;
        int64 nTotalFee = 0;
        ArrangeBlockTx(hashFork, nTime, hashBlock, MAX_BLOCK_SIZE, vtx, nTotalFee);
        mapTxCache[hashFork].AddNew(hashBlock, vtx);
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

// 将Tx添加到TxPoolView中
Errno CTxPool::AddNew(CTxPoolView& txView, const uint256& txid, const CTransaction& tx, const uint256& hashFork, int nForkHeight)
{
    vector<CTxOut> vPrevOutput;
    vPrevOutput.resize(tx.vInput.size());
    for (int i = 0; i < tx.vInput.size(); i++)
    {
        if (txView.IsSpent(tx.vInput[i].prevout))
        {
            StdTrace("CTxPool", "AddNew: tx input is spent, txid: %s, prevout: [%d]:%s",
                     txid.GetHex().c_str(), tx.vInput[i].prevout.n, tx.vInput[i].prevout.hash.ToString().c_str());
            return ERR_TRANSACTION_CONFLICTING_INPUT;
        }
        txView.GetUnspent(tx.vInput[i].prevout, vPrevOutput[i]);
    }

    if (!pBlockChain->GetTxUnspent(hashFork, tx.vInput, vPrevOutput))
    {
        StdTrace("CTxPool", "AddNew: GetTxUnspent fail, txid: %s, hashFork: %s",
                 txid.GetHex().c_str(), hashFork.GetHex().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    int64 nValueIn = 0;
    for (int i = 0; i < tx.vInput.size(); i++)
    {
        if (vPrevOutput[i].IsNull())
        {
            StdTrace("CTxPool", "AddNew: not find unspent, txid: %s, prevout: [%d]:%s",
                     txid.GetHex().c_str(), tx.vInput[i].prevout.n, tx.vInput[i].prevout.hash.GetHex().c_str());
            return ERR_TRANSACTION_CONFLICTING_INPUT;
        }
        nValueIn += vPrevOutput[i].nAmount;
    }

    Errno err = pCoreProtocol->VerifyTransaction(tx, vPrevOutput, nForkHeight, hashFork);
    if (err != OK)
    {
        StdTrace("CTxPool", "AddNew: VerifyTransaction fail, txid: %s", txid.GetHex().c_str());
        return err;
    }

    CDestination destIn = vPrevOutput[0].destTo;
    map<uint256, CPooledTx>::iterator mi = mapTx.insert(make_pair(txid, CPooledTx(tx, -1, GetSequenceNumber(), destIn, nValueIn))).first;
    if (!txView.AddNew(txid, (*mi).second))
    {
        StdTrace("CTxPool", "AddNew: txView AddNew fail, txid: %s", txid.GetHex().c_str());
        return ERR_NOT_FOUND;
    }
    return OK;
}

} // namespace bigbang
