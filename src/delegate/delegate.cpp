// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "delegate.h"

using namespace std;
using namespace xengine;

namespace bigbang
{
namespace delegate
{

#define MAX_DELEGATE_THRESH (50)
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
                       CDelegateEvolveResult& result)
{
    const int nTarget = nBlockHeight + CONSENSUS_INTERVAL;
    const int nEnrollEnd = nBlockHeight + CONSENSUS_DISTRIBUTE_INTERVAL + 1;
    const int nPublish = nBlockHeight + 1;

    result.Clear();

    // init
    {
        auto t0 = boost::posix_time::microsec_clock::universal_time();

        CDelegateVote& vote = mapVote[nTarget];
        vote.CreateDelegate(setDelegate);
        vote.Setup(MAX_DELEGATE_THRESH, result.mapEnrollData);

        auto t1 = boost::posix_time::microsec_clock::universal_time();
        xengine::DebugLog("CDelegate", (string("Setup height:") + to_string(nTarget) + " time:" + to_string((t1 - t0).ticks())).c_str());
    }
    // enroll & distribute
    {
        map<int, CDelegateVote>::iterator it = mapVote.find(nEnrollEnd);
        if (it != mapVote.end())
        {
            auto t0 = boost::posix_time::microsec_clock::universal_time();

            CDelegateVote& vote = (*it).second;
            vote.Enroll(mapWeight, mapEnrollData);
            vote.Distribute(result.mapDistributeData);

            auto t1 = boost::posix_time::microsec_clock::universal_time();
            xengine::DebugLog("CDelegate", (string("Enroll height:") + to_string(nEnrollEnd) + " time:" + to_string((t1 - t0).ticks())).c_str());
        }
    }
    // publish
    {
        map<int, CDelegateVote>::iterator it = mapVote.find(nPublish);
        if (it != mapVote.end())
        {
            auto t0 = boost::posix_time::microsec_clock::universal_time();

            CDelegateVote& vote = (*it).second;
            vote.Publish(result.mapPublishData);

            auto t1 = boost::posix_time::microsec_clock::universal_time();
            xengine::DebugLog("CDelegate", (string("Publish height:") + to_string(nPublish) + " time:" + to_string((t1 - t0).ticks())).c_str());
        }
    }
}

void CDelegate::Rollback(int nBlockHeightFrom, int nBlockHeightTo)
{
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
    }
}

bool CDelegate::HandleDistribute(int nTargetHeight, const CDestination& destFrom,
                                 const vector<unsigned char>& vchDistributeData)
{
    map<int, CDelegateVote>::iterator it = mapVote.find(nTargetHeight);
    if (it != mapVote.end())
    {
        CDelegateVote& vote = (*it).second;

        auto t0 = boost::posix_time::microsec_clock::universal_time();

        bool ret = vote.Accept(destFrom, vchDistributeData);

        auto t1 = boost::posix_time::microsec_clock::universal_time();
        xengine::DebugLog("CDelegate", (string("Accept height:") + to_string(nTargetHeight) + " time:" + to_string((t1 - t0).ticks())).c_str());

        return ret;
    }
    return false;
}

bool CDelegate::HandlePublish(int nTargetHeight, const CDestination& destFrom,
                              const vector<unsigned char>& vchPublishData, bool& fCompleted)
{
    map<int, CDelegateVote>::iterator it = mapVote.find(nTargetHeight);
    if (it != mapVote.end())
    {
        CDelegateVote& vote = (*it).second;

        auto t0 = boost::posix_time::microsec_clock::universal_time();

        bool ret = vote.Collect(destFrom, vchPublishData, fCompleted);

        auto t1 = boost::posix_time::microsec_clock::universal_time();
        xengine::DebugLog("CDelegate", (string("Collect height:") + to_string(nTargetHeight) + " time:" + to_string((t1 - t0).ticks())).c_str());

        return ret;
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
        CDelegateVote& vote = (*it).second;

        auto t0 = boost::posix_time::microsec_clock::universal_time();

        vote.GetAgreement(nAgreement, nWeight, mapBallot);

        auto t1 = boost::posix_time::microsec_clock::universal_time();
        xengine::DebugLog("CDelegate", (string("Reconstruct height:") + to_string(nTargetHeight) + " time:" + to_string((t1 - t0).ticks())).c_str());
    }
}

void CDelegate::GetProof(int nTargetHeight, vector<unsigned char>& vchProof)
{
    map<int, CDelegateVote>::iterator it = mapVote.find(nTargetHeight);
    if (it != mapVote.end())
    {
        CDelegateVote& vote = (*it).second;
        vote.GetProof(vchProof);
    }
}

} // namespace delegate
} // namespace bigbang
