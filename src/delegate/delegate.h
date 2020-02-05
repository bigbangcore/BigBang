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
};

class CDelegate
{
public:
    CDelegate() = default;
    ~CDelegate() = default;
    bool Initialize();
    void Deinitialize();
    void AddNewDelegate(const CDestination& destDelegate);
    void RemoveDelegate(const CDestination& destDelegate);

    void Evolve(int nBlockHeight, const std::map<CDestination, std::size_t>& mapWeight,
                const std::map<CDestination, std::vector<unsigned char>>& mapEnrollData,
                CDelegateEvolveResult& result, const uint256& block_hash);
    void Rollback(int nBlockHeightFrom, int nBlockHeightTo);
    bool HandleDistribute(int nTargetHeight, const CDestination& destFrom,
                          const std::vector<unsigned char>& vchDistributeData);
    bool HandlePublish(int nTargetHeight, const CDestination& destFrom,
                       const std::vector<unsigned char>& vchPublishData, bool& fCompleted);
    void GetAgreement(int nTargetHeight, uint256& nAgreement, std::size_t& nWeight, std::map<CDestination, std::size_t>& mapBallot);
    void GetProof(int nTargetHeight, std::vector<unsigned char>& vchProof);

protected:
    std::set<CDestination> setDelegate;
    std::map<int, CDelegateVote> mapVote;
};

} // namespace delegate
} // namespace bigbang

#endif //DELEGATE_DELEGATE_H
