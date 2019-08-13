// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "proof.h"

#include "key.h"
#include "rpc/auto_protocol.h"
#include "template.h"
#include "util.h"

using namespace std;
using namespace xengine;
using namespace bigbang::crypto;

//////////////////////////////
// CTemplateProof

CTemplateProof::CTemplateProof(const bigbang::crypto::CPubKey keyMintIn, const CDestination& destSpendIn)
  : CTemplate(TEMPLATE_PROOF), keyMint(keyMintIn), destSpend(destSpendIn)
{
}

CTemplateProof* CTemplateProof::clone() const
{
    return new CTemplateProof(*this);
}

bool CTemplateProof::GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
                                        std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const
{
    if (!CTemplate::GetSignDestination(tx, vchSig, setSubDest, vchSubSig))
    {
        return false;
    }

    setSubDest.clear();
    setSubDest.insert(destSpend);
    return true;
}

void CTemplateProof::GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.mint.strMint = destInstance.SetPubKey(keyMint).ToString();
    obj.mint.strSpent = (destInstance = destSpend).ToString();
}

bool CTemplateProof::ValidateParam() const
{
    if (!keyMint)
    {
        return false;
    }

    if (!IsTxSpendable(destSpend))
    {
        return false;
    }

    return true;
}

bool CTemplateProof::SetTemplateData(const vector<uint8>& vchDataIn)
{
    CIDataStream is(vchDataIn);
    try
    {
        is >> keyMint >> destSpend;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }

    return true;
}

bool CTemplateProof::SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    if (obj.strType != GetTypeName(TEMPLATE_PROOF))
    {
        return false;
    }

    if (!destInstance.SetPubKey(obj.mint.strMint))
    {
        return false;
    }
    keyMint = destInstance.GetPubKey();

    if (!destInstance.ParseString(obj.mint.strSpent))
    {
        return false;
    }
    destSpend = destInstance;

    return true;
}

void CTemplateProof::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << keyMint << destSpend;
}

bool CTemplateProof::VerifyTxSignature(const uint256& hash, const uint256& hashAnchor, const CDestination& destTo,
                                       const vector<uint8>& vchSig, bool& fCompleted) const
{
    return destSpend.VerifyTxSignature(hash, hashAnchor, destTo, vchSig, fCompleted);
}

bool CTemplateProof::VerifyBlockSignature(const uint256& hash, const vector<uint8>& vchSig) const
{
    return keyMint.Verify(hash, vchSig);
}
