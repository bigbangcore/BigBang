// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TEMPLATE_MINT_H
#define COMMON_TEMPLATE_MINT_H

#include <boost/shared_ptr.hpp>

#include "template.h"

class CTemplateMint;
typedef boost::shared_ptr<CTemplateMint> CTemplateMintPtr;

class CBlock;

class CTemplateMint : virtual public CTemplate
{
public:
    // Construct by CTemplateMint pointer.
    static const CTemplateMintPtr CreateTemplatePtr(CTemplateMint* ptr);

    // Verify block signature.
    static bool VerifyBlockSignature(const CTemplateId& nIdIn, const uint256& hash, const std::vector<uint8>& vchSig);

    // Verify block mint destination
    static bool VerifyBlockMintDestination(const CTemplateId& nIdIn, const std::vector<uint8>& vchSig, const CDestination& destMint);

    // Build block signature by concrete template.
    bool BuildBlockSignature(const uint256& hash, const std::vector<uint8>& vchPreSig, std::vector<uint8>& vchSig) const;

    virtual void GetMintTemplateData(bigbang::crypto::CPubKey& keyMintOut, CDestination& destOwnerOut) const = 0;

protected:
    // Verify block signature by concrete template.
    virtual bool VerifyBlockSignature(const uint256& hash, const std::vector<uint8>& vchSig) const = 0;
};

#endif // COMMON_TEMPLATE_MINT_H
