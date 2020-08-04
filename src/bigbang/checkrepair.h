// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_CHECKREPAIR_H
#define STORAGE_CHECKREPAIR_H

#include "../common/template/fork.h"
#include "address.h"
#include "block.h"
#include "blockindexdb.h"
#include "core.h"
#include "delegatecomm.h"
#include "delegateverify.h"
#include "param.h"
#include "struct.h"
#include "timeseries.h"
#include "txindexdb.h"
#include "txpooldata.h"
#include "unspentdb.h"
#include "util.h"
#include "walletdb.h"

using namespace xengine;
using namespace bigbang::storage;
using namespace boost::filesystem;
using namespace std;

namespace bigbang
{

/////////////////////////////////////////////////////////////////////////
// CCheckBlockTx

class CCheckBlockTx
{
public:
    CCheckBlockTx(const CTransaction& txIn, const CTxContxt& contxtIn, int nHeight, const uint256& hashAtForkIn, uint32 nFileNoIn, uint32 nOffsetIn)
      : tx(txIn), txContxt(contxtIn), hashAtFork(hashAtForkIn), txIndex(nHeight, nFileNoIn, nOffsetIn)
    {
    }

public:
    CTransaction tx;
    CTxContxt txContxt;
    CTxIndex txIndex;
    uint256 hashAtFork;
};

class CCheckTxOut : public CTxOut
{
public:
    uint256 txidSpent;
    CDestination destSpent;

public:
    CCheckTxOut() {}
    CCheckTxOut(const CDestination destToIn, int64 nAmountIn, uint32 nTxTimeIn, uint32 nLockUntilIn)
      : CTxOut(destToIn, nAmountIn, nTxTimeIn, nLockUntilIn) {}
    CCheckTxOut(const CTransaction& tx)
      : CTxOut(tx) {}
    CCheckTxOut(const CTransaction& tx, const CDestination& destToIn, int64 nValueIn)
      : CTxOut(tx, destToIn, nValueIn) {}
    CCheckTxOut(const CTxOut& txOut)
      : CTxOut(txOut) {}

    bool IsSpent()
    {
        return (txidSpent != 0);
    }
    void SetUnspent()
    {
        txidSpent = 0;
        destSpent.SetNull();
    }
    void SetSpent(const uint256& txidSpentIn, const CDestination& destSpentIn)
    {
        txidSpent = txidSpentIn;
        destSpent = destSpentIn;
    }

    friend bool operator==(const CCheckTxOut& a, const CCheckTxOut& b)
    {
        return (a.txidSpent == b.txidSpent && a.destSpent == b.destSpent && a.destTo == b.destTo && a.nAmount == b.nAmount && a.nTxTime == b.nTxTime && a.nLockUntil == b.nLockUntil);
    }
    friend bool operator!=(const CCheckTxOut& a, const CCheckTxOut& b)
    {
        return !(a.txidSpent == b.txidSpent && a.destSpent == b.destSpent && a.destTo == b.destTo && a.nAmount == b.nAmount && a.nTxTime == b.nTxTime && a.nLockUntil == b.nLockUntil);
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        CTxOut::Serialize(s, opt);
        s.Serialize(txidSpent, opt);
        s.Serialize(destSpent, opt);
    }
};

class CCheckForkUnspentWalker : public CForkUnspentDBWalker
{
public:
    CCheckForkUnspentWalker() {}

    bool Walk(const CTxOutPoint& txout, const CTxOut& output) override;
    bool CheckForkUnspent(map<CTxOutPoint, CCheckTxOut>& mapBlockForkUnspent);

public:
    map<CTxOutPoint, CCheckTxOut> mapForkUnspent;

    vector<CTxUnspent> vAddUpdate;
    vector<CTxOutPoint> vRemove;
};

/////////////////////////////////////////////////////////////////////////
// CCheckForkStatus & CCheckForkManager

class CCheckForkStatus
{
public:
    CCheckForkStatus() {}

    void InsertSubline(int nHeight, const uint256& hashSubline)
    {
        auto it = mapSubline.lower_bound(nHeight);
        while (it != mapSubline.upper_bound(nHeight))
        {
            if (it->second == hashSubline)
            {
                return;
            }
            ++it;
        }
        mapSubline.insert(std::make_pair(nHeight, hashSubline));
    }

public:
    CForkContext ctxt;
    uint256 hashLastBlock;
    std::multimap<int, uint256> mapSubline;
};

class CCheckForkSchedule
{
public:
    CCheckForkSchedule() {}
    void AddNewJoint(const uint256& hashJoint, const uint256& hashFork)
    {
        std::multimap<uint256, uint256>::iterator it = mapJoint.lower_bound(hashJoint);
        while (it != mapJoint.upper_bound(hashJoint))
        {
            if (it->second == hashFork)
            {
                return;
            }
            ++it;
        }
        mapJoint.insert(std::make_pair(hashJoint, hashFork));
    }

public:
    CForkContext ctxtFork;
    std::multimap<uint256, uint256> mapJoint;
};

class CCheckValidFdForkId
{
public:
    CCheckValidFdForkId() {}
    CCheckValidFdForkId(const uint256& hashRefFdBlockIn, const std::map<uint256, int>& mapForkIdIn)
    {
        hashRefFdBlock = hashRefFdBlockIn;
        mapForkId.clear();
        mapForkId.insert(mapForkIdIn.begin(), mapForkIdIn.end());
    }
    int GetCreatedHeight(const uint256& hashFork)
    {
        const auto it = mapForkId.find(hashFork);
        if (it != mapForkId.end())
        {
            return it->second;
        }
        return -1;
    }

public:
    uint256 hashRefFdBlock;
    std::map<uint256, int> mapForkId; // When hashRefFdBlock == 0, it is the total quantity, otherwise it is the increment
};

class CCheckForkManager
{
public:
    CCheckForkManager()
      : fTestnet(false), fOnlyCheck(false) {}
    ~CCheckForkManager();

    bool SetParam(const string& strDataPathIn, bool fTestnetIn, bool fOnlyCheckIn, const uint256& hashGenesisBlockIn);
    bool FetchForkStatus();
    void GetForkList(vector<uint256>& vForkList);
    void GetTxFork(const uint256& hashFork, int nHeight, vector<uint256>& vFork);

    bool AddBlockForkContext(const CBlockEx& blockex);
    bool VerifyBlockForkTx(const uint256& hashPrev, const CTransaction& tx, vector<CForkContext>& vForkCtxt);
    bool GetTxForkRedeemParam(const CTransaction& tx, const CDestination& destIn, CDestination& destRedeem, uint256& hashFork);
    bool AddForkContext(const uint256& hashPrevBlock, const uint256& hashNewBlock, const vector<CForkContext>& vForkCtxt,
                        bool fCheckPointBlock, uint256& hashRefFdBlock, map<uint256, int>& mapValidFork);
    bool GetForkContext(const uint256& hashFork, CForkContext& ctxt);
    bool ValidateOrigin(const CBlock& block, const CProfile& parentProfile, CProfile& forkProfile);
    bool VerifyValidFork(const uint256& hashPrevBlock, const uint256& hashFork, const string& strForkName);
    bool GetValidFdForkId(const uint256& hashBlock, map<uint256, int>& mapFdForkIdOut);
    int GetValidForkCreatedHeight(const uint256& hashBlock, const uint256& hashFork);

    bool CheckDbValidFork(const uint256& hashBlock, const uint256& hashRefFdBlock, const map<uint256, int>& mapValidFork);
    bool AddDbValidForkHash(const uint256& hashBlock, const uint256& hashRefFdBlock, const map<uint256, int>& mapValidFork);
    bool AddDbForkContext(const CForkContext& ctxt);
    bool UpdateDbForkLast(const uint256& hashFork, const uint256& hashLastBlock);

    bool GetValidForkContext(const uint256& hashPrimaryLastBlock, const uint256& hashFork, CForkContext& ctxt);

public:
    string strDataPath;
    bool fTestnet;
    bool fOnlyCheck;
    uint256 hashGenesisBlock;
    CForkDB dbFork;
    map<uint256, CCheckForkStatus> mapForkStatus;
    map<int, uint256> mapCheckPoints;
    std::map<uint256, CCheckForkSchedule> mapForkSched;
    std::map<uint256, CCheckValidFdForkId> mapBlockValidFork;
};

/////////////////////////////////////////////////////////////////////////
// CCheckWalletTxWalker

class CCheckWalletTx : public CWalletTx
{
public:
    uint64 nSequenceNumber;
    uint64 nNextSequenceNumber;

public:
    CCheckWalletTx()
      : nSequenceNumber(0), nNextSequenceNumber(0) {}
    CCheckWalletTx(const CWalletTx& wtx, uint64 nSeq)
      : CWalletTx(wtx), nSequenceNumber(nSeq), nNextSequenceNumber(0) {}
};

class CWalletTxLink
{
public:
    CWalletTxLink()
      : nSequenceNumber(0), ptx(nullptr) {}
    CWalletTxLink(CCheckWalletTx* ptxin)
      : ptx(ptxin)
    {
        hashTX = ptx->txid;
        nSequenceNumber = ptx->nSequenceNumber;
    }

public:
    uint256 hashTX;
    uint64 nSequenceNumber;
    CCheckWalletTx* ptx;
};

typedef boost::multi_index_container<
    CWalletTxLink,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::member<CWalletTxLink, uint256, &CWalletTxLink::hashTX>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::member<CWalletTxLink, uint64, &CWalletTxLink::nSequenceNumber>>>>
    CWalletTxLinkSet;
typedef CWalletTxLinkSet::nth_index<0>::type CWalletTxLinkSetByTxHash;
typedef CWalletTxLinkSet::nth_index<1>::type CWalletTxLinkSetBySequenceNumber;

class CCheckWalletForkUnspent
{
public:
    CCheckWalletForkUnspent(const uint256& hashForkIn)
      : nSeqCreate(0), hashFork(hashForkIn) {}

    bool LocalTxExist(const uint256& txid);
    CCheckWalletTx* GetLocalWalletTx(const uint256& txid);
    bool AddTx(const CWalletTx& wtx);
    void RemoveTx(const uint256& txid);

    bool UpdateUnspent();
    bool AddWalletSpent(const CTxOutPoint& txPoint, const uint256& txidSpent, const CDestination& sendTo);
    bool AddWalletUnspent(const CTxOutPoint& txPoint, const CTxOut& txOut);

    int GetTxAtBlockHeight(const uint256& txid);
    bool CheckWalletUnspent(const CTxOutPoint& point, const CCheckTxOut& out);

protected:
    uint256 hashFork;
    uint64 nSeqCreate;
    CWalletTxLinkSet setWalletTxLink;

public:
    map<uint256, CCheckWalletTx> mapWalletTx;
    map<CTxOutPoint, CCheckTxOut> mapWalletUnspent;
};

class CCheckWalletTxWalker : public CWalletDBTxWalker
{
public:
    CCheckWalletTxWalker()
      : nWalletTxCount(0), pCheckForkManager(nullptr) {}

    void SetForkManager(CCheckForkManager* pFork)
    {
        pCheckForkManager = pFork;
    }

    bool Walk(const CWalletTx& wtx) override;

    bool Exist(const uint256& hashFork, const uint256& txid);
    CCheckWalletTx* GetWalletTx(const uint256& hashFork, const uint256& txid);
    bool AddWalletTx(const CWalletTx& wtx);
    void RemoveWalletTx(const uint256& hashFork, int nHeight, const uint256& txid);
    bool UpdateUnspent();
    int GetTxAtBlockHeight(const uint256& hashFork, const uint256& txid);
    bool CheckWalletUnspent(const uint256& hashFork, const CTxOutPoint& point, const CCheckTxOut& out);

protected:
    CCheckForkManager* pCheckForkManager;

public:
    int64 nWalletTxCount;
    map<uint256, CCheckWalletForkUnspent> mapWalletFork;
};

/////////////////////////////////////////////////////////////////////////
// CCheckDBAddrWalker

class CCheckDBAddrWalker : public storage::CWalletDBAddrWalker
{
public:
    CCheckDBAddrWalker() {}
    bool WalkPubkey(const crypto::CPubKey& pubkey, int version, const crypto::CCryptoCipher& cipher) override
    {
        return setAddress.insert(CDestination(pubkey)).second;
    }
    bool WalkTemplate(const CTemplateId& tid, const std::vector<unsigned char>& vchData) override
    {
        return setAddress.insert(CDestination(tid)).second;
    }
    bool CheckAddress(const CDestination& dest)
    {
        return (setAddress.find(dest) != setAddress.end());
    }

public:
    set<CDestination> setAddress;
};

/////////////////////////////////////////////////////////////////////////
// CCheckDelegateDB

class CCheckDelegateDB : public CDelegateDB
{
public:
    CCheckDelegateDB() {}
    ~CCheckDelegateDB()
    {
        Deinitialize();
    }

public:
    bool CheckDelegate(const uint256& hashBlock);
    bool UpdateDelegate(const uint256& hashBlock, CBlockEx& block, uint32 nBlockFile, uint32 nBlockOffset);
};

/////////////////////////////////////////////////////////////////////////
// CCheckTsBlock

class CCheckTsBlock : public CTimeSeriesCached
{
public:
    CCheckTsBlock() {}
    ~CCheckTsBlock()
    {
        Deinitialize();
    }
};

/////////////////////////////////////////////////////////////////////////
// CCheckBlockIndexWalker

class CCheckBlockIndexWalker : public CBlockDBWalker
{
public:
    bool Walk(CBlockOutline& outline) override
    {
        mapBlockIndex.insert(make_pair(outline.hashBlock, outline));
        return true;
    }
    bool CheckBlock(const CBlockOutline& cacheBlock)
    {
        map<uint256, CBlockOutline>::iterator it = mapBlockIndex.find(cacheBlock.hashBlock);
        if (it == mapBlockIndex.end())
        {
            StdLog("check", "Find block index fail, hash: %s", cacheBlock.hashBlock.GetHex().c_str());
            return false;
        }
        const CBlockOutline& outline = it->second;
        if (!(outline.nFile == cacheBlock.nFile && outline.nOffset == cacheBlock.nOffset))
        {
            StdLog("check", "Check block index fail, hash: %s",
                   cacheBlock.hashBlock.GetHex().c_str());
            return false;
        }
        return true;
    }
    CBlockOutline* GetBlockOutline(const uint256& hashBlock)
    {
        map<uint256, CBlockOutline>::iterator it = mapBlockIndex.find(hashBlock);
        if (it == mapBlockIndex.end())
        {
            return nullptr;
        }
        return &(it->second);
    }

public:
    map<uint256, CBlockOutline> mapBlockIndex;
};

/////////////////////////////////////////////////////////////////////////
// CCheckDbTxPool
class CCheckForkTxPool
{
public:
    CCheckForkTxPool() {}

    bool AddTx(const uint256& txid, const CAssembledTx& tx);
    bool CheckTxExist(const uint256& txid);
    bool CheckTxPoolUnspent(const CTxOutPoint& point, const CCheckTxOut& out);
    bool GetWalletTx(const uint256& hashFork, const set<CDestination>& setAddress, vector<CWalletTx>& vWalletTx);

protected:
    bool Spent(const CTxOutPoint& point, const uint256& txidSpent, const CDestination& sendTo);
    bool Unspent(const CTxOutPoint& point, const CTxOut& out);

public:
    uint256 hashFork;
    vector<pair<uint256, CAssembledTx>> vTx;
    map<uint256, CAssembledTx> mapTxPoolTx;
    map<CTxOutPoint, CCheckTxOut> mapTxPoolUnspent;
};

class CCheckTxPoolData
{
public:
    CCheckTxPoolData() {}

    void AddForkUnspent(const uint256& hashFork, const map<CTxOutPoint, CCheckTxOut>& mapUnspent);
    bool FetchTxPool(const string& strPath);
    bool CheckTxExist(const uint256& hashFork, const uint256& txid);
    bool CheckTxPoolUnspent(const uint256& hashFork, const CTxOutPoint& point, const CCheckTxOut& out);
    bool GetTxPoolWalletTx(const set<CDestination>& setAddress, vector<CWalletTx>& vWalletTx);

public:
    map<uint256, CCheckForkTxPool> mapForkTxPool;
};

/////////////////////////////////////////////////////////////////////////
// CCheckBlockFork

class CCheckBlockFork
{
public:
    CCheckBlockFork()
      : pOrigin(nullptr), pLast(nullptr) {}

    void UpdateMaxTrust(CBlockIndex* pBlockIndex);
    bool AddBlockTx(const CTransaction& txIn, const CTxContxt& contxtIn, int nHeight, const uint256& hashAtForkIn, uint32 nFileNoIn, uint32 nOffsetIn);
    bool AddBlockSpent(const CTxOutPoint& txPoint, const uint256& txidSpent, const CDestination& sendTo);
    bool AddBlockUnspent(const CTxOutPoint& txPoint, const CTxOut& txOut);
    bool CheckTxExist(const uint256& txid, int& nHeight);

public:
    CBlockIndex* pOrigin;
    CBlockIndex* pLast;
    map<uint256, CCheckBlockTx> mapBlockTx;
    map<CTxOutPoint, CCheckTxOut> mapBlockUnspent;
};

/////////////////////////////////////////////////////////////////////////
// CCheckBlockWalker
class CCheckBlockWalker : public CTSWalker<CBlockEx>
{
public:
    CCheckBlockWalker(bool fTestnetIn, bool fOnlyCheckIn)
      : nBlockCount(0), nMainChainHeight(0), nMainChainTxCount(0), objProofParam(fTestnetIn), fOnlyCheck(fOnlyCheckIn), pCheckForkManager(NULL) {}
    ~CCheckBlockWalker();

    bool Initialize(const string& strPath, CCheckForkManager* pForkMn);

    bool Walk(const CBlockEx& block, uint32 nFile, uint32 nOffset) override;

    bool GetBlockTrust(const CBlockEx& block, uint256& nChainTrust, const CBlockIndex* pIndexPrev = nullptr, const CDelegateAgreement& agreement = CDelegateAgreement(), const CBlockIndex* pIndexRef = nullptr, std::size_t nEnrollTrust = 0);
    bool GetProofOfWorkTarget(const CBlockIndex* pIndexPrev, int nAlgo, int& nBits);
    bool GetBlockDelegateAgreement(const uint256& hashBlock, const CBlock& block, CBlockIndex* pIndexPrev, CDelegateAgreement& agreement, size_t& nEnrollTrust);
    bool GetBlockDelegateEnrolled(const uint256& hashBlock, CBlockIndex* pIndex, CDelegateEnrolled& enrolled);
    bool RetrieveAvailDelegate(const uint256& hash, int height, const vector<uint256>& vBlockRange, int64 nMinEnrollAmount,
                               map<CDestination, size_t>& mapWeight, map<CDestination, vector<unsigned char>>& mapEnrollData,
                               vector<pair<CDestination, int64>>& vecAmount);

    bool UpdateBlockNext();
    bool CheckRepairFork();
    bool UpdateBlockTx();
    bool AddBlockTx(const CTransaction& txIn, const CTxContxt& contxtIn, int nHeight, const uint256& hashAtForkIn, uint32 nFileNoIn, uint32 nOffsetIn, const vector<uint256>& vFork);
    CBlockIndex* AddNewIndex(const uint256& hash, const CBlock& block, uint32 nFile, uint32 nOffset, uint256 nChainTrust);
    CBlockIndex* AddNewIndex(const uint256& hash, const CBlockOutline& objBlockOutline);
    void ClearBlockIndex();
    bool CheckTxExist(const uint256& hashFork, const uint256& txid, int& nHeight);
    bool GetBlockWalletTx(const set<CDestination>& setAddress, vector<CWalletTx>& vWalletTx);
    bool CheckBlockIndex();
    bool CheckRefBlock();

public:
    bool fOnlyCheck;
    int64 nBlockCount;
    uint32 nMainChainHeight;
    int64 nMainChainTxCount;
    uint256 hashGenesis;
    CProofOfWorkParam objProofParam;
    CCheckForkManager* pCheckForkManager;
    map<uint256, CCheckBlockFork> mapCheckFork;
    map<uint256, CBlockEx> mapBlock;
    map<uint256, CBlockIndex*> mapBlockIndex;
    CBlockIndexDB dbBlockIndex;
    CCheckBlockIndexWalker objBlockIndexWalker;
    CCheckDelegateDB objDelegateDB;
    CCheckTsBlock objTsBlock;
};

/////////////////////////////////////////////////////////////////////////
// CCheckRepairData

class CCheckRepairData
{
public:
    CCheckRepairData(const string& strPath, bool fTestnetIn, bool fOnlyCheckIn)
      : strDataPath(strPath), fTestnet(fTestnetIn), fOnlyCheck(fOnlyCheckIn),
        objBlockWalker(fTestnetIn, fOnlyCheckIn), objProofOfWorkParam(fTestnetIn) {}

protected:
    bool FetchBlockData();
    bool FetchUnspent();
    bool FetchTxPool();
    bool FetchWalletAddress();
    bool FetchWalletTx();

    bool CheckBlockUnspent();
    bool CheckWalletTx(vector<CWalletTx>& vAddTx, vector<uint256>& vRemoveTx);
    bool CheckTxIndex();

    bool RemoveTxPoolFile();
    bool RepairUnspent();
    bool RepairWalletTx(const vector<CWalletTx>& vAddTx, const vector<uint256>& vRemoveTx);
    bool RestructureWalletTx();

public:
    bool CheckRepairData();

protected:
    string strDataPath;
    bool fTestnet;
    bool fOnlyCheck;

    CProofOfWorkParam objProofOfWorkParam;
    CCheckForkManager objForkManager;
    CCheckBlockWalker objBlockWalker;
    map<uint256, CCheckForkUnspentWalker> mapForkUnspentWalker;
    CCheckDBAddrWalker objWalletAddressWalker;
    CCheckWalletTxWalker objWalletTxWalker;
    CCheckTxPoolData objTxPoolData;
};

} // namespace bigbang

#endif //STORAGE_CHECKREPAIR_H
