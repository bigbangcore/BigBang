// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "template.h"

#include "json/json_spirit_reader_template.h"
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include "delegate.h"
#include "exchange.h"
#include "fork.h"
#include "multisig.h"
#include "payment.h"
#include "proof.h"
#include "rpc/auto_protocol.h"
#include "stream/datastream.h"
#include "template.h"
#include "templateid.h"
#include "transaction.h"
#include "vote.h"
#include "weighted.h"

using namespace std;
using namespace xengine;
using namespace bigbang::crypto;
using namespace bigbang::rpc;

struct CTypeInfo
{
    uint16 nType;
    CTemplate* ptr;
    std::string strName;
};

using CTypeInfoSet = boost::multi_index_container<
    CTypeInfo,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::member<CTypeInfo, uint16, &CTypeInfo::nType>>,
        boost::multi_index::ordered_unique<boost::multi_index::member<CTypeInfo, std::string, &CTypeInfo::strName>>>>;

static const CTypeInfoSet setTypeInfo = {
    { TEMPLATE_WEIGHTED, new CTemplateWeighted, "weighted" },
    { TEMPLATE_MULTISIG, new CTemplateMultiSig, "multisig" },
    { TEMPLATE_FORK, new CTemplateFork, "fork" },
    { TEMPLATE_PROOF, new CTemplateProof, "mint" },
    { TEMPLATE_DELEGATE, new CTemplateDelegate, "delegate" },
    { TEMPLATE_EXCHANGE, new CTemplateExchange, "exchange" },
    { TEMPLATE_VOTE, new CTemplateVote, "vote" },
    { TEMPLATE_PAYMENT, new CTemplatePayment, "payment" },
};

static const CTypeInfo* GetTypeInfoByType(uint16 nTypeIn)
{
    const auto& idxType = setTypeInfo.get<0>();
    auto it = idxType.find(nTypeIn);
    return (it == idxType.end()) ? nullptr : &(*it);
}

const CTypeInfo* GetTypeInfoByName(std::string strNameIn)
{
    const auto& idxName = setTypeInfo.get<1>();
    auto it = idxName.find(strNameIn);
    return (it == idxName.end()) ? nullptr : &(*it);
}

//////////////////////////////
// CTemplate
const CTemplatePtr CTemplate::CreateTemplatePtr(CTemplate* ptr)
{
    if (ptr)
    {
        if (!ptr->ValidateParam())
        {
            delete ptr;
            return nullptr;
        }
        ptr->BuildTemplateData();
        ptr->BuildTemplateId();
    }
    return CTemplatePtr(ptr);
}

const CTemplatePtr CTemplate::CreateTemplatePtr(const CDestination& destIn, const vector<uint8>& vchDataIn)
{
    return CreateTemplatePtr(destIn.GetTemplateId(), vchDataIn);
}

const CTemplatePtr CTemplate::CreateTemplatePtr(const CTemplateId& nIdIn, const vector<uint8>& vchDataIn)
{
    return CreateTemplatePtr(nIdIn.GetType(), vchDataIn);
}

const CTemplatePtr CTemplate::CreateTemplatePtr(uint16 nTypeIn, const vector<uint8>& vchDataIn)
{
    const CTypeInfo* pTypeInfo = GetTypeInfoByType(nTypeIn);
    if (!pTypeInfo)
    {
        return nullptr;
    }

    CTemplate* ptr = pTypeInfo->ptr->clone();
    if (ptr)
    {
        if (!ptr->SetTemplateData(vchDataIn) || !ptr->ValidateParam())
        {
            delete ptr;
            return nullptr;
        }
        ptr->BuildTemplateData();
        ptr->BuildTemplateId();
    }
    return CTemplatePtr(ptr);
}

const CTemplatePtr CTemplate::CreateTemplatePtr(const CTemplateRequest& obj, CDestination&& destInstance)
{
    const CTypeInfo* pTypeInfo = GetTypeInfoByName(obj.strType);
    if (!pTypeInfo)
    {
        return nullptr;
    }

    CTemplate* ptr = pTypeInfo->ptr->clone();
    if (ptr)
    {
        if (!ptr->SetTemplateData(obj, move(destInstance)) || !ptr->ValidateParam())
        {
            delete ptr;
            return nullptr;
        }
        ptr->BuildTemplateData();
        ptr->BuildTemplateId();
    }
    return CTemplatePtr(ptr);
}

const CTemplatePtr CTemplate::Import(const vector<uint8>& vchTemplateIn)
{
    if (vchTemplateIn.size() < 2)
    {
        return nullptr;
    }

    uint16 nTemplateTypeIn = vchTemplateIn[0] | (((uint16)vchTemplateIn[1]) << 8);
    vector<uint8> vchDataIn(vchTemplateIn.begin() + 2, vchTemplateIn.end());

    return CreateTemplatePtr(nTemplateTypeIn, vchDataIn);
}

bool CTemplate::IsValidType(const uint16 nTypeIn)
{
    return (nTypeIn > TEMPLATE_MIN && nTypeIn < TEMPLATE_MAX);
}

string CTemplate::GetTypeName(uint16 nTypeIn)
{
    const CTypeInfo* pTypeInfo = GetTypeInfoByType(nTypeIn);
    return pTypeInfo ? pTypeInfo->strName : "";
}

bool CTemplate::VerifyTxSignature(const CTemplateId& nIdIn, const uint16 nType, const uint256& hash, const uint256& hashAnchor,
                                  const CDestination& destTo, const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted)
{
    CTemplatePtr ptr = CreateTemplatePtr(nIdIn.GetType(), vchSig);
    if (!ptr)
    {
        return false;
    }

    if (ptr->nId != nIdIn)
    {
        return false;
    }
    vector<uint8> vchSubSig(vchSig.begin() + ptr->vchData.size(), vchSig.end());
    return ptr->VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSubSig, nForkHeight, fCompleted);
}

bool CTemplate::IsTxSpendable(const CDestination& dest)
{
    if (dest.IsPubKey())
    {
        return true;
    }
    else if (dest.IsTemplate())
    {
        uint16 nType = dest.GetTemplateId().GetType();
        const CTypeInfo* pTypeInfo = GetTypeInfoByType(nType);
        if (pTypeInfo)
        {
            return (dynamic_cast<CSpendableTemplate*>(pTypeInfo->ptr) != nullptr);
        }
    }
    return false;
}

bool CTemplate::IsDestInRecorded(const CDestination& dest)
{
    if (dest.IsTemplate())
    {
        uint16 nType = dest.GetTemplateId().GetType();
        const CTypeInfo* pTypeInfo = GetTypeInfoByType(nType);
        if (pTypeInfo)
        {
            return (dynamic_cast<CSendToRecordedTemplate*>(pTypeInfo->ptr) != nullptr);
        }
    }
    return false;
}

bool CTemplate::ParseDelegateDest(const CDestination& destIn, const CDestination& sendTo, const std::vector<uint8>& vchSigIn, CDestination& destInDelegateOut, CDestination& sendToDelegateOut)
{
    std::vector<uint8> vchSubSigOut;
    bool fSendToVoteTemplate = false;
    CTemplateId tid;
    if (sendTo.GetTemplateId(tid))
    {
        if (tid.GetType() == TEMPLATE_DELEGATE)
        {
            sendToDelegateOut = sendTo;
        }
        else if (tid.GetType() == TEMPLATE_VOTE)
        {
            CDestination destOwnerTemp;
            if (!CSendToRecordedTemplate::ParseDest(vchSigIn, sendToDelegateOut, destOwnerTemp, vchSubSigOut))
            {
                return false;
            }
            fSendToVoteTemplate = true;
        }
    }
    if (!fSendToVoteTemplate)
    {
        vchSubSigOut = std::move(vchSigIn);
    }

    if (destIn.GetTemplateId(tid))
    {
        if (tid.GetType() == TEMPLATE_DELEGATE)
        {
            destInDelegateOut = destIn;
        }
        else if (tid.GetType() == TEMPLATE_VOTE)
        {
            CTemplatePtr ptr = CTemplate::CreateTemplatePtr(destIn, vchSubSigOut);
            if (ptr == nullptr)
            {
                return false;
            }
            CDestination destOwnerTemp;
            boost::dynamic_pointer_cast<CSendToRecordedTemplate>(ptr)->GetDelegateOwnerDestination(destInDelegateOut, destOwnerTemp);
        }
    }
    return true;
}

const uint16& CTemplate::GetTemplateType() const
{
    return nType;
}

const CTemplateId& CTemplate::GetTemplateId() const
{
    return nId;
}

const vector<uint8>& CTemplate::GetTemplateData() const
{
    return vchData;
}

std::string CTemplate::GetName() const
{
    return GetTypeName(nType);
}

vector<uint8> CTemplate::Export() const
{
    vector<uint8> vchTemplate;
    vchTemplate.reserve(2 + vchData.size());
    vchTemplate.push_back((uint8)(nType & 0xff));
    vchTemplate.push_back((uint8)(nType >> 8));
    vchTemplate.insert(vchTemplate.end(), vchData.begin(), vchData.end());
    return vchTemplate;
}

bool CTemplate::GetSignDestination(const CTransaction& tx, const vector<uint8>& vchSig,
                                   set<CDestination>& setSubDest, vector<uint8>& vchSubSig) const
{
    if (!vchSig.empty())
    {
        if (vchSig.size() < vchData.size())
        {
            return false;
        }
        vchSubSig.assign(vchSig.begin() + vchData.size(), vchSig.end());
    }
    return true;
}

bool CTemplate::BuildTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor,
                                 const CDestination& destTo, const int32 nForkHeight,
                                 const vector<uint8>& vchPreSig, vector<uint8>& vchSig,
                                 bool& fCompleted) const
{
    vchSig = vchData;
    if (!VerifyTxSignature(hash, nType, hashAnchor, destTo, vchPreSig, nForkHeight, fCompleted))
    {
        return false;
    }

    return BuildTxSignature(hash, nType, hashAnchor, destTo, vchPreSig, vchSig);
}

CTemplate::CTemplate(const uint16 nTypeIn)
  : nType(nTypeIn)
{
}

bool CTemplate::VerifyTemplateData(const vector<uint8>& vchSig) const
{
    return (vchSig.size() >= vchData.size() && memcmp(&vchData[0], &vchSig[0], vchData.size()) == 0);
}

void CTemplate::BuildTemplateId()
{
    nId = CTemplateId(nType, bigbang::crypto::CryptoHash(&vchData[0], vchData.size()));
}

bool CTemplate::BuildTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                 const vector<uint8>& vchPreSig, vector<uint8>& vchSig) const
{
    vchSig.insert(vchSig.end(), vchPreSig.begin(), vchPreSig.end());
    return true;
}
