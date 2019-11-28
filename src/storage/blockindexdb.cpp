// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockindexdb.h"

#include "rocksdbeng.h"

using namespace std;
using namespace xengine;

namespace bigbang
{
namespace storage
{

//////////////////////////////
// CBlockIndexDB

bool CBlockIndexDB::Initialize(const boost::filesystem::path& pathData)
{
    CRocksDBArguments args;
    args.path = (pathData / "blockindex").string();
    args.syncwrite = false;
    CRocksDBEngine* engine = new CRocksDBEngine(args);

    if (!Open(engine))
    {
        delete engine;
        return false;
    }

    return true;
}

void CBlockIndexDB::Deinitialize()
{
    Close();
}

bool CBlockIndexDB::AddNewBlock(const CBlockOutline& outline)
{
    return Write(outline.GetBlockHash(), outline);
}

bool CBlockIndexDB::RemoveBlock(const uint256& hashBlock)
{
    return Erase(hashBlock);
}

bool CBlockIndexDB::WalkThroughBlock(CBlockDBWalker& walker)
{
    return WalkThrough(boost::bind(&CBlockIndexDB::LoadBlockWalker, this, _1, _2, boost::ref(walker)));
}

void CBlockIndexDB::Clear()
{
    RemoveAll();
}

bool CBlockIndexDB::LoadBlockWalker(CBufStream& ssKey, CBufStream& ssValue, CBlockDBWalker& walker)
{
    CBlockOutline outline;
    ssValue >> outline;
    return walker.Walk(outline);
}

} // namespace storage
} // namespace bigbang
