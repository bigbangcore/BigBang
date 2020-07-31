// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TRANSACTION_H
#define COMMON_TRANSACTION_H

#include <set>
#include <stream/datastream.h>
#include <stream/stream.h>

#include "crypto.h"
#include "destination.h"
#include "uint256.h"

class CTxOutPoint
{
    friend class xengine::CStream;

public:
    uint256 hash;
    uint8 n;

public:
    CTxOutPoint()
    {
        SetNull();
    }
    CTxOutPoint(uint256 hashIn, uint8 nIn)
    {
        hash = hashIn;
        n = nIn;
    }
    virtual ~CTxOutPoint() = default;
    virtual void SetNull()
    {
        hash = 0;
        n = -1;
    }
    virtual bool IsNull() const
    {
        return (hash == 0 && n == decltype(n)(-1));
    }
    friend bool operator<(const CTxOutPoint& a, const CTxOutPoint& b)
    {
        return (a.hash < b.hash || (a.hash == b.hash && a.n < b.n));
    }

    friend bool operator==(const CTxOutPoint& a, const CTxOutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const CTxOutPoint& a, const CTxOutPoint& b)
    {
        return !(a == b);
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(hash, opt);
        s.Serialize(n, opt);
    }
};

class CTxIn
{
    friend class xengine::CStream;

public:
    CTxOutPoint prevout;
    CTxIn() {}
    CTxIn(const CTxOutPoint& prevoutIn)
      : prevout(prevoutIn) {}
    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout == b.prevout);
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(prevout, opt);
    }
};

class CTransaction
{
    friend class xengine::CStream;

public:
    uint16 nVersion;
    uint16 nType;
    uint32 nTimeStamp;
    uint32 nLockUntil;
    //uint256 hashAnchor;
    std::vector<CTxIn> vInput;
    CDestination sendTo;
    int64 nAmount;
    int64 nTxFee;
    std::vector<uint8> vchData;
    std::vector<uint8> vchSig;
    enum
    {
        TX_TOKEN = 0x0000,   // normal Tx 0
        TX_CERT = 0xff00,    // Enroll Tx 65280
        TX_GENESIS = 0x0100, // 256
        TX_STAKE = 0x0200,   // DPoS mint tx 512
        TX_WORK = 0x0300     // PoW mint tx 768
    };
    CTransaction()
    {
        SetNull();
    }
    virtual ~CTransaction() = default;
    virtual void SetNull()
    {
        nVersion = 1;
        nType = 0;
        nTimeStamp = 0;
        nLockUntil = 0;
        //hashAnchor = 0;
        vInput.clear();
        sendTo.SetNull();
        nAmount = 0;
        nTxFee = 0;
        vchData.clear();
        vchSig.clear();
    }
    bool IsNull() const
    {
        return (vInput.empty() && sendTo.IsNull());
    }
    bool IsMintTx() const
    {
        return (nType == TX_GENESIS || nType == TX_STAKE || nType == TX_WORK);
    }
    std::string GetTypeString() const
    {
        if (nType == TX_TOKEN)
            return std::string("token");
        if (nType == TX_CERT)
            return std::string("certification");
        if (nType == TX_GENESIS)
            return std::string("genesis");
        if (nType == TX_STAKE)
            return std::string("stake");
        if (nType == TX_WORK)
            return std::string("work");
        return std::string("undefined");
    }
    int64 GetTxTime() const
    {
        return ((int64)nTimeStamp);
    }
    uint256 GetHash() const
    {
        xengine::CBufStream ss;
        ss << (*this);

        uint256 hash = bigbang::crypto::CryptoHash(ss.GetData(), ss.GetSize());

        return uint256(nTimeStamp, uint224(hash));
    }
    uint256 GetSignatureHash() const
    {
        xengine::CBufStream ss;
        ss << nVersion << nType << nTimeStamp << nLockUntil /*<< hashAnchor*/ << vInput << sendTo << nAmount << nTxFee << vchData;
        return bigbang::crypto::CryptoHash(ss.GetData(), ss.GetSize());
    }

    int64 GetChange(int64 nValueIn) const
    {
        return (nValueIn - nAmount - nTxFee);
    }
    uint32 GetLockUntil(const uint32 n = 0) const
    {
        if (n == (nLockUntil >> 31))
        {
            return nLockUntil & 0x7FFFFFFF;
        }
        return 0;
    }
    bool SetLockUntil(const uint32 nHeight, const uint32 n = 0)
    {
        if (nHeight >> 31)
        {
            return false;
        }
        nLockUntil = (n << 31) | nHeight;
        return true;
    }
    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return (a.nVersion == b.nVersion && a.nType == b.nType && a.nTimeStamp == b.nTimeStamp && a.nLockUntil == b.nLockUntil /*&& a.hashAnchor == b.hashAnchor*/ && a.vInput == b.vInput && a.sendTo == b.sendTo && a.nAmount == b.nAmount && a.nTxFee == b.nTxFee && a.vchData == b.vchData && a.vchSig == b.vchSig);
    }
    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return !(a == b);
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(nVersion, opt);
        s.Serialize(nType, opt);
        s.Serialize(nTimeStamp, opt);
        s.Serialize(nLockUntil, opt);
        //s.Serialize(hashAnchor, opt);
        s.Serialize(vInput, opt);
        s.Serialize(sendTo, opt);
        s.Serialize(nAmount, opt);
        s.Serialize(nTxFee, opt);
        s.Serialize(vchData, opt);
        s.Serialize(vchSig, opt);
    }
};

class CTxOut
{
    friend class xengine::CStream;

public:
    CDestination destTo;
    int64 nAmount;
    uint32 nTxTime;
    uint32 nLockUntil;

public:
    CTxOut()
    {
        SetNull();
    }
    CTxOut(const CDestination destToIn, int64 nAmountIn, uint32 nTxTimeIn, uint32 nLockUntilIn)
      : destTo(destToIn), nAmount(nAmountIn), nTxTime(nTxTimeIn), nLockUntil(nLockUntilIn) {}
    CTxOut(const CTransaction& tx)
      : destTo(tx.sendTo)
    {
        nAmount = tx.nAmount;
        nTxTime = tx.nTimeStamp;
        nLockUntil = tx.nLockUntil;
    }
    CTxOut(const CTransaction& tx, const CDestination& destToIn, int64 nValueIn)
      : destTo(destToIn)
    {
        nAmount = tx.GetChange(nValueIn);
        nTxTime = tx.nTimeStamp;
        nLockUntil = 0;
    }
    void SetNull()
    {
        destTo.SetNull();
        nAmount = 0;
        nTxTime = 0;
        nLockUntil = 0;
    }
    bool IsNull() const
    {
        return (destTo.IsNull() || nAmount <= 0);
    }
    bool IsLocked(int nBlockHeight) const
    {
        return (nBlockHeight < (nLockUntil & 0x7FFFFFFF));
    }
    int64 GetTxTime() const
    {
        return nTxTime;
    }
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "TxOutput : (" << destTo.ToString() << "," << nAmount << "," << nTxTime << "," << nLockUntil << ")";
        return oss.str();
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(destTo, opt);
        s.Serialize(nAmount, opt);
        s.Serialize(nTxTime, opt);
        s.Serialize(nLockUntil, opt);
    }
};

class CTxUnspent : public CTxOutPoint
{
public:
    CTxOut output;

public:
    CTxUnspent()
    {
        SetNull();
    }
    CTxUnspent(const CTxOutPoint& out, const CTxOut& outputIn)
      : CTxOutPoint(out), output(outputIn) {}
    virtual ~CTxUnspent() = default;
    void SetNull() override
    {
        CTxOutPoint::SetNull();
        output.SetNull();
    }
    bool IsNull() const override
    {
        return (CTxOutPoint::IsNull() || output.IsNull());
    }
};

class CAssembledTx : public CTransaction
{
    friend class xengine::CStream;

public:
    CDestination destIn;
    int64 nValueIn;
    int nBlockHeight;

public:
    CAssembledTx()
    {
        SetNull();
    }
    CAssembledTx(const CTransaction& tx, int nBlockHeightIn, const CDestination& destInIn = CDestination(), int64 nValueInIn = 0)
      : CTransaction(tx), destIn(destInIn), nValueIn(nValueInIn), nBlockHeight(nBlockHeightIn)
    {
    }
    virtual ~CAssembledTx() = default;
    void SetNull() override
    {
        CTransaction::SetNull();
        destIn.SetNull();
        nValueIn = 0;
    }
    int64 GetChange() const
    {
        return (nValueIn - nAmount - nTxFee);
    }
    const CTxOut GetOutput(int n = 0) const
    {
        if (n == 0)
        {
            return CTxOut(sendTo, nAmount, nTimeStamp, GetLockUntil(0));
        }
        else if (n == 1)
        {
            return CTxOut(destIn, GetChange(), nTimeStamp, GetLockUntil(1));
        }
        return CTxOut();
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        CTransaction::Serialize(s, opt);
        s.Serialize(destIn, opt);
        s.Serialize(nValueIn, opt);
        s.Serialize(nBlockHeight, opt);
    }
};

class CTxInContxt
{
    friend class xengine::CStream;

public:
    int64 nAmount;
    uint32 nTxTime;
    uint32 nLockUntil;

public:
    CTxInContxt()
    {
        nAmount = 0;
        nTxTime = 0;
        nLockUntil = 0;
    }
    CTxInContxt(const CTxOut& prevOutput)
    {
        nAmount = prevOutput.nAmount;
        nTxTime = prevOutput.nTxTime;
        nLockUntil = prevOutput.nLockUntil;
    }
    bool IsLocked(int nBlockHeight) const
    {
        return (nBlockHeight < (nLockUntil & 0x7FFFFFFF));
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(nAmount, opt);
        s.Serialize(nTxTime, opt);
        s.Serialize(nLockUntil, opt);
    }
};

class CTxContxt
{
    friend class xengine::CStream;

public:
    CDestination destIn;
    std::vector<CTxInContxt> vin;
    int64 GetValueIn() const
    {
        int64 nValueIn = 0;
        for (std::size_t i = 0; i < vin.size(); i++)
        {
            nValueIn += vin[i].nAmount;
        }
        return nValueIn;
    }
    void SetNull()
    {
        destIn.SetNull();
        vin.clear();
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(destIn, opt);
        s.Serialize(vin, opt);
    }
};

class CTxIndex
{
public:
    int nBlockHeight;
    uint32 nFile;
    uint32 nOffset;

public:
    CTxIndex()
    {
        SetNull();
    }
    CTxIndex(int nBlockHeightIn, uint32 nFileIn, uint32 nOffsetIn)
    {
        nBlockHeight = nBlockHeightIn;
        nFile = nFileIn;
        nOffset = nOffsetIn;
    }
    void SetNull()
    {
        nBlockHeight = 0;
        nFile = 0;
        nOffset = 0;
    }
    bool IsNull() const
    {
        return (nFile == 0);
    };
};

class CTxFilter
{
public:
    std::set<CDestination> setDest;

public:
    CTxFilter(const CDestination& destIn)
    {
        setDest.insert(destIn);
    }
    CTxFilter(const std::set<CDestination>& setDestIn)
      : setDest(setDestIn) {}
    virtual bool FoundTx(const uint256& hashFork, const CAssembledTx& tx) = 0;
};

class CTxId : public uint256
{
public:
    CTxId(const uint256& txid = uint256())
      : uint256(txid) {}
    int64 GetTxTime() const
    {
        return ((int64)Get32(7));
    }
    uint224 GetTxHash() const
    {
        return uint224(*this);
    }
};

#endif //COMMON_TRANSACTION_H
