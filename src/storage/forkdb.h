// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_FORKDB_H
#define STORAGE_FORKDB_H

#include <map>

#include "forkcontext.h"
#include "uint256.h"
#include "xengine.h"

namespace bigbang
{
namespace storage
{

class CForkDB : public xengine::CKVDB
{
    LOGGER_CHANNEL("CForkDB");

public:
    CForkDB() {}
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool AddNewForkContext(const CForkContext& ctxt);
    bool RemoveForkContext(const uint256& hashFork);
    bool RetrieveForkContext(const uint256& hashFork, CForkContext& ctxt);
    bool ListForkContext(std::vector<CForkContext>& vForkCtxt);
    bool UpdateFork(const uint256& hashFork, const uint256& hashLastBlock = uint256());
    bool RemoveFork(const uint256& hashFork);
    bool RetrieveFork(const uint256& hashFork, uint256& hashLastBlock);
    bool ListFork(std::vector<std::pair<uint256, uint256>>& vFork);
    void Clear();

protected:
    bool LoadCtxtWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                        std::multimap<int, CForkContext>& mapCtxt);
    bool LoadForkWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                        std::multimap<int, uint256>& mapJoint, std::map<uint256, uint256>& mapFork);
};

} // namespace storage
} // namespace bigbang

#endif //STORAGE_FORKDB_H
