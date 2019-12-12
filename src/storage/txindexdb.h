// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_TXINDEXDB_H
#define STORAGE_TXINDEXDB_H

#include <boost/thread/thread.hpp>

#include "ctsdb.h"
#include "transaction.h"
#include "xengine.h"

namespace bigbang
{
namespace storage
{

class CTxIndexDB
{
    typedef CCTSDB<uint224, CTxIndex, CCTSChunkSnappy<uint224, CTxIndex>> CForkTxDB;

public:
    CTxIndexDB();
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool LoadFork(const uint256& hashFork);
    bool Update(const uint256& hashFork, const std::vector<std::pair<uint256, CTxIndex>>& vTxNew,
                const std::vector<uint256>& vTxDel);
    bool Retrieve(const uint256& hashFork, const uint256& txid, CTxIndex& txIndex);
    bool Retrieve(const uint256& txid, CTxIndex& txIndex, uint256& hashFork);

    void Clear();
    void Flush(const uint256& hashFork);

protected:
    void FlushProc();

protected:
    boost::filesystem::path pathTxIndex;
    xengine::CRWAccess rwAccess;
    std::map<uint256, std::shared_ptr<CForkTxDB>> mapTxDB;

    boost::mutex mtxFlush;
    boost::condition_variable condFlush;
    boost::thread* pThreadFlush;
    bool fStopFlush;
};

} // namespace storage
} // namespace bigbang

#endif //STORAGE_TXINDEXDB_H
