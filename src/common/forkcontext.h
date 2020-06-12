// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_FORKCONTEXT_H
#define COMMON_FORKCONTEXT_H

#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <map>
#include <stream/stream.h>
#include <string>
#include <vector>

#include "destination.h"
#include "profile.h"
#include "uint256.h"

class CForkContext
{
    friend class xengine::CStream;

public:
    std::string strName;
    std::string strSymbol;
    uint256 hashFork;
    uint256 hashParent;
    uint256 hashJoint;
    uint256 txidEmbedded;
    uint16 nVersion;
    uint16 nFlag;
    int64 nAmount;
    int64 nMintReward;
    int64 nMinTxFee;
    uint32 nHalveCycle;
    int nJointHeight;
    CDestination destOwner;

public:
    enum
    {
        FORK_FLAG_ISOLATED = (1 << 0),
        FORK_FLAG_PRIVATE = (1 << 1),
        FORK_FLAG_ENCLOSED = (1 << 2),
    };
    CForkContext()
    {
        SetNull();
    }
    CForkContext(const uint256& hashForkIn, const uint256& hashJointIn, const uint256& txidEmbeddedIn,
                 const CProfile& profile)
    {
        hashFork = hashForkIn;
        hashJoint = hashJointIn;
        txidEmbedded = txidEmbeddedIn;
        strName = profile.strName;
        strSymbol = profile.strSymbol;
        hashParent = profile.hashParent;
        nVersion = profile.nVersion;
        nFlag = profile.nFlag;
        nAmount = profile.nAmount;
        nMintReward = profile.nMintReward;
        nMinTxFee = profile.nMinTxFee;
        nHalveCycle = profile.nHalveCycle;
        destOwner = profile.destOwner;
        nJointHeight = profile.nJointHeight;
    }
    virtual ~CForkContext() = default;
    virtual void SetNull()
    {
        hashFork = 0;
        hashParent = 0;
        hashJoint = 0;
        txidEmbedded = 0;
        nVersion = 1;
        nFlag = 0;
        nAmount = 0;
        nMintReward = 0;
        nMinTxFee = 0;
        nHalveCycle = 0;
        nJointHeight = -1;
        strName.clear();
        strSymbol.clear();
        destOwner.SetNull();
    }
    bool IsNull() const
    {
        return strName.empty();
    }
    bool IsIsolated() const
    {
        return (nFlag & FORK_FLAG_ISOLATED);
    }
    bool IsPrivate() const
    {
        return (nFlag & FORK_FLAG_PRIVATE);
    }
    bool IsEnclosed() const
    {
        return (nFlag & FORK_FLAG_ENCLOSED);
    }
    void SetFlag(uint16 flag)
    {
        nFlag |= flag;
    }
    void ResetFlag(uint16 flag)
    {
        nFlag &= ~flag;
    }
    void SetFlag(bool fIsolated, bool fPrivate, bool fEnclosed)
    {
        nFlag = 0;
        nFlag |= (fIsolated ? FORK_FLAG_ISOLATED : 0);
        nFlag |= (fPrivate ? FORK_FLAG_PRIVATE : 0);
        nFlag |= (fEnclosed ? FORK_FLAG_ENCLOSED : 0);
    }
    const CProfile GetProfile() const
    {
        CProfile profile;
        profile.strName = strName;
        profile.strSymbol = strSymbol;
        profile.hashParent = hashParent;
        profile.nVersion = nVersion;
        profile.nFlag = nFlag;
        profile.nAmount = nAmount;
        profile.nMintReward = nMintReward;
        profile.nMinTxFee = nMinTxFee;
        profile.nHalveCycle = nHalveCycle;
        profile.destOwner = destOwner;
        profile.nJointHeight = nJointHeight;
        return profile;
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(strName, opt);
        s.Serialize(strSymbol, opt);
        s.Serialize(hashFork, opt);
        s.Serialize(hashParent, opt);
        s.Serialize(hashJoint, opt);
        s.Serialize(txidEmbedded, opt);
        s.Serialize(nVersion, opt);
        s.Serialize(nFlag, opt);
        s.Serialize(nAmount, opt);
        s.Serialize(nMintReward, opt);
        s.Serialize(nMinTxFee, opt);
        s.Serialize(nHalveCycle, opt);
        s.Serialize(nJointHeight, opt);
        s.Serialize(destOwner, opt);
    }
};

class CForkContextEx : public CForkContext
{
    friend class xengine::CStream;

public:
    CForkContextEx()
        : nCreatedHeight(-1), fActive(false)
    {
    }
    CForkContextEx(const CForkContext& ctxtIn, const int nCreatedHeightIn = -1, const bool fActiveIn = true)
      : CForkContext(ctxtIn), nCreatedHeight(nCreatedHeightIn), fActive(fActiveIn)
    {
    }

    bool IsActive() const
    {
        return fActive;
    }

    bool operator<(const CForkContextEx& ctxt) const
    {
        return hashFork < ctxt.hashFork;
    }

public:
    int32 nCreatedHeight;
    bool fActive;

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        CForkContext::Serialize(s, opt);
        s.Serialize(nCreatedHeight, opt);
        s.Serialize(fActive, opt);
    }
};

typedef boost::multi_index_container<
    CForkContextEx,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::member<CForkContext, uint256, &CForkContext::hashFork>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CForkContext, uint256, &CForkContext::hashJoint>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CForkContext, int, &CForkContext::nJointHeight>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CForkContext, uint256, &CForkContext::hashParent>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CForkContext, std::string, &CForkContext::strName>>,
        boost::multi_index::ordered_unique<boost::multi_index::member<CForkContext, uint256, &CForkContext::txidEmbedded>>>>
    CForkContextExSet;
typedef CForkContextExSet::nth_index<0>::type CForkContextExSetByFork;
typedef CForkContextExSet::nth_index<1>::type CForkContextExSetByJoint;
typedef CForkContextExSet::nth_index<2>::type CForkContextExSetByHeight;
typedef CForkContextExSet::nth_index<3>::type CForkContextExSetByParent;
typedef CForkContextExSet::nth_index<4>::type CForkContextExSetByName;
typedef CForkContextExSet::nth_index<5>::type CForkContextExSetByTx;

class CForkSetManager
{
public:
    CForkSetManager()
    {
    }
    ~CForkSetManager()
    {
    }
    CForkSetManager(const CForkSetManager& mgr)
    {
        boost::shared_lock<boost::shared_mutex> rlock(mgr.rwAccess);
        forkSet = mgr.forkSet;
    }
    CForkSetManager(CForkSetManager&& mgr)
    {
        boost::unique_lock<boost::shared_mutex> wlock(mgr.rwAccess);
        forkSet = std::move(mgr.forkSet);
    }
    CForkSetManager& operator=(const CForkSetManager& mgr)
    {
        boost::shared_lock<boost::shared_mutex> rlock(mgr.rwAccess, boost::defer_lock);
        boost::unique_lock<boost::shared_mutex> wlock(rwAccess, boost::defer_lock);
        boost::lock(rlock, wlock);
        forkSet = mgr.forkSet;
        return *this;
    }
    CForkSetManager& operator=(CForkSetManager&& mgr)
    {
        boost::unique_lock<boost::shared_mutex> wlock1(mgr.rwAccess, boost::defer_lock);
        boost::unique_lock<boost::shared_mutex> wlock2(rwAccess, boost::defer_lock);
        boost::lock(wlock1, wlock2);
        forkSet = std::move(mgr.forkSet);
        return *this;
    }

    void Clear()
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
        forkSet.clear();
    }
    
    void Insert(const CForkContextEx& ctxt)
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
        CForkContextExSetByFork& idxFork = forkSet.get<0>();
        CForkContextExSetByFork::iterator it = idxFork.find(ctxt.hashFork);
        if (it != idxFork.end())
        {
            if (it->txidEmbedded != ctxt.txidEmbedded)
            {
                idxFork.erase(it);
                forkSet.insert(ctxt);
            }
            else
            {
                idxFork.replace(it, ctxt);
            }
        }
        else
        {
            forkSet.insert(ctxt);
        }
    }

    void EraseByFork(const uint256& hashFork)
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
        CForkContextExSetByFork& idxFork = forkSet.get<0>();
        idxFork.erase(hashFork);
    }

    void ChangeActive(const uint256& hashFork, bool fActive)
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
        CForkContextExSetByFork& idxFork = forkSet.get<0>();
        CForkContextExSetByFork::iterator it = idxFork.find(hashFork);
        if (it != idxFork.end())
        {
            if (it->fActive ^ fActive)
            {
                CForkContextEx ctxt = *it;
                ctxt.fActive = fActive;
                idxFork.replace(it, ctxt);
            }
        }
    }

    void ChangeCreatedHeight(const uint256& hashFork, const int32 nCreatedHeight)
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
        CForkContextExSetByFork& idxFork = forkSet.get<0>();
        CForkContextExSetByFork::iterator it = idxFork.find(hashFork);
        if (it != idxFork.end())
        {
            if (it->nCreatedHeight != nCreatedHeight)
            {
                CForkContextEx ctxt = *it;
                ctxt.nCreatedHeight = nCreatedHeight;
                idxFork.replace(it, ctxt);
            }
        }
    }

    bool Exist(const uint256& hashFork) const
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
        const CForkContextExSetByFork& idxFork = forkSet.get<0>();
        const CForkContextExSetByFork::iterator it = idxFork.find(hashFork);
        return it != idxFork.end();
    }

    bool RetrieveByFork(const uint256& hashFork, CForkContextEx& ctxt) const
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
        const CForkContextExSetByFork& idxFork = forkSet.get<0>();
        const CForkContextExSetByFork::iterator it = idxFork.find(hashFork);
        if (it != idxFork.end())
        {
            ctxt = *it;
            return true;
        }
        return false;
    }

    bool RetrieveByJoint(const uint256& hashFork, std::set<CForkContextEx>& setCtxt) const
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
        const CForkContextExSetByJoint& idxJoint = forkSet.get<1>();
        CForkContextExSetByJoint::const_iterator itBegin = idxJoint.lower_bound(hashFork);
        CForkContextExSetByJoint::const_iterator itEnd = idxJoint.upper_bound(hashFork);
        if (itBegin == itEnd)
        {
            return false;
        }
        for (; itBegin != itEnd; ++itBegin)
        {
            setCtxt.insert(*itBegin);
        }
        return true;
    }

    bool RetrieveByHeight(const int nHeight, std::set<CForkContextEx>& setCtxt) const
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
        const CForkContextExSetByHeight& idxHeight = forkSet.get<2>();
        CForkContextExSetByHeight::const_iterator itBegin = idxHeight.lower_bound(nHeight);
        CForkContextExSetByHeight::const_iterator itEnd = idxHeight.upper_bound(nHeight);
        if (itBegin == itEnd)
        {
            return false;
        }
        for (; itBegin != itEnd; ++itBegin)
        {
            setCtxt.insert(*itBegin);
        }
        return true;
    }

    bool RetrieveByParent(const uint256& hashFork, std::set<CForkContextEx>& setCtxt) const
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
        const CForkContextExSetByParent& idxParent = forkSet.get<3>();
        CForkContextExSetByParent::const_iterator itBegin = idxParent.lower_bound(hashFork);
        CForkContextExSetByParent::const_iterator itEnd = idxParent.upper_bound(hashFork);
        if (itBegin == itEnd)
        {
            return false;
        }
        for (; itBegin != itEnd; ++itBegin)
        {
            setCtxt.insert(*itBegin);
        }
        return true;
    }

    bool RetrieveByName(const std::string& strName, std::set<CForkContextEx>& setCtxt) const
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
        const CForkContextExSetByName& idxName = forkSet.get<4>();
        CForkContextExSetByName::iterator itBegin = idxName.lower_bound(strName);
        CForkContextExSetByName::iterator itEnd = idxName.upper_bound(strName);
        if (itBegin == itEnd)
        {
            return false;
        }
        for (; itBegin != itEnd; ++itBegin)
        {
            setCtxt.insert(*itBegin);
        }
        return true;
    }

    bool RetrieveByTx(const uint256& txid, CForkContextEx& ctxt) const
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
        const CForkContextExSetByTx& idxTx = forkSet.get<5>();
        const CForkContextExSetByTx::iterator it = idxTx.find(txid);
        if (it != idxTx.end())
        {
            ctxt = *it;
            return true;
        }
        return false;
    }
    
    // fork hash list of all fork, compared by hash(also by origin height)
    void GetForkList(std::vector<uint256>& vFork) const
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
        const CForkContextExSetByFork& idxFork = forkSet.get<0>();
        vFork.reserve(idxFork.size());
        for (auto it = idxFork.begin(); it != idxFork.end(); it++)
        {
            vFork.push_back(it->hashFork);
        }
    }

    // CForkContextEx list of all fork, compared by hash(also by origin height)
    void GetForkContextExList(std::vector<CForkContextEx>& vFork) const
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
        const CForkContextExSetByFork& idxFork = forkSet.get<0>();
        vFork.reserve(idxFork.size());
        for (auto it = idxFork.begin(); it != idxFork.end(); it++)
        {
            vFork.push_back(*it);
        }
    }

    bool GetSubline(const uint256& hashFork, std::map<uint256, int32>& mapSubline) const
    {
        std::set<CForkContextEx> setCtxt;
        if (RetrieveByParent(hashFork, setCtxt))
        {
            for (auto& ctxt : setCtxt)
            {
                mapSubline.insert(std::make_pair(ctxt.hashFork, ctxt.nJointHeight));
            }
            return true;
        }
        return false;
    }

    bool GetDescendant(const uint256& hashFork, std::vector<std::pair<uint256, uint256>>& vDescendant) const
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwAccess);

        CForkContextEx ctxt;
        if (!RetrieveByFork(hashFork, ctxt))
        {
            return false;
        }

        vDescendant.clear();
        vDescendant.reserve(forkSet.size());
        vDescendant.push_back(std::make_pair(hashFork, ctxt.txidEmbedded));

        const CForkContextExSetByParent& idxParent = forkSet.get<3>();
        for (size_t i = 0; i < vDescendant.size(); i++)
        {
            const uint256& hash = vDescendant[i].first;
            CForkContextExSetByParent::const_iterator itBegin = idxParent.lower_bound(hash);
            CForkContextExSetByParent::const_iterator itEnd = idxParent.upper_bound(hash);
            for (; itBegin != itEnd; ++itBegin)
            {
                vDescendant.push_back(std::make_pair(itBegin->hashFork, itBegin->txidEmbedded));
            }
        }

        return true;
    }

protected:
    mutable boost::shared_mutex rwAccess;
    CForkContextExSet forkSet;
};

#endif //COMMON_FORKCONTEXT_H
