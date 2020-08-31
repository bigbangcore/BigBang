// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "defi.h"

#include "param.h"

using namespace std;
using namespace xengine;
using namespace bigbang::storage;

namespace bigbang
{

//////////////////////////////
// CDeFiRelationGraph

CDeFiRelationGraph::~CDeFiRelationGraph()
{
    for (auto it = mapDestNode.begin(); it != mapDestNode.end(); ++it)
    {
        if (it->second)
        {
            delete it->second;
            it->second = nullptr;
        }
    }
}

bool CDeFiRelationGraph::ConstructRelationGraph(const std::map<CDestination, CAddrInfo>& mapAddress)
{
    for (const auto& vd : mapAddress)
    {
        if (!UpdateAddress(vd.first, vd.second.destParent, vd.second.txid))
        {
            StdLog("CDeFiRelationGraph", "ConstructRelationGraph: UpdateAddress fail");
            return false;
        }
    }

    if (!UpdateParent())
    {
        StdLog("CDeFiRelationGraph", "ConstructRelationGraph: UpdateParent fail");
        return false;
    }

    return true;
}

bool CDeFiRelationGraph::UpdateAddress(const CDestination& dest, const CDestination& parent, const uint256& txid)
{
    if (dest.IsNull() || parent.IsNull() || txid == 0)
    {
        StdError("CDeFiRelationGraph", "UpdateAddress: param error");
        return false;
    }
    auto it = mapDestNode.find(dest);
    if (it == mapDestNode.end())
    {
        CDeFiRelationNode* pNewAddr = new CDeFiRelationNode(dest, parent, txid);
        if (pNewAddr == nullptr)
        {
            StdError("CDeFiRelationGraph", "UpdateAddress: new error, dest: %s", CAddress(dest).ToString().c_str());
            return false;
        }
        mapDestNode.insert(make_pair(dest, pNewAddr));
    }
    else
    {
        StdError("CDeFiRelationGraph", "UpdateAddress: duplicate address, dest: %s", CAddress(dest).ToString().c_str());
        return false;
    }
    return true;
}

bool CDeFiRelationGraph::UpdateParent()
{
    for (auto it = mapDestNode.begin(); it != mapDestNode.end(); ++it)
    {
        if (!it->second)
        {
            StdError("CDeFiRelationGraph", "UpdateParent: address is null, dest: %s", CAddress(it->first).ToString().c_str());
            return false;
        }
        if (it->second->parent.IsNull())
        {
            continue;
        }
        auto mt = mapDestNode.find(it->second->parent);
        if (mt != mapDestNode.end())
        {
            if (!mt->second)
            {
                StdError("CDeFiRelationGraph", "UpdateParent: parent address is null, dest: %s", CAddress(it->first).ToString().c_str());
                return false;
            }
        }
        else
        {
            CDeFiRelationNode* pNewAddr = new CDeFiRelationNode(it->second->parent, CDestination(), uint256());
            if (pNewAddr == nullptr)
            {
                StdError("CDeFiRelationGraph", "UpdateParent: new error, dest: %s", CAddress(it->second->parent).ToString().c_str());
                return false;
            }
            mt = mapDestNode.insert(make_pair(it->second->parent, pNewAddr)).first;
            setRoot.insert(it->second->parent);
        }
        it->second->pParent = mt->second;
        mt->second->setSubline.insert(it->second);
    }
    return true;
}

//////////////////////////////
// CDeFiForkReward
CDeFiRewardSet CDeFiForkReward::null;

bool CDeFiForkReward::ExistFork(const uint256& forkid) const
{
    return forkReward.count(forkid);
}

void CDeFiForkReward::AddFork(const uint256& forkid, const CProfile& profile)
{
    CForkReward fr;
    fr.profile = profile;
    forkReward.insert(std::make_pair(forkid, fr));
}

CProfile CDeFiForkReward::GetForkProfile(const uint256& forkid)
{
    auto it = forkReward.find(forkid);
    return (it == forkReward.end()) ? CProfile() : it->second.profile;
}

int32 CDeFiForkReward::PrevRewardHeight(const uint256& forkid, const int32 nHeight)
{
    auto it = forkReward.find(forkid);
    if (it != forkReward.end())
    {
        CProfile& profile = it->second.profile;
        if (!profile.defi.IsNull())
        {
            int32 nJointHeight = profile.nJointHeight;
            int32 nRewardCycle = profile.defi.nRewardCycle;
            if (nHeight > nJointHeight + 1 && nRewardCycle > 0)
            {
                StdDebug("PrevRewardHeight", "SHT PrevRewardHeight nHeight: %d, nJoint: %u, nRewardCycle: %u, prev: %d",
                         nHeight, nJointHeight, nRewardCycle, ((nHeight - nJointHeight - 2) / nRewardCycle) * nRewardCycle + nJointHeight + 1);
                return ((nHeight - nJointHeight - 2) / nRewardCycle) * nRewardCycle + nJointHeight + 1;
            }
        }
    }
    return -1;
}

int64 CDeFiForkReward::GetSectionReward(const uint256& forkid, const uint256& hash)
{
    double nReward = 0;
    CProfile profile = GetForkProfile(forkid);
    if (profile.IsNull())
    {
        // StdDebug("CDeFiForkReward", "SHT GetSectionReward profile is null, forkid: %s", forkid.ToString().c_str());
        return (int64)nReward;
    }

    int32 nEndHeight = CBlock::GetBlockHeightByHash(hash) + 1;
    int32 nBeginHeight = PrevRewardHeight(forkid, CBlock::GetBlockHeightByHash(hash)) + 1;

    // the next block of origin
    if (nBeginHeight <= profile.nJointHeight + 1)
    {
        nBeginHeight = profile.nJointHeight + 2;
    }
    // StdDebug("CDeFiForkReward", "SHT GetSectionReward nBeginHeight: %d, nEndHeight: %d", nBeginHeight, nEndHeight);

    double nCoinbase = 0;
    int32 nNextHeight = 0;
    while (nBeginHeight < nEndHeight)
    {
        if (GetDecayCoinbase(profile, nBeginHeight, nCoinbase, nNextHeight))
        {
            // StdDebug("CDeFiForkReward", "SHT GetSectionReward GetDecayCoinbase nBeinHeight: %d, nCoinbase: %ld, nNexHeight: %d", nBeginHeight, nCoinbase, nNextHeight);
            uint32 nHeight = min(nEndHeight, nNextHeight) - nBeginHeight;
            nReward += nCoinbase * nHeight;
            nBeginHeight += nHeight;
        }
        else
        {
            StdError("CDeFiForkReward", "SHT GetSectionReward GetDecayCoinbase fail");
        }
    }

    return (int64)nReward;
}

bool CDeFiForkReward::ExistForkSection(const uint256& forkid, const uint256& section)
{
    auto it = forkReward.find(forkid);
    if (it != forkReward.end())
    {
        return it->second.reward.count(section);
    }
    return false;
}

const CDeFiRewardSet& CDeFiForkReward::GetForkSection(const uint256& forkid, const uint256& section)
{
    auto it = forkReward.find(forkid);
    if (it != forkReward.end())
    {
        auto im = it->second.reward.find(section);
        if (im != it->second.reward.end())
        {
            return im->second;
        }
    }
    return null;
}

void CDeFiForkReward::AddForkSection(const uint256& forkid, const uint256& hash, CDeFiRewardSet&& reward)
{
    auto it = forkReward.find(forkid);
    if (it != forkReward.end())
    {
        it->second.reward[hash] = std::move(reward);
    }
    while (forkReward.size() > MAX_REWARD_CACHE)
    {
        if (forkReward.begin()->first != hash)
        {
            forkReward.erase(forkReward.begin());
        }
        else
        {
            break;
        }
    }
}

bool CDeFiForkReward::GetDecayCoinbase(const CProfile& profile, const int32 nHeight, double& nCoinbase, int32& nNextHeight)
{
    if (nHeight <= profile.nJointHeight + 1)
    {
        // StdDebug("CDeFiForkReward", "SHT GetDecayCoinbase nHeight <= profile.nJointHeight + 1, nHeight: %d, nJointHeight: %d", nHeight, profile.nJointHeight);
        return false;
    }

    int32 nJointHeight = profile.nJointHeight;
    int32 nDecayCycle = profile.defi.nDecayCycle;
    int32 nSupplyCycle = profile.defi.nSupplyCycle;
    uint8 nCoinbaseDecayPercent = profile.defi.nCoinbaseDecayPercent;
    uint32 nInitCoinbasePercent = profile.defi.nInitCoinbasePercent;
    // StdDebug("CDeFiForkReward", "SHT GetDecayCoinbase nJointHeight: %u, nDecayCycle: %u, nSupplyCycle: %u, nCoinbaseDecayPercent: %u, nInitCoinbasePercent: %u",
    //     nJointHeight, nDecayCycle, nSupplyCycle, (uint32)nCoinbaseDecayPercent, nInitCoinbasePercent);
    int32 nSupplyCount = (nDecayCycle <= 0) ? 0 : (nDecayCycle / nSupplyCycle);
    // StdDebug("CDeFiForkReward", "SHT GetDecayCoinbase nSupplyCount: %d", nSupplyCount);

    // for example:
    // [2] nJoint height
    // [3] origin height
    // [4, 5, 6] the first supply cycle of the first decay cycle
    // [7, 8, 9] the second sypply cycle of the first decay cycle
    // [10, 11, 12] the first supply cycle of the second decay cycle
    // [13, 14, 15] the second supply cycle of the second decay cycle
    int32 nDecayCount = (nDecayCycle <= 0) ? 0 : ((nHeight - nJointHeight - 2) / nDecayCycle);
    // StdDebug("CDeFiForkReward", "SHT GetDecayCoinbase nDecayCount: %d", nDecayCount);
    int32 nDecayHeight = nDecayCount * nDecayCycle + nJointHeight + 2;
    // StdDebug("CDeFiForkReward", "SHT GetDecayCoinbase nDecayHeight: %d", nDecayHeight);
    int32 nCurSupplyCount = (nHeight - nDecayHeight) / nSupplyCycle;
    // StdDebug("CDeFiForkReward", "SHT GetDecayCoinbase nCurSupplyCount: %d", nCurSupplyCount);

    // supply = init * (1 + nInitCoinbasePercent) ^ nSupplyCount * (1 + nInitCoinbasePercent * nCoinbaseDecayPercent) ^ nCurSupplyCount
    int64 nSupply = profile.nAmount;
    // StdDebug("CDeFiForkReward", "SHT GetDecayCoinbase nSupply: %ld", nSupply);
    double fCoinbaseIncreasing = (double)nInitCoinbasePercent / 100;
    // StdDebug("CDeFiForkReward", "SHT GetDecayCoinbase fCoinbaseIncreasing: %f", fCoinbaseIncreasing);
    for (int i = 0; i <= nDecayCount; i++)
    {
        if (i < nDecayCount)
        {
            nSupply *= pow(1 + fCoinbaseIncreasing, nSupplyCount);
            // StdDebug("CDeFiForkReward", "SHT GetDecayCoinbase i: %d, nSupplyCount: %d, supply: %ld", i, nSupplyCount, nSupply);
            fCoinbaseIncreasing = fCoinbaseIncreasing * nCoinbaseDecayPercent / 100;
            // StdDebug("CDeFiForkReward", "SHT GetDecayCoinbase i: %d, fCoinbaseIncreasing: %f", i, fCoinbaseIncreasing);
        }
        else
        {
            nSupply *= pow(1 + fCoinbaseIncreasing, nCurSupplyCount);
            // StdDebug("CDeFiForkReward", "SHT GetDecayCoinbase i: %d, nCurSupplyCount: %d, supply: %ld", i, nCurSupplyCount, nSupply);
        }
    }

    nCoinbase = nSupply * fCoinbaseIncreasing / nSupplyCycle;
    // StdDebug("CDeFiForkReward", "SHT GetDecayCoinbase nCoinbase: %ld", nCoinbase);
    nNextHeight = (nCurSupplyCount + 1) * nSupplyCycle + nDecayHeight;
    // StdDebug("CDeFiForkReward", "SHT GetDecayCoinbase nNextHeight: %d", nNextHeight);
    return true;
}

map<CDestination, int64> CDeFiForkReward::ComputeStakeReward(const int64 nMin, const int64 nReward,
                                                             const std::map<CDestination, int64>& mapAddressAmount)
{
    map<CDestination, int64> reward;

    if (nReward == 0)
    {
        return reward;
    }

    // sort by token
    multimap<int64, pair<CDestination, uint32>> mapRank;
    for (auto& p : mapAddressAmount)
    {
        if (p.second >= nMin)
        {
            mapRank.insert(make_pair(p.second, make_pair(p.first, 0)));
        }
    }

    // tag rank
    uint32 nRank = 1;
    uint32 nPos = 0;
    uint32 nTotal = 0;
    int64 nToken = -1;
    for (auto& p : mapRank)
    {
        ++nPos;
        if (p.first != nToken)
        {
            p.second.second = nPos;
            nRank = nPos;
            nToken = p.first;
        }
        else
        {
            p.second.second = nRank;
        }

        nTotal += p.second.second;
    }

    // reward
    double nUnitReward = (double)nReward / nTotal;
    for (auto& p : mapRank)
    {
        reward.insert(make_pair(p.second.first, (int64)(nUnitReward * p.second.second)));
    }

    return reward;
}

map<CDestination, int64> CDeFiForkReward::ComputePromotionReward(const int64 nReward,
                                                                 const map<CDestination, int64>& mapAddressAmount,
                                                                 const std::map<int64, uint32>& mapPromotionTokenTimes,
                                                                 CDeFiRelationGraph& relation)
{
    map<CDestination, int64> reward;

    if (nReward == 0)
    {
        return reward;
    }

    // compute promotion power
    int64 nTotal = 0;
    relation.PostorderTraversal([&](CDeFiRelationNode* pNode) {
        // amount
        auto it = mapAddressAmount.find(pNode->dest);
        pNode->nAmount = (it == mapAddressAmount.end()) ? 0 : (it->second / COIN);

        // power
        pNode->nPower = 0;
        if (!pNode->setSubline.empty())
        {
            StdDebug("CDeFiForkReward", "SHT ComputePromotionReward dest: %s, amount: %ld", CAddress(pNode->dest).ToString().c_str(), pNode->nAmount);
            int64 nMax = -1;
            for (auto& p : pNode->setSubline)
            {
                StdDebug("CDeFiForkReward", "SHT ComputePromotionReward dest: %s, child: %s, amount: %ld", CAddress(pNode->dest).ToString().c_str(), CAddress(p->dest).ToString().c_str(), p->nAmount);
                pNode->nAmount += p->nAmount;
                int64 n = 0;
                if (p->nAmount <= nMax)
                {
                    n = p->nAmount;
                }
                else
                {
                    n = nMax;
                    nMax = p->nAmount;
                }

                if (n < 0)
                {
                    continue;
                }

                int64 nLastToken = 0;
                int64 nChildPower = 0;
                for (auto& tokenTimes : mapPromotionTokenTimes)
                {
                    StdDebug("CDeFiForkReward", "n: %ld, token: %lu, times: %u", n, tokenTimes.first, tokenTimes.second);
                    if (n > tokenTimes.first)
                    {
                        nChildPower += (tokenTimes.first - nLastToken) * tokenTimes.second;
                        nLastToken = tokenTimes.first;
                        StdDebug("CDeFiForkReward", "1 nChildPower: %ld", nChildPower);
                        StdDebug("CDeFiForkReward", "1 nLastToken: %lu", nLastToken);
                    }
                    else
                    {
                        nChildPower += (n - nLastToken) * tokenTimes.second;
                        nLastToken = n;
                        StdDebug("CDeFiForkReward", "2 nChildPower: %ld", nChildPower);
                        StdDebug("CDeFiForkReward", "2 nLastToken: %lu", nLastToken);
                        break;
                    }
                }
                StdDebug("CDeFiForkReward", "3 nChildPower: %ld", nChildPower);
                nChildPower += (n - nLastToken);
                StdDebug("CDeFiForkReward", "SHT ComputePromotionReward dest: %s, child power: %ld", CAddress(pNode->dest).ToString().c_str(), nChildPower);
                pNode->nPower += nChildPower;
            }
            StdDebug("CDeFiForkReward", "SHT ComputePromotionReward dest: %s, max: %ld, power: %ld", CAddress(pNode->dest).ToString().c_str(), nMax, llround(pow(nMax, 1.0 / 3)));
            pNode->nPower += llround(pow(nMax, 1.0 / 3));
        }
        StdDebug("CDeFiForkReward", "SHT ComputePromotionReward dest: %s power: %ld", CAddress(pNode->dest).ToString().c_str(), pNode->nPower);

        if (pNode->nPower > 0)
        {
            nTotal += pNode->nPower;
            reward.insert(make_pair(pNode->dest, pNode->nPower));
        }

        return true;
    });

    StdDebug("CDeFiForkReward", "SHT ComputePromotionReward total power: %ld", nTotal);
    // reward
    if (nTotal > 0)
    {
        double nUnitReward = (double)nReward / nTotal;
        for (auto& p : reward)
        {
            p.second *= nUnitReward;
            StdDebug("CDeFiForkReward", "SHT ComputePromotionReward dest: %s reward: %ld", CAddress(p.first).ToString().c_str(), p.second);
        }
    }

    return reward;
}

} // namespace bigbang