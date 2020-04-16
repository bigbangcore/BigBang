// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MPVSS_MPBOX_H
#define MPVSS_MPBOX_H

#include <map>
#include <vector>

#include "curve25519/curve25519.h"
#include "uint256.h"

class CMPSealedBox;

class CMPOpenedBox
{
public:
    CMPOpenedBox();
    CMPOpenedBox(const std::vector<uint256>& vCoeffIn, const uint256& nPrivKeyIn);
    bool IsNull() const
    {
        return (vCoeff.empty() || !nPrivKey);
    }
    bool Validate() const
    {
        for (int i = 0; i < vCoeff.size(); i++)
            if (!vCoeff[i])
                return false;
        return (!vCoeff.empty());
    }
    const uint256 PrivKey() const;
    const uint256 PubKey() const;
    const uint256 SharedKey(const uint256& pubkey) const;
    const uint256 Polynomial(std::size_t nThresh, uint32_t nX) const;
    void Signature(const uint256& hash, const uint256& r, uint256& nR, uint256& nS) const;
    bool VerifySignature(const uint256& hash, const uint256& nR, const uint256& nS) const;
    bool MakeSealedBox(CMPSealedBox& sealed, const uint256& nIdent, const uint256& r) const;

public:
    std::vector<uint256> vCoeff;
    uint256 nPrivKey;
};

class CMPSealedBox
{
public:
    CMPSealedBox();
    CMPSealedBox(const std::vector<uint256>& vEncryptedCoeffIn, const uint256& nPubKeyIn, const uint256& nRIn, const uint256& nSIn);
    bool IsNull() const
    {
        return (vEncryptedCoeff.empty() || !nPubKey);
    }
    const uint256 PubKey() const;
    bool VerifySignature(const uint256& nIdent) const;
    bool VerifySignature(const uint256& hash, const uint256& nR, const uint256& nS) const;
    bool VerifyPolynomial(uint32_t nX, const uint256& v);
    void PrecalcPolynomial(std::size_t nThresh, std::size_t nLastIndex);

public:
    std::vector<uint256> vEncryptedCoeff;
    std::vector<uint256> vEncryptedShare;
    uint256 nPubKey;
    uint256 nR;
    uint256 nS;
};

#endif // MPVSS_MPBOX_H
