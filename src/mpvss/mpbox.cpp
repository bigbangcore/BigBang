// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mpbox.h"

#include <stdexcept>
#include <string>

#include "util.h"

using namespace std;
using namespace xengine;

//////////////////////////////
// CMPSealedBox

static inline const uint256 MPEccPubkey(const uint256& s)
{
    uint256 pubkey;
    CEdwards25519 P;
    P.Generate(s);
    P.Pack(pubkey.begin());
    return pubkey;
}

static inline bool MPEccPubkeyValidate(const uint256& pubkey)
{
    CEdwards25519 P;
    return (!!pubkey && P.Unpack(pubkey.begin()) && P != CEdwards25519());
}

static inline const uint256 MPEccSign(const uint256& key, const uint256& r, const uint256& hash)
{
    CSC25519 sign = CSC25519(key.begin()) + CSC25519(r.begin()) * CSC25519(hash.begin());
    return uint256(sign.Data());
}

static inline bool MPEccVerify(const uint256& pubkey, const uint256& rG, const uint256& signature, const uint256& hash)
{
    CEdwards25519 P, R, S;
    if (P.Unpack(pubkey.begin()) && R.Unpack(rG.begin()))
    {
        S.Generate(signature);
        return (S == (P + R.ScalarMult(hash)));
    }
    return false;
}

static inline const uint256 MPEccSharedKey(const uint256& key, const uint256& other)
{
    uint256 shared;
    CEdwards25519 P;
    if (P.Unpack(other.begin()))
    {
        P.ScalarMult(key).Pack(shared.begin());
    }
    return shared;
}

//////////////////////////////
// CMPOpenedBox
CMPOpenedBox::CMPOpenedBox()
{
}

CMPOpenedBox::CMPOpenedBox(const std::vector<uint256>& vCoeffIn, const uint256& nPrivKeyIn)
  : vCoeff(vCoeffIn), nPrivKey(nPrivKeyIn)
{
}

const uint256 CMPOpenedBox::PrivKey() const
{
    return nPrivKey;
}

const uint256 CMPOpenedBox::PubKey() const
{
    return MPEccPubkey(PrivKey());
}

const uint256 CMPOpenedBox::SharedKey(const uint256& pubkey) const
{
    return MPEccSharedKey(PrivKey(), pubkey);
}

const uint256 CMPOpenedBox::Polynomial(std::size_t nThresh, uint32_t nX) const
{
    if (IsNull() || nThresh > vCoeff.size())
    {
        throw runtime_error("Box is null or insufficient");
    }

    CSC25519 f = CSC25519(vCoeff[0].begin());
    for (size_t i = 1; i < nThresh; i++)
    {
        f += CSC25519(vCoeff[i].begin()) * CSC25519::naturalPowTable[nX - 1][i - 1];
    }
    return uint256(f.Data());
}

void CMPOpenedBox::Signature(const uint256& hash, const uint256& r, uint256& nR, uint256& nS) const
{
    nR = MPEccPubkey(r);
    nS = MPEccSign(PrivKey(), r, hash);
}

bool CMPOpenedBox::VerifySignature(const uint256& hash, const uint256& nR, const uint256& nS) const
{
    return MPEccVerify(PubKey(), nR, nS, hash);
}

bool CMPOpenedBox::MakeSealedBox(CMPSealedBox& sealed, const uint256& nIdent, const uint256& r) const
{
    if (!Validate() || !r)
    {
        return false;
    }

    try
    {
        sealed.vEncryptedCoeff.resize(vCoeff.size());
        for (int i = 0; i < vCoeff.size(); i++)
        {
            sealed.vEncryptedCoeff[i] = MPEccPubkey(vCoeff[i]);
        }
        sealed.nPubKey = PubKey();
        Signature(nIdent, r, sealed.nR, sealed.nS);
        return true;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
    }
    return false;
}

//////////////////////////////
// CMPSealedBox

CMPSealedBox::CMPSealedBox()
{
}

CMPSealedBox::CMPSealedBox(const vector<uint256>& vEncryptedCoeffIn, const uint256& nPubKeyIn, const uint256& nRIn, const uint256& nSIn)
  : vEncryptedCoeff(vEncryptedCoeffIn), nPubKey(nPubKeyIn), nR(nRIn), nS(nSIn)
{
}

const uint256 CMPSealedBox::PubKey() const
{
    if (IsNull())
    {
        throw runtime_error("Box is empty");
    }
    return nPubKey;
}

bool CMPSealedBox::VerifySignature(const uint256& nIdent) const
{
    if (IsNull())
    {
        return false;
    }
    for (size_t i = 0; i < vEncryptedCoeff.size(); i++)
    {
        if (!MPEccPubkeyValidate(vEncryptedCoeff[i]))
        {
            return false;
        }
    }
    return MPEccVerify(PubKey(), nR, nS, nIdent);
}

bool CMPSealedBox::VerifySignature(const uint256& hash, const uint256& nSignR, const uint256& nSignS) const
{
    if (IsNull())
    {
        return false;
    }
    return MPEccVerify(PubKey(), nSignR, nSignS, hash);
}

bool CMPSealedBox::VerifyPolynomial(uint32_t nX, const uint256& v)
{
    if (nX >= vEncryptedShare.size())
    {
        return false;
    }
    return (vEncryptedShare[nX] == MPEccPubkey(v));
}

void CMPSealedBox::PrecalcPolynomial(size_t nThresh, size_t nLastIndex)
{
    vector<CEdwards25519> vP;
    vP.resize(nThresh);
    vEncryptedShare.resize(nLastIndex);
    for (int i = 0; i < nThresh; i++)
    {
        vP[i].Unpack(vEncryptedCoeff[i].begin());
    }

    vector<pair<CSC25519, CEdwards25519>> vTerm(nThresh);
    for (uint32_t nX = 1; nX < nLastIndex; nX++)
    {
        CEdwards25519 P = vP[0];
        for (size_t i = 1; i < nThresh; i++)
        {
            P += vP[i].ScalarMult(CSC25519::naturalPowTable[nX - 1][i - 1]);
        }
        P.Pack(vEncryptedShare[nX].begin());
    }
}
