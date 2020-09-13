// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TEMPLATE_PAYMENT_H
#define COMMON_TEMPLATE_PAYMENT_H

#include "key.h"
#include "template.h"

class CTemplatePayment;
typedef boost::shared_ptr<CTemplatePayment> CTemplatePaymentPtr;

class CTemplatePayment : virtual public CTemplate
{
public:
    static const CTemplatePaymentPtr CreateTemplatePtr(CTemplatePayment* ptr);

public:
    CTemplatePayment();

    CTemplatePayment(const std::vector<unsigned char>& vchDataIn);

    virtual CTemplatePayment* clone() const;
    virtual void GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const;

    virtual bool GetSignDestination(const CTransaction& tx, const uint256& hashFork, int nHeight, const std::vector<uint8>& vchSig,
                                    std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const;

    bool VerifyTransaction(const CTransaction& tx,
                           uint32 height,
                           std::multimap<int64, CDestination>& mapVotes,
                           const uint256& nAgreement,
                           int64 nValueIn);

protected:
    virtual bool ValidateParam() const;
    virtual bool SetTemplateData(const std::vector<uint8>& vchDataIn);
    virtual bool SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance);
    virtual void BuildTemplateData();
    virtual bool VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                   const std::vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const;

public:
    CDestination m_business;
    CDestination m_customer;
    uint32 m_height_exec;
    uint64 m_amount;
    uint64 m_pledge;
    uint32 m_height_end;
    static const int SafeHeight = 30;
    static const int DataLen = sizeof(m_business.prefix) + sizeof(m_business.data) + sizeof(m_customer.prefix) + sizeof(m_customer.data) + sizeof(m_height_exec) + sizeof(m_amount) + sizeof(m_pledge) + sizeof(m_height_end);
};

#endif