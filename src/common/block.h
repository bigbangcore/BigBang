// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_BLOCK_H
#define COMMON_BLOCK_H

#include <stream/datastream.h>
#include <stream/stream.h>
#include <vector>

#include "proof.h"
#include "transaction.h"
#include "uint256.h"

class CBlock
{
    friend class xengine::CStream;

public:
    uint16 nVersion;
    uint16 nType;
    uint32 nTimeStamp;
    uint256 hashPrev;
    uint256 hashMerkle;
    std::vector<uint8> vchProof;
    CTransaction txMint;
    std::vector<CTransaction> vtx;
    std::vector<uint8> vchSig;
    enum
    {
        BLOCK_GENESIS = 0xffff,
        BLOCK_ORIGIN = 0xff00,
        BLOCK_PRIMARY = 0x0001,
        BLOCK_SUBSIDIARY = 0x0002,
        BLOCK_EXTENDED = 0x0004,
        BLOCK_VACANT = 0x0008,
    };

public:
    CBlock()
    {
        SetNull();
    }
    void SetNull()
    {
        nVersion = 1;
        nType = 0;
        nTimeStamp = 0;
        hashPrev = 0;
        hashMerkle = 0;
        vchProof.clear();
        txMint.SetNull();
        vtx.clear();
        vchSig.clear();
    }
    bool IsNull() const
    {
        return (nType == 0 || nTimeStamp == 0 || txMint.IsNull());
    }
    bool IsGenesis() const
    {
        return (nType == BLOCK_GENESIS);
    }
    bool IsOrigin() const
    {
        return (nType >> 15);
    }
    bool IsPrimary() const
    {
        return (nType & 1);
    }
    bool IsExtended() const
    {
        return (nType == BLOCK_EXTENDED);
    }
    bool IsVacant() const
    {
        return (nType == BLOCK_VACANT);
    }
    bool IsProofOfWork() const
    {
        return (txMint.nType == CTransaction::TX_WORK);
    }
    uint256 GetHash() const
    {
        xengine::CBufStream ss;
        ss << nVersion << nType << nTimeStamp << hashPrev << hashMerkle << vchProof << txMint;
        uint256 hash = bigbang::crypto::CryptoHash(ss.GetData(), ss.GetSize());
        return uint256(GetBlockHeight(), uint224(hash));
    }
    std::size_t GetTxSerializedOffset() const
    {
        return (sizeof(nVersion) + sizeof(nType) + sizeof(nTimeStamp) + sizeof(hashPrev) + sizeof(hashMerkle) + xengine::GetSerializeSize(vchProof));
    }
    void GetSerializedProofOfWorkData(std::vector<unsigned char>& vchProofOfWork) const
    {
        xengine::CBufStream ss;
        ss << nVersion << nType << nTimeStamp << hashPrev << vchProof;
        vchProofOfWork.assign(ss.GetData(), ss.GetData() + ss.GetSize());
    }
    int64 GetBlockTime() const
    {
        return (int64)nTimeStamp;
    }
    uint32 GetBlockHeight() const
    {
        if (IsGenesis())
        {
            return 0;
        }
        else if (IsExtended())
        {
            return hashPrev.Get32(7);
        }
        else
        {
            return hashPrev.Get32(7) + 1;
        }
    }
    uint64 GetBlockBeacon(int idx = 0) const
    {
        if (vchProof.empty())
        {
            return hashPrev.Get64(idx & 3);
        }
        return 0;
    }
    int64 GetBlockMint(int64 nValueIn) const
    {
        return (txMint.nAmount - nValueIn);
    }
    int64 GetBlockMint() const
    {
        int64 nTotalTxFee = 0;
        for (const CTransaction& tx : vtx)
        {
            nTotalTxFee += tx.nTxFee;
        }
        return GetBlockMint(nTotalTxFee);
    }
    uint256 BuildMerkleTree(std::vector<uint256>& vMerkleTree) const
    {
        vMerkleTree.clear();
        for (const CTransaction& tx : vtx)
            vMerkleTree.push_back(tx.GetHash());
        int j = 0;
        for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
        {
            for (int i = 0; i < nSize; i += 2)
            {
                int i2 = std::min(i + 1, nSize - 1);
                vMerkleTree.push_back(bigbang::crypto::CryptoHash(vMerkleTree[j + i], vMerkleTree[j + i2]));
            }
            j += nSize;
        }
        return (vMerkleTree.empty() ? uint64(0) : vMerkleTree.back());
    }
    uint256 CalcMerkleTreeRoot() const
    {
        std::vector<uint256> vMerkleTree;
        return BuildMerkleTree(vMerkleTree);
    }
    static uint32 GetBlockHeightByHash(const uint256& hash)
    {
        return hash.Get32(7);
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(nVersion, opt);
        s.Serialize(nType, opt);
        s.Serialize(nTimeStamp, opt);
        s.Serialize(hashPrev, opt);
        s.Serialize(hashMerkle, opt);
        s.Serialize(vchProof, opt);
        s.Serialize(txMint, opt);
        s.Serialize(vtx, opt);
        s.Serialize(vchSig, opt);
    }
};

class CBlockEx : public CBlock
{
    friend class xengine::CStream;

public:
    std::vector<CTxContxt> vTxContxt;

public:
    CBlockEx() {}
    CBlockEx(const CBlock& block, const std::vector<CTxContxt>& vTxContxtIn = std::vector<CTxContxt>())
      : CBlock(block), vTxContxt(vTxContxtIn)
    {
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        CBlock::Serialize(s, opt);
        s.Serialize(vTxContxt, opt);
    }
};

inline std::string GetBlockTypeStr(uint16 nType, uint16 nMintType)
{
    if (nType == CBlock::BLOCK_GENESIS)
        return std::string("genesis");
    if (nType == CBlock::BLOCK_ORIGIN)
        return std::string("origin");
    if (nType == CBlock::BLOCK_EXTENDED)
        return std::string("extended");
    std::string str("undefined-");
    if (nType == CBlock::BLOCK_PRIMARY)
        str = "primary-";
    if (nType == CBlock::BLOCK_SUBSIDIARY)
        str = "subsidiary-";
    if (nType == CBlock::BLOCK_VACANT)
        str = "vacant";
    if (nMintType == CTransaction::TX_WORK)
        return (str + "pow");
    if (nMintType == CTransaction::TX_STAKE)
        return (str + "dpos");
    return str;
}

class CBlockIndex
{
public:
    const uint256* phashBlock;
    CBlockIndex* pOrigin;
    CBlockIndex* pPrev;
    CBlockIndex* pNext;
    uint256 txidMint;
    uint16 nMintType;
    uint16 nVersion;
    uint16 nType;
    uint32 nTimeStamp;
    uint32 nHeight;
    uint64 nRandBeacon;
    uint256 nChainTrust;
    int64 nMoneySupply;
    uint8 nProofAlgo;
    uint8 nProofBits;
    uint32 nFile;
    uint32 nOffset;

public:
    CBlockIndex()
    {
        phashBlock = nullptr;
        pOrigin = this;
        pPrev = nullptr;
        pNext = nullptr;
        txidMint = 0;
        nMintType = 0;
        nVersion = 0;
        nType = 0;
        nTimeStamp = 0;
        nHeight = 0;
        nChainTrust = uint64(0);
        nRandBeacon = 0;
        nMoneySupply = 0;
        nProofAlgo = 0;
        nProofBits = 0;
        nFile = 0;
        nOffset = 0;
    }
    CBlockIndex(const CBlock& block, uint32 nFileIn, uint32 nOffsetIn)
    {
        phashBlock = nullptr;
        pOrigin = this;
        pPrev = nullptr;
        pNext = nullptr;
        txidMint = (block.IsVacant() ? uint64(0) : block.txMint.GetHash());
        nMintType = block.txMint.nType;
        nVersion = block.nVersion;
        nType = block.nType;
        nTimeStamp = block.nTimeStamp;
        nHeight = block.GetBlockHeight();
        nChainTrust = uint64(0);
        nMoneySupply = 0;
        nRandBeacon = 0;
        if (IsProofOfWork() && block.vchProof.size() >= CProofOfHashWorkCompact::PROOFHASHWORK_SIZE)
        {
            CProofOfHashWorkCompact proof;
            proof.Load(block.vchProof);
            nProofAlgo = proof.nAlgo;
            nProofBits = proof.nBits;
        }
        else
        {
            nProofAlgo = 0;
            nProofBits = 0;
        }
        nFile = nFileIn;
        nOffset = nOffsetIn;
    }
    uint256 GetBlockHash() const
    {
        return *phashBlock;
    }
    int GetBlockHeight() const
    {
        return nHeight;
    }
    int64 GetBlockTime() const
    {
        return (int64)nTimeStamp;
    }
    uint256 GetOriginHash() const
    {
        return pOrigin->GetBlockHash();
    }
    uint256 GetParentHash() const
    {
        return (!pOrigin->pPrev ? uint64(0) : pOrigin->pPrev->GetOriginHash());
    }
    int64 GetMoneySupply() const
    {
        return nMoneySupply;
    }
    bool IsOrigin() const
    {
        return (nType >> 15);
    }
    bool IsPrimary() const
    {
        return (nType & 1);
    }
    bool IsExtended() const
    {
        return (nType == CBlock::BLOCK_EXTENDED);
    }
    bool IsVacant() const
    {
        return (nType == CBlock::BLOCK_VACANT);
    }
    bool IsProofOfWork() const
    {
        return (nMintType == CTransaction::TX_WORK);
    }
    bool IsEquivalent(const CBlockIndex* pIndexCompare) const
    {
        if (pIndexCompare != nullptr)
        {
            const CBlockIndex* pIndex = this;
            while (pIndex)
            {
                if (pIndex == pIndexCompare)
                {
                    return true;
                }
                if (pIndex->nType != CBlock::BLOCK_VACANT
                    || pIndex->GetBlockHeight() <= pIndexCompare->GetBlockHeight())
                {
                    break;
                }
                pIndex = pIndex->pPrev;
            }
        }
        return false;
    }
    const std::string GetBlockType() const
    {
        return GetBlockTypeStr(nType, nMintType);
    }
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "CBlockIndex : hash=" << GetBlockHash().ToString()
            << " prev=" << (pPrev ? pPrev->GetBlockHash().ToString() : "nullptr")
            << " height=" << nHeight
            << " type=" << GetBlockType()
            << " time=" << nTimeStamp
            << " trust=" << nChainTrust.ToString();
        return oss.str();
    }
};

class CBlockOutline : public CBlockIndex
{
    friend class xengine::CStream;

public:
    uint256 hashBlock;
    uint256 hashPrev;
    uint256 hashOrigin;

public:
    CBlockOutline()
    {
        hashBlock = 0;
        hashPrev = 0;
        hashOrigin = 0;
    }
    CBlockOutline(const CBlockIndex* pIndex)
      : CBlockIndex(*pIndex)
    {
        hashBlock = pIndex->GetBlockHash();
        hashPrev = (pPrev ? pPrev->GetBlockHash() : uint64(0));
        hashOrigin = pOrigin->GetBlockHash();
    }
    uint256 GetBlockHash() const
    {
        return hashBlock;
    }
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "CBlockOutline : hash=" << GetBlockHash().ToString()
            << " prev=" << hashPrev.ToString()
            << " height=" << nHeight << " file=" << nFile << " offset=" << nOffset
            << " type=" << GetBlockType();
        return oss.str();
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(hashBlock, opt);
        s.Serialize(hashPrev, opt);
        s.Serialize(hashOrigin, opt);
        s.Serialize(txidMint, opt);
        s.Serialize(nMintType, opt);
        s.Serialize(nVersion, opt);
        s.Serialize(nType, opt);
        s.Serialize(nTimeStamp, opt);
        s.Serialize(nHeight, opt);
        s.Serialize(nRandBeacon, opt);
        s.Serialize(nChainTrust, opt);
        s.Serialize(nMoneySupply, opt);
        s.Serialize(nProofAlgo, opt);
        s.Serialize(nProofBits, opt);
        s.Serialize(nFile, opt);
        s.Serialize(nOffset, opt);
    }
};

class CBlockLocator
{
    friend class xengine::CStream;

public:
    CBlockLocator() {}
    virtual ~CBlockLocator() {}

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(vBlockHash, opt);
    }

public:
    std::vector<uint256> vBlockHash;
};

class CBlockChange : public CBlockEx
{
    friend class xengine::CStream;

public:
    uint8 nOperator;
    uint256 fork;
    enum
    {
        BLOCK_CHANGE_REMOVE = 0x00,
        BLOCK_CHANGE_ADD = 0x01,
    };

public:
    CBlockChange() {}
    CBlockChange(const CBlockEx& block, const uint8 nOperatorIn, const uint256& forkIn)
      : CBlockEx(block), nOperator(nOperatorIn), fork(forkIn)
    {
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(nOperator, opt);
        s.Serialize(fork, opt);
        CBlockEx::Serialize(s, opt);
    }
};

#endif //COMMON_BLOCK_H
