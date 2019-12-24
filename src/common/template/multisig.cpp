// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "multisig.h"

#include "rpc/auto_protocol.h"
#include "template.h"

using namespace std;
using namespace xengine;
using namespace bigbang::crypto;
using namespace bigbang;

//////////////////////////////
// CTemplateMultiSig

CTemplateMultiSig::CTemplateMultiSig(const uint8 nRequiredIn, const std::map<bigbang::crypto::CPubKey, uint8>& mapPubKeyWeightIn)
  : CTemplate(TEMPLATE_MULTISIG), CTemplateWeighted(nRequiredIn, mapPubKeyWeightIn)
{
}

CTemplateMultiSig* CTemplateMultiSig::clone() const
{
    return new CTemplateMultiSig(*this);
}

void CTemplateMultiSig::GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.multisig.nRequired = nRequired;
    for (const auto& keyweight : mapPubKeyWeight)
    {
        obj.multisig.vecAddresses.push_back(destInstance.SetPubKey(keyweight.first).ToString());
    }
}

bool CTemplateMultiSig::SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    if (obj.strType != GetTypeName(TEMPLATE_MULTISIG))
    {
        return false;
    }

    nRequired = obj.multisig.nRequired;
    if (nRequired != obj.weighted.nRequired)
    {
        return false;
    }

    for (const string& key : obj.multisig.vecPubkeys)
    {
        if (!destInstance.SetPubKey(key))
        {
            return false;
        }
        mapPubKeyWeight.insert(make_pair(destInstance.GetPubKey(), 1));
    }

    return true;
}
