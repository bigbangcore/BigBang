// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_TIMESERIES_H
#define STORAGE_TIMESERIES_H

#include <boost/filesystem.hpp>
#include <boost/thread/thread.hpp>
#include <xengine.h>

#include "uint256.h"

namespace bigbang
{
namespace storage
{

class CDiskPos
{
    friend class xengine::CStream;

public:
    uint32 nFile;
    uint32 nOffset;

public:
    CDiskPos(uint32 nFileIn = 0, uint32 nOffsetIn = 0)
      : nFile(nFileIn), nOffset(nOffsetIn) {}
    bool IsNull() const
    {
        return (nFile == 0);
    }
    bool operator==(const CDiskPos& b) const
    {
        return (nFile == b.nFile && nOffset == b.nOffset);
    }
    bool operator!=(const CDiskPos& b) const
    {
        return (nFile != b.nFile || nOffset != b.nOffset);
    }
    bool operator<(const CDiskPos& b) const
    {
        return (nFile < b.nFile || (nFile == b.nFile && nOffset < b.nOffset));
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(nFile, opt);
        s.Serialize(nOffset, opt);
    }
};

template <typename T>
class CTSWalker
{
public:
    virtual bool Walk(const T& t, uint32 nFile, uint32 nOffset) = 0;
};

class CTimeSeriesBase
{
public:
    CTimeSeriesBase();
    ~CTimeSeriesBase();
    virtual bool Initialize(const boost::filesystem::path& pathLocationIn, const std::string& strPrefixIn);
    virtual void Deinitialize();

protected:
    bool CheckDiskSpace();
    const std::string FileName(uint32 nFile);
    bool GetFilePath(uint32 nFile, std::string& strPath);
    bool GetLastFilePath(uint32& nFile, std::string& strPath);

protected:
    enum
    {
        MAX_FILE_SIZE = 0x7F000000,
        MAX_CHUNK_SIZE = 0x200000
    };
    boost::filesystem::path pathLocation;
    std::string strPrefix;
    uint32 nLastFile;
};

class CTimeSeriesCached : public CTimeSeriesBase
{
public:
    CTimeSeriesCached();
    ~CTimeSeriesCached();
    bool Initialize(const boost::filesystem::path& pathLocationIn, const std::string& strPrefixIn);
    void Deinitialize();
    template <typename T>
    bool Write(const T& t, uint32& nFile, uint32& nOffset, bool fWriteCache = true)
    {
        boost::unique_lock<boost::mutex> lock(mtxCache);

        std::string pathFile;
        if (!GetLastFilePath(nFile, pathFile))
        {
            return false;
        }
        try
        {
            xengine::CFileStream fs(pathFile.c_str());
            fs.SeekToEnd();
            uint32 nSize = fs.GetSerializeSize(t);
            fs << nMagicNum << nSize;
            nOffset = fs.GetCurPos();
            fs << t;
        }
        catch (std::exception& e)
        {
            xengine::StdError(__PRETTY_FUNCTION__, e.what());
            return false;
        }
        if (fWriteCache)
        {
            if (!WriteToCache(t, CDiskPos(nFile, nOffset)))
            {
                ResetCache();
            }
        }
        return true;
    }
    template <typename T>
    bool Write(const T& t, CDiskPos& pos, bool fWriteCache = true)
    {
        boost::unique_lock<boost::mutex> lock(mtxCache);

        std::string pathFile;
        if (!GetLastFilePath(pos.nFile, pathFile))
        {
            return false;
        }
        try
        {
            xengine::CFileStream fs(pathFile.c_str());
            fs.SeekToEnd();
            uint32 nSize = fs.GetSerializeSize(t);
            fs << nMagicNum << nSize;
            pos.nOffset = fs.GetCurPos();
            fs << t;
        }
        catch (std::exception& e)
        {
            xengine::StdError(__PRETTY_FUNCTION__, e.what());
            return false;
        }
        if (fWriteCache)
        {
            if (!WriteToCache(t, pos))
            {
                ResetCache();
            }
        }
        return true;
    }
    template <typename T>
    bool Read(T& t, uint32 nFile, uint32 nOffset, bool fWriteCache = true)
    {
        boost::unique_lock<boost::mutex> lock(mtxCache);

        if (ReadFromCache(t, CDiskPos(nFile, nOffset)))
        {
            return true;
        }

        std::string pathFile;
        if (!GetFilePath(nFile, pathFile))
        {
            return false;
        }
        try
        {
            // Open history file to read
            xengine::CFileStream fs(pathFile.c_str());
            fs.Seek(nOffset);
            fs >> t;
        }
        catch (std::exception& e)
        {
            xengine::StdError(__PRETTY_FUNCTION__, e.what());
            return false;
        }

        if (fWriteCache)
        {
            if (!WriteToCache(t, CDiskPos(nFile, nOffset)))
            {
                ResetCache();
            }
        }
        return true;
    }
    template <typename T>
    bool Read(T& t, const CDiskPos& pos, bool fWriteCache = true)
    {
        boost::unique_lock<boost::mutex> lock(mtxCache);

        if (ReadFromCache(t, pos))
        {
            return true;
        }

        std::string pathFile;
        if (!GetFilePath(pos.nFile, pathFile))
        {
            return false;
        }
        try
        {
            // Open history file to read
            xengine::CFileStream fs(pathFile.c_str());
            fs.Seek(pos.nOffset);
            fs >> t;
        }
        catch (std::exception& e)
        {
            xengine::StdError(__PRETTY_FUNCTION__, e.what());
            return false;
        }

        if (fWriteCache)
        {
            if (!WriteToCache(t, pos))
            {
                ResetCache();
            }
        }
        return true;
    }
    template <typename T>
    bool WalkThrough(CTSWalker<T>& walker, uint32& nLastFileRet, uint32& nLastPosRet)
    {
        bool fRet = true;
        uint32 nFile = 1;
        uint32 nOffset = 0;
        nLastFileRet = 0;
        nLastPosRet = 0;
        std::string pathFile;

        while (GetFilePath(nFile, pathFile) && fRet)
        {
            nLastFileRet = nFile;
            try
            {
                xengine::CFileStream fs(pathFile.c_str());
                fs.Seek(0);
                nOffset = 0;

                while (!fs.IsEOF() && fRet)
                {
                    uint32 nMagic, nSize;
                    T t;
                    try
                    {
                        fs >> nMagic >> nSize >> t;
                    }
                    catch (std::exception& e)
                    {
                        break;
                    }
                    if (nMagic != nMagicNum || fs.GetCurPos() - nOffset - 8 != nSize
                        || !walker.Walk(t, nFile, nOffset + 8))
                    {
                        fRet = false;
                        break;
                    }
                    nOffset = fs.GetCurPos();
                }
            }
            catch (std::exception& e)
            {
                xengine::StdError(__PRETTY_FUNCTION__, e.what());
                fRet = false;
            }
            nFile++;
        }
        nLastPosRet = nOffset;
        return fRet;
    }
    template <typename T>
    bool ReadDirect(T& t, uint32 nFile, uint32 nOffset)
    {
        std::string pathFile;
        if (!GetFilePath(nFile, pathFile))
        {
            return false;
        }
        try
        {
            xengine::CFileStream fs(pathFile.c_str());
            fs.Seek(nOffset);
            fs >> t;
        }
        catch (...)
        {
            return false;
        }

        return true;
    }

protected:
    void ResetCache();
    bool VacateCache(uint32 nNeeded);
    template <typename T>
    bool WriteToCache(const T& t, const CDiskPos& diskpos)
    {
        if (mapCachePos.count(diskpos))
        {
            return true;
        }
        uint32 nSize = cacheStream.GetSerializeSize(t);
        if (!VacateCache(nSize))
        {
            return false;
        }
        try
        {
            std::size_t nPos;
            cacheStream << diskpos << nSize;
            nPos = cacheStream.GetWritePos();
            cacheStream << t;
            mapCachePos.insert(std::make_pair(diskpos, nPos));
            return true;
        }
        catch (std::exception& e)
        {
            xengine::StdError(__PRETTY_FUNCTION__, e.what());
        }
        return false;
    }
    template <typename T>
    bool ReadFromCache(T& t, const CDiskPos& diskpos)
    {
        std::map<CDiskPos, size_t>::iterator it = mapCachePos.find(diskpos);
        if (it != mapCachePos.end())
        {
            if (cacheStream.Seek((*it).second))
            {
                try
                {
                    cacheStream >> t;
                    return true;
                }
                catch (std::exception& e)
                {
                    xengine::StdError(__PRETTY_FUNCTION__, e.what());
                }
            }
            ResetCache();
        }
        return false;
    }

protected:
    enum
    {
        FILE_CACHE_SIZE = 0x2000000
    };
    boost::mutex mtxCache;
    xengine::CCircularStream cacheStream;
    std::map<CDiskPos, std::size_t> mapCachePos;
    static const uint32 nMagicNum;
};

class CTimeSeriesChunk : public CTimeSeriesBase
{
public:
    CTimeSeriesChunk();
    ~CTimeSeriesChunk();
    template <typename T>
    bool Write(const T& t, CDiskPos& pos)
    {
        boost::unique_lock<boost::mutex> lock(mtxWriter);

        std::string pathFile;
        if (!GetLastFilePath(pos.nFile, pathFile))
        {
            return false;
        }
        try
        {
            xengine::CFileStream fs(pathFile.c_str());
            fs.SeekToEnd();
            uint32 nSize = fs.GetSerializeSize(t);
            fs << nMagicNum << nSize;
            pos.nOffset = fs.GetCurPos();
            fs << t;
        }
        catch (std::exception& e)
        {
            xengine::StdError(__PRETTY_FUNCTION__, e.what());
            return false;
        }
        return true;
    }
    template <typename T>
    bool WriteBatch(const typename std::vector<T>& vBatch, std::vector<CDiskPos>& vPos)
    {
        boost::unique_lock<boost::mutex> lock(mtxWriter);

        size_t n = 0;

        while (n < vBatch.size())
        {
            uint32 nFile, nOffset;
            std::string pathFile;
            if (!GetLastFilePath(nFile, pathFile))
            {
                return false;
            }
            try
            {
                xengine::CFileStream fs(pathFile.c_str());
                fs.SeekToEnd();
                do
                {
                    uint32 nSize = fs.GetSerializeSize(vBatch[n]);
                    fs << nMagicNum << nSize;
                    nOffset = fs.GetCurPos();
                    fs << vBatch[n++];
                    vPos.push_back(CDiskPos(nFile, nOffset));
                } while (n < vBatch.size() && nOffset < MAX_FILE_SIZE - MAX_CHUNK_SIZE - 8);
            }
            catch (std::exception& e)
            {
                xengine::StdError(__PRETTY_FUNCTION__, e.what());
                return false;
            }
        }

        return true;
    }
    template <typename T>
    bool Read(T& t, const CDiskPos& pos)
    {
        std::string pathFile;
        if (!GetFilePath(pos.nFile, pathFile))
        {
            return false;
        }
        try
        {
            // Open history file to read
            xengine::CFileStream fs(pathFile.c_str());
            fs.Seek(pos.nOffset);
            fs >> t;
        }
        catch (std::exception& e)
        {
            xengine::StdError(__PRETTY_FUNCTION__, e.what());
            return false;
        }
        return true;
    }

protected:
    boost::mutex mtxWriter;
    static const uint32 nMagicNum;
};

} // namespace storage
} // namespace bigbang

#endif //STORAGE_TIMESERIES_H
