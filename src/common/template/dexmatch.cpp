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
                                     const std::vector<CDestination> vDestSellerDealIn, int nSellerValidHeightIn, int nSellerSectHeightIn,
                                     const CDestination& destBuyerOrderIn, const CDestination& destBuyerIn, const uint256& hashBuyerSecretIn, int nBuyerValidHeightIn)
  : CTemplate(TEMPLATE_DEXMATCH),
    destMatch(destMatchIn),
    nMatchAmount(nMatchAmountIn),
    dFee(dFeeIn),
    destSellerOrder(destSellerOrderIn),
    destSeller(destSellerIn),
    vDestSellerDeal(vDestSellerDealIn),
    nSellerValidHeight(nSellerValidHeightIn),
    nSellerSectHeight(nSellerSectHeightIn),
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
        int nStartHeight = nSellerValidHeight - nSellerSectHeight * vDestSellerDeal.size();
        for (int i = 0; i < vDestSellerDeal.size(); ++i)
        {
            if (nHeight < nStartHeight + nSellerSectHeight * (i + 1))
            {
                setSubDest.insert(vDestSellerDeal[i]);
                return true;
            }
        }
        return false;
    }
    setSubDest.insert(destSeller);
    return true;
}

void CTemplateDexMatch::GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.dexmatch.strMatch_Address = (destInstance = destMatch).ToString();
    obj.dexmatch.dMatch_Amount = (double)nMatchAmount / COIN;
    obj.dexmatch.dFee = dFee;

    obj.dexmatch.strSeller_Order_Address = (destInstance = destSellerOrder).ToString();
    obj.dexmatch.strSeller_Address = (destInstance = destSeller).ToString();

    for (const auto& dest : vDestSellerDeal)
    {
        obj.dexmatch.vecSeller_Deal_Address.push_back((destInstance = dest).ToString());
    }

    obj.dexmatch.nSeller_Valid_Height = nSellerValidHeight;
    obj.dexmatch.nSeller_Sect_Height = nSellerSectHeight;

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
    if (vDestSellerDeal.empty())
    {
        return false;
    }
    for (const auto& dest : vDestSellerDeal)
    {
        if (!IsTxSpendable(dest))
        {
            return false;
        }
    }
    if (nSellerValidHeight <= 0)
    {
        return false;
    }
    if (nSellerSectHeight < TNS_DEX_MIN_SECT_HEIGHT || nSellerSectHeight > TNS_DEX_MAX_SECT_HEIGHT)
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
        is >> destMatch >> nMatchAmount >> dFee >> destSellerOrder >> destSeller >> vDestSellerDeal >> nSellerValidHeight >> nSellerSectHeight >> destBuyerOrder >> destBuyer >> hashBuyerSecret >> nBuyerValidHeight;
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

    for (const auto& strDest : obj.dexmatch.vecSeller_Deal_Address)
    {
        if (!destInstance.ParseString(strDest))
        {
            return false;
        }
        vDestSellerDeal.push_back(destInstance);
    }

    nSellerValidHeight = obj.dexmatch.nSeller_Valid_Height;
    nSellerSectHeight = obj.dexmatch.nSeller_Sect_Height;

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
    os << destMatch << nMatchAmount << dFee << destSellerOrder << destSeller << vDestSellerDeal << nSellerValidHeight << nSellerSectHeight << destBuyerOrder << destBuyer << hashBuyerSecret << nBuyerValidHeight;
}

bool CTemplateDexMatch::VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                          const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const
{
    if (nForkHeight < nSellerValidHeight)
    {
        int nStartHeight = nSellerValidHeight - nSellerSectHeight * vDestSellerDeal.size();
        for (int i = 0; i < vDestSellerDeal.size(); ++i)
        {
            if (nForkHeight < nStartHeight + nSellerSectHeight * (i + 1))
            {
                return vDestSellerDeal[i].VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
            }
        }
        return false;
    }
    return destSeller.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
}
