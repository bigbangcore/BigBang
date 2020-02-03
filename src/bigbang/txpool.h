// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_TXPOOL_H
#define BIGBANG_TXPOOL_H

#include "base.h"
#include "txpooldata.h"

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
    }

public:
    uint256 hashTX;
    uint64 nSequenceNumber;
    CPooledTx* ptx;
};

typedef boost::multi_index_container<
    CPooledTxLink,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::member<CPooledTxLink, uint256, &CPooledTxLink::hashTX>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CPooledTxLink, uint64, &CPooledTxLink::nSequenceNumber>>>>
    CPooledTxLinkSet;
typedef CPooledTxLinkSet::nth_index<0>::type CPooledTxLinkSetByTxHash;
typedef CPooledTxLinkSet::nth_index<1>::type CPooledTxLinkSetBySequenceNumber;

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
            setTxLinkIndex.erase(txid);
        }
    }
    void Clear()
    {
        setTxLinkIndex.clear();
        mapSpent.clear();
    }
    void InvalidateSpent(const CTxOutPoint& out, CTxPoolView& viewInvolvedTx);
    void ArrangeBlockTx(std::vector<CTransaction>& vtx, int64& nTotalTxFee, int64 nBlockTime, std::size_t nMaxSize);

public:
    CPooledTxLinkSet setTxLinkIndex;
    std::map<CTxOutPoint, CSpent> mapSpent;
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
    void ArrangeBlockTx(const uint256& hashFork, int64 nBlockTime, std::size_t nMaxSize,
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

protected:
    storage::CTxPoolData datTxPool;
    mutable boost::shared_mutex rwAccess;
    ICoreProtocol* pCoreProtocol;
    IBlockChain* pBlockChain;
    std::map<uint256, CTxPoolView> mapPoolView;
    std::map<uint256, CPooledTx> mapTx;
    uint64 nLastSequenceNumber;
};

} // namespace bigbang

#endif //BIGBANG_TXPOOL_H
