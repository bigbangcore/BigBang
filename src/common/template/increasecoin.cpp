// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "increasecoin.h"

#include "rpc/auto_protocol.h"
#include "template.h"
#include "transaction.h"
#include "util.h"

using namespace std;
using namespace xengine;
using namespace bigbang::crypto;

//////////////////////////////
// CTemplateIncreaseCoin

CTemplateIncreaseCoin::CTemplateIncreaseCoin(int nTakeEffectHeightIn, int64 nIncreaseCoinIn, int64 nBlockRewardIn, const CDestination& destOwnerIn)
  : CTemplate(TEMPLATE_INCREASECOIN), nTakeEffectHeight(nTakeEffectHeightIn), nIncreaseCoin(nIncreaseCoinIn), nBlockReward(nBlockRewardIn), destOwner(destOwnerIn)
{
}

CTemplateIncreaseCoin* CTemplateIncreaseCoin::clone() const
{
    return new CTemplateIncreaseCoin(*this);
}

bool CTemplateIncreaseCoin::GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
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

void CTemplateIncreaseCoin::GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.increasecoin.nHeight = nTakeEffectHeight;
    obj.increasecoin.dAmount = CTokenCoin::ValueFromAmount(nIncreaseCoin);
    obj.increasecoin.dMint = CTokenCoin::ValueFromAmount(nBlockReward);
    obj.increasecoin.strOwner = (destInstance = destOwner).ToString();
}

void CTemplateIncreaseCoin::GetIncreaseCoinParam(int& nTakeEffectHeightOut, int64& nIncreaseCoinOut, int64& nBlockRewardOut, CDestination& destOwnerOut) const
{
    nTakeEffectHeightOut = nTakeEffectHeight;
    nIncreaseCoinOut = nIncreaseCoin;
    nBlockRewardOut = nBlockReward;
    destOwnerOut = destOwner;
}

bool CTemplateIncreaseCoin::ValidateParam() const
{
    if (nTakeEffectHeight <= 0)
    {
        StdError("CTemplateIncreaseCoin", "ValidateParam: nTakeEffectHeight error, nTakeEffectHeight: %d", nTakeEffectHeight);
        return false;
    }
    if (!CTokenCoin::MoneyRange(nIncreaseCoin))
    {
        StdError("CTemplateIncreaseCoin", "ValidateParam: nIncreaseCoin error, nIncreaseCoin: %d", nIncreaseCoin);
        return false;
    }
    if (!CTokenCoin::RewardRange(nBlockReward))
    {
        StdError("CTemplateIncreaseCoin", "ValidateParam: nBlockReward error, nBlockReward: %d", nBlockReward);
        return false;
    }
    if (!IsTxSpendable(destOwner))
    {
        StdError("CTemplateIncreaseCoin", "ValidateParam: destOwner error");
        return false;
    }
    return true;
}

bool CTemplateIncreaseCoin::SetTemplateData(const std::vector<uint8>& vchDataIn)
{
    CIDataStream is(vchDataIn);
    try
    {
        is >> nTakeEffectHeight >> nIncreaseCoin >> nBlockReward >> destOwner;
    }
    catch (const std::exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

bool CTemplateIncreaseCoin::SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    if (obj.strType != GetTypeName(TEMPLATE_INCREASECOIN))
    {
        StdError("CTemplateIncreaseCoin", "SetTemplateData: GetTypeName error, strType: %s", obj.strType.c_str());
        return false;
    }
    nTakeEffectHeight = obj.increasecoin.nHeight;
    nIncreaseCoin = CTokenCoin::AmountFromValue(obj.increasecoin.dAmount);
    nBlockReward = CTokenCoin::AmountFromValue(obj.increasecoin.dMint);
    if (!destInstance.ParseString(obj.increasecoin.strOwner))
    {
        StdError("CTemplateIncreaseCoin", "SetTemplateData: ParseString strOwner fail, strOwner: %s", obj.increasecoin.strOwner.c_str());
        return false;
    }
    destOwner = destInstance;
    if (!ValidateParam())
    {
        StdError("CTemplateIncreaseCoin", "SetTemplateData: ValidateParam fail");
        return false;
    }
    return true;
}

void CTemplateIncreaseCoin::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << nTakeEffectHeight << nIncreaseCoin << nBlockReward << destOwner;
}

bool CTemplateIncreaseCoin::VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                              const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const
{
    return destOwner.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
}
