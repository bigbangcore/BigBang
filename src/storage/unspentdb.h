// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_UNSPENTDB_H
#define STORAGE_UNSPENTDB_H

#include <boost/thread/thread.hpp>

#include "transaction.h"
#include "xengine.h"

namespace bigbang
{
namespace storage
{

class CForkUnspentDBWalker
{
public:
    virtual bool Walk(const CTxOutPoint& txout, const CTxOut& output) = 0;
};

class CForkUnspentCheckWalker : public CForkUnspentDBWalker
{
public:
    CForkUnspentCheckWalker(const std::map<CTxOutPoint, CTxUnspent>& mapUnspent)
      : nMatch(0), nAll(0), nRanged(mapUnspent.size()), mapUnspentUTXO(mapUnspent){};
    bool Walk(const CTxOutPoint& txout, const CTxOut& output) override
    {
        ++nAll;
        for (const auto& item : mapUnspentUTXO)
        {
            const CTxUnspent& unspent = item.second;
            if (unspent.hash == txout.hash && unspent.n == txout.n
                && unspent.output.destTo == output.destTo
                && unspent.output.nAmount == output.nAmount
                && unspent.output.nLockUntil == output.nLockUntil)
            {
                ++nMatch;
                break;
            }
        }
        return true;
    };

public:
    uint64 nMatch;
    uint64 nAll;
    uint64 nRanged;
    const std::map<CTxOutPoint, CTxUnspent>& mapUnspentUTXO;
};

class CForkUnspentDB : public xengine::CKVDB
{
    LOGGER_CHANNEL("CForkUnspentDB");

private:
    typedef std::map<CTxOutPoint, CTxOut> MapType;
    class CDblMap
    {
    public:
        CDblMap()
          : nIdxUpper(0) {}
        MapType& GetUpperMap()
        {
            return mapCache[nIdxUpper];
        }
        MapType& GetLowerMap()
        {
            return mapCache[nIdxUpper ^ 1];
        }
        void Flip()
        {
            MapType& mapLower = mapCache[nIdxUpper ^ 1];
            mapLower.clear();
            nIdxUpper = nIdxUpper ^ 1;
        }
        void Clear()
        {
            mapCache[0].clear();
            mapCache[1].clear();
            nIdxUpper = 0;
        }

    protected:
        MapType mapCache[2];
        int nIdxUpper;
    };

public:
    CForkUnspentDB(const boost::filesystem::path& pathDB);
    ~CForkUnspentDB();
    bool RemoveAll();
    bool UpdateUnspent(const std::vector<CTxUnspent>& vAddNew, const std::vector<CTxOutPoint>& vRemove);
    bool WriteUnspent(const CTxOutPoint& txout, const CTxOut& output);
    bool ReadUnspent(const CTxOutPoint& txout, CTxOut& output);
    bool Copy(CForkUnspentDB& dbUnspent);
    void SetCache(const CDblMap& dblCacheIn)
    {
        dblCache = dblCacheIn;
    }
    bool WalkThroughUnspent(CForkUnspentDBWalker& walker);
    bool Flush();

protected:
    bool CopyWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                    CForkUnspentDB& dbUnspent);
    bool LoadWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                    CForkUnspentDBWalker& walker, const MapType& mapUpper, const MapType& mapLower);

protected:
    xengine::CRWAccess rwUpper;
    xengine::CRWAccess rwLower;
    CDblMap dblCache;
};

class CUnspentDB
{
    LOGGER_CHANNEL("CUnspentDB");

public:
    CUnspentDB();
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool Exists(const uint256& hashFork)
    {
        return mapUnspentDB.count(hashFork) != 0;
    }
    bool AddNewFork(const uint256& hashFork);
    bool RemoveFork(const uint256& hashFork);
    void Clear();
    bool Update(const uint256& hashFork,
                const std::vector<CTxUnspent>& vAddNew, const std::vector<CTxOutPoint>& vRemove);
    bool Retrieve(const uint256& hashFork, const CTxOutPoint& txout, CTxOut& output);
    bool Copy(const uint256& srcFork, const uint256& destFork);
    bool WalkThrough(const uint256& hashFork, CForkUnspentDBWalker& walker);

protected:
    void FlushProc();

protected:
    boost::filesystem::path pathUnspent;
    xengine::CRWAccess rwAccess;
    std::map<uint256, std::shared_ptr<CForkUnspentDB>> mapUnspentDB;

    boost::mutex mtxFlush;
    boost::condition_variable condFlush;
    boost::thread* pThreadFlush;
    bool fStopFlush;
};

} // namespace storage
} // namespace bigbang

#endif //STORAGE_UNSPENTDB_H
