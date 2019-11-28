// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_ROCKSDBENG_H
#define STORAGE_ROCKSDBENG_H
#include <boost/filesystem/path.hpp>
//#include <leveldb/db.h>
//#include <leveldb/write_batch.h>
#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>

#include "xengine.h"

namespace bigbang
{
namespace storage
{

class CRocksDBArguments
{
public:
    CRocksDBArguments();
    ~CRocksDBArguments();

public:
    std::string path;
    size_t cache;
    bool syncwrite;
    int files;
};

class CRocksDBEngine : public xengine::CKVDBEngine
{
public:
    CRocksDBEngine(CRocksDBArguments& arguments);
    ~CRocksDBEngine();

    bool Open() override;
    void Close() override;
    bool TxnBegin() override;
    bool TxnCommit() override;
    void TxnAbort() override;
    bool Get(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue) override;
    bool Put(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue, bool fOverwrite) override;
    bool Remove(xengine::CBufStream& ssKey) override;
    bool RemoveAll() override;
    bool MoveFirst() override;
    bool MoveTo(xengine::CBufStream& ssKey) override;
    bool MoveNext(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue) override;

protected:
    std::string path;
    rocksdb::DB* pdb;
    rocksdb::Iterator* piter;
    rocksdb::WriteBatch* pbatch;
    rocksdb::Options options;
    rocksdb::ReadOptions readoptions;
    rocksdb::WriteOptions writeoptions;
    rocksdb::WriteOptions batchoptions;
};

} // namespace storage
} // namespace bigbang

#endif //STORAGE_ROCKSDBENG_H