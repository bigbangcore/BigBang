// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "worldline.h"

#include "delegatecomm.h"
#include "delegateverify.h"
#include "netchn.h"

using namespace std;
using namespace xengine;

#define ENROLLED_CACHE_COUNT (120)
#define AGREEMENT_CACHE_COUNT (16)

namespace bigbang
{

//////////////////////////////
// CWorldLine

CWorldLine::CWorldLine()
  : cacheEnrolled(ENROLLED_CACHE_COUNT), cacheAgreement(AGREEMENT_CACHE_COUNT)
{
    pCoreProtocol = nullptr;
    pTxPoolCtrl = nullptr;
}

CWorldLine::~CWorldLine()
{
}

bool CWorldLine::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        ERROR("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("txpoolcontroller", pTxPoolCtrl))
    {
        ERROR("Failed to request txpool");
        return false;
    }

    return true;
}

void CWorldLine::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pTxPoolCtrl = nullptr;
}

bool CWorldLine::HandleInvoke()
{

    if (!blockBase.Initialize(Config() ? Config()->pathData : "./", Config() ? Config()->fDebug : true))
    {
        ERROR("Failed to initialize container");
        return false;
    }

    if (!CheckContainer())
    {
        blockBase.Clear();
        INFO("Block container is invalid,try rebuild from block storage");
        // Rebuild ...
        if (!RebuildContainer())
        {
            blockBase.Clear();
            ERROR("Failed to rebuild Block container,reconstruct all");
        }
    }

    if (blockBase.IsEmpty())
    {
        CBlock block;
        pCoreProtocol->GetGenesisBlock(block);
        if (!InsertGenesisBlock(block))
        {
            ERROR("Failed to create genesis block");
            return false;
        }
    }

    return true;
}

void CWorldLine::HandleHalt()
{
    blockBase.Deinitialize();
    cacheEnrolled.Clear();
    cacheAgreement.Clear();
}

void CWorldLine::GetForkStatus(map<uint256, CForkStatus>& mapForkStatus)
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

bool CWorldLine::GetForkProfile(const uint256& hashFork, CProfile& profile)
{
    return blockBase.RetrieveProfile(hashFork, profile);
}

bool CWorldLine::GetForkContext(const uint256& hashFork, CForkContext& ctxt)
{
    return blockBase.RetrieveForkContext(hashFork, ctxt);
}

bool CWorldLine::GetForkAncestry(const uint256& hashFork, vector<pair<uint256, uint256>> vAncestry)
{
    return blockBase.RetrieveAncestry(hashFork, vAncestry);
}

int CWorldLine::GetBlockCount(const uint256& hashFork)
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

bool CWorldLine::GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight)
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

bool CWorldLine::GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock)
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

bool CWorldLine::GetBlockHash(const uint256& hashFork, int nHeight, vector<uint256>& vBlockHash)
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

bool CWorldLine::GetLastBlock(const uint256& hashFork, uint256& hashBlock, int& nHeight, int64& nTime)
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

bool CWorldLine::GetLastBlockTime(const uint256& hashFork, int nDepth, vector<int64>& vTime)
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

bool CWorldLine::GetBlock(const uint256& hashBlock, CBlock& block)
{
    return blockBase.Retrieve(hashBlock, block);
}

bool CWorldLine::GetBlockEx(const uint256& hashBlock, CBlockEx& block)
{
    return blockBase.Retrieve(hashBlock, block);
}

bool CWorldLine::GetOrigin(const uint256& hashFork, CBlock& block)
{
    return blockBase.RetrieveOrigin(hashFork, block);
}

bool CWorldLine::Exists(const uint256& hashBlock)
{
    return blockBase.Exists(hashBlock);
}

bool CWorldLine::GetTransaction(const uint256& txid, CTransaction& tx)
{
    return blockBase.RetrieveTx(txid, tx);
}

bool CWorldLine::ExistsTx(const uint256& txid)
{
    return blockBase.ExistsTx(txid);
}

bool CWorldLine::GetTxLocation(const uint256& txid, uint256& hashFork, int& nHeight)
{
    return blockBase.RetrieveTxLocation(txid, hashFork, nHeight);
}

bool CWorldLine::GetTxUnspent(const uint256& hashFork, const vector<CTxIn>& vInput, vector<CTxOut>& vOutput)
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

bool CWorldLine::FilterTx(const uint256& hashFork, CTxFilter& filter)
{
    return blockBase.FilterTx(hashFork, filter);
}

bool CWorldLine::FilterTx(const uint256& hashFork, int nDepth, CTxFilter& filter)
{
    return blockBase.FilterTx(hashFork, nDepth, filter);
}

bool CWorldLine::ListForkContext(vector<CForkContext>& vForkCtxt)
{
    return blockBase.ListForkContext(vForkCtxt);
}

Errno CWorldLine::AddNewForkContext(const CTransaction& txFork, CForkContext& ctxt)
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
        ERROR("Invalid orign block found in tx (%s)", txid.GetHex().c_str());
        return ERR_BLOCK_INVALID_FORK;
    }
    uint256 hashFork = block.GetHash();

    CForkContext ctxtParent;
    if (!blockBase.RetrieveForkContext(profile.hashParent, ctxtParent))
    {
        INFO("AddNewForkContext Retrieve parent context Error: %s ", profile.hashParent.ToString().c_str());
        return ERR_MISSING_PREV;
    }

    CProfile forkProfile;
    Errno err = pCoreProtocol->ValidateOrigin(block, ctxtParent.GetProfile(), forkProfile);
    if (err != OK)
    {
        INFO("AddNewForkContext Validate Block Error(%s) : %s ", ErrorString(err), hashFork.ToString().c_str());
        return err;
    }

    ctxt = CForkContext(block.GetHash(), block.hashPrev, txid, profile);
    if (!blockBase.AddNewForkContext(ctxt))
    {
        INFO("AddNewForkContext Already Exists : %s ", hashFork.ToString().c_str());
        return ERR_ALREADY_HAVE;
    }

    return OK;
}

Errno CWorldLine::AddNewBlock(const CBlock& block, CWorldLineUpdate& update)
{
    uint256 hash = block.GetHash();
    Errno err = OK;

    if (blockBase.Exists(hash))
    {
        INFO("AddNewBlock Already Exists : %s ", hash.ToString().c_str());
        return ERR_ALREADY_HAVE;
    }

    err = pCoreProtocol->ValidateBlock(block);
    if (err != OK)
    {
        INFO("AddNewBlock Validate Block Error(%s) : %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    CBlockIndex* pIndexPrev;
    if (!blockBase.RetrieveIndex(block.hashPrev, &pIndexPrev))
    {
        INFO("AddNewBlock Retrieve Prev Index Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    int64 nReward;

    err = VerifyBlock(hash, block, pIndexPrev, nReward);
    if (err != OK)
    {
        INFO("AddNewBlock Verify Block Error(%s) : %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    storage::CBlockView view;
    if (!blockBase.GetBlockView(block.hashPrev, view, !block.IsOrigin()))
    {
        INFO("AddNewBlock Get Block View Error: %s ", block.hashPrev.ToString().c_str());
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
            INFO("AddNewBlock Verify tx Error(first pow) : %s ", txid.ToString().c_str());
            return ERR_TRANSACTION_INVALID;
        }
        CTxContxt txContxt;
        err = GetTxContxt(view, tx, txContxt);
        if (err != OK)
        {
            INFO("AddNewBlock Get txContxt Error(%s) : %s ", ErrorString(err), txid.ToString().c_str());
            return err;
        }
        if (!pTxPoolCtrl->Exists(txid))
        {
            err = pCoreProtocol->VerifyBlockTx(tx, txContxt, pIndexPrev, nForkHeight, pIndexPrev->GetOriginHash());
            if (err != OK)
            {
                INFO("AddNewBlock Verify BlockTx Error(%s) : %s ", ErrorString(err), txid.ToString().c_str());
                return err;
            }
        }
        vTxContxt.push_back(txContxt);
        view.AddTx(txid, tx, txContxt.destIn, txContxt.GetValueIn());

        nTotalFee += tx.nTxFee;
    }

    if (block.txMint.nAmount > nTotalFee + nReward)
    {
        INFO("AddNewBlock Mint tx amount invalid : (%ld > %ld + %ld ", block.txMint.nAmount, nTotalFee, nReward);
        return ERR_BLOCK_TRANSACTIONS_INVALID;
    }

    CBlockIndex* pIndexNew;
    if (!blockBase.AddNew(hash, blockex, &pIndexNew))
    {
        INFO("AddNewBlock Storage AddNew Error : %s ", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    INFO("AddNew Block : %s", pIndexNew->ToString().c_str());
    if (Config()->fDebug)
    {
        DEBUG("New Block %s tx : %s", hash.ToString().c_str(), view.ToString().c_str());
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
        INFO("AddNewBlock Storage Commit BlockView Error : %s ", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    update = CWorldLineUpdate(pIndexNew);
    view.GetTxUpdated(update.setTxUpdate);
    if (!GetBlockChanges(pIndexNew, pIndexFork, update.vBlockAddNew, update.vBlockRemove))
    {
        INFO("AddNewBlock Storage GetBlockChanges Error : %s ", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    return OK;
}

Errno CWorldLine::AddNewOrigin(const CBlock& block, CWorldLineUpdate& update)
{
    uint256 hash = block.GetHash();
    Errno err = OK;

    if (blockBase.Exists(hash))
    {
        INFO("AddNewOrigin Already Exists : %s ", hash.ToString().c_str());
        return ERR_ALREADY_HAVE;
    }

    err = pCoreProtocol->ValidateBlock(block);
    if (err != OK)
    {
        INFO("AddNewOrigin Validate Block Error(%s) : %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    CBlockIndex* pIndexPrev;
    if (!blockBase.RetrieveIndex(block.hashPrev, &pIndexPrev))
    {
        INFO("AddNewOrigin Retrieve Prev Index Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    CProfile parent;
    if (!blockBase.RetrieveProfile(pIndexPrev->GetOriginHash(), parent))
    {
        INFO("AddNewOrigin Retrieve parent profile Error: %s ", block.hashPrev.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }
    CProfile profile;
    err = pCoreProtocol->ValidateOrigin(block, parent, profile);
    if (err != OK)
    {
        INFO("AddNewOrigin Validate Origin Error(%s): %s ", ErrorString(err), hash.ToString().c_str());
        return err;
    }

    CBlockIndex* pIndexDuplicated;
    if (blockBase.RetrieveFork(profile.strName, &pIndexDuplicated))
    {
        INFO("AddNewOrigin Validate Origin Error(duplated fork name): %s, \nexisted: %s",
             hash.ToString().c_str(), pIndexDuplicated->GetOriginHash().GetHex().c_str());
        return ERR_ALREADY_HAVE;
    }

    storage::CBlockView view;

    if (profile.IsIsolated())
    {
        if (!blockBase.GetBlockView(view))
        {
            INFO("AddNewOrigin Get Block View Error: %s ", block.hashPrev.ToString().c_str());
            return ERR_SYS_STORAGE_ERROR;
        }
    }
    else
    {
        if (!blockBase.GetBlockView(block.hashPrev, view, false))
        {
            INFO("AddNewOrigin Get Block View Error: %s ", block.hashPrev.ToString().c_str());
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
        INFO("AddNewOrigin Storage AddNew Error : %s ", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    INFO("AddNew Origin Block : %s ", hash.ToString().c_str());
    INFO("    %s", pIndexNew->ToString().c_str());

    if (!blockBase.CommitBlockView(view, pIndexNew))
    {
        INFO("AddNewOrigin Storage Commit BlockView Error : %s ", hash.ToString().c_str());
        return ERR_SYS_STORAGE_ERROR;
    }

    update = CWorldLineUpdate(pIndexNew);
    view.GetTxUpdated(update.setTxUpdate);
    update.vBlockAddNew.push_back(blockex);

    return OK;
}

bool CWorldLine::GetProofOfWorkTarget(const uint256& hashPrev, int nAlgo, int& nBits, int64& nReward)
{
    CBlockIndex* pIndexPrev;
    if (!blockBase.RetrieveIndex(hashPrev, &pIndexPrev))
    {
        INFO("GetProofOfWorkTarget : Retrieve Prev Index Error: %s ", hashPrev.ToString().c_str());
        return false;
    }
    if (!pIndexPrev->IsPrimary())
    {
        INFO("GetProofOfWorkTarget : Previous is not primary: %s ", hashPrev.ToString().c_str());
        return false;
    }
    if (!pCoreProtocol->GetProofOfWorkTarget(pIndexPrev, nAlgo, nBits, nReward))
    {
        INFO("GetProofOfWorkTarget : Unknown proof-of-work algo: %s ", hashPrev.ToString().c_str());
        return false;
    }
    return true;
}

bool CWorldLine::GetBlockMintReward(const uint256& hashPrev, int64& nReward)
{
    CBlockIndex* pIndexPrev;
    if (!blockBase.RetrieveIndex(hashPrev, &pIndexPrev))
    {
        INFO("Get block reward: Retrieve Prev Index Error, hashPrev: %s", hashPrev.ToString().c_str());
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
            INFO("Get block reward: Get fork profile fail, hashPrev: %s", hashPrev.ToString().c_str());
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

bool CWorldLine::GetBlockLocator(const uint256& hashFork, CBlockLocator& locator)
{
    return blockBase.GetForkBlockLocator(hashFork, locator);
}

bool CWorldLine::GetBlockLocatorFromHash(const uint256& hashFork, const uint256& blockHash, CBlockLocator& locator)
{
    return blockBase.GetForkBlockLocatorFromHash(hashFork, blockHash, locator);
}

bool CWorldLine::GetBlockInv(const uint256& hashFork, const CBlockLocator& locator, vector<uint256>& vBlockHash, size_t nMaxCount)
{
    return blockBase.GetForkBlockInv(hashFork, locator, vBlockHash, nMaxCount);
}

bool CWorldLine::GetBlockDelegateEnrolled(const uint256& hashBlock, CDelegateEnrolled& enrolled)
{
    enrolled.Clear();

    if (cacheEnrolled.Retrieve(hashBlock, enrolled))
    {
        return true;
    }

    CBlockIndex* pIndex;
    if (!blockBase.RetrieveIndex(hashBlock, &pIndex))
    {
        INFO("GetBlockDelegateEnrolled : Retrieve block Index Error: %s ", hashBlock.ToString().c_str());
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
        INFO("GetBlockDelegateEnrolled : Retrieve Avail Delegate Error: %s ", hashBlock.ToString().c_str());
        return false;
    }

    cacheEnrolled.AddNew(hashBlock, enrolled);

    return true;
}

bool CWorldLine::GetBlockDelegateAgreement(const uint256& hashBlock, CDelegateAgreement& agreement)
{
    agreement.Clear();

    if (cacheAgreement.Retrieve(hashBlock, agreement))
    {
        return true;
    }

    CBlockIndex* pIndex = nullptr;
    if (!blockBase.RetrieveIndex(hashBlock, &pIndex))
    {
        INFO("GetBlockDelegateAgreement : Retrieve block Index Error: %s ", hashBlock.ToString().c_str());
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
        INFO("GetBlockDelegateAgreement : Retrieve block Error: %s ", hashBlock.ToString().c_str());
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
        INFO("GetBlockDelegateAgreement : Invalid block proof : %s ", hashBlock.ToString().c_str());
        return false;
    }

    pCoreProtocol->GetDelegatedBallot(agreement.nAgreement, agreement.nWeight, mapBallot, agreement.vBallot, pIndexRef->GetBlockHeight());

    cacheAgreement.AddNew(hashBlock, agreement);

    return true;
}

bool CWorldLine::CheckContainer()
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

bool CWorldLine::RebuildContainer()
{
    return false;
}

bool CWorldLine::InsertGenesisBlock(CBlock& block)
{
    return blockBase.Initiate(block.GetHash(), block);
}

Errno CWorldLine::GetTxContxt(storage::CBlockView& view, const CTransaction& tx, CTxContxt& txContxt)
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

bool CWorldLine::GetBlockChanges(const CBlockIndex* pIndexNew, const CBlockIndex* pIndexFork,
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

bool CWorldLine::GetBlockDelegateAgreement(const uint256& hashBlock, const CBlock& block, const CBlockIndex* pIndexPrev,
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
        INFO("GetBlockDelegateAgreement : Invalid block proof : %s ", hashBlock.ToString().c_str());
        return false;
    }

    pCoreProtocol->GetDelegatedBallot(agreement.nAgreement, agreement.nWeight, mapBallot, agreement.vBallot, pIndexPrev->GetBlockHeight() + 1);

    cacheAgreement.AddNew(hashBlock, agreement);

    return true;
}

Errno CWorldLine::VerifyBlock(const uint256& hashBlock, const CBlock& block, CBlockIndex* pIndexPrev, int64& nReward)
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
  : pForkManager(nullptr), pCoreProtocol(nullptr), pNetChannelModel(nullptr)
{
}

CWorldLineController::~CWorldLineController()
{
}

bool CWorldLineController::HandleInitialize()
{
    if (!GetObject("worldline", pWorldLine))
    {
        ERROR("Failed to request worldline");
        return false;
    }
    if (!GetObject("forkmanager", pForkManager))
    {
        ERROR("Failed to request forkmanager");
        return false;
    }

    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        ERROR("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("netchannelmodel", pNetChannelModel))
    {
        ERROR("Failed to request netchannelmodel");
        return false;
    }

    RegisterHandler(PTR_HANDLER(CAddBlockMessage, boost::bind(&CWorldLineController::HandleAddBlock, this, _1), true));

    return true;
}

void CWorldLineController::HandleDeinitialize()
{
    DeregisterHandler();

    pWorldLine = nullptr;
    pForkManager = nullptr;
    pCoreProtocol = nullptr;
    pNetChannelModel = nullptr;
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

Errno CWorldLineController::AddNewBlock(const CBlock& block, CWorldLineUpdate& update, uint64 nNonce)
{
    // TODO: Remove it when upgrade CDispatcher
    Errno err = AddNewBlockIntoWorldLine(block, update);

    auto spAddedBlockMsg = CAddedBlockMessage::Create();
    spAddedBlockMsg->spNonce = CNonce::Create(0);
    spAddedBlockMsg->hashFork = update.hashFork;
    spAddedBlockMsg->block = block;
    spAddedBlockMsg->update = update;
    PUBLISH_MESSAGE(spAddedBlockMsg);

    return err;
}

Errno CWorldLineController::AddNewOrigin(const CBlock& block, CWorldLineUpdate& update, uint64 nNonce)
{
    // TODO: Remove it when upgrade CDispatcher
    Errno err = AddNewOriginIntoWorldLine(block, update);

    auto spAddedBlockMsg = CAddedBlockMessage::Create();
    spAddedBlockMsg->spNonce = CNonce::Create(0);
    spAddedBlockMsg->hashFork = update.hashFork;
    spAddedBlockMsg->block = block;
    spAddedBlockMsg->update = update;
    PUBLISH_MESSAGE(spAddedBlockMsg);

    return err;
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

bool CWorldLineController::GetBlockLocatorFromHash(const uint256& hashFork, const uint256& blockHash, CBlockLocator& locator)
{
    return pWorldLine->GetBlockLocatorFromHash(hashFork, blockHash, locator);
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

void CWorldLineController::HandleAddBlock(std::shared_ptr<CAddBlockMessage> msg)
{
    if (!msg->spNonce->fValid)
    {
        TRACE("Discard a new block of invalid nonce (%u)", msg->spNonce->nNonce);
        return;
    }

    // Add new block
    auto spAddedBlockMsg = CAddedBlockMessage::Create();
    spAddedBlockMsg->spNonce = msg->spNonce;
    spAddedBlockMsg->hashFork = msg->hashFork;
    spAddedBlockMsg->block = msg->block;

    const CBlock& block = msg->block;
    if (!block.IsOrigin())
    {
        spAddedBlockMsg->nErrno = AddNewBlockIntoWorldLine(block, spAddedBlockMsg->update);
    }
    else
    {
        spAddedBlockMsg->nErrno = AddNewOriginIntoWorldLine(block, spAddedBlockMsg->update);
    }

    msg->promiseAdded.set_value(*spAddedBlockMsg);

    const CWorldLineUpdate& update = spAddedBlockMsg->update;
    PUBLISH_MESSAGE(spAddedBlockMsg);
    SyncForkHeight(update.nLastBlockHeight);
    // Create new fork
    vector<CTransaction>
        vForkTx;
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
                    spAddedOriginMsg->spNonce = CNonce::Create(0);
                    spAddedOriginMsg->nErrno = OK;
                    spAddedOriginMsg->hashFork = originBlock.GetHash();
                    PUBLISH_MESSAGE(spAddedOriginMsg);
                }
                else
                {
                    WARN("Add origin block in tx (%s) failed : %s", tx.GetHash().GetHex().c_str(), ErrorString(err));
                }
            }
            else
            {
                WARN("Invalid origin block found in tx (%s)", tx.GetHash().GetHex().c_str());
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

void CWorldLineController::SyncForkHeight(int nPrimaryHeight)
{
    map<uint256, CForkStatus> mapForkStatus;
    GetForkStatus(mapForkStatus);
    for (map<uint256, CForkStatus>::iterator it = mapForkStatus.begin(); it != mapForkStatus.end(); ++it)
    {
        const uint256& hashFork = (*it).first;
        CForkStatus& status = (*it).second;
        if (!pForkManager->IsAllowed(hashFork) || !pNetChannelModel->IsForkSynchronized(hashFork))
        {
            continue;
        }

        vector<int64> vTimeStamp;
        int nDepth = nPrimaryHeight - status.nLastBlockHeight;

        if (nDepth > 1 && hashFork != pCoreProtocol->GetGenesisBlockHash()
            && GetLastBlockTime(pCoreProtocol->GetGenesisBlockHash(), nDepth, vTimeStamp))
        {
            uint256 hashPrev = status.hashLastBlock;
            for (int nHeight = status.nLastBlockHeight + 1; nHeight < nPrimaryHeight; nHeight++)
            {
                std::promise<CAddedBlockMessage> promiseAdded;
                auto futureAdded = promiseAdded.get_future();
                auto spAddBlockMsg = CAddBlockMessage::Create(std::move(promiseAdded));
                spAddBlockMsg->hashFork = hashFork;
                spAddBlockMsg->spNonce = CNonce::Create();
                spAddBlockMsg->block.nType = CBlock::BLOCK_VACANT;
                spAddBlockMsg->block.hashPrev = hashPrev;
                spAddBlockMsg->block.nTimeStamp = vTimeStamp[nPrimaryHeight - nHeight];
                PUBLISH_MESSAGE(spAddBlockMsg);
                if (futureAdded.get().nErrno != OK)
                {
                    break;
                }
                hashPrev = spAddBlockMsg->block.GetHash();
            }
        }
    }
}

} // namespace bigbang
