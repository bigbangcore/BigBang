// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_ADDRESSDB_H
#define STORAGE_ADDRESSDB_H

#include <boost/thread/thread.hpp>

#include "transaction.h"
#include "xengine.h"

namespace bigbang
{
namespace storage
{

//////////////////////////////
// CAddrInfo

class CAddrInfo
{
    friend class xengine::CStream;

public:
    CDestination destInviteParent;
    uint256 hashTxInvite;

public:
    CAddrInfo()
    {
        SetNull();
    }
    CAddrInfo(const CDestination& destInviteParentIn, const uint256& hashTxInviteIn)
      : destInviteParent(destInviteParentIn), hashTxInvite(hashTxInviteIn) {}
    void SetNull()
    {
        destInviteParent.SetNull();
        hashTxInvite = 0;
    }
    bool IsNull() const
    {
        return (destInviteParent.IsNull() || hashTxInvite == 0);
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(destInviteParent, opt);
        s.Serialize(hashTxInvite, opt);
    }
};

//////////////////////////////
// CForkAddressDBWalker

class CForkAddressDBWalker
{
public:
    virtual bool Walk(const CDestination& dest, const CAddrInfo& addrInfo) = 0;
};

//////////////////////////////
// CListAddressWalker

class CListAddressWalker : public CForkAddressDBWalker
{
public:
    CListAddressWalker() {}
    bool Walk(const CDestination& dest, const CAddrInfo& addrInfo) override
    {
        mapAddress.insert(std::make_pair(dest, addrInfo));
        return true; //continue walk through processing
    }

public:
    std::map<CDestination, CAddrInfo> mapAddress;
};

//////////////////////////////
// CForkAddressDB

class CForkAddressDB : public xengine::CKVDB
{
    typedef std::map<CDestination, CAddrInfo> MapType;
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
    CForkAddressDB(const boost::filesystem::path& pathDB);
    ~CForkAddressDB();
    bool RemoveAll();
    bool UpdateAddress(const std::vector<std::pair<CDestination, CAddrInfo>>& vAddNew, const std::vector<CDestination>& vRemove);
    bool RepairAddress(const std::vector<std::pair<CDestination, CAddrInfo>>& vAddUpdate, const std::vector<CDestination>& vRemove);
    bool WriteAddress(const CDestination& dest, const CAddrInfo& addrInfo);
    bool ReadAddress(const CDestination& dest, CAddrInfo& addrInfo);
    bool Copy(CForkAddressDB& dbAddress);
    void SetCache(const CDblMap& dblCacheIn)
    {
        dblCache = dblCacheIn;
    }
    bool WalkThroughAddress(CForkAddressDBWalker& walker);
    bool Flush();

protected:
    bool CopyWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                    CForkAddressDB& dbAddress);
    bool LoadWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                    CForkAddressDBWalker& walker, const MapType& mapUpper, const MapType& mapLower);

protected:
    xengine::CRWAccess rwUpper;
    xengine::CRWAccess rwLower;
    CDblMap dblCache;
};

class CAddressDB
{
public:
    CAddressDB();
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool Exists(const uint256& hashFork)
    {
        return (!!mapAddressDB.count(hashFork));
    }
    bool AddNewFork(const uint256& hashFork);
    bool RemoveFork(const uint256& hashFork);
    void Clear();
    bool Update(const uint256& hashFork,
                const std::vector<std::pair<CDestination, CAddrInfo>>& vAddNew, const std::vector<CDestination>& vRemove);
    bool RepairAddress(const uint256& hashFork, const std::vector<std::pair<CDestination, CAddrInfo>>& vAddUpdate, const std::vector<CDestination>& vRemove);
    bool Retrieve(const uint256& hashFork, const CDestination& dest, CAddrInfo& addrInfo);
    bool Copy(const uint256& srcFork, const uint256& destFork);
    bool WalkThrough(const uint256& hashFork, CForkAddressDBWalker& walker);
    void Flush(const uint256& hashFork);

protected:
    void FlushProc();

protected:
    boost::filesystem::path pathAddress;
    xengine::CRWAccess rwAccess;
    std::map<uint256, std::shared_ptr<CForkAddressDB>> mapAddressDB;

    boost::mutex mtxFlush;
    boost::condition_variable condFlush;
    boost::thread* pThreadFlush;
    bool fStopFlush;
};

} // namespace storage
} // namespace bigbang

#endif //STORAGE_ADDRESSDB_H
