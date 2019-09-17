// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TEMPLATE_PROOF_H
#define COMMON_TEMPLATE_PROOF_H

#include "destination.h"
#include "mint.h"

class CTemplateProof : virtual public CTemplateMint
{
public:
    CTemplateProof(const bigbang::crypto::CPubKey keyMintIn = bigbang::crypto::CPubKey(),
                   const CDestination& destSpendIn = CDestination());
    virtual CTemplateProof* clone() const;
    virtual bool GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
                                    std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const;
    virtual void GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const;

protected:
    virtual bool ValidateParam() const;
    virtual bool SetTemplateData(const std::vector<uint8>& vchDataIn);
    virtual bool SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance);
    virtual void BuildTemplateData();
    virtual bool VerifyTxSignature(const uint256& hash, const uint256& hashAnchor, const CDestination& destTo,
                                   const std::vector<uint8>& vchSig, bool& fCompleted) const;
    virtual bool VerifyBlockSignature(const uint256& hash, const std::vector<uint8>& vchSig) const;
    virtual bool VerifyBlockSpendAddress(const CDestination& destSpendIn) const;

protected:
    bigbang::crypto::CPubKey keyMint;
    CDestination destSpend;
};

#endif // COMMON_TEMPLATE_PROOF_H
