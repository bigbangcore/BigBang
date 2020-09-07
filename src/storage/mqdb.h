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

const std::string CLIENT_ID_OUT_OF_MQ_CLUSTER = "OUTER-NODE";

class CForkKnownIP
{
public:
    CForkKnownIP() {}
    CForkKnownIP(const uint256& forkidIn, const uint32& nodeipIn)
      : forkID(forkidIn), nodeIP(nodeipIn) {}

public:
    uint256 forkID;
    uint32 nodeIP;
};

typedef boost::multi_index_container<
    CForkKnownIP,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CForkKnownIP, uint256, &CForkKnownIP::forkID>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CForkKnownIP, uint32, &CForkKnownIP::nodeIP>>>>
    CForkKnownIPSet;

typedef CForkKnownIPSet::nth_index<0>::type CForkKnownIpSetById;
typedef CForkKnownIPSet::nth_index<1>::type CForkKnownIpSetByIp;

class CSuperNode
{
    friend class xengine::CStream;

public:
    std::string superNodeID;
    uint32 ipAddr;
    std::vector<uint256> vecOwnedForks;
    int8 nodeCat;

public:
    CSuperNode(const std::string& id = std::string(), uint32 ip = 0,
               const std::vector<uint256>& forks = std::vector<uint256>(), int8 cat = 0)
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

    static std::string Int2Ip(const unsigned long& ipNum)
    {
        boost::asio::ip::address_v4 addr(ipNum);
        return addr.to_string();
    }

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "CSuperNode : superNodeID=" << superNodeID
            << " ipAddr=" << Int2Ip(ipAddr) << " ";
        for (auto const& f : vecOwnedForks)
        {
            oss << " ownedFork=" << f.ToString() << " ";
        }
        oss << " nodeCat=" << std::to_string(nodeCat) << " ";
        return oss.str();
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
    CSuperNodeDB() = default;
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool AddNewSuperNode(const CSuperNode& cli);
    bool RemoveSuperNode(const std::string& cliID, const uint32& ipNum);
    bool RetrieveSuperNode(const std::string& superNodeID, const uint32& ipNum, std::vector<uint256>& vFork);
    bool UpdateSuperNode(const std::string& cliID, const uint32& ipNum, const std::vector<uint256>& vFork);
    bool ListSuperNode(std::vector<CSuperNode>& vCli); //return all nodes
    void Clear();
    bool ClearSuperNode(const CSuperNode& cli);
    bool FetchSuperNode(std::vector<CSuperNode>& vCli, const uint8& mask);  //only return super nodes on request
    bool AddOuterNodes(const std::vector<CSuperNode>& outers, bool fSuper); //outer nodes come from peers

protected:
    bool LoadSuperNodeWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                             std::map<std::pair<std::string, uint32>, std::vector<uint256>>& mapCli);
    bool FetchSuperNodeWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                              std::map<std::pair<std::string, uint32>, std::vector<uint256>>& mapCli, const uint8& mask);
};

} // namespace storage
} // namespace bigbang

#endif //BIGBANG_MQDB_H
