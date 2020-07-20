// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_CACHE_H
#define XENGINE_CACHE_H

#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/thread/thread.hpp>

#include "rwlock.h"

namespace xengine
{

template <typename K, typename V>
class CCache
{
    class CKeyValue
    {
    public:
        K key;
        mutable V value;

    public:
        CKeyValue() {}
        CKeyValue(const K& keyIn, const V& valueIn)
          : key(keyIn), value(valueIn) {}
    };
    typedef boost::multi_index_container<
        CKeyValue,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<boost::multi_index::member<CKeyValue, K, &CKeyValue::key>>,
            boost::multi_index::sequenced<>>>
        CKeyValueContainer;
    typedef typename CKeyValueContainer::template nth_index<1>::type CKeyValueList;

public:
    CCache(std::size_t nMaxCountIn = 0)
      : nMaxCount(nMaxCountIn) {}
    bool Exists(const K& key) const
    {
        CReadLock rlock(rwAccess);
        return (!!cntrCache.count(key));
    }
    bool Retrieve(const K& key, V& value) const
    {
        CReadLock rlock(rwAccess);
        typename CKeyValueContainer::const_iterator it = cntrCache.find(key);
        if (it != cntrCache.end())
        {
            value = (*it).value;
            return true;
        }
        return false;
    }
    void AddNew(const K& key, const V& value)
    {
        CWriteLock wlock(rwAccess);
        std::pair<typename CKeyValueContainer::iterator, bool> ret = cntrCache.insert(CKeyValue(key, value));
        if (!ret.second)
        {
            (*(ret.first)).value = value;
        }
        if (nMaxCount != 0 && cntrCache.size() > nMaxCount)
        {
            CKeyValueList& listCache = cntrCache.template get<1>();
            listCache.pop_front();
        }
    }
    void Remove(const K& key)
    {
        CWriteLock wlock(rwAccess);
        cntrCache.erase(key);
    }
    void Clear()
    {
        CWriteLock wlock(rwAccess);
        cntrCache.clear();
    }

protected:
    mutable CRWAccess rwAccess;
    CKeyValueContainer cntrCache;
    std::size_t nMaxCount;
};

} // namespace xengine

#endif //XENGINE_CACHE_H
