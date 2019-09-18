// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "delegate.h"

#include "rpc/auto_protocol.h"
#include "template.h"
#include "transaction.h"
#include "util.h"

using namespace std;
using namespace xengine;
using namespace bigbang::crypto;

//////////////////////////////
// CTemplateDelegate

CTemplateDelegate::CTemplateDelegate(const bigbang::crypto::CPubKey& keyDelegateIn, const CDestination& destOwnerIn)
  : CTemplate(TEMPLATE_DELEGATE), keyDelegate(keyDelegateIn), destOwner(destOwnerIn)
{
}

CTemplateDelegate* CTemplateDelegate::clone() const
{
    return new CTemplateDelegate(*this);
}

bool CTemplateDelegate::GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
                                           std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const
{
    return false;

    if (!CTemplate::GetSignDestination(tx, vchSig, setSubDest, vchSubSig))
    {
        return false;
    }

    setSubDest.clear();
    if (tx.sendTo.GetTemplateId() == nId)
    {
        setSubDest.insert(CDestination(keyDelegate));
    }
    else
    {
        setSubDest.insert(tx.sendTo);
    }

    return true;
}

void CTemplateDelegate::GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.delegate.strDelegate = destInstance.SetPubKey(keyDelegate).ToString();
    obj.delegate.strOwner = (destInstance = destOwner).ToString();
}

bool CTemplateDelegate::BuildVssSignature(const uint256& hash, const vector<uint8>& vchDelegateSig, vector<uint8>& vchVssSig)
{
    if (!keyDelegate.Verify(hash, vchDelegateSig))
    {
        return false;
    }

    vchVssSig.clear();
    xengine::CODataStream ods(vchVssSig, CDestination::DESTINATION_SIZE + vchData.size() + vchDelegateSig.size());
    //ods << CDestination(nId) << vchData << vchDelegateSig;
    ods << CDestination(nId);
    ods.Push(&vchData[0], vchData.size());
    ods.Push(&vchDelegateSig[0], vchDelegateSig.size());

    return true;
}

bool CTemplateDelegate::ValidateParam() const
{
    if (!keyDelegate)
    {
        return false;
    }

    if (!IsTxSpendable(destOwner))
    {
        return false;
    }

    return true;
}

bool CTemplateDelegate::SetTemplateData(const std::vector<uint8>& vchDataIn)
{
    CIDataStream is(vchDataIn);
    try
    {
        is >> keyDelegate >> destOwner;
    }
    catch (const std::exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }

    return true;
}

bool CTemplateDelegate::SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    if (obj.strType != GetTypeName(TEMPLATE_DELEGATE))
    {
        return false;
    }

    if (!destInstance.SetPubKey(obj.delegate.strDelegate))
    {
        return false;
    }
    keyDelegate = destInstance.GetPubKey();

    if (!destInstance.ParseString(obj.delegate.strOwner))
    {
        return false;
    }
    destOwner = destInstance;

    return true;
}

void CTemplateDelegate::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << keyDelegate << destOwner;
}

bool CTemplateDelegate::VerifyTxSignature(const uint256& hash, const uint256& hashAnchor, const CDestination& destTo,
                                          const vector<uint8>& vchSig, bool& fCompleted) const
{
    if (destTo.GetTemplateId() == nId)
    {
        return CDestination(keyDelegate).VerifyTxSignature(hash, hashAnchor, destTo, vchSig, fCompleted);
    }
    else
    {
        return destTo.VerifyTxSignature(hash, hashAnchor, destTo, vchSig, fCompleted);
    }
}

bool CTemplateDelegate::VerifyBlockSignature(const uint256& hash, const vector<uint8>& vchSig) const
{
    return keyDelegate.Verify(hash, vchSig);
}

