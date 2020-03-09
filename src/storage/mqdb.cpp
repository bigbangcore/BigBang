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
// CForkNodeDB

bool CForkNodeDB::Initialize(const boost::filesystem::path& pathData)
{
    CLevelDBArguments args;
    args.path = (pathData / "forknode").string();
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

void CForkNodeDB::Deinitialize()
{
    Close();
}

bool CForkNodeDB::AddNewForkNode(const CForkNode& cli)
{
    vector<CForkNode> nodes;
    ListForkNode(nodes);
    bool ret = Write(cli.forkNodeID, cli.vecOwnedForks, true);  //overwrite
    ListForkNode(nodes);
    return ret;
}

bool CForkNodeDB::RemoveForkNode(const string& cliID)
{
    return Erase(cliID);
}

bool CForkNodeDB::RetrieveForkNode(const string& cliID, CForkNode& cli)
{
    return Read(cliID, cli);
}

bool CForkNodeDB::ListForkNode(std::vector<CForkNode>& vCli)
{
    map<std::string, std::vector<uint256>> mapCli;

    if (!WalkThrough(boost::bind(&CForkNodeDB::LoadForkNodeWalker, this, _1, _2, boost::ref(mapCli))))
    {
        return false;
    }

    vCli.reserve(mapCli.size());
    for (const auto& it : mapCli)
    {
        CForkNode node;
        node.forkNodeID = it.first;
        cout << node.forkNodeID << endl;
        node.vecOwnedForks = it.second;
        vCli.emplace_back(node);
        for (const auto& i : node.vecOwnedForks)
        {
            cout << "the fork is:" << i.ToString() << endl;
        }
    }
    return true;
}

void CForkNodeDB::Clear()
{
    RemoveAll();
}

bool CForkNodeDB::LoadForkNodeWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
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
