// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_FORKMANAGER_H
#define BIGBANG_FORKMANAGER_H

#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <stack>

#include "base.h"
#include "peernet.h"

namespace bigbang
{

class CForkSchedule
{
public:
    CForkSchedule(bool fAllowedIn = false)
      : fAllowed(fAllowedIn), fSynchronized(false)
    {
    }
    bool IsAllowed() const
    {
        return fAllowed;
    }
    bool IsJointEmpty() const
    {
        return mapJoint.empty();
    }
    bool IsHalted() const
    {
        return (!fAllowed && mapJoint.empty());
    }
    void AddNewJoint(const uint256& hashJoint, const uint256& hashFork)
    {
        mapJoint.insert(std::make_pair(hashJoint, hashFork));
    }
    vector<uint256> RemoveJoint(const uint256& hashJoint)
    {
        vector<uint256> vecNextFork;
        std::multimap<uint256, uint256>::iterator it = mapJoint.lower_bound(hashJoint);
        while (it != mapJoint.upper_bound(hashJoint))
        {
            vecNextFork.push_back(it->second);
            mapJoint.erase(it++);
        }
        return vecNextFork;
    }

    void SetSyncStatus(bool fSync)
    {
        fSynchronized = fSync;
    }
    bool IsSynchronized() const
    {
        return fSynchronized;
    }

public:
    bool fAllowed;
    bool fSynchronized;
    std::multimap<uint256, uint256> mapJoint;
};

class CForkManager : public IForkManager
{
public:
    CForkManager();
    ~CForkManager();
    bool IsAllowed(const uint256& hashFork) const override;
    bool GetJoint(const uint256& hashFork, uint256& hashParent, uint256& hashJoint, int32& nJointHeight) const override;
    bool LoadForkContext(std::vector<uint256>& vActive) override;
    void ForkUpdate(const CBlockChainUpdate& update, std::vector<uint256>& vActive, std::vector<uint256>& vDeactive) override;
    void GetForkList(std::vector<uint256>& vFork) const override;
    bool GetSubline(const uint256& hashFork, std::vector<std::pair<int32, uint256>>& vSubline) const override;
    bool GetCreatedHeight(const uint256& hashFork, int32& nCreatedHeight) const override;
    const CForkSetManager& GetForkSetManager() const override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;
    bool AddNewForkContext(CForkContextEx& ctxt, std::vector<uint256>& vActive);
    bool IsAllowedFork(const uint256& hashFork, const uint256& hashParent) const;

protected:
    mutable boost::shared_mutex rwAccess;
    ICoreProtocol* pCoreProtocol;
    IBlockChain* pBlockChain;
    bool fAllowAnyFork;
    std::set<uint256> setForkAllowed;
    std::set<uint256> setGroupAllowed;
    std::map<uint256, CForkSchedule> mapForkSched;
    CForkSetManager forkSetMgr;
};

} // namespace bigbang

#endif //BIGBANG_FORKMANAGER_H
