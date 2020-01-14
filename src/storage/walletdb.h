// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_WALLETDB_H
#define STORAGE_WALLETDB_H

#include <boost/function.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <xengine.h>

#include "key.h"
#include "wallettx.h"

namespace bigbang
{
namespace storage
{

class CWalletDBAddrWalker
{
public:
    virtual bool WalkPubkey(const crypto::CPubKey& pubkey, int version, const crypto::CCryptoCipher& cipher) = 0;
    virtual bool WalkTemplate(const CTemplateId& tid, const std::vector<unsigned char>& vchData) = 0;
};

class CWalletDBTxWalker
{
public:
    virtual bool Walk(const CWalletTx& wtx) = 0;
};

class CWalletDBTxSeqWalker
{
public:
    virtual bool Walk(const uint256& txid, const uint256& hashFork, const int nBlockHeight) = 0;
};

class CWalletAddrDB : public xengine::CKVDB
{
public:
    CWalletAddrDB() {}
    bool Initialize(const boost::filesystem::path& pathWallet);
    void Deinitialize();
    bool UpdateKey(const crypto::CPubKey& pubkey, int version, const crypto::CCryptoCipher& cipher);
    bool UpdateTemplate(const CTemplateId& tid, const std::vector<unsigned char>& vchData);
    bool EraseAddress(const CDestination& dest);
    bool WalkThroughAddress(CWalletDBAddrWalker& walker);

protected:
    bool AddressDBWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue, CWalletDBAddrWalker& walker);
};

class CWalletTxSeq
{
    friend class xengine::CStream;

public:
    CWalletTxSeq() {}
    CWalletTxSeq(const CWalletTx& wtx)
      : txid(wtx.txid), hashFork(wtx.hashFork), nBlockHeight(wtx.nBlockHeight) {}
    uint256 txid;
    uint256 hashFork;
    int nBlockHeight;

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(txid, opt);
        s.Serialize(hashFork, opt);
        s.Serialize(nBlockHeight, opt);
    }
};

class CWalletTxDB : public xengine::CKVDB
{
public:
    CWalletTxDB()
      : nSequence(0), nTxCount(0) {}
    bool Initialize(const boost::filesystem::path& pathWallet);
    void Deinitialize();
    bool Clear();
    void SetSequence(uint64 nSequenceIn)
    {
        nSequence = nSequenceIn;
    }
    void SetTxCount(std::size_t nTxCountIn)
    {
        nTxCount = nTxCountIn;
    }
    bool AddNewTx(const CWalletTx& wtx);
    bool UpdateTx(const std::vector<CWalletTx>& vWalletTx, const std::vector<uint256>& vRemove);
    bool RetrieveTx(const uint256& txid, CWalletTx& wtx);
    bool ExistsTx(const uint256& txid);
    std::size_t GetTxCount();
    bool WalkThroughTxSeq(CWalletDBTxSeqWalker& walker);
    bool WalkThroughTx(CWalletDBTxWalker& walker);

protected:
    bool TxSeqWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue, CWalletDBTxSeqWalker& walker);
    bool TxWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue, CWalletDBTxWalker& walker);
    bool Reset();

protected:
    uint64 nSequence;
    std::size_t nTxCount;
};

class CWalletTxCache
{
    typedef boost::multi_index_container<
        CWalletTx,
        boost::multi_index::indexed_by<
            boost::multi_index::sequenced<>,
            boost::multi_index::ordered_unique<boost::multi_index::member<CWalletTx, uint256, &CWalletTx::txid>>,
            boost::multi_index::ordered_non_unique<boost::multi_index::member<CWalletTx, uint256, &CWalletTx::hashFork>>>>
        CWalletTxList;
    typedef CWalletTxList::nth_index<1>::type CWalletTxListByTxid;
    typedef CWalletTxList::nth_index<2>::type CWalletTxListByFork;

public:
    bool Exists(const uint256& txid)
    {
        return (!!listWalletTx.get<1>().count(txid));
    }
    std::size_t Count()
    {
        return listWalletTx.size();
    }
    void Clear()
    {
        listWalletTx.clear();
    }
    void AddNew(const CWalletTx& wtx)
    {
        listWalletTx.push_back(wtx);
    }
    void Remove(const uint256& txid)
    {
        listWalletTx.get<1>().erase(txid);
    }
    void Update(const CWalletTx& wtx)
    {
        CWalletTxListByTxid& idxByTxid = listWalletTx.get<1>();
        CWalletTxListByTxid::iterator it = idxByTxid.find(wtx.txid);
        if (it != idxByTxid.end())
        {
            (*it).nFlags = wtx.nFlags;
        }
        else
        {
            listWalletTx.push_back(wtx);
        }
    }
    bool Get(const uint256& txid, CWalletTx& wtx)
    {
        CWalletTxListByTxid& idxByTxid = listWalletTx.get<1>();
        CWalletTxListByTxid::iterator it = idxByTxid.find(txid);
        if (it != idxByTxid.end())
        {
            wtx = (*it);
            wtx.nRefCount = 0;
            return true;
        }
        return false;
    }
    void ListTx(int nOffset, int nCount, std::vector<CWalletTx>& vWalletTx)
    {
        CWalletTxList::iterator it = listWalletTx.begin();
        while (it != listWalletTx.end() && nOffset--)
        {
            ++it;
        }
        for (; it != listWalletTx.end() && nCount--; ++it)
        {
            vWalletTx.push_back((*it));
        }
    }
    void ListForkTx(const uint256& hashFork, std::vector<uint256>& vForkTx)
    {
        CWalletTxListByFork& idxByFork = listWalletTx.get<2>();
        for (CWalletTxListByFork::iterator it = idxByFork.lower_bound(hashFork);
             it != idxByFork.upper_bound(hashFork); ++it)
        {
            vForkTx.push_back((*it).txid);
        }
    }

protected:
    CWalletTxList listWalletTx;
};

class CWalletDB
{
public:
    CWalletDB();
    ~CWalletDB();
    bool Initialize(const boost::filesystem::path& pathWallet);
    void Deinitialize();
    bool UpdateKey(const crypto::CPubKey& pubkey, int version, const crypto::CCryptoCipher& cipher);
    bool UpdateTemplate(const CTemplateId& tid, const std::vector<unsigned char>& vchData);
    bool WalkThroughAddress(CWalletDBAddrWalker& walker);

    bool AddNewTx(const CWalletTx& wtx);
    bool UpdateTx(const std::vector<CWalletTx>& vWalletTx, const std::vector<uint256>& vRemove = std::vector<uint256>());
    bool RetrieveTx(const uint256& txid, CWalletTx& wtx);
    bool ExistsTx(const uint256& txid);
    std::size_t GetTxCount();
    bool ListTx(const uint256& hashFork, const CDestination& dest, int nOffset, int nCount, std::vector<CWalletTx>& vWalletTx);
    bool ListRollBackTx(const uint256& hashFork, int nMinHeight, std::vector<uint256>& vForkTx);
    bool WalkThroughTx(CWalletDBTxWalker& walker);
    bool ClearTx();

protected:
    bool ListDBTx(int nOffset, int nCount, std::vector<CWalletTx>& vWalletTx);

protected:
    CWalletAddrDB dbAddr;
    CWalletTxDB dbWtx;
    CWalletTxCache txCache;
};

} // namespace storage
} // namespace bigbang

#endif //STORAGE_WALLETDB_H
