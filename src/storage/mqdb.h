// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_MQDB_H
#define BIGBANG_MQDB_H

#include "uint256.h"
#include "xengine.h"

namespace bigbang
{
namespace storage
{

class CForkNode
{
    friend class xengine::CStream;

public:
    std::string forkNodeID;
    std::vector<uint256> vecOwnedForks;

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(forkNodeID, opt);
        s.Serialize(vecOwnedForks, opt);
    }
};

class CForkNodeDB : public xengine::CKVDB
{
public:
    CForkNodeDB() {}
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool AddNewForkNode(const CForkNode& cli);
    bool RemoveForkNode(const std::string& cliID);
    bool RetrieveForkNode(const std::string& forkNodeID, CForkNode& cli);
    bool ListForkNode(std::vector<CForkNode>& vCli);
    void Clear();

protected:
    bool LoadForkNodeWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                            std::map<std::string, std::vector<uint256>>& mapCli);
};

} // namespace storage
} // namespace bigbang

#endif //BIGBANG_MQDB_H
