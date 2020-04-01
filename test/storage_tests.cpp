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
    cout << GetLocalTime() << "  file test.........." << endl;
    int64 nBeginTime = GetTime();

    try
    {
        xengine::CFileStream fs("./.bigbang/block/block_000001.dat");
        fs.Seek(0);

        uint32 nOffset = 0;
        uint32 nBlockCount = 0;

        while (!fs.IsEOF())
        {
            cout << "file test: nBlockCount: " << nBlockCount << endl;
            uint32 nMagic, nSize;
            CBlockEx t;
            try
            {
                fs >> nMagic >> nSize >> t;
            }
            catch (std::exception& e)
            {
                cout << "error: " << e.what();
                break;
            }
            if (nMagic != nMagicNum || fs.GetCurPos() - nOffset - 8 != nSize)
            {
                cout << "nMagic error, nMagic: " << nMagic << ", nMagicNum: " << nMagicNum << ", GetCurPos: " << fs.GetCurPos() << ", nOffset: " << nOffset << ", nSize: " << nSize << endl;
                break;
            }
            nOffset = fs.GetCurPos();
            nBlockCount++;
        }
        cout << GetLocalTime() << " file test success: nBlockCount: " << nBlockCount << ", time: " << GetTime() - nBeginTime << endl;
    }
    catch (std::exception& e)
    {
        cout << "error: " << e.what();
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
    if (!tsBlock.Initialize(path("./.bigbang") / "block", BLOCKFILE_PREFIX))
    {
        cout << "Initialize fail" << endl;
        return;
    }

    cout << GetLocalTime() << "  WalkThrough start...." << endl;

    uint32 nLastFileRet = 0;
    uint32 nLastPosRet = 0;
    CMyBlockWalker walker;
    BOOST_CHECK(tsBlock.WalkThrough(walker, nLastFileRet, nLastPosRet, false));
    cout << GetLocalTime() << "  WalkThrough success, count: " << walker.nBlockCount << endl;
}

BOOST_AUTO_TEST_CASE(fileread)
{
    cout << GetLocalTime << "  start...." << endl;

    FILE* f = fopen("./.bigbang/block/block_000001.dat", "rb");
    if (f == nullptr)
    {
        cout << "fopen fail" << endl;
        return;
    }
    fseek(f, 0, SEEK_END);
    int filesize = ftell(f);
    cout << GetLocalTime() << "  filesize: " << filesize << endl;
    fseek(f, 0, SEEK_SET);

    uint32 nDataLen = 0;
    uint8* pBuf = (uint8*)malloc(filesize);
    if (pBuf == NULL)
    {
        cout << "malloc fail" << endl;
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
    cout << GetLocalTime() << "  read end, nDataLen: " << nDataLen << endl;

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
            cout << "error: " << e.what() << endl;
            break;
        }
        nBlockCount++;
        nSyDataLen -= (nSize + 8);
    }
    cout << GetLocalTime << "  data end, nDataLen: " << nDataLen << ", nBlockCount: " << nBlockCount << ", nSyDataLen: " << nSyDataLen << endl;

    free(pBuf);
}

BOOST_AUTO_TEST_SUITE_END()
