// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "leveldbeng.h"

#include <boost/filesystem/path.hpp>

#include "leveldb/cache.h"
#include "leveldb/filter_policy.h"

using namespace xengine;

namespace bigbang
{
namespace storage
{

CLevelDBArguments::CLevelDBArguments()
{
    cache = 32 << 20;
    syncwrite = false;
    files = 256;
}

CLevelDBArguments::~CLevelDBArguments()
{
}

CLevelDBEngine::CLevelDBEngine(CLevelDBArguments& arguments)
  : path(arguments.path)
{
    options.block_cache = leveldb::NewLRUCache(arguments.cache / 2);
    options.write_buffer_size = arguments.cache / 4;
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    options.create_if_missing = true;
    options.compression = leveldb::kNoCompression;
    options.max_open_files = arguments.files;

    pdb = nullptr;
    piter = nullptr;
    pbatch = nullptr;

    readoptions.verify_checksums = true;

    writeoptions.sync = arguments.syncwrite;
    batchoptions.sync = true;
}

CLevelDBEngine::~CLevelDBEngine()
{
    delete pbatch;
    pbatch = nullptr;
    delete piter;
    piter = nullptr;
    delete pdb;
    pdb = nullptr;
    delete options.filter_policy;
    options.filter_policy = nullptr;
    delete options.block_cache;
    options.block_cache = nullptr;
}

bool CLevelDBEngine::Open()
{
    leveldb::Status status = leveldb::DB::Open(options, path, &pdb);
    if (!status.ok())
    {
        return false;
    }

    return true;
}

void CLevelDBEngine::Close()
{
    delete pbatch;
    pbatch = nullptr;
    delete piter;
    piter = nullptr;
    delete pdb;
    pdb = nullptr;
}

bool CLevelDBEngine::TxnBegin()
{
    if (pbatch != nullptr)
    {
        return false;
    }
    return ((pbatch = new leveldb::WriteBatch()) != nullptr);
}

bool CLevelDBEngine::TxnCommit()
{
    if (pbatch != nullptr)
    {
        leveldb::WriteOptions batchoption;

        leveldb::Status status = pdb->Write(batchoptions, pbatch);
        delete pbatch;
        pbatch = nullptr;
        return status.ok();
    }
    return false;
}

void CLevelDBEngine::TxnAbort()
{
    delete pbatch;
    pbatch = nullptr;
}

bool CLevelDBEngine::Get(CBufStream& ssKey, CBufStream& ssValue)
{
    leveldb::Slice slKey(ssKey.GetData(), ssKey.GetSize());
    std::string strValue;
    leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
    if (status.ok())
    {
        ssValue.Write(strValue.data(), strValue.size());
        return true;
    }
    return false;
}

bool CLevelDBEngine::Put(CBufStream& ssKey, CBufStream& ssValue, bool fOverwrite)
{
    leveldb::Slice slKey(ssKey.GetData(), ssKey.GetSize());
    if (!fOverwrite)
    {
        std::string strValue;
        leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
        if (status.ok() || !status.IsNotFound())
        {
            return false;
        }
    }

    leveldb::Slice slValue(ssValue.GetData(), ssValue.GetSize());
    if (pbatch != nullptr)
    {
        pbatch->Put(slKey, slValue);
        return true;
    }

    leveldb::Status status = pdb->Put(writeoptions, slKey, slValue);

    return status.ok();
}

bool CLevelDBEngine::Remove(CBufStream& ssKey)
{
    leveldb::Slice slKey(ssKey.GetData(), ssKey.GetSize());

    if (pbatch != nullptr)
    {
        pbatch->Delete(slKey);
        return true;
    }

    leveldb::Status status = pdb->Delete(writeoptions, slKey);

    return status.ok();
}

bool CLevelDBEngine::RemoveAll()
{
    Close();

    leveldb::Status status = leveldb::DestroyDB(path, options);
    if (!status.ok())
    {
        return false;
    }

    return Open();
}

bool CLevelDBEngine::MoveFirst()
{
    delete piter;

    if ((piter = pdb->NewIterator(readoptions)) == nullptr)
    {
        return false;
    }

    piter->SeekToFirst();

    return true;
}

bool CLevelDBEngine::MoveTo(CBufStream& ssKey)
{
    delete piter;

    leveldb::Slice slKey(ssKey.GetData(), ssKey.GetSize());

    if ((piter = pdb->NewIterator(readoptions)) == nullptr)
    {
        return false;
    }

    piter->Seek(slKey);

    return true;
}

bool CLevelDBEngine::MoveNext(CBufStream& ssKey, CBufStream& ssValue)
{
    if (piter == nullptr || !piter->Valid())
        return false;

    leveldb::Slice slKey = piter->key();
    leveldb::Slice slValue = piter->value();

    ssKey.Write(slKey.data(), slKey.size());
    ssValue.Write(slValue.data(), slValue.size());

    piter->Next();

    return true;
}

} // namespace storage
} // namespace bigbang
