// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "forkdb.h"

#include <boost/bind.hpp>

#include "leveldbeng.h"

using namespace std;
using namespace xengine;

namespace bigbang
{
namespace storage
{

//////////////////////////////
// CForkDB

bool CForkDB::Initialize(const boost::filesystem::path& pathData)
{
    CLevelDBArguments args;
    args.path = (pathData / "fork").string();
    args.syncwrite = false;
    args.files = 16;
    args.cache = 2 << 20;

    CLevelDBEngine* engine = new CLevelDBEngine(args);

    if (!Open(engine))
    {
        delete engine;
        return false;
    }

    return true;
}

void CForkDB::Deinitialize()
{
    Close();
}

bool CForkDB::AddNewForkContext(const CForkContext& ctxt)
{
    return Write(make_pair(string("ctxt"), ctxt.hashFork), ctxt);
}

bool CForkDB::RemoveForkContext(const uint256& hashFork)
{
    return Erase(make_pair(string("ctxt"), hashFork));
}

bool CForkDB::RetrieveForkContext(const uint256& hashFork, CForkContext& ctxt)
{
    return Read(make_pair(string("ctxt"), hashFork), ctxt);
}

bool CForkDB::ListForkContext(vector<CForkContext>& vForkCtxt)
{
    multimap<int, CForkContext> mapCtxt;

    if (!WalkThrough(boost::bind(&CForkDB::LoadCtxtWalker, this, _1, _2, boost::ref(mapCtxt))))
    {
        return false;
    }

    vForkCtxt.reserve(mapCtxt.size());
    for (multimap<int, CForkContext>::iterator it = mapCtxt.begin(); it != mapCtxt.end(); ++it)
    {
        vForkCtxt.push_back((*it).second);
    }

    return true;
}

bool CForkDB::UpdateFork(const uint256& hashFork, const uint256& hashLastBlock)
{
    return Write(make_pair(string("active"), hashFork), hashLastBlock);
}

bool CForkDB::RemoveFork(const uint256& hashFork)
{
    return Erase(make_pair(string("active"), hashFork));
}

bool CForkDB::RetrieveFork(const uint256& hashFork, uint256& hashLastBlock)
{
    return Read(make_pair(string("active"), hashFork), hashLastBlock);
}

bool CForkDB::ListFork(vector<pair<uint256, uint256>>& vFork)
{
    multimap<int, uint256> mapJoint;
    map<uint256, uint256> mapFork;

    if (!WalkThrough(boost::bind(&CForkDB::LoadForkWalker, this, _1, _2, boost::ref(mapJoint), boost::ref(mapFork))))
    {
        return false;
    }

    vFork.reserve(mapFork.size());
    for (multimap<int, uint256>::iterator it = mapJoint.begin(); it != mapJoint.end(); ++it)
    {
        map<uint256, uint256>::iterator mi = mapFork.find((*it).second);
        if (mi != mapFork.end())
        {
            vFork.push_back(*mi);
        }
    }
    return true;
}

void CForkDB::Clear()
{
    RemoveAll();
}

bool CForkDB::LoadCtxtWalker(CBufStream& ssKey, CBufStream& ssValue, multimap<int, CForkContext>& mapCtxt)
{
    string strPrefix;
    ssKey >> strPrefix;

    if (strPrefix == "ctxt")
    {
        CForkContext ctxt;
        ssValue >> ctxt;
        mapCtxt.insert(make_pair(ctxt.nJointHeight, ctxt));
    }
    return true;
}

bool CForkDB::LoadForkWalker(CBufStream& ssKey, CBufStream& ssValue,
                             multimap<int, uint256>& mapJoint, map<uint256, uint256>& mapFork)
{
    string strPrefix;
    uint256 hashFork;
    ssKey >> strPrefix >> hashFork;

    if (strPrefix == "ctxt")
    {
        CForkContext ctxt;
        ssValue >> ctxt;
        mapJoint.insert(make_pair(ctxt.nJointHeight, hashFork));
    }
    else if (strPrefix == "active")
    {
        uint256 hashLastBlock;
        ssValue >> hashLastBlock;
        mapFork.insert(make_pair(hashFork, hashLastBlock));
    }
    return true;
}

} // namespace storage
} // namespace bigbang
