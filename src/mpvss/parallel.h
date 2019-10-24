// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MPVSS_PARALLEL_H
#define MPVSS_PARALLEL_H

#include <algorithm>
#include <atomic>
#include <exception>
#include <future>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

#include "logger.h"

/**
 * Parallel computer for CPU intensive computing
 */
class ParallelComputer
{
public:
    ParallelComputer(uint8_t nNum = std::thread::hardware_concurrency())
      : nParallelNum(nNum)
    {
        if (nParallelNum == 0)
        {
            nParallelNum = std::thread::hardware_concurrency();
        }
    }

    /**
     * Transform [nTotal] data from [fnInput] to [fnOutput] by [fnTrans].
     * All transformation success return true, or false.
     * nTotal, number of data.
     * fnInput, function "T (index)" or "tuple<T1, T2...> (index)" fetch original data "T" or "T1,T2..." of "index".
     * fnOutput, function "void (index, T)" save transformed data "T" of "index".
     * fnTrans, function "T (U)" or "T (U1, U2...)" transforms data from "U" or "U1,U2...".
     * 
     * NOTICE: All data about fnInput, fnOutput, fnTrans will be used by multi-threads, confirm they are visited(heap, global)
     * NOTICE: Impletation is without mutex, so suggest to use [vector] instead of [map]
     */
    template <typename InputFunc, typename OutputFunc, typename TransFunc>
    bool Transform(const uint32_t nTotal, InputFunc fnInput, OutputFunc fnOutput, TransFunc fnTrans)
    {
        std::atomic_size_t nCurrent;
        nCurrent.store(0);

        uint8_t nThreads = std::min(nTotal, (uint32_t)nParallelNum);
        std::vector<std::future<bool>> vFuture(nThreads);

        for (int i = 0; i < nThreads; ++i)
        {
            vFuture[i] = std::async(std::launch::async, [&] {
                try
                {
                    size_t nIndex;
                    while ((nIndex = nCurrent.fetch_add(1)) < nTotal)
                    {
                        auto params = fnInput(nIndex);
                        auto result = CallFunction(fnTrans, params, IsTuple<typename std::decay<decltype(params)>::type>());
                        fnOutput(nIndex, result);
                    }
                    return true;
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("parallel", e.what());
                    return false;
                }
            });
        }

        bool ret = true;
        std::for_each(vFuture.begin(), vFuture.end(), [&ret](std::future<bool>& f) { ret &= f.get(); });
        return ret;
    }

    /**
     * Transform [itInBegin, itInEnd) data to [itOutBegin, ) by [fnTrans].
     * All transformation success return true, or false.
     * itInBegin, begin iterator of original data.
     * itInEnd, end iterator of original data.
     * itOutBegin, begin iterator of transformed data. Confirm reserved enough space from [itOutBegin], at least diff(itInBegin, itInEnd)
     * fnTrans, function "T (U)" or "T (U1, U2...)" transforms data from "U" or "U1,U2...".
     * 
     * NOTICE: Impletation is without mutex, so suggest to use [random iterator] instead of [forward iterator].
     */
    template <typename InputIterator, typename OutputIterator, typename TransFunc>
    bool Transform(InputIterator itInBegin, InputIterator itInEnd, OutputIterator itOutBegin, TransFunc fnTrans)
    {
        uint32_t nTotal = IteratorDifferece(itInBegin, itInEnd, typename std::iterator_traits<InputIterator>::iterator_category());

        std::atomic_size_t nCurrent;
        nCurrent.store(0);

        uint8_t nThreads = std::min(nTotal, (uint32_t)nParallelNum);
        std::vector<std::future<bool>> vFuture(nThreads);

        for (int i = 0; i < nThreads; ++i)
        {
            vFuture[i] = std::async(std::launch::async, [&] {
                try
                {
                    size_t nIndex;
                    while ((nIndex = nCurrent.fetch_add(1)) < nTotal)
                    {
                        auto& params = *IteratorIncrease(itInBegin, nIndex, typename std::iterator_traits<InputIterator>::iterator_category());
                        auto result = CallFunction(fnTrans, params, IsTuple<typename std::decay<decltype(params)>::type>());
                        *IteratorIncrease(itOutBegin, nIndex, typename std::iterator_traits<OutputIterator>::iterator_category()) = result;
                    }
                    return true;
                }
                catch (std::exception& e)
                {
                    LOG_ERROR("parallel", e.what());
                    return false;
                }
            });
        }

        bool ret = true;
        std::for_each(vFuture.begin(), vFuture.end(), [&ret](std::future<bool>& f) { ret &= f.get(); });
        return ret;
    }

    /**
     * Execute [nTotal] data from [fnInput] by [fnTrans].
     * All execution success return true, or false.
     * nTotal, number of data.
     * fnInput, function "T (index)" or "tuple<T1, T2...> (index)" fetch original data "T" or "T1,T2..." of "index".
     * fnTrans, function "T (U)" or "T (U1, U2...)" executes data with "U" or "U1,U2...".
     * 
     * NOTICE: All data about fnInput, fnTrans will be used by multi-threads, confirm they are visited(heap, global)
     * NOTICE: Impletation is without mutex, so suggest to use [vector] instead of [map]
     */
    template <typename InputFunc, typename TransFunc>
    bool Execute(const uint32_t nTotal, InputFunc fnInput, TransFunc fnTrans)
    {
        std::atomic_size_t nCurrent;
        nCurrent.store(0);

        uint8_t nThreads = std::min(nTotal, (uint32_t)nParallelNum);
        std::vector<std::future<bool>> vFuture(nThreads);
        for (int i = 0; i < nThreads; ++i)
        {
            vFuture[i] = std::async(std::launch::async, [&] {
                try
                {
                    size_t nIndex;
                    while ((nIndex = nCurrent.fetch_add(1)) < nTotal)
                    {
                        auto params = fnInput(nIndex);
                        ExecuteFunction(fnTrans, params, IsTuple<typename std::decay<decltype(params)>::type>());
                    }
                    return true;
                }
                catch (std::exception& e)
                {
                    LOG_ERROR("parallel", e.what());
                    return false;
                }
            });
        }

        bool ret = true;
        std::for_each(vFuture.begin(), vFuture.end(), [&ret](std::future<bool>& f) { ret &= f.get(); });
        return ret;
    }

    /**
     * Execute [itInBegin, itInEnd) data by [fnTrans].
     * All execution success return true, or false.
     * itInBegin, begin iterator of original data.
     * itInEnd, end iterator of original data.
     * fnTrans, function "T (U)" or "T (U1, U2...)" executes data with "U" or "U1,U2...".
     * 
     * NOTICE: Impletation is without mutex, so suggest to use [random iterator] instead of [forward iterator].
     */
    template <typename InputIterator, typename TransFunc>
    bool Execute(InputIterator itInBegin, InputIterator itInEnd, TransFunc fnTrans)
    {
        uint32_t nTotal = IteratorDifferece(itInBegin, itInEnd, typename std::iterator_traits<InputIterator>::iterator_category());

        std::atomic_size_t nCurrent;
        nCurrent.store(0);

        uint8_t nThreads = std::min(nTotal, (uint32_t)nParallelNum);
        std::vector<std::future<bool>> vFuture(nThreads);

        for (int i = 0; i < nThreads; ++i)
        {
            vFuture[i] = std::async(std::launch::async, [&] {
                try
                {
                    size_t nIndex;
                    while ((nIndex = nCurrent.fetch_add(1)) < nTotal)
                    {
                        auto params = *IteratorIncrease(itInBegin, nIndex, typename std::iterator_traits<InputIterator>::iterator_category());
                        ExecuteFunction(fnTrans, params, IsTuple<typename std::decay<decltype(params)>::type>());
                    }
                    return true;
                }
                catch (std::exception& e)
                {
                    LOG_ERROR("parallel", e.what());
                    return false;
                }
            });
        }

        bool ret = true;
        std::for_each(vFuture.begin(), vFuture.end(), [&ret](std::future<bool>& f) { ret &= f.get(); });
        return ret;
    }

    /**
     * Execute [nTotal] data from [fnInput] by [fnTrans].
     * If one task return false, all tasks stop and return false, or true.
     * nTotal, number of data.
     * fnInput, function "T (index)" or "tuple<T1, T2...> (index)" fetch original data "T" or "T1,T2..." of "index".
     * fnTrans, function "T (U)" or "T (U1, U2...)" executes data with "U" or "U1,U2...". Return bool.
     * 
     * NOTICE: All data about fnInput, fnTrans will be used by multi-threads, confirm they are visited(heap, global)
     * NOTICE: Impletation is without mutex, so suggest to use [vector] instead of [map]
     */
    template <typename InputFunc, typename TransFunc>
    bool ExecuteUntil(const uint32_t nTotal, InputFunc fnInput, TransFunc fnTrans)
    {
        std::atomic_size_t nCurrent;
        nCurrent.store(0);
        bool fContinue = true;

        uint8_t nThreads = std::min(nTotal, (uint32_t)nParallelNum);
        std::vector<std::future<bool>> vFuture(nThreads);
        for (int i = 0; i < nThreads; ++i)
        {
            vFuture[i] = std::async(std::launch::async, [&] {
                try
                {
                    size_t nIndex;
                    while ((nIndex = nCurrent.fetch_add(1)) < nTotal && fContinue)
                    {
                        auto params = fnInput(nIndex);
                        if (!CallFunction(fnTrans, params, IsTuple<typename std::decay<decltype(params)>::type>()))
                        {
                            fContinue = false;
                            return false;
                        }
                    }
                    return true;
                }
                catch (std::exception& e)
                {
                    LOG_ERROR("parallel", e.what());
                    return false;
                }
            });
        }

        bool ret = true;
        std::for_each(vFuture.begin(), vFuture.end(), [&ret](std::future<bool>& f) { ret &= f.get(); });
        return ret;
    }

    /**
     * Execute [itInBegin, itInEnd) data by [fnTrans].
     * If one task return false, all tasks stop and return false, or true.
     * itInBegin, begin iterator of original data.
     * itInEnd, end iterator of original data.
     * fnTrans, function "T (U)" or "T (U1, U2...)" executes data with "U" or "U1,U2...". Return bool.
     * 
     * NOTICE: Impletation is without mutex, so suggest to use [random iterator] instead of [forward iterator].
     */
    template <typename InputIterator, typename TransFunc>
    bool ExecuteUntil(InputIterator itInBegin, InputIterator itInEnd, TransFunc fnTrans)
    {
        uint32_t nTotal = IteratorDifferece(itInBegin, itInEnd, typename std::iterator_traits<InputIterator>::iterator_category());

        std::atomic_size_t nCurrent;
        nCurrent.store(0);
        bool fContinue = true;

        uint8_t nThreads = std::min(nTotal, (uint32_t)nParallelNum);
        std::vector<std::future<bool>> vFuture(nThreads);

        for (int i = 0; i < nThreads; ++i)
        {
            vFuture[i] = std::async(std::launch::async, [&] {
                try
                {
                    size_t nIndex;
                    while ((nIndex = nCurrent.fetch_add(1)) < nTotal && fContinue)
                    {
                        auto params = *IteratorIncrease(itInBegin, nIndex, typename std::iterator_traits<InputIterator>::iterator_category());
                        if (!CallFunction(fnTrans, params, IsTuple<typename std::decay<decltype(params)>::type>()))
                        {
                            fContinue = false;
                            return false;
                        }
                    }
                    return true;
                }
                catch (std::exception& e)
                {
                    LOG_ERROR("parallel", e.what());
                    return false;
                }
            });
        }

        bool ret = true;
        std::for_each(vFuture.begin(), vFuture.end(), [&ret](std::future<bool>& f) { ret &= f.get(); });
        return ret;
    }

protected:
    uint8_t nParallelNum;

protected:
    struct NoTuple
    {
    };

    template <int...>
    struct IndexTuple
    {
    };

    template <int N, int... S>
    struct MakeIndex : MakeIndex<N - 1, N - 1, S...>
    {
    };

    template <int... S>
    struct MakeIndex<0, S...>
    {
        typedef IndexTuple<S...> type;
    };

    template <typename T>
    struct IsTuple : NoTuple
    {
    };

    template <typename... T>
    struct IsTuple<std::tuple<T...>> : MakeIndex<sizeof...(T)>::type
    {
    };

    template <typename Fun, typename Tuple, int... S>
    typename std::result_of<Fun(typename std::tuple_element<S, Tuple>::type...)>::type
    CallFunction(Fun f, Tuple& t, IndexTuple<S...>)
    {
        return f(std::get<S>(t)...);
    }

    template <typename Fun, typename Param>
    typename std::result_of<Fun(Param)>::type
    CallFunction(Fun f, Param& p, NoTuple)
    {
        return f(p);
    }

    template <typename Fun, typename Tuple, int... S>
    void ExecuteFunction(Fun f, Tuple& t, IndexTuple<S...>)
    {
        f(std::get<S>(t)...);
    }

    template <typename Fun, typename Param>
    void ExecuteFunction(Fun f, Param& p, NoTuple)
    {
        f(p);
    }

    template <typename Iterator>
    std::ptrdiff_t IteratorDifferece(Iterator begin, Iterator end, std::random_access_iterator_tag)
    {
        return (end - begin);
    }

    template <typename Iterator>
    std::ptrdiff_t IteratorDifferece(Iterator begin, Iterator end, std::forward_iterator_tag)
    {
        std::ptrdiff_t count = 0;
        for (Iterator it = begin; it != end; it++, count++)
            ;
        return count;
    }

    template <typename Iterator>
    Iterator IteratorIncrease(Iterator begin, std::ptrdiff_t diff, std::random_access_iterator_tag)
    {
        return begin + diff;
    }

    template <typename Iterator>
    Iterator IteratorIncrease(Iterator begin, std::ptrdiff_t diff, std::forward_iterator_tag)
    {
        Iterator it = begin;
        for (std::ptrdiff_t i = 0; i < diff; i++, it++)
            ;
        return it;
    }
};

// InputFunc for vector without checking out of range.
// usage: std::bind(LoadVectorData, std::cref(vector), std::placeholders::_1)
template <typename T>
T LoadVectorData(const std::vector<T>& v, const int i)
{
    return v[i];
}

// OutputFunc for vector without checking out of range.
// usage: std::bind(StoreVectorData, std::ref(vector), std::placeholders::_1, std::placeholders::_2)
template <typename T>
void StoreVectorData(std::vector<T>& v, const int i, const T& t)
{
    v[i] = t;
}

// InputFunc for map without checking out of range.
// usage: std::bind(LoadMapData, std::cref(map), std::placeholders::_1)
template <typename T, typename U>
std::pair<T, U> LoadMapData(const std::map<T, U>& m, int i)
{
    auto it = m.begin();
    while (i > 0)
    {
        ++it;
        --i;
    }
    return *it;
}

// OutputFunc for map without checking out of range.
// usage: std::bind(StoreMapData, std::ref(map), std::placeholders::_1, std::placeholders::_2)
template <typename T, typename U>
void StoreMapData(std::map<T, U>& m, int i, const U& u)
{
    auto it = m.begin();
    while (i > 0)
    {
        ++it;
        --i;
    }
    it->second = u;
}

#endif // MPVSS_PARALLEL_H
