// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "delegate.h"

using namespace std;
using namespace xengine;

namespace bigbang
{
namespace delegate
{

//////////////////////////////
// CDelegate

CDelegate::CDelegate()
{
}

CDelegate::~CDelegate()
{
}

bool CDelegate::Initialize()
{
    return true;
}

void CDelegate::Deinitialize()
{
    mapVote.clear();
}

void CDelegate::AddNewDelegate(const CDestination& destDelegate)
{
    setDelegate.insert(destDelegate);
}

void CDelegate::RemoveDelegate(const CDestination& destDelegate)
{
    setDelegate.erase(destDelegate);
}

void CDelegate::Evolve(int nBlockHeight, const map<CDestination, size_t>& mapWeight,
                       const map<CDestination, vector<unsigned char>>& mapEnrollData,
                       CDelegateEvolveResult& result, const uint256& block_hash)
{
    const int nTarget = nBlockHeight + CONSENSUS_INTERVAL;
    const int nEnrollEnd = nBlockHeight + CONSENSUS_DISTRIBUTE_INTERVAL + 1;
    const int nPublish = nBlockHeight + 1;
    //const int nDelete = std::abs(nBlockHeight - 5);
    const int nDelete = nBlockHeight - CONSENSUS_DISTRIBUTE_INTERVAL - 1;

    result.Clear();

    /*if (mapVote.count(nDelete))
    {
        mapVote.erase(mapVote.find(nDelete));
    }*/
    if (nDelete > 0)
    {
        map<int, CDelegateVote>::iterator it = mapVote.find(nDelete);
        if (it != mapVote.end())
        {
            if (it->second.hashDistributeBlock != 0)
            {
                mapDistributeVote.erase(it->second.hashDistributeBlock);
            }
            mapVote.erase(it);
        }
    }

    // init
    {
        auto t0 = boost::posix_time::microsec_clock::universal_time();
        if (!mapVote.count(nTarget))
        {
            CDelegateVote& vote = mapVote[nTarget];
            vote.CreateDelegate(setDelegate);
            vote.Setup(MAX_DELEGATE_THRESH, result.mapEnrollData, block_hash);

            auto t1 = boost::posix_time::microsec_clock::universal_time();
            xengine::StdDebug("CDelegate", "Evolve Setup: target height: %d, time: %ld us, Setup block: [%d] %s",
                              nTarget, (t1 - t0).ticks(), vote.blockHash.Get32(7), vote.blockHash.GetHex().c_str());
        }
        else
        {
            xengine::StdError("CDelegate", "Evolve Setup: Already exist, target height: %d, Setup block: [%d] %s",
                              nTarget, block_hash.Get32(7), block_hash.GetHex().c_str());
        }
    }
    // enroll & distribute
    {
        map<int, CDelegateVote>::iterator it = mapVote.find(nEnrollEnd);
        if (it != mapVote.end())
        {
            auto t0 = boost::posix_time::microsec_clock::universal_time();

            //CDelegateVote& vote = (*it).second;
            if (it->second.hashDistributeBlock != 0 && mapDistributeVote.find(it->second.hashDistributeBlock) != mapDistributeVote.end())
            {
                mapDistributeVote.erase(it->second.hashDistributeBlock);
            }
            (*it).second.hashDistributeBlock = block_hash;
            CDelegateVote& vote = mapDistributeVote[block_hash];
            vote = (*it).second;
            if (!vote.is_enroll)
            {
                vote.is_enroll = true;

                vote.Enroll(mapWeight, mapEnrollData);
                vote.Distribute(result.mapDistributeData);
                vote.hashDistributeBlock = block_hash;

                auto t1 = boost::posix_time::microsec_clock::universal_time();
                xengine::StdDebug("CDelegate", "Evolve Enroll: target height: %d, time: %ld us, Setup block: [%d] %s, Distribute block: [%d] %s",
                                  nEnrollEnd, (t1 - t0).ticks(), vote.blockHash.Get32(7), vote.blockHash.GetHex().c_str(),
                                  vote.hashDistributeBlock.Get32(7), vote.hashDistributeBlock.GetHex().c_str());
            }
            else
            {
                xengine::StdError("CDelegate", "Evolve Enroll: Already is_enroll, target height: %d, Setup block: [%d] %s",
                                  nEnrollEnd, vote.blockHash.Get32(7), vote.blockHash.GetHex().c_str());
            }
        }
    }
    // publish
    {
        map<int, CDelegateVote>::iterator it = mapVote.find(nPublish);
        if (it != mapVote.end())
        {
            //CDelegateVote& vote = (*it).second;
            CDelegateVote& vote = mapDistributeVote[(*it).second.hashDistributeBlock];
            if (!vote.is_enroll)
            {
                xengine::StdError("CDelegate", "Evolve Publish: not enroll, target height: %d, Setup block: [%d] %s",
                                  nPublish, vote.blockHash.Get32(7), vote.blockHash.GetHex().c_str());
            }
            else
            {
                if (!vote.is_public)
                {
                    vote.is_public = true;

                    auto t0 = boost::posix_time::microsec_clock::universal_time();

                    vote.Publish(result.mapPublishData);
                    vote.hashPublishBlock = block_hash;

                    auto t1 = boost::posix_time::microsec_clock::universal_time();
                    xengine::StdDebug("CDelegate", "Evolve Publish: target height: %d, time: %ld us, Setup block: [%d] %s, Publish block: [%d] %s",
                                      nPublish, (t1 - t0).ticks(), vote.blockHash.Get32(7), vote.blockHash.GetHex().c_str(),
                                      vote.hashPublishBlock.Get32(7), vote.hashPublishBlock.GetHex().c_str());
                }
                else
                {
                    xengine::StdError("CDelegate", "Evolve Publish: Already is_public, target height: %d, Setup block: [%d] %s",
                                      nPublish, vote.blockHash.Get32(7), vote.blockHash.GetHex().c_str());
                }
            }
        }
    }
}

void CDelegate::Rollback(int nBlockHeightFrom, int nBlockHeightTo)
{
    /*
    // init
    {
        mapVote.erase(mapVote.upper_bound(nBlockHeightTo + CONSENSUS_INTERVAL), mapVote.end());
    }
    // enroll
    {
        mapVote.erase(mapVote.upper_bound(nBlockHeightTo + CONSENSUS_DISTRIBUTE_INTERVAL),
                      mapVote.upper_bound(nBlockHeightFrom + CONSENSUS_DISTRIBUTE_INTERVAL));
    }
    // distribute
    {
        mapVote.erase(mapVote.upper_bound(nBlockHeightTo + 1),
                      mapVote.upper_bound(nBlockHeightFrom + 1));
    }
    // publish
    {
        mapVote.erase(nBlockHeightFrom + 1);
    }*/
}

bool CDelegate::HandleDistribute(int nTargetHeight, const CDestination& destFrom,
                                 const vector<unsigned char>& vchDistributeData)
{
    map<int, CDelegateVote>::iterator it = mapVote.find(nTargetHeight);
    if (it != mapVote.end())
    {
        //CDelegateVote& vote = (*it).second;
        CDelegateVote& vote = mapDistributeVote[(*it).second.hashDistributeBlock];

        auto t0 = boost::posix_time::microsec_clock::universal_time();

        bool ret = vote.Accept(destFrom, vchDistributeData);

        auto t1 = boost::posix_time::microsec_clock::universal_time();
        xengine::StdDebug("CDelegate", "HandleDistribute: Accept target height: %d, time: %ld us, ret: %s, Setup block: [%d] %s, Distribute block: [%d] %s",
                          nTargetHeight, (t1 - t0).ticks(), (ret ? "true" : "false"), vote.blockHash.Get32(7), vote.blockHash.GetHex().c_str(),
                          vote.hashDistributeBlock.Get32(7), vote.hashDistributeBlock.GetHex().c_str());
        return ret;
    }
    else
    {
        xengine::StdError("CDelegate", "HandleDistribute: find target height fail, target height: %d", nTargetHeight);
    }
    return false;
}

bool CDelegate::HandlePublish(int nTargetHeight, const CDestination& destFrom,
                              const vector<unsigned char>& vchPublishData, bool& fCompleted)
{
    map<int, CDelegateVote>::iterator it = mapVote.find(nTargetHeight);
    if (it != mapVote.end())
    {
        //CDelegateVote& vote = (*it).second;
        CDelegateVote& vote = mapDistributeVote[(*it).second.hashDistributeBlock];

        auto t0 = boost::posix_time::microsec_clock::universal_time();

        bool ret = vote.Collect(destFrom, vchPublishData, fCompleted);

        auto t1 = boost::posix_time::microsec_clock::universal_time();

        xengine::StdDebug("CDelegate", "HandlePublish : Collect target height: %d, time: %ld us, ret: %s, Setup block: [%d] %s, Publish block: [%d] %s",
                          nTargetHeight, (t1 - t0).ticks(), (ret ? "true" : "false"), vote.blockHash.Get32(7), vote.blockHash.GetHex().c_str(),
                          vote.hashPublishBlock.Get32(7), vote.hashPublishBlock.GetHex().c_str());
        return ret;
    }
    else
    {
        xengine::StdError("CDelegate", "HandlePublish: find target height fail, height: %d", nTargetHeight);
    }
    return false;
}

void CDelegate::GetAgreement(int nTargetHeight, uint256& nAgreement, size_t& nWeight, map<CDestination, size_t>& mapBallot)
{
    nAgreement = 0;
    nWeight = 0;

    map<int, CDelegateVote>::iterator it = mapVote.find(nTargetHeight);
    if (it != mapVote.end())
    {
        //CDelegateVote& vote = (*it).second;
        CDelegateVote& vote = mapDistributeVote[(*it).second.hashDistributeBlock];

        auto t0 = boost::posix_time::microsec_clock::universal_time();

        vote.GetAgreement(nAgreement, nWeight, mapBallot);

        auto t1 = boost::posix_time::microsec_clock::universal_time();
        xengine::StdDebug("CDelegate", "Get agreement: Reconstruct target height: %d, time: %ld us, Setup block: [%d] %s, Distribute block: [%d] %s, Publish block: [%d] %s",
                          nTargetHeight, (t1 - t0).ticks(), vote.blockHash.Get32(7), vote.blockHash.GetHex().c_str(),
                          vote.hashDistributeBlock.Get32(7), vote.hashDistributeBlock.GetHex().c_str(),
                          vote.hashPublishBlock.Get32(7), vote.hashPublishBlock.GetHex().c_str());
    }
    else
    {
        xengine::StdError("CDelegate", "Get agreement: find target height fail, height: %d", nTargetHeight);
    }
}

void CDelegate::GetProof(int nTargetHeight, vector<unsigned char>& vchProof)
{
    map<int, CDelegateVote>::iterator it = mapVote.find(nTargetHeight);
    if (it != mapVote.end())
    {
        //CDelegateVote& vote = (*it).second;
        CDelegateVote& vote = mapDistributeVote[(*it).second.hashDistributeBlock];
        vote.GetProof(vchProof);
    }
}

} // namespace delegate
} // namespace bigbang
