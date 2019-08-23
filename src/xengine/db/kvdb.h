// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_KVDB_H
#define XENGINE_KVDB_H

#include <boost/function.hpp>
#include <boost/thread.hpp>

#include "stream/stream.h"
#include "util.h"

namespace xengine
{

class CKVDBEngine
{
public:
    virtual ~CKVDBEngine() {}

    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual bool TxnBegin() = 0;
    virtual bool TxnCommit() = 0;
    virtual void TxnAbort() = 0;
    virtual bool Get(CBufStream& ssKey, CBufStream& ssValue) = 0;
    virtual bool Put(CBufStream& ssKey, CBufStream& ssValue, bool fOverwrite) = 0;
    virtual bool Remove(CBufStream& ssKey) = 0;
    virtual bool RemoveAll() = 0;
    virtual bool MoveFirst() = 0;
    virtual bool MoveTo(CBufStream& ssKey) = 0;
    virtual bool MoveNext(CBufStream& ssKey, CBufStream& ssValue) = 0;
};

class CKVDB
{
public:
    typedef boost::function<bool(CBufStream&, CBufStream&)> WalkerFunc;

    CKVDB()
      : dbEngine(nullptr) {}
    CKVDB(CKVDBEngine* engine)
      : dbEngine(engine)
    {
        boost::recursive_mutex::scoped_lock lock(mtx);
        if (dbEngine != nullptr)
        {
            if (!dbEngine->Open())
            {
                delete dbEngine;
                dbEngine = nullptr;
            }
        }
    }

    virtual ~CKVDB()
    {
        boost::recursive_mutex::scoped_lock lock(mtx);
        if (dbEngine != nullptr)
        {
            dbEngine->Close();
            delete dbEngine;
        }
    }

    bool Open(CKVDBEngine* engine)
    {
        if (dbEngine == nullptr && engine != nullptr && engine->Open())
        {
            boost::recursive_mutex::scoped_lock lock(mtx);
            dbEngine = engine;
            return true;
        }

        return false;
    }

    void Close()
    {
        boost::recursive_mutex::scoped_lock lock(mtx);
        if (dbEngine != nullptr)
        {
            dbEngine->Close();
            delete dbEngine;
            dbEngine = nullptr;
        }
    }

    bool TxnBegin()
    {
        boost::recursive_mutex::scoped_lock lock(mtx);
        if (dbEngine != nullptr)
        {
            return dbEngine->TxnBegin();
        }
        return false;
    }

    bool TxnCommit()
    {
        boost::recursive_mutex::scoped_lock lock(mtx);
        if (dbEngine != nullptr)
        {
            return dbEngine->TxnCommit();
        }
        return false;
    }

    void TxnAbort()
    {
        boost::recursive_mutex::scoped_lock lock(mtx);
        if (dbEngine != nullptr)
        {
            dbEngine->TxnAbort();
        }
    }

    bool RemoveAll()
    {
        boost::recursive_mutex::scoped_lock lock(mtx);
        if (dbEngine != nullptr)
        {
            if (dbEngine->RemoveAll())
            {
                return true;
            }
            Close();
        }
        return false;
    }

    bool IsValid() const
    {
        return (dbEngine != nullptr);
    }

protected:
    virtual bool DBWalker(CBufStream& ssKey, CBufStream& ssValue)
    {
        return false;
    }
    template <typename K, typename T>
    bool Read(const K& key, T& value)
    {
        CBufStream ssKey, ssValue;
        ssKey << key;

        try
        {
            boost::recursive_mutex::scoped_lock lock(mtx);

            if (dbEngine == nullptr)
                return false;

            if (dbEngine->Get(ssKey, ssValue))
            {
                ssValue >> value;
                return true;
            }
        }
        catch (const boost::thread_interrupted&)
        {
            throw;
        }
        catch (std::exception& e)
        {
            StdError(__PRETTY_FUNCTION__, e.what());
        }

        return false;
    }

    template <typename K, typename T>
    bool Write(const K& key, const T& value, bool fOverwrite = true)
    {
        CBufStream ssKey, ssValue;
        ssKey << key;
        ssValue << value;

        try
        {
            boost::recursive_mutex::scoped_lock lock(mtx);

            if (dbEngine == nullptr)
                return false;
            return dbEngine->Put(ssKey, ssValue, fOverwrite);
        }
        catch (const boost::thread_interrupted&)
        {
            throw;
        }
        catch (std::exception& e)
        {
            StdError(__PRETTY_FUNCTION__, e.what());
        }

        return false;
    }

    template <typename K>
    bool Erase(const K& key)
    {
        CBufStream ssKey;
        ssKey << key;

        try
        {
            boost::recursive_mutex::scoped_lock lock(mtx);

            if (dbEngine == nullptr)
                return false;
            return dbEngine->Remove(ssKey);
        }
        catch (const boost::thread_interrupted&)
        {
            throw;
        }
        catch (std::exception& e)
        {
            StdError(__PRETTY_FUNCTION__, e.what());
        }

        return false;
    }

    bool WalkThrough()
    {
        try
        {
            boost::recursive_mutex::scoped_lock lock(mtx);

            if (dbEngine == nullptr)
                return false;

            if (!dbEngine->MoveFirst())
                return false;

            for (;;)
            {
                CBufStream ssKey, ssValue;
                if (!dbEngine->MoveNext(ssKey, ssValue))
                    break;

                if (!DBWalker(ssKey, ssValue))
                    break;
            }
            return true;
        }
        catch (std::exception& e)
        {
            StdError(__PRETTY_FUNCTION__, e.what());
        }

        return false;
    }

    bool WalkThrough(WalkerFunc fnWalker)
    {
        try
        {
            boost::recursive_mutex::scoped_lock lock(mtx);

            if (dbEngine == nullptr)
                return false;

            if (!dbEngine->MoveFirst())
                return false;

            for (;;)
            {
                CBufStream ssKey, ssValue;
                if (!dbEngine->MoveNext(ssKey, ssValue))
                    break;

                if (!fnWalker(ssKey, ssValue))
                    break;
            }
            return true;
        }
        catch (std::exception& e)
        {
            StdError(__PRETTY_FUNCTION__, e.what());
        }

        return false;
    }

    template <typename K>
    bool WalkThrough(WalkerFunc fnWalker, const K& keyBegin)
    {
        try
        {
            boost::recursive_mutex::scoped_lock lock(mtx);

            if (dbEngine == nullptr)
                return false;

            CBufStream ssKeyBegin;
            ssKeyBegin << keyBegin;

            if (!dbEngine->MoveTo(ssKeyBegin))
                return false;

            for (;;)
            {
                CBufStream ssKey, ssValue;
                if (!dbEngine->MoveNext(ssKey, ssValue))
                    break;

                if (!fnWalker(ssKey, ssValue))
                    break;
            }
            return true;
        }
        catch (std::exception& e)
        {
            StdError(__PRETTY_FUNCTION__, e.what());
        }

        return false;
    }

protected:
    boost::recursive_mutex mtx;
    CKVDBEngine* dbEngine;
};

} // namespace xengine

#endif //XENGINE_KVDB_H
