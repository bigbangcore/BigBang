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

CTemplateDexOrder::CTemplateDexOrder(const CDestination& destSellerIn, const std::vector<char> vCoinPairIn,
                                     double dPriceIn, double dFeeIn, const uint256& hashSecretIn, const std::vector<std::vector<uint8>>& vSecretEncIn,
                                     int nValidHeightIn, int nSectHeightIn, const std::vector<CDestination>& vDestMatchIn, const std::vector<CDestination>& vDestDealIn)
  : CTemplate(TEMPLATE_DEXORDER),
    destSeller(destSellerIn),
    vCoinPair(vCoinPairIn),
    dPrice(dPriceIn),
    dFee(dFeeIn),
    hashSecret(hashSecretIn),
    vSecretEnc(vSecretEncIn),
    nValidHeight(nValidHeightIn),
    nSectHeight(nSectHeightIn),
    vDestMatch(vDestMatchIn),
    vDestDeal(vDestDealIn)
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
        int nStartHeight = nValidHeight - nSectHeight * vDestMatch.size();
        for (int i = 0; i < vDestMatch.size(); ++i)
        {
            if (nHeight < nStartHeight + nSectHeight * (i + 1))
            {
                setSubDest.insert(vDestMatch[i]);
                return true;
            }
        }
        return false;
    }
    setSubDest.insert(destSeller);
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
    obj.dexorder.dPrice = dPrice;
    obj.dexorder.dFee = dFee;
    obj.dexorder.strSecret_Hash = hashSecret.GetHex();

    for (const auto& vEnc : vSecretEnc)
    {
        obj.dexorder.vecSecret_Enc.push_back(ToHexString(vEnc));
    }

    obj.dexorder.nValid_Height = nValidHeight;
    obj.dexorder.nSect_Height = nSectHeight;

    for (const auto& dest : vDestMatch)
    {
        obj.dexorder.vecMatch_Address.push_back((destInstance = dest).ToString());
    }

    for (const auto& dest : vDestDeal)
    {
        obj.dexorder.vecDeal_Address.push_back((destInstance = dest).ToString());
    }
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
    if (vSecretEnc.empty())
    {
        return false;
    }
    for (const auto& vEnc : vSecretEnc)
    {
        if (vEnc.empty())
        {
            return false;
        }
    }
    if (nValidHeight <= 0)
    {
        return false;
    }
    if (nSectHeight < TNS_DEX_MIN_SECT_HEIGHT || nSectHeight > TNS_DEX_MAX_SECT_HEIGHT)
    {
        return false;
    }

    if (vDestMatch.empty())
    {
        return false;
    }
    for (const auto& dest : vDestMatch)
    {
        if (!IsTxSpendable(dest))
        {
            return false;
        }
    }

    if (vDestDeal.empty())
    {
        return false;
    }
    for (const auto& dest : vDestDeal)
    {
        if (!IsTxSpendable(dest))
        {
            return false;
        }
    }

    if (vSecretEnc.size() != vDestDeal.size())
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
        is >> destSeller >> vCoinPair >> dPrice >> dFee >> hashSecret >> vSecretEnc >> nValidHeight >> nSectHeight >> vDestMatch >> vDestDeal;
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

    dPrice = obj.dexorder.dPrice;
    dFee = obj.dexorder.dFee;

    if (hashSecret.SetHex(obj.dexorder.strSecret_Hash) != obj.dexorder.strSecret_Hash.size())
    {
        return false;
    }
    
    for (const auto& strEnc : obj.dexorder.vecSecret_Enc)
    {
        if (strEnc.empty())
        {
            return false;
        }
        vSecretEnc.push_back(ParseHexString(strEnc));
    }

    nValidHeight = obj.dexorder.nValid_Height;
    nSectHeight = obj.dexorder.nSect_Height;

    for (const auto& strDest : obj.dexorder.vecMatch_Address)
    {
        if (!destInstance.ParseString(strDest))
        {
            return false;
        }
        vDestMatch.push_back(destInstance);
    }

    for (const auto& strDest : obj.dexorder.vecDeal_Address)
    {
        if (!destInstance.ParseString(strDest))
        {
            return false;
        }
        vDestDeal.push_back(destInstance);
    }
    return true;
}

void CTemplateDexOrder::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << destSeller << vCoinPair << dPrice << dFee << hashSecret << vSecretEnc << nValidHeight << nSectHeight << vDestMatch << vDestDeal;
}

bool CTemplateDexOrder::VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                          const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const
{
    if (nForkHeight < nValidHeight)
    {
        int nStartHeight = nValidHeight - nSectHeight * vDestMatch.size();
        for (int i = 0; i < vDestMatch.size(); ++i)
        {
            if (nForkHeight < nStartHeight + nSectHeight * (i + 1))
            {
                return vDestMatch[i].VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
            }
        }
        return false;
    }
    return destSeller.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
}
