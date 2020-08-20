// Copyright (c) 2019-2020 The Bigbang developers
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

bool CForkDB::WriteGenesisBlockHash(const uint256& hashGenesisBlockIn)
{
    return Write(make_pair(string("GenesisBlock"), uint256()), hashGenesisBlockIn);
}

bool CForkDB::GetGenesisBlockHash(uint256& hashGenesisBlockOut)
{
    return Read(make_pair(string("GenesisBlock"), uint256()), hashGenesisBlockOut);
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

bool CForkDB::ListForkContext(vector<CForkContext>& vForkCtxt, map<uint256, pair<uint256, map<uint256, int>>>& mapValidForkId)
{
    multimap<int, CForkContext> mapCtxt;

    if (!WalkThrough(boost::bind(&CForkDB::LoadCtxtWalker, this, _1, _2, boost::ref(mapCtxt), boost::ref(mapValidForkId))))
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

    uint256 hashGenesisBlock;
    if (!GetGenesisBlockHash(hashGenesisBlock))
    {
        return false;
    }

    uint256 hashLastBlock;
    if (!RetrieveFork(hashGenesisBlock, hashLastBlock))
    {
        hashLastBlock = hashGenesisBlock;
    }

    map<uint256, int> mapValidFork;
    if (hashLastBlock == hashGenesisBlock)
    {
        mapValidFork.insert(make_pair(hashGenesisBlock, 0));
    }
    else
    {
        uint256 hashRefFdBlock;
        if (RetrieveValidForkHash(hashLastBlock, hashRefFdBlock, mapValidFork))
        {
            if (hashRefFdBlock != 0)
            {
                uint256 hashTempBlock;
                map<uint256, int> mapTempValidFork;
                if (RetrieveValidForkHash(hashRefFdBlock, hashTempBlock, mapTempValidFork) && !mapTempValidFork.empty())
                {
                    mapValidFork.insert(mapTempValidFork.begin(), mapTempValidFork.end());
                }
            }
        }
        if (mapValidFork.empty())
        {
            mapValidFork.insert(make_pair(hashGenesisBlock, 0));
        }
    }

    if (!WalkThrough(boost::bind(&CForkDB::LoadForkWalker, this, _1, _2, boost::ref(mapJoint), boost::ref(mapFork))))
    {
        return false;
    }

    vFork.reserve(mapFork.size());
    for (multimap<int, uint256>::iterator it = mapJoint.begin(); it != mapJoint.end(); ++it)
    {
        map<uint256, uint256>::iterator mi = mapFork.find((*it).second);
        if (mi != mapFork.end() && mapValidFork.find(it->second) != mapValidFork.end())
        {
            vFork.push_back(*mi);
        }
    }
    return true;
}

bool CForkDB::AddValidForkHash(const uint256& hashBlock, const uint256& hashRefFdBlock, const map<uint256, int>& mapValidFork)
{
    return Write(make_pair(string("valid"), hashBlock), CValidForkId(hashRefFdBlock, mapValidFork));
}

bool CForkDB::RetrieveValidForkHash(const uint256& hashBlock, uint256& hashRefFdBlock, map<uint256, int>& mapValidFork)
{
    CValidForkId validForkId;
    if (!Read(make_pair(string("valid"), hashBlock), validForkId))
    {
        return false;
    }
    hashRefFdBlock = validForkId.hashRefFdBlock;
    mapValidFork.clear();
    mapValidFork.insert(validForkId.mapForkId.begin(), validForkId.mapForkId.end());
    return true;
}

void CForkDB::Clear()
{
    RemoveAll();
}

bool CForkDB::LoadCtxtWalker(CBufStream& ssKey, CBufStream& ssValue, multimap<int, CForkContext>& mapCtxt,
                             map<uint256, pair<uint256, map<uint256, int>>>& mapBlockForkId)
{
    string strPrefix;
    uint256 hashFork;
    ssKey >> strPrefix >> hashFork;

    if (strPrefix == "ctxt")
    {
        CForkContext ctxt;
        ssValue >> ctxt;
        mapCtxt.insert(make_pair(ctxt.nJointHeight, ctxt));
    }
    else if (strPrefix == "valid")
    {
        CValidForkId validForkId;
        ssValue >> validForkId;
        validForkId.GetForkId(mapBlockForkId[hashFork]);
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
