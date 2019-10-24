// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mpvss.h"

#include <iostream>
#include <stdlib.h>

#include "mpinterpolation.h"
#include "util.h"

using namespace std;
using namespace xengine;

//////////////////////////////
// CMPParticipant
CMPParticipant::CMPParticipant()
{
}

CMPParticipant::CMPParticipant(const CMPCandidate& candidate, size_t nIndexIn, const uint256& nSharedKeyIn)
  : nWeight(candidate.nWeight), nIndex(nIndexIn), sBox(candidate.sBox), nSharedKey(nSharedKeyIn)
{
}

const uint256 CMPParticipant::Encrypt(const uint256& data) const
{
    return (nSharedKey ^ data);
}

const uint256 CMPParticipant::Decrypt(const uint256& cipher) const
{
    return (nSharedKey ^ cipher);
}

bool CMPParticipant::AcceptShare(size_t nThresh, size_t nIndexIn, const vector<uint256>& vEncrypedShare)
{
    vShare.resize(vEncrypedShare.size());
    for (size_t i = 0; i < vEncrypedShare.size(); i++)
    {
        vShare[i] = Decrypt(vEncrypedShare[i]);
        if (!sBox.VerifyPolynomial(nIndexIn + i, vShare[i]))
        {
            vShare.clear();
            return false;
        }
    }
    return true;
}

bool CMPParticipant::VerifyShare(size_t nThresh, size_t nIndexIn, const vector<uint256>& vShare)
{
    for (size_t i = 0; i < vShare.size(); i++)
    {
        if (!sBox.VerifyPolynomial(nIndexIn + i, vShare[i]))
        {
            return false;
        }
    }
    return true;
}

void CMPParticipant::PrepareVerification(std::size_t nThresh, std::size_t nLastIndex)
{
    sBox.PrecalcPolynomial(nThresh, nLastIndex);
}

//////////////////////////////
// CMPSecretShare

CMPSecretShare::CMPSecretShare()
{
    nIndex = 0;
    nThresh = 0;
    nWeight = 0;
}

CMPSecretShare::CMPSecretShare(const uint256& nIdentIn)
  : nIdent(nIdentIn)
{
    nIndex = 0;
    nThresh = 0;
    nWeight = 0;
}

void CMPSecretShare::RandGeneretor(uint256& r)
{
    uint8_t* p = r.begin();
    for (int i = 0; i < 32; i++)
    {
        *p++ = rand();
    }
}

const uint256 CMPSecretShare::RandShare()
{
    uint256 r;
    RandGeneretor(r);
    r[7] &= 0x0FFFFFFFULL;
    return r;
}

bool CMPSecretShare::GetParticipantRange(const uint256& nIdentIn, size_t& nIndexRet, size_t& nWeightRet)
{
    if (nIdentIn == nIdent)
    {
        nIndexRet = nIndex;
        nWeightRet = nWeight;
        return true;
    }

    map<uint256, CMPParticipant>::iterator it = mapParticipant.find(nIdentIn);
    if (it == mapParticipant.end())
    {
        return false;
    }

    nIndexRet = (*it).second.nIndex;
    nWeightRet = (*it).second.nWeight;
    return true;
}

void CMPSecretShare::Setup(size_t nMaxThresh, CMPSealedBox& sealed)
{
    myBox.vCoeff.clear();
    myBox.vCoeff.resize(nMaxThresh);
    do
    {
        myBox.nPrivKey = RandShare();
        for (int i = 0; i < nMaxThresh; i++)
        {
            myBox.vCoeff[i] = RandShare();
        }
    } while (!myBox.MakeSealedBox(sealed, nIdent, RandShare()));

    nIndex = 0;
    nThresh = 0;
    nWeight = 0;
    mapParticipant.clear();
    mapOpenedShare.clear();
}

void CMPSecretShare::SetupWitness()
{
    myBox.vCoeff.clear();
    nIdent = 0;
    nIndex = 0;
    nThresh = 0;
    nWeight = 0;
    mapParticipant.clear();
    mapOpenedShare.clear();
}

void CMPSecretShare::Enroll(const vector<CMPCandidate>& vCandidate)
{
    size_t nLastIndex = 1;
    for (size_t i = 0; i < vCandidate.size(); i++)
    {
        const CMPCandidate& candidate = vCandidate[i];
        if (candidate.nIdent == nIdent)
        {
            nIndex = nLastIndex;
            nWeight = candidate.nWeight;
            nLastIndex += candidate.nWeight;
        }
        else if (!mapParticipant.count(candidate.nIdent) && candidate.Verify())
        {
            try
            {
                uint256 shared = myBox.SharedKey(candidate.PubKey());
                mapParticipant[candidate.nIdent] = CMPParticipant(candidate, nLastIndex, shared);
                nLastIndex += candidate.nWeight;
            }
            catch (exception& e)
            {
                LOG_ERROR("CMPSecretShare", "Enroll error: %s", e.what());
            }
        }
    }
    nThresh = (nLastIndex - 1) / 2 + 1;

    // parallel compute
    vector<CMPParticipant*> vParticipant;
    vParticipant.reserve(mapParticipant.size());
    for (auto& x : mapParticipant)
    {
        vParticipant.push_back(&x.second);
    }
    computer.Execute(vParticipant.begin(), vParticipant.end(),
                     [&](CMPParticipant* p) { p->PrepareVerification(nThresh, nLastIndex); });
}

void CMPSecretShare::Distribute(map<uint256, vector<uint256>>& mapShare)
{
    vector<CMPParticipant*> vecParticipant;
    vecParticipant.reserve(mapParticipant.size());
    vector<vector<uint256>*> vecShare;
    vecShare.reserve(mapParticipant.size());
    for (auto it = mapParticipant.begin(); it != mapParticipant.end(); it++)
    {
        vecParticipant.push_back(&it->second);
        vector<uint256>& vShare = mapShare[it->first];
        vecShare.push_back(&vShare);
    }

    computer.Transform(
        vecParticipant.size(),
        std::bind(&LoadVectorData<CMPParticipant*>, cref(vecParticipant), placeholders::_1),
        [&](size_t nIndex, const vector<uint256>& vShare) {
            *vecShare[nIndex] = vShare;
        },
        [&](CMPParticipant* p) {
            vector<uint256> vShare;
            vShare.resize(p->nWeight);
            for (size_t i = 0; i < p->nWeight; i++)
            {
                vShare[i] = p->Encrypt(myBox.Polynomial(nThresh, p->nIndex + i));
            }
            return vShare;
        });
}

bool CMPSecretShare::Accept(const uint256& nIdentFrom, const vector<uint256>& vEncryptedShare)
{
    if (vEncryptedShare.size() == nWeight)
    {
        map<uint256, CMPParticipant>::iterator it = mapParticipant.find(nIdentFrom);
        if (it != mapParticipant.end())
        {
            return (*it).second.AcceptShare(nThresh, nIndex, vEncryptedShare);
        }
    }
    return false;
}

void CMPSecretShare::Publish(map<uint256, vector<uint256>>& mapShare)
{
    mapShare.clear();

    for (map<uint256, CMPParticipant>::iterator it = mapParticipant.begin(); it != mapParticipant.end(); ++it)
    {
        if (!(*it).second.vShare.empty())
        {
            mapShare.insert(make_pair((*it).first, (*it).second.vShare));
        }
    }

    vector<uint256>& myShare = mapShare[nIdent];
    for (size_t i = 0; i < nWeight; i++)
    {
        myShare.push_back(myBox.Polynomial(nThresh, nIndex + i));
    }
}

bool CMPSecretShare::Collect(const uint256& nIdentFrom, const map<uint256, vector<uint256>>& mapShare, bool& fCompleted)
{
    size_t nIndexFrom, nWeightFrom;

    fCompleted = false;

    if (!GetParticipantRange(nIdentFrom, nIndexFrom, nWeightFrom))
    {
        return false;
    }

    vector<tuple<CMPParticipant*, const vector<uint256>*>> vecPartShare;
    vecPartShare.reserve(mapShare.size());
    for (map<uint256, vector<uint256>>::const_iterator mi = mapShare.begin(); mi != mapShare.end(); ++mi)
    {
        const vector<uint256>& vShare = (*mi).second;
        if (nWeightFrom != vShare.size())
        {
            return false;
        }

        if ((*mi).first == nIdent)
        {
            for (size_t i = 0; i < vShare.size(); i++)
            {
                if (myBox.Polynomial(nThresh, nIndexFrom + i) != vShare[i])
                {
                    return false;
                }
            }
            continue;
        }

        map<uint256, CMPParticipant>::iterator it = mapParticipant.find((*mi).first);
        if (it == mapParticipant.end())
        {
            return false;
        }

        vecPartShare.emplace_back(&it->second, &vShare);
    }

    if (!computer.ExecuteUntil(vecPartShare.begin(), vecPartShare.end(),
                               [&](CMPParticipant* p, const vector<uint256>* pVecShare) -> bool {
                                   return p->VerifyShare(nThresh, nIndexFrom, *pVecShare);
                               }))
    {
        return false;
    }

    size_t nCompleteCollect = 0;
    for (map<uint256, vector<uint256>>::const_iterator mi = mapShare.begin(); mi != mapShare.end(); ++mi)
    {
        vector<pair<uint32_t, uint256>>& vOpenedShare = mapOpenedShare[(*mi).first];
        for (size_t i = 0; i < nWeightFrom && vOpenedShare.size() < nThresh; i++)
        {
            vOpenedShare.emplace_back(nIndexFrom + i, (*mi).second[i]);
        }
        if (vOpenedShare.size() == nThresh)
        {
            nCompleteCollect++;
        }
    }
    fCompleted = (nCompleteCollect >= mapShare.size());
    return true;
}

void CMPSecretShare::Reconstruct(map<uint256, pair<uint256, size_t>>& mapSecret)
{
    using ShareType = tuple<const uint256*, std::vector<std::pair<uint32_t, uint256>>*>;
    vector<ShareType> vOpenedShare;
    vOpenedShare.reserve(mapOpenedShare.size());

    map<uint256, vector<pair<uint32_t, uint256>>>::iterator it;
    for (it = mapOpenedShare.begin(); it != mapOpenedShare.end(); ++it)
    {
        vOpenedShare.emplace_back(&it->first, &it->second);
    }

    // parallel compute
    using DataType = pair<uint256, size_t>;
    vector<DataType> vData(vOpenedShare.size());

    computer.Transform(vOpenedShare.begin(), vOpenedShare.end(), vData.begin(),
                       [&](const uint256* pIdent, std::vector<std::pair<uint32_t, uint256>>* pShare) {
                           if (pShare->size() == nThresh)
                           {
                               const uint256& nIdentAvail = *pIdent;
                               size_t nIndexRet, nWeightRet;
                               if (GetParticipantRange(nIdentAvail, nIndexRet, nWeightRet))
                               {
                                   return make_pair(MPNewton(*pShare), nWeightRet);
                               }
                           }
                           return make_pair(uint256(uint64(0)), (size_t)0);
                       });

    for (int i = 0; i < vData.size(); ++i)
    {
        const DataType& data = vData[i];
        if (data.second > 0)
        {
            const uint256* pIndet = get<0>(vOpenedShare[i]);
            mapSecret[*pIndet] = data;
        }
    }
}

void CMPSecretShare::Signature(const uint256& hash, uint256& nR, uint256& nS)
{
    uint256 r = RandShare();
    myBox.Signature(hash, r, nR, nS);
}

bool CMPSecretShare::VerifySignature(const uint256& nIdentFrom, const uint256& hash, const uint256& nR, const uint256& nS)
{
    if (nIdentFrom == nIdent)
    {
        return myBox.VerifySignature(hash, nR, nS);
    }
    map<uint256, CMPParticipant>::iterator it = mapParticipant.find(nIdentFrom);
    if (it == mapParticipant.end())
    {
        return false;
    }
    CMPParticipant& participant = (*it).second;
    return participant.sBox.VerifySignature(hash, nR, nS);
}
