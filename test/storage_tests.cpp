// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "address.h"
#include "block.h"
#include "test_big.h"
#include "timeseries.h"

using namespace std;
using namespace xengine;
using namespace bigbang;
using namespace bigbang::storage;
using namespace boost::filesystem;

BOOST_FIXTURE_TEST_SUITE(storage_tests, BasicUtfSetup)

// basic config
const uint32 nMagicNum = 0x5E33A1EF;

BOOST_AUTO_TEST_CASE(filetest)
{
    printf("%s  file test..........\n", GetLocalTime().c_str());
    int64 nBeginTime = GetTime();

    try
    {
        xengine::CFileStream fs("./.bigbang/block/block_000001.dat");
        fs.Seek(0);

        uint32 nOffset = 0;
        uint32 nBlockCount = 0;

        while (!fs.IsEOF())
        {
            //printf("file test: nBlockCount: %d..........\n", nBlockCount);
            uint32 nMagic, nSize;
            CBlockEx t;
            try
            {
                fs >> nMagic >> nSize >> t;
            }
            catch (std::exception& e)
            {
                BOOST_ERROR("error: " << e.what());
                break;
            }
            if (nMagic != nMagicNum || fs.GetCurPos() - nOffset - 8 != nSize)
            {
                printf("nMagic error, nMagic: %x, nMagicNum: %x, GetCurPos: %ld, nOffset: %d, nSize: %d\n",
                       nMagic, nMagicNum, fs.GetCurPos(), nOffset, nSize);
                break;
            }
            nOffset = fs.GetCurPos();
            nBlockCount++;
        }
        printf("%s  file test success: nBlockCount: %d, time: %ld.\n", GetLocalTime().c_str(), nBlockCount, GetTime() - nBeginTime);
    }
    catch (std::exception& e)
    {
        xengine::StdError(__PRETTY_FUNCTION__, e.what());
    }
}

#define BLOCKFILE_PREFIX "block"

class CMyBlockWalker : public CTSWalker<CBlockEx>
{
public:
    CMyBlockWalker()
      : nBlockCount(0) {}
    ~CMyBlockWalker() {}

    bool Walk(const CBlockEx& block, uint32 nFile, uint32 nOffset) override
    {
        nBlockCount++;
        return true;
    }

public:
    int nBlockCount;
};

BOOST_AUTO_TEST_CASE(timewalk)
{
    CTimeSeriesCached tsBlock;
    BOOST_CHECK(tsBlock.Initialize(path("./.bigbang") / "block", BLOCKFILE_PREFIX));

    printf("%s  WalkThrough start....\n", GetLocalTime().c_str());

    uint32 nLastFileRet = 0;
    uint32 nLastPosRet = 0;
    CMyBlockWalker walker;
    BOOST_CHECK(tsBlock.WalkThrough(walker, nLastFileRet, nLastPosRet, false));
    printf("%s  WalkThrough success, count: %d\n", GetLocalTime().c_str(), walker.nBlockCount);
}

BOOST_AUTO_TEST_CASE(fileread)
{
    printf("%s  start....\n", GetLocalTime().c_str());

    FILE* f = fopen("./.bigbang/block/block_000001.dat", "rb");
    if (f == nullptr)
    {
        printf("fopen fail\n");
        return;
    }
    fseek(f, 0, SEEK_END);
    int filesize = ftell(f);
    printf("%s  filesize: %d\n", GetLocalTime().c_str(), filesize);
    fseek(f, 0, SEEK_SET);

    uint32 nDataLen = 0;
    uint8* pBuf = (uint8*)malloc(filesize);
    if (pBuf == NULL)
    {
        printf("malloc fail\n");
        return;
    }
    uint8* pos = pBuf;
    while (!feof(f))
    {
        int sylen = filesize - nDataLen;
        if (sylen > 1024 * 1024 * 4)
        {
            sylen = 1024 * 1024 * 4;
        }
        else
        {
            if (sylen <= 0)
            {
                break;
            }
        }
        size_t nLen = fread(pos, 1, sylen, f);
        if (nLen > 0)
        {
            nDataLen += nLen;
            pos += nLen;
        }
    }
    fclose(f);
    printf("%s  read end, nDataLen: %d.\n", GetLocalTime().c_str(), nDataLen);

    CBufStream is;
    is.Write((const char*)pBuf, nDataLen);

    int nBlockCount = 0;
    uint32 nSyDataLen = nDataLen;
    while (nSyDataLen > 0)
    {
        uint32 nMagic, nSize;
        CBlockEx t;
        try
        {
            is >> nMagic >> nSize >> t;
        }
        catch (std::exception& e)
        {
            BOOST_ERROR("error: " << e.what());
            break;
        }
        nBlockCount++;
        nSyDataLen -= (nSize + 8);
    }
    printf("%s  data end, nDataLen: %d, nBlockCount: %d, nSyDataLen: %d.\n", GetLocalTime().c_str(), nDataLen, nBlockCount, nSyDataLen);

    free(pBuf);
}

BOOST_AUTO_TEST_SUITE_END()
