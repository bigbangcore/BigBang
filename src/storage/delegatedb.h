// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_DELEGATEDB_H
#define STORAGE_DELEGATEDB_H

#include <map>

#include "destination.h"
#include "timeseries.h"
#include "uint256.h"
#include "xengine.h"

namespace bigbang
{
namespace storage
{

class CDelegateContext
{
    friend class xengine::CStream;

public:
    std::map<CDestination, int64> mapVote;
    std::map<int, std::map<CDestination, CDiskPos>> mapEnrollTx;

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(mapVote, opt);
        s.Serialize(mapEnrollTx, opt);
    }
};

class CDelegateDB : public xengine::CKVDB
{
public:
    CDelegateDB()
      : cacheDelegate(MAX_CACHE_COUNT) {}
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool AddNew(const uint256& hashBlock, const CDelegateContext& ctxtDelegate);
    bool Remove(const uint256& hashBlock);
    bool RetrieveDelegatedVote(const uint256& hashBlock, std::map<CDestination, int64>& mapVote);
    bool RetrieveEnrollTx(int height, const std::vector<uint256>& vBlockRange,
                          std::map<CDestination, CDiskPos>& mapEnrollTxPos);
    void Clear();

protected:
    bool Retrieve(const uint256& hashBlock, CDelegateContext& ctxtDelegate);

protected:
    enum
    {
        MAX_CACHE_COUNT = 64,
    };
    xengine::CCache<uint256, CDelegateContext> cacheDelegate;
};

} // namespace storage
} // namespace bigbang

#endif //STORAGE_DELEGATEDB_H
