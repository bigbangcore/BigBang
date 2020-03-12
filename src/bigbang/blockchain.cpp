// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockchain.h"

#include "delegatecomm.h"
#include "delegateverify.h"

using namespace std;
using namespace xengine;

#define ENROLLED_CACHE_COUNT (120)
#define AGREEMENT_CACHE_COUNT (16)

namespace bigbang
{

//////////////////////////////
// CBlockChain

CBlockChain::CBlockChain()
  : cacheEnrolled(ENROLLED_CACHE_COUNT), cacheAgreement(AGREEMENT_CACHE_COUNT)
{
    pCoreProtocol = nullptr;
    pTxPool = nullptr;
}

CBlockChain::~CBlockChain()
{
}

bool CBlockChain::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("txpool", pTxPool))
    {
        Error("Failed to request txpool");
        return false;
    }

    return true;
}

void CBlockChain::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pTxPool = nullptr;
}

bool CBlockChain::HandleInvoke()
{
    if (!cntrBlock.Initialize(Config()->pathData, Config()->fDebug))
    {
        Error("Failed to initialize container");
        return false;
    }

    if (!CheckContainer())
    {
        cntrBlock.Clear();
        Log("Block container is invalid,try rebuild from block storage");
        // Rebuild ...
        if (!RebuildContainer())
        {
            cntrBlock.Clear();
            Error("Failed to rebuild Block container,reconstruct all");
        }
    }

    if (cntrBlock.IsEmpty())
    {
        CBlock block;
        pCoreProtocol->GetGenesisBlock(block);
        if (!InsertGenesisBlock(block))
        {
            Error("Failed to create genesis block");
            return false;
        }
    }

    return true;
}

void CBlockChain::HandleHalt()
{
    cntrBlock.Deinitialize();
    cacheEnrolled.Clear();
    cacheAgreement.Clear();
}

void CBlockChain::GetForkStatus(map<uint256, CForkStatus>& mapForkStatus)
{
    mapForkStatus.clear();

    multimap<int, CBlockIndex*> mapForkIndex;
    cntrBlock.ListForkIndex(mapForkIndex);
    for (multimap<int, CBlockIndex*>::iterator it = mapForkIndex.begin(); it != mapForkIndex.end(); ++it)
    {
        CBlockIndex* pIndex = (*it).second;
        int nForkHeight = (*it).first;
        uint256 hashFork = pIndex->GetOriginHash();
        uint256 hashParent = pIndex->GetParentHash();

        if (hashParent != 0)
        {
            mapForkStatus[hashParent].mapSubline.insert(make_pair(nForkHeight, hashFork));
        }

        map<uint256, CForkStatus>::iterator mi = mapForkStatus.insert(make_pair(hashFork, CForkStatus(hashFork, hashParent, nForkHeight))).first;
        CForkStatus& status = (*mi).second;
        status.hashLastBlock = pIndex->GetBlockHash();
        status.nLastBlockTime = pIndex->GetBlockTime();
        status.nLastBlockHeight = pIndex->GetBlockHeight();
        status.nMoneySupply = pIndex->GetMoneySupply();
    }
}

bool CBlockChain::GetForkProfile(const uint256& hashFork, CProfile& profile)
{
    return cntrBlock.RetrieveProfile(hashFork, profile);
}

bool CBlockChain::GetForkContext(const uint256& hashFork, CForkContext& ctxt)
{
    return cntrBlock.RetrieveForkContext(hashFork, ctxt);
}

bool CBlockChain::GetForkAncestry(const uint256& hashFork, vector<pair<uint256, uint256>> vAncestry)
{
    return cntrBlock.RetrieveAncestry(hashFork, vAncestry);
}

int CBlockChain::GetBlockCount(const uint256& hashFork)
{
    int nCount = 0;
    CBlockIndex* pIndex = nullptr;
    if (cntrBlock.RetrieveFork(hashFork, &pIndex))
    {
        while (pIndex != nullptr)
        {
            pIndex = pIndex->pPrev;
            ++nCount;
        }
    }
    return nCount;
}

bool CBlockChain::GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveIndex(hashBlock, &pIndex))
    {
        return false;
    }
    hashFork = pIndex->GetOriginHash();
    nHeight = pIndex->GetBlockHeight();
    return true;
}

bool CBlockChain::GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight, uint256& hashNext)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveIndex(hashBlock, &pIndex))
    {
        return false;
    }
    hashFork = pIndex->GetOriginHash();
    nHeight = pIndex->GetBlockHeight();
    if (pIndex->pNext != nullptr)
    {
        hashNext = pIndex->pNext->GetBlockHash();
    }
    else
    {
        hashNext = 0;
    }
    return true;
}

bool CBlockChain::GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveFork(hashFork, &pIndex) || pIndex->GetBlockHeight() < nHeight)
    {
        return false;
    }
    while (pIndex != nullptr && pIndex->GetBlockHeight() > nHeight)
    {
        pIndex = pIndex->pPrev;
    }
    while (pIndex != nullptr && pIndex->GetBlockHeight() == nHeight && pIndex->IsExtended())
    {
        pIndex = pIndex->pPrev;
    }
    hashBlock = !pIndex ? uint64(0) : pIndex->GetBlockHash();
    return (pIndex != nullptr);
}

bool CBlockChain::GetBlockHash(const uint256& hashFork, int nHeight, vector<uint256>& vBlockHash)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveFork(hashFork, &pIndex) || pIndex->GetBlockHeight() < nHeight)
    {
        return false;
    }
    while (pIndex != nullptr && pIndex->GetBlockHeight() > nHeight)
    {
        pIndex = pIndex->pPrev;
    }
    while (pIndex != nullptr && pIndex->GetBlockHeight() == nHeight)
    {
        vBlockHash.push_back(pIndex->GetBlockHash());
        pIndex = pIndex->pPrev;
    }
    std::reverse(vBlockHash.begin(), vBlockHash.end());
    return (!vBlockHash.empty());
}

bool CBlockChain::GetLastBlock(const uint256& hashFork, uint256& hashBlock, int& nHeight, int64& nTime)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveFork(hashFork, &pIndex))
    {
        return false;
    }
    hashBlock = pIndex->GetBlockHash();
    nHeight = pIndex->GetBlockHeight();
    nTime = pIndex->GetBlockTime();
    return true;
}

bool CBlockChain::GetLastBlockTime(const uint256& hashFork, int nDepth, vector<int64>& vTime)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveFork(hashFork, &pIndex))
    {
        return false;
    }

    vTime.clear();
    while (nDepth > 0 && pIndex != nullptr)
    {
        vTime.push_back(pIndex->GetBlockTime());
        if (!pIndex->IsExtended())
        {
            nDepth--;
        }
        pIndex = pIndex->pPrev;
    }
    return true;
}

bool CBlockChain::GetBlock(const uint256& hashBlock, CBlock& block)
{
    return cntrBlock.Retrieve(hashBlock, block);
}

bool CBlockChain::GetBlockEx(const uint256& hashBlock, CBlockEx& block)
{
    return cntrBlock.Retrieve(hashBlock, block);
}

bool CBlockChain::GetOrigin(const uint256& hashFork, CBlock& block)
{
    return cntrBlock.RetrieveOrigin(hashFork, block);
}

bool CBlockChain::Exists(const uint256& hashBlock)
{
    return cntrBlock.Exists(hashBlock);
}

bool CBlockChain::GetTransaction(const uint256& txid, CTransaction& tx)
{
    return cntrBlock.RetrieveTx(txid, tx);
}

bool CBlockChain::ExistsTx(const uint256& txid)
{
    return cntrBlock.ExistsTx(txid);
}

bool CBlockChain::GetTxLocation(const uint256& txid, uint256& hashFork, int& nHeight)
{
    return cntrBlock.RetrieveTxLocation(txid, hashFork, nHeight);
}

bool CBlockChain::GetTxUnspent(const uint256& hashFork, const vector<CTxIn>& vInput, vector<CTxOut>& vOutput)
{
    vOutput.resize(vInput.size());
    storage::CBlockView view;
    if (!cntrBlock.GetForkBlockView(hashFork, view))
    {
        return false;
    }

    for (std::size_t i = 0; i < vInput.size(); i++)
    {
        if (vOutput[i].IsNull())
        {
            view.RetrieveUnspent(vInput[i].prevout, vOutput[i]);
        }
    }
    return true;
}

bool CBlockChain::FilterTx(const uint256& hashFork, CTxFilter& filter)
{
    return cntrBlock.FilterTx(hashFork, filter);
}

bool CBlockChain::FilterTx(const uint256& hashFork, int nDepth, CTxFilter& filter)
{
    return cntrBlock.FilterTx(hashFork, nDepth, filter);
}

bool CBlockChain::ListForkContext(vector<CForkContext>& vForkCtxt)
{
    return cntrBlock.ListForkContext(vForkCtxt);
}

Errno CBlockChain::AddNewForkContext(const CTransaction& txFork, CForkContext& ctxt)
{
    uint256 txid = txFork.GetHash();

    CBlock block;
    CProfile profile;
    try
    {
        CBufStream ss;
        ss.Write((const char*)&txFork.vchData[0], txFork.vchData.size());
        ss >> block;
        if (!block.IsOrigin() || block.IsPrimary())
        {
            throw std::runtime_error("invalid block");
        }
        if (!profile.Load(block.vchProof))
        {
            throw std::runtime_error("invalid profile");
        }
    }
    catch (...)
    {
        Error("Invalid orign block found in tx (%s)", txid.GetHex().c_str());
        return ERR_BLOCK_INVALID_FORK;
    }
    uint256 hashFork = block.GetHash();

    CForkContext ctxtParent;
    if (!cntrBlock.RetrieveForkContext(profile.hashParent, ctxtParent))
    {
        Log("AddNewForkContext Retrieve parent context Error: %s ", profile.hashParent.ToString().c_str());
        return ERR_MISSING_PREV;
    }

    CProfile forkProfile;
    Errno err = pCoreProtocol->ValidateOrigin(block, ctxtParent.GetProfile(), forkProfile);
    if (err != OK)
    {
        Log("AddNewForkContext Validate Block Error(%s) : %s ", ErrorString(err), hashFork.ToString().c_str());
        return err;
    }

    ctxt = CForkContext(block.GetHash(), block.hashPrev, txid, profile);
    if (!cntrBlock.AddNewForkContext(ctxt))
    {
        Log("AddNewForkContext Already Exists : %s ", hashFork.ToString().c_str());
        return ERR_ALREADY_HAVE;
    }

    return OK;
}

// CDispatcher::AddNewBlock调用，非Origin Block
Errno CBlockChain::AddNewBlock(const CBlock& block, CBlockChainUpdate& update)
{
    uint256 hash = block.GetHash();
    Errno err = OK;

    if (cntrBlock.Exists(hash))
    {
        Log("AddNewBlock Already Exists : %s ", hash.ToString().c_str());
        return ERR_ALREADY_HAVE;
    }

    // 主要是校验Block，比如Block签名，Block时间，其中的交易，以及Merkle Hash之类的
    err = pCoreProtocol->ValidateBlock(block);
    if (err != OK)
    {
        Log("AddNewBlock Validate Block Error(%s) : %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    CBlockIndex* pIndexPrev;
    // 拿到前序Block的Index，该前序不可能在Parent Fork上
    if (!cntrBlock.RetrieveIndex(block.hashPrev, &pIndexPrev))
    {
        Log("AddNewBlock Retrieve Prev Index Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    int64 nReward;
    CDelegateAgreement agreement;
    CBlockIndex* pIndexRef = nullptr;
    // 校验Block（类型，挖矿奖励，Cert Tx的计数，共识结果等等）
    err = VerifyBlock(hash, block, pIndexPrev, nReward, agreement, &pIndexRef);
    if (err != OK)
    {
        Log("AddNewBlock Verify Block Error(%s) : %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    storage::CBlockView view;
    // 新Block入库之前拿到前序Block关联的的Fork的BlockView，在View中进行长短链切换，并且BlockView是可提交的
    if (!cntrBlock.GetBlockView(block.hashPrev, view, !block.IsOrigin()))
    {
        Log("AddNewBlock Get Block View Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    if (!block.IsVacant())
    {
        // 添加挖矿交易进BlockView
        view.AddTx(block.txMint.GetHash(), block.txMint);
    }

    //把Block转换到BLockEx，为了拿到TxContxt列表，也就是vTx对应的详细信息
    CBlockEx blockex(block);
    // 此时vTxContxt是空的
    vector<CTxContxt>& vTxContxt = blockex.vTxContxt;

    int64 nTotalFee = 0;

    vTxContxt.reserve(block.vtx.size());

    int nForkHeight;
    if (block.nType == block.BLOCK_EXTENDED)
    {
        nForkHeight = pIndexPrev->nHeight; //子块不增加所属Fork的高度
    }
    else
    {
        nForkHeight = pIndexPrev->nHeight + 1; // 其他块才增加所属Fork的高度
    }

    // map<pair<CDestination, uint256>, uint256> mapEnrollTx;
    // 处理当前Block里打包好的所有交易(校验Tx)
    for (const CTransaction& tx : block.vtx)
    {
        uint256 txid = tx.GetHash();
        CTxContxt txContxt;
        // 从BlockView中拿到Tx对应的TxContxt上下文信息 （此时View的数据没有入库Commit）
        err = GetTxContxt(view, tx, txContxt);
        if (err != OK)
        {
            Log("AddNewBlock Get txContxt Error([%d] %s) : %s ", err, ErrorString(err), txid.ToString().c_str());
            return err;
        }
        // tx不在TxPool中意味着tx一定要正确上链才能校验Tx
        if (!pTxPool->Exists(txid))
        {
            // 校验Block中的tx(前序时间，与花费费用是否合理，模板，from地址签名是否合法)
            err = pCoreProtocol->VerifyBlockTx(tx, txContxt, pIndexPrev, nForkHeight, pIndexPrev->GetOriginHash());
            if (err != OK)
            {
                Log("AddNewBlock Verify BlockTx Error(%s) : %s ", ErrorString(err), txid.ToString().c_str());
                return err;
            }
        }

        // check enroll tx
        // if (tx.nType == CTransaction::TX_CERT)
        // {
        //     // check outdated
        //     uint32 nEnrollInterval = block.GetBlockHeight() - CBlock::GetBlockHeightByHash(tx.hashAnchor);
        //     if (nEnrollInterval > CONSENSUS_ENROLL_INTERVAL)
        //     {
        //         Log("AddNewBlock block %s delegate cert tx outdate, txid: %s, anchor: %s", hash.ToString().c_str(), txid.ToString().c_str(), tx.hashAnchor.ToString().c_str());
        //         return ERR_BLOCK_TRANSACTIONS_INVALID;
        //     }

        //     // check amount
        //     map<CDestination, int64> mapVote;
        //     if (!cntrBlock.GetBlockDelegateVote(tx.hashAnchor, mapVote))
        //     {
        //         Log("AddNewBlock Get block %s delegate vote error, txid: %s, anchor: %s", hash.ToString().c_str(), txid.ToString().c_str(), tx.hashAnchor.ToString().c_str());
        //         return ERR_BLOCK_TRANSACTIONS_INVALID;
        //     }
        //     auto voteIt = mapVote.find(tx.sendTo);
        //     if (voteIt == mapVote.end() || voteIt->second < pCoreProtocol->MinEnrollAmount())
        //     {
        //         Log("AddNewBlock enroll amount is less than the minimum: %d, txid: %s, amount: %d", pCoreProtocol->MinEnrollAmount(), txid.ToString().c_str(), voteIt == mapVote.end() ? 0 : voteIt->second);
        //         return ERR_BLOCK_TRANSACTIONS_INVALID;
        //     }

        //     // check repeat
        //     auto enrollTxIt = mapEnrollTx.find(make_pair(tx.sendTo, tx.hashAnchor));
        //     if (enrollTxIt != mapEnrollTx.end())
        //     {
        //         Log("AddNewBlock enroll tx repeat, dest: %s, txid1: %s, txid2:%s", tx.sendTo.ToString().c_str(), txid.ToString().c_str(), enrollTxIt->second.ToString().c_str());
        //         return ERR_BLOCK_TRANSACTIONS_INVALID;
        //     }
        //     vector<uint256> vBlockRange;
        //     CBlockIndex* pIndex = pIndexPrev;
        //     for (int i = 0; i < CONSENSUS_ENROLL_INTERVAL - 1 && i < nEnrollInterval; i++)
        //     {
        //         vBlockRange.push_back(pIndex->GetBlockHash());
        //         pIndex = pIndex->pPrev;
        //     }
        //     map<CDestination, storage::CDiskPos> mapEnrollTxPos;
        //     if (!cntrBlock.GetDelegateEnrollTx(CBlock::GetBlockHeightByHash(tx.hashAnchor), vBlockRange, mapEnrollTxPos))
        //     {
        //         Log("AddNewBlock get enroll tx error, txid: %s, anchor: %s", txid.ToString().c_str(), tx.hashAnchor.ToString().c_str());
        //         return ERR_BLOCK_TRANSACTIONS_INVALID;
        //     }
        //     if (mapEnrollTxPos.find(tx.sendTo) != mapEnrollTxPos.end())
        //     {
        //         Log("AddNewBlock enroll tx exist, txid: %s, anchor: %s", txid.ToString().c_str(), tx.hashAnchor.ToString().c_str());
        //         return ERR_BLOCK_TRANSACTIONS_INVALID;
        //     }

        //     mapEnrollTx.insert(make_pair(make_pair(tx.sendTo, tx.hashAnchor), txid));
        // }
        
        // 把tx对应的上下文信息保存到BlockEx中，以增加信息字段vTxContxt
        vTxContxt.push_back(txContxt);
        // 添加新增打包好的tx到BlockView中
        view.AddTx(txid, tx, txContxt.destIn, txContxt.GetValueIn());
        // 累加各个交易费
        nTotalFee += tx.nTxFee;
    }
    // 挖矿奖励不能大于总打包交易费和奖励值
    if (block.txMint.nAmount > nTotalFee + nReward)
    {
        Log("AddNewBlock Mint tx amount invalid : (%ld > %ld + %ld ", block.txMint.nAmount, nTotalFee, nReward);
        return ERR_BLOCK_TRANSACTIONS_INVALID;
    }

    // Get block trust
    uint256 nChainTrust = pCoreProtocol->GetBlockTrust(block, pIndexPrev, agreement, pIndexRef);
    StdTrace("BlockChain", "AddNewBlock block chain trust: %s", nChainTrust.GetHex().c_str());

    CBlockIndex* pIndexNew;
    if (!cntrBlock.AddNew(hash, blockex, &pIndexNew, nChainTrust, pCoreProtocol->MinEnrollAmount()))
    {
        Log("AddNewBlock Storage AddNew Error : %s ", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }
    Log("AddNew Block : %s", pIndexNew->ToString().c_str());

    CBlockIndex* pIndexFork = nullptr;
    if (cntrBlock.RetrieveFork(pIndexNew->GetOriginHash(), &pIndexFork)
        && (pIndexFork->nChainTrust > pIndexNew->nChainTrust
            || (pIndexFork->nChainTrust == pIndexNew->nChainTrust && !pIndexNew->IsEquivalent(pIndexFork))))
    {
        Log("AddNew Block : Short chain, new block height: %d, fork chain trust: %s, fork last block: %s",
            pIndexNew->GetBlockHeight(), pIndexFork->nChainTrust.GetHex().c_str(), pIndexFork->GetBlockHash().GetHex().c_str());
        return OK;
    }

    if (!cntrBlock.CommitBlockView(view, pIndexNew))
    {
        Log("AddNewBlock Storage Commit BlockView Error : %s ", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    update = CBlockChainUpdate(pIndexNew);
    view.GetTxUpdated(update.setTxUpdate);
    if (!GetBlockChanges(pIndexNew, pIndexFork, update.vBlockAddNew, update.vBlockRemove))
    {
        Log("AddNewBlock Storage GetBlockChanges Error : %s ", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    if (!update.vBlockRemove.empty())
    {
        uint32 nTxAdd = 0;
        for (const auto& b : update.vBlockAddNew)
        {
            Log("Chain rollback occur[added block]: height: %u hash: %s time: %u",
                b.GetBlockHeight(), b.GetHash().ToString().c_str(), b.nTimeStamp);
            Log("Chain rollback occur[added mint tx]: %s", b.txMint.GetHash().ToString().c_str());
            ++nTxAdd;
            for (const auto& t : b.vtx)
            {
                Log("Chain rollback occur[added tx]: %s", t.GetHash().ToString().c_str());
                ++nTxAdd;
            }
        }
        uint32 nTxDel = 0;
        for (const auto& b : update.vBlockRemove)
        {
            Log("Chain rollback occur[removed block]: height: %u hash: %s time: %u",
                b.GetBlockHeight(), b.GetHash().ToString().c_str(), b.nTimeStamp);
            Log("Chain rollback occur[removed mint tx]: %s", b.txMint.GetHash().ToString().c_str());
            ++nTxDel;
            for (const auto& t : b.vtx)
            {
                Log("Chain rollback occur[removed tx]: %s", t.GetHash().ToString().c_str());
                ++nTxDel;
            }
        }
        Log("Chain rollback occur, [height]: %u [hash]: %s "
            "[nBlockAdd]: %u [nBlockDel]: %u [nTxAdd]: %u [nTxDel]: %u",
            pIndexNew->GetBlockHeight(), pIndexNew->GetBlockHash().ToString().c_str(),
            update.vBlockAddNew.size(), update.vBlockRemove.size(), nTxAdd, nTxDel);
    }

    return OK;
}

// 添加Fork的第一个Block入库，因为是Origin类型的
Errno CBlockChain::AddNewOrigin(const CBlock& block, CBlockChainUpdate& update)
{
    uint256 hash = block.GetHash();
    Errno err = OK;

    if (cntrBlock.Exists(hash))
    {
        Log("AddNewOrigin Already Exists : %s ", hash.ToString().c_str());
        return ERR_ALREADY_HAVE;
    }

    // 主要是校验Block，比如Block签名，Block时间，其中的交易，以及Merkle Hash之类的
    err = pCoreProtocol->ValidateBlock(block);
    if (err != OK)
    {
        Log("AddNewOrigin Validate Block Error(%s) : %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    // 获取Origin Block的上一个Block的 Index，相当于是父分支的分叉点的Block
    CBlockIndex* pIndexPrev;
    if (!cntrBlock.RetrieveIndex(block.hashPrev, &pIndexPrev))
    {
        Log("AddNewOrigin Retrieve Prev Index Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    // 获取Origin Block的父分支的信息(名称，版本，挖矿奖励，交易费用等)
    CProfile parent;
    if (!cntrBlock.RetrieveProfile(pIndexPrev->GetOriginHash(), parent))
    {
        Log("AddNewOrigin Retrieve parent profile Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }
    // 根据该Origin Block拿到并校验该Fork的信息
    CProfile profile;
    err = pCoreProtocol->ValidateOrigin(block, parent, profile);
    if (err != OK)
    {
        Log("AddNewOrigin Validate Origin Error(%s): %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    // 根据Fork的名称查找本地是否有同名的Fork，不支持同名Fork，有同名就报错
    CBlockIndex* pIndexDuplicated;
    if (cntrBlock.RetrieveFork(profile.strName, &pIndexDuplicated))
    {
        Log("AddNewOrigin Validate Origin Error(duplated fork name): %s, \nexisted: %s",
            hash.ToString().c_str(), pIndexDuplicated->GetOriginHash().GetHex().c_str());
        return ERR_ALREADY_HAVE;
    }

    storage::CBlockView view;

    if (profile.IsIsolated())
    {
        // 如果是独立的Fork，就获得与该Fork关联的已初始化完成的Block View
        if (!cntrBlock.GetBlockView(view))
        {
            Log("AddNewOrigin Get Block View Error: %s ", block.hashPrev.ToString().c_str());
            return ERR_SYS_STORAGE_ERROR;
        }
    }
    else
    {
        // 如果不是独立的Fork，就获得Parent Fork关联的BlockView     独立与否，就在于与父分支有没有关联
        // 在获得与只关联的View的过程中，View的中的Tx是需要删除和新增的，相当于拿到前序Block的View的最新状态
        if (!cntrBlock.GetBlockView(block.hashPrev, view, false))
        {
            Log("AddNewOrigin Get Block View Error: %s ", block.hashPrev.ToString().c_str());
            return ERR_SYS_STORAGE_ERROR;
        }
    }

    // 挖矿奖励Tx有效就添加进Fork关联的BlockView中
    if (block.txMint.nAmount != 0)
    {
        view.AddTx(block.txMint.GetHash(), block.txMint);
    }

    // Get block trust
    // 获取当前Origin Block的Trust值，并需要其Parent Fork分叉点Block的Index，Origin Block的Trust值是0
    uint256 nChainTrust = pCoreProtocol->GetBlockTrust(block, pIndexPrev);

    CBlockIndex* pIndexNew;
    CBlockEx blockex(block);
    // 正式将该Origin Block入库，并获得该Block的Index
    if (!cntrBlock.AddNew(hash, blockex, &pIndexNew, nChainTrust, pCoreProtocol->MinEnrollAmount()))
    {
        Log("AddNewOrigin Storage AddNew Error : %s ", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    Log("AddNew Origin Block : %s ", hash.ToString().c_str());
    Log("    %s", pIndexNew->ToString().c_str());

    // 以最新的Origin Block更新并提交对应的Block View
    if (!cntrBlock.CommitBlockView(view, pIndexNew))
    {
        Log("AddNewOrigin Storage Commit BlockView Error : %s ", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }
    // 该Origin Block是当前最新的，把该Index转成Update对象(最新高度，最新BlockHash，时间戳等)
    update = CBlockChainUpdate(pIndexNew);
    // 把当前最新Block View的Tx Update写入Update对象
    view.GetTxUpdated(update.setTxUpdate);
    update.vBlockAddNew.push_back(blockex); // 该Origin Block是新增的，加入Update的BlockAddNew中

    return OK;
}

// uint320 GetBlockTrust() const
// {
//     if (IsVacant() && vchProof.empty())
//     {
//         return uint320();
//     }
//     else if (IsProofOfWork())
//     {
//         CProofOfHashWorkCompact proof;
//         proof.Load(vchProof);
//         return uint320(0, (~uint256(uint64(0)) << proof.nBits));
//     }
//     else
//     {
//         return uint320((uint64)vchProof[0], uint256(uint64(0)));
//     }
// }

bool CBlockChain::GetProofOfWorkTarget(const uint256& hashPrev, int nAlgo, int& nBits, int64& nReward)
{
    CBlockIndex* pIndexPrev;
    if (!cntrBlock.RetrieveIndex(hashPrev, &pIndexPrev))
    {
        Log("GetProofOfWorkTarget : Retrieve Prev Index Error: %s ", hashPrev.ToString().c_str());
        return false;
    }
    if (!pIndexPrev->IsPrimary())
    {
        Log("GetProofOfWorkTarget : Previous is not primary: %s ", hashPrev.ToString().c_str());
        return false;
    }
    if (!pCoreProtocol->GetProofOfWorkTarget(pIndexPrev, nAlgo, nBits, nReward))
    {
        Log("GetProofOfWorkTarget : Unknown proof-of-work algo: %s ", hashPrev.ToString().c_str());
        return false;
    }
    return true;
}

// 拿到对应Block的奖励
bool CBlockChain::GetBlockMintReward(const uint256& hashPrev, int64& nReward)
{
    CBlockIndex* pIndexPrev;
    if (!cntrBlock.RetrieveIndex(hashPrev, &pIndexPrev))
    {
        Log("Get block reward: Retrieve Prev Index Error, hashPrev: %s", hashPrev.ToString().c_str());
        return false;
    }

    // 主链块有20的奖励
    if (pIndexPrev->IsPrimary())
    {
        nReward = pCoreProtocol->GetPrimaryMintWorkReward(pIndexPrev);
    }
    else
    {
        // 因为支链主块的奖励是创建分支的时候配置的，所以要拿到分支对应的Profile的奖励字段，进行计算实际的奖励
        CProfile profile;
        if (!GetForkProfile(pIndexPrev->GetOriginHash(), profile))
        {
            Log("Get block reward: Get fork profile fail, hashPrev: %s", hashPrev.ToString().c_str());
            return false;
        }
        if (profile.nHalveCycle == 0)
        {
            nReward = profile.nMintReward;
        }
        else
        {
            nReward = profile.nMintReward / pow(2, (pIndexPrev->GetBlockHeight() + 1 - profile.nJointHeight) / profile.nHalveCycle);
        }
    }
    return true;
}

bool CBlockChain::GetBlockLocator(const uint256& hashFork, CBlockLocator& locator, uint256& hashDepth, int nIncStep)
{
    return cntrBlock.GetForkBlockLocator(hashFork, locator, hashDepth, nIncStep);
}

bool CBlockChain::GetBlockInv(const uint256& hashFork, const CBlockLocator& locator, vector<uint256>& vBlockHash, size_t nMaxCount)
{
    return cntrBlock.GetForkBlockInv(hashFork, locator, vBlockHash, nMaxCount);
}

bool CBlockChain::ListForkUnspent(const uint256& hashFork, const CDestination& dest, uint32 nMax, std::vector<CTxUnspent>& vUnspent)
{
    return cntrBlock.ListForkUnspent(hashFork, dest, nMax, vUnspent);
}

bool CBlockChain::ListForkUnspentBatch(const uint256& hashFork, uint32 nMax, std::map<CDestination, std::vector<CTxUnspent>>& mapUnspent)
{
    return cntrBlock.ListForkUnspentBatch(hashFork, nMax, mapUnspent);
}

bool CBlockChain::GetVotes(const CDestination& destDelegate, int64& nVotes)
{
    return cntrBlock.GetVotes(pCoreProtocol->GetGenesisBlockHash(), destDelegate, nVotes);
}

bool CBlockChain::ListDelegatePayment(uint32 height, CBlock& block, std::multimap<int64, CDestination>& mapVotes)
{
    std::vector<uint256> vBlockHash;
    if (!GetBlockHash(pCoreProtocol->GetGenesisBlockHash(), height, vBlockHash) || vBlockHash.size() == 0)
    {
        return false;
    }
    cntrBlock.GetDelegatePaymentList(vBlockHash[0], mapVotes);
    if (!GetBlock(vBlockHash[0], block))
    {
        return false;
    }
    return true;
}

bool CBlockChain::ListDelegate(uint32 nCount, std::multimap<int64, CDestination>& mapVotes)
{
    return cntrBlock.GetDelegateList(pCoreProtocol->GetGenesisBlockHash(), nCount, mapVotes);
}

bool CBlockChain::VerifyRepeatBlock(const uint256& hashFork, const CBlock& block, const uint256& hashBlockRef)
{
    uint32 nRefTimeStamp = 0;
    if (hashBlockRef != 0 && (block.IsSubsidiary() || block.IsExtended()))
    {
        CBlockIndex* pIndexRef;
        if (!cntrBlock.RetrieveIndex(hashBlockRef, &pIndexRef))
        {
            StdLog("CBlockChain", "VerifyRepeatBlock: RetrieveIndex fail, hashBlockRef: %s, block: %s",
                   hashBlockRef.GetHex().c_str(), block.GetHash().GetHex().c_str());
            return false;
        }
        if (block.IsSubsidiary())
        {
            if (block.GetBlockTime() != pIndexRef->GetBlockTime())
            {
                StdLog("CBlockChain", "VerifyRepeatBlock: Subsidiary block time error, block time: %ld, ref block time: %ld, hashBlockRef: %s, block: %s",
                       block.GetBlockTime(), pIndexRef->GetBlockTime(), hashBlockRef.GetHex().c_str(), block.GetHash().GetHex().c_str());
                return false;
            }
        }
        else
        {
            if (block.GetBlockTime() <= pIndexRef->GetBlockTime()
                || block.GetBlockTime() >= pIndexRef->GetBlockTime() + BLOCK_TARGET_SPACING)
            {
                StdLog("CBlockChain", "VerifyRepeatBlock: Extended block time error, block time: %ld, ref block time: %ld, hashBlockRef: %s, block: %s",
                       block.GetBlockTime(), pIndexRef->GetBlockTime(), hashBlockRef.GetHex().c_str(), block.GetHash().GetHex().c_str());
                return false;
            }
        }
        nRefTimeStamp = pIndexRef->nTimeStamp;
    }
    return cntrBlock.VerifyRepeatBlock(hashFork, block.GetBlockHeight(), block.txMint.sendTo, block.nType, block.nTimeStamp, nRefTimeStamp, EXTENDED_BLOCK_SPACING);
}

bool CBlockChain::GetBlockDelegateVote(const uint256& hashBlock, map<CDestination, int64>& mapVote)
{
    return cntrBlock.GetBlockDelegateVote(hashBlock, mapVote);
}

int64 CBlockChain::GetDelegateWeightRatio(const uint256& hashBlock)
{
    return pCoreProtocol->MinEnrollAmount();
}

bool CBlockChain::GetDelegateCertTxCount(const uint256& hashLastBlock, map<CDestination, int>& mapVoteCert)
{
    CBlockIndex* pLastIndex = nullptr;
    if (!cntrBlock.RetrieveIndex(hashLastBlock, &pLastIndex))
    {
        StdLog("CBlockChain", "GetDelegateCertTxCount: RetrieveIndex fail, block: %s", hashLastBlock.GetHex().c_str());
        return false;
    }
    if (pLastIndex->GetBlockHeight() <= 0)
    {
        return true;
    }

    int nMinHeight = pLastIndex->GetBlockHeight() - CONSENSUS_ENROLL_INTERVAL + 2;
    if (nMinHeight < 1)
    {
        nMinHeight = 1;
    }

    CBlockIndex* pIndex = pLastIndex;
    for (int i = 0; i < CONSENSUS_ENROLL_INTERVAL - 1 && pIndex != nullptr; i++)
    {
        std::map<int, std::set<CDestination>> mapEnrollDest;
        if (cntrBlock.GetBlockDelegatedEnrollTx(pIndex->GetBlockHash(), mapEnrollDest))
        {
            for (const auto& t : mapEnrollDest)
            {
                if (t.first >= nMinHeight)
                {
                    for (const auto& m : t.second)
                    {
                        map<CDestination, int>::iterator it = mapVoteCert.find(m);
                        if (it == mapVoteCert.end())
                        {
                            mapVoteCert.insert(make_pair(m, 1));
                        }
                        else
                        {
                            it->second++;
                        }
                    }
                }
            }
        }
        pIndex = pIndex->pPrev;
    }

    int nMaxCertCount = CONSENSUS_ENROLL_INTERVAL + 2;
    if (nMaxCertCount > pLastIndex->GetBlockHeight())
    {
        nMaxCertCount = pLastIndex->GetBlockHeight();
    }
    for (auto& v : mapVoteCert)
    {
        v.second = nMaxCertCount - v.second;
        if (v.second < 0)
        {
            v.second = 0;
        }
    }
    return true;
}

// 从Enroll阶段完毕的Block拿Enroll结果
bool CBlockChain::GetBlockDelegateEnrolled(const uint256& hashBlock, CDelegateEnrolled& enrolled)
{
    // Log("CBlockChain::GetBlockDelegateEnrolled enter .... height: %d, hashBlock: %s", CBlock::GetBlockHeightByHash(hashBlock), hashBlock.ToString().c_str());
    enrolled.Clear();
    // 先从缓存里拿Enroll结果，拿到就可以正常返回
    if (cacheEnrolled.Retrieve(hashBlock, enrolled))
    {
        return true;
    }

    CBlockIndex* pIndex;
    // 拿到Enroll完毕的Block Index
    if (!cntrBlock.RetrieveIndex(hashBlock, &pIndex))
    {
        Log("GetBlockDelegateEnrolled : Retrieve block Index Error: %s \n", hashBlock.ToString().c_str());
        return false;
    }
    int64 nMinEnrollAmount = pCoreProtocol->MinEnrollAmount();
    // 这个Enroll完毕的点显然要大于ENROLL时间区块段，不然相当于还没登记，正常返回
    if (pIndex->GetBlockHeight() < CONSENSUS_ENROLL_INTERVAL)
    {
        return true;
    }
    vector<uint256> vBlockRange;
    // 遍历到Enroll阶段开始的Index，并且把Enroll阶段的所有Blockhash倒序列存放
    for (int i = 0; i < CONSENSUS_ENROLL_INTERVAL; i++)
    {
        vBlockRange.push_back(pIndex->GetBlockHash());
        pIndex = pIndex->pPrev;
    }
    // 最终通过Enroll最后的Blockhash和Enroll开始的Block height拿到Enroll的结果（mapWeight权重，mapEnrollData）
    if (!cntrBlock.RetrieveAvailDelegate(hashBlock, pIndex->GetBlockHeight(), vBlockRange, nMinEnrollAmount,
                                         enrolled.mapWeight, enrolled.mapEnrollData, enrolled.vecAmount))
    {
        Log("GetBlockDelegateEnrolled : Retrieve Avail Delegate Error: %s \n", hashBlock.ToString().c_str());
        return false;
    }

    cacheEnrolled.AddNew(hashBlock, enrolled);

    return true;
}

bool CBlockChain::GetBlockDelegateAgreement(const uint256& hashBlock, CDelegateAgreement& agreement)
{
    agreement.Clear();
    // 从缓存里查，查到共识结果就返回
    if (cacheAgreement.Retrieve(hashBlock, agreement))
    {
        return true;
    }
    // 拿到当前Block的Index
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveIndex(hashBlock, &pIndex))
    {
        Log("GetBlockDelegateAgreement : Retrieve block Index Error: %s \n", hashBlock.ToString().c_str());
        return false;
    }
    CBlockIndex* pIndexRef = pIndex;
    // 如果同步过来的Block还没有共识的高度区间高，那么就没有共识结果，暂时返回true，没必要往下查
    if (pIndex->GetBlockHeight() < CONSENSUS_INTERVAL)
    {
        return true;
    }

    CBlock block;
    // 通过Index拿到这个Block数据
    if (!cntrBlock.Retrieve(pIndex, block))
    {
        Log("GetBlockDelegateAgreement : Retrieve block Error: %s \n", hashBlock.ToString().c_str());
        return false;
    }
    // 如果当前Block为target block 那么向前遍历Distribute + 1的长度就是Enroll阶段完毕的点
    for (int i = 0; i < CONSENSUS_DISTRIBUTE_INTERVAL + 1; i++)
    {
        pIndex = pIndex->pPrev;
    }

    CDelegateEnrolled enrolled;
    // 通过Enrolled完毕的Index拿到Enroll结果（mapWeight权重，mapEnrollData，vecVoteAmount投票金额）
    if (!GetBlockDelegateEnrolled(pIndex->GetBlockHash(), enrolled))
    {
        return false;
    }

    // 由Enroll的结果生成Verifier对象，通过该对象从当前target block中解码并校验出Agreement对象的部分字段(nWeight,nAgreement)还有mapBallot表
    delegate::CDelegateVerify verifier(enrolled.mapWeight, enrolled.mapEnrollData);
    map<CDestination, size_t> mapBallot;
    if (!verifier.VerifyProof(block.vchProof, agreement.nAgreement, agreement.nWeight, mapBallot))
    {
        Log("GetBlockDelegateAgreement : Invalid block proof : %s \n", hashBlock.ToString().c_str());
        return false;
    }

    // 这一步主要是拿到Agreement对象的vBallot，里面对随机信标进行随机数计算，共识结果
    pCoreProtocol->GetDelegatedBallot(agreement.nAgreement, agreement.nWeight, mapBallot, enrolled.vecAmount, pIndex->GetMoneySupply(), agreement.vBallot, pIndexRef->GetBlockHeight());

    cacheAgreement.AddNew(hashBlock, agreement);

    return true;
}

int64 CBlockChain::GetBlockMoneySupply(const uint256& hashBlock)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveIndex(hashBlock, &pIndex) || pIndex == nullptr)
    {
        return -1;
    }
    return pIndex->GetMoneySupply();
}

bool CBlockChain::CheckContainer()
{
    if (cntrBlock.IsEmpty())
    {
        return true;
    }
    if (!cntrBlock.Exists(pCoreProtocol->GetGenesisBlockHash()))
    {
        return false;
    }
    return cntrBlock.CheckConsistency(StorageConfig()->nCheckLevel, StorageConfig()->nCheckDepth);
}

bool CBlockChain::RebuildContainer()
{
    return false;
}

bool CBlockChain::InsertGenesisBlock(CBlock& block)
{
    uint256 nChainTrust = pCoreProtocol->GetBlockTrust(block);
    return cntrBlock.Initiate(block.GetHash(), block, nChainTrust);
}

// 从BlockView中拿到Tx对应的TxContxt上下文信息
Errno CBlockChain::GetTxContxt(storage::CBlockView& view, const CTransaction& tx, CTxContxt& txContxt)
{
    txContxt.SetNull();
    // 从Tx的输入查找到View中的前序Tx输出
    for (const CTxIn& txin : tx.vInput)
    {
        CTxOut output;
        if (!view.RetrieveUnspent(txin.prevout, output))
        {
            Log("GetTxContxt: RetrieveUnspent fail, prevout: [%d]:%s", txin.prevout.n, txin.prevout.hash.GetHex().c_str());
            return ERR_MISSING_PREV;
        }
        // 前序Tx的输出转账地址就是后续Tx的转入地址，也就是后续该Tx的from地址
        if (txContxt.destIn.IsNull())
        {
            txContxt.destIn = output.destTo;
        }
        else if (txContxt.destIn != output.destTo)
        {
            // 如果不等于，肯定报错，也就是from地址必须一样
            Log("GetTxContxt: destIn error, destIn: %s, destTo: %s",
                txContxt.destIn.ToString().c_str(), output.destTo.ToString().c_str());
            return ERR_TRANSACTION_INVALID;
        }
        // 前序Tx的输出作为后续Tx的输入
        txContxt.vin.push_back(CTxInContxt(output));
    }
    return OK;
}

bool CBlockChain::GetBlockChanges(const CBlockIndex* pIndexNew, const CBlockIndex* pIndexFork,
                                  vector<CBlockEx>& vBlockAddNew, vector<CBlockEx>& vBlockRemove)
{
    while (pIndexNew != pIndexFork)
    {
        int64 nLastBlockTime = pIndexFork ? pIndexFork->GetBlockTime() : -1;
        if (pIndexNew->GetBlockTime() >= nLastBlockTime)
        {
            CBlockEx block;
            if (!cntrBlock.Retrieve(pIndexNew, block))
            {
                return false;
            }
            vBlockAddNew.push_back(block);
            pIndexNew = pIndexNew->pPrev;
        }
        else
        {
            CBlockEx block;
            if (!cntrBlock.Retrieve(pIndexFork, block))
            {
                return false;
            }
            vBlockRemove.push_back(block);
            pIndexFork = pIndexFork->pPrev;
        }
    }
    return true;
}

bool CBlockChain::GetBlockDelegateAgreement(const uint256& hashBlock, const CBlock& block, const CBlockIndex* pIndexPrev,
                                            CDelegateAgreement& agreement)
{
    agreement.Clear();

    if (pIndexPrev->GetBlockHeight() < CONSENSUS_INTERVAL - 1)
    {
        return true;
    }

    const CBlockIndex* pIndex = pIndexPrev;

    for (int i = 0; i < CONSENSUS_DISTRIBUTE_INTERVAL; i++)
    {
        pIndex = pIndex->pPrev;
    }

    CDelegateEnrolled enrolled;

    if (!GetBlockDelegateEnrolled(pIndex->GetBlockHash(), enrolled))
    {
        return false;
    }

    delegate::CDelegateVerify verifier(enrolled.mapWeight, enrolled.mapEnrollData);
    map<CDestination, size_t> mapBallot;
    if (!verifier.VerifyProof(block.vchProof, agreement.nAgreement, agreement.nWeight, mapBallot))
    {
        Log("GetBlockDelegateAgreement : Invalid block proof : %s \n", hashBlock.ToString().c_str());
        return false;
    }

    pCoreProtocol->GetDelegatedBallot(agreement.nAgreement, agreement.nWeight, mapBallot, enrolled.vecAmount, pIndex->GetMoneySupply(), agreement.vBallot, pIndexPrev->GetBlockHeight() + 1);

    cacheAgreement.AddNew(hashBlock, agreement);

    return true;
}

// 校验Block 主要是Block的类型等
Errno CBlockChain::VerifyBlock(const uint256& hashBlock, const CBlock& block, CBlockIndex* pIndexPrev,
                               int64& nReward, CDelegateAgreement& agreement, CBlockIndex** ppIndexRef)
{
    nReward = 0;
    // Block一定是非Origin的
    if (block.IsOrigin())
    {
        return ERR_BLOCK_INVALID_FORK;
    }

    if (block.IsPrimary())
    {
        // 主链的Block走这里，前序Block也肯定是主块，不然就报错
        if (!pIndexPrev->IsPrimary())
        {
            return ERR_BLOCK_INVALID_FORK;
        }

        //校验Block打包的Cert Tx的count与前序Block的count是否超出
        if (!VerifyBlockCertTx(block))
        {
            return ERR_BLOCK_CERTTX_OUT_OF_BOUND;
        }

        // 获得以该Block维target block的的DelegateAgreement（共识结果） 这步主要是校验了，这个Block同步过来肯定共识结果要是能获得的
        if (!GetBlockDelegateAgreement(hashBlock, block, pIndexPrev, agreement))
        {
            return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
        }

        // 得到前序Block的奖励
        if (!GetBlockMintReward(block.hashPrev, nReward))
        {
            return ERR_BLOCK_COINBASE_INVALID;
        }

        /*if (pCoreProtocol->CheckSpecialHeight(pIndexPrev->GetBlockHeight() + 1))
        {
            if (!pCoreProtocol->VerifySpecialAddress(pIndexPrev->GetBlockHeight() + 1, block))
            {
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }
            if (!agreement.IsProofOfWork())
            {
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }
            return pCoreProtocol->VerifyProofOfWork(block, pIndexPrev);
        }
        else*/
        {
            // PoW或DPoS共识结果就用相应的函数校验结果
            if (agreement.IsProofOfWork())
            {
                return pCoreProtocol->VerifyProofOfWork(block, pIndexPrev);
            }
            else
            {
                return pCoreProtocol->VerifyDelegatedProofOfStake(block, pIndexPrev, agreement);
            }
        }
    }
    else if (!block.IsVacant())
    {
        if (pIndexPrev->IsPrimary())
        {
            return ERR_BLOCK_INVALID_FORK;
        }

        // 拿到支链主块共识结果的证明
        CProofOfPiggyback proof;
        proof.Load(block.vchProof);

        CDelegateAgreement agreement;
        // 通过支链主块的证明拿到共识结果，hashRefBlock是支链主块参考引用主链主块的证明
        if (!GetBlockDelegateAgreement(proof.hashRefBlock, agreement))
        {
            return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
        }

        // 支链主块的共识要与解码出来的一致，而且一定要是DPoS出的
        if (agreement.nAgreement != proof.nAgreement || agreement.nWeight != proof.nWeight
            || agreement.IsProofOfWork())
        {
            return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
        }

        // 拿到该支链主块共识结果对应的主块的BlockIndex作为输出参数
        if (!cntrBlock.RetrieveIndex(proof.hashRefBlock, ppIndexRef))
        {
            return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
        }

        if (block.IsExtended())
        {
            // 如果是子块就校验前序Block的共识结果，只有前序（支链主块）才是它的依据
            CBlock blockPrev;
            if (!cntrBlock.Retrieve(pIndexPrev, blockPrev) || blockPrev.IsVacant())
            {
                return ERR_MISSING_PREV;
            }

            CProofOfPiggyback proofPrev;
            proofPrev.Load(blockPrev.vchProof);
            if (proof.nAgreement != proofPrev.nAgreement || proof.nWeight != proofPrev.nWeight)
            {
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }
            nReward = 0;
        }
        else
        {
            if (!GetBlockMintReward(block.hashPrev, nReward))
            {
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }
        }
        // 校验支链的块
        return pCoreProtocol->VerifySubsidiary(block, pIndexPrev, *ppIndexRef, agreement);
    }
    return OK;
}

bool CBlockChain::VerifyBlockCertTx(const CBlock& block)
{
    std::map<CDestination, int> mapBlockCert;
    // 统计打包进Block的Cert Tx发送到不同Delegate模板地址的CERT交易个数
    for (const auto& d : block.vtx)
    {
        if (d.nType == CTransaction::TX_CERT)
        {
            ++mapBlockCert[d.sendTo];
        }
    }
    if (!mapBlockCert.empty())
    {
        // 与前序Block的模板地址Cert交易个数的表做对比，如果比前序Block的tx个数大，那么就是错误的
        std::map<CDestination, int> mapVoteCert;
        if (GetDelegateCertTxCount(block.hashPrev, mapVoteCert))
        {
            for (const auto& d : mapBlockCert)
            {
                std::map<CDestination, int>::iterator it = mapVoteCert.find(d.first);
                if (it != mapVoteCert.end() && d.second > it->second)
                {
                    StdLog("CBlockChain", "VerifyBlockCertTx: block cert count: %d, prev cert count: %d, dest: %s", d.second > it->second, CAddress(d.first).ToString().c_str());
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace bigbang
