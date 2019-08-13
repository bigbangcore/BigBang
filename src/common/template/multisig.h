// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TEMPLATE_MULTISIG_H
#define COMMON_TEMPLATE_MULTISIG_H

#include "weighted.h"

class CTemplateMultiSig : virtual public CTemplateWeighted
{
public:
    CTemplateMultiSig(const uint8 nRequiredIn = 0, const WeightedMap& mapPubKeyWeightIn = WeightedMap());
    virtual CTemplateMultiSig* clone() const;
    virtual void GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const;

protected:
    virtual bool SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance);
};

#endif // COMMON_TEMPLATE_MULTISIG_H
