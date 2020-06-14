// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DELEGATE_DELEGATE_H
#define DELEGATE_DELEGATE_H

#include "delegatecomm.h"
#include "delegatevote.h"
#include "destination.h"

namespace bigbang
{
namespace delegate
{

class CDelegateEvolveResult
{
public:
    void Clear()
    {
        mapEnrollData.clear();
        mapDistributeData.clear();
        mapPublishData.clear();
    }

public:
    std::map<CDestination, std::vector<unsigned char>> mapEnrollData;
    std::map<CDestination, std::vector<unsigned char>> mapDistributeData;
    std::map<CDestination, std::vector<unsigned char>> mapPublishData;
    uint256 hashDistributeOfPublish;
    CSecretShare witness;
};

class CDelegate
{
    friend class xengine::CStream;

public:
    CDelegate();
    ~CDelegate();
    bool Initialize();
    void Deinitialize();
    void Clear();
    void AddNewDelegate(const CDestination& destDelegate);
    void RemoveDelegate(const CDestination& destDelegate);

    bool SetupDistribute(const uint256& hashBlock, CDelegateVote& vote);
    void SetEnroll(const uint256& hashBlock, const CDelegateVote& vote);
    void RemoveDistribute(const uint256& hashBlock);

    void Evolve(int nBlockHeight, const std::map<CDestination, std::size_t>& mapWeight,
                const std::map<CDestination, std::vector<unsigned char>>& mapEnrollData,
                CDelegateEvolveResult& result, const uint256& hashBlock);
    void GetEvolveData(int nBlockHeight, CDelegateEvolveResult& result, const uint256& hashBlock);
    bool HandleDistribute(int nTargetHeight, const uint256& hashDistributeAnchor, const CDestination& destFrom,
                          const std::vector<unsigned char>& vchDistributeData);
    bool HandlePublish(int nTargetHeight, const uint256& hashDistributeAnchor, const CDestination& destFrom,
                       const std::vector<unsigned char>& vchPublishData, bool& fCompleted);
    void GetAgreement(int nTargetHeight, const uint256& hashDistributeAnchor, uint256& nAgreement, std::size_t& nWeight, std::map<CDestination, std::size_t>& mapBallot);
    void GetProof(int nTargetHeight, std::vector<unsigned char>& vchProof);
    bool IsCompleted(int nTargetHeight);
    int64 GetPublishedTime(int nTargetHeight);

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(mapVote, opt);
        s.Serialize(mapDistributeVote, opt);
    }

protected:
    std::set<CDestination> setDelegate;
    std::map<int, CDelegateVote> mapVote;
    std::map<uint256, CDelegateVote> mapDistributeVote;
};

} // namespace delegate
} // namespace bigbang

#endif //DELEGATE_DELEGATE_H
