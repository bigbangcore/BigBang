// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mint.h"

#include "block.h"
#include "template.h"

using namespace std;

//////////////////////////////
// CTemplateMint

const CTemplateMintPtr CTemplateMint::CreateTemplatePtr(CTemplateMint* ptr)
{
    return boost::dynamic_pointer_cast<CTemplateMint>(CTemplate::CreateTemplatePtr(ptr));
}

bool CTemplateMint::VerifyBlockSignature(const CTemplateId& nIdIn, const uint256& hash, const vector<uint8>& vchSig)
{
    uint16 nType = nIdIn.GetType();
    if (nType != TEMPLATE_PROOF && nType != TEMPLATE_DELEGATE)
    {
        return false;
    }

    const CTemplateMintPtr ptr = boost::dynamic_pointer_cast<CTemplateMint>(CTemplate::CreateTemplatePtr(nType, vchSig));
    if (!ptr)
    {
        return false;
    }

    vector<uint8> vchSubSig(vchSig.begin() + ptr->vchData.size(), vchSig.end());
    return ptr->VerifyBlockSignature(hash, vchSubSig);
}

// Verify block mint destination
bool CTemplateMint::VerifyBlockMintDestination(const CTemplateId& nIdIn, const std::vector<uint8>& vchSig, const CDestination& destMint)
{
    uint16 nType = nIdIn.GetType();
    if (nType != TEMPLATE_PROOF && nType != TEMPLATE_DELEGATE)
    {
        return false;
    }
    if (nType == TEMPLATE_DELEGATE)
    {
        const CTemplateMintPtr ptr = boost::dynamic_pointer_cast<CTemplateMint>(CTemplate::CreateTemplatePtr(nType, vchSig));
        if (!ptr)
        {
            return false;
        }
        bigbang::crypto::CPubKey keyMintOut;
        CDestination destOwnerOut;
        ptr->GetMintTemplateData(keyMintOut, destOwnerOut);
        if (destMint != CDestination(keyMintOut))
        {
            return false;
        }
    }
    else
    {
        if (destMint != CDestination(nIdIn))
        {
            return false;
        }
    }
    return true;
}

bool CTemplateMint::BuildBlockSignature(const uint256& hash, const std::vector<uint8>& vchPreSig, std::vector<uint8>& vchSig) const
{
    vchSig = vchData;

    if (!VerifyBlockSignature(hash, vchPreSig))
    {
        return false;
    }

    vchSig.insert(vchSig.end(), vchPreSig.begin(), vchPreSig.end());
    return true;
}
