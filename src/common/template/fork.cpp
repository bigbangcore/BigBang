// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "fork.h"

#include "destination.h"
#include "rpc/auto_protocol.h"
#include "template.h"
#include "util.h"

using namespace std;
using namespace xengine;

const int64 MORTGAGE_BASE = 10000000000;    // initial mortgage
const int32 MORTGAGE_DECAY_CYCLE = 525600;  // decay cycle
const double MORTGAGE_DECAY_QUANTITY = 0.5; // decay quantity

//////////////////////////////
// CTemplateFork

CTemplateFork::CTemplateFork(const CDestination& destRedeemIn, const uint256& hashForkIn)
  : CTemplate(TEMPLATE_FORK), destRedeem(destRedeemIn), hashFork(hashForkIn)
{
}

CTemplateFork* CTemplateFork::clone() const
{
    return new CTemplateFork(*this);
}

bool CTemplateFork::GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
                                       std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const
{
    if (!CTemplate::GetSignDestination(tx, vchSig, setSubDest, vchSubSig))
    {
        return false;
    }

    setSubDest.clear();
    setSubDest.insert(destRedeem);
    return true;
}

void CTemplateFork::GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.fork.strFork = hashFork.GetHex();
    obj.fork.strRedeem = (destInstance = destRedeem).ToString();
}

int64 CTemplateFork::LockedCoin(const int32 nForkHeight)
{
    return (int64)(MORTGAGE_BASE * pow(MORTGAGE_DECAY_QUANTITY, nForkHeight / MORTGAGE_DECAY_CYCLE));
}

int64 CTemplateFork::LockedCoin(const CDestination& destTo, const int32 nForkHeight) const
{
    return LockedCoin(nForkHeight);
}

bool CTemplateFork::ValidateParam() const
{
    if (!IsTxSpendable(destRedeem))
    {
        return false;
    }

    if (!hashFork)
    {
        return false;
    }

    return true;
}

bool CTemplateFork::SetTemplateData(const vector<uint8>& vchDataIn)
{
    CIDataStream is(vchDataIn);
    try
    {
        is >> destRedeem >> hashFork;
    }
    catch (exception& e)
    {
        ErrorLog(__PRETTY_FUNCTION__, e.what());
        return false;
    }

    return true;
}

bool CTemplateFork::SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    if (obj.strType != GetTypeName(TEMPLATE_FORK))
    {
        return false;
    }

    if (!destInstance.ParseString(obj.fork.strRedeem))
    {
        return false;
    }
    destRedeem = destInstance;

    if (hashFork.SetHex(obj.fork.strFork) != obj.fork.strFork.size())
    {
        return false;
    }

    return true;
}

void CTemplateFork::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << destRedeem << hashFork;
}

bool CTemplateFork::VerifyTxSignature(const uint256& hash, const uint256& hashAnchor, const CDestination& destTo,
                                      const vector<uint8>& vchSig, bool& fCompleted) const
{
    return destRedeem.VerifyTxSignature(hash, hashAnchor, destTo, vchSig, fCompleted);
}
