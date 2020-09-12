// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "vote.h"

#include "rpc/auto_protocol.h"
#include "template.h"
#include "transaction.h"
#include "util.h"

using namespace std;
using namespace xengine;
using namespace bigbang::crypto;

//////////////////////////////
// CTemplateVote

CTemplateVote::CTemplateVote(const CDestination& destDelegateIn, const CDestination& destOwnerIn)
  : CTemplate(TEMPLATE_VOTE), destDelegate(destDelegateIn), destOwner(destOwnerIn)
{
}

CTemplateVote* CTemplateVote::clone() const
{
    return new CTemplateVote(*this);
}

bool CTemplateVote::GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
                                       std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const
{
    if (!CTemplate::GetSignDestination(tx, vchSig, setSubDest, vchSubSig))
    {
        return false;
    }
    setSubDest.clear();
    setSubDest.insert(destOwner);
    return true;
}

void CTemplateVote::GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.vote.strDelegate = (destInstance = destDelegate).ToString();
    obj.vote.strOwner = (destInstance = destOwner).ToString();
}

bool CTemplateVote::GetDelegateOwnerDestination(CDestination& destDelegateOut, CDestination& destOwnerOut) const
{
    destDelegateOut = destDelegate;
    destOwnerOut = destOwner;
    return true;
}

bool CTemplateVote::ParseDelegateDest(const CDestination& destIn, const CDestination& sendTo, const std::vector<uint8>& vchSigIn,
                                      CDestination& destInDelegateOut, CDestination& sendToDelegateOut)
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
            auto ptr = CTemplate::CreateTemplatePtr(TEMPLATE_VOTE, vchSigIn);
            if (ptr == nullptr)
            {
                return false;
            }
            auto vote = boost::dynamic_pointer_cast<CTemplateVote>(ptr);

            CDestination destOwnerTemp;
            vote->GetDelegateOwnerDestination(sendToDelegateOut, destOwnerTemp);

            CTransaction tx;
            std::set<CDestination> setSubDest;
            if (!vote->GetSignDestination(tx, vchSigIn, setSubDest, vchSubSigOut))
            {
                return false;
            }

            fSendToVoteTemplate = true;
        }
    }

    if (destIn.GetTemplateId(tid))
    {
        if (tid.GetType() == TEMPLATE_DELEGATE)
        {
            destInDelegateOut = destIn;
        }
        else if (tid.GetType() == TEMPLATE_VOTE)
        {
            if (!fSendToVoteTemplate)
            {
                vchSubSigOut = vchSigIn;
            }
            auto ptr = CTemplate::CreateTemplatePtr(TEMPLATE_VOTE, vchSubSigOut);
            if (ptr == nullptr)
            {
                return false;
            }
            CDestination destOwnerTemp;
            boost::dynamic_pointer_cast<CTemplateVote>(ptr)->GetDelegateOwnerDestination(destInDelegateOut, destOwnerTemp);
        }
    }
    return true;
}

bool CTemplateVote::ValidateParam() const
{
    CTemplateId tid;
    if (!(destDelegate.GetTemplateId(tid) && tid.GetType() == TEMPLATE_DELEGATE))
    {
        return false;
    }
    if (!IsTxSpendable(destOwner))
    {
        return false;
    }
    return true;
}

bool CTemplateVote::SetTemplateData(const std::vector<uint8>& vchDataIn)
{
    CIDataStream is(vchDataIn);
    try
    {
        is >> destDelegate >> destOwner;
    }
    catch (const std::exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

bool CTemplateVote::SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    if (obj.strType != GetTypeName(TEMPLATE_VOTE))
    {
        return false;
    }
    if (!destInstance.ParseString(obj.vote.strDelegate))
    {
        return false;
    }
    destDelegate = destInstance;
    if (!destInstance.ParseString(obj.vote.strOwner))
    {
        return false;
    }
    destOwner = destInstance;
    return true;
}

void CTemplateVote::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << destDelegate << destOwner;
}

bool CTemplateVote::VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                      const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const
{
    return destOwner.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
}
