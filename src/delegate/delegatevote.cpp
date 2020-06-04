// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "delegatevote.h"

#include "crypto.h"

using namespace std;
using namespace xengine;

namespace bigbang
{
namespace delegate
{

//////////////////////////////
// CDelegateData

const uint256 CDelegateData::GetHash() const
{
    vector<unsigned char> vch;
    CODataStream os(vch);
    os << mapShare;
    return crypto::CryptoHash(&vch[0], vch.size());
}

//////////////////////////////
// CSecretShare

CSecretShare::CSecretShare()
{
}

CSecretShare::CSecretShare(const uint256& nIdentIn)
  : CMPSecretShare(nIdentIn)
{
}

CSecretShare::~CSecretShare()
{
}

void CSecretShare::Distribute(CDelegateData& delegateData)
{
    delegateData.nIdentFrom = nIdent;
    CMPSecretShare::Distribute(delegateData.mapShare);
    Signature(delegateData.GetHash(), delegateData.nR, delegateData.nS);
}

void CSecretShare::Publish(CDelegateData& delegateData)
{
    delegateData.nIdentFrom = nIdent;
    CMPSecretShare::Publish(delegateData.mapShare);
    Signature(delegateData.GetHash(), delegateData.nR, delegateData.nS);
}

void CSecretShare::RandGeneretor(uint256& r)
{
    crypto::CryptoGetRand256(r);
}

//////////////////////////////
// CDelegateVote

CDelegateVote::CDelegateVote()
{
    witness.SetupWitness();
    is_enroll = false;
    is_published = false;
    nPublishedTime = 0;
}

CDelegateVote::~CDelegateVote()
{
}

void CDelegateVote::CreateDelegate(const set<CDestination>& setDelegate)
{
    for (set<CDestination>::const_iterator it = setDelegate.begin(); it != setDelegate.end(); ++it)
    {
        const CDestination& dest = (*it);
        mapDelegate.insert(make_pair(dest, CSecretShare(DestToIdentUInt256(dest))));
    }
}

void CDelegateVote::Setup(size_t nMaxThresh, map<CDestination, vector<unsigned char>>& mapEnrollData, const uint256& block_hash)
{
    for (map<CDestination, CSecretShare>::iterator it = mapDelegate.begin(); it != mapDelegate.end(); ++it)
    {
        CSecretShare& delegate = (*it).second;
        CMPSealedBox sealed;
        delegate.Setup(nMaxThresh, sealed);

        CODataStream os(mapEnrollData[(*it).first]);
        os << sealed.nPubKey << sealed.vEncryptedCoeff << sealed.nR << sealed.nS;
    }
    blockHash = block_hash;
}

void CDelegateVote::Distribute(map<CDestination, std::vector<unsigned char>>& mapDistributeData)
{
    for (map<CDestination, CSecretShare>::iterator it = mapDelegate.begin(); it != mapDelegate.end(); ++it)
    {
        CSecretShare& delegate = (*it).second;
        if (delegate.IsEnrolled())
        {
            CDelegateData delegateData;
            delegate.Distribute(delegateData);

            CODataStream os(mapDistributeData[(*it).first]);
            os << delegateData;
        }
    }
}

void CDelegateVote::Publish(map<CDestination, vector<unsigned char>>& mapPublishData)
{
    for (map<CDestination, CSecretShare>::iterator it = mapDelegate.begin(); it != mapDelegate.end(); ++it)
    {
        CSecretShare& delegate = (*it).second;
        if (delegate.IsEnrolled())
        {
            CDelegateData delegateData;
            delegate.Publish(delegateData);

            CODataStream os(mapPublishData[(*it).first]);
            os << delegateData;
        }
    }
}

CSecretShare CDelegateVote::Enroll(const map<CDestination, size_t>& mapWeight,
                                   const map<CDestination, vector<unsigned char>>& mapEnrollData)
{
    vector<CMPCandidate> vCandidate;
    vCandidate.reserve(mapWeight.size());
    for (map<CDestination, size_t>::const_iterator it = mapWeight.begin(); it != mapWeight.end(); ++it)
    {
        map<CDestination, vector<unsigned char>>::const_iterator mi = mapEnrollData.find((*it).first);
        if (mi != mapEnrollData.end())
        {
            try
            {
                vector<uint256> vEncryptedCoeff;
                uint256 nPubKey, nR, nS;

                CIDataStream is((*mi).second);
                is >> nPubKey >> vEncryptedCoeff >> nR >> nS;

                vCandidate.push_back(CMPCandidate(DestToIdentUInt256((*it).first), (*it).second,
                                                  CMPSealedBox(vEncryptedCoeff, nPubKey, nR, nS)));
            }
            catch (exception& e)
            {
                StdError(__PRETTY_FUNCTION__, e.what());
            }
        }
    }
    witness.Enroll(vCandidate);

    for (map<CDestination, CSecretShare>::iterator it = mapDelegate.begin(); it != mapDelegate.end(); ++it)
    {
        CSecretShare& delegate = (*it).second;
        delegate.Enroll(vCandidate);
    }

    return witness;
}

bool CDelegateVote::Accept(const CDestination& destFrom, const vector<unsigned char>& vchDistributeData)
{
    CDelegateData delegateData;
    try
    {
        CIDataStream is(vchDistributeData);
        is >> delegateData;
        if (delegateData.nIdentFrom != DestToIdentUInt256(destFrom) || !VerifySignature(delegateData))
        {
            return false;
        }
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }

    for (map<CDestination, CSecretShare>::iterator it = mapDelegate.begin(); it != mapDelegate.end(); ++it)
    {
        CSecretShare& delegate = (*it).second;
        if (delegate.IsEnrolled())
        {
            map<uint256, vector<uint256>>::iterator mi = delegateData.mapShare.find(delegate.GetIdent());
            if (mi != delegateData.mapShare.end())
            {
                if (!delegate.Accept(delegateData.nIdentFrom, (*mi).second))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool CDelegateVote::Collect(const CDestination& destFrom, const vector<unsigned char>& vchPublishData, bool& fCompleted)
{
    try
    {
        CDelegateData delegateData;
        CIDataStream is(vchPublishData);
        is >> delegateData;
        if (delegateData.nIdentFrom == DestToIdentUInt256(destFrom) && VerifySignature(delegateData))
        {
            fCompleted = witness.IsCollectCompleted();
            if (fCompleted)
            {
                StdTrace("vote", "CDelegateVote::Collect is enough");
                return true;
            }

            if (witness.Collect(delegateData.nIdentFrom, delegateData.mapShare))
            {
                vCollected.push_back(delegateData);
                fCompleted = witness.IsCollectCompleted();
                return true;
            }
            else
            {
                StdError("vote", "CDelegateVote::Collect witness collect fail");
            }
        }
        else
        {
            StdError("vote", "CDelegateVote::Collect fail, delegateData.nIdentFrom == DestToIdentUInt256(destFrom): %d, VerifySignature(delegateData): %d",
                     delegateData.nIdentFrom == DestToIdentUInt256(destFrom), VerifySignature(delegateData));
        }
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
    }
    return false;
}

void CDelegateVote::GetAgreement(uint256& nAgreement, size_t& nWeight, map<CDestination, size_t>& mapBallot)
{
    nAgreement = 0;
    nWeight = 0;
    mapBallot.clear();

    map<uint256, pair<uint256, size_t>> mapSecret;

    witness.Reconstruct(mapSecret);

    if (!mapSecret.empty())
    {
        if (!witness.IsCollectCompleted())
        {
            StdLog("CDelegateVote", "Get agreement: mapSecret not is empty, completed: false");
        }
        vector<unsigned char> vch;
        CODataStream os(vch);
        for (map<uint256, pair<uint256, size_t>>::iterator it = mapSecret.begin();
             it != mapSecret.end(); ++it)
        {
            os << (*it).second.first;
            nWeight += (*it).second.second;
            mapBallot.insert(make_pair(DestFromIdentUInt256((*it).first), (*it).second.second));
        }
        nAgreement = crypto::CryptoHash(&vch[0], vch.size());
    }
    else
    {
        StdTrace("CDelegateVote", "Get agreement: mapSecret is empty, completed: %s", (witness.IsCollectCompleted() ? "true" : "false"));
    }
}

void CDelegateVote::GetProof(vector<unsigned char>& vchProof)
{
    CODataStream os(vchProof);
    os << vCollected;
}

bool CDelegateVote::IsCollectCompleted()
{
    return witness.IsCollectCompleted();
}

bool CDelegateVote::VerifySignature(const CDelegateData& delegateData)
{
    return witness.VerifySignature(delegateData.nIdentFrom, delegateData.GetHash(),
                                   delegateData.nR, delegateData.nS);
}

} // namespace delegate
} // namespace bigbang
