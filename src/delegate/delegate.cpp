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
    mapDistributeVote.clear();
}

void CDelegate::Clear()
{
    Deinitialize();
}

void CDelegate::AddNewDelegate(const CDestination& destDelegate)
{
    setDelegate.insert(destDelegate);
}

void CDelegate::RemoveDelegate(const CDestination& destDelegate)
{
    setDelegate.erase(destDelegate);
}

bool CDelegate::SetupDistribute(const uint256& hashBlock, CDelegateVote& vote)
{
    map<uint256, CDelegateVote>::iterator mt = mapDistributeVote.find(hashBlock);
    if (mt != mapDistributeVote.end())
    {
        return false;
    }

    const int nEnrollEnd = hashBlock.Get32(7) + CONSENSUS_DISTRIBUTE_INTERVAL + 1;
    map<int, CDelegateVote>::iterator it = mapVote.find(nEnrollEnd);
    if (it == mapVote.end())
    {
        it = mapVote.insert(make_pair(nEnrollEnd, CDelegateVote())).first;
        if (it == mapVote.end())
        {
            StdError("CDelegate", "SetupDistribute: insert vote fail, target height: %d", nEnrollEnd);
            return false;
        }
        it->second.CreateDelegate(setDelegate);
        it->second.Setup(MAX_DELEGATE_THRESH);
    }

    mt = mapDistributeVote.insert(make_pair(hashBlock, CDelegateVote(it->second))).first;
    if (mt == mapDistributeVote.end())
    {
        StdError("CDelegate", "SetupDistribute: insert distribute fail");
        return false;
    }
    vote = mt->second;
    return true;
}

void CDelegate::SetEnroll(const uint256& hashBlock, const CDelegateVote& vote)
{
    map<uint256, CDelegateVote>::iterator mt = mapDistributeVote.find(hashBlock);
    if (mt != mapDistributeVote.end())
    {
        mt->second = vote;
    }
}

void CDelegate::RemoveDistribute(const uint256& hashBlock)
{
    mapDistributeVote.erase(hashBlock);
}

void CDelegate::Evolve(int nBlockHeight, const map<CDestination, size_t>& mapWeight,
                       const map<CDestination, vector<unsigned char>>& mapEnrollData,
                       CDelegateEvolveResult& result, const uint256& hashBlock)
{
    const int nTarget = nBlockHeight + CONSENSUS_INTERVAL;
    const int nEnrollEnd = nBlockHeight + CONSENSUS_DISTRIBUTE_INTERVAL + 1;
    const int nPublish = nBlockHeight + 1;
    const int nDelete = nBlockHeight - CONSENSUS_INTERVAL * 2;

    result.Clear();

    // delete
    if (nDelete > 0)
    {
        if (mapVote.size() < CONSENSUS_INTERVAL * 10)
        {
            map<int, CDelegateVote>::iterator it = mapVote.find(nDelete);
            if (it != mapVote.end())
            {
                StdTrace("CDelegate", "Evolve Deletate: erase vote, target height: %d, distribute block: %s",
                         nDelete, it->second.hashDistributeBlock.GetHex().c_str());
                const int nDistributeHeight = nDelete - (CONSENSUS_DISTRIBUTE_INTERVAL + 1);
                if (nDistributeHeight > 0)
                {
                    std::map<uint256, CDelegateVote>::iterator mt = mapDistributeVote.begin();
                    while (mt != mapDistributeVote.end())
                    {
                        if (mt->first.Get32(7) == nDistributeHeight)
                        {
                            mapDistributeVote.erase(mt++);
                        }
                        else
                        {
                            ++mt;
                        }
                    }
                }
                mapVote.erase(it);
            }
        }
        else
        {
            map<int, CDelegateVote>::iterator it = mapVote.begin();
            while (it != mapVote.end() && mapVote.size() > CONSENSUS_INTERVAL * 2)
            {
                const int nDistributeHeight = it->first - (CONSENSUS_DISTRIBUTE_INTERVAL + 1);
                if (nDistributeHeight > 0)
                {
                    std::map<uint256, CDelegateVote>::iterator mt = mapDistributeVote.begin();
                    while (mt != mapDistributeVote.end())
                    {
                        if (mt->first.Get32(7) == nDistributeHeight)
                        {
                            mapDistributeVote.erase(mt++);
                        }
                        else
                        {
                            ++mt;
                        }
                    }
                }
                mapVote.erase(it++);
            }
        }
    }

    // init
    {
        map<int, CDelegateVote>::iterator it = mapVote.find(nTarget);
        if (it == mapVote.end())
        {
            it = mapVote.insert(make_pair(nTarget, CDelegateVote())).first;
            if (it == mapVote.end())
            {
                StdError("CDelegate", "Evolve Setup: insert fail, target height: %d", nTarget);
                return;
            }

            auto t0 = boost::posix_time::microsec_clock::universal_time();

            it->second.CreateDelegate(setDelegate);
            it->second.Setup(MAX_DELEGATE_THRESH);

            auto t1 = boost::posix_time::microsec_clock::universal_time();

            StdDebug("CDelegate", "Evolve Setup: target height: %d, time: %ld us, setup block: [%d] %s",
                     nTarget, (t1 - t0).ticks(), hashBlock.Get32(7), hashBlock.GetHex().c_str());
        }
        it->second.GetSetupData(result.mapEnrollData);
    }

    // enroll & distribute
    {
        map<int, CDelegateVote>::iterator it = mapVote.find(nEnrollEnd);
        do
        {
            if (it == mapVote.end())
            {
                StdError("CDelegate", "Evolve Enroll: find target height fail, target height: %d", nEnrollEnd);
                break;
            }

            it->second.hashDistributeBlock = hashBlock;
            CDelegateVote& vote = mapDistributeVote[hashBlock];
            if (!vote.is_enroll)
            {
                vote = it->second;

                auto t0 = boost::posix_time::microsec_clock::universal_time();

                vote.is_enroll = true;
                result.witness = vote.Enroll(mapWeight, mapEnrollData);

                auto t1 = boost::posix_time::microsec_clock::universal_time();

                StdDebug("CDelegate", "Evolve Enroll: target height: %d, time: %ld us, distribute block: [%d] %s",
                         nEnrollEnd, (t1 - t0).ticks(), hashBlock.Get32(7), hashBlock.GetHex().c_str());
            }
            vote.Distribute(result.mapDistributeData);
            vote.hashDistributeBlock = hashBlock;
        } while (0);
    }

    // publish
    {
        map<int, CDelegateVote>::iterator it = mapVote.find(nPublish);
        do
        {
            if (it == mapVote.end())
            {
                StdError("CDelegate", "Evolve Publish: find target height fail, target height: %d", nPublish);
                break;
            }

            if (it->second.hashDistributeBlock == 0)
            {
                StdError("CDelegate", "Evolve Publish: hashDistributeBlock is null, target height: %d", nPublish);
                break;
            }

            const uint256& hashDistribute = it->second.hashDistributeBlock;
            map<uint256, CDelegateVote>::iterator mt = mapDistributeVote.find(hashDistribute);
            if (mt == mapDistributeVote.end())
            {
                StdError("CDelegate", "Evolve Publish: find distribute vote fail, target height: %d, distribute block: [%d] %s",
                         nPublish, hashDistribute.Get32(7), hashDistribute.GetHex().c_str());
                break;
            }

            CDelegateVote& vote = mt->second;
            if (!vote.is_enroll)
            {
                StdError("CDelegate", "Evolve Publish: not is enroll, target height: %d, distribute block: [%d] %s",
                         nPublish, hashDistribute.Get32(7), hashDistribute.GetHex().c_str());
                break;
            }
            if (vote.hashPublishBlock != 0 && vote.hashPublishBlock != hashBlock)
            {
                StdLog("CDelegate", "Evolve Publish: hash public block, target height: %d, distribute block: [%d] %s, old public: %s, new public: %s",
                       nPublish, hashDistribute.Get32(7), hashDistribute.GetHex().c_str(),
                       vote.hashPublishBlock.GetHex().c_str(), hashBlock.GetHex().c_str());
                vote.is_published = false;
                vote.nPublishedTime = 0;
            }
            if (vote.is_published)
            {
                StdError("CDelegate", "Evolve Publish: already public, target height: %d, distribute block: [%d] %s",
                         nPublish, hashDistribute.Get32(7), hashDistribute.GetHex().c_str());
                vote.Publish(result.mapPublishData);
                result.hashDistributeOfPublish = hashDistribute;
                break;
            }

            auto t0 = boost::posix_time::microsec_clock::universal_time();

            vote.Publish(result.mapPublishData);
            vote.hashPublishBlock = hashBlock;
            vote.is_published = true;
            vote.nPublishedTime = GetTime();
            result.hashDistributeOfPublish = hashDistribute;

            auto t1 = boost::posix_time::microsec_clock::universal_time();

            StdDebug("CDelegate", "Evolve Publish: target height: %d, time: %ld us, distribute block: [%d] %s",
                     nPublish, (t1 - t0).ticks(), hashDistribute.Get32(7), hashDistribute.GetHex().c_str());
        } while (0);
    }
}

void CDelegate::GetEvolveData(int nBlockHeight, CDelegateEvolveResult& result, const uint256& hashBlock)
{
    const int nEnrollEnd = nBlockHeight + CONSENSUS_DISTRIBUTE_INTERVAL + 1;
    const int nPublish = nBlockHeight + 1;

    result.Clear();

    // distribute
    {
        map<int, CDelegateVote>::iterator it = mapVote.find(nEnrollEnd);
        do
        {
            if (it == mapVote.end())
            {
                StdError("CDelegate", "GetEvolveData Enroll: find target height fail, target height: %d", nEnrollEnd);
                break;
            }
            if (it->second.hashDistributeBlock == 0)
            {
                StdError("CDelegate", "GetEvolveData Enroll: hashDistributeBlock is 0, target height: %d", nEnrollEnd);
                break;
            }
            if (it->second.hashDistributeBlock != hashBlock)
            {
                StdError("CDelegate", "GetEvolveData Enroll: hashDistributeBlock not is hashBlock, target height: %d, hashDistributeBlock: %s, hashBlock: %s",
                         nEnrollEnd, it->second.hashDistributeBlock.GetHex().c_str(), hashBlock.GetHex().c_str());
                break;
            }
            map<uint256, CDelegateVote>::iterator mt = mapDistributeVote.find(it->second.hashDistributeBlock);
            if (mt == mapDistributeVote.end())
            {
                StdError("CDelegate", "GetEvolveData Enroll: find distribute vote fail, target height: %d, distribute block: [%d] %s",
                         nEnrollEnd, it->second.hashDistributeBlock.Get32(7), it->second.hashDistributeBlock.GetHex().c_str());
                break;
            }
            mt->second.Distribute(result.mapDistributeData);
        } while (0);
    }

    // publish
    {
        map<int, CDelegateVote>::iterator it = mapVote.find(nPublish);
        do
        {
            if (it == mapVote.end())
            {
                StdError("CDelegate", "GetEvolveData Publish: find target height fail, target height: %d", nPublish);
                break;
            }
            if (it->second.hashDistributeBlock == 0)
            {
                StdError("CDelegate", "GetEvolveData Publish: hashDistributeBlock is null, target height: %d", nPublish);
                break;
            }
            map<uint256, CDelegateVote>::iterator mt = mapDistributeVote.find(it->second.hashDistributeBlock);
            if (mt == mapDistributeVote.end())
            {
                StdError("CDelegate", "GetEvolveData Publish: find distribute vote fail, target height: %d, distribute block: [%d] %s",
                         nPublish, it->second.hashDistributeBlock.Get32(7), it->second.hashDistributeBlock.GetHex().c_str());
                break;
            }
            mt->second.Publish(result.mapPublishData);
            result.hashDistributeOfPublish = it->second.hashDistributeBlock;
        } while (0);
    }
}

bool CDelegate::HandleDistribute(int nTargetHeight, const uint256& hashDistributeAnchor, const CDestination& destFrom,
                                 const vector<unsigned char>& vchDistributeData)
{
    map<int, CDelegateVote>::iterator it = mapVote.find(nTargetHeight);
    if (it == mapVote.end())
    {
        StdError("CDelegate", "HandleDistribute: find target height fail, target height: %d", nTargetHeight);
        return false;
    }
    const uint256& hashDistribute = it->second.hashDistributeBlock;
    if (hashDistribute == 0)
    {
        StdError("CDelegate", "HandleDistribute: hashDistributeBlock is null, target height: %d", nTargetHeight);
        return false;
    }
    if (hashDistribute != hashDistributeAnchor)
    {
        StdError("CDelegate", "HandleDistribute: distribute anchor error, target height: %d, local distribute: %s, anchor distribute: %s",
                 nTargetHeight, hashDistribute.GetHex().c_str(), hashDistributeAnchor.GetHex().c_str());
        return false;
    }
    map<uint256, CDelegateVote>::iterator mt = mapDistributeVote.find(hashDistribute);
    if (mt == mapDistributeVote.end())
    {
        StdError("CDelegate", "HandleDistribute: find distribute vote fail, target height: %d", nTargetHeight);
        return false;
    }
    CDelegateVote& vote = mt->second;

    auto t0 = boost::posix_time::microsec_clock::universal_time();
    bool ret = vote.Accept(destFrom, vchDistributeData);
    auto t1 = boost::posix_time::microsec_clock::universal_time();

    StdDebug("CDelegate", "HandleDistribute: Accept target height: %d, time: %ld us, ret: %s, distribute block: [%d] %s",
             nTargetHeight, (t1 - t0).ticks(), (ret ? "true" : "false"),
             hashDistribute.Get32(7), hashDistribute.GetHex().c_str());
    return ret;
}

bool CDelegate::HandlePublish(int nTargetHeight, const uint256& hashDistributeAnchor, const CDestination& destFrom,
                              const vector<unsigned char>& vchPublishData, bool& fCompleted)
{
    map<int, CDelegateVote>::iterator it = mapVote.find(nTargetHeight);
    if (it == mapVote.end())
    {
        StdError("CDelegate", "HandlePublish: find target height fail, height: %d", nTargetHeight);
        return false;
    }
    const uint256& hashDistribute = it->second.hashDistributeBlock;
    if (hashDistribute == 0)
    {
        StdError("CDelegate", "HandlePublish: hashDistributeBlock is null, target height: %d", nTargetHeight);
        return false;
    }
    if (hashDistribute != hashDistributeAnchor)
    {
        StdError("CDelegate", "HandlePublish: distribute anchor error, target height: %d, local distribute: %s, anchor distribute: %s",
                 nTargetHeight, hashDistribute.GetHex().c_str(), hashDistributeAnchor.GetHex().c_str());
        return false;
    }
    map<uint256, CDelegateVote>::iterator mt = mapDistributeVote.find(hashDistribute);
    if (mt == mapDistributeVote.end())
    {
        StdError("CDelegate", "HandlePublish: find distribute vote fail, target height: %d", nTargetHeight);
        return false;
    }
    CDelegateVote& vote = mt->second;

    auto t0 = boost::posix_time::microsec_clock::universal_time();
    bool ret = vote.Collect(destFrom, vchPublishData, fCompleted);
    auto t1 = boost::posix_time::microsec_clock::universal_time();

    StdDebug("CDelegate", "HandlePublish: Collect target height: %d, time: %ld us, ret: %s, completed: %s, distribute block: [%d] %s",
             nTargetHeight, (t1 - t0).ticks(), (ret ? "true" : "false"), (fCompleted ? "true" : "false"),
             hashDistribute.Get32(7), hashDistribute.GetHex().c_str());
    return ret;
}

void CDelegate::GetAgreement(int nTargetHeight, const uint256& hashDistributeAnchor, uint256& nAgreement, size_t& nWeight, map<CDestination, size_t>& mapBallot)
{
    nAgreement = 0;
    nWeight = 0;

    map<int, CDelegateVote>::iterator it = mapVote.find(nTargetHeight);
    if (it == mapVote.end())
    {
        StdError("CDelegate", "Get agreement: find target height fail, height: %d", nTargetHeight);
        return;
    }
    const uint256& hashDistribute = it->second.hashDistributeBlock;
    if (hashDistribute == 0)
    {
        StdError("CDelegate", "Get agreement: hashDistributeBlock is null, target height: %d", nTargetHeight);
        return;
    }
    if (hashDistribute != hashDistributeAnchor)
    {
        StdError("CDelegate", "Get agreement: hashDistributeBlock error, target height: %d, local distribute: %s, anchor distribute: %s",
                 nTargetHeight, hashDistribute.GetHex().c_str(), hashDistributeAnchor.GetHex().c_str());
        return;
    }
    map<uint256, CDelegateVote>::iterator mt = mapDistributeVote.find(hashDistribute);
    if (mt == mapDistributeVote.end())
    {
        StdError("CDelegate", "Get agreement: find distribute vote fail, target height: %d", nTargetHeight);
        return;
    }

    auto t0 = boost::posix_time::microsec_clock::universal_time();
    mt->second.GetAgreement(nAgreement, nWeight, mapBallot);
    auto t1 = boost::posix_time::microsec_clock::universal_time();

    StdDebug("CDelegate", "Get agreement: Reconstruct target height: %d, time: %ld us, nAgreement: %s, nWeight: %ld, mapBallot.size: %ld, distribute block: [%d] %s",
             nTargetHeight, (t1 - t0).ticks(), nAgreement.GetHex().c_str(), nWeight, mapBallot.size(),
             hashDistribute.Get32(7), hashDistribute.GetHex().c_str());
}

void CDelegate::GetProof(int nTargetHeight, vector<unsigned char>& vchProof)
{
    map<int, CDelegateVote>::iterator it = mapVote.find(nTargetHeight);
    if (it == mapVote.end())
    {
        StdError("CDelegate", "Get proof: find target height fail, height: %d", nTargetHeight);
        return;
    }
    const uint256& hashDistribute = it->second.hashDistributeBlock;
    if (hashDistribute == 0)
    {
        StdError("CDelegate", "Get proof: hashDistributeBlock is null, target height: %d", nTargetHeight);
        return;
    }
    map<uint256, CDelegateVote>::iterator mt = mapDistributeVote.find(hashDistribute);
    if (mt == mapDistributeVote.end())
    {
        StdError("CDelegate", "Get proof: find distribute vote fail, target height: %d", nTargetHeight);
        return;
    }
    mt->second.GetProof(vchProof);
}

bool CDelegate::IsCompleted(int nTargetHeight)
{
    map<int, CDelegateVote>::iterator it = mapVote.find(nTargetHeight);
    if (it == mapVote.end())
    {
        StdError("CDelegate", "Is completed: find target height fail, height: %d", nTargetHeight);
        return false;
    }
    const uint256& hashDistribute = it->second.hashDistributeBlock;
    if (hashDistribute == 0)
    {
        StdError("CDelegate", "Is completed: hashDistributeBlock is null, target height: %d", nTargetHeight);
        return false;
    }
    map<uint256, CDelegateVote>::iterator mt = mapDistributeVote.find(hashDistribute);
    if (mt == mapDistributeVote.end())
    {
        StdError("CDelegate", "Is completed: find distribute vote fail, target height: %d", nTargetHeight);
        return false;
    }
    return mt->second.IsCollectCompleted();
}

int64 CDelegate::GetPublishedTime(int nTargetHeight)
{
    map<int, CDelegateVote>::iterator it = mapVote.find(nTargetHeight);
    if (it == mapVote.end())
    {
        StdError("CDelegate", "Get publish time: find target height fail, target height: %d", nTargetHeight);
        return -1;
    }
    const uint256& hashDistribute = it->second.hashDistributeBlock;
    if (hashDistribute == 0)
    {
        StdError("CDelegate", "Get publish time: hashDistributeBlock is null, target height: %d", nTargetHeight);
        return -1;
    }
    map<uint256, CDelegateVote>::iterator mt = mapDistributeVote.find(hashDistribute);
    if (mt == mapDistributeVote.end())
    {
        StdError("CDelegate", "Get publish time: find distribute vote fail, target height: %d", nTargetHeight);
        return -1;
    }
    CDelegateVote& vote = mt->second;

    if (vote.is_published)
    {
        return vote.nPublishedTime;
    }
    return 0;
}

} // namespace delegate
} // namespace bigbang
