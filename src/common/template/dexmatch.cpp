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

CTemplateDexMatch::CTemplateDexMatch(const CDestination& destMatchIn, const std::vector<char>& vCoinPairIn, int64 nMatchAmountIn, int nFeeIn,
                                     const uint256& hashSecretIn, const std::vector<uint8>& encSecretIn,
                                     const CDestination& destSellerOrderIn, const CDestination& destSellerIn,
                                     const CDestination& destSellerDealIn, int nSellerValidHeightIn,
                                     const CDestination& destBuyerIn, const uint256& hashBuyerSecretIn, int nBuyerValidHeightIn)
  : CTemplate(TEMPLATE_DEXMATCH),
    destMatch(destMatchIn),
    vCoinPair(vCoinPairIn),
    nMatchAmount(nMatchAmountIn),
    nFee(nFeeIn),
    hashSecret(hashSecretIn),
    encSecret(encSecretIn),
    destSellerOrder(destSellerOrderIn),
    destSeller(destSellerIn),
    destSellerDeal(destSellerDealIn),
    nSellerValidHeight(nSellerValidHeightIn),
    destBuyer(destBuyerIn),
    hashBuyerSecret(hashBuyerSecretIn),
    nBuyerValidHeight(nBuyerValidHeightIn)
{
}

CTemplateDexMatch* CTemplateDexMatch::clone() const
{
    return new CTemplateDexMatch(*this);
}

bool CTemplateDexMatch::GetSignDestination(const CTransaction& tx, const uint256& hashFork, int nHeight, const vector<uint8>& vchSig,
                                           set<CDestination>& setSubDest, vector<uint8>& vchSubSig) const
{
    if (!CTemplate::GetSignDestination(tx, hashFork, nHeight, vchSig, setSubDest, vchSubSig))
    {
        return false;
    }
    setSubDest.clear();
    if (nHeight < nSellerValidHeight)
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
    if (!vCoinPair.empty())
    {
        std::string strCoinPairTemp;
        strCoinPairTemp.assign(&(vCoinPair[0]), vCoinPair.size());
        obj.dexmatch.strCoinpair = strCoinPairTemp;
    }
    obj.dexmatch.dMatch_Amount = (double)nMatchAmount / COIN;
    obj.dexmatch.dFee = DoubleFromInt(nFee);

    obj.dexmatch.strSecret_Hash = hashSecret.GetHex();
    obj.dexmatch.strSecret_Enc = ToHexString(encSecret);

    obj.dexmatch.strSeller_Order_Address = (destInstance = destSellerOrder).ToString();
    obj.dexmatch.strSeller_Address = (destInstance = destSeller).ToString();
    obj.dexmatch.strSeller_Deal_Address = (destInstance = destSellerDeal).ToString();

    obj.dexmatch.nSeller_Valid_Height = nSellerValidHeight;

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
    if (vCoinPair.empty())
    {
        return false;
    }
    if (nMatchAmount <= 0)
    {
        return false;
    }
    if (nFee <= 1 || nFee >= DOUBLE_PRECISION)
    {
        return false;
    }
    if (hashSecret == 0)
    {
        return false;
    }
    if (encSecret.empty())
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
        is >> destMatch >> vCoinPair >> nMatchAmount >> nFee >> hashSecret >> encSecret >> destSellerOrder >> destSeller >> destSellerDeal >> nSellerValidHeight >> destBuyer >> hashBuyerSecret >> nBuyerValidHeight;
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

    if (obj.dexmatch.strCoinpair.empty())
    {
        return false;
    }
    vCoinPair.assign(obj.dexmatch.strCoinpair.c_str(), obj.dexmatch.strCoinpair.c_str() + obj.dexmatch.strCoinpair.size());

    nMatchAmount = (int64)(obj.dexmatch.dMatch_Amount * COIN + 0.5);
    nFee = IntFromDouble(obj.dexmatch.dFee);

    if (hashSecret.SetHex(obj.dexmatch.strSecret_Hash) != obj.dexmatch.strSecret_Hash.size())
    {
        return false;
    }

    encSecret = ParseHexString(obj.dexmatch.strSecret_Enc);

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
    os << destMatch << vCoinPair << nMatchAmount << nFee << hashSecret << encSecret << destSellerOrder << destSeller << destSellerDeal << nSellerValidHeight << destBuyer << hashBuyerSecret << nBuyerValidHeight;
}

bool CTemplateDexMatch::VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                          const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const
{
    if (nForkHeight < nSellerValidHeight)
    {
        return destSellerDeal.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
    }
    return destSeller.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
}
