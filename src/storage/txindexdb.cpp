// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txindexdb.h"

#include <boost/bind.hpp>

using namespace std;
using namespace xengine;

namespace bigbang
{
namespace storage
{

#define TXINDEX_FLUSH_INTERVAL (3600)
//////////////////////////////
// CTxIndexDB

CTxIndexDB::CTxIndexDB()
{
    pThreadFlush = nullptr;
    fStopFlush = true;
}

bool CTxIndexDB::Initialize(const boost::filesystem::path& pathData)
{
    pathTxIndex = pathData / "txindex";

    if (!boost::filesystem::exists(pathTxIndex))
    {
        boost::filesystem::create_directories(pathTxIndex);
    }

    if (!boost::filesystem::is_directory(pathTxIndex))
    {
        return false;
    }

    fStopFlush = false;
    pThreadFlush = new boost::thread(boost::bind(&CTxIndexDB::FlushProc, this));
    if (pThreadFlush == nullptr)
    {
        fStopFlush = true;
        return false;
    }

    return true;
}

void CTxIndexDB::Deinitialize()
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

        for (map<uint256, std::shared_ptr<CForkTxDB>>::iterator it = mapTxDB.begin();
             it != mapTxDB.end(); ++it)
        {
            std::shared_ptr<CForkTxDB> spTxDB = (*it).second;

            spTxDB->Flush();
            spTxDB->Flush();
            spTxDB->Deinitialize();
        }
        mapTxDB.clear();
    }
}

bool CTxIndexDB::LoadFork(const uint256& hashFork)
{
    CWriteLock wlock(rwAccess);

    map<uint256, std::shared_ptr<CForkTxDB>>::iterator it = mapTxDB.find(hashFork);
    if (it != mapTxDB.end())
    {
        return true;
    }

    std::shared_ptr<CForkTxDB> spTxDB(new CForkTxDB());
    if (spTxDB == nullptr || !spTxDB->Initialize(pathTxIndex / hashFork.GetHex()))
    {
        return false;
    }
    mapTxDB.insert(make_pair(hashFork, spTxDB));
    return true;
}

bool CTxIndexDB::Update(const uint256& hashFork, const vector<pair<uint256, CTxIndex>>& vTxNew,
                        const vector<uint256>& vTxDel)
{
    CReadLock rlock(rwAccess);

    map<uint256, std::shared_ptr<CForkTxDB>>::iterator it = mapTxDB.find(hashFork);
    if (it == mapTxDB.end())
    {
        return false;
    }

    std::shared_ptr<CForkTxDB> spTxDB = (*it).second;

    for (int i = 0; i < vTxNew.size(); i++)
    {
        CTxId txid(vTxNew[i].first);
        spTxDB->Update(txid.GetTxTime(), txid.GetTxHash(), vTxNew[i].second);
    }

    for (int i = 0; i < vTxDel.size(); i++)
    {
        CTxId txid(vTxDel[i]);
        spTxDB->Erase(txid.GetTxTime(), txid.GetTxHash());
    }
    return true;
}

bool CTxIndexDB::Retrieve(const uint256& hashFork, const uint256& txidIn, CTxIndex& txIndex)
{
    CReadLock rlock(rwAccess);

    map<uint256, std::shared_ptr<CForkTxDB>>::iterator it = mapTxDB.find(hashFork);
    if (it == mapTxDB.end())
    {
        return false;
    }

    std::shared_ptr<CForkTxDB> spTxDB = (*it).second;

    CTxId txid(txidIn);

    return spTxDB->Retrieve(txid.GetTxTime(), txid.GetTxHash(), txIndex);
}

bool CTxIndexDB::Retrieve(const uint256& txidIn, CTxIndex& txIndex, uint256& hashFork)
{
    CReadLock rlock(rwAccess);

    CTxId txid(txidIn);

    for (map<uint256, std::shared_ptr<CForkTxDB>>::iterator it = mapTxDB.begin();
         it != mapTxDB.end(); ++it)
    {
        std::shared_ptr<CForkTxDB> spTxDB = (*it).second;
        if (spTxDB->Retrieve(txid.GetTxTime(), txid.GetTxHash(), txIndex))
        {
            hashFork = (*it).first;
            return true;
        }
    }

    return false;
}

void CTxIndexDB::Clear()
{
    CWriteLock wlock(rwAccess);

    for (map<uint256, std::shared_ptr<CForkTxDB>>::iterator it = mapTxDB.begin();
         it != mapTxDB.end(); ++it)
    {
        std::shared_ptr<CForkTxDB> spTxDB = (*it).second;
        spTxDB->RemoveAll();
        spTxDB->Deinitialize();
    }
    mapTxDB.clear();
}

void CTxIndexDB::Flush(const uint256& hashFork)
{
    boost::unique_lock<boost::mutex> lock(mtxFlush);
    CReadLock rlock(rwAccess);

    map<uint256, std::shared_ptr<CForkTxDB>>::iterator it = mapTxDB.find(hashFork);
    if (it != mapTxDB.end())
    {
        it->second->Flush();
    }
}

void CTxIndexDB::FlushProc()
{
    SetThreadName("TxIndexDB");
    boost::system_time timeout = boost::get_system_time();

    boost::unique_lock<boost::mutex> lock(mtxFlush);
    while (!fStopFlush)
    {
        timeout += boost::posix_time::seconds(TXINDEX_FLUSH_INTERVAL);

        while (!fStopFlush)
        {
            if (!condFlush.timed_wait(lock, timeout))
            {
                break;
            }
        }

        if (!fStopFlush)
        {
            vector<std::shared_ptr<CForkTxDB>> vTxDB;
            vTxDB.reserve(mapTxDB.size());
            {
                CReadLock rlock(rwAccess);

                for (map<uint256, std::shared_ptr<CForkTxDB>>::iterator it = mapTxDB.begin();
                     it != mapTxDB.end(); ++it)
                {
                    vTxDB.push_back((*it).second);
                }
            }
            for (int i = 0; i < vTxDB.size(); i++)
            {
                vTxDB[i]->Flush();
            }
        }
    }
}

} // namespace storage
} // namespace bigbang
