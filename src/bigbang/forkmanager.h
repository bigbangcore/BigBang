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

class CForkLink
{
public:
    CForkLink()
      : nJointHeight(-1) {}
    CForkLink(const CForkContext& ctxt)
      : hashFork(ctxt.hashFork), hashParent(ctxt.hashParent), hashJoint(ctxt.hashJoint), nJointHeight(ctxt.nJointHeight)
    {
    }

public:
    uint256 hashFork;
    uint256 hashParent;
    uint256 hashJoint;
    int nJointHeight;
};

typedef boost::multi_index_container<
    CForkLink,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::member<CForkLink, uint256, &CForkLink::hashFork>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CForkLink, uint256, &CForkLink::hashJoint>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CForkLink, int, &CForkLink::nJointHeight>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CForkLink, uint256, &CForkLink::hashParent>>>>
    CForkLinkSet;
typedef CForkLinkSet::nth_index<0>::type CForkLinkSetByFork;
typedef CForkLinkSet::nth_index<1>::type CForkLinkSetByJoint;
typedef CForkLinkSet::nth_index<2>::type CForkLinkSetByHeight;
typedef CForkLinkSet::nth_index<3>::type CForkLinkSetByParent;

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
    void RemoveJoint(const uint256& hashJoint, std::vector<uint256>& vFork)
    {
        std::multimap<uint256, uint256>::iterator it = mapJoint.lower_bound(hashJoint);
        while (it != mapJoint.upper_bound(hashJoint))
        {
            vFork.push_back((*it).second);
            mapJoint.erase(it++);
        }
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
    bool GetJoint(const uint256& hashFork, uint256& hashParent, uint256& hashJoint, int& nHeight) const override;
    bool LoadForkContext(std::vector<uint256>& vActive) override;
    void ForkUpdate(const CBlockChainUpdate& update, std::vector<uint256>& vActive, std::vector<uint256>& vDeactive) override;
    bool AddNewForkContext(const CForkContext& ctxt, std::vector<uint256>& vActive);
    void GetForkList(std::vector<uint256>& vFork) const override;
    bool GetSubline(const uint256& hashFork, std::vector<std::pair<int, uint256>>& vSubline) const override;
    bool SetForkFilter(const std::vector<uint256>& vFork = std::vector<uint256>(), const std::vector<uint256>& vGroup = std::vector<uint256>()) override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;
    bool IsAllowedFork(const uint256& hashFork, const uint256& hashParent) const;

protected:
    mutable boost::shared_mutex rwAccess;
    ICoreProtocol* pCoreProtocol;
    IBlockChain* pBlockChain;
    bool fAllowAnyFork;
    std::set<uint256> setForkAllowed;
    std::set<uint256> setGroupAllowed;
    std::map<uint256, CForkSchedule> mapForkSched;
    CForkLinkSet setForkIndex;
};

} // namespace bigbang

#endif //BIGBANG_FORKMANAGER_H
