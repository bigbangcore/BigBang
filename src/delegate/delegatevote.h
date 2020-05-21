// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DELEGATE_DELEGATEVOTE_H
#define DELEGATE_DELEGATEVOTE_H

#include <set>

#include "delegatecomm.h"
#include "mpvss.h"

namespace bigbang
{
namespace delegate
{

class CDelegateData
{
public:
    CDelegateData()
    {
    }
    CDelegateData(const uint256& nIdentFromIn)
      : nIdentFrom(nIdentFromIn)
    {
    }
    void ToDataStream(xengine::CODataStream& os) const
    {
        os << nIdentFrom << mapShare << nR << nS;
    }
    void FromDataStream(xengine::CIDataStream& is)
    {
        is >> nIdentFrom >> mapShare >> nR >> nS;
    }
    const uint256 GetHash() const;
    std::string ToString() const
    {
        std::ostringstream os;
        os << "CDelegateData: \n";
        os << nIdentFrom.GetHex() << "\n";
        os << nR.GetHex() << "\n";
        os << nS.GetHex() << "\n";
        os << mapShare.size() << "\n";
        for (std::map<uint256, std::vector<uint256>>::const_iterator it = mapShare.begin();
             it != mapShare.end(); ++it)
        {
            os << " " << (*it).first.GetHex() << " " << (*it).second.size() << "\n";
            for (int i = 0; i < (*it).second.size(); i++)
            {
                os << "   " << (*it).second[i].GetHex() << "\n";
            }
        }
        return os.str();
    }

public:
    uint256 nIdentFrom;
    std::map<uint256, std::vector<uint256>> mapShare;
    uint256 nR;
    uint256 nS;
};

class CSecretShare : public CMPSecretShare
{
public:
    CSecretShare();
    CSecretShare(const uint256& nIdentIn);
    ~CSecretShare();
    void Distribute(CDelegateData& delegateData);
    void Publish(CDelegateData& delegateData);

protected:
    void RandGeneretor(uint256& r);
};

// 一段共识过程中的一个快照，也就是对应到某个高度，block hash下的共识快照
class CDelegateVote
{
public:
    CDelegateVote();
    ~CDelegateVote();
    void CreateDelegate(const std::set<CDestination>& setDelegate);

    void Setup(std::size_t nMaxThresh, std::map<CDestination, std::vector<unsigned char>>& mapEnrollData, const uint256& block_hash);
    void Distribute(std::map<CDestination, std::vector<unsigned char>>& mapDistributeData);
    void Publish(std::map<CDestination, std::vector<unsigned char>>& mapPublishData);

    void Enroll(const std::map<CDestination, size_t>& mapWeight,
                const std::map<CDestination, std::vector<unsigned char>>& mapEnrollData);
    bool Accept(const CDestination& destFrom, const std::vector<unsigned char>& vchDistributeData);
    bool Collect(const CDestination& destFrom, const std::vector<unsigned char>& vchPublishData, bool& fCompleted);
    void GetAgreement(uint256& nAgreement, std::size_t& nWeight, std::map<CDestination, std::size_t>& mapBallot);
    void GetProof(std::vector<unsigned char>& vchProof);
    bool IsCollectCompleted();

protected:
    bool VerifySignature(const CDelegateData& delegateData);

protected:
    std::map<CDestination, CSecretShare> mapDelegate;
    CSecretShare witness;

    std::vector<CDelegateData> vCollected;

public:
    uint256 blockHash;
    bool is_enroll;
    bool is_published;
    int64 nPublishedTime;
    uint256 hashDistributeBlock;
    uint256 hashPublishBlock;
};

} // namespace delegate
} // namespace bigbang

#endif //DELEGATE_DELEGATE_VOTE_H
