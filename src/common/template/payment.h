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
    CTemplatePayment(
        const CDestination& business,
        const CDestination& customer,
        uint32 height_begin,
        uint32 height_exec,
        uint64 amount,
        uint64 pledge,
        uint32 height_end);

    CTemplatePayment(const std::vector<unsigned char>& vchDataIn);
    CTemplatePayment(const CDestination& business = CDestination(),
                    const CDestination& customer = CDestination(),
                    uint32 height_begin = 0,
                    uint32 height_exec = 0,
                    uint32 height_end = 0,
                    uint64 amount = 0,
                    uint64 pledge = 0);

    virtual CTemplatePayment* clone() const;
    virtual void GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const;


    virtual bool GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
                                    std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const;

    bool VerifySignature(const uint256& hash, const std::vector<uint8>& vchSig, int height, const uint256& fork);

protected:
    virtual bool ValidateParam() const;
    virtual bool SetTemplateData(const std::vector<uint8>& vchDataIn);
    virtual bool SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance);
    virtual void BuildTemplateData();

    virtual bool VerifyTxSignature(const uint256& hash, const uint256& hashAnchor, const CDestination& destTo,
                                   const std::vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const;

public:
    // 商家地址
    CDestination m_business;
    // 客户地址
    CDestination m_customer;
    // 确认23个节点的高度
    uint32 m_height_begin;
    // 确定违约资金转账到什么地方的高度
    uint32 m_height_exec;
    // 金额
    uint64 m_amount;
    // 抵押金额
    uint64 m_pledge;
    // 指定块的高度后，交易自动完成
    uint32 m_height_end;
    // 数据长度
    static const int DataLen = sizeof(m_business) + sizeof(m_customer) + sizeof(m_height_begin) + sizeof(m_height_exec) + sizeof(m_amount) + sizeof(m_pledge) + sizeof(m_height_end);
};

#endif