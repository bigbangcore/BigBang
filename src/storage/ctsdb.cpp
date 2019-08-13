// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ctsdb.h"

#include <boost/bind.hpp>

#include "leveldbeng.h"

using namespace std;
using namespace xengine;

namespace bigbang
{
namespace storage
{

//////////////////////////////
// CCTSIndex

CCTSIndex::CCTSIndex()
{
}

CCTSIndex::~CCTSIndex()
{
}

bool CCTSIndex::Initialize(const boost::filesystem::path& pathCTSDB)
{
    CLevelDBArguments args;
    args.path = (pathCTSDB / "index").string();
    args.syncwrite = false;
    CLevelDBEngine* engine = new CLevelDBEngine(args);

    if (!Open(engine))
    {
        delete engine;
        return false;
    }

    return true;
}

void CCTSIndex::Deinitialize()
{
    Close();
}

bool CCTSIndex::Update(const vector<int64>& vTime, const vector<CDiskPos>& vPos, const vector<int64>& vDel)
{
    if (vTime.size() != vPos.size())
    {
        return false;
    }

    if (!TxnBegin())
    {
        return false;
    }

    for (int i = 0; i < vTime.size(); i++)
    {
        Write(vTime[i], vPos[i]);
    }

    for (int i = 0; i < vDel.size(); i++)
    {
        Erase(vDel[i]);
    }

    if (!TxnCommit())
    {
        return false;
    }

    return true;
}

bool CCTSIndex::Retrieve(const int64 nTime, CDiskPos& pos)
{
    return Read(nTime, pos);
}

} // namespace storage
} // namespace bigbang
