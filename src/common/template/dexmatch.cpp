// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dexmatch.h"

#include "destination.h"
#include "rpc/auto_protocol.h"
#include "template.h"
#include "transaction.h"
#include "util.h"

using namespace std;
using namespace xengine;

static const int64 COIN = 1000000;

//////////////////////////////
// CTemplateDexMatch

CTemplateDexMatch::CTemplateDexMatch(const CDestination& destMatchIn, int64 nMatchAmountIn, double dFeeIn,
                                     const CDestination& destSellerOrderIn, const CDestination& destSellerIn,
                                     const CDestination& destSellerDealIn, int nSellerValidHeightIn, const CDestination& destBuyerOrderIn,
                                     const CDestination& destBuyerIn, const uint256& hashBuyerSecretIn, int nBuyerValidHeightIn)
  : destMatch(destMatchIn),
    nMatchAmount(nMatchAmountIn),
    dFee(dFeeIn),
    destSellerOrder(destSellerOrderIn),
    destSeller(destSellerIn),
    destSellerDeal(destSellerDealIn),
    nSellerValidHeight(nSellerValidHeightIn),
    destBuyerOrder(destBuyerOrderIn),
    destBuyer(destBuyerIn),
    hashBuyerSecret(hashBuyerSecretIn),
    nBuyerValidHeight(nBuyerValidHeightIn)
{
}

CTemplateDexMatch* CTemplateDexMatch::clone() const
{
    return new CTemplateDexMatch(*this);
}

bool CTemplateDexMatch::GetSignDestination(const CTransaction& tx, const vector<uint8>& vchSig,
                                           set<CDestination>& setSubDest, vector<uint8>& vchSubSig) const
{
    if (!CTemplate::GetSignDestination(tx, vchSig, setSubDest, vchSubSig))
    {
        return false;
    }
    setSubDest.clear();
    if (tx.sendTo == destMatch || tx.sendTo == destSellerDeal || tx.sendTo == destBuyer)
    {
        setSubDest.insert(destSellerDeal);
    }
    else
    {
        setSubDest.insert(destSeller);
    }
    return true;
}

void CTemplateDexMatch::GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.dexmatch.strMatch_Address = (destInstance = destMatch).ToString();
    obj.dexmatch.dMatch_Amount = (double)nMatchAmount / COIN;
    obj.dexmatch.dFee = dFee;

    obj.dexmatch.strSeller_Order_Address = (destInstance = destSellerOrder).ToString();
    obj.dexmatch.strSeller_Address = (destInstance = destSeller).ToString();
    obj.dexmatch.strSeller_Deal_Address = (destInstance = destSellerDeal).ToString();
    obj.dexmatch.nSeller_Valid_Height = nSellerValidHeight;

    obj.dexmatch.strBuyer_Order_Address = (destInstance = destBuyerOrder).ToString();
    obj.dexmatch.strBuyer_Address = (destInstance = destBuyer).ToString();
    obj.dexmatch.strBuyer_Secret_Hash = hashBuyerSecret.GetHex();
    obj.dexmatch.nBuyer_Valid_Height = nBuyerValidHeight;
}

bool CTemplateDexMatch::ValidateParam() const
{
    if (!IsTxSpendable(destMatch))
    {
        return false;
    }
    if (nMatchAmount <= 0)
    {
        return false;
    }
    if (dFee <= 0.0)
    {
        return false;
    }
    if (!destSellerOrder.IsTemplate() || destSellerOrder.GetTemplateId().GetType() != TEMPLATE_DEXORDER)
    {
        return false;
    }
    if (!IsTxSpendable(destSeller))
    {
        return false;
    }
    if (!IsTxSpendable(destSellerDeal))
    {
        return false;
    }
    if (nSellerValidHeight <= 0)
    {
        return false;
    }
    if (!destBuyerOrder.IsTemplate() || destBuyerOrder.GetTemplateId().GetType() != TEMPLATE_DEXORDER)
    {
        return false;
    }
    if (!IsTxSpendable(destBuyer))
    {
        return false;
    }
    if (hashBuyerSecret == 0)
    {
        return false;
    }
    if (nBuyerValidHeight <= 0)
    {
        return false;
    }
    return true;
}

bool CTemplateDexMatch::SetTemplateData(const vector<uint8>& vchDataIn)
{
    CIDataStream is(vchDataIn);
    try
    {
        is >> destMatch >> nMatchAmount >> dFee >> destSellerOrder >> destSeller >> destSellerDeal >> nSellerValidHeight >> destBuyerOrder >> destBuyer >> hashBuyerSecret >> nBuyerValidHeight;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

bool CTemplateDexMatch::SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    if (obj.strType != GetTypeName(TEMPLATE_DEXMATCH))
    {
        return false;
    }

    if (!destInstance.ParseString(obj.dexmatch.strMatch_Address))
    {
        return false;
    }
    destMatch = destInstance;

    nMatchAmount = obj.dexmatch.dMatch_Amount * COIN;
    dFee = obj.dexmatch.dFee;

    if (!destInstance.ParseString(obj.dexmatch.strSeller_Order_Address))
    {
        return false;
    }
    destSellerOrder = destInstance;

    if (!destInstance.ParseString(obj.dexmatch.strSeller_Address))
    {
        return false;
    }
    destSeller = destInstance;

    if (!destInstance.ParseString(obj.dexmatch.strSeller_Deal_Address))
    {
        return false;
    }
    destSellerDeal = destInstance;

    nSellerValidHeight = obj.dexmatch.nSeller_Valid_Height;

    if (!destInstance.ParseString(obj.dexmatch.strBuyer_Order_Address))
    {
        return false;
    }
    destBuyerOrder = destInstance;

    if (!destInstance.ParseString(obj.dexmatch.strBuyer_Address))
    {
        return false;
    }
    destBuyer = destInstance;

    if (hashBuyerSecret.SetHex(obj.dexmatch.strBuyer_Secret_Hash) != obj.dexmatch.strBuyer_Secret_Hash.size())
    {
        return false;
    }
    nBuyerValidHeight = obj.dexmatch.nBuyer_Valid_Height;
    return true;
}

void CTemplateDexMatch::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << destMatch << nMatchAmount << dFee << destSellerOrder << destSeller << destSellerDeal << nSellerValidHeight << destBuyerOrder << destBuyer << hashBuyerSecret << nBuyerValidHeight;
}

bool CTemplateDexMatch::VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                          const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const
{
    if (destTo == destMatch || destTo == destSellerDeal || destTo == destBuyer)
    {
        return destSellerDeal.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
    }
    return destSeller.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
}
