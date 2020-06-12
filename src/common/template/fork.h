// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TEMPLATE_FORK_H
#define COMMON_TEMPLATE_FORK_H

#include "template.h"

class CTemplateFork : virtual public CTemplate
{
public:
    static int64 CreatedCoin();

public:
    CTemplateFork(const CDestination& destRedeemIn = CDestination(), const uint256& hashForkIn = uint256());
    virtual CTemplateFork* clone() const;
    virtual bool GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
                                    std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const;
    virtual void GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const;
    virtual int64 LockedCoin(const CDestination& destTo, const int32 nForkHeight, const int32 nCreatedHeight) const;
    virtual const uint256& GetHashFork() const;

protected:
    virtual bool ValidateParam() const;
    virtual bool SetTemplateData(const std::vector<uint8>& vchDataIn);
    virtual bool SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance);
    virtual void BuildTemplateData();
    virtual bool VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                   const std::vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const;

protected:
    CDestination destRedeem;
    uint256 hashFork;
};

#endif // COMMON_TEMPLATE_FORK_H
