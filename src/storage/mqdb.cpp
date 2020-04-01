// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mqdb.h"

#include "leveldbeng.h"

using namespace std;
using namespace xengine;

namespace bigbang
{
namespace storage
{

const string CLIENT_ID_OUT_OF_MQ_CLUSTER = "OUTER-NODE";
//////////////////////////////
// CSuperNodeDB

bool CSuperNodeDB::Initialize(const boost::filesystem::path& pathData)
{
    CLevelDBArguments args;
    args.path = (pathData / "supernode").string();
    args.syncwrite = true;
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

void CSuperNodeDB::Deinitialize()
{
    Close();
}

bool CSuperNodeDB::AddNewSuperNode(const CSuperNode& cli)
{
    bool ret = false;
    if (1 == cli.nodeCat)
    {
        if (!ClearSuperNode())
        {
            return false;
        }
        ret = Write(make_pair(cli.superNodeID, cli.ipAddr), cli.vecOwnedForks, true);
    }
    else if (2 == cli.nodeCat)
    {
        if (!ClearSuperNode())
        {
            return false;
        }
        ret = Write(make_pair(cli.superNodeID, cli.ipAddr), cli.vecOwnedForks, true);
    }
    else if (0 == cli.nodeCat)
    {
        ret = Write(make_pair(CLIENT_ID_OUT_OF_MQ_CLUSTER, cli.ipAddr), cli.vecOwnedForks, true);
    }
    return ret;
}

bool CSuperNodeDB::RemoveSuperNode(const string& cliID, const int8& ipNum)
{
    return Erase(make_pair(cliID, ipNum));
}

bool CSuperNodeDB::RetrieveSuperNode(const string& cliID, const int8& ipNum, CSuperNode& cli)
{
    return Read(make_pair(cliID, ipNum), cli);
}

bool CSuperNodeDB::ListSuperNode(std::vector<CSuperNode>& vCli)
{
    map<pair<string, uint32>, vector<uint256>> mapCli;

    if (!WalkThrough(boost::bind(&CSuperNodeDB::LoadSuperNodeWalker, this, _1, _2, boost::ref(mapCli))))
    {
        return false;
    }

    vCli.reserve(mapCli.size());
    for (const auto& it : mapCli)
    {
        CSuperNode node;
        node.superNodeID = it.first.first;
        node.ipAddr = it.first.second;
        node.vecOwnedForks = it.second;
        vCli.emplace_back(node);
    }
    return true;
}

void CSuperNodeDB::Clear()
{
    RemoveAll();
}

bool CSuperNodeDB::FetchSuperNodeWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                                       map<pair<string, uint32>, vector<uint256>>& mapCli)
{
    string strCliID;
    uint32 nIP;
    ssKey >> strCliID >> nIP;

    if (strCliID == CLIENT_ID_OUT_OF_MQ_CLUSTER)
    {
        return true;
    }

    std::vector<uint256> forks;
    ssValue >> forks;
    mapCli.insert(make_pair(make_pair(strCliID, nIP), forks));
    return true;
}

bool CSuperNodeDB::FetchSuperNode(std::vector<CSuperNode>& vCli)
{
    map<pair<string, uint32>, vector<uint256>> mapCli;

    if (!WalkThrough(boost::bind(&CSuperNodeDB::FetchSuperNodeWalker, this, _1, _2, boost::ref(mapCli))))
    {
        return false;
    }

    vCli.reserve(mapCli.size());
    for (const auto& it : mapCli)
    {
        CSuperNode node;
        node.superNodeID = it.first.first;
        node.ipAddr = it.first.second;
        node.vecOwnedForks = it.second;
        vCli.emplace_back(node);
    }
    return true;
}

bool CSuperNodeDB::ClearSuperNode()
{
    vector<CSuperNode> vSuperNode;
    if (!FetchSuperNode(vSuperNode))
    {
        return false;
    }

    for (auto const& supernode : vSuperNode)
    {
        if (!RemoveSuperNode(supernode.superNodeID, supernode.ipAddr))
        {
            return false;
        }
    }

    return true;
}

bool CSuperNodeDB::LoadSuperNodeWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                                     map<pair<string, uint32>, vector<uint256>>& mapCli)
{
    string strCliID;
    uint32 nIP;
    ssKey >> strCliID >> nIP;
    std::vector<uint256> forks;
    ssValue >> forks;
    mapCli.insert(make_pair(make_pair(strCliID, nIP), forks));
    return true;
}

} // namespace storage
} // namespace bigbang
