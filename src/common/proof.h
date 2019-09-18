// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_PROOF_H
#define COMMON_PROOF_H

#include <stream/datastream.h>

#include "destination.h"
#include "key.h"
#include "uint256.h"

class CProofOfSecretShare
{
public:
    unsigned char nWeight;
    uint256 nAgreement;

public:
    void Save(std::vector<unsigned char>& vchProof)
    {
        xengine::CODataStream os(vchProof);
        ToStream(os);
    }
    bool Load(const std::vector<unsigned char>& vchProof)
    {
        if (vchProof.size() < size())
        {
            return false;
        }
        xengine::CIDataStream is(vchProof);
        FromStream(is);
        return true;
    }
    size_t size()
    {
        std::vector<unsigned char> vchProof;
        Save(vchProof);
        return vchProof.size();
    }

protected:
    virtual void ToStream(xengine::CODataStream& os)
    {
        os << nWeight << nAgreement;
    }
    virtual void FromStream(xengine::CIDataStream& is)
    {
        is >> nWeight >> nAgreement;
    }
};

class CProofOfPiggyback : public CProofOfSecretShare
{
public:
    uint256 hashRefBlock;

protected:
    virtual void ToStream(xengine::CODataStream& os) override
    {
        CProofOfSecretShare::ToStream(os);
        os << hashRefBlock;
    }
    virtual void FromStream(xengine::CIDataStream& is) override
    {
        CProofOfSecretShare::FromStream(is);
        is >> hashRefBlock;
    }
};

class CProofOfHashWork : public CProofOfSecretShare
{
public:
    unsigned char nAlgo;
    unsigned char nBits;
    CDestination destMint;
    uint64_t nNonce;

protected:
    virtual void ToStream(xengine::CODataStream& os) override
    {
        CProofOfSecretShare::ToStream(os);
        os << nAlgo << nBits << destMint.prefix << destMint.data << nNonce;
    }
    virtual void FromStream(xengine::CIDataStream& is) override
    {
        CProofOfSecretShare::FromStream(is);
        is >> nAlgo >> nBits >> destMint.prefix >> destMint.data >> nNonce;
    }
};

class CProofOfHashWorkCompact
{
public:
    unsigned char nAlgo;
    unsigned char nBits;
    CDestination destMint;
    uint64_t nNonce;

public:
    enum
    {
        PROOFHASHWORK_SIZE = 43
    };
    void Save(std::vector<unsigned char>& vchProof)
    {
        unsigned char* p = &vchProof[vchProof.size() - PROOFHASHWORK_SIZE];
        *p++ = nAlgo;
        *p++ = nBits;
        *p++ = destMint.prefix;
        *((uint256*)p) = destMint.data;
        p += 32;
        *((uint64_t*)p) = nNonce;
    }
    void Load(const std::vector<unsigned char>& vchProof)
    {
        const unsigned char* p = &vchProof[vchProof.size() - PROOFHASHWORK_SIZE];
        nAlgo = *p++;
        nBits = *p++;
        destMint.prefix = *p++;
        destMint.data = *((uint256*)p);
        p += 32;
        nNonce = *((uint64_t*)p);
    }
};

#endif //COMMON_PROOF_H
