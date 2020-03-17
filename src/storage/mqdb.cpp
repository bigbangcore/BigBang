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
    vector<CSuperNode> nodes;
    ListSuperNode(nodes);
    bool ret = Write(cli.superNodeID, cli.vecOwnedForks, true);  //overwrite
    ListSuperNode(nodes);
    return ret;
}

bool CSuperNodeDB::RemoveSuperNode(const string& cliID)
{
    return Erase(cliID);
}

bool CSuperNodeDB::RetrieveSuperNode(const string& cliID, CSuperNode& cli)
{
    return Read(cliID, cli);
}

bool CSuperNodeDB::ListSuperNode(std::vector<CSuperNode>& vCli)
{
    map<std::string, std::vector<uint256>> mapCli;

    if (!WalkThrough(boost::bind(&CSuperNodeDB::LoadSuperNodeWalker, this, _1, _2, boost::ref(mapCli))))
    {
        return false;
    }

    vCli.reserve(mapCli.size());
    for (const auto& it : mapCli)
    {
        CSuperNode node;
        node.superNodeID = it.first;
        node.vecOwnedForks = it.second;
        vCli.emplace_back(node);
    }
    return true;
}

void CSuperNodeDB::Clear()
{
    RemoveAll();
}

bool CSuperNodeDB::LoadSuperNodeWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                                     map<std::string, std::vector<uint256>>& mapCli)
{
    string strCliID;
    ssKey >> strCliID;
    std::vector<uint256> forks;
    ssValue >> forks;
    mapCli.insert(make_pair(strCliID, forks));
    return true;
}

} // namespace storage
} // namespace bigbang
