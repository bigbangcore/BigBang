// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "timeseries.h"

using namespace std;
using namespace boost::filesystem;
using namespace xengine;

namespace bigbang
{
namespace storage
{

//////////////////////////////
// CTimeSeriesBase

CTimeSeriesBase::CTimeSeriesBase()
{
    nLastFile = 0;
}

CTimeSeriesBase::~CTimeSeriesBase()
{
}

bool CTimeSeriesBase::Initialize(const path& pathLocationIn, const string& strPrefixIn)
{
    if (!exists(pathLocationIn))
    {
        create_directories(pathLocationIn);
    }

    if (!is_directory(pathLocationIn))
    {
        return false;
    }

    pathLocation = pathLocationIn;
    strPrefix = strPrefixIn;
    nLastFile = 1;

    return CheckDiskSpace();
}

void CTimeSeriesBase::Deinitialize()
{
}

bool CTimeSeriesBase::CheckDiskSpace()
{
    // 15M
    return (space(pathLocation).available > 15000000);
}

const std::string CTimeSeriesBase::FileName(uint32 nFile)
{
    ostringstream oss;
    oss << strPrefix << "_" << setfill('0') << setw(6) << nFile << ".dat";
    return oss.str();
}

bool CTimeSeriesBase::GetFilePath(uint32 nFile, string& strPath)
{
    path current = pathLocation / FileName(nFile);
    if (exists(current) && is_regular_file(current))
    {
        strPath = current.string();
        return true;
    }
    return false;
}

bool CTimeSeriesBase::GetLastFilePath(uint32& nFile, std::string& strPath)
{
    for (;;)
    {
        path last = pathLocation / FileName(nLastFile);
        if (!exists(last))
        {
            FILE* fp = fopen(last.string().c_str(), "w+");
            if (fp == nullptr)
            {
                break;
            }
            fclose(fp);
        }
        if (is_regular_file(last) && file_size(last) < MAX_FILE_SIZE - MAX_CHUNK_SIZE - 8)
        {
            nFile = nLastFile;
            strPath = last.string();
            return true;
        }
        nLastFile++;
    }
    return false;
}

//////////////////////////////
// CTimeSeriesCached

const uint32 CTimeSeriesCached::nMagicNum = 0x5E33A1EF;

CTimeSeriesCached::CTimeSeriesCached()
  : cacheStream(FILE_CACHE_SIZE)
{
}

CTimeSeriesCached::~CTimeSeriesCached()
{
}

bool CTimeSeriesCached::Initialize(const path& pathLocationIn, const string& strPrefixIn)
{

    if (!CTimeSeriesBase::Initialize(pathLocationIn, strPrefixIn))
    {
        return false;
    }

    {
        boost::unique_lock<boost::mutex> lock(mtxCache);

        ResetCache();
    }
    return true;
}

void CTimeSeriesCached::Deinitialize()
{
    boost::unique_lock<boost::mutex> lock(mtxCache);

    ResetCache();
}

void CTimeSeriesCached::ResetCache()
{
    cacheStream.Clear();
    mapCachePos.clear();
}

bool CTimeSeriesCached::VacateCache(uint32 nNeeded)
{
    const size_t nHdrSize = 12;

    while (cacheStream.GetBufFreeSpace() < nNeeded + nHdrSize)
    {
        CDiskPos diskpos;
        uint32 nSize = 0;

        try
        {
            cacheStream.Rewind();
            cacheStream >> diskpos >> nSize;
        }
        catch (exception& e)
        {
            ErrorLog(__PRETTY_FUNCTION__, e.what());
            return false;
        }

        mapCachePos.erase(diskpos);
        cacheStream.Consume(nSize + nHdrSize);
    }
    return true;
}

//////////////////////////////
// CTimeSeriesChunk

const uint32 CTimeSeriesChunk::nMagicNum = 0x5E33A1EF;

CTimeSeriesChunk::CTimeSeriesChunk()
{
}

CTimeSeriesChunk::~CTimeSeriesChunk()
{
}

} // namespace storage
} // namespace bigbang
