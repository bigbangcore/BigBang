// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rocksdbeng.h"

#include <boost/filesystem/path.hpp>
#include <rocksdb/cache.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/table.h>
#include <rocksdb/utilities/leveldb_options.h>

using namespace xengine;

namespace bigbang
{
namespace storage
{

CRocksDBArguments::CRocksDBArguments()
{
    cache = 32 << 20;
    syncwrite = false;
    files = 256;
}

CRocksDBArguments::~CRocksDBArguments()
{
}

CRocksDBEngine::CRocksDBEngine(CRocksDBArguments& arguments)
  : path(arguments.path)
{
    // Migrating from LevelDB to RocksDB
    // https://rocksdb.org/blog/2015/01/16/migrating-from-leveldb-to-rocksdb-2.html
    rocksdb::LevelDBOptions levelDBoptions;
    levelDBoptions.write_buffer_size = arguments.cache / 4;
    levelDBoptions.filter_policy = rocksdb::NewBloomFilterPolicy(10);
    levelDBoptions.create_if_missing = true;
    levelDBoptions.compression = rocksdb::kNoCompression;
    levelDBoptions.max_open_files = arguments.files;

    options = rocksdb::ConvertOptions(levelDBoptions);

    rocksdb::BlockBasedTableOptions table_options;
    table_options.block_cache = rocksdb::NewLRUCache(arguments.cache / 2);
    table_options.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10));
    options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));

    pdb = nullptr;
    piter = nullptr;
    pbatch = nullptr;

    readoptions.verify_checksums = true;

    writeoptions.sync = arguments.syncwrite;
    batchoptions.sync = true;
}

CRocksDBEngine::~CRocksDBEngine()
{
    delete pbatch;
    pbatch = nullptr;
    delete piter;
    piter = nullptr;
    delete pdb;
    pdb = nullptr;
    //delete options.filter_policy;
    //options.filter_policy = nullptr;
    //delete options.block_cache;
    //options.block_cache = nullptr;
}

bool CRocksDBEngine::Open()
{
    rocksdb::Status status = rocksdb::DB::Open(options, path, &pdb);
    return status.ok();
}

void CRocksDBEngine::Close()
{
    delete pbatch;
    pbatch = nullptr;
    delete piter;
    piter = nullptr;
    delete pdb;
    pdb = nullptr;
}

bool CRocksDBEngine::TxnBegin()
{
    if (pbatch != nullptr)
    {
        return false;
    }
    return ((pbatch = new rocksdb::WriteBatch()) != nullptr);
}

bool CRocksDBEngine::TxnCommit()
{
    if (pbatch != nullptr)
    {
        rocksdb::WriteOptions batchoption;

        rocksdb::Status status = pdb->Write(batchoptions, pbatch);
        delete pbatch;
        pbatch = nullptr;
        return status.ok();
    }

    return false;
}

void CRocksDBEngine::TxnAbort()
{
    delete pbatch;
    pbatch = nullptr;
}

bool CRocksDBEngine::Get(CBufStream& ssKey, CBufStream& ssValue)
{
    rocksdb::Slice slKey(ssKey.GetData(), ssKey.GetSize());
    std::string strValue;
    rocksdb::Status status = pdb->Get(readoptions, slKey, &strValue);
    if (status.ok())
    {
        ssValue.Write(strValue.data(), strValue.size());
        return true;
    }
    return false;
}

bool CRocksDBEngine::Put(CBufStream& ssKey, CBufStream& ssValue, bool fOverwrite)
{
    rocksdb::Slice slKey(ssKey.GetData(), ssKey.GetSize());
    if (!fOverwrite)
    {
        std::string strValue;
        rocksdb::Status status = pdb->Get(readoptions, slKey, &strValue);
        if (status.ok() || !status.IsNotFound())
        {
            return false;
        }
    }

    rocksdb::Slice slValue(ssValue.GetData(), ssValue.GetSize());
    if (pbatch != nullptr)
    {
        pbatch->Put(slKey, slValue);
        return true;
    }

    rocksdb::Status status = pdb->Put(writeoptions, slKey, slValue);

    return status.ok();
}

bool CRocksDBEngine::Remove(CBufStream& ssKey)
{
    rocksdb::Slice slKey(ssKey.GetData(), ssKey.GetSize());

    if (pbatch != nullptr)
    {
        pbatch->Delete(slKey);
        return true;
    }

    rocksdb::Status status = pdb->Delete(writeoptions, slKey);

    return status.ok();
}

bool CRocksDBEngine::RemoveAll()
{
    Close();

    rocksdb::Status status = rocksdb::DestroyDB(path, options);
    if (!status.ok())
    {
        return false;
    }

    return Open();
}

bool CRocksDBEngine::MoveFirst()
{
    delete piter;

    if ((piter = pdb->NewIterator(readoptions)) == nullptr)
    {
        return false;
    }

    piter->SeekToFirst();

    return true;
}

bool CRocksDBEngine::MoveTo(CBufStream& ssKey)
{
    delete piter;

    rocksdb::Slice slKey(ssKey.GetData(), ssKey.GetSize());

    if ((piter = pdb->NewIterator(readoptions)) == nullptr)
    {
        return false;
    }

    piter->Seek(slKey);

    return true;
}

bool CRocksDBEngine::MoveNext(CBufStream& ssKey, CBufStream& ssValue)
{
    if (piter == nullptr || !piter->Valid())
        return false;

    rocksdb::Slice slKey = piter->key();
    rocksdb::Slice slValue = piter->value();

    ssKey.Write(slKey.data(), slKey.size());
    ssValue.Write(slValue.data(), slValue.size());

    piter->Next();

    return true;
}

} // namespace storage
} // namespace bigbang