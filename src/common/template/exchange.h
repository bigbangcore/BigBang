// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TEMPLATE_EXCHANGE_H
#define COMMON_TEMPLATE_EXCHANGE_H

#include "key.h"
#include "template.h"

class CTemplateExchange;
typedef boost::shared_ptr<CTemplateExchange> CTemplateExchangePtr;

class CTemplateExchange : virtual public CTemplate
{
public:
    //static int64 LockedCoin(const int32 nForkHeight);
    static const CTemplateExchangePtr CreateTemplatePtr(CTemplateExchange* ptr);

public:
    CTemplateExchange(
        const CDestination& destSpend_m_,
        const CDestination& destSpend_s_,
        int height_m_,
        int height_s_,
        const uint256& fork_m_,
        const uint256& fork_s_);

    // TODO: should remove this because of no way to catch exception by xengine::CIDataStream::operator>>() in constructor
    CTemplateExchange(const std::vector<unsigned char>& vchDataIn);
    CTemplateExchange(const CDestination& destSpend_m_ = CDestination(),
                      const CDestination& destSpend_s_ = CDestination(),
                      const uint256& fork_m_ = uint256(),
                      const uint256& fork_s_ = uint256(),
                      const int height_m_ = 0,
                      const int height_s_ = 0);

    virtual CTemplateExchange* clone() const;
    virtual bool GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
                                    std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const;
    virtual void GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const;

    virtual bool BuildTxSignature(const uint256& hash,
                                  const uint256& hashAnchor,
                                  const CDestination& destTo,
                                  const std::vector<uint8>& vchPreSig,
                                  std::vector<uint8>& vchSig) const;

    bool VerifySignature(const uint256& hash, const std::vector<uint8>& vchSig, int height, const uint256& fork);

protected:
    virtual bool ValidateParam() const;
    virtual bool SetTemplateData(const std::vector<uint8>& vchDataIn);
    virtual bool SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance);
    virtual void BuildTemplateData();

    virtual bool VerifyTxSignature(const uint256& hash, const uint256& hashAnchor, const CDestination& destTo,
                                   const std::vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const;

public:
    CDestination destSpend_m;
    CDestination destSpend_s;
    int height_m;
    int height_s;
    uint256 fork_m;
    uint256 fork_s;
};

#endif