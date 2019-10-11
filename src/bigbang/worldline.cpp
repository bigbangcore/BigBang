// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "worldline.h"

#include "delegatecomm.h"
#include "delegateverify.h"

using namespace std;
using namespace xengine;

#define ENROLLED_CACHE_COUNT (120)
#define AGREEMENT_CACHE_COUNT (16)

namespace bigbang
{

//////////////////////////////
// CWorldLineModel

CWorldLineModel::CWorldLineModel()
  : cacheEnrolled(ENROLLED_CACHE_COUNT), cacheAgreement(AGREEMENT_CACHE_COUNT)
{
    pCoreProtocol = nullptr;
    pTxPoolCtrl = nullptr;
}

CWorldLineModel::~CWorldLineModel()
{
}

bool CWorldLineModel::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol\n");
        return false;
    }

    if (!GetObject("txpoolcontroller", pTxPoolCtrl))
    {
        Error("Failed to request txpool\n");
        return false;
    }

    return true;
}

void CWorldLineModel::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pTxPoolCtrl = nullptr;
}

bool CWorldLineModel::HandleInvoke()
{

    if (!blockBase.Initialize(Config() ? Config()->pathData : "./", Config() ? Config()->fDebug : true))
    {
        Error("Failed to initialize container\n");
        return false;
    }

    if (!CheckContainer())
    {
        blockBase.Clear();
        Log("Block container is invalid,try rebuild from block storage\n");
        // Rebuild ...
        if (!RebuildContainer())
        {
            blockBase.Clear();
            Error("Failed to rebuild Block container,reconstruct all\n");
        }
    }

    if (blockBase.IsEmpty())
    {
        CBlock block;
        pCoreProtocol->GetGenesisBlock(block);
        if (!InsertGenesisBlock(block))
        {
            Error("Failed to create genesis block\n");
            return false;
        }
    }

    return true;
}

void CWorldLineModel::HandleHalt()
{
    blockBase.Deinitialize();
    cacheEnrolled.Clear();
    cacheAgreement.Clear();
}

void CWorldLineModel::GetForkStatus(map<uint256, CForkStatus>& mapForkStatus)
{
    mapForkStatus.clear();

    multimap<int, CBlockIndex*> mapForkIndex;
    blockBase.ListForkIndex(mapForkIndex);
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

bool CWorldLineModel::GetForkProfile(const uint256& hashFork, CProfile& profile)
{
    return blockBase.RetrieveProfile(hashFork, profile);
}

bool CWorldLineModel::GetForkContext(const uint256& hashFork, CForkContext& ctxt)
{
    return blockBase.RetrieveForkContext(hashFork, ctxt);
}

bool CWorldLineModel::GetForkAncestry(const uint256& hashFork, vector<pair<uint256, uint256>> vAncestry)
{
    return blockBase.RetrieveAncestry(hashFork, vAncestry);
}

int CWorldLineModel::GetBlockCount(const uint256& hashFork)
{
    int nCount = 0;
    CBlockIndex* pIndex = nullptr;
    if (blockBase.RetrieveFork(hashFork, &pIndex))
    {
        while (pIndex != nullptr)
        {
            pIndex = pIndex->pPrev;
            ++nCount;
        }
    }
    return nCount;
}

bool CWorldLineModel::GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight)
{
    CBlockIndex* pIndex = nullptr;
    if (!blockBase.RetrieveIndex(hashBlock, &pIndex))
    {
        return false;
    }
    hashFork = pIndex->GetOriginHash();
    nHeight = pIndex->GetBlockHeight();
    return true;
}

bool CWorldLineModel::GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock)
{
    CBlockIndex* pIndex = nullptr;
    if (!blockBase.RetrieveFork(hashFork, &pIndex) || pIndex->GetBlockHeight() < nHeight)
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

bool CWorldLineModel::GetBlockHash(const uint256& hashFork, int nHeight, vector<uint256>& vBlockHash)
{
    CBlockIndex* pIndex = nullptr;
    if (!blockBase.RetrieveFork(hashFork, &pIndex) || pIndex->GetBlockHeight() < nHeight)
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

bool CWorldLineModel::GetLastBlock(const uint256& hashFork, uint256& hashBlock, int& nHeight, int64& nTime)
{
    CBlockIndex* pIndex = nullptr;
    if (!blockBase.RetrieveFork(hashFork, &pIndex))
    {
        return false;
    }
    hashBlock = pIndex->GetBlockHash();
    nHeight = pIndex->GetBlockHeight();
    nTime = pIndex->GetBlockTime();
    return true;
}

bool CWorldLineModel::GetLastBlockTime(const uint256& hashFork, int nDepth, vector<int64>& vTime)
{
    CBlockIndex* pIndex = nullptr;
    if (!blockBase.RetrieveFork(hashFork, &pIndex))
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

bool CWorldLineModel::GetBlock(const uint256& hashBlock, CBlock& block)
{
    return blockBase.Retrieve(hashBlock, block);
}

bool CWorldLineModel::GetBlockEx(const uint256& hashBlock, CBlockEx& block)
{
    return blockBase.Retrieve(hashBlock, block);
}

bool CWorldLineModel::GetOrigin(const uint256& hashFork, CBlock& block)
{
    return blockBase.RetrieveOrigin(hashFork, block);
}

bool CWorldLineModel::Exists(const uint256& hashBlock)
{
    return blockBase.Exists(hashBlock);
}

bool CWorldLineModel::GetTransaction(const uint256& txid, CTransaction& tx)
{
    return blockBase.RetrieveTx(txid, tx);
}

bool CWorldLineModel::ExistsTx(const uint256& txid)
{
    return blockBase.ExistsTx(txid);
}

bool CWorldLineModel::GetTxLocation(const uint256& txid, uint256& hashFork, int& nHeight)
{
    return blockBase.RetrieveTxLocation(txid, hashFork, nHeight);
}

bool CWorldLineModel::GetTxUnspent(const uint256& hashFork, const vector<CTxIn>& vInput, vector<CTxOut>& vOutput)
{
    vOutput.resize(vInput.size());
    storage::CBlockView view;
    if (!blockBase.GetForkBlockView(hashFork, view))
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

bool CWorldLineModel::FilterTx(const uint256& hashFork, CTxFilter& filter)
{
    return blockBase.FilterTx(hashFork, filter);
}

bool CWorldLineModel::FilterTx(const uint256& hashFork, int nDepth, CTxFilter& filter)
{
    return blockBase.FilterTx(hashFork, nDepth, filter);
}

bool CWorldLineModel::ListForkContext(vector<CForkContext>& vForkCtxt)
{
    return blockBase.ListForkContext(vForkCtxt);
}

Errno CWorldLineModel::AddNewForkContext(const CTransaction& txFork, CForkContext& ctxt)
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
        Error("Invalid orign block found in tx (%s)\n", txid.GetHex().c_str());
        return ERR_BLOCK_INVALID_FORK;
    }
    uint256 hashFork = block.GetHash();

    CForkContext ctxtParent;
    if (!blockBase.RetrieveForkContext(profile.hashParent, ctxtParent))
    {
        Log("AddNewForkContext Retrieve parent context Error: %s \n", profile.hashParent.ToString().c_str());
        return ERR_MISSING_PREV;
    }

    CProfile forkProfile;
    Errno err = pCoreProtocol->ValidateOrigin(block, ctxtParent.GetProfile(), forkProfile);
    if (err != OK)
    {
        Log("AddNewForkContext Validate Block Error(%s) : %s \n", ErrorString(err), hashFork.ToString().c_str());
        return err;
    }

    ctxt = CForkContext(block.GetHash(), block.hashPrev, txid, profile);
    if (!blockBase.AddNewForkContext(ctxt))
    {
        Log("AddNewForkContext Already Exists : %s \n", hashFork.ToString().c_str());
        return ERR_ALREADY_HAVE;
    }

    return OK;
}

Errno CWorldLineModel::AddNewBlock(const CBlock& block, CWorldLineUpdate& update)
{
    uint256 hash = block.GetHash();
    Errno err = OK;

    if (blockBase.Exists(hash))
    {
        Log("AddNewBlock Already Exists : %s \n", hash.ToString().c_str());
        return ERR_ALREADY_HAVE;
    }

    err = pCoreProtocol->ValidateBlock(block);
    if (err != OK)
    {
        Log("AddNewBlock Validate Block Error(%s) : %s \n", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    CBlockIndex* pIndexPrev;
    if (!blockBase.RetrieveIndex(block.hashPrev, &pIndexPrev))
    {
        Log("AddNewBlock Retrieve Prev Index Error: %s \n", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    int64 nReward;

    err = VerifyBlock(hash, block, pIndexPrev, nReward);
    if (err != OK)
    {
        Log("AddNewBlock Verify Block Error(%s) : %s \n", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    storage::CBlockView view;
    if (!blockBase.GetBlockView(block.hashPrev, view, !block.IsOrigin()))
    {
        Log("AddNewBlock Get Block View Error: %s \n", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    view.AddTx(block.txMint);

    CBlockEx blockex(block);
    vector<CTxContxt>& vTxContxt = blockex.vTxContxt;

    int64 nTotalFee = 0;

    vTxContxt.reserve(block.vtx.size());

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
        if (tx.nType == CTransaction::TX_CERT && pCoreProtocol->CheckFirstPow(nForkHeight))
        {
            Log("AddNewBlock Verify tx Error(first pow) : %s \n", txid.ToString().c_str());
            return ERR_TRANSACTION_INVALID;
        }
        CTxContxt txContxt;
        err = GetTxContxt(view, tx, txContxt);
        if (err != OK)
        {
            Log("AddNewBlock Get txContxt Error(%s) : %s \n", ErrorString(err), txid.ToString().c_str());
            return err;
        }
        if (!pTxPoolCtrl->Exists(txid))
        {
            err = pCoreProtocol->VerifyBlockTx(tx, txContxt, pIndexPrev, nForkHeight, pIndexPrev->GetOriginHash());
            if (err != OK)
            {
                Log("AddNewBlock Verify BlockTx Error(%s) : %s \n", ErrorString(err), txid.ToString().c_str());
                return err;
            }
        }
        vTxContxt.push_back(txContxt);
        view.AddTx(txid, tx, txContxt.destIn, txContxt.GetValueIn());

        nTotalFee += tx.nTxFee;
    }

    if (block.txMint.nAmount > nTotalFee + nReward)
    {
        Log("AddNewBlock Mint tx amount invalid : (%ld > %ld + %ld \n", block.txMint.nAmount, nTotalFee, nReward);
        return ERR_BLOCK_TRANSACTIONS_INVALID;
    }

    CBlockIndex* pIndexNew;
    if (!blockBase.AddNew(hash, blockex, &pIndexNew))
    {
        Log("AddNewBlock Storage AddNew Error : %s \n", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    Log("AddNew Block : %s\n", pIndexNew->ToString().c_str());
    if (Config()->fDebug)
    {
        Debug("New Block %s tx : %s\n", hash.ToString().c_str(), view.ToString().c_str());
    }

    CBlockIndex* pIndexFork = nullptr;
    if (blockBase.RetrieveFork(pIndexNew->GetOriginHash(), &pIndexFork)
        && (pIndexFork->nChainTrust > pIndexNew->nChainTrust
            || (pIndexFork->nChainTrust == pIndexNew->nChainTrust && !pIndexNew->IsEquivalent(pIndexFork))))
    {
        return OK;
    }

    if (!blockBase.CommitBlockView(view, pIndexNew))
    {
        Log("AddNewBlock Storage Commit BlockView Error : %s \n", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    update = CWorldLineUpdate(pIndexNew);
    view.GetTxUpdated(update.setTxUpdate);
    if (!GetBlockChanges(pIndexNew, pIndexFork, update.vBlockAddNew, update.vBlockRemove))
    {
        Log("AddNewBlock Storage GetBlockChanges Error : %s \n", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    return OK;
}

Errno CWorldLineModel::AddNewOrigin(const CBlock& block, CWorldLineUpdate& update)
{
    uint256 hash = block.GetHash();
    Errno err = OK;

    if (blockBase.Exists(hash))
    {
        Log("AddNewOrigin Already Exists : %s \n", hash.ToString().c_str());
        return ERR_ALREADY_HAVE;
    }

    err = pCoreProtocol->ValidateBlock(block);
    if (err != OK)
    {
        Log("AddNewOrigin Validate Block Error(%s) : %s \n", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    CBlockIndex* pIndexPrev;
    if (!blockBase.RetrieveIndex(block.hashPrev, &pIndexPrev))
    {
        Log("AddNewOrigin Retrieve Prev Index Error: %s \n", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    CProfile parent;
    if (!blockBase.RetrieveProfile(pIndexPrev->GetOriginHash(), parent))
    {
        Log("AddNewOrigin Retrieve parent profile Error: %s \n", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }
    CProfile profile;
    err = pCoreProtocol->ValidateOrigin(block, parent, profile);
    if (err != OK)
    {
        Log("AddNewOrigin Validate Origin Error(%s): %s \n", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    CBlockIndex* pIndexDuplicated;
    if (blockBase.RetrieveFork(profile.strName, &pIndexDuplicated))
    {
        Log("AddNewOrigin Validate Origin Error(duplated fork name): %s, \nexisted: %s\n",
            hash.ToString().c_str(), pIndexDuplicated->GetOriginHash().GetHex().c_str());
        return ERR_ALREADY_HAVE;
    }

    storage::CBlockView view;

    if (profile.IsIsolated())
    {
        if (!blockBase.GetBlockView(view))
        {
            Log("AddNewOrigin Get Block View Error: %s \n", block.hashPrev.ToString().c_str());
            return ERR_SYS_STORAGE_ERROR;
        }
    }
    else
    {
        if (!blockBase.GetBlockView(block.hashPrev, view, false))
        {
            Log("AddNewOrigin Get Block View Error: %s \n", block.hashPrev.ToString().c_str());
            return ERR_SYS_STORAGE_ERROR;
        }
    }

    if (block.txMint.nAmount != 0)
    {
        view.AddTx(block.txMint.GetHash(), block.txMint);
    }

    CBlockIndex* pIndexNew;
    CBlockEx blockex(block);

    if (!blockBase.AddNew(hash, blockex, &pIndexNew))
    {
        Log("AddNewOrigin Storage AddNew Error : %s \n", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    Log("AddNew Origin Block : %s \n", hash.ToString().c_str());
    Log("    %s\n", pIndexNew->ToString().c_str());

    if (!blockBase.CommitBlockView(view, pIndexNew))
    {
        Log("AddNewOrigin Storage Commit BlockView Error : %s \n", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    update = CWorldLineUpdate(pIndexNew);
    view.GetTxUpdated(update.setTxUpdate);
    update.vBlockAddNew.push_back(blockex);

    return OK;
}

bool CWorldLineModel::GetProofOfWorkTarget(const uint256& hashPrev, int nAlgo, int& nBits, int64& nReward)
{
    CBlockIndex* pIndexPrev;
    if (!blockBase.RetrieveIndex(hashPrev, &pIndexPrev))
    {
        Log("GetProofOfWorkTarget : Retrieve Prev Index Error: %s \n", hashPrev.ToString().c_str());
        return false;
    }
    if (!pIndexPrev->IsPrimary())
    {
        Log("GetProofOfWorkTarget : Previous is not primary: %s \n", hashPrev.ToString().c_str());
        return false;
    }
    if (!pCoreProtocol->GetProofOfWorkTarget(pIndexPrev, nAlgo, nBits, nReward))
    {
        Log("GetProofOfWorkTarget : Unknown proof-of-work algo: %s \n", hashPrev.ToString().c_str());
        return false;
    }
    return true;
}

bool CWorldLineModel::GetBlockMintReward(const uint256& hashPrev, int64& nReward)
{
    CBlockIndex* pIndexPrev;
    if (!blockBase.RetrieveIndex(hashPrev, &pIndexPrev))
    {
        Log("Get block reward: Retrieve Prev Index Error, hashPrev: %s\n", hashPrev.ToString().c_str());
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
            Log("Get block reward: Get fork profile fail, hashPrev: %s\n", hashPrev.ToString().c_str());
            return false;
        }
        if (profile.nHalveCycle == 0)
        {
            nReward = profile.nMintReward;
        }
        else
        {
            nReward = profile.nMintReward / pow(2, (pIndexPrev->GetBlockHeight() + 1) / profile.nHalveCycle);
        }
    }
    return true;
}

bool CWorldLineModel::GetBlockLocator(const uint256& hashFork, CBlockLocator& locator)
{
    return blockBase.GetForkBlockLocator(hashFork, locator);
}

bool CWorldLineModel::GetBlockInv(const uint256& hashFork, const CBlockLocator& locator, vector<uint256>& vBlockHash, size_t nMaxCount)
{
    return blockBase.GetForkBlockInv(hashFork, locator, vBlockHash, nMaxCount);
}

bool CWorldLineModel::GetBlockDelegateEnrolled(const uint256& hashBlock, CDelegateEnrolled& enrolled)
{
    enrolled.Clear();

    if (cacheEnrolled.Retrieve(hashBlock, enrolled))
    {
        return true;
    }

    CBlockIndex* pIndex;
    if (!blockBase.RetrieveIndex(hashBlock, &pIndex))
    {
        Log("GetBlockDelegateEnrolled : Retrieve block Index Error: %s \n", hashBlock.ToString().c_str());
        return false;
    }
    int64 nDelegateWeightRatio = (pIndex->GetMoneySupply() + DELEGATE_THRESH - 1) / DELEGATE_THRESH;

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

    if (!blockBase.RetrieveAvailDelegate(hashBlock, pIndex->GetBlockHash(), vBlockRange, nDelegateWeightRatio,
                                         enrolled.mapWeight, enrolled.mapEnrollData))
    {
        Log("GetBlockDelegateEnrolled : Retrieve Avail Delegate Error: %s \n", hashBlock.ToString().c_str());
        return false;
    }

    cacheEnrolled.AddNew(hashBlock, enrolled);

    return true;
}

bool CWorldLineModel::GetBlockDelegateAgreement(const uint256& hashBlock, CDelegateAgreement& agreement)
{
    agreement.Clear();

    if (cacheAgreement.Retrieve(hashBlock, agreement))
    {
        return true;
    }

    CBlockIndex* pIndex = nullptr;
    if (!blockBase.RetrieveIndex(hashBlock, &pIndex))
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
    if (!blockBase.Retrieve(pIndex, block))
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
        return false;
    }

    delegate::CDelegateVerify verifier(enrolled.mapWeight, enrolled.mapEnrollData);
    map<CDestination, size_t> mapBallot;
    if (!verifier.VerifyProof(block.vchProof, agreement.nAgreement, agreement.nWeight, mapBallot))
    {
        Log("GetBlockDelegateAgreement : Invalid block proof : %s \n", hashBlock.ToString().c_str());
        return false;
    }

    pCoreProtocol->GetDelegatedBallot(agreement.nAgreement, agreement.nWeight, mapBallot, agreement.vBallot, pIndexRef->GetBlockHeight());

    cacheAgreement.AddNew(hashBlock, agreement);

    return true;
}

bool CWorldLineModel::CheckContainer()
{
    if (blockBase.IsEmpty())
    {
        return true;
    }
    if (!blockBase.Exists(pCoreProtocol->GetGenesisBlockHash()))
    {
        return false;
    }
    return blockBase.CheckConsistency(StorageConfig() ? StorageConfig()->nCheckLevel : 1,
                                      StorageConfig() ? StorageConfig()->nCheckDepth : 1);
}

bool CWorldLineModel::RebuildContainer()
{
    return false;
}

bool CWorldLineModel::InsertGenesisBlock(CBlock& block)
{
    return blockBase.Initiate(block.GetHash(), block);
}

Errno CWorldLineModel::GetTxContxt(storage::CBlockView& view, const CTransaction& tx, CTxContxt& txContxt)
{
    txContxt.SetNull();
    for (const CTxIn& txin : tx.vInput)
    {
        CTxOut output;
        if (!view.RetrieveUnspent(txin.prevout, output))
        {
            return ERR_MISSING_PREV;
        }
        if (txContxt.destIn.IsNull())
        {
            txContxt.destIn = output.destTo;
        }
        else if (txContxt.destIn != output.destTo)
        {
            return ERR_TRANSACTION_INVALID;
        }
        txContxt.vin.push_back(CTxInContxt(output));
    }
    return OK;
}

bool CWorldLineModel::GetBlockChanges(const CBlockIndex* pIndexNew, const CBlockIndex* pIndexFork,
                                 vector<CBlockEx>& vBlockAddNew, vector<CBlockEx>& vBlockRemove)
{
    while (pIndexNew != pIndexFork)
    {
        int64 nLastBlockTime = pIndexFork ? pIndexFork->GetBlockTime() : -1;
        if (pIndexNew->GetBlockTime() >= nLastBlockTime)
        {
            CBlockEx block;
            if (!blockBase.Retrieve(pIndexNew, block))
            {
                return false;
            }
            vBlockAddNew.push_back(block);
            pIndexNew = pIndexNew->pPrev;
        }
        else
        {
            CBlockEx block;
            if (!blockBase.Retrieve(pIndexFork, block))
            {
                return false;
            }
            vBlockRemove.push_back(block);
            pIndexFork = pIndexFork->pPrev;
        }
    }
    return true;
}

bool CWorldLineModel::GetBlockDelegateAgreement(const uint256& hashBlock, const CBlock& block, const CBlockIndex* pIndexPrev,
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

    pCoreProtocol->GetDelegatedBallot(agreement.nAgreement, agreement.nWeight, mapBallot, agreement.vBallot, pIndexPrev->GetBlockHeight() + 1);

    cacheAgreement.AddNew(hashBlock, agreement);

    return true;
}

Errno CWorldLineModel::VerifyBlock(const uint256& hashBlock, const CBlock& block, CBlockIndex* pIndexPrev, int64& nReward)
{
    nReward = 0;
    if (block.IsOrigin())
    {
        return ERR_BLOCK_INVALID_FORK;
    }

    if (block.IsPrimary())
    {
        if (!pIndexPrev->IsPrimary())
        {
            return ERR_BLOCK_INVALID_FORK;
        }

        CDelegateAgreement agreement;
        if (!GetBlockDelegateAgreement(hashBlock, block, pIndexPrev, agreement))
        {
            return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
        }

        if (!GetBlockMintReward(block.hashPrev, nReward))
        {
            return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
        }

        if (agreement.IsProofOfWork())
        {
            return pCoreProtocol->VerifyProofOfWork(block, pIndexPrev);
        }
        else
        {
            return pCoreProtocol->VerifyDelegatedProofOfStake(block, pIndexPrev, agreement);
        }
    }
    else if (!block.IsVacant())
    {
        if (pIndexPrev->IsPrimary())
        {
            return ERR_BLOCK_INVALID_FORK;
        }

        CProofOfPiggyback proof;
        proof.Load(block.vchProof);

        CDelegateAgreement agreement;
        if (!GetBlockDelegateAgreement(proof.hashRefBlock, agreement))
        {
            return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
        }

        if (agreement.nAgreement != proof.nAgreement || agreement.nWeight != proof.nWeight
            || agreement.IsProofOfWork())
        {
            return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
        }

        CBlockIndex* pIndexRef = nullptr;
        if (!blockBase.RetrieveIndex(proof.hashRefBlock, &pIndexRef))
        {
            return ERR_BLOCK_PROOF_OF_STAKE_INVALID;
        }

        if (block.IsExtended())
        {
            CBlock blockPrev;
            if (!blockBase.Retrieve(pIndexPrev, blockPrev) || blockPrev.IsVacant())
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

        return pCoreProtocol->VerifySubsidiary(block, pIndexPrev, pIndexRef, agreement);
    }

    return OK;
}

//////////////////////////////
// CWorldLineController

CWorldLineController::CWorldLineController()
  : pForkManager(nullptr)
{
}

CWorldLineController::~CWorldLineController()
{
}

bool CWorldLineController::HandleInitialize()
{
    if (!GetObject("worldline", pWorldLine))
    {
        Error("Failed to request worldline\n");
        return false;
    }
    if (!GetObject("forkmanager", pForkManager))
    {
        Error("Failed to request forkmanager\n");
        return false;
    }

    RegisterRefHandler<CAddBlockMessage>(boost::bind(&CWorldLineController::HandleAddBlock, this, _1));

    return true;
}

void CWorldLineController::HandleDeinitialize()
{
    DeregisterHandler(CAddedBlockMessage::MessageType());

    pWorldLine = nullptr;
    pForkManager = nullptr;
}

bool CWorldLineController::HandleInvoke()
{
    return StartActor();
}

void CWorldLineController::HandleHalt()
{
    StopActor();
}

Errno CWorldLineController::AddNewForkContext(const CTransaction& txFork, CForkContext& ctxt)
{
    return AddNewForkContextIntoWorldLine(txFork, ctxt);
}

Errno CWorldLineController::AddNewBlock(const CBlock& block, CWorldLineUpdate& update)
{
    return AddNewBlockIntoWorldLine(block, update);
}

Errno CWorldLineController::AddNewOrigin(const CBlock& block, CWorldLineUpdate& update)
{
    return AddNewOriginIntoWorldLine(block, update);
}

void CWorldLineController::GetForkStatus(std::map<uint256, CForkStatus>& mapForkStatus)
{
    pWorldLine->GetForkStatus(mapForkStatus);
}

bool CWorldLineController::GetForkProfile(const uint256& hashFork, CProfile& profile)
{
    return pWorldLine->GetForkProfile(hashFork, profile);
}

bool CWorldLineController::GetForkContext(const uint256& hashFork, CForkContext& ctxt)
{
    return pWorldLine->GetForkContext(hashFork, ctxt);
}

bool CWorldLineController::GetForkAncestry(const uint256& hashFork, std::vector<std::pair<uint256, uint256>> vAncestry)
{
    return pWorldLine->GetForkAncestry(hashFork, vAncestry);
}

int CWorldLineController::GetBlockCount(const uint256& hashFork)
{
    return pWorldLine->GetBlockCount(hashFork);
}

bool CWorldLineController::GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight)
{
    return pWorldLine->GetBlockLocation(hashBlock, hashFork, nHeight);
}

bool CWorldLineController::GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock)
{
    return pWorldLine->GetBlockHash(hashFork, nHeight, hashBlock);
}

bool CWorldLineController::GetBlockHash(const uint256& hashFork, int nHeight, std::vector<uint256>& vBlockHash)
{
    return pWorldLine->GetBlockHash(hashFork, nHeight, vBlockHash);
}

bool CWorldLineController::GetLastBlock(const uint256& hashFork, uint256& hashBlock, int& nHeight, int64& nTime)
{
    return pWorldLine->GetLastBlock(hashFork, hashBlock, nHeight, nTime);
}

bool CWorldLineController::GetLastBlockTime(const uint256& hashFork, int nDepth, std::vector<int64>& vTime)
{
    return pWorldLine->GetLastBlockTime(hashFork, nDepth, vTime);
}

bool CWorldLineController::GetBlock(const uint256& hashBlock, CBlock& block)
{
    return pWorldLine->GetBlock(hashBlock, block);
}

bool CWorldLineController::GetBlockEx(const uint256& hashBlock, CBlockEx& block)
{
    return pWorldLine->GetBlockEx(hashBlock, block);
}

bool CWorldLineController::GetOrigin(const uint256& hashFork, CBlock& block)
{
    return pWorldLine->GetOrigin(hashFork, block);
}

bool CWorldLineController::Exists(const uint256& hashBlock)
{
    return pWorldLine->Exists(hashBlock);
}

bool CWorldLineController::GetTransaction(const uint256& txid, CTransaction& tx)
{
    return pWorldLine->GetTransaction(txid, tx);
}

bool CWorldLineController::ExistsTx(const uint256& txid)
{
    return pWorldLine->ExistsTx(txid);
}

bool CWorldLineController::GetTxLocation(const uint256& txid, uint256& hashFork, int& nHeight)
{
    return pWorldLine->GetTxLocation(txid, hashFork, nHeight);
}

bool CWorldLineController::GetTxUnspent(const uint256& hashFork, const std::vector<CTxIn>& vInput,
                                        std::vector<CTxOut>& vOutput)
{
    return pWorldLine->GetTxUnspent(hashFork, vInput, vOutput);
}

bool CWorldLineController::FilterTx(const uint256& hashFork, CTxFilter& filter)
{
    return pWorldLine->FilterTx(hashFork, filter);
}

bool CWorldLineController::FilterTx(const uint256& hashFork, int nDepth, CTxFilter& filter)
{
    return pWorldLine->FilterTx(hashFork, nDepth, filter);
}

bool CWorldLineController::ListForkContext(std::vector<CForkContext>& vForkCtxt)
{
    return pWorldLine->ListForkContext(vForkCtxt);
}

bool CWorldLineController::GetProofOfWorkTarget(const uint256& hashPrev, int nAlgo, int& nBits, int64& nReward)
{
    return pWorldLine->GetProofOfWorkTarget(hashPrev, nAlgo, nBits, nReward);
}

bool CWorldLineController::GetBlockMintReward(const uint256& hashPrev, int64& nReward)
{
    return pWorldLine->GetBlockMintReward(hashPrev, nReward);
}

bool CWorldLineController::GetBlockLocator(const uint256& hashFork, CBlockLocator& locator)
{
    return pWorldLine->GetBlockLocator(hashFork, locator);
}

bool CWorldLineController::GetBlockInv(const uint256& hashFork, const CBlockLocator& locator, std::vector<uint256>& vBlockHash, std::size_t nMaxCount)
{
    return pWorldLine->GetBlockInv(hashFork, locator, vBlockHash, nMaxCount);
}

bool CWorldLineController::GetBlockDelegateEnrolled(const uint256& hashBlock, CDelegateEnrolled& enrolled)
{
    return pWorldLine->GetBlockDelegateEnrolled(hashBlock, enrolled);
}

bool CWorldLineController::GetBlockDelegateAgreement(const uint256& hashBlock, CDelegateAgreement& agreement)
{
    return pWorldLine->GetBlockDelegateAgreement(hashBlock, agreement);
}

void CWorldLineController::HandleAddBlock(const CAddBlockMessage& msg)
{
    // Add new block
    auto spAddedBlockMsg = CAddedBlockMessage::Create();
    spAddedBlockMsg->nNonce = msg.nNonce;
    spAddedBlockMsg->hashFork = msg.hashFork;
    spAddedBlockMsg->block = msg.block;

    const CBlock& block = msg.block;
    if (!block.IsOrigin())
    {
        spAddedBlockMsg->nError = AddNewBlockIntoWorldLine(block, spAddedBlockMsg->update);
    }
    else
    {
        spAddedBlockMsg->nError = AddNewOriginIntoWorldLine(block, spAddedBlockMsg->update);
    }
    const CWorldLineUpdate& update = spAddedBlockMsg->update;
    PUBLISH_MESSAGE(spAddedBlockMsg);

    // Create new fork
    vector<CTransaction> vForkTx;
    vector<uint256> vActive, vDeactive;
    pForkManager->ForkUpdate(update, vForkTx, vActive, vDeactive);

    for (auto& tx : vForkTx)
    {
        CForkContext ctxt;
        if (AddNewForkContextIntoWorldLine(tx, ctxt) == OK)
        {
            pForkManager->AddNewForkContext(ctxt, vActive);
            // Add new origin block
            auto spAddedOriginMsg = CAddedBlockMessage::Create();
            CBlock& originBlock = spAddedOriginMsg->block;
            try
            {
                CBufStream ss;
                ss.Write((const char*)&tx.vchData[0], tx.vchData.size());
                ss >> originBlock;
            }
            catch (...)
            {
                originBlock.SetNull();
            }

            if (originBlock.IsOrigin() && !originBlock.IsPrimary())
            {
                Errno err = AddNewOriginIntoWorldLine(originBlock, spAddedOriginMsg->update);
                if (err == OK)
                {
                    spAddedOriginMsg->nError = OK;
                    spAddedOriginMsg->nNonce = 0;
                    spAddedOriginMsg->hashFork = originBlock.GetHash();
                    PUBLISH_MESSAGE(spAddedOriginMsg);
                }
                else
                {
                    Warn("Add origin block in tx (%s) failed : %s\n", tx.GetHash().GetHex().c_str(), ErrorString(err));
                }
            }
            else
            {
                Warn("Invalid origin block found in tx (%s)\n", tx.GetHash().GetHex().c_str());
            }
        }
    }

    for (const uint256 hashFork : vActive)
    {
        auto spSubscribeMsg = CSubscribeForkMessage::Create(hashFork, 0);
        PUBLISH_MESSAGE(spSubscribeMsg);
    }

    for (const uint256 hashFork : vDeactive)
    {
        auto spUnsubscribeMsg = CUnsubscribeForkMessage::Create(hashFork);
        PUBLISH_MESSAGE(spUnsubscribeMsg);
    }
}

} // namespace bigbang
