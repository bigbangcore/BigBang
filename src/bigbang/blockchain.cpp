// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockchain.h"

#include "../common/template/fork.h"
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
    pForkManager = nullptr;
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

    if (!GetObject("forkmanager", pForkManager))
    {
        Error("Failed to request forkmanager\n");
        return false;
    }

    InitCheckPoints();

    return true;
}

void CBlockChain::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pTxPool = nullptr;
    pForkManager = nullptr;
}

bool CBlockChain::HandleInvoke()
{
    CBlock blockGenesis;
    pCoreProtocol->GetGenesisBlock(blockGenesis);

    if (!cntrBlock.Initialize(Config()->pathData, blockGenesis.GetHash(), Config()->fDebug))
    {
        Error("Failed to initialize container");
        return false;
    }

    /*if (!CheckContainer())
    {
        cntrBlock.Clear();
        Log("Block container is invalid,try rebuild from block storage");
        // Rebuild ...
        if (!RebuildContainer())
        {
            cntrBlock.Clear();
            Error("Failed to rebuild Block container,reconstruct all");
        }
    }*/

    if (cntrBlock.IsEmpty())
    {
        if (!InsertGenesisBlock(blockGenesis))
        {
            Error("Failed to create genesis block");
            return false;
        }
    }

    InitCheckPoints();

    // Check local block compared to checkpoint
    if (Config()->nMagicNum == MAINNET_MAGICNUM)
    {
        std::map<uint256, CForkStatus> mapForkStatus;
        GetForkStatus(mapForkStatus);
        for (const auto& fork : mapForkStatus)
        {
            CBlock block;
            if (!FindPreviousCheckPointBlock(fork.first, block))
            {
                StdError("BlockChain", "Find CheckPoint on fork %s Error when the node starting, you should purge data(bigbang -purge) to resync blockchain",
                         fork.first.ToString().c_str());
                return false;
            }
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

        map<uint256, CForkStatus>::iterator mi = mapForkStatus.find(hashFork);
        if (mi == mapForkStatus.end())
        {
            mi = mapForkStatus.insert(make_pair(hashFork, CForkStatus(hashFork, hashParent, nForkHeight))).first;
            if (mi == mapForkStatus.end())
            {
                continue;
            }
        }
        CForkStatus& status = (*mi).second;
        status.hashLastBlock = pIndex->GetBlockHash();
        status.nLastBlockTime = pIndex->GetBlockTime();
        status.nLastBlockHeight = pIndex->GetBlockHeight();
        status.nMoneySupply = pIndex->GetMoneySupply();
        status.nMintType = pIndex->nMintType;
    }
}

void CBlockChain::GetValidForkStatus(std::map<uint256, CForkStatus>& mapForkStatus)
{
    uint256 hashPrimaryLastBlock;
    int nTempHeight;
    int64 nTempTime;
    uint16 nTempMintType;
    if (!GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashPrimaryLastBlock, nTempHeight, nTempTime, nTempMintType))
    {
        StdError("BlockChain", "GetValidForkStatus GetLastBlock fail");
        return;
    }

    map<uint256, bool> mapValidFork;
    pForkManager->GetValidForkList(hashPrimaryLastBlock, mapValidFork);

    GetForkStatus(mapForkStatus);

    map<uint256, CForkStatus>::iterator it = mapForkStatus.begin();
    while (it != mapForkStatus.end())
    {
        const auto nt = mapValidFork.find(it->first);
        if (nt == mapValidFork.end() || !nt->second)
        {
            mapForkStatus.erase(it++);
            continue;
        }
        ++it;
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

bool CBlockChain::GetLastBlockOfHeight(const uint256& hashFork, const int nHeight, uint256& hashBlock, int64& nTime)
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
    if (pIndex == nullptr || pIndex->GetBlockHeight() != nHeight)
    {
        return false;
    }

    hashBlock = pIndex->GetBlockHash();
    nTime = pIndex->GetBlockTime();

    return true;
}

bool CBlockChain::GetLastBlock(const uint256& hashFork, uint256& hashBlock, int& nHeight, int64& nTime, uint16& nMintType)
{
    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveFork(hashFork, &pIndex))
    {
        return false;
    }
    hashBlock = pIndex->GetBlockHash();
    nHeight = pIndex->GetBlockHeight();
    nTime = pIndex->GetBlockTime();
    nMintType = pIndex->nMintType;
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

bool CBlockChain::GetTransaction(const uint256& txid, CTransaction& tx, uint256& hashFork, int& nHeight)
{
    return cntrBlock.RetrieveTx(txid, tx, hashFork, nHeight);
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

bool CBlockChain::ListForkContext(vector<CForkContext>& vForkCtxt, map<uint256, pair<uint256, map<uint256, int>>>& mapValidForkId)
{
    return cntrBlock.ListForkContext(vForkCtxt, mapValidForkId);
}

Errno CBlockChain::AddNewBlock(const CBlock& block, CBlockChainUpdate& update)
{
    uint256 hash = block.GetHash();
    Errno err = OK;

    if (cntrBlock.Exists(hash))
    {
        Log("AddNewBlock Already Exists : %s ", hash.ToString().c_str());
        return ERR_ALREADY_HAVE;
    }

    err = pCoreProtocol->ValidateBlock(block);
    if (err != OK)
    {
        Log("AddNewBlock Validate Block Error(%s) : %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    CBlockIndex* pIndexPrev;
    if (!cntrBlock.RetrieveIndex(block.hashPrev, &pIndexPrev))
    {
        Log("AddNewBlock Retrieve Prev Index Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    int64 nReward = 0;
    CDelegateAgreement agreement;
    size_t nEnrollTrust = 0;
    CBlockIndex* pIndexRef = nullptr;
    err = VerifyBlock(hash, block, pIndexPrev, nReward, agreement, nEnrollTrust, &pIndexRef);
    if (err != OK)
    {
        Log("AddNewBlock Verify Block Error(%s) : %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    bool fGetBranchBlock = true;
    if (block.IsVacant())
    {
        do
        {
            if (!block.IsPrimary() && pCoreProtocol->IsRefVacantHeight(block.GetBlockHeight()) && pIndexRef
                && !cntrBlock.VerifyRefBlock(pCoreProtocol->GetGenesisBlockHash(), pIndexRef->GetBlockHash()))
            {
                fGetBranchBlock = false;
                break;
            }

            uint256 nNewChainTrust;
            if (!pCoreProtocol->GetBlockTrust(block, nNewChainTrust, pIndexPrev, agreement, pIndexRef, nEnrollTrust))
            {
                break;
            }
            nNewChainTrust += pIndexPrev->nChainTrust;

            CBlockIndex* pIndexForkLast = nullptr;
            if (cntrBlock.RetrieveFork(pIndexPrev->GetOriginHash(), &pIndexForkLast) && pIndexForkLast->nChainTrust > nNewChainTrust)
            {
                fGetBranchBlock = false;
                break;
            }
        } while (0);
    }

    storage::CBlockView view;
    if (!cntrBlock.GetBlockView(block.hashPrev, view, !block.IsOrigin(), fGetBranchBlock))
    {
        Log("AddNewBlock Get Block View Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    if (!block.IsVacant() || !block.txMint.sendTo.IsNull())
    {
        view.AddTx(block.txMint.GetHash(), block.txMint);
    }

    CBlockEx blockex(block);
    vector<CTxContxt>& vTxContxt = blockex.vTxContxt;

    int64 nTotalFee = 0;
    if (block.vtx.size() > 0)
    {
        vTxContxt.reserve(block.vtx.size());
    }

    int nForkHeight;
    if (block.nType == block.BLOCK_EXTENDED)
    {
        nForkHeight = pIndexPrev->nHeight;
    }
    else
    {
        nForkHeight = pIndexPrev->nHeight + 1;
    }

    for (const CTransaction& tx : block.vtx)
    {
        uint256 txid = tx.GetHash();
        CTxContxt txContxt;
        err = GetTxContxt(view, tx, txContxt);
        if (err != OK)
        {
            Log("AddNewBlock Get txContxt Error([%d] %s) : %s ", err, ErrorString(err), txid.ToString().c_str());
            return err;
        }
        err = pCoreProtocol->VerifyBlockTx(tx, txContxt, pIndexPrev, nForkHeight, pIndexPrev->GetOriginHash());
        if (err != OK)
        {
            Log("AddNewBlock Verify BlockTx Error(%s) : %s ", ErrorString(err), txid.ToString().c_str());
            return err;
        }
        if (tx.nTimeStamp > block.nTimeStamp)
        {
            Log("AddNewBlock Verify BlockTx time fail: tx time: %d, block time: %d, tx: %s, block: %s",
                tx.nTimeStamp, block.nTimeStamp, txid.ToString().c_str(), hash.GetHex().c_str());
            return ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE;
        }

        vTxContxt.push_back(txContxt);
        view.AddTx(txid, tx, txContxt.destIn, txContxt.GetValueIn());

        StdTrace("BlockChain", "AddNewBlock: verify tx success, new tx: %s, new block: %s", txid.GetHex().c_str(), hash.GetHex().c_str());

        nTotalFee += tx.nTxFee;
    }
    view.AddBlock(hash, blockex);

    if (block.txMint.nAmount > nTotalFee + nReward)
    {
        Log("AddNewBlock Mint tx amount invalid : (%ld > %ld + %ld)", block.txMint.nAmount, nTotalFee, nReward);
        return ERR_BLOCK_TRANSACTIONS_INVALID;
    }

    // Get block trust
    uint256 nChainTrust;
    if (!pCoreProtocol->GetBlockTrust(block, nChainTrust, pIndexPrev, agreement, pIndexRef, nEnrollTrust))
    {
        Log("AddNewBlock get block trust fail, block: %s", hash.GetHex().c_str());
        return ERR_BLOCK_TRANSACTIONS_INVALID;
    }
    StdTrace("BlockChain", "AddNewBlock block chain trust: %s", nChainTrust.GetHex().c_str());

    CBlockIndex* pIndexNew;
    if (!cntrBlock.AddNew(hash, blockex, &pIndexNew, nChainTrust, pCoreProtocol->MinEnrollAmount()))
    {
        Log("AddNewBlock Storage AddNew Error : %s ", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }
    Log("AddNew Block : %s", pIndexNew->ToString().c_str());

    if (pIndexNew->GetOriginHash() == pCoreProtocol->GetGenesisBlockHash())
    {
        if (!AddBlockForkContext(blockex))
        {
            Log("AddNewBlock add block fork fail, block: %s", hash.ToString().c_str());
            return err;
        }
    }

    if (!pIndexNew->IsPrimary() && (!pIndexNew->IsVacant() || pCoreProtocol->IsRefVacantHeight(block.GetBlockHeight())) && pIndexRef
        && !cntrBlock.VerifyRefBlock(pCoreProtocol->GetGenesisBlockHash(), pIndexRef->GetBlockHash()))
    {
        Log("AddNew Block: Ref block short chain, refblock: %s, new block: %s, fork: %s",
            pIndexRef->GetBlockHash().GetHex().c_str(), hash.GetHex().c_str(), pIndexNew->GetOriginHash().GetHex().c_str());
        return OK;
    }

    CBlockIndex* pIndexFork = nullptr;
    if (cntrBlock.RetrieveFork(pIndexNew->GetOriginHash(), &pIndexFork)
        && (pIndexFork->nChainTrust > pIndexNew->nChainTrust
            || (pIndexFork->nChainTrust == pIndexNew->nChainTrust && !pIndexNew->IsEquivalent(pIndexFork))))
    {
        Log("AddNew Block : Short chain, new block height: %d, block type: %s, block: %s, fork chain trust: %s, fork last block: %s, fork: %s",
            pIndexNew->GetBlockHeight(), GetBlockTypeStr(block.nType, block.txMint.nType).c_str(), hash.GetHex().c_str(),
            pIndexFork->nChainTrust.GetHex().c_str(), pIndexFork->GetBlockHash().GetHex().c_str(), pIndexFork->GetOriginHash().GetHex().c_str());
        return OK;
    }

    if (!cntrBlock.CommitBlockView(view, pIndexNew))
    {
        Log("AddNewBlock Storage Commit BlockView Error : %s ", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    update = CBlockChainUpdate(pIndexNew);
    view.GetTxUpdated(update.setTxUpdate);
    view.GetBlockChanges(update.vBlockAddNew, update.vBlockRemove);

    StdLog("BlockChain", "AddNewBlock: Commit blockchain success, height: %d, block type: %s, add block: %ld, remove block: %ld, block tx count: %ld, block: %s, fork: %s",
           block.GetBlockHeight(), GetBlockTypeStr(block.nType, block.txMint.nType).c_str(),
           update.vBlockAddNew.size(), update.vBlockRemove.size(),
           block.vtx.size(), hash.GetHex().c_str(), pIndexFork->GetOriginHash().GetHex().c_str());

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

Errno CBlockChain::AddNewOrigin(const CBlock& block, CBlockChainUpdate& update)
{
    uint256 hash = block.GetHash();
    Errno err = OK;

    if (cntrBlock.Exists(hash))
    {
        Log("AddNewOrigin Already Exists : %s ", hash.ToString().c_str());
        return ERR_ALREADY_HAVE;
    }

    err = pCoreProtocol->ValidateBlock(block);
    if (err != OK)
    {
        Log("AddNewOrigin Validate Block Error(%s) : %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    CBlockIndex* pIndexPrev;
    if (!cntrBlock.RetrieveIndex(block.hashPrev, &pIndexPrev))
    {
        Log("AddNewOrigin Retrieve Prev Index Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    if (pIndexPrev->IsExtended() || pIndexPrev->IsVacant())
    {
        Log("Prev block should not be extended/vacant block");
        return ERR_BLOCK_TYPE_INVALID;
    }

    uint256 hashBlockRef;
    int64 nTimeRef;
    if (!GetLastBlockOfHeight(pCoreProtocol->GetGenesisBlockHash(), block.GetBlockHeight(), hashBlockRef, nTimeRef))
    {
        Log("Failed to query main chain reference block");
        return ERR_SYS_STORAGE_ERROR;
    }
    if (block.GetBlockTime() != nTimeRef)
    {
        Log("Invalid origin block time");
        return ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE;
    }

    CProfile parent;
    if (!cntrBlock.RetrieveProfile(pIndexPrev->GetOriginHash(), parent))
    {
        Log("AddNewOrigin Retrieve parent profile Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }
    CProfile profile;
    err = pCoreProtocol->ValidateOrigin(block, parent, profile);
    if (err != OK)
    {
        Log("AddNewOrigin Validate Origin Error(%s): %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

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
        if (!cntrBlock.GetBlockView(view))
        {
            Log("AddNewOrigin Get Block View Error: %s ", block.hashPrev.ToString().c_str());
            return ERR_SYS_STORAGE_ERROR;
        }
    }
    else
    {
        if (!cntrBlock.GetBlockView(block.hashPrev, view, false))
        {
            Log("AddNewOrigin Get Block View Error: %s ", block.hashPrev.ToString().c_str());
            return ERR_SYS_STORAGE_ERROR;
        }
    }

    if (block.txMint.nAmount != 0)
    {
        view.AddTx(block.txMint.GetHash(), block.txMint);
    }

    // Get block trust
    uint256 nChainTrust;
    if (!pCoreProtocol->GetBlockTrust(block, nChainTrust, pIndexPrev))
    {
        Log("AddNewOrigin get block trust fail, block: %s", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    CBlockIndex* pIndexNew;
    CBlockEx blockex(block);
    if (!cntrBlock.AddNew(hash, blockex, &pIndexNew, nChainTrust, pCoreProtocol->MinEnrollAmount()))
    {
        Log("AddNewOrigin Storage AddNew Error : %s ", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    Log("AddNew Origin Block : %s ", hash.ToString().c_str());
    Log("    %s", pIndexNew->ToString().c_str());

    if (!cntrBlock.CommitBlockView(view, pIndexNew))
    {
        Log("AddNewOrigin Storage Commit BlockView Error : %s ", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    update = CBlockChainUpdate(pIndexNew);
    view.GetTxUpdated(update.setTxUpdate);
    update.vBlockAddNew.push_back(blockex);

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

bool CBlockChain::GetBlockMintReward(const uint256& hashPrev, int64& nReward)
{
    CBlockIndex* pIndexPrev;
    if (!cntrBlock.RetrieveIndex(hashPrev, &pIndexPrev))
    {
        Log("Get block reward: Retrieve Prev Index Error, hashPrev: %s", hashPrev.ToString().c_str());
        return false;
    }

    if (pIndexPrev->IsPrimary())
    {
        nReward = pCoreProtocol->GetPrimaryMintWorkReward(pIndexPrev);
    }
    else
    {
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
                || block.GetBlockTime() >= pIndexRef->GetBlockTime() + BLOCK_TARGET_SPACING
                || ((block.GetBlockTime() - pIndexRef->GetBlockTime()) % EXTENDED_BLOCK_SPACING) != 0)
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

int64 CBlockChain::GetDelegateMinEnrollAmount(const uint256& hashBlock)
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

    int nMaxCertCount = CONSENSUS_ENROLL_INTERVAL * 4 / 3;
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

bool CBlockChain::GetBlockDelegateEnrolled(const uint256& hashBlock, CDelegateEnrolled& enrolled)
{
    // Log("CBlockChain::GetBlockDelegateEnrolled enter .... height: %d, hashBlock: %s", CBlock::GetBlockHeightByHash(hashBlock), hashBlock.ToString().c_str());
    enrolled.Clear();

    if (cacheEnrolled.Retrieve(hashBlock, enrolled))
    {
        return true;
    }

    CBlockIndex* pIndex;
    if (!cntrBlock.RetrieveIndex(hashBlock, &pIndex))
    {
        Log("GetBlockDelegateEnrolled : Retrieve block Index Error: %s \n", hashBlock.ToString().c_str());
        return false;
    }
    int64 nMinEnrollAmount = pCoreProtocol->MinEnrollAmount();

    if (pIndex->GetBlockHeight() < CONSENSUS_ENROLL_INTERVAL)
    {
        return true;
    }
    vector<uint256> vBlockRange;
    for (int i = 0; i < CONSENSUS_ENROLL_INTERVAL; i++)
    {
        vBlockRange.push_back(pIndex->GetBlockHash());
        pIndex = pIndex->pPrev;
    }

    if (!cntrBlock.RetrieveAvailDelegate(hashBlock, pIndex->GetBlockHeight(), vBlockRange, nMinEnrollAmount,
                                         enrolled.mapWeight, enrolled.mapEnrollData, enrolled.vecAmount))
    {
        Log("GetBlockDelegateEnrolled : Retrieve Avail Delegate Error: %s \n", hashBlock.ToString().c_str());
        return false;
    }

    cacheEnrolled.AddNew(hashBlock, enrolled);

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

uint32 CBlockChain::DPoSTimestamp(const uint256& hashPrev)
{
    CBlockIndex* pIndexPrev = nullptr;
    if (!cntrBlock.RetrieveIndex(hashPrev, &pIndexPrev) || pIndexPrev == nullptr)
    {
        return 0;
    }
    return pCoreProtocol->DPoSTimestamp(pIndexPrev);
}

Errno CBlockChain::VerifyPowBlock(const CBlock& block, bool& fLongChain)
{
    uint256 hash = block.GetHash();
    Errno err = OK;

    if (cntrBlock.Exists(hash))
    {
        Log("VerifyPowBlock Already Exists : %s ", hash.ToString().c_str());
        return ERR_ALREADY_HAVE;
    }

    err = pCoreProtocol->ValidateBlock(block);
    if (err != OK)
    {
        Log("VerifyPowBlock Validate Block Error(%s) : %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    CBlockIndex* pIndexPrev;
    if (!cntrBlock.RetrieveIndex(block.hashPrev, &pIndexPrev))
    {
        Log("VerifyPowBlock Retrieve Prev Index Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    int64 nReward = 0;
    CDelegateAgreement agreement;
    size_t nEnrollTrust = 0;
    CBlockIndex* pIndexRef = nullptr;
    err = VerifyBlock(hash, block, pIndexPrev, nReward, agreement, nEnrollTrust, &pIndexRef);
    if (err != OK)
    {
        Log("VerifyPowBlock Verify Block Error(%s) : %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    storage::CBlockView view;
    if (!cntrBlock.GetBlockView(block.hashPrev, view, !block.IsOrigin()))
    {
        Log("VerifyPowBlock Get Block View Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    if (!block.IsVacant() || !block.txMint.sendTo.IsNull())
    {
        view.AddTx(block.txMint.GetHash(), block.txMint);
    }

    CBlockEx blockex(block);
    vector<CTxContxt>& vTxContxt = blockex.vTxContxt;

    int64 nTotalFee = 0;
    if (block.vtx.size() > 0)
    {
        vTxContxt.reserve(block.vtx.size());
    }

    int nForkHeight;
    if (block.nType == block.BLOCK_EXTENDED)
    {
        nForkHeight = pIndexPrev->nHeight;
    }
    else
    {
        nForkHeight = pIndexPrev->nHeight + 1;
    }

    for (const CTransaction& tx : block.vtx)
    {
        uint256 txid = tx.GetHash();
        CTxContxt txContxt;
        err = GetTxContxt(view, tx, txContxt);
        if (err != OK)
        {
            Log("VerifyPowBlock Get txContxt Error([%d] %s) : %s ", err, ErrorString(err), txid.ToString().c_str());
            return err;
        }
        err = pCoreProtocol->VerifyBlockTx(tx, txContxt, pIndexPrev, nForkHeight, pIndexPrev->GetOriginHash());
        if (err != OK)
        {
            Log("VerifyPowBlock Verify BlockTx Error(%s) : %s ", ErrorString(err), txid.ToString().c_str());
            return err;
        }
        if (tx.nTimeStamp > block.nTimeStamp)
        {
            Log("VerifyPowBlock Verify BlockTx time fail: tx time: %d, block time: %d, tx: %s, block: %s",
                tx.nTimeStamp, block.nTimeStamp, txid.ToString().c_str(), hash.GetHex().c_str());
            return ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE;
        }

        vTxContxt.push_back(txContxt);
        view.AddTx(txid, tx, txContxt.destIn, txContxt.GetValueIn());

        StdTrace("BlockChain", "VerifyPowBlock: verify tx success, new tx: %s, new block: %s", txid.GetHex().c_str(), hash.GetHex().c_str());

        nTotalFee += tx.nTxFee;
    }

    if (block.txMint.nAmount > nTotalFee + nReward)
    {
        Log("VerifyPowBlock Mint tx amount invalid : (%ld > %ld + %ld)", block.txMint.nAmount, nTotalFee, nReward);
        return ERR_BLOCK_TRANSACTIONS_INVALID;
    }

    // Get block trust
    uint256 nNewBlockChainTrust;
    if (!pCoreProtocol->GetBlockTrust(block, nNewBlockChainTrust, pIndexPrev, agreement, pIndexRef, nEnrollTrust))
    {
        Log("VerifyPowBlock get block trust fail, block: %s", hash.GetHex().c_str());
        return ERR_BLOCK_TRANSACTIONS_INVALID;
    }
    nNewBlockChainTrust += pIndexPrev->nChainTrust;

    CBlockIndex* pIndexFork = nullptr;
    if (cntrBlock.RetrieveFork(pIndexPrev->GetOriginHash(), &pIndexFork)
        && pIndexFork->nChainTrust > nNewBlockChainTrust)
    {
        Log("VerifyPowBlock : Short chain, new block height: %d, block: %s, fork chain trust: %s, fork last block: %s",
            block.GetBlockHeight(), hash.GetHex().c_str(), pIndexFork->nChainTrust.GetHex().c_str(), pIndexFork->GetBlockHash().GetHex().c_str());
        fLongChain = false;
    }
    else
    {
        fLongChain = true;
    }

    return OK;
}

bool CBlockChain::VerifyBlockForkTx(const uint256& hashPrev, const CTransaction& tx, vector<CForkContext>& vForkCtxt)
{
    if (tx.vchData.empty())
    {
        Log("Verify block fork tx: invalid vchData, tx: %s", tx.GetHash().ToString().c_str());
        return false;
    }

    CBlock block;
    CProfile profile;
    try
    {
        CBufStream ss;
        ss.Write((const char*)&tx.vchData[0], tx.vchData.size());
        ss >> block;
        if (!block.IsOrigin() || block.IsPrimary())
        {
            Log("Verify block fork tx: invalid block, tx: %s", tx.GetHash().ToString().c_str());
            return false;
        }
        if (!profile.Load(block.vchProof))
        {
            Log("Verify block fork tx: invalid profile, tx: %s", tx.GetHash().ToString().c_str());
            return false;
        }
    }
    catch (...)
    {
        Log("Verify block fork tx: invalid vchData, tx: %s", tx.GetHash().ToString().c_str());
        return false;
    }
    uint256 hashNewFork = block.GetHash();

    do
    {
        CForkContext ctxtParent;
        if (!cntrBlock.RetrieveForkContext(profile.hashParent, ctxtParent))
        {
            bool fFindParent = false;
            for (const auto& vd : vForkCtxt)
            {
                if (vd.hashFork == profile.hashParent)
                {
                    ctxtParent = vd;
                    fFindParent = true;
                    break;
                }
            }
            if (!fFindParent)
            {
                Log("Verify block fork tx: Retrieve parent context, tx: %s", tx.GetHash().ToString().c_str());
                break;
            }
        }

        CProfile forkProfile;
        Errno err = pCoreProtocol->ValidateOrigin(block, ctxtParent.GetProfile(), forkProfile);
        if (err != OK)
        {
            Log("Verify block fork tx: Validate origin Error(%s), tx: %s", ErrorString(err), tx.GetHash().ToString().c_str());
            break;
        }

        if (!pForkManager->VerifyFork(hashPrev, hashNewFork, profile.strName))
        {
            Log("Verify block fork tx: verify fork fail, tx: %s", tx.GetHash().ToString().c_str());
            break;
        }
        bool fCheckRet = true;
        for (const auto& vd : vForkCtxt)
        {
            if (vd.hashFork == hashNewFork || vd.strName == profile.strName)
            {
                Log("Verify block fork tx: fork exist, tx: %s", tx.GetHash().ToString().c_str());
                fCheckRet = false;
                break;
            }
        }
        if (!fCheckRet)
        {
            break;
        }

        vForkCtxt.push_back(CForkContext(block.GetHash(), block.hashPrev, tx.GetHash(), profile));
    } while (0);

    return true;
}

bool CBlockChain::CheckForkValidLast(const uint256& hashFork, CBlockChainUpdate& update)
{
    CBlockIndex* pValidLastIndex = cntrBlock.GetForkValidLast(pCoreProtocol->GetGenesisBlockHash(), hashFork, pCoreProtocol->GetRefVacantHeight());
    if (pValidLastIndex == nullptr)
    {
        return false;
    }

    storage::CBlockView view;
    if (!cntrBlock.GetBlockView(pValidLastIndex->GetBlockHash(), view, true))
    {
        StdLog("BlockChain", "CheckForkValidLast: Get Block View Error, last block: %s, fork: %s",
               pValidLastIndex->GetBlockHash().ToString().c_str(), hashFork.GetHex().c_str());
        return false;
    }

    if (!cntrBlock.CommitBlockView(view, pValidLastIndex))
    {
        StdLog("BlockChain", "CheckForkValidLast: Storage Commit BlockView Error, last block: %s, fork: %s",
               pValidLastIndex->GetBlockHash().ToString().c_str(), hashFork.GetHex().c_str());
        return false;
    }

    StdLog("BlockChain", "CheckForkValidLast: Repair fork last success, last block: %s, fork: %s",
           pValidLastIndex->GetBlockHash().ToString().c_str(), hashFork.GetHex().c_str());

    update = CBlockChainUpdate(pValidLastIndex);
    view.GetTxUpdated(update.setTxUpdate);
    view.GetBlockChanges(update.vBlockAddNew, update.vBlockRemove);

    if (update.IsNull())
    {
        StdLog("BlockChain", "CheckForkValidLast: update is null, last block: %s, fork: %s",
               pValidLastIndex->GetBlockHash().ToString().c_str(), hashFork.GetHex().c_str());
    }
    if (update.vBlockAddNew.empty())
    {
        StdLog("BlockChain", "CheckForkValidLast: vBlockAddNew is empty, last block: %s, fork: %s",
               pValidLastIndex->GetBlockHash().ToString().c_str(), hashFork.GetHex().c_str());
    }
    return true;
}

bool CBlockChain::VerifyForkRefLongChain(const uint256& hashFork, const uint256& hashForkBlock, const uint256& hashPrimaryBlock)
{
    uint256 hashRefBlock;
    bool fOrigin = false;
    if (!cntrBlock.GetLastRefBlockHash(hashFork, hashForkBlock, hashRefBlock, fOrigin))
    {
        StdLog("BlockChain", "VerifyForkRefLongChain: Get ref block fail, last block: %s, fork: %s",
               hashForkBlock.GetHex().c_str(), hashFork.GetHex().c_str());
        return false;
    }
    if (!fOrigin)
    {
        if (!cntrBlock.VerifySameChain(hashRefBlock, hashPrimaryBlock))
        {
            StdLog("BlockChain", "VerifyForkRefLongChain: Fork does not refer to long chain, fork last: %s, ref block: %s, primayr block: %s, fork: %s",
                   hashForkBlock.GetHex().c_str(), hashRefBlock.GetHex().c_str(),
                   hashPrimaryBlock.GetHex().c_str(), hashFork.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CBlockChain::GetPrimaryHeightBlockTime(const uint256& hashLastBlock, int nHeight, uint256& hashBlock, int64& nTime)
{
    return cntrBlock.GetPrimaryHeightBlockTime(hashLastBlock, nHeight, hashBlock, nTime);
}

bool CBlockChain::IsVacantBlockBeforeCreatedForkHeight(const uint256& hashFork, const CBlock& block)
{
    uint256 hashPrimaryLastBlock;
    int nTempHeight;
    int64 nTempTime;
    uint16 nTempMintType;
    if (!GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashPrimaryLastBlock, nTempHeight, nTempTime, nTempMintType))
    {
        StdError("BlockChain", "AddNewBlock: GetLastBlock fail");
        return true;
    }

    int nCreatedHeight = pForkManager->GetValidForkCreatedHeight(hashPrimaryLastBlock, hashFork);
    if (nCreatedHeight < 0)
    {
        return true;
    }

    int nOriginHeight = CBlock::GetBlockHeightByHash(hashFork);
    int nTargetHeight = block.GetBlockHeight();

    if (nTargetHeight < nCreatedHeight)
    {
        Log("Target Block Is Vacant %s at height %d in range of (%d, %d)", block.IsVacant() ? "true" : "false", nTargetHeight, nOriginHeight, nCreatedHeight);
        return block.IsVacant();
    }

    return true;
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
    uint256 nChainTrust;
    if (!pCoreProtocol->GetBlockTrust(block, nChainTrust))
    {
        return false;
    }
    return cntrBlock.Initiate(block.GetHash(), block, nChainTrust);
}

Errno CBlockChain::GetTxContxt(storage::CBlockView& view, const CTransaction& tx, CTxContxt& txContxt)
{
    txContxt.SetNull();
    for (const CTxIn& txin : tx.vInput)
    {
        CTxOut output;
        if (!view.RetrieveUnspent(txin.prevout, output))
        {
            Log("GetTxContxt: RetrieveUnspent fail, prevout: [%d]:%s", txin.prevout.n, txin.prevout.hash.GetHex().c_str());
            return ERR_MISSING_PREV;
        }
        if (txContxt.destIn.IsNull())
        {
            txContxt.destIn = output.destTo;
        }
        else if (txContxt.destIn != output.destTo)
        {
            Log("GetTxContxt: destIn error, destIn: %s, destTo: %s",
                txContxt.destIn.ToString().c_str(), output.destTo.ToString().c_str());
            return ERR_TRANSACTION_INVALID;
        }
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
                                            CDelegateAgreement& agreement, size_t& nEnrollTrust)
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
        Log("GetBlockDelegateAgreement : GetBlockDelegateEnrolled fail, block: %s", hashBlock.ToString().c_str());
        return false;
    }

    delegate::CDelegateVerify verifier(enrolled.mapWeight, enrolled.mapEnrollData);
    map<CDestination, size_t> mapBallot;
    if (!verifier.VerifyProof(block.vchProof, agreement.nAgreement, agreement.nWeight, mapBallot, pCoreProtocol->DPoSConsensusCheckRepeated(block.GetBlockHeight())))
    {
        Log("GetBlockDelegateAgreement : Invalid block proof : %s", hashBlock.ToString().c_str());
        return false;
    }

    pCoreProtocol->GetDelegatedBallot(agreement.nAgreement, agreement.nWeight, mapBallot, enrolled.vecAmount,
                                      pIndex->GetMoneySupply(), agreement.vBallot, nEnrollTrust, pIndexPrev->GetBlockHeight() + 1);

    cacheAgreement.AddNew(hashBlock, agreement);

    return true;
}

bool CBlockChain::GetBlockDelegateAgreement(const uint256& hashBlock, CDelegateAgreement& agreement)
{
    agreement.Clear();

    if (cacheAgreement.Retrieve(hashBlock, agreement))
    {
        return true;
    }

    CBlockIndex* pIndex = nullptr;
    if (!cntrBlock.RetrieveIndex(hashBlock, &pIndex))
    {
        Log("GetBlockDelegateAgreement : Retrieve block Index Error: %s \n", hashBlock.ToString().c_str());
        return false;
    }

    CBlockIndex* pIndexRef = pIndex;
    if (pIndex->GetBlockHeight() < CONSENSUS_INTERVAL)
    {
        return true;
    }

    CBlock block;
    if (!cntrBlock.Retrieve(pIndex, block))
    {
        Log("GetBlockDelegateAgreement : Retrieve block Error: %s \n", hashBlock.ToString().c_str());
        return false;
    }

    for (int i = 0; i < CONSENSUS_DISTRIBUTE_INTERVAL + 1; i++)
    {
        pIndex = pIndex->pPrev;
    }

    CDelegateEnrolled enrolled;
    if (!GetBlockDelegateEnrolled(pIndex->GetBlockHash(), enrolled))
    {
        Log("GetBlockDelegateAgreement : Get delegate enrolled fail, block: %s", hashBlock.ToString().c_str());
        return false;
    }

    delegate::CDelegateVerify verifier(enrolled.mapWeight, enrolled.mapEnrollData);
    map<CDestination, size_t> mapBallot;
    if (!verifier.VerifyProof(block.vchProof, agreement.nAgreement, agreement.nWeight, mapBallot, pCoreProtocol->DPoSConsensusCheckRepeated(block.GetBlockHeight())))
    {
        Log("GetBlockDelegateAgreement : Invalid block proof : %s \n", hashBlock.ToString().c_str());
        return false;
    }

    size_t nEnrollTrust = 0;
    pCoreProtocol->GetDelegatedBallot(agreement.nAgreement, agreement.nWeight, mapBallot, enrolled.vecAmount,
                                      pIndex->GetMoneySupply(), agreement.vBallot, nEnrollTrust, pIndexRef->GetBlockHeight());

    cacheAgreement.AddNew(hashBlock, agreement);

    return true;
}

Errno CBlockChain::VerifyBlock(const uint256& hashBlock, const CBlock& block, CBlockIndex* pIndexPrev,
                               int64& nReward, CDelegateAgreement& agreement, size_t& nEnrollTrust, CBlockIndex** ppIndexRef)
{
    nReward = 0;
    if (block.IsOrigin())
    {
        Log("Verify block : Is origin, block: %s", hashBlock.GetHex().c_str());
        return ERR_BLOCK_INVALID_FORK;
    }

    if (block.IsPrimary())
    {
        if (!pIndexPrev->IsPrimary())
        {
            Log("Verify block : Prev block not is primary, prev: %s, block: %s",
                pIndexPrev->GetBlockHash().GetHex().c_str(), hashBlock.GetHex().c_str());
            return ERR_BLOCK_INVALID_FORK;
        }

        if (!VerifyBlockCertTx(block))
        {
            Log("Verify block : Verify cert tx fail, block: %s", hashBlock.GetHex().c_str());
            return ERR_BLOCK_CERTTX_OUT_OF_BOUND;
        }

        if (!GetBlockDelegateAgreement(hashBlock, block, pIndexPrev, agreement, nEnrollTrust))
        {
            Log("Verify block : Get agreement fail, block: %s", hashBlock.GetHex().c_str());
            return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
        }

        if (!GetBlockMintReward(block.hashPrev, nReward))
        {
            Log("Verify block : Get mint reward fail, block: %s", hashBlock.GetHex().c_str());
            return ERR_BLOCK_COINBASE_INVALID;
        }

        if (!pCoreProtocol->IsDposHeight(pIndexPrev->GetBlockHeight() + 1))
        {
            if (!agreement.IsProofOfWork())
            {
                Log("Verify block : POW stage not is pow block, block: %s", hashBlock.GetHex().c_str());
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }
            return pCoreProtocol->VerifyProofOfWork(block, pIndexPrev);
        }
        else
        {
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
    else if (block.IsSubsidiary() || block.IsExtended())
    {
        if (pIndexPrev->IsPrimary())
        {
            Log("Verify block : SubFork prev not is primary, prev: %s, block: %s",
                pIndexPrev->GetBlockHash().GetHex().c_str(), hashBlock.GetHex().c_str());
            return ERR_BLOCK_INVALID_FORK;
        }

        CProofOfPiggyback proof;
        if (!proof.Load(block.vchProof) || proof.hashRefBlock == 0)
        {
            Log("Verify block : SubFork load proof fail, block: %s", hashBlock.GetHex().c_str());
            return ERR_BLOCK_INVALID_FORK;
        }

        CDelegateAgreement agreement;
        if (!GetBlockDelegateAgreement(proof.hashRefBlock, agreement))
        {
            Log("Verify block : SubFork get agreement fail, block: %s", hashBlock.GetHex().c_str());
            return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
        }

        if (agreement.nAgreement != proof.nAgreement || agreement.nWeight != proof.nWeight
            || agreement.IsProofOfWork())
        {
            Log("Verify block : SubFork agreement error, ref agreement: %s, block agreement: %s, ref weight: %d, block weight: %d, type: %s, block: %s",
                agreement.nAgreement.GetHex().c_str(), proof.nAgreement.GetHex().c_str(),
                agreement.nWeight, proof.nWeight, (agreement.IsProofOfWork() ? "pow" : "dpos"),
                hashBlock.GetHex().c_str());
            return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
        }

        if (!cntrBlock.RetrieveIndex(proof.hashRefBlock, ppIndexRef) || *ppIndexRef == nullptr || !(*ppIndexRef)->IsPrimary())
        {
            Log("Verify block : SubFork retrieve ref index fail, ref block: %s, block: %s",
                proof.hashRefBlock.GetHex().c_str(), hashBlock.GetHex().c_str());
            return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
        }

        CProofOfPiggyback proofPrev;
        if (!pIndexPrev->IsOrigin() && (!pIndexPrev->IsVacant() || pCoreProtocol->IsRefVacantHeight(pIndexPrev->GetBlockHeight())))
        {
            CBlock blockPrev;
            if (!cntrBlock.Retrieve(pIndexPrev, blockPrev))
            {
                Log("Verify block : SubFork retrieve prev index fail, block: %s", hashBlock.GetHex().c_str());
                return ERR_MISSING_PREV;
            }
            if (!proofPrev.Load(blockPrev.vchProof) || proofPrev.hashRefBlock == 0)
            {
                Log("Verify block : SubFork load prev proof fail, block: %s", hashBlock.GetHex().c_str());
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }
            if (proof.hashRefBlock != proofPrev.hashRefBlock
                && !cntrBlock.VerifySameChain(proofPrev.hashRefBlock, proof.hashRefBlock))
            {
                Log("Verify block : SubFork verify same chain fail, prev ref: %s, block ref: %s, block: %s",
                    proofPrev.hashRefBlock.GetHex().c_str(), proof.hashRefBlock.GetHex().c_str(), hashBlock.GetHex().c_str());
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }
        }

        if (block.IsExtended())
        {
            if (pIndexPrev->IsOrigin() || pIndexPrev->IsVacant())
            {
                Log("Verify block : SubFork extended prev is origin or vacant, prev: %s, block: %s",
                    pIndexPrev->GetBlockHash().GetHex().c_str(), hashBlock.GetHex().c_str());
                return ERR_MISSING_PREV;
            }
            if (proof.nAgreement != proofPrev.nAgreement || proof.nWeight != proofPrev.nWeight)
            {
                Log("Verify block : SubFork extended agreement error, block: %s", hashBlock.GetHex().c_str());
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }
            nReward = 0;
        }
        else
        {
            if (!GetBlockMintReward(block.hashPrev, nReward))
            {
                Log("Verify block : SubFork get mint reward error, block: %s", hashBlock.GetHex().c_str());
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }
        }

        return pCoreProtocol->VerifySubsidiary(block, pIndexPrev, *ppIndexRef, agreement);
    }
    else if (block.IsVacant())
    {
        if (!pCoreProtocol->IsRefVacantHeight(block.GetBlockHeight()))
        {
            if (block.GetBlockTime() < pIndexPrev->GetBlockTime())
            {
                Log("Verify block : Vacant time error, block time: %d, prev time: %d, block: %s",
                    block.GetBlockTime(), pIndexPrev->GetBlockTime(), hashBlock.GetHex().c_str());
                return ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE;
            }
        }
        else
        {
            CProofOfPiggyback proof;
            if (!proof.Load(block.vchProof) || proof.hashRefBlock == 0)
            {
                Log("Verify block : Vacant load proof error, block: %s", hashBlock.GetHex().c_str());
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }

            CDelegateAgreement agreement;
            if (!GetBlockDelegateAgreement(proof.hashRefBlock, agreement))
            {
                Log("Verify block : Vacant get agreement fail, block: %s", hashBlock.GetHex().c_str());
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }

            if (agreement.nAgreement != proof.nAgreement || agreement.nWeight != proof.nWeight
                || agreement.IsProofOfWork())
            {
                Log("Verify block : Vacant agreement error, ref agreement: %s, block agreement: %s, ref weight: %d, block weight: %d, type: %s, block: %s",
                    agreement.nAgreement.GetHex().c_str(), proof.nAgreement.GetHex().c_str(),
                    agreement.nWeight, proof.nWeight, (agreement.IsProofOfWork() ? "pow" : "dpos"),
                    hashBlock.GetHex().c_str());
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }

            if (block.txMint.sendTo != agreement.GetBallot(0))
            {
                Log("Verify block : Vacant sendTo error, sendTo: %s, ballot: %s, block: %s",
                    CAddress(block.txMint.sendTo).ToString().c_str(),
                    CAddress(agreement.GetBallot(0)).ToString().c_str(),
                    hashBlock.GetHex().c_str());
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }

            if (block.txMint.nTimeStamp != block.GetBlockTime())
            {
                Log("Verify block : Vacant txMint timestamp error, mint tx time: %d, block time: %d, block: %s",
                    block.txMint.nTimeStamp, block.GetBlockTime(), hashBlock.GetHex().c_str());
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }

            if (!cntrBlock.RetrieveIndex(proof.hashRefBlock, ppIndexRef) || *ppIndexRef == nullptr || !(*ppIndexRef)->IsPrimary())
            {
                Log("Verify block : Vacant retrieve ref index fail, ref: %s, block: %s",
                    proof.hashRefBlock.GetHex().c_str(), hashBlock.GetHex().c_str());
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }

            if (!pIndexPrev->IsOrigin() && (!pIndexPrev->IsVacant() || pCoreProtocol->IsRefVacantHeight(pIndexPrev->GetBlockHeight())))
            {
                CBlock blockPrev;
                if (!cntrBlock.Retrieve(pIndexPrev, blockPrev))
                {
                    Log("Verify block : Vacant retrieve prev index fail, block: %s", hashBlock.GetHex().c_str());
                    return ERR_MISSING_PREV;
                }
                CProofOfPiggyback proofPrev;
                if (!proofPrev.Load(blockPrev.vchProof) || proofPrev.hashRefBlock == 0)
                {
                    Log("Verify block : Vacant load prev proof fail, block: %s", hashBlock.GetHex().c_str());
                    return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
                }
                if (proof.hashRefBlock != proofPrev.hashRefBlock
                    && !cntrBlock.VerifySameChain(proofPrev.hashRefBlock, proof.hashRefBlock))
                {
                    Log("Verify block : Vacant verify same chain fail, prev ref: %s, block ref: %s, block: %s",
                        proofPrev.hashRefBlock.GetHex().c_str(), proof.hashRefBlock.GetHex().c_str(), hashBlock.GetHex().c_str());
                    return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
                }
            }

            uint256 hashPrimaryBlock;
            int64 nPrimaryTime = 0;
            if (!cntrBlock.GetPrimaryHeightBlockTime((*ppIndexRef)->GetBlockHash(), block.GetBlockHeight(), hashPrimaryBlock, nPrimaryTime))
            {
                Log("Verify block : Vacant get height time, block ref: %s, block: %s",
                    (*ppIndexRef)->GetBlockHash().GetHex().c_str(), hashBlock.GetHex().c_str());
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }
            if (block.GetBlockTime() != nPrimaryTime)
            {
                Log("Verify block : Vacant time error, block time: %d, primary time: %d, ref block: %s, same height block: %s, block: %s",
                    block.GetBlockTime(), nPrimaryTime, (*ppIndexRef)->GetBlockHash().GetHex().c_str(),
                    hashPrimaryBlock.GetHex().c_str(), hashBlock.GetHex().c_str());
                return ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE;
            }
        }
    }
    else
    {
        Log("Verify block : block type error, nType: %d, block: %s", block.nType, hashBlock.GetHex().c_str());
        return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
    }

    return OK;
}

bool CBlockChain::VerifyBlockCertTx(const CBlock& block)
{
    map<CDestination, int> mapBlockCert;
    for (const auto& d : block.vtx)
    {
        if (d.nType == CTransaction::TX_CERT)
        {
            ++mapBlockCert[d.sendTo];
        }
    }
    if (!mapBlockCert.empty())
    {
        map<CDestination, int64> mapVote;
        if (!GetBlockDelegateVote(block.hashPrev, mapVote))
        {
            StdError("CBlockChain", "VerifyBlockCertTx: GetBlockDelegateVote fail");
            return false;
        }
        map<CDestination, int> mapVoteCert;
        if (!GetDelegateCertTxCount(block.hashPrev, mapVoteCert))
        {
            StdError("CBlockChain", "VerifyBlockCertTx: GetBlockDelegateVote fail");
            return false;
        }
        int64 nMinAmount = pCoreProtocol->MinEnrollAmount();
        for (const auto& d : mapBlockCert)
        {
            const CDestination& dest = d.first;
            map<CDestination, int64>::iterator mt = mapVote.find(dest);
            if (mt == mapVote.end() || mt->second < nMinAmount)
            {
                StdLog("CBlockChain", "VerifyBlockCertTx: not enough votes, votes: %ld, dest: %s",
                       (mt == mapVote.end() ? 0 : mt->second), CAddress(dest).ToString().c_str());
                return false;
            }
            map<CDestination, int>::iterator it = mapVoteCert.find(dest);
            if (it != mapVoteCert.end() && d.second > it->second)
            {
                StdLog("CBlockChain", "VerifyBlockCertTx: more than votes, block cert count: %d, available cert count: %d, dest: %s",
                       d.second, it->second, CAddress(dest).ToString().c_str());
                return false;
            }
        }
    }
    return true;
}

void CBlockChain::InitCheckPoints(const uint256& hashFork, const std::vector<CCheckPoint>& vCheckPointsIn)
{
    mapForkCheckPoints.insert(std::make_pair(hashFork, MapCheckPointsType()));
    for (const auto& point : vCheckPointsIn)
    {
        MapCheckPointsType& mapCheckPointType = mapForkCheckPoints[hashFork];
        mapCheckPointType.insert(std::make_pair(point.nHeight, point));
    }
}

void CBlockChain::InitCheckPoints()
{
    if (Config()->nMagicNum == MAINNET_MAGICNUM)
    {
        std::vector<CCheckPoint> vecGenesisCheckPoints, vecBBCN;
#ifdef BIGBANG_TESTNET
        vecGenesisCheckPoints.push_back(CCheckPoint(0, pCoreProtocol->GetGenesisBlockHash()));
        InitCheckPoints(pCoreProtocol->GetGenesisBlockHash(), vecGenesisCheckPoints);
#else
        for (const auto& vd : vCheckPoints)
        {
            vecGenesisCheckPoints.push_back(CCheckPoint(vd.first, vd.second));
        }

        vecBBCN.assign(
            { { 300000, uint256("000493e02a0f6ef977ebd5f844014badb412b6db85847b120f162b8d913b9028") },
              { 335999, uint256("0005207f9be2e4f1a278054058d4c17029ae6733cc8f7163a4e3099000deb9ff") } });

        InitCheckPoints(uint256("00000000b0a9be545f022309e148894d1e1c853ccac3ef04cb6f5e5c70f41a70"), vecGenesisCheckPoints);
        InitCheckPoints(uint256("000493e02a0f6ef977ebd5f844014badb412b6db85847b120f162b8d913b9028"), vecBBCN);
#endif
    }
}

bool CBlockChain::AddBlockForkContext(const CBlockEx& blockex)
{
    uint256 hashBlock = blockex.GetHash();
    vector<CForkContext> vForkCtxt;
    for (int i = 0; i < blockex.vtx.size(); i++)
    {
        const CTransaction& tx = blockex.vtx[i];
        const CTxContxt& txContxt = blockex.vTxContxt[i];
        if (tx.sendTo != txContxt.destIn)
        {
            if (tx.sendTo.GetTemplateId().GetType() == TEMPLATE_FORK)
            {
                if (!VerifyBlockForkTx(blockex.hashPrev, tx, vForkCtxt))
                {
                    StdLog("CBlockChain", "AddBlockForkContext: VerifyBlockForkTx fail, block: %s", hashBlock.ToString().c_str());
                    return false;
                }
            }
            if (txContxt.destIn.GetTemplateId().GetType() == TEMPLATE_FORK)
            {
                CDestination destRedeem;
                uint256 hashFork;
                if (!pCoreProtocol->GetTxForkRedeemParam(tx, txContxt.destIn, destRedeem, hashFork))
                {
                    StdLog("CBlockChain", "AddBlockForkContext: Get redeem param fail, block: %s, dest: %s",
                           hashBlock.ToString().c_str(), CAddress(txContxt.destIn).ToString().c_str());
                    return false;
                }
                auto it = vForkCtxt.begin();
                while (it != vForkCtxt.end())
                {
                    if (it->hashFork == hashFork)
                    {
                        StdLog("CBlockChain", "AddBlockForkContext: cancel fork, block: %s, fork: %s, dest: %s",
                               hashBlock.ToString().c_str(), it->hashFork.ToString().c_str(),
                               CAddress(txContxt.destIn).ToString().c_str());
                        vForkCtxt.erase(it++);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
        }
    }

    for (const CForkContext& ctxt : vForkCtxt)
    {
        if (!cntrBlock.AddNewForkContext(ctxt))
        {
            StdLog("CBlockChain", "AddBlockForkContext: AddNewForkContext fail, block: %s", hashBlock.ToString().c_str());
            return false;
        }
    }

    bool fCheckPointBlock = false;
    const auto mt = mapForkCheckPoints.find(pCoreProtocol->GetGenesisBlockHash());
    if (mt != mapForkCheckPoints.end())
    {
        const auto it = mt->second.find(CBlock::GetBlockHeightByHash(hashBlock));
        if (it != mt->second.end() && it->second.nBlockHash == hashBlock)
        {
            fCheckPointBlock = true;
        }
    }

    uint256 hashRefFdBlock;
    map<uint256, int> mapValidFork;
    if (!pForkManager->AddForkContext(blockex.hashPrev, hashBlock, vForkCtxt, fCheckPointBlock, hashRefFdBlock, mapValidFork))
    {
        StdLog("CBlockChain", "AddBlockForkContext: AddForkContext fail, block: %s", hashBlock.ToString().c_str());
        return false;
    }

    if (!cntrBlock.AddValidForkHash(hashBlock, hashRefFdBlock, mapValidFork))
    {
        StdLog("CBlockChain", "AddBlockForkContext: AddValidForkHash fail, block: %s", hashBlock.ToString().c_str());
        return false;
    }
    return true;
}

bool CBlockChain::HasCheckPoints(const uint256& hashFork) const
{
    auto iter = mapForkCheckPoints.find(hashFork);
    if (iter != mapForkCheckPoints.end())
    {
        return iter->second.size() > 0;
    }
    else
    {
        return false;
    }
}

bool CBlockChain::GetCheckPointByHeight(const uint256& hashFork, int nHeight, CCheckPoint& point)
{
    auto iter = mapForkCheckPoints.find(hashFork);
    if (iter != mapForkCheckPoints.end())
    {
        if (iter->second.count(nHeight) == 0)
        {
            return false;
        }
        else
        {
            point = iter->second[nHeight];
            return true;
        }
    }
    else
    {
        return false;
    }
}

std::vector<IBlockChain::CCheckPoint> CBlockChain::CheckPoints(const uint256& hashFork) const
{
    auto iter = mapForkCheckPoints.find(hashFork);
    if (iter != mapForkCheckPoints.end())
    {
        std::vector<IBlockChain::CCheckPoint> points;
        for (const auto& kv : iter->second)
        {
            points.push_back(kv.second);
        }

        return points;
    }

    return std::vector<IBlockChain::CCheckPoint>();
}

IBlockChain::CCheckPoint CBlockChain::LatestCheckPoint(const uint256& hashFork) const
{
    if (!HasCheckPoints(hashFork))
    {
        return IBlockChain::CCheckPoint();
    }
    return mapForkCheckPoints.at(hashFork).rbegin()->second;
}

IBlockChain::CCheckPoint CBlockChain::UpperBoundCheckPoint(const uint256& hashFork, int nHeight) const
{
    if (!HasCheckPoints(hashFork))
    {
        return IBlockChain::CCheckPoint();
    }

    auto& forkCheckPoints = mapForkCheckPoints.at(hashFork);
    auto iter = forkCheckPoints.upper_bound(nHeight);
    return (iter != forkCheckPoints.end()) ? IBlockChain::CCheckPoint(iter->second) : IBlockChain::CCheckPoint();
}

bool CBlockChain::VerifyCheckPoint(const uint256& hashFork, int nHeight, const uint256& nBlockHash)
{
    if (!HasCheckPoints(hashFork))
    {
        return true;
    }

    CCheckPoint point;
    if (!GetCheckPointByHeight(hashFork, nHeight, point))
    {
        return true;
    }

    if (nBlockHash != point.nBlockHash)
    {
        return false;
    }

    Log("HashFork %s Verified checkpoint at height %d/block %s", hashFork.ToString().c_str(), point.nHeight, point.nBlockHash.ToString().c_str());

    return true;
}

bool CBlockChain::FindPreviousCheckPointBlock(const uint256& hashFork, CBlock& block)
{
    if (!HasCheckPoints(hashFork))
    {
        return true;
    }

    const auto& points = CheckPoints(hashFork);
    int numCheckpoints = points.size();
    for (int i = numCheckpoints - 1; i >= 0; i--)
    {
        const CCheckPoint& point = points[i];

        uint256 hashBlock;
        if (!GetBlockHash(hashFork, point.nHeight, hashBlock))
        {
            StdTrace("BlockChain", "HashFork %s CheckPoint(%d, %s) doest not exists and continuely try to get previous checkpoint",
                     hashFork.ToString().c_str(), point.nHeight, point.nBlockHash.ToString().c_str());

            continue;
        }

        if (hashBlock != point.nBlockHash)
        {
            StdError("BlockChain", "CheckPoint(%d, %s)  does not match block hash %s",
                     point.nHeight, point.nBlockHash.ToString().c_str(), hashBlock.ToString().c_str());
            return false;
        }

        return GetBlock(hashBlock, block);
    }

    return true;
}

bool CBlockChain::IsSameBranch(const uint256& hashFork, const CBlock& block)
{
    uint256 bestChainBlockHash;
    if (!GetBlockHash(hashFork, block.GetBlockHeight(), bestChainBlockHash))
    {
        return true;
    }

    return block.GetHash() == bestChainBlockHash;
}

} // namespace bigbang
