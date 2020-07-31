// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "weighted.h"

#include <set>

#include "rpc/auto_protocol.h"
#include "template.h"
#include "util.h"

using namespace std;
using namespace xengine;
using namespace bigbang::crypto;

static const int MAX_KEY_NUMBER = 16;

//////////////////////////////
// CTemplateWeighted

CTemplateWeighted::CTemplateWeighted(const uint8 nRequiredIn, const std::map<bigbang::crypto::CPubKey, uint8>& mapPubKeyWeightIn)
  : CTemplate(TEMPLATE_WEIGHTED), nRequired(nRequiredIn), mapPubKeyWeight(mapPubKeyWeightIn)
{
}

CTemplateWeighted* CTemplateWeighted::clone() const
{
    return new CTemplateWeighted(*this);
}

bool CTemplateWeighted::GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
                                           std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const
{
    if (!CTemplate::GetSignDestination(tx, vchSig, setSubDest, vchSubSig))
    {
        return false;
    }

    setSubDest.clear();
    for (auto& keyweight : mapPubKeyWeight)
    {
        setSubDest.insert(keyweight.first);
    }

    return true;
}

void CTemplateWeighted::GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.weighted.nRequired = nRequired;
    for (const auto& keyweight : mapPubKeyWeight)
    {
        bigbang::rpc::CTemplatePubKeyWeight pubkey;
        pubkey.strKey = destInstance.SetPubKey(keyweight.first).ToString();
        pubkey.nWeight = keyweight.second;
        obj.weighted.vecAddresses.push_back(pubkey);
    }
}

bool CTemplateWeighted::ValidateParam() const
{
    if (nRequired < 1)
    {
        return false;
    }

    if (mapPubKeyWeight.size() <= 1 || mapPubKeyWeight.size() > MAX_KEY_NUMBER)
    {
        return false;
    }

    int nWeight = 0;
    for (const auto& keyweight : mapPubKeyWeight)
    {
        if (keyweight.second < 1)
        {
            return false;
        }
        nWeight += keyweight.second;
    }

    if (nWeight < nRequired)
    {
        return false;
    }

    return true;
}

bool CTemplateWeighted::SetTemplateData(const vector<uint8>& vchDataIn)
{
    CIDataStream is(vchDataIn);
    try
    {
        is >> nRequired >> mapPubKeyWeight;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }

    return true;
}

bool CTemplateWeighted::SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    if (obj.strType != GetTypeName(TEMPLATE_WEIGHTED))
    {
        return false;
    }

    nRequired = obj.weighted.nRequired;
    if (nRequired != obj.weighted.nRequired)
    {
        return false;
    }

    for (auto& keyweight : obj.weighted.vecPubkeys)
    {
        if (!destInstance.SetPubKey(keyweight.strKey))
        {
            return false;
        }
        mapPubKeyWeight.insert(make_pair(destInstance.GetPubKey(), keyweight.nWeight));
    }

    return true;
}

void CTemplateWeighted::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << nRequired << mapPubKeyWeight;
}

bool CTemplateWeighted::VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor,
                                          const CDestination& destTo, const vector<uint8>& vchSig,
                                          const int32 nForkHeight, bool& fCompleted) const
{
    set<uint256> setPubKey;
    for (const auto& keyweight : mapPubKeyWeight)
    {
        setPubKey.insert(keyweight.first);
    }

    set<uint256> setPartKey;
    // before HEIGHT_HASH_MULTI_SIGNER, used defect multi-sign algorithm
    //if (nForkHeight > 0 && nForkHeight < HEIGHT_HASH_MULTI_SIGNER)
    //{
    //    xengine::StdTrace("multi-sign-template", "nHeight: %u, range: (0, %u)", nForkHeight, HEIGHT_HASH_MULTI_SIGNER);
    //    if (!CryptoMultiVerifyDefect(setPubKey, hashAnchor.begin(), hashAnchor.size(), hash.begin(), hash.size(), vchSig, setPartKey))
    //    {
    //        return false;
    //    }
    //    xengine::StdTrace("multi-sign-template-success", "nHeight: %u, range: (0, %u)", nForkHeight, HEIGHT_HASH_MULTI_SIGNER);
    //}
    //else
    //{
    //xengine::StdTrace("multi-sign-template", "nHeight: %u, range: [%u, infinite)", nForkHeight, HEIGHT_HASH_MULTI_SIGNER);
    if (!CryptoMultiVerify(setPubKey, hash.begin(), hash.size(), vchSig, setPartKey))
    {
        return false;
    }
    //xengine::StdTrace("multi-sign-template-success", "nHeight: %u, range: [%u, infinite)", nForkHeight, HEIGHT_HASH_MULTI_SIGNER);
    //}

    int nWeight = 0;
    for (const uint256& key : setPartKey)
    {
        auto it = mapPubKeyWeight.find(CPubKey(key));
        if (it != mapPubKeyWeight.end())
        {
            nWeight += it->second;
        }
    }
    fCompleted = (nWeight >= nRequired);

    return true;
}
