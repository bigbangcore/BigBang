// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletdb.h"

#include <boost/bind.hpp>

#include "leveldbeng.h"

using namespace std;
using namespace xengine;

namespace bigbang
{
namespace storage
{

//////////////////////////////
// CWalletAddrDB

bool CWalletAddrDB::Initialize(const boost::filesystem::path& pathWallet)
{
    CLevelDBArguments args;
    args.path = (pathWallet / "addr").string();
    args.syncwrite = true;
    args.files = 8;
    args.cache = 1 << 20;

    CLevelDBEngine* engine = new CLevelDBEngine(args);

    if (!Open(engine))
    {
        delete engine;
        return false;
    }

    return true;
}

void CWalletAddrDB::Deinitialize()
{
    Close();
}

bool CWalletAddrDB::UpdateKey(const crypto::CPubKey& pubkey, int version, const crypto::CCryptoCipher& cipher)
{
    vector<unsigned char> vch;
    vch.resize(4 + 48 + 8);
    memcpy(&vch[0], &version, 4);
    memcpy(&vch[4], cipher.encrypted, 48);
    memcpy(&vch[52], &cipher.nonce, 8);

    return Write(CDestination(pubkey), vch);
}

bool CWalletAddrDB::UpdateTemplate(const CTemplateId& tid, const vector<unsigned char>& vchData)
{
    return Write(CDestination(tid), vchData);
}

bool CWalletAddrDB::EraseAddress(const CDestination& dest)
{
    return Erase(dest);
}

bool CWalletAddrDB::WalkThroughAddress(CWalletDBAddrWalker& walker)
{
    return WalkThrough(boost::bind(&CWalletAddrDB::AddressDBWalker, this, _1, _2, boost::ref(walker)));
}

bool CWalletAddrDB::AddressDBWalker(CBufStream& ssKey, CBufStream& ssValue, CWalletDBAddrWalker& walker)
{
    CDestination dest;
    vector<unsigned char> vch;
    ssKey >> dest;
    ssValue >> vch;

    if (dest.IsTemplate())
    {
        return walker.WalkTemplate(dest.GetTemplateId(), vch);
    }

    crypto::CPubKey pubkey;
    if (dest.GetPubKey(pubkey) && vch.size() == 4 + 48 + 8)
    {
        int version;
        crypto::CCryptoCipher cipher;

        memcpy(&version, &vch[0], 4);
        memcpy(cipher.encrypted, &vch[4], 48);
        memcpy(&cipher.nonce, &vch[52], 8);

        return walker.WalkPubkey(pubkey, version, cipher);
    }

    return false;
}

//////////////////////////////
// CWalletTxDB

bool CWalletTxDB::Initialize(const boost::filesystem::path& pathWallet)
{
    CLevelDBArguments args;
    args.path = (pathWallet / "wtx").string();

    CLevelDBEngine* engine = new CLevelDBEngine(args);

    if (!Open(engine))
    {
        delete engine;
        return false;
    }

    if (!Read(string("txcount"), nTxCount) || !Read(string("sequence"), nSequence))
    {
        return Reset();
    }

    return true;
}

void CWalletTxDB::Deinitialize()
{
    Close();
}

bool CWalletTxDB::Clear()
{
    RemoveAll();
    return Reset();
}

bool CWalletTxDB::AddNewTx(const CWalletTx& wtx)
{
    pair<uint64, CWalletTx> pairWalletTx;
    if (Read(make_pair(string("wtx"), wtx.txid), pairWalletTx))
    {
        pairWalletTx.second = wtx;
        return Write(make_pair(string("wtx"), wtx.txid), pairWalletTx);
    }

    pairWalletTx.first = nSequence++;
    pairWalletTx.second = wtx;

    if (!TxnBegin())
    {
        return false;
    }

    if (!Write(make_pair(string("wtx"), wtx.txid), pairWalletTx)
        || !Write(make_pair(string("seq"), BSwap64(pairWalletTx.first)), CWalletTxSeq(wtx))
        || !Write(string("txcount"), nTxCount + 1)
        || !Write(string("sequence"), nSequence))
    {
        TxnAbort();
        return false;
    }

    if (!TxnCommit())
    {
        return false;
    }

    ++nTxCount;

    return true;
}

bool CWalletTxDB::UpdateTx(const vector<CWalletTx>& vWalletTx, const vector<uint256>& vRemove)
{
    int nTxAddNew = 0;
    vector<pair<uint64, CWalletTx>> vTxUpdate;
    vTxUpdate.reserve(vWalletTx.size());

    for (const CWalletTx& wtx : vWalletTx)
    {
        pair<uint64, CWalletTx> pairWalletTx;
        if (Read(make_pair(string("wtx"), wtx.txid), pairWalletTx))
        {
            vTxUpdate.push_back(make_pair(pairWalletTx.first, wtx));
        }
        else
        {
            vTxUpdate.push_back(make_pair(nSequence++, wtx));
            ++nTxAddNew;
        }
    }

    vector<pair<uint64, uint256>> vTxRemove;
    vTxRemove.reserve(vRemove.size());

    for (const uint256& txid : vRemove)
    {
        pair<uint64, CWalletTx> pairWalletTx;
        if (Read(make_pair(string("wtx"), txid), pairWalletTx))
        {
            vTxRemove.push_back(make_pair(pairWalletTx.first, txid));
        }
    }

    if (!TxnBegin())
    {
        StdLog("CWalletTxDB", "UpdateTx: TxnBegin fail.");
        return false;
    }

    for (int i = 0; i < vTxUpdate.size(); i++)
    {
        pair<uint64, CWalletTx>& pairWalletTx = vTxUpdate[i];
        if (!Write(make_pair(string("wtx"), pairWalletTx.second.txid), pairWalletTx)
            || !Write(make_pair(string("seq"), BSwap64(pairWalletTx.first)), CWalletTxSeq(pairWalletTx.second)))
        {
            StdLog("CWalletTxDB", "UpdateTx: Write wtx or seq fail.");
            TxnAbort();
            return false;
        }
    }

    for (int i = 0; i < vTxRemove.size(); i++)
    {
        if (!Erase(make_pair(string("wtx"), vTxRemove[i].second))
            || !Erase(make_pair(string("seq"), BSwap64(vTxRemove[i].first))))
        {
            StdLog("CWalletTxDB", "UpdateTx: Erase fail.");
            TxnAbort();
            return false;
        }
    }

    if (!Write(string("txcount"), nTxCount + nTxAddNew - vTxRemove.size())
        || !Write(string("sequence"), nSequence))
    {
        StdLog("CWalletTxDB", "UpdateTx: Write txcount or sequence fail.");
        TxnAbort();
        return false;
    }

    if (!TxnCommit())
    {
        StdLog("CWalletTxDB", "UpdateTx: TxnCommit fail.");
        return false;
    }

    nTxCount += nTxAddNew - vTxRemove.size();

    return true;
}

bool CWalletTxDB::RetrieveTx(const uint256& txid, CWalletTx& wtx)
{
    pair<uint64, CWalletTx> pairWalletTx;
    if (!Read(make_pair(string("wtx"), txid), pairWalletTx))
    {
        return false;
    }
    wtx = pairWalletTx.second;
    return true;
}

bool CWalletTxDB::ExistsTx(const uint256& txid)
{
    pair<uint64, CWalletTx> pairWalletTx;
    return Read(make_pair(string("wtx"), txid), pairWalletTx);
}

size_t CWalletTxDB::GetTxCount()
{
    return nTxCount;
}

bool CWalletTxDB::WalkThroughTxSeq(CWalletDBTxSeqWalker& walker)
{
    return WalkThrough(boost::bind(&CWalletTxDB::TxSeqWalker, this, _1, _2, boost::ref(walker)),
                       make_pair(string("seq"), BSwap64(uint64(0))));
}

bool CWalletTxDB::WalkThroughTx(CWalletDBTxWalker& walker)
{
    return WalkThrough(boost::bind(&CWalletTxDB::TxWalker, this, _1, _2, boost::ref(walker)),
                       make_pair(string("seq"), BSwap64(uint64(0))));
}

bool CWalletTxDB::TxSeqWalker(CBufStream& ssKey, CBufStream& ssValue, CWalletDBTxSeqWalker& walker)
{
    string strPrefix;
    uint64 nSeqNum;
    CWalletTxSeq txSeq;

    ssKey >> strPrefix;
    if (strPrefix != "seq")
    {
        StdLog("CWalletTxDB", "TxSeqWalker: strPrefix != seq, strPrefix: %s", strPrefix.c_str());
        return false;
    }

    ssKey >> nSeqNum;
    ssValue >> txSeq;

    return walker.Walk(txSeq.txid, txSeq.hashFork, txSeq.nBlockHeight);
}

bool CWalletTxDB::TxWalker(CBufStream& ssKey, CBufStream& ssValue, CWalletDBTxWalker& walker)
{
    string strPrefix;
    uint64 nSeqNum;
    CWalletTxSeq txSeq;

    ssKey >> strPrefix;
    if (strPrefix != "seq")
    {
        StdLog("CWalletTxDB", "TxWalker: strPrefix != seq, strPrefix: %s", strPrefix.c_str());
        return false;
    }

    ssKey >> nSeqNum;
    ssValue >> txSeq;

    CWalletTx wtx;
    if (!RetrieveTx(txSeq.txid, wtx))
    {
        StdLog("CWalletTxDB", "TxWalker: RetrieveTx fail, txid: %s", txSeq.txid.GetHex().c_str());
        return false;
    }
    return walker.Walk(wtx);
}

bool CWalletTxDB::Reset()
{
    nSequence = 0;
    nTxCount = 0;

    if (!TxnBegin())
    {
        return false;
    }

    if (!Write(string("txcount"), size_t(0)))
    {
        TxnAbort();
        return false;
    }

    if (!Write(string("sequence"), nSequence))
    {
        TxnAbort();
        return false;
    }

    return TxnCommit();
}

//////////////////////////////
// CWalletDBListTxSeqWalker

class CWalletDBListTxSeqWalker : public CWalletDBTxSeqWalker
{
public:
    CWalletDBListTxSeqWalker(int nOffsetIn, int nMaxCountIn)
      : nOffset(nOffsetIn), nMaxCount(nMaxCountIn), nIndex(0)
    {
    }
    bool Walk(const uint256& txid, const uint256& hashFork, const int nBlockHeight) override
    {
        if (nIndex++ < nOffset)
        {
            return true;
        }
        vWalletTxid.push_back(txid);
        return (vWalletTxid.size() < nMaxCount);
    }

public:
    int nOffset;
    int nMaxCount;
    int nIndex;
    vector<uint256> vWalletTxid;
};

//////////////////////////////
// CWalletDBRollBackTxSeqWalker

class CWalletDBRollBackTxSeqWalker : public CWalletDBTxSeqWalker
{
public:
    CWalletDBRollBackTxSeqWalker(const uint256& hashForkIn, int nMinHeightIn, vector<uint256>& vForkTxIn)
      : hashFork(hashForkIn), nMinHeight(nMinHeightIn), vForkTx(vForkTxIn)
    {
    }
    bool Walk(const uint256& txidIn, const uint256& hashForkIn, const int nBlockHeightIn) override
    {
        if (hashForkIn == hashFork && (nBlockHeightIn < 0 || nBlockHeightIn >= nMinHeight))
        {
            vForkTx.push_back(txidIn);
        }
        return true;
    }

public:
    uint256 hashFork;
    int nMinHeight;
    vector<uint256>& vForkTx;
};

//////////////////////////////
// CWalletDB

CWalletDB::CWalletDB()
{
}

CWalletDB::~CWalletDB()
{
    Deinitialize();
}

bool CWalletDB::Initialize(const boost::filesystem::path& pathWallet)
{
    if (!boost::filesystem::exists(pathWallet))
    {
        boost::filesystem::create_directories(pathWallet);
    }

    if (!boost::filesystem::is_directory(pathWallet))
    {
        return false;
    }

    if (!dbAddr.Initialize(pathWallet))
    {
        return false;
    }

    if (!dbWtx.Initialize(pathWallet))
    {
        return false;
    }

    return true;
}

void CWalletDB::Deinitialize()
{
    FlushTx();

    dbWtx.Deinitialize();
    dbAddr.Deinitialize();
}

bool CWalletDB::UpdateKey(const crypto::CPubKey& pubkey, int version, const crypto::CCryptoCipher& cipher)
{
    return dbAddr.UpdateKey(pubkey, version, cipher);
}

bool CWalletDB::UpdateTemplate(const CTemplateId& tid, const vector<unsigned char>& vchData)
{
    return dbAddr.UpdateTemplate(tid, vchData);
}

bool CWalletDB::WalkThroughAddress(CWalletDBAddrWalker& walker)
{
    return dbAddr.WalkThroughAddress(walker);
}

bool CWalletDB::AddNewTx(const CWalletTx& wtx)
{
    /*if (wtx.nBlockHeight < 0)
    {
        txCache.AddNew(wtx);
        return true;
    }*/

    return dbWtx.AddNewTx(wtx);
}

bool CWalletDB::UpdateTx(const vector<CWalletTx>& vWalletTx, const vector<uint256>& vRemove)
{
    if (!dbWtx.UpdateTx(vWalletTx, vRemove))
    {
        StdLog("CWalletDB", "UpdateTx fail.");
        return false;
    }
    for (const CWalletTx& wtx : vWalletTx)
    {
        txCache.Remove(wtx.txid);
    }
    for (const uint256& txid : vRemove)
    {
        txCache.Remove(txid);
    }
    return true;
}

bool CWalletDB::RetrieveTx(const uint256& txid, CWalletTx& wtx)
{
    if (txCache.Get(txid, wtx))
    {
        return true;
    }

    return dbWtx.RetrieveTx(txid, wtx);
}

bool CWalletDB::ExistsTx(const uint256& txid)
{
    if (txCache.Exists(txid))
    {
        return true;
    }

    return dbWtx.ExistsTx(txid);
}

size_t CWalletDB::GetTxCount()
{
    return dbWtx.GetTxCount() + txCache.Count();
}

bool CWalletDB::ListTx(int nOffset, int nCount, vector<CWalletTx>& vWalletTx)
{
    size_t nDBTx = dbWtx.GetTxCount();

    if (nOffset < nDBTx)
    {
        if (!ListDBTx(nOffset, nCount, vWalletTx))
        {
            return false;
        }
        if (vWalletTx.size() < nCount)
        {
            txCache.ListTx(0, nCount - vWalletTx.size(), vWalletTx);
        }
    }
    else
    {
        txCache.ListTx(nOffset - nDBTx, nCount, vWalletTx);
    }
    return true;
}

bool CWalletDB::ListDBTx(int nOffset, int nCount, vector<CWalletTx>& vWalletTx)
{
    CWalletDBListTxSeqWalker walker(nOffset, nCount);
    if (!dbWtx.WalkThroughTxSeq(walker))
    {
        return false;
    }

    for (const uint256& txid : walker.vWalletTxid)
    {
        CWalletTx wtx;
        if (!dbWtx.RetrieveTx(txid, wtx))
        {
            return false;
        }
        vWalletTx.push_back(wtx);
    }

    return true;
}

bool CWalletDB::ListRollBackTx(const uint256& hashFork, int nMinHeight, vector<uint256>& vForkTx)
{
    CWalletDBRollBackTxSeqWalker walker(hashFork, nMinHeight, vForkTx);
    if (!dbWtx.WalkThroughTxSeq(walker))
    {
        return false;
    }
    txCache.ListForkTx(hashFork, vForkTx);
    return true;
}

bool CWalletDB::WalkThroughTx(CWalletDBTxWalker& walker)
{
    return dbWtx.WalkThroughTx(walker);
}

bool CWalletDB::ClearTx()
{
    txCache.Clear();
    return dbWtx.Clear();
}

void CWalletDB::FlushTx()
{
    vector<CWalletTx> vWalletTx;
    txCache.ListTx(0, -1, vWalletTx);
    if (!vWalletTx.empty())
    {
        UpdateTx(vWalletTx);
    }

    txCache.Clear();
}

} // namespace storage
} // namespace bigbang
