// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TEMPLATE_WEIGHTED_H
#define COMMON_TEMPLATE_WEIGHTED_H

#include "key.h"
#include "template.h"

class CTemplateWeighted : virtual public CTemplate, virtual public CSpendableTemplate
{
public:
    CTemplateWeighted(const uint8 nRequiredIn = 0, const std::map<bigbang::crypto::CPubKey, uint8>& mapPubKeyWeightIn = std::map<bigbang::crypto::CPubKey, uint8>());
    virtual CTemplateWeighted* clone() const;
    virtual bool GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
                                    std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const;
    virtual void GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const;

protected:
    virtual bool ValidateParam() const;
    virtual bool SetTemplateData(const std::vector<uint8>& vchDataIn);
    virtual bool SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance);
    virtual void BuildTemplateData();
    virtual bool VerifyTxSignature(const uint256& hash, const uint256& hashAnchor, const CDestination& destTo,
                                   const std::vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const;

protected:
    uint8 nRequired;
    std::map<bigbang::crypto::CPubKey, uint8> mapPubKeyWeight;
};

#endif // COMMON_TEMPLATE_WEIGHTED_H
