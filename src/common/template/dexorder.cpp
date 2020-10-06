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

//////////////////////////////
// CTemplateDexOrder

CTemplateDexOrder::CTemplateDexOrder(const CDestination& destSellerIn, const std::vector<char> vCoinPairIn,
                                     int nPriceIn, int nFeeIn, const std::vector<char>& vRecvDestIn, int nValidHeightIn,
                                     const CDestination& destMatchIn, const CDestination& destDealIn)
  : CTemplate(TEMPLATE_DEXORDER),
    destSeller(destSellerIn),
    vCoinPair(vCoinPairIn),
    nPrice(nPriceIn),
    nFee(nFeeIn),
    vRecvDest(vRecvDestIn),
    nValidHeight(nValidHeightIn),
    destMatch(destMatchIn),
    destDeal(destDealIn)
{
}

CTemplateDexOrder* CTemplateDexOrder::clone() const
{
    return new CTemplateDexOrder(*this);
}

bool CTemplateDexOrder::GetSignDestination(const CTransaction& tx, const uint256& hashFork, int nHeight, const vector<uint8>& vchSig,
                                           set<CDestination>& setSubDest, vector<uint8>& vchSubSig) const
{
    if (!CTemplate::GetSignDestination(tx, hashFork, nHeight, vchSig, setSubDest, vchSubSig))
    {
        return false;
    }
    setSubDest.clear();
    if (nHeight < nValidHeight)
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
    if (!vCoinPair.empty())
    {
        std::string strCoinPairTemp;
        strCoinPairTemp.assign(&(vCoinPair[0]), vCoinPair.size());
        obj.dexorder.strCoinpair = strCoinPairTemp;
    }
    obj.dexorder.dPrice = DoubleFromInt(nPrice);
    obj.dexorder.dFee = DoubleFromInt(nFee);

    if (!vRecvDest.empty())
    {
        std::string strRecvDestTemp;
        strRecvDestTemp.assign(&(vRecvDest[0]), vRecvDest.size());
        obj.dexorder.strRecv_Address = strRecvDestTemp;
    }

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
    if (vCoinPair.empty())
    {
        return false;
    }
    if (nPrice <= 0)
    {
        return false;
    }
    if (nFee <= 1 || nFee >= DOUBLE_PRECISION)
    {
        return false;
    }
    if (vRecvDest.empty())
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
        is >> destSeller >> vCoinPair >> nPrice >> nFee >> vRecvDest >> nValidHeight >> destMatch >> destDeal;
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

    if (obj.dexorder.strCoinpair.empty())
    {
        return false;
    }
    vCoinPair.assign(obj.dexorder.strCoinpair.c_str(), obj.dexorder.strCoinpair.c_str() + obj.dexorder.strCoinpair.size());

    nPrice = IntFromDouble(obj.dexorder.dPrice);
    nFee = IntFromDouble(obj.dexorder.dFee);

    if (obj.dexorder.strRecv_Address.empty())
    {
        return false;
    }
    vRecvDest.assign(obj.dexorder.strRecv_Address.c_str(), obj.dexorder.strRecv_Address.c_str() + obj.dexorder.strRecv_Address.size());

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
    os << destSeller << vCoinPair << nPrice << nFee << vRecvDest << nValidHeight << destMatch << destDeal;
}

bool CTemplateDexOrder::VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                          const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const
{
    if (nForkHeight < nValidHeight)
    {
        return destMatch.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
    }
    return destSeller.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
}
