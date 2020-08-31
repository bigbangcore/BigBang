// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TEMPLATE_SUPERCURRENCY_H
#define COMMON_TEMPLATE_SUPERCURRENCY_H

#include <boost/shared_ptr.hpp>

#include "template.h"

///////////////////////////////////////////////////////
// CTemplateIncreaseCoin

class CTokenCoin
{
public:
    static const int64 COIN = 1000000;
    static const int64 CENT = 10000;
    static const int64 MIN_TX_FEE = CENT;
    static const int64 MAX_MONEY = 1000000000000 * COIN;
    static bool MoneyRange(int64 nValue)
    {
        return (nValue >= 0 && nValue <= MAX_MONEY);
    }
    static const int64 MAX_REWARD_MONEY = 10000 * COIN;
    static bool RewardRange(int64 nValue)
    {
        return (nValue >= 0 && nValue <= MAX_REWARD_MONEY);
    }

    static int64 AmountFromValue(const double dAmount)
    {
        if (dAmount <= 0.0 || dAmount > MAX_MONEY)
        {
            return -1;
        }
        int64 nAmount = (int64)(dAmount * COIN + 0.5);
        if (!MoneyRange(nAmount))
        {
            return -1;
        }
        return nAmount;
    }

    static double ValueFromAmount(int64 amount)
    {
        return ((double)amount / (double)COIN);
    }
};

///////////////////////////////////////////////////////
// CTemplateIncreaseCoin

class CTemplateIncreaseCoin : virtual public CTemplate, virtual public CIncreaseCoinParamTemplate
{
public:
    CTemplateIncreaseCoin(int nTakeEffectHeightIn=0, int64 nIncreaseCoinIn=0, int64 nBlockRewardIn=0, const CDestination& destOwnerIn=CDestination());
    virtual CTemplateIncreaseCoin* clone() const;

    virtual bool GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
                                    std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const;
    virtual void GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const;

    virtual void GetIncreaseCoinParam(int& nTakeEffectHeightOut, int64& nIncreaseCoinOut, int64& nBlockRewardOut, CDestination& destOwnerOut) const;

protected:
    virtual bool ValidateParam() const;
    virtual bool SetTemplateData(const std::vector<uint8>& vchDataIn);
    virtual bool SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance);
    virtual void BuildTemplateData();
    virtual bool VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                   const std::vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const;

protected:
    int nTakeEffectHeight;
    int64 nIncreaseCoin;
    int64 nBlockReward;
    CDestination destOwner;
};

#endif // COMMON_TEMPLATE_SUPERCURRENCY_H
