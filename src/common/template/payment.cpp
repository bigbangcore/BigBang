// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "payment.h"

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../../bigbang/address.h"
#include "rpc/auto_protocol.h"
#include "stream/datastream.h"
#include "template.h"
#include "transaction.h"
#include "util.h"

using namespace std;
using namespace xengine;
using namespace bigbang::crypto;

//////////////////////////////
// CTemplatePayment

CTemplatePayment::CTemplatePayment()
  : CTemplate(TEMPLATE_PAYMENT)
{
    m_business = CDestination();
    m_customer = CDestination();
    m_height_exec = 0;
    m_height_end = 0;
    m_amount = 0;
    m_pledge = 0;
    vchData.clear();
    CODataStream os(vchData);
    os << m_business.prefix << m_business.data << m_customer.prefix << m_customer.data << m_height_exec << m_amount << m_pledge << m_height_end;
    nId = CTemplateId(nType, bigbang::crypto::CryptoHash(vchData.data(), vchData.size()));
}

CTemplatePayment::CTemplatePayment(const std::vector<unsigned char>& vchDataIn)
  : CTemplate(TEMPLATE_PAYMENT)
{
    xengine::CIDataStream is(vchDataIn);
    is >> m_business.prefix >> m_business.data >> m_customer.prefix >> m_customer.data >> m_height_exec >> m_amount >> m_pledge >> m_height_end;
    vchData.assign(vchDataIn.begin(), vchDataIn.begin() + DataLen);
    nId = CTemplateId(nType, bigbang::crypto::CryptoHash(vchData.data(), vchData.size()));
}

CTemplatePayment* CTemplatePayment::clone() const
{
    return new CTemplatePayment(*this);
}

const CTemplatePaymentPtr CTemplatePayment::CreateTemplatePtr(CTemplatePayment* ptr)
{
    return boost::dynamic_pointer_cast<CTemplatePayment>(CTemplate::CreateTemplatePtr(ptr));
}

bool CTemplatePayment::ValidateParam() const
{
    if (!m_business.IsPubKey() || !m_customer.IsPubKey())
    {
        return false;
    }
    if (m_height_exec > m_height_end || m_amount == 0)
    {
        return false;
    }
    if (m_pledge == m_amount)
    {
        return false;
    }
    if ((m_height_exec + SafeHeight) > m_height_end)
    {
        return false;
    }
    return true;
}

bool CTemplatePayment::GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
                                           std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const
{
    uint32 height;
    int64 nValueIn;
    xengine::CIDataStream ds(tx.vchSig);
    try
    {
        ds >> height >> nValueIn;
    }
    catch (const std::exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    setSubDest.clear();
    if (!m_business.IsPubKey() || !m_customer.IsPubKey())
    {
        return false;
    }
    if (height < (m_height_exec + SafeHeight))
    {
        return false;
    }
    else if (height < m_height_end)
    {
        setSubDest.insert(m_customer);
    }
    else
    {
        if (nValueIn == m_amount || nValueIn == (m_amount + m_pledge))
        {
            setSubDest.insert(m_business);
        }
        else if (nValueIn ==  m_pledge)
        {
            setSubDest.insert(m_customer);
        }
        else
        {
            return false;
        }
    }
    return true;
}

bool CTemplatePayment::SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    if (obj.strType != GetTypeName(TEMPLATE_PAYMENT))
    {
        return false;
    }

    bigbang::CAddress business(obj.payment.strBusiness);
    bigbang::CAddress customer(obj.payment.strCustomer);
    if (business.IsNull() || customer.IsNull())
    {
        return false;
    }
    m_business = business;
    m_customer = customer;

    m_height_exec = obj.payment.nHeight_Exec;
    m_height_end = obj.payment.nHeight_End;
    
    m_amount = obj.payment.nAmount;
    m_pledge = obj.payment.nPledge;
    return true;
}

void CTemplatePayment::GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.payment.strBusiness = m_business.ToString();
    obj.payment.strCustomer = m_customer.ToString();

    obj.payment.nHeight_Exec = m_height_exec;
    obj.payment.nHeight_End = m_height_end;
    obj.payment.nAmount = m_amount;
    obj.payment.nPledge = m_pledge;
}

void CTemplatePayment::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << m_business.prefix << m_business.data << m_customer.prefix << m_customer.data << m_height_exec << m_amount << m_pledge << m_height_end;
}

bool CTemplatePayment::SetTemplateData(const vector<uint8>& vchDataIn)
{
    CIDataStream is(vchDataIn);
    try
    {
        is >> m_business.prefix >> m_business.data >> m_customer.prefix >> m_customer.data >> m_height_exec >> m_amount >> m_pledge >> m_height_end;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

bool CTemplatePayment::VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                          const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const
{
    if (nForkHeight < (m_height_exec + SafeHeight))
    {
        return false;
    }
    else if (nForkHeight < m_height_end)
    {
        fCompleted = m_customer.GetPubKey().Verify(hash, vchSig);
        return fCompleted;
    }
    else
    {
        if (destTo == m_business)
        {
            fCompleted = m_business.GetPubKey().Verify(hash, vchSig);
            return fCompleted;
        }
        else if (destTo == m_customer)
        {
            fCompleted = m_customer.GetPubKey().Verify(hash, vchSig);
            return fCompleted;
        }
        else
        {
            return false;
        }
    }
}

bool CTemplatePayment::VerifyTransaction(const CTransaction& tx, uint32 height,std::multimap<int64, CDestination> &mapVotes,const uint256 &nAgreement,int64 nValueIn)
{
    if (tx.vInput.size() != 1)
    {
        return false;
    }
    if (height < (m_height_exec + SafeHeight))
    {
        return false;
    }
    else if (height < m_height_end)
    {
        if (tx.nAmount + tx.nTxFee != m_amount + m_pledge)
        {
            return false;
        }
        uint32 n = nAgreement.Get32() % mapVotes.size();
        std::vector<CDestination> votes;
        for (const auto& d : mapVotes)
        {
            votes.push_back(d.second);
        }
        return votes[n] == tx.sendTo;
    }
    else
    {
        if (tx.sendTo == m_business && (nValueIn == m_amount + m_pledge || nValueIn == m_amount))
        {
            return tx.nAmount + tx.nTxFee == m_amount;
        }
        if (tx.sendTo == m_customer && (nValueIn == m_amount + m_pledge || nValueIn == m_pledge))
        {
            return tx.nAmount + tx.nTxFee == m_pledge;
        }
        return false;
    }
}