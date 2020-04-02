// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_MQDB_H
#define BIGBANG_MQDB_H

#include <boost/asio.hpp>

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
    uint32 ipAddr;
    std::vector<uint256> vecOwnedForks;
    int8 nodeCat;

public:
    CSuperNode(std::string id = std::string(), uint32 ip = 0,
               std::vector<uint256> forks = std::vector<uint256>(), int8 cat = 0)
      : superNodeID(id),
        ipAddr(ip),
        vecOwnedForks(forks),
        nodeCat(cat) {}

    static bool Ip2Int(const std::string& ipStr, unsigned long& ipNum)
    {
        boost::system::error_code ec;
        boost::asio::ip::address_v4 addr = boost::asio::ip::address_v4::from_string(ipStr, ec);
        if (!ec)
        {
            ipNum = addr.to_ulong();
            return true;
        }
        return false;
    }

    static bool Int2Ip(const unsigned long& ipNum, std::string& ipStr)
    {
        boost::asio::ip::address_v4 addr(ipNum);
        ipStr = addr.to_string();
        return true;
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(superNodeID, opt);
        s.Serialize(ipAddr, opt);
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
    bool RemoveSuperNode(const std::string& cliID, const uint32& ipNum);
    bool RetrieveSuperNode(const std::string& superNodeID, const uint32& ipNum, std::vector<uint256>& vFork);
    bool UpdateSuperNode(const std::string& cliID, const uint32& ipNum, const std::vector<uint256>& vFork);
    bool ListSuperNode(std::vector<CSuperNode>& vCli);  //return all nodes
    void Clear();
    bool ClearSuperNode(const CSuperNode& cli);
    bool FetchSuperNode(std::vector<CSuperNode>& vCli); //only return super nodes including dpos and fork nodes

protected:
    bool LoadSuperNodeWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                             std::map<std::pair<std::string, uint32>, std::vector<uint256>>& mapCli);
    bool FetchSuperNodeWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                              std::map<std::pair<std::string, uint32>, std::vector<uint256>>& mapCli);
};

} // namespace storage
} // namespace bigbang

#endif //BIGBANG_MQDB_H
