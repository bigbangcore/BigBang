// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MPVSS_MPVSS_H
#define MPVSS_MPVSS_H

#include "mpbox.h"
#include "parallel.h"
#include "uint256.h"

class CMPCandidate
{
public:
    CMPCandidate() {}
    CMPCandidate(const uint256& nIdentIn, std::size_t nWeightIn, const CMPSealedBox& sBoxIn)
      : nIdent(nIdentIn), nWeight(nWeightIn), sBox(sBoxIn) {}
    bool Verify() const
    {
        return sBox.VerifySignature(nIdent);
    }
    const uint256 PubKey() const
    {
        return sBox.PubKey();
    }

public:
    uint256 nIdent;
    std::size_t nWeight;
    CMPSealedBox sBox;
};

class CMPParticipant
{
public:
    CMPParticipant();
    CMPParticipant(const CMPCandidate& candidate, std::size_t nIndexIn, const uint256& nSharedKeyIn);
    const uint256 Encrypt(const uint256& data) const;
    const uint256 Decrypt(const uint256& cipher) const;
    bool AcceptShare(std::size_t nThresh, std::size_t nIndexIn, const std::vector<uint256>& vEncrypedShare);
    bool VerifyShare(std::size_t nThresh, std::size_t nIndexIn, const std::vector<uint256>& vShare);
    void PrepareVerification(std::size_t nThresh, std::size_t nLastIndex);

public:
    std::size_t nWeight;
    std::size_t nIndex;
    CMPSealedBox sBox;
    uint256 nSharedKey;
    std::vector<uint256> vShare;
};

class CMPSecretShare
{
public:
    CMPSecretShare();
    CMPSecretShare(const uint256& nIdentIn);
    const uint256 GetIdent() const
    {
        return nIdent;
    }
    bool IsEnrolled() const
    {
        return (nIndex != 0);
    }
    void Setup(std::size_t nMaxThresh, CMPSealedBox& sealed);
    void SetupWitness();
    void Enroll(const std::vector<CMPCandidate>& vCandidate);
    void Distribute(std::map<uint256, std::vector<uint256>>& mapShare);
    bool Accept(const uint256& nIdentFrom, const std::vector<uint256>& vEncrypedShare);
    void Publish(std::map<uint256, std::vector<uint256>>& mapShare);
    bool Collect(const uint256& nIdentFrom, const std::map<uint256, std::vector<uint256>>& mapShare, bool& fCompleted);
    void Reconstruct(std::map<uint256, std::pair<uint256, std::size_t>>& mapSecret);
    void Signature(const uint256& hash, uint256& nR, uint256& nS);
    bool VerifySignature(const uint256& nIdentFrom, const uint256& hash, const uint256& nR, const uint256& nS);

protected:
    virtual void RandGeneretor(uint256& r);
    const uint256 RandShare();
    bool GetParticipantRange(const uint256& nIdentIn, std::size_t& nIndexRet, std::size_t& nWeightRet);

public:
    uint256 nIdent;
    CMPOpenedBox myBox;
    std::size_t nWeight;
    std::size_t nIndex;
    std::size_t nThresh;
    std::map<uint256, CMPParticipant> mapParticipant;
    std::map<uint256, std::vector<std::pair<uint32_t, uint256>>> mapOpenedShare;
    ParallelComputer computer;
};

#endif //MPVSS_MPVSS_H
