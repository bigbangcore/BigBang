// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TEMPLATE_DEXORDER_H
#define COMMON_TEMPLATE_DEXORDER_H

#include "template.h"

class CTemplateDexOrder : virtual public CTemplate, virtual public CSendToRecordedTemplate
{
public:
    CTemplateDexOrder(const CDestination& destSellerIn = CDestination(), const std::vector<char> vCoinPairIn = std::vector<char>(),
                      double dPriceIn = 0.0, double dFeeIn = 0.0, const uint256& hashSecretIn = uint256(), const std::vector<std::vector<uint8>>& vSecretEncIn = std::vector<std::vector<uint8>>(),
                      int nValidHeightIn = 0, int nSectHeight = 0, const std::vector<CDestination>& vDestMatchIn = std::vector<CDestination>(), const std::vector<CDestination>& vDestDealIn = std::vector<CDestination>());
    virtual CTemplateDexOrder* clone() const;
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
    CDestination destSeller;
    std::vector<char> vCoinPair;
    double dPrice;
    double dFee;
    uint256 hashSecret;
    std::vector<std::vector<uint8>> vSecretEnc;
    int nValidHeight;
    int nSectHeight;
    std::vector<CDestination> vDestMatch;
    std::vector<CDestination> vDestDeal;
};

#endif // COMMON_TEMPLATE_DEXORDER_H
