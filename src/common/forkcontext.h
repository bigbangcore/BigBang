// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_FORKCONTEXT_H
#define COMMON_FORKCONTEXT_H

#include <stream/stream.h>
#include <string>

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

#endif //COMMON_FORKCONTEXT_H
