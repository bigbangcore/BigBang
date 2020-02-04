// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/range/adaptor/reversed.hpp>

#include "leveldbeng.h"

#include "delegatedb.h"

using namespace std;
using namespace xengine;

namespace bigbang
{
namespace storage
{

//////////////////////////////
// CDelegateDB

bool CDelegateDB::Initialize(const boost::filesystem::path& pathData)
{
    CLevelDBArguments args;
    args.path = (pathData / "delegate").string();
    args.syncwrite = false;
    CLevelDBEngine* engine = new CLevelDBEngine(args);

    if (!Open(engine))
    {
        delete engine;
        return false;
    }

    return true;
}

void CDelegateDB::Deinitialize()
{
    cacheDelegate.Clear();
    Close();
}

bool CDelegateDB::AddNew(const uint256& hashBlock, const CDelegateContext& ctxtDelegate)
{
    if (!Write(hashBlock, ctxtDelegate))
    {
        return false;
    }

    cacheDelegate.AddNew(hashBlock, ctxtDelegate);
    return true;
}

bool CDelegateDB::Remove(const uint256& hashBlock)
{
    cacheDelegate.Remove(hashBlock);
    return Erase(hashBlock);
}

bool CDelegateDB::Retrieve(const uint256& hashBlock, CDelegateContext& ctxtDelegate)
{
    if (cacheDelegate.Retrieve(hashBlock, ctxtDelegate))
    {
        return true;
    }

    if (!Read(hashBlock, ctxtDelegate))
    {
        return false;
    }

    cacheDelegate.AddNew(hashBlock, ctxtDelegate);
    return true;
}

bool CDelegateDB::RetrieveDelegatedVote(const uint256& hashBlock, map<CDestination, int64>& mapVote)
{
    CDelegateContext ctxtDelegate;
    if (!Retrieve(hashBlock, ctxtDelegate))
    {
        return false;
    }
    mapVote = ctxtDelegate.mapVote;
    return true;
}

bool CDelegateDB::RetrieveEnrollTx(int height, const vector<uint256>& vBlockRange,
                                   map<CDestination, CDiskPos>& mapEnrollTxPos)
{
    for (const uint256& hash : boost::adaptors::reverse(vBlockRange))
    {
        CDelegateContext ctxtDelegate;
        if (!Retrieve(hash, ctxtDelegate))
        {
            return false;
        }

        map<int, map<CDestination, CDiskPos>>::iterator it = ctxtDelegate.mapEnrollTx.find(height);
        if (it != ctxtDelegate.mapEnrollTx.end())
        {
            mapEnrollTxPos.insert((*it).second.begin(), (*it).second.end());
        }
    }
    return true;
}

void CDelegateDB::Clear()
{
    cacheDelegate.Clear();
    RemoveAll();
}

} // namespace storage
} // namespace bigbang
