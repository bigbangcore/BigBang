// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_CTSDB_H
#define STORAGE_CTSDB_H

#include <boost/filesystem.hpp>
#include <boost/range/algorithm.hpp>
#include <iostream>
#include <snappy.h>

#include "timeseries.h"
#include "xengine.h"

namespace bigbang
{
namespace storage
{

class CCTSIndex : public xengine::CKVDB
{
public:
    CCTSIndex();
    ~CCTSIndex();
    bool Initialize(const boost::filesystem::path& pathCTSDB);
    void Deinitialize();
    bool Update(const std::vector<int64>& vTime, const std::vector<CDiskPos>& vPos,
                const std::vector<int64>& vDel);
    bool Retrieve(const int64, CDiskPos& pos);
};

template <typename K, typename V>
class CCTSChunk : public std::vector<std::pair<K, V>>
{
    typedef std::vector<std::pair<K, V>> basetype;
    friend class xengine::CStream;

public:
    CCTSChunk() {}
    template <class InputIterator>
    CCTSChunk(InputIterator first, InputIterator last)
      : basetype(first, last)
    {
    }
    bool Find(const K& k, V& v)
    {
        int s = 0, m = 0, e = basetype::size() - 1;
        while (s <= e)
        {
            m = (s + e) >> 1;
            if (basetype::at(m).first < k)
            {
                s = m + 1;
            }
            else if (basetype::at(m).first > k)
            {
                e = m - 1;
            }
            else
            {
                v = basetype::at(m).second;
                return true;
            }
        }
        return false;
    }

protected:
    void Serialize(xengine::CStream& s, xengine::SaveType& opt)
    {
        xengine::CVarInt var(basetype::size());
        s.Serialize(var, opt);
        s.Write((const char*)&((*this)[0]), basetype::size() * sizeof(std::pair<K, V>));
    }
    void Serialize(xengine::CStream& s, xengine::LoadType& opt)
    {
        xengine::CVarInt var;
        s.Serialize(var, opt);
        this->resize(var.nValue);
        s.Read((char*)&((*this)[0]), basetype::size() * sizeof(std::pair<K, V>));
    }
    void Serialize(xengine::CStream& s, std::size_t& serSize)
    {
        xengine::CVarInt var(basetype::size());
        serSize += xengine::GetSerializeSize(var) + basetype::size() * sizeof(std::pair<K, V>);
    }
};

template <typename K, typename V>
class CCTSChunkSnappy : public CCTSChunk<K, V>
{
    typedef std::vector<std::pair<K, V>> basetype;
    friend class xengine::CStream;

public:
    CCTSChunkSnappy() {}
    template <class InputIterator>
    CCTSChunkSnappy(InputIterator first, InputIterator last)
      : CCTSChunk<K, V>(first, last)
    {
    }

protected:
    void Serialize(xengine::CStream& s, xengine::SaveType& opt)
    {
        std::string strSnappy;
        snappy::Compress((const char*)&((*this)[0]), basetype::size() * sizeof(std::pair<K, V>), &strSnappy);
        s.Serialize(strSnappy, opt);
    }
    void Serialize(xengine::CStream& s, xengine::LoadType& opt)
    {
        std::string strSnappy, strUncompress;
        s.Serialize(strSnappy, opt);
        size_t size = 0;
        snappy::GetUncompressedLength(&strSnappy[0], strSnappy.size(), &size);
        // check size
        if (size % sizeof(std::pair<K, V>) != 0)
        {
            xengine::StdError("CCTSChunkSnappy", "**********************************");
            xengine::StdError("CCTSChunkSnappy", "Load uncompressed data error. size: %u is not divisible by sizeof(pair<K, V>): %u. "
                "\"DATA ERROR\": Should shut down and purge and resynchronize", size, sizeof(std::pair<K, V>));
            xengine::StdError("CCTSChunkSnappy", "**********************************");
            return;
        }
        basetype::resize(size / sizeof(std::pair<K, V>));
        snappy::RawUncompress(&strSnappy[0], strSnappy.size(), (char*)&((*this)[0]));
    }
    void Serialize(xengine::CStream& s, std::size_t& serSize)
    {
        std::string strSnappy;
        snappy::Compress((const char*)&((*this)[0]), basetype::size() * sizeof(std::pair<K, V>), &strSnappy);

        xengine::CVarInt var(strSnappy.size());
        serSize += xengine::GetSerializeSize(var) + strSnappy.size();
    }
};

template <typename K, typename V, typename C = CCTSChunk<K, V>>
class CCTSDB
{
    typedef std::map<int64, std::map<K, V>> MapType;
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
    bool Initialize(const boost::filesystem::path& pathCTSDB)
    {
        if (!boost::filesystem::exists(pathCTSDB))
        {
            boost::filesystem::create_directories(pathCTSDB);
        }

        if (!boost::filesystem::is_directory(pathCTSDB))
        {
            return false;
        }

        if (!dbIndex.Initialize(pathCTSDB))
        {
            return false;
        }

        if (!tsChunk.Initialize(pathCTSDB, "meta"))
        {
            return false;
        }

        return true;
    }
    void Deinitialize()
    {
        dbIndex.Deinitialize();
        tsChunk.Deinitialize();
        dblMeta.Clear();
    }
    void RemoveAll()
    {
        dbIndex.RemoveAll();
        dblMeta.Clear();
    }
    void Update(const int64 nTime, const K& key, const V& value)
    {
        xengine::CWriteLock wlock(rwUpper);

        GetUpdateMap(nTime)[key] = value;
    }
    void Erase(const int64 nTime, const K& key)
    {
        xengine::CWriteLock wlock(rwUpper);

        GetUpdateMap(nTime).erase(key);
    }
    bool Retrieve(const int64 nTime, const K& key, V& value)
    {
        {
            xengine::CReadLock rlock(rwUpper);

            MapType& mapUpper = dblMeta.GetUpperMap();

            typename MapType::iterator it = mapUpper.find(nTime);
            if (it != mapUpper.end())
            {
                std::map<K, V>& mapValue = (*it).second;
                typename std::map<K, V>::iterator mi = mapValue.find(key);
                if (mi != mapValue.end())
                {
                    value = (*mi).second;
                    return true;
                }
                return false;
            }
        }

        {
            xengine::CReadLock rlock(rwLower);
            MapType& mapLower = dblMeta.GetLowerMap();

            typename MapType::iterator it = mapLower.find(nTime);
            if (it != mapLower.end())
            {
                std::map<K, V>& mapValue = (*it).second;
                typename std::map<K, V>::iterator mi = mapValue.find(key);
                if (mi != mapValue.end())
                {
                    value = (*mi).second;
                    return true;
                }
                return false;
            }
        }

        C chunk;
        if (LoadFromFile(nTime, chunk))
        {
            return chunk.Find(key, value);
        }

        return false;
    }

    bool Flush()
    {
        xengine::CUpgradeLock ulock(rwLower);

        std::vector<int64> vTime, vDel;
        std::vector<C> vChunk;
        MapType& mapFlush = dblMeta.GetLowerMap();
        for (typename MapType::iterator it = mapFlush.begin(); it != mapFlush.end(); ++it)
        {
            std::map<K, V>& mapValue = (*it).second;
            if (mapValue.empty())
            {
                vDel.push_back((*it).first);
            }
            else
            {
                vTime.push_back((*it).first);
                vChunk.push_back(C(mapValue.begin(), mapValue.end()));
            }
        }

        std::vector<CDiskPos> vPos;
        if (!vChunk.empty())
        {
            if (!tsChunk.WriteBatch(vChunk, vPos))
            {
                return false;
            }
        }
        if (!vPos.empty() || !vDel.empty())
        {
            if (!dbIndex.Update(vTime, vPos, vDel))
            {
                return false;
            }
        }

        ulock.Upgrade();

        {
            xengine::CWriteLock wlock(rwUpper);
            dblMeta.Flip();
        }

        return true;
    }

protected:
    std::map<K, V>& GetUpdateMap(const int64 nTime)
    {
        MapType& mapUpdate = dblMeta.GetUpperMap();

        typename MapType::iterator it = mapUpdate.find(nTime);
        if (it != mapUpdate.end())
        {
            return (*it).second;
        }

        if (rwLower.ReadTryLock())
        {
            MapType& mapLower = dblMeta.GetLowerMap();
            typename MapType::iterator mi = mapLower.find(nTime);
            if (mi != mapLower.end())
            {
                mapUpdate[nTime] = (*mi).second;
                rwLower.ReadUnlock();
                return mapUpdate[nTime];
            }
            rwLower.ReadUnlock();
        }

        C chunk;
        if (LoadFromFile(nTime, chunk))
        {
            mapUpdate[nTime].insert(chunk.begin(), chunk.end());
        }
        return mapUpdate[nTime];
    }
    bool LoadFromFile(const int64 nTime, C& chunk)
    {
        CDiskPos pos;
        if (dbIndex.Retrieve(nTime, pos))
        {
            return tsChunk.Read(chunk, pos);
        }
        return false;
    }

protected:
    xengine::CRWAccess rwUpper;
    xengine::CRWAccess rwLower;
    CCTSIndex dbIndex;
    CTimeSeriesChunk tsChunk;
    CDblMap dblMeta;
};

} // namespace storage
} // namespace bigbang

#endif //STORAGE_CTSDB_H
