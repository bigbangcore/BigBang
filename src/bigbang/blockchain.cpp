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
    if (!cntrBlock.Initialize(Config()->pathData, Config()->fDebug))
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
        CBlock block;
        pCoreProtocol->GetGenesisBlock(block);
        if (!InsertGenesisBlock(block))
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

        map<uint256, CForkStatus>::iterator mi = mapForkStatus.insert(make_pair(hashFork, CForkStatus(hashFork, hashParent, nForkHeight))).first;
        CForkStatus& status = (*mi).second;
        status.hashLastBlock = pIndex->GetBlockHash();
        status.nLastBlockTime = pIndex->GetBlockTime();
        status.nLastBlockHeight = pIndex->GetBlockHeight();
        status.nMoneySupply = pIndex->GetMoneySupply();
        status.nMintType = pIndex->nMintType;
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
        if (!pTxPool->Exists(txid))
        {
            err = pCoreProtocol->VerifyBlockTx(tx, txContxt, pIndexPrev, nForkHeight, pIndexPrev->GetOriginHash());
            if (err != OK)
            {
                Log("AddNewBlock Verify BlockTx Error(%s) : %s ", ErrorString(err), txid.ToString().c_str());
                return err;
            }
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
                || ((block.GetBlockTime() - pIndexRef->GetBlockTime()) / EXTENDED_BLOCK_SPACING) != 0)
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
        if (!pTxPool->Exists(txid))
        {
            err = pCoreProtocol->VerifyBlockTx(tx, txContxt, pIndexPrev, nForkHeight, pIndexPrev->GetOriginHash());
            if (err != OK)
            {
                Log("VerifyPowBlock Verify BlockTx Error(%s) : %s ", ErrorString(err), txid.ToString().c_str());
                return err;
            }
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
    int nCreatedHeight = -1;
    if (!pForkManager->GetCreatedHeight(hashFork, nCreatedHeight))
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

        CBlock blockPrev;
        if (!cntrBlock.Retrieve(pIndexPrev, blockPrev))
        {
            Log("Verify block : SubFork retrieve prev index fail, block: %s", hashBlock.GetHex().c_str());
            return ERR_MISSING_PREV;
        }

        CProofOfPiggyback proofPrev;
        if (!blockPrev.IsVacant() || pCoreProtocol->IsRefVacantHeight(blockPrev.GetBlockHeight()))
        {
            if (!proofPrev.Load(blockPrev.vchProof) || proofPrev.hashRefBlock == 0)
            {
                Log("Verify block : SubFork load prev proof fail, block: %s", hashBlock.GetHex().c_str());
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }
            if (proof.hashRefBlock != proofPrev.hashRefBlock
                && !cntrBlock.VerifySameChain(proofPrev.hashRefBlock, proof.hashRefBlock))
            {
                Log("Verify block : SubFork verify same chain fail, block: %s", hashBlock.GetHex().c_str());
                return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
            }
        }

        if (block.IsExtended())
        {
            if (blockPrev.IsVacant())
            {
                Log("Verify block : SubFork extended prev is vacant, prev: %s, block: %s",
                    blockPrev.GetHash().GetHex().c_str(), hashBlock.GetHex().c_str());
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

            CBlock blockPrev;
            if (!cntrBlock.Retrieve(pIndexPrev, blockPrev))
            {
                Log("Verify block : Vacant retrieve prev index fail, block: %s", hashBlock.GetHex().c_str());
                return ERR_MISSING_PREV;
            }

            if (!blockPrev.IsVacant() || pCoreProtocol->IsRefVacantHeight(blockPrev.GetBlockHeight()))
            {
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

void CBlockChain::InitCheckPoints(const uint256& hashFork, const std::vector<CCheckPoint>& vCheckPoints)
{
    mapForkCheckPoints.insert(std::make_pair(hashFork, MapCheckPointsType()));
    for (const auto& point : vCheckPoints)
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
        vecGenesisCheckPoints.push_back(CCheckPoint(56496, uint256("0000dcb0c5b56c12fc06ecc15ed8322e560402f04b417482a2fef9157db759b4")));

        InitCheckPoints(pCoreProtocol->GetGenesisBlockHash(), vecGenesisCheckPoints);

        {
            std::vector<CCheckPoint> vecCheckPoints;
            vecCheckPoints.assign(
                { { 56496, uint256("0000dcb00e0c099c80d7090b216d161bbab152bf1f277cc1df9490700874dbff") } });

            InitCheckPoints(uint256("0000001f9a046730bf5102283f43fe51bd1c1b913b3b931c1566d9c5e1463a7e"), vecCheckPoints);
        }

        {
            std::vector<CCheckPoint> vecCheckPoints;
            vecCheckPoints.assign(
                { { 56496, uint256("0000dcb027555dafd8743c7e5426c77a206a88947e5069c6f56e738b9b6457d0") } });

            InitCheckPoints(uint256("00001195d2d0771094ec8459f0b375bab1e0dd75f179cf6f93e678ac86e8bd32"), vecCheckPoints);
        }

        {
            std::vector<CCheckPoint> vecCheckPoints;
            vecCheckPoints.assign(
                { { 56496, uint256("0000dcb0412bc768b7f2ea1bfc4870590ba8f5adec26183857c6bc1f5d2b095c") } });

            InitCheckPoints(uint256("000038731942c3096b32df1c39e1dae1c163a392158d666582a7abf751cca2d0"), vecCheckPoints);
        }

#else
        vecGenesisCheckPoints.assign(
            { { 0, uint256("00000000b0a9be545f022309e148894d1e1c853ccac3ef04cb6f5e5c70f41a70") },
              { 100, uint256("000000649ec479bb9944fb85905822cb707eb2e5f42a5d58e598603b642e225d") },
              { 1000, uint256("000003e86cc97e8b16aaa92216a66c2797c977a239bbd1a12476bad68580be73") },
              { 2000, uint256("000007d07acd442c737152d0cd9d8e99b6f0177781323ccbe20407664e01da8f") },
              { 5000, uint256("00001388dbb69842b373352462b869126b9fe912b4d86becbb3ad2bf1d897840") },
              { 10000, uint256("00002710c3f3cd6c931f568169c645e97744943e02b0135aae4fcb3139c0fa6f") },
              { 16000, uint256("00003e807c1e13c95e8601d7e870a1e13bc708eddad137a49ba6c0628ce901df") },
              { 23000, uint256("000059d889977b9d0cd3d3fa149aa4c6e9c9da08c05c016cb800d52b2ecb620c") },
              { 31000, uint256("000079188913bbe13cb3ff76df2ba2f9d2180854750ab9a37dc8d197668d2215") },
              { 40000, uint256("00009c40c22952179a522909e8bec05617817952f3b9aebd1d1e096413fead5b") },
              { 50000, uint256("0000c3506e5e7fae59bee39965fb45e284f86c993958e5ce682566810832e7e8") },
              { 70000, uint256("000111701e15e979b4633e45b762067c6369e6f0ca8284094f6ce476b10f50de") },
              { 90000, uint256("00015f902819ebe9915f30f0faeeb08e7cd063b882d9066af898a1c67257927c") },
              { 110000, uint256("0001adb06ed43e55b0f960a212590674c8b10575de7afa7dc0bb0e53e971f21b") },
              { 130000, uint256("0001fbd054458ec9f75e94d6779def1ee6c6d009dbbe2f7759f5c6c75c4f9630") },
              { 150000, uint256("000249f070fe5b5fcb1923080c5dcbd78a6f31182ae32717df84e708b225370b") },
              { 170000, uint256("00029810ac925d321a415e2fb83d703dcb2ebb2d42b66584c3666eb5795d8ad6") },
              { 190000, uint256("0002e6304834d0f859658c939b77f9077073f42e91bf3f512bee644bd48180e1") },
              { 210000, uint256("000334508ed90eb9419392e1fce660467973d3dede5ca51f6e457517d03f2138") },
              { 230000, uint256("00038270812d3b2f338b5f8c9d00edfd084ae38580c6837b6278f20713ff20cc") },
              { 238000, uint256("0003a1b031248f0c0060fd8afd807f30ba34f81b6fcbbe84157e380d2d7119bc") },
              { 285060, uint256("00045984ae81f672b42525e0465dd05239c742fe0b6723a15c4fd03215362eae") } });

        vecBBCN.assign(
            { { 300000, uint256("000493e02a0f6ef977ebd5f844014badb412b6db85847b120f162b8d913b9028") },
              { 335999, uint256("0005207f9be2e4f1a278054058d4c17029ae6733cc8f7163a4e3099000deb9ff") } });

        InitCheckPoints(uint256("00000000b0a9be545f022309e148894d1e1c853ccac3ef04cb6f5e5c70f41a70"), vecGenesisCheckPoints);
        InitCheckPoints(uint256("000493e02a0f6ef977ebd5f844014badb412b6db85847b120f162b8d913b9028"), vecBBCN);
#endif
    }
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
