// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_TXPOOL_H
#define BIGBANG_TXPOOL_H

#include "base.h"
#include "txpooldata.h"

// This macro value is related to DPoS Weight value / PoW weight, if weight ratio changed, you must change it
#define CACHE_HEIGHT_INTERVAL 23

namespace bigbang
{

class CPooledTx : public CAssembledTx
{
public:
    uint64 nSequenceNumber;
    std::size_t nSerializeSize;
    uint64 nNextSequenceNumber;

public:
    CPooledTx()
    {
        SetNull();
    }
    CPooledTx(const CAssembledTx& tx, uint64 nSequenceNumberIn)
      : CAssembledTx(tx), nSequenceNumber(nSequenceNumberIn), nNextSequenceNumber(0)
    {
        nSerializeSize = xengine::GetSerializeSize(static_cast<const CTransaction&>(tx));
    }
    CPooledTx(const CTransaction& tx, int nBlockHeightIn, uint64 nSequenceNumberIn, const CDestination& destInIn = CDestination(), int64 nValueInIn = 0)
      : CAssembledTx(tx, nBlockHeightIn, destInIn, nValueInIn), nSequenceNumber(nSequenceNumberIn), nNextSequenceNumber(0)
    {
        nSerializeSize = xengine::GetSerializeSize(tx);
    }
    void SetNull() override
    {
        CAssembledTx::SetNull();
        nSequenceNumber = 0;
        nSerializeSize = 0;
        nNextSequenceNumber = 0;
    }
};

class CPooledTxLink
{
public:
    CPooledTxLink()
      : nSequenceNumber(0), ptx(nullptr) {}
    CPooledTxLink(CPooledTx* ptxin)
      : ptx(ptxin)
    {
        hashTX = ptx->GetHash();
        nSequenceNumber = ptx->nSequenceNumber;
        nType = ptx->nType;
    }

public:
    uint256 hashTX;
    uint64 nSequenceNumber;
    uint16 nType;
    CPooledTx* ptx;
};

class ComparePooledTxLinkByTxScore
{
public:
    bool operator()(const CPooledTxLink& a, const CPooledTxLink& b) const
    {
        return GetScore(a) > GetScore(b);
    }

private:
    double GetScore(const CPooledTxLink& link) const
    {
        return (double)((double)link.nType + (double)(1.0f / (double)(link.nSequenceNumber + 1)));
    }
};

struct tx_score
{
};

typedef boost::multi_index_container<
    CPooledTxLink,
    boost::multi_index::indexed_by<
        // sorted by Tx ID
        boost::multi_index::ordered_unique<boost::multi_index::member<CPooledTxLink, uint256, &CPooledTxLink::hashTX>>,
        // sorted by entry sequence
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CPooledTxLink, uint64, &CPooledTxLink::nSequenceNumber>>,
        // sorted by Tx Type
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CPooledTxLink, uint16, &CPooledTxLink::nType>>,
        // sorted by tx score
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<tx_score>,
            boost::multi_index::identity<CPooledTxLink>,
            ComparePooledTxLinkByTxScore>>>

    CPooledTxLinkSet;
typedef CPooledTxLinkSet::nth_index<0>::type CPooledTxLinkSetByTxHash;
typedef CPooledTxLinkSet::nth_index<1>::type CPooledTxLinkSetBySequenceNumber;
typedef CPooledTxLinkSet::nth_index<2>::type CPooledTxLinkSetByTxType;
typedef CPooledTxLinkSet::nth_index<3>::type CPooledTxLinkSetByTxScore;

typedef boost::multi_index_container<
    CPooledTxLink,
    boost::multi_index::indexed_by<
        // sorted by Tx ID
        boost::multi_index::ordered_unique<boost::multi_index::member<CPooledTxLink, uint256, &CPooledTxLink::hashTX>>,
        // sorted by entry sequence
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CPooledTxLink, uint64, &CPooledTxLink::nSequenceNumber>>>>

    CPooledCertTxLinkSet;
typedef CPooledCertTxLinkSet::nth_index<0>::type CPooledCertTxLinkSetByTxHash;
typedef CPooledCertTxLinkSet::nth_index<1>::type CPooledCertTxLinkSetBySequenceNumber;

class CTxPoolView
{
public:
    class CSpent : public CTxOut
    {
    public:
        CSpent()
          : txidNextTx(uint64(0)) {}
        CSpent(const CTxOut& output)
          : CTxOut(output), txidNextTx(uint64(0)) {}
        CSpent(const uint256& txidNextTxIn)
          : txidNextTx(txidNextTxIn) {}
        void SetSpent(const uint256& txidNextTxIn)
        {
            *this = CSpent(txidNextTxIn);
        }
        void SetUnspent(const CTxOut& output)
        {
            *this = CSpent(output);
        }
        bool IsSpent() const
        {
            return (txidNextTx != 0);
        };

    public:
        uint256 txidNextTx;
    };

public:
    std::size_t Count() const
    {
        return setTxLinkIndex.size();
    }
    bool Exists(const uint256& txid) const
    {
        return (!!setTxLinkIndex.count(txid));
    }
    CPooledTx* Get(uint256 txid)
    {
        CPooledTxLinkSetByTxHash& idxTx = setTxLinkIndex.get<0>();
        CPooledTxLinkSetByTxHash::iterator mi = idxTx.find(txid);
        if (mi == idxTx.end())
        {
            return nullptr;
        }
        return (*mi).ptx;
    }
    CPooledTx* Get(uint64 nSeq)
    {
        CPooledTxLinkSetBySequenceNumber& idxTx = setTxLinkIndex.get<1>();
        CPooledTxLinkSetBySequenceNumber::iterator mi = idxTx.find(nSeq);
        if (mi == idxTx.end())
        {
            return nullptr;
        }
        return (*mi).ptx;
    }
    bool IsSpent(const CTxOutPoint& out) const
    {
        std::map<CTxOutPoint, CSpent>::const_iterator it = mapSpent.find(out);
        if (it != mapSpent.end())
        {
            return (*it).second.IsSpent();
        }
        return false;
    }
    bool GetUnspent(const CTxOutPoint& out, CTxOut& unspent) const
    {
        std::map<CTxOutPoint, CSpent>::const_iterator it = mapSpent.find(out);
        if (it != mapSpent.end() && !(*it).second.IsSpent())
        {
            unspent = static_cast<CTxOut>((*it).second);
            return (!unspent.IsNull());
        }
        return false;
    }
    bool GetSpent(const CTxOutPoint& out, uint256& txidNextTxRet) const
    {
        std::map<CTxOutPoint, CSpent>::const_iterator it = mapSpent.find(out);
        if (it != mapSpent.end())
        {
            txidNextTxRet = (*it).second.txidNextTx;
            return (*it).second.IsSpent();
        }
        return false;
    }
    void SetUnspent(const CTxOutPoint& out)
    {
        CPooledTx* pTx = Get(out.hash);
        if (pTx != nullptr)
        {
            mapSpent[out].SetUnspent(pTx->GetOutput(out.n));
        }
        else
        {
            mapSpent.erase(out);
        }
    }
    void SetSpent(const CTxOutPoint& out, const uint256& txidNextTxIn)
    {
        mapSpent[out].SetSpent(txidNextTxIn);
    }
    bool AddNew(const uint256& txid, CPooledTx& tx);
    void Remove(const uint256& txid)
    {
        CPooledTx* pTx = Get(txid);
        if (pTx != nullptr)
        {
            for (std::size_t i = 0; i < pTx->vInput.size(); i++)
            {
                SetUnspent(pTx->vInput[i].prevout);
            }
            xengine::StdTrace("CTxPoolView", "Remove: setTxLinkIndex erase, txid: %s, seq: %ld",
                              txid.GetHex().c_str(), pTx->nSequenceNumber);
            setTxLinkIndex.erase(txid);
        }
    }
    void Clear()
    {
        setTxLinkIndex.clear();
        mapSpent.clear();
    }
    void InvalidateSpent(const CTxOutPoint& out, CTxPoolView& viewInvolvedTx);
    void ArrangeBlockTx(std::vector<CTransaction>& vtx, int64& nTotalTxFee, int64 nBlockTime, std::size_t nMaxSize, std::map<CDestination, int>& mapVoteCert,
                        std::map<CDestination, int64>& mapVote, int64 nWeightRatio);

private:
    void GetAllPrevTxLink(const CPooledTxLink& link, std::vector<CPooledTxLink>& prevLinks);
    bool AddArrangeBlockTx(std::vector<CTransaction>& vtx, int64& nTotalTxFee, int64 nBlockTime, std::size_t nMaxSize, std::size_t& nTotalSize,
                           std::map<CDestination, int>& mapVoteCert, std::set<uint256>& setUnTx, CPooledTx* ptx, std::map<CDestination, int64>& mapVote, int64 nWeightRatio);

public:
    CPooledTxLinkSet setTxLinkIndex;
    std::map<CTxOutPoint, CSpent> mapSpent;
};

class CTxCache
{
public:
    CTxCache(size_t nHeightIntervalIn = 0)
      : nHeightInterval(nHeightIntervalIn) {}
    CTxCache(const CTxCache& cache)
      : nHeightInterval(cache.nHeightInterval), mapCache(cache.mapCache) {}
    bool Exists(const uint256& hash)
    {
        return mapCache.count(hash) > 0;
    }
    void AddNew(const uint256& hash, const std::vector<CTransaction>& vtxIn)
    {
        mapCache[hash] = vtxIn;

        const uint256& highestHash = mapCache.rbegin()->first;
        uint32 upperHeight = CBlock::GetBlockHeightByHash(highestHash);

        if (upperHeight > nHeightInterval)
        {
            uint32 lowerHeight = upperHeight - (nHeightInterval - 1);

            for (auto iter = mapCache.begin(); iter != mapCache.end();)
            {
                uint32 height = CBlock::GetBlockHeightByHash(iter->first);
                if (height < lowerHeight)
                {
                    iter = mapCache.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }
        }
    }
    bool Retrieve(const uint256& hash, std::vector<CTransaction>& vtx)
    {
        if (mapCache.find(hash) != mapCache.end())
        {
            vtx = mapCache[hash];
            return true;
        }
        return false;
    }
    void Remove(const uint256& hash)
    {
        mapCache.erase(hash);
    }
    void Clear()
    {
        mapCache.clear();
    }

private:
    size_t nHeightInterval;
    std::map<uint256, std::vector<CTransaction>> mapCache;
};

class CTxPool : public ITxPool
{
public:
    CTxPool();
    ~CTxPool();
    bool Exists(const uint256& txid) override;
    void Clear() override;
    std::size_t Count(const uint256& fork) const override;
    Errno Push(const CTransaction& tx, uint256& hashFork, CDestination& destIn, int64& nValueIn) override;
    void Pop(const uint256& txid) override;
    bool Get(const uint256& txid, CTransaction& tx) const override;
    void ListTx(const uint256& hashFork, std::vector<std::pair<uint256, std::size_t>>& vTxPool) override;
    void ListTx(const uint256& hashFork, std::vector<uint256>& vTxPool) override;
    bool FilterTx(const uint256& hashFork, CTxFilter& filter) override;
    bool ArrangeBlockTx(const uint256& hashFork, const uint256& hashPrev, int64 nBlockTime, std::size_t nMaxSize,
                        std::vector<CTransaction>& vtx, int64& nTotalTxFee) override;
    bool FetchInputs(const uint256& hashFork, const CTransaction& tx, std::vector<CTxOut>& vUnspent) override;
    bool SynchronizeBlockChain(const CBlockChainUpdate& update, CTxSetChange& change) override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;
    bool LoadData();
    bool SaveData();
    Errno AddNew(CTxPoolView& txView, const uint256& txid, const CTransaction& tx, const uint256& hashFork, int nForkHeight);
    uint64 GetSequenceNumber()
    {
        if (mapTx.empty())
        {
            nLastSequenceNumber = 0;
        }
        return ((++nLastSequenceNumber) << 24);
    }
    void ArrangeBlockTx(const uint256& hashFork, int64 nBlockTime, const uint256& hashBlock, std::size_t nMaxSize,
                        std::vector<CTransaction>& vtx, int64& nTotalTxFee);

protected:
    storage::CTxPoolData datTxPool;
    mutable boost::shared_mutex rwAccess;
    ICoreProtocol* pCoreProtocol;
    IBlockChain* pBlockChain;
    std::map<uint256, CTxPoolView> mapPoolView;
    std::map<uint256, CPooledTx> mapTx;
    uint64 nLastSequenceNumber;
    std::map<uint256, CTxCache> mapTxCache;
};

} // namespace bigbang

#endif //BIGBANG_TXPOOL_H
