// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_PROOF_H
#define COMMON_PROOF_H

#include <stream/datastream.h>

#include "destination.h"
#include "key.h"
#include "uint256.h"
#include "util.h"

class CProofOfSecretShare
{
public:
    //unsigned char nWeight;
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
        try
        {
            FromStream(is);
        }
        catch (const std::exception& e)
        {
            xengine::StdError(__PRETTY_FUNCTION__, e.what());
            return false;
        }
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
        os << nAgreement;
    }
    virtual void FromStream(xengine::CIDataStream& is)
    {
        is >> nAgreement;
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
    uint32_t nBits;
    uint64_t nNonce;

protected:
    virtual void ToStream(xengine::CODataStream& os) override
    {
        CProofOfSecretShare::ToStream(os);
        os << nBits << nNonce;
    }
    virtual void FromStream(xengine::CIDataStream& is) override
    {
        CProofOfSecretShare::FromStream(is);
        is >> nBits >> nNonce;
    }
};

class CProofOfHashWorkCompact
{
public:
    uint32_t nBits;
    uint64_t nNonce;

public:
    enum
    {
        PROOFHASHWORK_SIZE = 12
    };
    CProofOfHashWorkCompact()
      : nBits(0), nNonce(0)
    {
    }
    void Save(std::vector<unsigned char>& vchProof)
    {
        if (vchProof.size() < PROOFHASHWORK_SIZE)
        {
            return;
        }
        unsigned char* p = &vchProof[vchProof.size() - PROOFHASHWORK_SIZE];
        *((uint32_t*)p) = nBits;
        p += 4;
        *((uint64_t*)p) = nNonce;
    }
    void Load(const std::vector<unsigned char>& vchProof)
    {
        if (vchProof.size() < PROOFHASHWORK_SIZE)
        {
            return;
        }
        const unsigned char* p = &vchProof[vchProof.size() - PROOFHASHWORK_SIZE];
        nBits = *((uint32_t*)p);
        p += 4;
        nNonce = *((uint64_t*)p);
    }
};

class CConsensusParam
{
public:
    CConsensusParam()
      : nPrevTime(0), nPrevHeight(0), nPrevMintType(0), nWaitTime(0), fPow(false), ret(false) {}

    uint256 hashPrev;
    int64 nPrevTime;
    int nPrevHeight;
    uint16 nPrevMintType;
    int64 nWaitTime;
    bool fPow;
    bool ret;
};

#endif //COMMON_PROOF_H
