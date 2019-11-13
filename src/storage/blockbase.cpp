// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockbase.h"

#include <boost/timer/timer.hpp>
#include <cstdio>

#include "template/template.h"
#include "util.h"

using namespace std;
using namespace boost::filesystem;
using namespace xengine;

#define BLOCKFILE_PREFIX "block"
#define LOGFILE_NAME "storage.log"

namespace bigbang
{
namespace storage
{

//////////////////////////////
// CBlockBaseDBWalker

class CBlockWalker : public CBlockDBWalker
{
public:
    CBlockWalker(CBlockBase* pBaseIn)
      : pBase(pBaseIn) {}
    bool Walk(CBlockOutline& outline) override
    {
        return pBase->LoadIndex(outline);
    }

public:
    CBlockBase* pBase;
};

//////////////////////////////
// CBlockView

CBlockView::CBlockView()
  : pBlockBase(nullptr), hashFork(uint64(0)), fCommittable(false)
{
}

CBlockView::~CBlockView()
{
    Deinitialize();
}

void CBlockView::Initialize(CBlockBase* pBlockBaseIn, boost::shared_ptr<CBlockFork> spForkIn,
                            const uint256& hashForkIn, bool fCommittableIn)
{
    Deinitialize();

    pBlockBase = pBlockBaseIn;
    spFork = spForkIn;
    hashFork = hashForkIn;
    fCommittable = fCommittableIn;
    if (pBlockBase && spFork)
    {
        if (fCommittable)
        {
            spFork->UpgradeLock();
        }
        else
        {
            spFork->ReadLock();
        }
    }
    vTxRemove.clear();
    vTxAddNew.clear();
}

void CBlockView::Deinitialize()
{
    if (pBlockBase)
    {
        if (spFork)
        {
            if (fCommittable)
            {
                spFork->UpgradeUnlock();
            }
            else
            {
                spFork->ReadUnlock();
            }
            spFork = nullptr;
        }
        pBlockBase = nullptr;
        hashFork = 0;
        mapTx.clear();
        mapUnspent.clear();
    }
}

bool CBlockView::ExistsTx(const uint256& txid) const
{
    map<uint256, CTransaction>::const_iterator it = mapTx.find(txid);
    if (it != mapTx.end())
    {
        return (!(*it).second.IsNull());
    }
    return (!!(pBlockBase->ExistsTx(txid)));
}

bool CBlockView::RetrieveTx(const uint256& txid, CTransaction& tx)
{
    map<uint256, CTransaction>::const_iterator it = mapTx.find(txid);
    if (it != mapTx.end())
    {
        tx = (*it).second;
        return (!tx.IsNull());
    }
    return pBlockBase->RetrieveTx(txid, tx);
}

bool CBlockView::RetrieveUnspent(const CTxOutPoint& out, CTxOut& unspent)
{
    map<CTxOutPoint, CUnspent>::const_iterator it = mapUnspent.find(out);
    if (it != mapUnspent.end())
    {
        if ((*it).second.IsNull())
        {
            StdTrace("CBlockView", "RetrieveUnspent: unspent is null, txout: [%d]:%s", out.n, out.hash.GetHex().c_str());
            return false;
        }
        unspent = (*it).second;
    }
    else
    {
        if (!pBlockBase->GetTxUnspent(hashFork, out, unspent))
        {
            StdTrace("CBlockView", "RetrieveUnspent: BlockBase GetTxUnspent fail, txout: [%d]:%s, hashFork: %s",
                     out.n, out.hash.GetHex().c_str(), hashFork.GetHex().c_str());
            return false;
        }
    }
    return true;
}

void CBlockView::AddTx(const uint256& txid, const CTransaction& tx, const CDestination& destIn, int64 nValueIn)
{
    mapTx[txid] = tx;
    vTxAddNew.push_back(txid);

    for (int i = 0; i < tx.vInput.size(); i++)
    {
        mapUnspent[tx.vInput[i].prevout].Disable();
    }
    CTxOut output0(tx);
    if (!output0.IsNull())
    {
        mapUnspent[CTxOutPoint(txid, 0)].Enable(output0);
    }
    CTxOut output1(tx, destIn, nValueIn);
    if (!output1.IsNull())
    {
        mapUnspent[CTxOutPoint(txid, 1)].Enable(output1);
    }
}

void CBlockView::RemoveTx(const uint256& txid, const CTransaction& tx, const CTxContxt& txContxt)
{
    mapTx[txid].SetNull();
    vTxRemove.push_back(txid);
    for (int i = 0; i < tx.vInput.size(); i++)
    {
        const CTxInContxt& in = txContxt.vin[i];
        mapUnspent[tx.vInput[i].prevout].Enable(CTxOut(txContxt.destIn, in.nAmount, in.nTxTime, in.nLockUntil));
    }
    mapUnspent[CTxOutPoint(txid, 0)].Disable();
    mapUnspent[CTxOutPoint(txid, 1)].Disable();
}

void CBlockView::GetUnspentChanges(vector<CTxUnspent>& vAddNew, vector<CTxOutPoint>& vRemove)
{
    vAddNew.reserve(mapUnspent.size());
    vRemove.reserve(mapUnspent.size());

    for (map<CTxOutPoint, CUnspent>::iterator it = mapUnspent.begin(); it != mapUnspent.end(); ++it)
    {
        const CTxOutPoint& out = (*it).first;
        const CUnspent& unspent = (*it).second;
        if (unspent.IsModified())
        {
            if (!unspent.IsNull())
            {
                vAddNew.push_back(CTxUnspent(out, unspent));
            }
            else
            {
                vRemove.push_back(out);
            }
        }
    }
}

void CBlockView::GetTxUpdated(set<uint256>& setUpdate)
{
    for (int i = 0; i < vTxRemove.size(); i++)
    {
        const uint256& txid = vTxRemove[i];
        if (!mapTx[txid].IsNull())
        {
            setUpdate.insert(txid);
        }
    }
}

void CBlockView::GetTxRemoved(vector<uint256>& vRemove)
{
    vRemove.reserve(vTxRemove.size());
    for (int i = 0; i < vTxRemove.size(); i++)
    {
        const uint256& txid = vTxRemove[i];
        if (mapTx[txid].IsNull())
        {
            vRemove.push_back(txid);
        }
    }
}

//////////////////////////////
// CBlockBase

CBlockBase::CBlockBase()
  : fDebugLog(false)
{
}

CBlockBase::~CBlockBase()
{
    dbBlock.Deinitialize();
    tsBlock.Deinitialize();
}

bool CBlockBase::Initialize(const path& pathDataLocation, bool fDebug, bool fRenewDB)
{
    if (!SetupLog(pathDataLocation, fDebug))
    {
        return false;
    }

    Log("B", "Initializing... (Path : %s)", pathDataLocation.string().c_str());

    if (!dbBlock.Initialize(pathDataLocation))
    {
        Error("B", "Failed to initialize block db");
        return false;
    }

    if (!tsBlock.Initialize(pathDataLocation / "block", BLOCKFILE_PREFIX))
    {
        dbBlock.Deinitialize();
        Error("B", "Failed to initialize block tsfile");
        return false;
    }

    if (fRenewDB)
    {
        Clear();
    }
    else if (!LoadDB())
    {
        dbBlock.Deinitialize();
        tsBlock.Deinitialize();
        {
            CWriteLock wlock(rwAccess);

            ClearCache();
        }
        Error("B", "Failed to load block db");
        return false;
    }
    Log("B", "Initialized");
    return true;
}

void CBlockBase::Deinitialize()
{
    dbBlock.Deinitialize();
    tsBlock.Deinitialize();
    {
        CWriteLock wlock(rwAccess);

        ClearCache();
    }
    Log("B", "Deinitialized");
}

bool CBlockBase::Exists(const uint256& hash) const
{
    CReadLock rlock(rwAccess);

    return (!!mapIndex.count(hash));
}

bool CBlockBase::ExistsTx(const uint256& txid)
{
    uint256 hashFork;
    CTxIndex txIndex;
    return dbBlock.RetrieveTxIndex(txid, txIndex, hashFork);
}

bool CBlockBase::IsEmpty() const
{
    CReadLock rlock(rwAccess);

    return mapIndex.empty();
}

void CBlockBase::Clear()
{
    CWriteLock wlock(rwAccess);

    dbBlock.RemoveAll();
    ClearCache();
}

bool CBlockBase::Initiate(const uint256& hashGenesis, const CBlock& blockGenesis, const uint256& nChainTrust)
{
    if (!IsEmpty())
    {
        StdTrace("[BlockBase][TRACE]", "Is not empty");
        return false;
    }
    uint32 nFile, nOffset;
    if (!tsBlock.Write(CBlockEx(blockGenesis), nFile, nOffset))
    {
        StdTrace("[BlockBase][TRACE]", "Write genesis %s block failed", hashGenesis.ToString().c_str());
        return false;
    }

    uint32 nTxOffset = nOffset + blockGenesis.GetTxSerializedOffset();
    uint256 txidMintTx = blockGenesis.txMint.GetHash();

    vector<pair<uint256, CTxIndex>> vTxNew;
    vTxNew.push_back(make_pair(txidMintTx, CTxIndex(0, nFile, nTxOffset)));

    vector<CTxUnspent> vAddNew;
    vAddNew.push_back(CTxUnspent(CTxOutPoint(txidMintTx, 0), CTxOut(blockGenesis.txMint)));

    {
        CWriteLock wlock(rwAccess);
        CBlockIndex* pIndexNew = AddNewIndex(hashGenesis, blockGenesis, nFile, nOffset, nChainTrust);
        if (pIndexNew == nullptr)
        {
            StdTrace("[BlockBase][TRACE]", "Add New Index %s block failed", hashGenesis.ToString().c_str());
            return false;
        }

        if (!dbBlock.AddNewBlock(CBlockOutline(pIndexNew)))
        {
            StdTrace("[BlockBase][TRACE]", "Add New genesis Block %s block failed", hashGenesis.ToString().c_str());
            return false;
        }

        CDelegateContext ctxtDelegate;
        if (!dbBlock.UpdateDelegateContext(hashGenesis, ctxtDelegate))
        {
            StdTrace("[BlockBase][TRACE]", "Update Delegate Contetxt %s block failed", hashGenesis.ToString().c_str());
            return false;
        }

        CProfile profile;
        if (!profile.Load(blockGenesis.vchProof))
        {
            StdTrace("[BlockBase][TRACE]", "Load genesis %s block Proof failed", hashGenesis.ToString().c_str());
            return false;
        }

        CForkContext ctxt(hashGenesis, uint64(0), uint64(0), profile);
        if (!dbBlock.AddNewForkContext(ctxt))
        {
            StdTrace("[BlockBase][TRACE]", "Add New Fork COntext %s block failed", hashGenesis.ToString().c_str());
            return false;
        }

        if (!dbBlock.AddNewFork(hashGenesis))
        {
            StdTrace("[BlockBase][TRACE]", "Add New Fork %s  failed", hashGenesis.ToString().c_str());
            return false;
        }

        boost::shared_ptr<CBlockFork> spFork = AddNewFork(profile, pIndexNew);
        if (spFork != nullptr)
        {
            CWriteLock wForkLock(spFork->GetRWAccess());

            if (!dbBlock.UpdateFork(hashGenesis, hashGenesis, uint64(0), vTxNew, vector<uint256>(), vAddNew, vector<CTxOutPoint>()))
            {
                StdTrace("[BlockBase][TRACE]", "Update Fork %s failed", hashGenesis.ToString().c_str());
                return false;
            }
            spFork->UpdateLast(pIndexNew);
        }
        else
        {
            StdTrace("[BlockBase][TRACE]", "Add New Fork profile  %s  failed", hashGenesis.ToString().c_str());
            return false;
        }

        Log("B", "Initiate genesis %s", hashGenesis.ToString().c_str());
    }
    return true;
}

bool CBlockBase::AddNew(const uint256& hash, CBlockEx& block, CBlockIndex** ppIndexNew, const uint256& nChainTrust)
{
    if (Exists(hash))
    {
        StdTrace("[BlockBase][TRACE]", "Exist Block: %s", hash.ToString().c_str());
        return false;
    }

    uint32 nFile, nOffset;
    if (!tsBlock.Write(block, nFile, nOffset))
    {
        StdTrace("[BlockBase][TRACE]", "Write block %s failed", hash.ToString().c_str());
        return false;
    }
    {
        CWriteLock wlock(rwAccess);

        CBlockIndex* pIndexNew = AddNewIndex(hash, block, nFile, nOffset, nChainTrust);
        if (pIndexNew == nullptr)
        {
            StdTrace("[BlockBase][TRACE]", "AddNewIndex faild: %s", hash.ToString().c_str());
            return false;
        }

        if (!dbBlock.AddNewBlock(CBlockOutline(pIndexNew)))
        {
            StdTrace("[BlockBase][TRACE]", "AddNewBlock failed: %s", hash.ToString().c_str());
            mapIndex.erase(hash);
            delete pIndexNew;
            return false;
        }

        if (pIndexNew->IsPrimary())
        {
            if (!UpdateDelegate(hash, block, CDiskPos(nFile, nOffset)))
            {
                StdTrace("[BlockBase][TRACE]", "UpdateDElegate failed: %s", hash.ToString().c_str());
                dbBlock.RemoveBlock(hash);
                mapIndex.erase(hash);
                delete pIndexNew;
                return false;
            }
        }

        *ppIndexNew = pIndexNew;
    }

    Log("B", "AddNew block,hash=%s", hash.ToString().c_str());
    return true;
}

bool CBlockBase::AddNewForkContext(const CForkContext& ctxt)
{
    if (!dbBlock.AddNewForkContext(ctxt))
    {
        Error("F", "Failed to addnew forkcontext in %s", ctxt.hashFork.GetHex().c_str());
        return false;
    }
    Log("F", "AddNew forkcontext,hash=%s", ctxt.hashFork.GetHex().c_str());
    return true;
}

bool CBlockBase::Retrieve(const uint256& hash, CBlock& block)
{
    block.SetNull();

    CBlockIndex* pIndex;
    {
        CReadLock rlock(rwAccess);

        if (!(pIndex = GetIndex(hash)))
        {
            StdTrace("[BlockBase][TRACE]", "Retrieve::GetIndex %s block failed", hash.ToString().c_str());
            return false;
        }
    }
    if (!tsBlock.Read(block, pIndex->nFile, pIndex->nOffset, false))
    {
        StdTrace("[BlockBase][TRACE]", "Retrieve::Read %s block failed", hash.ToString().c_str());
        return false;
    }
    return true;
}

bool CBlockBase::Retrieve(const CBlockIndex* pIndex, CBlock& block)
{
    block.SetNull();

    if (!tsBlock.Read(block, pIndex->nFile, pIndex->nOffset, false))
    {
        StdTrace("[BlockBase][TRACE]", "RetrieveFromIndex::Read %s block failed", pIndex->GetBlockHash().ToString().c_str());
        return false;
    }
    return true;
}

bool CBlockBase::Retrieve(const uint256& hash, CBlockEx& block)
{
    block.SetNull();

    CBlockIndex* pIndex;
    {
        CReadLock rlock(rwAccess);

        if (!(pIndex = GetIndex(hash)))
        {
            StdTrace("[BlockBase][TRACE]", "RetrieveBlockEx::GetIndex %s block failed", hash.ToString().c_str());
            return false;
        }
    }
    if (!tsBlock.Read(block, pIndex->nFile, pIndex->nOffset))
    {
        StdTrace("[BlockBase][TRACE]", "RetrieveBlockEx::Read %s block failed", hash.ToString().c_str());

        return false;
    }
    return true;
}

bool CBlockBase::Retrieve(const CBlockIndex* pIndex, CBlockEx& block)
{
    block.SetNull();

    if (!tsBlock.Read(block, pIndex->nFile, pIndex->nOffset))
    {
        StdTrace("[BlockBase][TRACE]", "RetrieveFromIndex::GetIndex %s block failed", pIndex->GetBlockHash().ToString().c_str());

        return false;
    }
    return true;
}

bool CBlockBase::RetrieveIndex(const uint256& hash, CBlockIndex** ppIndex)
{
    CReadLock rlock(rwAccess);

    *ppIndex = GetIndex(hash);
    return (*ppIndex != nullptr);
}

bool CBlockBase::RetrieveFork(const uint256& hash, CBlockIndex** ppIndex)
{
    CReadLock rlock(rwAccess);

    boost::shared_ptr<CBlockFork> spFork = GetFork(hash);
    if (spFork != nullptr)
    {
        CReadLock rForkLock(spFork->GetRWAccess());

        *ppIndex = spFork->GetLast();

        return true;
    }

    return false;
}

bool CBlockBase::RetrieveFork(const string& strName, CBlockIndex** ppIndex)
{
    CReadLock rlock(rwAccess);

    boost::shared_ptr<CBlockFork> spFork = GetFork(strName);
    if (spFork != nullptr)
    {
        CReadLock rForkLock(spFork->GetRWAccess());

        *ppIndex = spFork->GetLast();

        return true;
    }

    return false;
}

bool CBlockBase::RetrieveProfile(const uint256& hash, CProfile& profile)
{
    CReadLock rlock(rwAccess);

    boost::shared_ptr<CBlockFork> spFork = GetFork(hash);
    if (spFork == nullptr)
    {
        return false;
    }

    profile = spFork->GetProfile();

    return true;
}

bool CBlockBase::RetrieveForkContext(const uint256& hash, CForkContext& ctxt)
{
    return dbBlock.RetrieveForkContext(hash, ctxt);
}

bool CBlockBase::RetrieveAncestry(const uint256& hash, vector<pair<uint256, uint256>> vAncestry)
{
    CForkContext ctxt;
    if (!dbBlock.RetrieveForkContext(hash, ctxt))
    {
        StdTrace("[BlockBase][TRACE]", "Ancestry Retrieve hashFork %s failed", hash.ToString().c_str());
        return false;
    }

    while (ctxt.hashParent != 0)
    {
        vAncestry.push_back(make_pair(ctxt.hashParent, ctxt.hashJoint));
        if (!dbBlock.RetrieveForkContext(ctxt.hashParent, ctxt))
        {
            return false;
        }
    }

    std::reverse(vAncestry.begin(), vAncestry.end());
    return true;
}

bool CBlockBase::RetrieveOrigin(const uint256& hash, CBlock& block)
{
    block.SetNull();

    CForkContext ctxt;
    if (!dbBlock.RetrieveForkContext(hash, ctxt))
    {
        StdTrace("[BlockBase][TRACE]", "RetrieveOrigin::RetrieveForkContext %s block failed", hash.ToString().c_str());
        return false;
    }

    CTransaction tx;
    if (!RetrieveTx(ctxt.txidEmbedded, tx))
    {
        StdTrace("[BlockBase][TRACE]", "RetrieveOrigin::RetrieveTx %s tx failed", ctxt.txidEmbedded.ToString().c_str());
        return false;
    }

    try
    {
        CBufStream ss;
        ss.Write((const char*)&tx.vchData[0], tx.vchData.size());
        ss >> block;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

bool CBlockBase::RetrieveTx(const uint256& txid, CTransaction& tx)
{
    tx.SetNull();
    uint256 hashFork;
    CTxIndex txIndex;
    if (!dbBlock.RetrieveTxIndex(txid, txIndex, hashFork))
    {
        StdTrace("[BlockBase][TRACE]", "RetrieveTx::RetrieveTxIndex %s tx failed", txid.ToString().c_str());
        return false;
    }

    if (!tsBlock.Read(tx, txIndex.nFile, txIndex.nOffset))
    {
        StdTrace("[BlockBase][TRACE]", "RetrieveTx::Read %s tx failed", txid.ToString().c_str());
        return false;
    }
    return true;
}

bool CBlockBase::RetrieveTx(const uint256& hashFork, const uint256& txid, CTransaction& tx)
{
    tx.SetNull();

    CTxIndex txIndex;
    if (!dbBlock.RetrieveTxIndex(hashFork, txid, txIndex))
    {
        StdTrace("[BlockBase][TRACE]", "RetrieveTxFromFork::RetrieveTxIndex fork:%s txid: %s tx failed",
                 hashFork.ToString().c_str(), txid.ToString().c_str());
        return false;
    }

    if (!tsBlock.Read(tx, txIndex.nFile, txIndex.nOffset))
    {
        StdTrace("[BlockBase][TRACE]", "RetrieveTxFromFork::Read %s tx failed",
                 txid.ToString().c_str());
        return false;
    }
    return true;
}

bool CBlockBase::RetrieveTxLocation(const uint256& txid, uint256& hashFork, int& nHeight)
{
    CTxIndex txIndex;
    if (!dbBlock.RetrieveTxIndex(txid, txIndex, hashFork))
    {
        StdTrace("[BlockBase][TRACE]", "RetrieveTxLocation::RetrieveTxIndex %s tx failed",
                 txid.ToString().c_str());
        return false;
    }

    nHeight = txIndex.nBlockHeight;
    return true;
}

bool CBlockBase::RetrieveAvailDelegate(const uint256& hash, int height, const vector<uint256>& vBlockRange,
                                       int64 nDelegateWeightRatio,
                                       map<CDestination, size_t>& mapWeight,
                                       map<CDestination, vector<unsigned char>>& mapEnrollData)
{
    map<CDestination, int64> mapVote;
    if (!dbBlock.RetrieveDelegate(hash, mapVote))
    {
        StdTrace("[BlockBase][TRACE]", "RetrieveAvailDelegate::RetrieveDelegate %s block failed",
                 hash.ToString().c_str());
        return false;
    }

    map<CDestination, CDiskPos> mapEnrollTxPos;
    if (!dbBlock.RetrieveEnroll(height, vBlockRange, mapEnrollTxPos))
    {
        StdTrace("[BlockBase][TRACE]", "RetrieveAvailDelegate::RetrieveEnroll block %s height %d failed",
                 hash.ToString().c_str(), height);
        return false;
    }

    for (map<CDestination, int64>::iterator it = mapVote.begin(); it != mapVote.end(); ++it)
    {
        if ((*it).second >= nDelegateWeightRatio)
        {
            const CDestination& dest = (*it).first;
            map<CDestination, CDiskPos>::iterator mi = mapEnrollTxPos.find(dest);
            if (mi != mapEnrollTxPos.end())
            {
                CTransaction tx;
                if (!tsBlock.Read(tx, (*mi).second))
                {
                    StdTrace("[BlockBase][TRACE]", "RetrieveAvailDelegate::Read %s tx failed", tx.GetHash().ToString().c_str());
                    return false;
                }
                mapWeight.insert(make_pair(dest, size_t((*it).second / nDelegateWeightRatio)));
                mapEnrollData.insert(make_pair(dest, tx.vchData));
            }
        }
    }
    return true;
}

void CBlockBase::ListForkIndex(multimap<int, CBlockIndex*>& mapForkIndex)
{
    CReadLock rlock(rwAccess);

    mapForkIndex.clear();
    for (map<uint256, boost::shared_ptr<CBlockFork>>::iterator it = mapFork.begin(); it != mapFork.end(); ++it)
    {
        CReadLock rForkLock((*it).second->GetRWAccess());

        CBlockIndex* pIndex = (*it).second->GetLast();
        mapForkIndex.insert(make_pair(pIndex->pOrigin->GetBlockHeight() - 1, pIndex));
    }
}

bool CBlockBase::GetBlockView(CBlockView& view)
{
    boost::shared_ptr<CBlockFork> spFork;
    view.Initialize(this, spFork, uint64(0), false);
    return true;
}

bool CBlockBase::GetBlockView(const uint256& hash, CBlockView& view, bool fCommitable)
{
    CBlockIndex* pIndex = nullptr;
    uint256 hashOrigin;
    boost::shared_ptr<CBlockFork> spFork;

    {
        CReadLock rlock(rwAccess);
        pIndex = GetIndex(hash);
        if (pIndex == nullptr)
        {
            StdTrace("[BlockBase][TRACE]", "GetBlockView::GetIndex %s block failed", hash.ToString().c_str());
            return false;
        }

        hashOrigin = pIndex->GetOriginHash();
        spFork = GetFork(hashOrigin);
        if (spFork == nullptr)
        {
            StdTrace("[BlockBase][TRACE]", "GetBlockView::GetFork %s  failed", hashOrigin.ToString().c_str());
            return false;
        }
    }

    view.Initialize(this, spFork, hashOrigin, fCommitable);

    {
        CReadLock rlock(rwAccess);
        CBlockIndex* pForkLast = spFork->GetLast();

        vector<CBlockIndex*> vPath;
        CBlockIndex* pBranch = GetBranch(pForkLast, pIndex, vPath);

        for (CBlockIndex* p = pForkLast; p != pBranch; p = p->pPrev)
        {
            // remove block tx;
            CBlockEx block;
            if (!tsBlock.Read(block, p->nFile, p->nOffset))
            {
                return false;
            }
            for (int j = block.vtx.size() - 1; j >= 0; j--)
            {
                view.RemoveTx(block.vtx[j].GetHash(), block.vtx[j], block.vTxContxt[j]);
            }
            if (!block.txMint.IsNull())
            {
                view.RemoveTx(block.txMint.GetHash(), block.txMint);
            }
        }

        for (int i = vPath.size() - 1; i >= 0; i--)
        {
            // add block tx;
            CBlockEx block;
            if (!tsBlock.Read(block, vPath[i]->nFile, vPath[i]->nOffset))
            {
                return false;
            }
            view.AddTx(block.txMint.GetHash(), block.txMint);
            for (int j = 0; j < block.vtx.size(); j++)
            {
                const CTxContxt& txContxt = block.vTxContxt[j];
                view.AddTx(block.vtx[j].GetHash(), block.vtx[j], txContxt.destIn, txContxt.GetValueIn());
            }
        }
    }
    return true;
}

bool CBlockBase::GetForkBlockView(const uint256& hashFork, CBlockView& view)
{
    boost::shared_ptr<CBlockFork> spFork = GetFork(hashFork);
    if (spFork == nullptr)
    {
        return false;
    }
    view.Initialize(this, spFork, hashFork, false);
    return true;
}

bool CBlockBase::CommitBlockView(CBlockView& view, CBlockIndex* pIndexNew)
{
    const uint256 hashFork = pIndexNew->GetOriginHash();

    boost::shared_ptr<CBlockFork> spFork;

    if (hashFork == view.GetForkHash())
    {
        if (!view.IsCommittable())
        {
            StdTrace("[BlockBase][TRACE]", "CommitBlockView Is not COmmitable");
            return false;
        }
        spFork = view.GetFork();
    }
    else
    {
        CProfile profile;
        if (!LoadForkProfile(pIndexNew->pOrigin, profile))
        {
            StdTrace("[BlockBase][TRACE]", "CommitBlockView::LoadForkProfile %s block failed", pIndexNew->pOrigin->GetBlockHash().ToString().c_str());
            return false;
        }
        if (!dbBlock.AddNewFork(hashFork))
        {
            StdTrace("[BlockBase][TRACE]", "CommitBlockView::AddNewFork %s  failed", hashFork.ToString().c_str());
            return false;
        }
        spFork = AddNewFork(profile, pIndexNew);
    }

    vector<pair<uint256, CTxIndex>> vTxNew;
    if (!GetTxNewIndex(view, pIndexNew, vTxNew))
    {
        StdTrace("[BlockBase][TRACE]", "CommitBlockView::GetTxNewIndex view failed");
        return false;
    }

    vector<uint256> vTxDel;
    view.GetTxRemoved(vTxDel);

    vector<CTxUnspent> vAddNew;
    vector<CTxOutPoint> vRemove;
    view.GetUnspentChanges(vAddNew, vRemove);

    if (hashFork == view.GetForkHash())
    {
        spFork->UpgradeToWrite();
    }

    if (!dbBlock.UpdateFork(hashFork, pIndexNew->GetBlockHash(), view.GetForkHash(), vTxNew, vTxDel, vAddNew, vRemove))
    {
        StdTrace("[BlockBase][TRACE]", "CommitBlockView::UpdateFork %s  failed", hashFork.ToString().c_str());
        return false;
    }
    spFork->UpdateLast(pIndexNew);

    Log("B", "Update fork %s, last block hash=%s", hashFork.ToString().c_str(),
        pIndexNew->GetBlockHash().ToString().c_str());
    return true;
}

bool CBlockBase::LoadIndex(CBlockOutline& outline)
{
    uint256 hash = outline.GetBlockHash();
    CBlockIndex* pIndexNew = nullptr;

    map<uint256, CBlockIndex*>::iterator mi = mapIndex.find(hash);
    if (mi != mapIndex.end())
    {
        pIndexNew = (*mi).second;
        *pIndexNew = static_cast<CBlockIndex&>(outline);
    }
    else
    {
        pIndexNew = new CBlockIndex(static_cast<CBlockIndex&>(outline));
        if (pIndexNew == nullptr)
        {
            return false;
        }
        mi = mapIndex.insert(make_pair(hash, pIndexNew)).first;
    }

    pIndexNew->phashBlock = &((*mi).first);
    pIndexNew->pPrev = nullptr;
    pIndexNew->pOrigin = pIndexNew;

    if (outline.hashPrev != 0)
    {
        pIndexNew->pPrev = GetOrCreateIndex(outline.hashPrev);
    }

    if (!pIndexNew->IsOrigin())
    {
        pIndexNew->pOrigin = GetOrCreateIndex(outline.hashOrigin);
    }

    return true;
}

bool CBlockBase::LoadTx(CTransaction& tx, uint32 nTxFile, uint32 nTxOffset, uint256& hashFork)
{
    tx.SetNull();
    if (!tsBlock.Read(tx, nTxFile, nTxOffset))
    {
        StdTrace("[BlockBase][TRACE]", "LoadTx::Read %s block failed", tx.GetHash().ToString().c_str());
        return false;
    }
    CBlockIndex* pIndex = (tx.hashAnchor != 0 ? GetIndex(tx.hashAnchor) : GetOriginIndex(tx.GetHash()));
    if (pIndex == nullptr)
    {
        return false;
    }
    hashFork = pIndex->GetOriginHash();
    return true;
}

bool CBlockBase::FilterTx(const uint256& hashFork, CTxFilter& filter)
{
    CReadLock rlock(rwAccess);

    boost::shared_ptr<CBlockFork> spFork = GetFork(hashFork);
    if (spFork == nullptr)
    {
        StdTrace("[BlockBase][TRACE]", "FilterTx::GetFork %s  failed", hashFork.ToString().c_str());
        return false;
    }

    CReadLock rForkLock(spFork->GetRWAccess());

    for (CBlockIndex* pIndex = spFork->GetOrigin(); pIndex != nullptr; pIndex = pIndex->pNext)
    {
        CBlockEx block;
        if (!tsBlock.Read(block, pIndex->nFile, pIndex->nOffset))
        {
            return false;
        }
        int nBlockHeight = pIndex->GetBlockHeight();
        if (filter.setDest.count(block.txMint.sendTo))
        {
            if (!filter.FoundTx(hashFork, CAssembledTx(block.txMint, nBlockHeight)))
            {
                return false;
            }
        }
        for (int i = 0; i < block.vtx.size(); i++)
        {
            CTransaction& tx = block.vtx[i];
            CTxContxt& ctxt = block.vTxContxt[i];

            if (filter.setDest.count(tx.sendTo) || filter.setDest.count(ctxt.destIn))
            {
                if (!filter.FoundTx(hashFork, CAssembledTx(tx, nBlockHeight, ctxt.destIn, ctxt.GetValueIn())))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool CBlockBase::FilterTx(const uint256& hashFork, int nDepth, CTxFilter& filter)
{
    CReadLock rlock(rwAccess);

    boost::shared_ptr<CBlockFork> spFork = GetFork(hashFork);
    if (spFork == nullptr)
    {
        StdTrace("[BlockBase][TRACE]", "FilterTx::GetFork %s  failed", hashFork.ToString().c_str());
        return false;
    }

    CReadLock rForkLock(spFork->GetRWAccess());

    int nCount = 0;
    for (CBlockIndex* pIndex = spFork->GetLast(); pIndex != nullptr && nCount++ < nDepth; pIndex = pIndex->pPrev)
    {
        CBlockEx block;
        if (!tsBlock.Read(block, pIndex->nFile, pIndex->nOffset))
        {
            return false;
        }
        int nBlockHeight = pIndex->GetBlockHeight();
        if (filter.setDest.count(block.txMint.sendTo))
        {
            if (!filter.FoundTx(hashFork, CAssembledTx(block.txMint, nBlockHeight)))
            {
                return false;
            }
        }
        for (int i = 0; i < block.vtx.size(); i++)
        {
            CTransaction& tx = block.vtx[i];
            CTxContxt& ctxt = block.vTxContxt[i];

            if (filter.setDest.count(tx.sendTo) || filter.setDest.count(ctxt.destIn))
            {
                if (!filter.FoundTx(hashFork, CAssembledTx(tx, nBlockHeight, ctxt.destIn, ctxt.GetValueIn())))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool CBlockBase::ListForkContext(std::vector<CForkContext>& vForkCtxt)
{
    return dbBlock.ListForkContext(vForkCtxt);
}

bool CBlockBase::GetForkBlockLocator(const uint256& hashFork, CBlockLocator& locator, uint256& hashDepth, int nIncStep)
{
    CReadLock rlock(rwAccess);

    boost::shared_ptr<CBlockFork> spFork = GetFork(hashFork);
    if (spFork == nullptr)
    {
        StdTrace("BlockBase", "GetForkBlockLocator GetFork failed, hashFork: %s", hashFork.ToString().c_str());
        return false;
    }

    CBlockIndex* pIndex = nullptr;
    {
        CReadLock rForkLock(spFork->GetRWAccess());
        pIndex = spFork->GetLast();
        if (pIndex == nullptr)
        {
            StdTrace("BlockBase", "GetForkBlockLocator GetLast failed, hashFork: %s", hashFork.ToString().c_str());
            return false;
        }
    }

    if (hashDepth != 0)
    {
        CBlockIndex* pStartIndex = GetIndex(hashDepth);
        if (pStartIndex != nullptr && pStartIndex->pNext != nullptr)
        {
            pIndex = pStartIndex;
        }
    }

    while (pIndex)
    {
        if (pIndex->GetOriginHash() != hashFork)
        {
            hashDepth = 0;
            break;
        }
        locator.vBlockHash.push_back(pIndex->GetBlockHash());
        if (pIndex->IsOrigin())
        {
            hashDepth = 0;
            break;
        }
        if (locator.vBlockHash.size() >= nIncStep / 2)
        {
            pIndex = pIndex->pPrev;
            if (pIndex == nullptr)
            {
                hashDepth = 0;
            }
            else
            {
                hashDepth = pIndex->GetBlockHash();
            }
            break;
        }
        for (int i = 0; i < nIncStep && !pIndex->IsOrigin(); i++)
        {
            pIndex = pIndex->pPrev;
            if (pIndex == nullptr)
            {
                hashDepth = 0;
                break;
            }
        }
    }

    return true;
}

bool CBlockBase::GetForkBlockInv(const uint256& hashFork, const CBlockLocator& locator, vector<uint256>& vBlockHash, size_t nMaxCount)
{
    CReadLock rlock(rwAccess);

    boost::shared_ptr<CBlockFork> spFork = GetFork(hashFork);
    if (spFork == nullptr)
    {
        StdTrace("[BlockBase][TRACE]", "GetForkBlockInv::GetFork %s failed", hashFork.ToString().c_str());
        return false;
    }

    CReadLock rForkLock(spFork->GetRWAccess());
    CBlockIndex* pIndexLast = spFork->GetLast();
    CBlockIndex* pIndex = nullptr;
    for (const uint256& hash : locator.vBlockHash)
    {
        pIndex = GetIndex(hash);
        if (pIndex != nullptr && (pIndex == pIndexLast || pIndex->pNext != nullptr))
        {
            if (pIndex->GetOriginHash() != hashFork)
            {
                return false;
            }
            break;
        }
        pIndex = nullptr;
    }

    if (pIndex != nullptr)
    {
        pIndex = pIndex->pNext;
        while (pIndex != nullptr && vBlockHash.size() < nMaxCount)
        {
            vBlockHash.push_back(pIndex->GetBlockHash());
            pIndex = pIndex->pNext;
        }
    }
    return true;
}

bool CBlockBase::CheckConsistency(int nCheckLevel, int nCheckDepth)
{
    boost::timer::cpu_timer t_lock;
    t_lock.start();

    CReadLock rlock(rwAccess);

    Log("B", "Getting lock duration ===> %s.", t_lock.format().c_str());

    boost::timer::cpu_timer t_check;
    t_check.start();

    Log("B", "Check consistency with parameters check-level:%d and check-depth:%d.", nCheckLevel, nCheckDepth);

    int nLevel = nCheckLevel;
    if (nCheckLevel < 0)
    {
        nLevel = 0;
    }
    if (nCheckLevel > 3)
    {
        nLevel = 3;
    }
    int32 nDepth = nCheckDepth;

    Log("B", "Consistency checking level is %d", nLevel);

    vector<pair<uint256, uint256>> vFork;
    if (!dbBlock.ListFork(vFork))
    {
        Error("B", "List fork failed.");
        return false;
    }

    for (const auto& fork : vFork)
    {
        boost::timer::cpu_timer t_fork;
        t_fork.start();

        //checking of level 0: fork/block

        //check field refblock of table fork must be in rows in table block
        CBlockIndex* pBlockRefIndex = GetIndex(fork.second);
        if (!pBlockRefIndex)
        {
            Error("B", "Get referenced block index failed.");
            return false;
        }

        boost::shared_ptr<CBlockFork> spFork = GetFork(fork.first);
        if (nullptr == spFork)
        {
            Error("B", "Get fork failed.");
            return false;
        }
        CBlockIndex* pLastBlock = spFork->GetLast();
        if (nullptr == pLastBlock)
        {
            Error("B", "Get last block index of current fork failed.");
            return false;
        }

        bool fIsMainFork = spFork->GetOrigin()->IsPrimary();

        if (0 == nDepth || pLastBlock->nHeight < nDepth)
        {
            nDepth = pLastBlock->nHeight;
            Log("B", "Consistency checking depth is {%d} for fork:{%s}", nDepth, fork.first.ToString().c_str());
        }

        CBlockIndex* pIndex = pLastBlock;
        map<CDestination, int64> mapNextBlockDelegate;

        map<CTxOutPoint, CTxUnspent> mapUnspentUTXO;
        vector<CTxOutPoint> vSpentUTXO;

        while (pIndex && pLastBlock->nHeight - pIndex->nHeight < nDepth)
        {
            //be able to read from block files
            CBlockEx block;
            if (!tsBlock.ReadDirect(block, pIndex->nFile, pIndex->nOffset))
            {
                Error("B", "Retrieve block from file directly failed.");
                return false;
            }

            //consistent between database and block file
            if (!(pIndex->GetBlockHash() == block.GetHash()
                  && pIndex->pPrev->GetBlockHash() == block.hashPrev
                  && pIndex->nVersion == block.nVersion
                  && pIndex->nType == block.nType
                  && pIndex->nTimeStamp == block.nTimeStamp
                  && ((pIndex->nMintType == 0) ? block.IsVacant()
                                               : (!block.IsVacant() && pIndex->txidMint == block.txMint.GetHash() && pIndex->nMintType == block.txMint.nType))))
            {
                Error("B", "Block info are not consistent in db and file.");
                return false;
            }

            //checking of level 1: transaction
            if (nLevel >= 1 && !pIndex->IsVacant())
            {
                auto lmdChkTx = [&](const uint256& txid, const CTxIndex& pTxIndex) -> bool {
                    CTransaction tx;
                    if (!tsBlock.ReadDirect(tx, pTxIndex.nFile, pTxIndex.nOffset))
                    {
                        Error("B", "Retrieve tx from file directly failed.");
                        return false;
                    }

                    //consistent between database and block file
                    if (txid != tx.GetHash() || pTxIndex.nBlockHeight != pIndex->nHeight)
                    {
                        return false;
                    }

                    return true;
                };

                CTxIndex pTxIdx;
                uint256 fk;
                if (!dbBlock.RetrieveTxIndex(pIndex->txidMint, pTxIdx, fk))
                {
                    Error("B", "Retrieve mint tx index from db failed.");
                    return false;
                }
                if (!lmdChkTx(pIndex->txidMint, pTxIdx))
                {
                    Error("B", "Mint tx info are not consistent in db and file.");
                    return false;
                }

                for (auto const& tx : block.vtx)
                {
                    pTxIdx.SetNull();
                    const uint256& txid = tx.GetHash();
                    if (!dbBlock.RetrieveTxIndex(txid, pTxIdx, fk))
                    {
                        Error("B", "Retrieve token tx index from db failed.");
                        return false;
                    }
                    if (!lmdChkTx(txid, pTxIdx))
                    {
                        Error("B", "Token tx info are not consistent in db and file.");
                        return false;
                    }
                }
            }

            //checking of level 2: delegate/enroll
            if (nLevel >= 2 && fIsMainFork)
            {
                static bool fIsLastBlock = true;

                if (fIsLastBlock)
                {
                    if (!dbBlock.RetrieveDelegate(block.GetHash(), mapNextBlockDelegate))
                    {
                        Error("B", "Retrieve the latest delegate record from db failed.");
                        return false;
                    }
                    fIsLastBlock = false;
                }
                else
                { //compare delegate in this iteration with the previous one
                    map<CDestination, int64> mapPrevBlockDelegate;
                    if (!dbBlock.RetrieveDelegate(block.GetHash(), mapPrevBlockDelegate))
                    {
                        Error("B", "Retrieve the following previous delegate record from db failed.");
                        return false;
                    }
                    if (mapNextBlockDelegate != mapPrevBlockDelegate)
                    {
                        Error("B", "Delegate records followed one by one do not match.");
                        return false;
                    }
                    mapNextBlockDelegate = mapPrevBlockDelegate;
                }

                if (block.txMint.nType == CTransaction::TX_STAKE)
                {
                    mapNextBlockDelegate[block.txMint.sendTo] -= block.txMint.nAmount;
                }

                map<pair<uint256, CDestination>, tuple<uint256, uint32, uint32>> mapEnrollRanged;
                for (int i = 0; i < block.vtx.size(); i++)
                {
                    const CTransaction& tx = block.vtx[i];
                    {
                        CTemplateId tid;
                        if (tx.sendTo.GetTemplateId(tid) && tid.GetType() == TEMPLATE_DELEGATE)
                        {
                            mapNextBlockDelegate[tx.sendTo] -= tx.nAmount;
                        }
                    }

                    const CTxContxt& txContxt = block.vTxContxt[i];
                    {
                        CTemplateId tid;
                        if (txContxt.destIn.GetTemplateId(tid) && tid.GetType() == TEMPLATE_DELEGATE)
                        {
                            mapNextBlockDelegate[txContxt.destIn] += tx.nAmount + tx.nTxFee;
                        }
                    }

                    if (tx.nType == CTransaction::TX_CERT)
                    {
                        const uint256& anchor = tx.hashAnchor;
                        const CDestination& dest = tx.sendTo;
                        const uint256& blk = block.GetHash();
                        CTxIndex txIdx;
                        uint256 fk;
                        if (!dbBlock.RetrieveTxIndex(tx.GetHash(), txIdx, fk))
                        {
                            Error("B", "Retrieve enroll tx index from table transaction failed.");
                            return false;
                        }
                        const uint32& nFile = txIdx.nFile;
                        const uint32& nOffset = txIdx.nOffset;
                        mapEnrollRanged[make_pair(anchor, dest)] = make_tuple(blk, nFile, nOffset);
                    }
                }

                vector<CDestination> vDestNull;
                for (const auto& delegate : mapNextBlockDelegate)
                {
                    if (delegate.second < 0)
                    {
                        Error("B", "Amount on delegate template address must not be less than zero.");
                        return false;
                    }
                    if (delegate.second == 0)
                    {
                        vDestNull.push_back(delegate.first);
                    }
                }
                for (const auto& dest : vDestNull)
                {
                    mapNextBlockDelegate.erase(dest);
                }

                //compare enroll ranged in argument of nDepth with table enroll
                vector<uint256> vBlockRange;
                vBlockRange.push_back(block.GetHash());
                map<CDestination, CDiskPos> mapRes;
                if (!dbBlock.RetrieveEnroll(GetIndex(block.hashPrev)->GetBlockHeight(), vBlockRange, mapRes))
                {
                    Error("B", "Retrieve enroll tx records from table enroll failed.");
                    return false;
                }
                map<CDestination, CDiskPos> mapResComp;
                for (const auto& enroll : mapEnrollRanged)
                {
                    const CDestination& dest = enroll.first.second;
                    const tuple<uint256, uint32, uint32>& pos = enroll.second;
                    const uint32& file = get<1>(pos);
                    const uint32& offset = get<2>(pos);
                    mapResComp.insert(make_pair(dest, CDiskPos(file, offset)));
                }
                if (mapRes != mapResComp)
                {
                    Error("B", "Enroll transactions in tables enroll and transaction do not match.");
                    return false;
                }
            }

            //checking of level 3: unspent
            if (nLevel >= 3)
            {
                CTransaction txMint;
                if (!RetrieveTx(block.txMint.GetHash(), txMint))
                {
                    return false;
                }
                mapUnspentUTXO.insert(make_pair(CTxOutPoint(block.txMint.GetHash(), 0), CTxUnspent(CTxOutPoint(block.txMint.GetHash(), 0), CTxOut(txMint.sendTo, txMint.nAmount, txMint.nTimeStamp, txMint.nLockUntil))));

                for (int i = 0; i < block.vtx.size(); ++i)
                {
                    const CTransaction& tx = block.vtx[i];
                    const CTxContxt& txCtxt = block.vTxContxt[i];
                    int64 nChange = txCtxt.GetValueIn() - tx.nAmount - tx.nTxFee;
                    mapUnspentUTXO.insert(make_pair(
                        CTxOutPoint(tx.GetHash(), 0),
                        CTxUnspent(CTxOutPoint(tx.GetHash(), 0), CTxOut(tx.sendTo, tx.nAmount, tx.nTimeStamp, tx.nLockUntil))));
                    if (nChange > 0)
                    {
                        Log("B", "Tx(%s) with a change(%s) on height(%d): to prepare to check.", tx.GetHash().ToString().c_str(), to_string(nChange).c_str(), pIndex->nHeight);
                        if (!CheckInputSingleAddressForTxWithChange(tx.GetHash()))
                        {
                            Error("B", "Tx(%s) with a change(%s) on height(%d): input must be a single address.", tx.GetHash().ToString().c_str(), to_string(nChange).c_str(), pIndex->nHeight);
                            return false;
                        }
                        else
                        {
                            mapUnspentUTXO.insert(make_pair(
                                CTxOutPoint(tx.GetHash(), 1),
                                CTxUnspent(CTxOutPoint(tx.GetHash(), 1), CTxOut(txCtxt.destIn, nChange, tx.nTimeStamp, tx.nLockUntil))));
                        }
                    }
                    for (const auto& txin : tx.vInput)
                    {
                        vSpentUTXO.push_back(txin.prevout);
                    }
                }

                vector<CTxOutPoint> vRemovedUTXO;
                for (const auto& spent : vSpentUTXO)
                {
                    if (mapUnspentUTXO.find(spent) != mapUnspentUTXO.end())
                    {
                        mapUnspentUTXO.erase(spent);
                        vRemovedUTXO.push_back(spent);
                    }
                }

                for (const auto& txDel : vRemovedUTXO)
                {
                    const auto& pos = find(vSpentUTXO.begin(), vSpentUTXO.end(), txDel);
                    vSpentUTXO.erase(pos);
                }
            }

            pIndex = pIndex->pPrev;
        }
        Log("B", "Checking duration before comparing unspent ===> %s", t_fork.format().c_str());
        if (nLevel >= 3)
        {
            //compare unspent with transaction
            CForkUnspentCheckWalker walker(mapUnspentUTXO);
            if (!dbBlock.WalkThroughUnspent(fork.first, walker))
            {
                Error("B", "{%d} ranged unspent records failed to walk through.", mapUnspentUTXO.size());
                return false;
            }

            if (walker.nMatch != mapUnspentUTXO.size())
            {
                Error("B", "{%d} ranged unspent records do not match with full collection of unspent.", mapUnspentUTXO.size());
                return false;
            }
        }

        Log("B", "Checking duration of fork{%s} ===> %s", fork.first.ToString().c_str(), t_fork.format().c_str());
    }

    Log("B", "Checking duration ===> %s", t_check.format().c_str());

    Log("B", "Data consistency verified.");

    return true;
}

bool CBlockBase::CheckInputSingleAddressForTxWithChange(const uint256& txid)
{
    CTransaction tx;
    if (!RetrieveTx(txid, tx))
    {
        Error("B", "[CBlockBase::CheckInputSingleAddressForTxWithChange](%s): Failed to call to RetrieveTx.", txid.ToString().c_str());
        return false;
    }

    //get all inputs whose index is 0 if any
    vector<CDestination> vDestNoChange;
    vector<uint256> vTxExistChange;
    for (const auto& i : tx.vInput)
    {
        if (i.prevout.n == 0)
        {
            CTransaction prevTx;
            if (!RetrieveTx(i.prevout.hash, prevTx))
            {
                Error("B", "[CBlockBase::CheckInputSingleAddressForTxWithChange](%s): Failed to call to RetrieveTxIndex.", txid.ToString().c_str());
                return false;
            }
            vDestNoChange.push_back(prevTx.sendTo);
        }
        else
        {
            vTxExistChange.push_back(i.prevout.hash);
        }
    }

    sort(vDestNoChange.begin(), vDestNoChange.end());
    auto end_iter = unique(vDestNoChange.begin(), vDestNoChange.end());
    vDestNoChange.erase(end_iter, vDestNoChange.end());

    //if destinations are not equal, return false
    if (vDestNoChange.size() > 1)
    {
        Error("B", "[CBlockBase::CheckInputSingleAddressForTxWithChange](%s): {vDestNoChange.size() > 1}.", txid.ToString().c_str());
        return false;
    }

    //if destination from input is not equal to output, return false
    if (vDestNoChange.size() == 1)
    {
        CDestination dest = *(vDestNoChange.begin());
        if (dest != tx.sendTo)
        {
            Error("B", "[CBlockBase::CheckInputSingleAddressForTxWithChange](%s): {dest != txIdx.sendTo}.", txid.ToString().c_str());
            return false;
        }
    }

    //if exist output index is 1, recur the process
    vector<bool> vRes;
    for (const auto& i : vTxExistChange)
    {
        vRes.push_back(CheckInputSingleAddressForTxWithChange(i));
    }

    if (!vRes.empty())
    {
        int nFalse = count(vRes.begin(), vRes.end(), false);
        if (nFalse <= 0)
        {
            return true;
        }
        else
        {
            Error("B", "Tx(%s) one or more preout validate failed.", txid.ToString().c_str());
            return false;
        }
        //        return count(vRes.begin(), vRes.end(), false) <= 0;
    }
    else
    {
        return true;
    }
}

bool CBlockBase::ListForkUnspent(const uint256& forkId, const CDestination& destOwner, int nMax, std::vector<CTxUnspent>& vUnspent)
{
    vUnspent.clear();
    CListUnspentWalker walker(forkId, destOwner, nMax);
    dbBlock.WalkThroughUnspent(forkId, walker);
    vUnspent = walker.vUnspent;
    return true;
}

CBlockIndex* CBlockBase::GetIndex(const uint256& hash) const
{
    map<uint256, CBlockIndex*>::const_iterator mi = mapIndex.find(hash);
    return (mi != mapIndex.end() ? (*mi).second : nullptr);
}

CBlockIndex* CBlockBase::GetOrCreateIndex(const uint256& hash)
{
    map<uint256, CBlockIndex*>::const_iterator mi = mapIndex.find(hash);
    if (mi == mapIndex.end())
    {
        mi = mapIndex.insert(make_pair(hash, new CBlockIndex())).first;
    }
    return ((*mi).second);
}

CBlockIndex* CBlockBase::GetBranch(CBlockIndex* pIndexRef, CBlockIndex* pIndex, vector<CBlockIndex*>& vPath)
{
    vPath.clear();
    while (pIndex != pIndexRef)
    {
        if (pIndexRef->GetBlockTime() > pIndex->GetBlockTime())
        {
            pIndexRef = pIndexRef->pPrev;
        }
        else if (pIndex->GetBlockTime() > pIndexRef->GetBlockTime())
        {
            vPath.push_back(pIndex);
            pIndex = pIndex->pPrev;
        }
        else
        {
            vPath.push_back(pIndex);
            pIndex = pIndex->pPrev;
            pIndexRef = pIndexRef->pPrev;
        }
    }
    return pIndex;
}

CBlockIndex* CBlockBase::GetOriginIndex(const uint256& txidMint) const
{
    for (map<uint256, boost::shared_ptr<CBlockFork>>::const_iterator mi = mapFork.begin(); mi != mapFork.end(); ++mi)
    {
        CBlockIndex* pIndex = (*mi).second->GetOrigin();
        if (pIndex->txidMint == txidMint)
        {
            return pIndex;
        }
    }
    return nullptr;
}

CBlockIndex* CBlockBase::AddNewIndex(const uint256& hash, const CBlock& block, uint32 nFile, uint32 nOffset, uint256 nChainTrust)
{
    CBlockIndex* pIndexNew = new CBlockIndex(block, nFile, nOffset);
    if (pIndexNew != nullptr)
    {
        map<uint256, CBlockIndex*>::iterator mi = mapIndex.insert(make_pair(hash, pIndexNew)).first;
        pIndexNew->phashBlock = &((*mi).first);

        int64 nMoneySupply = block.GetBlockMint();
        uint64 nRandBeacon = block.GetBlockBeacon();
        CBlockIndex* pIndexPrev = nullptr;
        map<uint256, CBlockIndex*>::iterator miPrev = mapIndex.find(block.hashPrev);
        if (miPrev != mapIndex.end())
        {
            pIndexPrev = (*miPrev).second;
            pIndexNew->pPrev = pIndexPrev;
            if (!pIndexNew->IsOrigin())
            {
                pIndexNew->pOrigin = pIndexPrev->pOrigin;
                nRandBeacon ^= pIndexNew->pOrigin->nRandBeacon;
            }
            nMoneySupply += pIndexPrev->nMoneySupply;
            nChainTrust += pIndexPrev->nChainTrust;
        }
        pIndexNew->nMoneySupply = nMoneySupply;
        pIndexNew->nChainTrust = nChainTrust;
        pIndexNew->nRandBeacon = nRandBeacon;
    }
    return pIndexNew;
}

boost::shared_ptr<CBlockFork> CBlockBase::GetFork(const uint256& hash)
{
    map<uint256, boost::shared_ptr<CBlockFork>>::iterator mi = mapFork.find(hash);
    return (mi != mapFork.end() ? (*mi).second : nullptr);
}

boost::shared_ptr<CBlockFork> CBlockBase::GetFork(const std::string& strName)
{
    for (map<uint256, boost::shared_ptr<CBlockFork>>::iterator mi = mapFork.begin(); mi != mapFork.end(); ++mi)
    {
        const CProfile& profile = (*mi).second->GetProfile();
        if (profile.strName == strName)
        {
            return ((*mi).second);
        }
    }
    return nullptr;
}

boost::shared_ptr<CBlockFork> CBlockBase::AddNewFork(const CProfile& profileIn, CBlockIndex* pIndexLast)
{
    boost::shared_ptr<CBlockFork> spFork = boost::shared_ptr<CBlockFork>(new CBlockFork(profileIn, pIndexLast));
    if (spFork != nullptr)
    {
        spFork->UpdateNext();
        mapFork.insert(make_pair(pIndexLast->GetOriginHash(), spFork));
    }

    return spFork;
}

bool CBlockBase::LoadForkProfile(const CBlockIndex* pIndexOrigin, CProfile& profile)
{
    profile.SetNull();

    CBlock block;
    if (!Retrieve(pIndexOrigin, block))
    {
        return false;
    }

    if (!profile.Load(block.vchProof))
    {
        return false;
    }

    return true;
}

bool CBlockBase::UpdateDelegate(const uint256& hash, CBlockEx& block, const CDiskPos& posBlock)
{
    CDelegateContext ctxtDelegate;

    map<CDestination, int64>& mapDelegate = ctxtDelegate.mapVote;
    map<int, map<CDestination, CDiskPos>>& mapEnrollTx = ctxtDelegate.mapEnrollTx;

    if (!dbBlock.RetrieveDelegate(block.hashPrev, mapDelegate))
    {
        return false;
    }

    CBufStream ss;
    CVarInt var(block.vtx.size());
    uint32 nOffset = posBlock.nOffset + block.GetTxSerializedOffset()
                     + ss.GetSerializeSize(block.txMint)
                     + ss.GetSerializeSize(var);

    for (int i = 0; i < block.vtx.size(); i++)
    {
        CTransaction& tx = block.vtx[i];
        {
            CTemplateId tid;
            if (tx.sendTo.GetTemplateId(tid) && tid.GetType() == TEMPLATE_DELEGATE)
            {
                mapDelegate[tx.sendTo] += tx.nAmount;
            }
        }

        CTxContxt& txContxt = block.vTxContxt[i];
        {
            CTemplateId tid;
            if (txContxt.destIn.GetTemplateId(tid) && tid.GetType() == TEMPLATE_DELEGATE)
            {
                mapDelegate[txContxt.destIn] -= tx.nAmount + tx.nTxFee;
            }
        }

        if (tx.nType == CTransaction::TX_CERT)
        {
            mapEnrollTx[GetIndex(block.hashPrev)->GetBlockHeight()].insert(make_pair(txContxt.destIn, CDiskPos(posBlock.nFile, nOffset)));
        }
        nOffset += ss.GetSerializeSize(tx);
    }
    return dbBlock.UpdateDelegateContext(hash, ctxtDelegate);
}

bool CBlockBase::GetTxUnspent(const uint256 fork, const CTxOutPoint& out, CTxOut& unspent)
{
    return dbBlock.RetrieveTxUnspent(fork, out, unspent);
}

bool CBlockBase::GetTxNewIndex(CBlockView& view, CBlockIndex* pIndexNew, vector<pair<uint256, CTxIndex>>& vTxNew)
{
    vector<CBlockIndex*> vPath;
    if (view.GetFork() != nullptr && view.GetFork()->GetLast() != nullptr)
    {
        GetBranch(view.GetFork()->GetLast(), pIndexNew, vPath);
    }
    else
    {
        vPath.push_back(pIndexNew);
    }

    CBufStream ss;
    for (int i = vPath.size() - 1; i >= 0; i--)
    {
        CBlockIndex* pIndex = vPath[i];
        CBlockEx block;
        if (!tsBlock.Read(block, pIndex->nFile, pIndex->nOffset))
        {
            return false;
        }
        int nHeight = pIndex->GetBlockHeight();
        uint32 nOffset = pIndex->nOffset + block.GetTxSerializedOffset();

        if (!block.txMint.IsNull())
        {
            CTxIndex txIndex(nHeight, pIndex->nFile, nOffset);
            vTxNew.push_back(make_pair(block.txMint.GetHash(), txIndex));
        }
        nOffset += ss.GetSerializeSize(block.txMint);

        CVarInt var(block.vtx.size());
        nOffset += ss.GetSerializeSize(var);
        for (int i = 0; i < block.vtx.size(); i++)
        {
            CTransaction& tx = block.vtx[i];
            uint256 txid = tx.GetHash();
            CTxIndex txIndex(nHeight, pIndex->nFile, nOffset);
            vTxNew.push_back(make_pair(txid, txIndex));
            nOffset += ss.GetSerializeSize(tx);
        }
    }
    return true;
}

void CBlockBase::ClearCache()
{
    map<uint256, CBlockIndex*>::iterator mi;
    for (mi = mapIndex.begin(); mi != mapIndex.end(); ++mi)
    {
        delete (*mi).second;
    }
    mapIndex.clear();
    mapFork.clear();
}

bool CBlockBase::LoadDB()
{
    CWriteLock wlock(rwAccess);

    ClearCache();
    CBlockWalker walker(this);
    if (!dbBlock.WalkThroughBlock(walker))
    {
        ClearCache();
        return false;
    }

    vector<pair<uint256, uint256>> vFork;
    if (!dbBlock.ListFork(vFork))
    {
        ClearCache();
        return false;
    }
    for (int i = 0; i < vFork.size(); i++)
    {
        CBlockIndex* pIndex = GetIndex(vFork[i].second);
        if (pIndex == nullptr)
        {
            ClearCache();
            return false;
        }
        CProfile profile;
        if (!LoadForkProfile(pIndex->pOrigin, profile))
        {
            return false;
        }
        boost::shared_ptr<CBlockFork> spFork = AddNewFork(profile, pIndex);
        if (spFork == nullptr)
        {
            return false;
        }
    }

    return true;
}

bool CBlockBase::SetupLog(const path& pathLocation, bool fDebug)
{

    if (!log.SetLogFilePath((pathLocation / LOGFILE_NAME).string()))
    {
        return false;
    }
    fDebugLog = fDebug;
    return true;
}

} // namespace storage
} // namespace bigbang
