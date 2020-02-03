// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_BLOCKINDEXDB_H
#define STORAGE_BLOCKINDEXDB_H

#include "block.h"
#include "xengine.h"

namespace bigbang
{
namespace storage
{

class CBlockDBWalker
{
public:
    virtual bool Walk(CBlockOutline& outline) = 0;
};

class CBlockIndexDB : public xengine::CKVDB
{
public:
    CBlockIndexDB() {}
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool AddNewBlock(const CBlockOutline& outline);
    bool RemoveBlock(const uint256& hashBlock);
    bool WalkThroughBlock(CBlockDBWalker& walker);
    void Clear();

protected:
    bool LoadBlockWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                         CBlockDBWalker& walker);
};

} // namespace storage
} // namespace bigbang

#endif //STORAGE_BLOCKINDEXDB_H
