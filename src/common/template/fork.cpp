// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "fork.h"

#include "destination.h"
#include "rpc/auto_protocol.h"
#include "template.h"
#include "util.h"

using namespace std;
using namespace xengine;

static const int64 COIN = 1000000;
const int64 MORTGAGE_BASE = 10000 * COIN;   // initial mortgage
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
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }

    return true;
}

// 设置解析模板数据，并存入模板对象
bool CTemplateFork::SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    // 校验模板类型
    if (obj.strType != GetTypeName(TEMPLATE_FORK))
    {
        return false;
    }

    // 解析并校验赎回pubkey地址
    if (!destInstance.ParseString(obj.fork.strRedeem))
    {
        return false;
    }
    // 把解析完成的地址存储赎回地址的destRedeem字段
    destRedeem = destInstance;

    // 设置make origin生成的ForkHash。strFork参数一般是make orgin的hash，也就是Origin Block的hash
    if (hashFork.SetHex(obj.fork.strFork) != obj.fork.strFork.size())
    {
        return false;
    }

    return true;
}

// 将模板数据的二进制数据写入模板vchData字段中
void CTemplateFork::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << destRedeem << hashFork;
}

bool CTemplateFork::VerifyTxSignature(const uint256& hash, const uint256& hashAnchor, const CDestination& destTo,
                                      const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const
{
    return destRedeem.VerifyTxSignature(hash, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
}
