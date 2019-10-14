// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_TXPOOL_H
#define BIGBANG_TXPOOL_H

#include "base.h"
#include "message.h"
#include "txpooldata.h"

namespace bigbang
{

class CPooledTx : public CAssembledTx
{
public:
    std::size_t nSequenceNumber;
    std::size_t nSerializeSize;

public:
    CPooledTx()
    {
        SetNull();
    }
    CPooledTx(const CAssembledTx& tx, std::size_t nSequenceNumberIn)
      : CAssembledTx(tx), nSequenceNumber(nSequenceNumberIn)
    {
        nSerializeSize = xengine::GetSerializeSize(static_cast<const CTransaction&>(tx));
    }
    CPooledTx(const CTransaction& tx, int nBlockHeightIn, std::size_t nSequenceNumberIn, const CDestination& destInIn = CDestination(), int64 nValueInIn = 0)
      : CAssembledTx(tx, nBlockHeightIn, destInIn, nValueInIn), nSequenceNumber(nSequenceNumberIn)
    {
        nSerializeSize = xengine::GetSerializeSize(tx);
    }
    void SetNull() override
    {
        CAssembledTx::SetNull();
        nSequenceNumber = 0;
        nSerializeSize = 0;
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
    std::size_t nSequenceNumber;
    CPooledTx* ptx;
};

typedef boost::multi_index_container<
    CPooledTxLink,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::member<CPooledTxLink, uint256, &CPooledTxLink::hashTX>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CPooledTxLink, std::size_t, &CPooledTxLink::nSequenceNumber>>>>
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
        return setTxLinkIndex.count(txid) != 0;
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
    void AddNew(const uint256& txid, CPooledTx& tx)
    {
        CPooledTxLinkSetByTxHash& idxTx = setTxLinkIndex.get<0>();
        CPooledTxLinkSetByTxHash::iterator mi = idxTx.find(txid);
        if (mi != idxTx.end())
        {
            setTxLinkIndex.erase(txid);
        }
        setTxLinkIndex.insert(CPooledTxLink(&tx));
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
    }
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
    void InvalidateSpent(const CTxOutPoint& out, std::vector<uint256>& vInvolvedTx);
    void ArrangeBlockTx(std::vector<CTransaction>& vtx, int64& nTotalTxFee, int64 nBlockTime, std::size_t nMaxSize) const;

public:
    CPooledTxLinkSet setTxLinkIndex;
    std::map<CTxOutPoint, CSpent> mapSpent;
};

class CTxPool : public ITxPool
{
public:
    CTxPool();
    ~CTxPool();
    bool Exists(const uint256& txid) const override;
    std::size_t Count(const uint256& fork) const override;
    bool Get(const uint256& txid, CTransaction& tx) const override;
    void ListTx(const uint256& hashFork, std::vector<std::pair<uint256, std::size_t>>& vTxPool) const override;
    void ListTx(const uint256& hashFork, std::vector<uint256>& vTxPool) const override;
    bool FilterTx(const uint256& hashFork, CTxFilter& filter) const override;
    void ArrangeBlockTx(const uint256& hashFork, int64 nBlockTime, std::size_t nMaxSize,
                        std::vector<CTransaction>& vtx, int64& nTotalTxFee) const override;
    bool FetchInputs(const uint256& hashFork, const CTransaction& tx, std::vector<CTxOut>& vUnspent) const override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;
    bool LoadData();
    bool SaveData();
    Errno AddNew(CTxPoolView& txView, const uint256& txid, const CTransaction& tx, const uint256& hashFork, int nForkHeight);
    std::size_t GetSequenceNumber()
    {
        if (mapTx.empty())
        {
            nLastSequenceNumber = 0;
        }
        return ++nLastSequenceNumber;
    }

    void Clear() override;
    Errno Push(const CTransaction& tx, uint256& hashFork, CDestination& destIn, int64& nValueIn) override;
    void Pop(const uint256& txid) override;
    bool SynchronizeWorldLine(const CWorldLineUpdate& update, CTxSetChange& change) override;

protected:
    storage::CTxPoolData datTxPool;
    mutable boost::shared_mutex rwAccess;
    ICoreProtocol* pCoreProtocol;
    IWorldLineController* pWorldLineCtrl;
    std::map<uint256, CTxPoolView> mapPoolView;
    std::unordered_map<uint256, CPooledTx> mapTx;
    std::size_t nLastSequenceNumber;
};

class CTxPoolController : public ITxPoolController
{
public:
    CTxPoolController();
    ~CTxPoolController();
    void Clear() override;
    Errno Push(const CTransaction& tx, uint256& hashFork, CDestination& destIn, int64& nValueIn) override;
    void Pop(const uint256& txid) override;
    bool SynchronizeWorldLine(const CWorldLineUpdate& update, CTxSetChange& change) override;

public:
    bool Exists(const uint256& txid) override;
    std::size_t Count(const uint256& fork) const override;
    bool Get(const uint256& txid, CTransaction& tx) const override;
    void ListTx(const uint256& hashFork, std::vector<std::pair<uint256, std::size_t>>& vTxPool) override;
    void ListTx(const uint256& hashFork, std::vector<uint256>& vTxPool) override;
    bool FilterTx(const uint256& hashFork, CTxFilter& filter) override;
    void ArrangeBlockTx(const uint256& hashFork, int64 nBlockTime, std::size_t nMaxSize,
                        std::vector<CTransaction>& vtx, int64& nTotalTxFee) override;
    bool FetchInputs(const uint256& hashFork, const CTransaction& tx, std::vector<CTxOut>& vUnspent) override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;

    void HandleAddTx(const CAddTxMessage& msg);
    void HandleRemoveTx(const CRemoveTxMessage& msg);
    void HandleClearTx(const CClearTxMessage& msg);
    void HandleAddedBlock(const CAddedBlockMessage& msg);
};

} // namespace bigbang

#endif //BIGBANG_TXPOOL_H
