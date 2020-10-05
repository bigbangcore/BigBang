// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TEMPLATE_DEXMATCH_H
#define COMMON_TEMPLATE_DEXMATCH_H

#include "template.h"

class CTemplateDexMatch : virtual public CTemplate, virtual public CSendToRecordedTemplate
{
public:
    CTemplateDexMatch(const CDestination& destMatchIn = CDestination(), int64 nMatchAmountIn = 0, int64 nFeeIn = 0,
                      const CDestination& destSellerOrderIn = CDestination(), const CDestination& destSellerIn = CDestination(),
                      const std::vector<CDestination>& vDestSellerDealIn = std::vector<CDestination>(), int nSellerValidHeightIn = 0, int nSellerSectHeightIn = 0,
                      const CDestination& destBuyerIn = CDestination(), const uint256& hashBuyerSecretIn = uint256(), int nBuyerValidHeightIn = 0);
    virtual CTemplateDexMatch* clone() const;
    virtual bool GetSignDestination(const CTransaction& tx, const uint256& hashFork, int nHeight, const std::vector<uint8>& vchSig,
                                    std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const;
    virtual void GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const;

protected:
    virtual bool ValidateParam() const;
    virtual bool SetTemplateData(const std::vector<uint8>& vchDataIn);
    virtual bool SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance);
    virtual void BuildTemplateData();
    virtual bool VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                   const std::vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const;

public:
    CDestination destMatch;
    int64 nMatchAmount;
    int64 nFee;

    CDestination destSellerOrder;
    CDestination destSeller;
    std::vector<CDestination> vDestSellerDeal;
    int nSellerValidHeight;
    int nSellerSectHeight;

    CDestination destBuyer;
    uint256 hashBuyerSecret;
    int nBuyerValidHeight;
};

#endif // COMMON_TEMPLATE_DEXMATCH_H
