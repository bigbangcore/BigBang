/// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_STRUCT_H
#define BIGBANG_STRUCT_H

#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <map>
#include <set>
#include <vector>

#include "block.h"
#include "proto.h"
#include "transaction.h"
#include "uint256.h"

namespace bigbang
{

inline int64 CalcMinTxFee(const uint32 nVchData, const uint32 nMinFee)
{
    if (0 == nVchData)
    {
        return nMinFee;
    }
    uint32_t multiplier = nVchData / 200;
    if (nVchData % 200 > 0)
    {
        multiplier++;
    }
    if (multiplier > 5)
    {
        return nMinFee + 1000 + (multiplier - 5) * 400;
    }
    else
    {
        return nMinFee + multiplier * 200;
    }
}

// Status
class CForkStatus
{
public:
    CForkStatus() {}
    CForkStatus(const uint256& hashOriginIn, const uint256& hashParentIn, int nOriginHeightIn)
      : hashOrigin(hashOriginIn),
        hashParent(hashParentIn),
        nOriginHeight(nOriginHeightIn),
        nLastBlockTime(0),
        nLastBlockHeight(-1),
        nMoneySupply(0)
    {
    }

public:
    uint256 hashOrigin;
    uint256 hashParent;
    int nOriginHeight;

    uint256 hashLastBlock;
    int64 nLastBlockTime;
    int nLastBlockHeight;
    int64 nMoneySupply;
    std::multimap<int, uint256> mapSubline;
};

class CWalletBalance
{
public:
    int64 nAvailable;
    int64 nLocked;
    int64 nUnconfirmed;

public:
    void SetNull()
    {
        nAvailable = 0;
        nLocked = 0;
        nUnconfirmed = 0;
    }
};

// Notify
// 该类对象用于消息通知，通知链的最新状态。
class CBlockChainUpdate
{
public:
    CBlockChainUpdate()
    {
        SetNull();
    }
    CBlockChainUpdate(CBlockIndex* pIndex)
    {
        hashFork = pIndex->GetOriginHash();
        hashParent = pIndex->GetParentHash();
        nOriginHeight = pIndex->pOrigin->GetBlockHeight() - 1;
        hashLastBlock = pIndex->GetBlockHash();
        nLastBlockTime = pIndex->GetBlockTime();
        nLastBlockHeight = pIndex->GetBlockHeight();
        nMoneySupply = pIndex->GetMoneySupply();
    }
    void SetNull()
    {
        hashFork = 0;
        nOriginHeight = -1;
        nLastBlockHeight = -1;
    }
    bool IsNull() const
    {
        return (hashFork == 0);
    }

public:
    uint256 hashFork;
    uint256 hashParent;
    int nOriginHeight;
    uint256 hashLastBlock;
    int64 nLastBlockTime;
    int nLastBlockHeight;
    int64 nMoneySupply;
    std::set<uint256> setTxUpdate;
    std::vector<CBlockEx> vBlockAddNew;
    std::vector<CBlockEx> vBlockRemove;
};

class CTxSetChange
{
public:
    uint256 hashFork;
    std::map<uint256, int> mapTxUpdate;
    std::vector<CAssembledTx> vTxAddNew;
    std::vector<std::pair<uint256, std::vector<CTxIn>>> vTxRemove;
};

class CNetworkPeerUpdate
{
public:
    bool fActive;
    uint64 nPeerNonce;
    network::CAddress addrPeer;
};

class CTransactionUpdate
{
public:
    CTransactionUpdate()
    {
        SetNull();
    }
    void SetNull()
    {
        hashFork = 0;
    }
    bool IsNull() const
    {
        return (hashFork == 0);
    }

public:
    uint256 hashFork;
    int64 nChange;
    CTransaction txUpdate;
};

class CDelegateRoutine
{
public:
    std::vector<std::pair<uint256, std::map<CDestination, size_t>>> vEnrolledWeight;

    std::vector<CTransaction> vEnrollTx;
    std::vector<std::pair<uint256, std::map<CDestination, std::vector<unsigned char>>>> vDistributeData;
    std::map<CDestination, std::vector<unsigned char>> mapPublishData;
    uint256 hashDistributeOfPublish;
    bool fPublishCompleted;

public:
    CDelegateRoutine()
      : fPublishCompleted(false) {}
};

class CDelegateEnrolled
{
public:
    CDelegateEnrolled() {}
    void Clear()
    {
        mapWeight.clear();
        mapEnrollData.clear();
        vecAmount.clear();
    }

public:
    std::map<CDestination, std::size_t> mapWeight;                    // Delegate各个模板地址的Enroll权重
    std::map<CDestination, std::vector<unsigned char>> mapEnrollData; // Delegate各个模板地址的Enroll Data
    std::vector<std::pair<CDestination, int64>> vecAmount;            // Delegate各个模板地址的Enroll金额
};

class CDelegateAgreement
{
public:
    CDelegateAgreement()
    {
        nAgreement = 0;
        nWeight = 0;
    }
    bool IsProofOfWork() const
    {
        return (vBallot.empty());
    }
    const CDestination GetBallot(int nIndex) const
    {
        if (vBallot.empty())
        {
            return CDestination();
        }
        return vBallot[nIndex % vBallot.size()];
    }
    bool operator==(const CDelegateAgreement& other)
    {
        return (nAgreement == other.nAgreement && nWeight == other.nWeight);
    }
    void Clear()
    {
        nAgreement = 0;
        nWeight = 0;
        vBallot.clear();
    }

public:
    uint256 nAgreement;
    std::size_t nWeight;
    std::vector<CDestination> vBallot;
};

/* Protocol & Event */
class CNil
{
    friend class xengine::CStream;

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
    }
};

class CBlockMakerUpdate
{
public:
    uint256 hashBlock;
    int64 nBlockTime;
    int nBlockHeight;
    uint256 nAgreement;
    std::size_t nWeight;
    uint16 nMintType;
};

/* Net Channel */
class CPeerKnownTx
{
public:
    CPeerKnownTx() {}
    CPeerKnownTx(const uint256& txidIn)
      : txid(txidIn), nTime(xengine::GetTime()) {}

public:
    uint256 txid;
    int64 nTime;
};

typedef boost::multi_index_container<
    CPeerKnownTx,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::member<CPeerKnownTx, uint256, &CPeerKnownTx::txid>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CPeerKnownTx, int64, &CPeerKnownTx::nTime>>>>
    CPeerKnownTxSet;

typedef CPeerKnownTxSet::nth_index<0>::type CPeerKnownTxSetById;
typedef CPeerKnownTxSet::nth_index<1>::type CPeerKnownTxSetByTime;

typedef boost::multi_index_container<
    uint256,
    boost::multi_index::indexed_by<
        boost::multi_index::sequenced<>,
        boost::multi_index::ordered_unique<boost::multi_index::identity<uint256>>>>
    CUInt256List;
typedef CUInt256List::nth_index<1>::type CUInt256ByValue;

/* CStatItemBlockMaker & CStatItemP2pSyn */
class CStatItemBlockMaker
{
public:
    CStatItemBlockMaker()
      : nTimeValue(0), nPOWBlockCount(0), nDPOSBlockCount(0), nBlockTPS(0), nTxTPS(0) {}

    uint32 nTimeValue;

    uint64 nPOWBlockCount;
    uint64 nDPOSBlockCount;
    uint64 nBlockTPS;
    uint64 nTxTPS;
};

class CStatItemP2pSyn
{
public:
    CStatItemP2pSyn()
      : nRecvBlockCount(0), nRecvTxTPS(0), nSendBlockCount(0), nSendTxTPS(0), nSynRecvTxTPS(0), nSynSendTxTPS(0) {}

    uint32 nTimeValue;

    uint64 nRecvBlockCount;
    uint64 nRecvTxTPS;
    uint64 nSendBlockCount;
    uint64 nSendTxTPS;

    uint64 nSynRecvTxTPS;
    uint64 nSynSendTxTPS;
};

} // namespace bigbang

#endif // BIGBANG_STRUCT_H
