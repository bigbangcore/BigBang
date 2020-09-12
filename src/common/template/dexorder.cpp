// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dexorder.h"

#include "destination.h"
#include "rpc/auto_protocol.h"
#include "template.h"
#include "transaction.h"
#include "util.h"

using namespace std;
using namespace xengine;

static const int64 COIN = 1000000;

//////////////////////////////
// CTemplateDexOrder

CTemplateDexOrder::CTemplateDexOrder(const CDestination& destSellerIn, int nCoinPairIn,
                                     double dPriceIn, double dFeeIn, const uint256& hashSecretIn, const uint256& hashEncryptionIn,
                                     int nValidHeightIn, const CDestination& destMatchIn, const CDestination& destDealIn)
  : destSeller(destSellerIn),
    nCoinPair(nCoinPairIn),
    dPrice(dPriceIn),
    dFee(dFeeIn),
    hashSecret(hashSecretIn),
    hashEncryption(hashEncryptionIn),
    nValidHeight(nValidHeightIn),
    destMatch(destMatchIn),
    destDeal(destDealIn)
{
}

CTemplateDexOrder* CTemplateDexOrder::clone() const
{
    return new CTemplateDexOrder(*this);
}

bool CTemplateDexOrder::GetSignDestination(const CTransaction& tx, const vector<uint8>& vchSig,
                                           set<CDestination>& setSubDest, vector<uint8>& vchSubSig) const
{
    if (!CTemplate::GetSignDestination(tx, vchSig, setSubDest, vchSubSig))
    {
        return false;
    }
    setSubDest.clear();
    CTemplateId tid;
    if (tx.sendTo.GetTemplateId(tid) && tid.GetType() == TEMPLATE_DEXMATCH)
    {
        setSubDest.insert(destMatch);
    }
    else
    {
        setSubDest.insert(destSeller);
    }
    return true;
}

void CTemplateDexOrder::GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.dexorder.strSeller_Address = (destInstance = destSeller).ToString();
    const CCoinPairType* pCoinPair = GetCoinPairByType(nCoinPair);
    if (pCoinPair)
    {
        obj.dexorder.strCoinpair = pCoinPair->strName;
    }
    obj.dexorder.dPrice = dPrice;
    obj.dexorder.dFee = dFee;
    obj.dexorder.strSecret_Hash = hashSecret.GetHex();
    obj.dexorder.strSecret_Enc = hashEncryption.GetHex();
    obj.dexorder.nValid_Height = nValidHeight;
    obj.dexorder.strMatch_Address = (destInstance = destMatch).ToString();
    obj.dexorder.strDeal_Address = (destInstance = destDeal).ToString();
}

bool CTemplateDexOrder::ValidateParam() const
{
    if (!IsTxSpendable(destSeller))
    {
        return false;
    }
    if (GetCoinPairByType(nCoinPair) == nullptr)
    {
        return false;
    }
    if (dPrice <= 0.0)
    {
        return false;
    }
    if (dFee <= 0.0)
    {
        return false;
    }
    if (hashSecret == 0)
    {
        return false;
    }
    if (hashEncryption == 0)
    {
        return false;
    }
    if (nValidHeight <= 0)
    {
        return false;
    }
    if (!IsTxSpendable(destMatch))
    {
        return false;
    }
    if (!IsTxSpendable(destDeal))
    {
        return false;
    }
    return true;
}

bool CTemplateDexOrder::SetTemplateData(const std::vector<uint8>& vchDataIn)
{
    CIDataStream is(vchDataIn);
    try
    {
        is >> destSeller >> nCoinPair >> dPrice >> dFee >> hashSecret >> hashEncryption >> nValidHeight >> destMatch >> destDeal;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

bool CTemplateDexOrder::SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    if (obj.strType != GetTypeName(TEMPLATE_DEXORDER))
    {
        return false;
    }

    if (!destInstance.ParseString(obj.dexorder.strSeller_Address))
    {
        return false;
    }
    destSeller = destInstance;

    string strCoinPair;
    boost::to_lower_copy(back_inserter(strCoinPair), obj.dexorder.strCoinpair.c_str());

    const CCoinPairType* pCoinPair = GetCoinPairByName(strCoinPair);
    if (pCoinPair == nullptr)
    {
        return false;
    }
    nCoinPair = pCoinPair->nType;

    dPrice = obj.dexorder.dPrice;
    dFee = obj.dexorder.dFee;

    if (hashSecret.SetHex(obj.dexorder.strSecret_Hash) != obj.dexorder.strSecret_Hash.size())
    {
        return false;
    }

    if (hashEncryption.SetHex(obj.dexorder.strSecret_Enc) != obj.dexorder.strSecret_Enc.size())
    {
        return false;
    }

    nValidHeight = obj.dexorder.nValid_Height;

    if (!destInstance.ParseString(obj.dexorder.strMatch_Address))
    {
        return false;
    }
    destMatch = destInstance;

    if (!destInstance.ParseString(obj.dexorder.strDeal_Address))
    {
        return false;
    }
    destDeal = destInstance;
    return true;
}

void CTemplateDexOrder::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << destSeller << nCoinPair << dPrice << dFee << hashSecret << hashEncryption << nValidHeight << destMatch << destDeal;
}

bool CTemplateDexOrder::VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                          const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const
{
    CTemplateId tid;
    if (destTo.GetTemplateId(tid) && tid.GetType() == TEMPLATE_DEXMATCH)
    {
        return destMatch.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
    }
    return destSeller.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
}
