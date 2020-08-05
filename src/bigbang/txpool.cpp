// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txpool.h"

#include <algorithm>
#include <boost/range/adaptor/reversed.hpp>
#include <deque>

using namespace std;
using namespace xengine;

static const unsigned int MAX_BLOCK_SIZE_EXCEPT_TX = 512;
static const unsigned int MAX_BLOCK_TX_SIZE = MAX_BLOCK_SIZE - MAX_BLOCK_SIZE_EXCEPT_TX;
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

void CTxPoolView::InvalidateSpent(const CTxOutPoint& out, CTxPoolView& viewInvolvedTx)
{
    vector<CTxOutPoint> vOutPoint;
    vOutPoint.push_back(out);
    for (std::size_t i = 0; i < vOutPoint.size(); i++)
    {
        uint256 txidNextTx;
        CPooledTx* pNextTx = nullptr;
        if (GetSpent(vOutPoint[i], txidNextTx) && ((pNextTx = Get(txidNextTx)) != nullptr))
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
            viewInvolvedTx.AddNew(txidNextTx, *pNextTx);
            setTxLinkIndex.erase(txidNextTx);
        }
        else
        {
            mapSpent.erase(vOutPoint[i]);
        }
    }
}

void CTxPoolView::GetAllPrevTxLink(const CPooledTxLink& link, vector<CPooledTxLink>& prevLinks, CPooledCertTxLinkSet& setCertTxLink)
{
    CPooledTxLinkSetByTxHash& idxTx = setTxLinkIndex.get<0>();
    CPooledCertTxLinkSetByTxHash& idxCertTx = setCertTxLink.get<0>();
    if (prevLinks.capacity() == 0)
    {
        size_t nSize = setTxLinkIndex.size();
        if (nSize > 1024)
        {
            nSize = 1024;
        }
        prevLinks.reserve(nSize);
    }
    prevLinks.clear();
    prevLinks.push_back(link);
    for (int n = 0; n < prevLinks.size(); ++n)
    {
        const CPooledTxLink& curLink = prevLinks[n];
        if (curLink.ptx != nullptr)
        {
            for (int i = 0; i < curLink.ptx->vInput.size(); ++i)
            {
                const uint256& prevTxid = curLink.ptx->vInput[i].prevout.hash;
                if (idxCertTx.find(prevTxid) == idxCertTx.end())
                {
                    auto iter = idxTx.find(prevTxid);
                    if (iter != idxTx.end())
                    {
                        prevLinks.push_back(*iter);
                        setCertTxLink.insert(*iter);
                    }
                }
            }
        }
    }
}

bool CTxPoolView::AddArrangeBlockTx(vector<CTransaction>& vtx, int64& nTotalTxFee, int64 nBlockTime, size_t nMaxSize, size_t& nTotalSize,
                                    map<CDestination, int>& mapVoteCert, set<uint256>& setUnTx, CPooledTx* ptx, map<CDestination, int64>& mapVote, int64 nMinEnrollAmount, bool fIsDposHeight)
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

void CTxPoolView::ArrangeBlockTx(vector<CTransaction>& vtx, int64& nTotalTxFee, int64 nBlockTime, size_t nMaxSize, map<CDestination, int>& mapVoteCert, map<CDestination, int64>& mapVote, int64 nMinEnrollAmount, bool fIsDposHeight)
{
    size_t nTotalSize = 0;
    set<uint256> setUnTx;
    CPooledCertTxLinkSet setCertRelativesIndex;
    std::vector<CPooledTxLink> prevLinks;
    nTotalTxFee = 0;

    // Collect all cert related tx
    //const CPooledTxLinkSetByTxType& idxTxLinkType = setTxLinkIndex.get<2>();
    //const auto iterBegin = idxTxLinkType.lower_bound((uint16)(CTransaction::TX_CERT));
    //const auto iterEnd = idxTxLinkType.upper_bound((uint16)(CTransaction::TX_CERT));
    //for (auto iter = iterBegin; iter != iterEnd; ++iter)
    //{
    //    if (iter->ptx && iter->nType == CTransaction::TX_CERT)
    //    {
    //         GetAllPrevTxLink(*iter, prevLinks, setCertRelativesIndex);
    //         setCertRelativesIndex.insert(*iter);
    //     }
    //}

    // process all cert related tx by seqnum
    const CPooledCertTxLinkSetBySequenceNumber& idxCertTxLinkSeq = setCertRelativesIndex.get<1>();
    for (auto& i : idxCertTxLinkSeq)
    {
        if (i.ptx)
        {
            if (!AddArrangeBlockTx(vtx, nTotalTxFee, nBlockTime, nMaxSize, nTotalSize, mapVoteCert, setUnTx, i.ptx, mapVote, nMinEnrollAmount, fIsDposHeight))
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
            if (!AddArrangeBlockTx(vtx, nTotalTxFee, nBlockTime, nMaxSize, nTotalSize, mapVoteCert, setUnTx, i.ptx, mapVote, nMinEnrollAmount, fIsDposHeight))
            {
                return;
            }
        }
    }
}

//////////////////////////////
// CCertTxDestCache

void CCertTxDestCache::AddDelegate(const CDestination& dest)
{
    setDelegate.insert(dest);
}

void CCertTxDestCache::AddCertTx(const CDestination& dest, const uint256& txid)
{
    if (setDelegate.find(dest) == setDelegate.end())
    {
        auto mt = mapCertTxDest.find(dest);
        if (mt != mapCertTxDest.end())
        {
            auto it = mt->second.find(txid);
            if (it != mt->second.end())
            {
                it->second = GetTime();
            }
            else
            {
                mt->second.insert(std::make_pair(txid, GetTime()));
            }
        }
    }
}

void CCertTxDestCache::RemoveCertTx(const CDestination& dest, const uint256& txid)
{
    auto mt = mapCertTxDest.find(dest);
    if (mt != mapCertTxDest.end())
    {
        mt->second.erase(txid);
        if (mt->second.size() == 0)
        {
            mapCertTxDest.erase(mt);
        }
    }
}

bool CCertTxDestCache::GetTimeoutCertTx(const CDestination& dest, uint256& txid)
{
    auto mt = mapCertTxDest.find(dest);
    if (mt != mapCertTxDest.end())
    {
        int64 nMinTime = 0;
        for (auto it = mt->second.begin(); it != mt->second.end(); ++it)
        {
            if (GetTime() - it->second >= CACHE_CERTTX_TIMEOUT)
            {
                if (nMinTime == 0 || it->second < nMinTime)
                {
                    nMinTime = it->second;
                    txid = it->first;
                }
            }
        }
        if (nMinTime != 0)
        {
            return true;
        }
    }
    return false;
}

bool CCertTxDestCache::IsOverMaxCertCount(const CDestination& dest)
{
    auto mt = mapCertTxDest.find(dest);
    if (mt != mapCertTxDest.end() && mt->second.size() >= MAX_CACHE_CERTTX_COUNT)
    {
        return false;
    }
    return true;
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
    if (!pBlockChain->GetBlockLocation(pCoreProtocol->GetGenesisBlockHash(), hashFork, nHeight))
    {
        //StdError("CTxPool", "Push: GetBlockLocation fail, txid: %s, hashAnchor: %s",
        //         txid.GetHex().c_str(), tx.hashAnchor.GetHex().c_str());
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
    RemoveTx(txid);
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

bool CTxPool::Get(const uint256& txid, CAssembledTx& tx) const
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

bool CTxPool::ListForkUnspent(const uint256& hashFork, const CDestination& dest, uint32 nMax, const std::vector<CTxUnspent>& vUnspentOnChain, std::vector<CTxUnspent>& vUnspent)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    map<uint256, CTxPoolView>::const_iterator it = mapPoolView.find(hashFork);
    if (it != mapPoolView.end())
    {
        const CTxPoolView& txPoolView = it->second;
        ListUnspent(txPoolView, dest, nMax, vUnspentOnChain, vUnspent);
        return true;
    }

    return false;
}

bool CTxPool::ListForkUnspentBatch(const uint256& hashFork, uint32 nMax, const std::map<CDestination, std::vector<CTxUnspent>>& mapUnspentOnChain, std::map<CDestination, std::vector<CTxUnspent>>& mapUnspent)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    map<uint256, CTxPoolView>::const_iterator it = mapPoolView.find(hashFork);
    if (it != mapPoolView.end())
    {
        const CTxPoolView& txPoolView = it->second;
        for (const auto& kv : mapUnspentOnChain)
        {
            const CDestination& dest = kv.first;
            const std::vector<CTxUnspent>& vUnspentOnChain = kv.second;
            ListUnspent(txPoolView, dest, nMax, vUnspentOnChain, mapUnspent[dest]);
        }

        return true;
    }

    return false;
}

void CTxPool::ListUnspent(const CTxPoolView& txPoolView, const CDestination& dest, uint32 nMax, const std::vector<CTxUnspent>& vUnspentOnChain, std::vector<CTxUnspent>& vUnspent)
{
    uint32 nCount = 0;
    std::set<CTxUnspent> setTxUnspent;
    for (size_t i = 0; i < vUnspentOnChain.size(); i++)
    {
        const CTxUnspent& unspentOnChain = vUnspentOnChain[i];
        CTxOutPoint outpoint(unspentOnChain.hash, unspentOnChain.n);

        if (nMax != 0 && nCount >= nMax)
        {
            return;
        }

        if (!txPoolView.IsSpent(outpoint))
        {
            vUnspent.push_back(unspentOnChain);
            setTxUnspent.insert(unspentOnChain);
            nCount++;
        }
    }

    std::vector<CTxUnspent> vTxPoolUnspent;
    txPoolView.ListUnspent(dest, setTxUnspent, (nMax != 0) ? (nMax - nCount) : nMax, vTxPoolUnspent);
    vUnspent.insert(vUnspent.end(), vTxPoolUnspent.begin(), vTxPoolUnspent.end());
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

bool CTxPool::ArrangeBlockTx(const uint256& hashFork, const uint256& hashPrev, int64 nBlockTime, /*size_t nMaxSize, */
                             vector<CTransaction>& vtx, int64& nTotalTxFee)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);

    auto it = mapTxCache.find(hashFork);
    if (it == mapTxCache.end())
    {
        StdError("CTxPool", "ArrangeBlockTx: find hashFork failed");
        return false;
    }
    CTxCache& cache = it->second;

    if (!cache.Retrieve(hashPrev, vtx))
    {
        StdError("CTxPool", "ArrangeBlockTx: find hashPrev in cache failed");
        return false;
    }

    nTotalTxFee = 0;
    for (const auto& tx : vtx)
    {
        nTotalTxFee += tx.nTxFee;
    }
    return true;
}

void CTxPool::ArrangeBlockTx(const uint256& hashFork, int64 nBlockTime, const uint256& hashBlock, std::size_t nMaxSize,
                             std::vector<CTransaction>& vtx, int64& nTotalTxFee, int nHeight)
{
    map<CDestination, int> mapVoteCert;
    std::map<CDestination, int64> mapVote;
    int64 nMinEnrollAmount = 0;
    // if (hashFork == pCoreProtocol->GetGenesisBlockHash())
    //{
    // if (!pBlockChain->GetDelegateCertTxCount(hashBlock, mapVoteCert))
    // {
    //     StdError("CTxPool", "ArrangeBlockTx: GetDelegateCertTxCount fail");
    //     return;
    // }

    // if (!pBlockChain->GetBlockDelegateVote(hashBlock, mapVote))
    // {
    //     StdError("CTxPool", "ArrangeBlockTx: GetBlockDelegateVote fail");
    //     return;
    // }

    // nMinEnrollAmount = pBlockChain->GetDelegateMinEnrollAmount(hashBlock);
    // if (nMinEnrollAmount < 0)
    // {
    //     StdError("CTxPool", "ArrangeBlockTx: GetDelegateMinEnrollAmount fail");
    //     return;
    // }
    //}

    mapPoolView[hashFork].ArrangeBlockTx(vtx, nTotalTxFee, nBlockTime, nMaxSize, mapVoteCert, mapVote, nMinEnrollAmount, false);
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

bool CTxPool::SynchronizeBlockChain(const CBlockChainUpdate& update, CTxSetChange& change)
{
    change.hashFork = update.hashFork;

    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);

    CTxPoolView viewInvolvedTx;
    CTxPoolView& txView = mapPoolView[update.hashFork];

    //int nHeight = update.nLastBlockHeight - update.vBlockAddNew.size() + 1;
    for (const CBlockEx& block : boost::adaptors::reverse(update.vBlockAddNew))
    {
        int nBlockHeight = block.GetBlockHeight();
        if (block.txMint.nAmount != 0)
        {
            change.vTxAddNew.push_back(CAssembledTx(block.txMint, nBlockHeight /*nHeight*/));
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
                    change.mapTxUpdate.insert(make_pair(txid, nBlockHeight));
                }
                else
                {
                    for (const CTxIn& txin : tx.vInput)
                    {
                        txView.InvalidateSpent(txin.prevout, viewInvolvedTx);
                    }
                    change.vTxAddNew.push_back(CAssembledTx(tx, nBlockHeight /*nHeight*/, txContxt.destIn, txContxt.GetValueIn()));
                }
            }
            else
            {
                change.mapTxUpdate.insert(make_pair(txid, nBlockHeight /*nHeight*/));
            }
        }
        //nHeight++;
    }

    vector<pair<uint256, vector<CTxIn>>> vTxRemove;
    //std::vector<CBlockEx> vBlockRemove = update.vBlockRemove;
    //std::reverse(vBlockRemove.begin(), vBlockRemove.end());
    //for (const CBlockEx& block : vBlockRemove)
    for (const CBlockEx& block : boost::adaptors::reverse(update.vBlockRemove))
    {
        for (int i = 0; i < block.vtx.size(); ++i)
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
    ArrangeBlockTx(update.hashFork, lastBlockEx.GetBlockTime(), lastBlockEx.GetHash(), MAX_BLOCK_TX_SIZE, vtx, nTotalFee, lastBlockEx.GetBlockHeight() + 1);

    auto& cache = mapTxCache[update.hashFork];
    cache.AddNew(lastBlockEx.GetHash(), vtx);

    mapPoolView[update.hashFork].SetLastBlock(lastBlockEx.GetHash(), lastBlockEx.GetBlockTime());
    return true;
}

void CTxPool::AddDestDelegate(const CDestination& destDeleage)
{
    //certTxDest.AddDelegate(destDeleage);
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
        ArrangeBlockTx(hashFork, nTime, hashBlock, MAX_BLOCK_SIZE, vtx, nTotalFee, nHeight + 1);
        mapTxCache[hashFork].AddNew(hashBlock, vtx);

        mapPoolView[hashFork].SetLastBlock(hashBlock, nTime);
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

void CTxPool::RemoveTx(const uint256& txid)
{
    map<uint256, CPooledTx>::iterator it = mapTx.find(txid);
    if (it == mapTx.end())
    {
        StdError("CTxPool", "RemoveTx: find fail, txid: %s", txid.GetHex().c_str());
        return;
    }

    //CPooledTx& tx = (*it).second;
    uint256 hashFork;
    int nHeight;
    if (!pBlockChain->GetBlockLocation(/*tx.hashAnchor*/ pCoreProtocol->GetGenesisBlockHash(), hashFork, nHeight))
    {
        StdError("CTxPool", "RemoveTx: GetBlockLocation fail, txid: %s", txid.GetHex().c_str());
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
        mapTx.erase(mi->hashTX);
    }
    StdTrace("CTxPool", "RemoveTx success, txid: %s", txid.GetHex().c_str());
}

} // namespace bigbang
