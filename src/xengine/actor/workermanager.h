// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_MESSAGE_WORKERMANAGER_H
#define XENGINE_MESSAGE_WORKERMANAGER_H

#include <unordered_map>
namespace xengine
{

class CActorWorker;

/**
 * @brief CActorWorkerManager Manage actor workers
 * @tparam T worker tag type
 */
template <typename T>
class CWorkerManager
{
public:
    /**
     * @brief Construstor
     */
    CWorkerManager() {}

    /**
     * @brief Deconstrustor
     */
    ~CWorkerManager()
    {
        Clear();
    }

    /**
     * @brief Get worker count
     * @return worker count
     */
    size_t Count() const
    {
        return mapWorker.size();
    }

    /**
     * @brief Add a worker
     * @param tag worker tag
     * @param spWorkerIn worker
     * @return spWorkerIn
     */
    std::shared_ptr<CActorWorker> Add(const T& tag, std::shared_ptr<CActorWorker> spWorkerIn)
    {
        mapWorker.insert(make_pair(tag, spWorkerIn));
        return mapWorker[tag];
    }

    /**
     * @brief Get the worker with tag
     * @param tag worker tag
     * @return worker
     */
    std::shared_ptr<CActorWorker> Get(const T& tag)
    {
        auto it = mapWorker.find(tag);
        return (it == mapWorker.end()) ? nullptr : it->second;
    }

    /**
     * @brief Remove the worker with tag
     * @param tag worker tag
     */
    void Remove(const T& tag)
    {
        mapWorker.erase(tag);
    }

    /**
     * @brief Remove all workers
     */
    void Clear()
    {
        mapWorker.clear();
    }

protected:
    /// worker set
    std::unordered_map<T, std::shared_ptr<CActorWorker>> mapWorker;
};

} // namespace xengine

#endif // XENGINE_MESSAGE_WORKERMANAGER_H