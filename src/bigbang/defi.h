// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_DEFI_H
#define BIGBANG_DEFI_H

#include <map>
#include <set>
#include <stack>
#include <type_traits>

#include "address.h"
#include "addressdb.h"
#include "profile.h"
#include "struct.h"
#include "xengine.h"

namespace bigbang
{

class CDeFiRelationNode
{
public:
    CDeFiRelationNode()
      : pParent(nullptr), nPower(0), nAmount(0) {}
    CDeFiRelationNode(const CDestination& destIn, const CDestination& parentIn, const uint256& txidIn)
      : dest(destIn), parent(parentIn), txid(txidIn), pParent(nullptr), nPower(0), nAmount(0) {}

public:
    CDestination dest;
    CDestination parent;
    uint256 txid;
    CDeFiRelationNode* pParent;
    std::set<CDeFiRelationNode*> setSubline;
    int64 nPower;
    int64 nAmount;
};

class CDeFiRelationGraph
{
public:
    CDeFiRelationGraph() {}
    ~CDeFiRelationGraph();

    bool ConstructRelationGraph(const std::map<CDestination, storage::CAddrInfo>& mapAddress);

    // postorder traversal
    // walker: bool (*function)(CDeFiRelationNode*)
    template <typename NodeWalker>
    bool PostorderTraversal(NodeWalker walker)
    {
        for (auto& dest : setRoot)
        {
            CDeFiRelationNode* pNode = mapDestNode[dest];
            if (pNode == nullptr)
            {
                xengine::StdError("CDeFiRelationGraph", "PostorderTraversal no root address, dest: %s", CAddress(dest).ToString().c_str());
                return false;
            }

            // postorder traversal
            std::stack<CDeFiRelationNode*> st;
            do
            {
                // if pNode != nullptr push and down, or pop and up.
                if (pNode != nullptr)
                {
                    if (!pNode->setSubline.empty())
                    {
                        st.push(pNode);
                        pNode = *pNode->setSubline.begin();
                        continue;
                    }
                }
                else
                {
                    pNode = st.top();
                    st.pop();
                }

                // call walker
                if (!walker(pNode))
                {
                    return false;
                }

                // root or the last child of parent. fetch from stack when next loop
                if (pNode->pParent == nullptr || pNode == *pNode->pParent->setSubline.rbegin())
                {
                    pNode = nullptr;
                }
                else
                {
                    auto it = pNode->pParent->setSubline.find(pNode);
                    if (it == pNode->pParent->setSubline.end())
                    {
                        xengine::StdError("CDeFiRelationGraph", "PostorderTraversal parent have not child, parent: %s, child: %s", CAddress(pNode->pParent->dest).ToString().c_str(), CAddress(pNode->dest).ToString().c_str());
                        return false;
                    }
                    else
                    {
                        pNode = *++it;
                    }
                }
            } while (!st.empty());
        }
        return true;
    }

protected:
    bool UpdateAddress(const CDestination& dest, const CDestination& parent, const uint256& txid);
    bool UpdateParent();

public:
    std::map<CDestination, CDeFiRelationNode*> mapDestNode;
    std::set<CDestination> setRoot;
};

class CDeFiForkReward
{
public:
    enum
    {
        MAX_REWARD_CACHE = 20
    };

    typedef std::map<uint256, CDeFiRewardSet> MapSectionReward;
    struct CForkReward
    {
        MapSectionReward reward;
        CProfile profile;
    };
    typedef std::map<uint256, CForkReward> MapForkReward;

    // return exist fork or not
    bool ExistFork(const uint256& forkid) const;
    // add a fork and its profile
    void AddFork(const uint256& forkid, const CProfile& profile);
    // get a fork profile
    CProfile GetForkProfile(const uint256& forkid);
    // return the last height of previous reward cycle of nHeight
    int32 PrevRewardHeight(const uint256& forkid, const int32 nHeight);
    // return the total reward from the beginning of section to the height of hash
    int64 GetSectionReward(const uint256& forkid, const uint256& hash);
    // return the coinbase of nHeight and the next different coinbase beginning height
    bool GetDecayCoinbase(const CProfile& profile, const int32 nHeight, int64& nCoinbase, int32& nNextHeight);
    // return exist section cache of fork or not
    bool ExistForkSection(const uint256& forkid, const uint256& section);
    // return the section reward set. Should use ExistForkSection to determine if it exists first.
    const CDeFiRewardSet& GetForkSection(const uint256& forkid, const uint256& section);
    // Add a section reward set of fork
    void AddForkSection(const uint256& forkid, const uint256& hash, CDeFiRewardSet&& reward);

    // compute stake reward
    std::map<CDestination, int64> ComputeStakeReward(const uint256& hash, const int64 nMin, const int64 nReward,
                                                     const std::map<CDestination, int64>& mapAddressAmount);
    // compute promotion reward
    std::map<CDestination, int64> ComputePromotionReward(const uint256& hash, const int64 nReward,
                                                         const std::map<CDestination, int64>& mapAddressAmount,
                                                         const std::map<int64, uint32>& mapPromotionTokenTimes,
                                                         CDeFiRelationGraph& relation);

protected:
    MapForkReward forkReward;
    static CDeFiRewardSet null;
};

} // namespace bigbang

#endif // BIGBANG_DEFI_H