// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockdb.h"

#include "stream/datastream.h"

using namespace std;

namespace bigbang
{
namespace storage
{

//////////////////////////////
// CBlockDB

CBlockDB::CBlockDB()
{
}

CBlockDB::~CBlockDB()
{
}

bool CBlockDB::Initialize(const boost::filesystem::path& pathData)
{
    if (!dbFork.Initialize(pathData))
    {
        return false;
    }

    if (!dbBlockIndex.Initialize(pathData))
    {
        return false;
    }

    if (!dbTxIndex.Initialize(pathData))
    {
        return false;
    }

    if (!dbUnspent.Initialize(pathData))
    {
        return false;
    }

    if (!dbDelegate.Initialize(pathData))
    {
        return false;
    }

    return LoadFork();
}

void CBlockDB::Deinitialize()
{
    dbDelegate.Deinitialize();
    dbUnspent.Deinitialize();
    dbTxIndex.Deinitialize();
    dbBlockIndex.Deinitialize();
    dbFork.Deinitialize();
}

bool CBlockDB::RemoveAll()
{
    dbDelegate.Clear();
    dbUnspent.Clear();
    dbTxIndex.Clear();
    dbBlockIndex.Clear();
    dbFork.Clear();

    return true;
}

bool CBlockDB::AddNewForkContext(const CForkContext& ctxt)
{
    return dbFork.AddNewForkContext(ctxt);
}

bool CBlockDB::RetrieveForkContext(const uint256& hash, CForkContext& ctxt)
{
    ctxt.SetNull();
    return dbFork.RetrieveForkContext(hash, ctxt);
}

bool CBlockDB::ListForkContext(vector<CForkContext>& vForkCtxt)
{
    vForkCtxt.clear();
    return dbFork.ListForkContext(vForkCtxt);
}

bool CBlockDB::AddNewFork(const uint256& hash)
{
    if (!dbFork.UpdateFork(hash))
    {
        return false;
    }

    if (!dbTxIndex.LoadFork(hash))
    {
        dbFork.RemoveFork(hash);
        return false;
    }

    if (!dbUnspent.AddNewFork(hash))
    {
        dbFork.RemoveFork(hash);
        return false;
    }

    return true;
}

bool CBlockDB::RemoveFork(const uint256& hash)
{
    if (!dbUnspent.RemoveFork(hash))
    {
        return false;
    }

    return dbFork.RemoveFork(hash);
}

bool CBlockDB::ListFork(vector<pair<uint256, uint256>>& vFork)
{
    vFork.clear();
    return dbFork.ListFork(vFork);
}

bool CBlockDB::UpdateFork(const uint256& hash, const uint256& hashRefBlock, const uint256& hashForkBased,
                          const vector<pair<uint256, CTxIndex>>& vTxNew, const vector<uint256>& vTxDel,
                          const vector<CTxUnspent>& vAddNew, const vector<CTxOutPoint>& vRemove)
{
    if (!dbUnspent.Exists(hash))
    {
        return false;
    }

    bool fIgnoreTxDel = false;
    if (hashForkBased != hash && hashForkBased != 0)
    {
        if (!dbUnspent.Copy(hashForkBased, hash))
        {
            return false;
        }
        fIgnoreTxDel = true;
    }

    if (!dbFork.UpdateFork(hash, hashRefBlock))
    {
        return false;
    }

    if (!dbTxIndex.Update(hash, vTxNew, fIgnoreTxDel ? vector<uint256>() : vTxDel))
    {
        return false;
    }
    dbTxIndex.Flush(hash);

    dbTxIndex.Flush(hash);
    dbTxIndex.Flush(hash);

    if (!dbUnspent.Update(hash, vAddNew, vRemove))
    {
        return false;
    }
    dbUnspent.Flush(hash);

    dbUnspent.Flush(hash);
    dbUnspent.Flush(hash);

    return true;
}

bool CBlockDB::AddNewBlock(const CBlockOutline& outline)
{
    return dbBlockIndex.AddNewBlock(outline);
}

bool CBlockDB::RemoveBlock(const uint256& hash)
{
    return dbBlockIndex.RemoveBlock(hash);
}

bool CBlockDB::UpdateDelegateContext(const uint256& hash, const CDelegateContext& ctxtDelegate)
{
    return dbDelegate.AddNew(hash, ctxtDelegate);
}

bool CBlockDB::WalkThroughBlock(CBlockDBWalker& walker)
{
    return dbBlockIndex.WalkThroughBlock(walker);
}

bool CBlockDB::RetrieveTxIndex(const uint256& txid, CTxIndex& txIndex, uint256& fork)
{
    txIndex.SetNull();
    return dbTxIndex.Retrieve(txid, txIndex, fork);
}

bool CBlockDB::RetrieveTxIndex(const uint256& fork, const uint256& txid, CTxIndex& txIndex)
{
    txIndex.SetNull();
    return dbTxIndex.Retrieve(fork, txid, txIndex);
}

bool CBlockDB::RetrieveTxUnspent(const uint256& fork, const CTxOutPoint& out, CTxOut& unspent)
{
    return dbUnspent.Retrieve(fork, out, unspent);
}

bool CBlockDB::WalkThroughUnspent(const uint256& hashFork, CForkUnspentDBWalker& walker)
{
    return dbUnspent.WalkThrough(hashFork, walker);
}

bool CBlockDB::RetrieveDelegate(const uint256& hash, map<CDestination, int64>& mapDelegate)
{
    return dbDelegate.RetrieveDelegatedVote(hash, mapDelegate);
}

bool CBlockDB::RetrieveEnroll(int height, const vector<uint256>& vBlockRange,
                              map<CDestination, CDiskPos>& mapEnrollTxPos)
{
    return dbDelegate.RetrieveEnrollTx(height, vBlockRange, mapEnrollTxPos);
}

bool CBlockDB::LoadFork()
{
    vector<pair<uint256, uint256>> vFork;
    if (!dbFork.ListFork(vFork))
    {
        return false;
    }

    for (int i = 0; i < vFork.size(); i++)
    {
        if (!dbTxIndex.LoadFork(vFork[i].first))
        {
            return false;
        }

        if (!dbUnspent.AddNewFork(vFork[i].first))
        {
            return false;
        }
    }
    return true;
}

} // namespace storage
} // namespace bigbang
