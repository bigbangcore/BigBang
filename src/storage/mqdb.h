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

class CSuperNode
{
    friend class xengine::CStream;

public:
    std::string superNodeID;
    std::vector<uint256> vecOwnedForks;
    int8 nodeCat;

public:
    CSuperNode(std::string id = std::string(), std::vector<uint256> forks = std::vector<uint256>())
    : superNodeID(id), vecOwnedForks(forks) {}

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(superNodeID, opt);
        s.Serialize(vecOwnedForks, opt);
        s.Serialize(nodeCat, opt);
    }
};

class CSuperNodeDB : public xengine::CKVDB
{
public:
    CSuperNodeDB() {}
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool AddNewSuperNode(const CSuperNode& cli);
    bool RemoveSuperNode(const std::string& cliID);
    bool RetrieveSuperNode(const std::string& superNodeID, CSuperNode& cli);
    bool ListSuperNode(std::vector<CSuperNode>& vCli);
    void Clear();

protected:
    bool LoadSuperNodeWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                            std::map<std::string, std::vector<uint256>>& mapCli);
};

} // namespace storage
} // namespace bigbang

#endif //BIGBANG_MQDB_H
