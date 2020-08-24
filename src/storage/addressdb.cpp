// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "addressdb.h"

#include <boost/bind.hpp>

#include "leveldbeng.h"

using namespace std;
using namespace xengine;

namespace bigbang
{
namespace storage
{

#define ADDRESS_FLUSH_INTERVAL (60)

//////////////////////////////
// CForkAddressDB

CForkAddressDB::CForkAddressDB(const boost::filesystem::path& pathDB)
{
    CLevelDBArguments args;
    args.path = pathDB.string();
    args.syncwrite = false;
    CLevelDBEngine* engine = new CLevelDBEngine(args);

    if (!CKVDB::Open(engine))
    {
        delete engine;
    }
}

CForkAddressDB::~CForkAddressDB()
{
    Close();
    dblCache.Clear();
}

bool CForkAddressDB::RemoveAll()
{
    if (!CKVDB::RemoveAll())
    {
        return false;
    }
    dblCache.Clear();
    return true;
}

bool CForkAddressDB::UpdateAddress(const vector<pair<CDestination, CAddrInfo>>& vAddNew, const vector<pair<CDestination, CAddrInfo>>& vRemove)
{
    xengine::CWriteLock wlock(rwUpper);

    MapType& mapUpper = dblCache.GetUpperMap();

    for (const auto& addr : vRemove)
    {
        CAddrInfo addrInfo;
        if (GetAddress(addr.first, addrInfo) && addrInfo.hashTxInvite == addr.second.hashTxInvite)
        {
            mapUpper[addr.first].SetNull();
        }
    }

    for (const auto& vd : vAddNew)
    {
        if (vd.first.IsNull() || vd.second.destInviteParent.IsNull() || vd.first == vd.second.destInviteParent)
        {
            continue;
        }
        CAddrInfo addrInfo;
        if (!GetAddress(vd.first, addrInfo))
        {
            bool fLoop = false;
            CAddrInfo addrParentInfo;
            if (GetAddress(vd.second.destInviteParent, addrInfo))
            {
                addrParentInfo = addrInfo;
                while (1)
                {
                    if (addrInfo.destInviteRoot == vd.first)
                    {
                        fLoop = true;
                        break;
                    }
                    if (addrInfo.destInviteRoot.IsNull() || !GetAddress(addrInfo.destInviteRoot, addrInfo))
                    {
                        break;
                    }
                }
            }
            if (!fLoop)
            {
                auto& addr = mapUpper[vd.first];
                addr = vd.second;
                if (!addrParentInfo.IsNull())
                {
                    addr.destInviteRoot = addrParentInfo.destInviteRoot;
                }
                else
                {
                    addr.destInviteRoot = vd.second.destInviteParent;
                }
            }
        }
    }

    return true;
}

bool CForkAddressDB::RepairAddress(const vector<pair<CDestination, CAddrInfo>>& vAddUpdate, const vector<CDestination>& vRemove)
{
    // Remove
    {
        if (!TxnBegin())
        {
            return false;
        }
        for (const auto& dest : vRemove)
        {
            Erase(dest);
        }
        if (!TxnCommit())
        {
            return false;
        }
    }

    // Construct new data
    map<CDestination, CAddrInfo> mapAddNew;
    {
        CAddrInfo addrInfo;
        for (const auto& vd : vAddUpdate)
        {
            if (vd.first.IsNull() || vd.second.destInviteParent.IsNull() || vd.first == vd.second.destInviteParent)
            {
                continue;
            }
            if (!Read(vd.first, addrInfo) && mapAddNew.count(vd.first) == 0)
            {
                bool fLoop = false;
                CAddrInfo addrParentInfo;
                auto kt = mapAddNew.find(vd.second.destInviteParent);
                if (kt != mapAddNew.end())
                {
                    addrInfo = kt->second;
                    addrParentInfo = kt->second;
                }
                else if (Read(vd.second.destInviteParent, addrInfo))
                {
                    addrParentInfo = addrInfo;
                }
                if (!addrParentInfo.IsNull())
                {
                    while (1)
                    {
                        if (addrInfo.destInviteRoot == vd.first)
                        {
                            fLoop = true;
                            break;
                        }
                        if (addrInfo.destInviteRoot.IsNull())
                        {
                            break;
                        }
                        if (!Read(addrInfo.destInviteRoot, addrInfo))
                        {
                            auto nt = mapAddNew.find(vd.second.destInviteRoot);
                            if (nt == mapAddNew.end())
                            {
                                break;
                            }
                            addrInfo = kt->second;
                        }
                    }
                }
                if (!fLoop)
                {
                    auto& addr = mapAddNew[vd.first];
                    addr = vd.second;
                    if (!addrParentInfo.IsNull())
                    {
                        addr.destInviteRoot = addrParentInfo.destInviteRoot;
                    }
                    else
                    {
                        addr.destInviteRoot = vd.second.destInviteParent;
                    }
                }
            }
        }
    }

    // Add
    {
        if (!TxnBegin())
        {
            return false;
        }
        for (const auto& addr : mapAddNew)
        {
            Write(addr.first, addr.second);
        }
        if (!TxnCommit())
        {
            return false;
        }
    }
    return true;
}

bool CForkAddressDB::WriteAddress(const CDestination& dest, const CAddrInfo& addrInfo)
{
    return Write(dest, addrInfo);
}

bool CForkAddressDB::ReadAddress(const CDestination& dest, CAddrInfo& addrInfo)
{
    {
        xengine::CReadLock rlock(rwUpper);

        MapType& mapUpper = dblCache.GetUpperMap();
        typename MapType::iterator it = mapUpper.find(dest);
        if (it != mapUpper.end())
        {
            if (!(*it).second.IsNull())
            {
                addrInfo = (*it).second;
                return true;
            }
            return false;
        }
    }

    {
        xengine::CReadLock rlock(rwLower);
        MapType& mapLower = dblCache.GetLowerMap();
        typename MapType::iterator it = mapLower.find(dest);
        if (it != mapLower.end())
        {
            if (!(*it).second.IsNull())
            {
                addrInfo = (*it).second;
                return true;
            }
            return false;
        }
    }

    return Read(dest, addrInfo);
}

bool CForkAddressDB::Copy(CForkAddressDB& dbAddress)
{
    if (!dbAddress.RemoveAll())
    {
        return false;
    }

    try
    {
        xengine::CReadLock rulock(rwUpper);
        xengine::CReadLock rdlock(rwLower);

        if (!WalkThrough(boost::bind(&CForkAddressDB::CopyWalker, this, _1, _2, boost::ref(dbAddress))))
        {
            return false;
        }

        dbAddress.SetCache(dblCache);
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

bool CForkAddressDB::WalkThroughAddress(CForkAddressDBWalker& walker)
{
    try
    {
        xengine::CReadLock rulock(rwUpper);
        xengine::CReadLock rdlock(rwLower);

        MapType& mapUpper = dblCache.GetUpperMap();
        MapType& mapLower = dblCache.GetLowerMap();

        if (!WalkThrough(boost::bind(&CForkAddressDB::LoadWalker, this, _1, _2, boost::ref(walker),
                                     boost::ref(mapUpper), boost::ref(mapLower))))
        {
            return false;
        }

        for (MapType::iterator it = mapLower.begin(); it != mapLower.end(); ++it)
        {
            const CDestination& dest = (*it).first;
            const CAddrInfo& addrInfo = (*it).second;
            if (!mapUpper.count(dest) && !addrInfo.IsNull())
            {
                if (!walker.Walk(dest, addrInfo))
                {
                    return false;
                }
            }
        }
        for (MapType::iterator it = mapUpper.begin(); it != mapUpper.end(); ++it)
        {
            const CDestination& dest = (*it).first;
            const CAddrInfo& addrInfo = (*it).second;
            if (!addrInfo.IsNull())
            {
                if (!walker.Walk(dest, addrInfo))
                {
                    return false;
                }
            }
        }
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

bool CForkAddressDB::CopyWalker(CBufStream& ssKey, CBufStream& ssValue,
                                CForkAddressDB& dbAddress)
{
    CDestination dest;
    CAddrInfo addrInfo;
    ssKey >> dest;
    ssValue >> addrInfo;

    return dbAddress.WriteAddress(dest, addrInfo);
}

bool CForkAddressDB::LoadWalker(CBufStream& ssKey, CBufStream& ssValue,
                                CForkAddressDBWalker& walker, const MapType& mapUpper, const MapType& mapLower)
{
    CDestination dest;
    CAddrInfo addrInfo;
    ssKey >> dest;

    if (mapUpper.count(dest) || mapLower.count(dest))
    {
        return true;
    }

    ssValue >> addrInfo;

    return walker.Walk(dest, addrInfo);
}

bool CForkAddressDB::GetAddress(const CDestination& dest, CAddrInfo& addrInfo)
{
    {
        MapType& mapUpper = dblCache.GetUpperMap();
        typename MapType::iterator it = mapUpper.find(dest);
        if (it != mapUpper.end())
        {
            if (!(*it).second.IsNull())
            {
                addrInfo = (*it).second;
                return true;
            }
            return false;
        }
    }

    {
        MapType& mapLower = dblCache.GetLowerMap();
        typename MapType::iterator it = mapLower.find(dest);
        if (it != mapLower.end())
        {
            if (!(*it).second.IsNull())
            {
                addrInfo = (*it).second;
                return true;
            }
            return false;
        }
    }

    return Read(dest, addrInfo);
}

bool CForkAddressDB::Flush()
{
    xengine::CUpgradeLock ulock(rwLower);

    vector<pair<CDestination, CAddrInfo>> vAddNew;
    vector<CDestination> vRemove;

    MapType& mapLower = dblCache.GetLowerMap();
    for (typename MapType::iterator it = mapLower.begin(); it != mapLower.end(); ++it)
    {
        CAddrInfo& addrInfo = (*it).second;
        if (!addrInfo.IsNull())
        {
            vAddNew.push_back(*it);
        }
        else
        {
            vRemove.push_back((*it).first);
        }
    }

    if (!TxnBegin())
    {
        return false;
    }

    for (const auto& addr : vAddNew)
    {
        Write(addr.first, addr.second);
    }

    for (int i = 0; i < vRemove.size(); i++)
    {
        Erase(vRemove[i]);
    }

    if (!TxnCommit())
    {
        return false;
    }

    ulock.Upgrade();

    {
        xengine::CWriteLock wlock(rwUpper);
        dblCache.Flip();
    }

    return true;
}

//////////////////////////////
// CAddressDB

CAddressDB::CAddressDB()
{
    pThreadFlush = nullptr;
    fStopFlush = true;
}

bool CAddressDB::Initialize(const boost::filesystem::path& pathData)
{
    pathAddress = pathData / "address";

    if (!boost::filesystem::exists(pathAddress))
    {
        boost::filesystem::create_directories(pathAddress);
    }

    if (!boost::filesystem::is_directory(pathAddress))
    {
        return false;
    }

    fStopFlush = false;
    pThreadFlush = new boost::thread(boost::bind(&CAddressDB::FlushProc, this));
    if (pThreadFlush == nullptr)
    {
        fStopFlush = true;
        return false;
    }

    return true;
}

void CAddressDB::Deinitialize()
{
    if (pThreadFlush)
    {
        {
            boost::unique_lock<boost::mutex> lock(mtxFlush);
            fStopFlush = true;
        }
        condFlush.notify_all();
        pThreadFlush->join();
        delete pThreadFlush;
        pThreadFlush = nullptr;
    }

    {
        CWriteLock wlock(rwAccess);

        for (map<uint256, std::shared_ptr<CForkAddressDB>>::iterator it = mapAddressDB.begin();
             it != mapAddressDB.end(); ++it)
        {
            std::shared_ptr<CForkAddressDB> spAddress = (*it).second;

            spAddress->Flush();
            spAddress->Flush();
        }
        mapAddressDB.clear();
    }
}

bool CAddressDB::AddNewFork(const uint256& hashFork)
{
    CWriteLock wlock(rwAccess);

    map<uint256, std::shared_ptr<CForkAddressDB>>::iterator it = mapAddressDB.find(hashFork);
    if (it != mapAddressDB.end())
    {
        return true;
    }

    std::shared_ptr<CForkAddressDB> spAddress(new CForkAddressDB(pathAddress / hashFork.GetHex()));
    if (spAddress == nullptr || !spAddress->IsValid())
    {
        return false;
    }
    mapAddressDB.insert(make_pair(hashFork, spAddress));
    return true;
}

bool CAddressDB::RemoveFork(const uint256& hashFork)
{
    CWriteLock wlock(rwAccess);

    map<uint256, std::shared_ptr<CForkAddressDB>>::iterator it = mapAddressDB.find(hashFork);
    if (it != mapAddressDB.end())
    {
        (*it).second->RemoveAll();
        mapAddressDB.erase(it);
        return true;
    }
    return false;
}

void CAddressDB::Clear()
{
    CWriteLock wlock(rwAccess);

    map<uint256, std::shared_ptr<CForkAddressDB>>::iterator it = mapAddressDB.begin();
    while (it != mapAddressDB.end())
    {
        (*it).second->RemoveAll();
        mapAddressDB.erase(it++);
    }
}

bool CAddressDB::Update(const uint256& hashFork,
                        const vector<pair<CDestination, CAddrInfo>>& vAddNew, const vector<pair<CDestination, CAddrInfo>>& vRemove)
{
    CReadLock rlock(rwAccess);

    map<uint256, std::shared_ptr<CForkAddressDB>>::iterator it = mapAddressDB.find(hashFork);
    if (it != mapAddressDB.end())
    {
        return (*it).second->UpdateAddress(vAddNew, vRemove);
    }
    return false;
}

bool CAddressDB::RepairAddress(const uint256& hashFork, const vector<pair<CDestination, CAddrInfo>>& vAddUpdate, const vector<CDestination>& vRemove)
{
    CReadLock rlock(rwAccess);

    map<uint256, std::shared_ptr<CForkAddressDB>>::iterator it = mapAddressDB.find(hashFork);
    if (it != mapAddressDB.end())
    {
        return (*it).second->RepairAddress(vAddUpdate, vRemove);
    }
    return false;
}

bool CAddressDB::Retrieve(const uint256& hashFork, const CDestination& dest, CAddrInfo& addrInfo)
{
    CReadLock rlock(rwAccess);

    map<uint256, std::shared_ptr<CForkAddressDB>>::iterator it = mapAddressDB.find(hashFork);
    if (it != mapAddressDB.end())
    {
        return (*it).second->ReadAddress(dest, addrInfo);
    }
    return false;
}

bool CAddressDB::Copy(const uint256& srcFork, const uint256& destFork)
{
    CReadLock rlock(rwAccess);

    map<uint256, std::shared_ptr<CForkAddressDB>>::iterator itSrc = mapAddressDB.find(srcFork);
    if (itSrc == mapAddressDB.end())
    {
        return false;
    }

    map<uint256, std::shared_ptr<CForkAddressDB>>::iterator itDest = mapAddressDB.find(destFork);
    if (itDest == mapAddressDB.end())
    {
        return false;
    }

    return ((*itSrc).second->Copy(*(*itDest).second));
}

bool CAddressDB::WalkThrough(const uint256& hashFork, CForkAddressDBWalker& walker)
{
    CReadLock rlock(rwAccess);

    map<uint256, std::shared_ptr<CForkAddressDB>>::iterator it = mapAddressDB.find(hashFork);
    if (it != mapAddressDB.end())
    {
        return (*it).second->WalkThroughAddress(walker);
    }
    return false;
}

void CAddressDB::Flush(const uint256& hashFork)
{
    boost::unique_lock<boost::mutex> lock(mtxFlush);
    CReadLock rlock(rwAccess);

    map<uint256, std::shared_ptr<CForkAddressDB>>::iterator it = mapAddressDB.find(hashFork);
    if (it != mapAddressDB.end())
    {
        (*it).second->Flush();
    }
}

void CAddressDB::FlushProc()
{
    SetThreadName("AddressDB");
    boost::system_time timeout = boost::get_system_time();
    boost::unique_lock<boost::mutex> lock(mtxFlush);
    while (!fStopFlush)
    {
        timeout += boost::posix_time::seconds(ADDRESS_FLUSH_INTERVAL);

        while (!fStopFlush)
        {
            if (!condFlush.timed_wait(lock, timeout))
            {
                break;
            }
        }

        if (!fStopFlush)
        {
            vector<std::shared_ptr<CForkAddressDB>> vAddressDB;
            vAddressDB.reserve(mapAddressDB.size());
            {
                CReadLock rlock(rwAccess);

                for (map<uint256, std::shared_ptr<CForkAddressDB>>::iterator it = mapAddressDB.begin();
                     it != mapAddressDB.end(); ++it)
                {
                    vAddressDB.push_back((*it).second);
                }
            }
            for (int i = 0; i < vAddressDB.size(); i++)
            {
                vAddressDB[i]->Flush();
            }
        }
    }
}

} // namespace storage
} // namespace bigbang
