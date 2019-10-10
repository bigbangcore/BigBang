// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus.h"

#include "address.h"
#include "template/delegate.h"

using namespace std;
using namespace xengine;

namespace bigbang
{

//////////////////////////////
// CDelegateTxFilter
class CDelegateTxFilter : public CTxFilter
{
public:
    CDelegateTxFilter(CDelegateContext& ctxtIn)
      : CTxFilter(ctxtIn.GetDestination()), ctxt(ctxtIn)
    {
    }
    bool FoundTx(const uint256& hashFork, const CAssembledTx& tx) override
    {
        ctxt.AddNewTx(tx);
        return true;
    }

protected:
    CDelegateContext& ctxt;
};

//////////////////////////////
// CDelegateContext

CDelegateContext::CDelegateContext()
{
}

CDelegateContext::CDelegateContext(const crypto::CKey& keyDelegateIn, const CDestination& destOwnerIn)
  : keyDelegate(keyDelegateIn), destOwner(destOwnerIn)
{
    templDelegate = CTemplate::CreateTemplatePtr(new CTemplateDelegate(keyDelegateIn.GetPubKey(), destOwner));
    destDelegate.SetTemplateId(templDelegate->GetTemplateId());
}

void CDelegateContext::Clear()
{
    mapTx.clear();
    mapUnspent.clear();
}

void CDelegateContext::ChangeTxSet(const CTxSetChange& change)
{
    for (std::size_t i = 0; i < change.vTxRemove.size(); i++)
    {
        const uint256& txid = change.vTxRemove[i].first;
        map<uint256, CDelegateTx>::iterator it = mapTx.find(txid);
        if (it != mapTx.end())
        {
            for (const CTxIn& txin : change.vTxRemove[i].second)
            {
                map<uint256, CDelegateTx>::iterator mi = mapTx.find(txin.prevout.hash);
                if (mi != mapTx.end())
                {
                    mapUnspent.insert(make_pair(txin.prevout, &(*mi).second));
                }
            }
            mapTx.erase(it);
        }
    }
    for (unordered_map<uint256, int>::const_iterator it = change.mapTxUpdate.begin(); it != change.mapTxUpdate.end(); ++it)
    {
        const uint256& txid = (*it).first;
        map<uint256, CDelegateTx>::iterator mi = mapTx.find(txid);
        if (mi != mapTx.end())
        {
            (*mi).second.nBlockHeight = (*it).second;
        }
    }

    for (const CAssembledTx& tx : change.vTxAddNew)
    {
        AddNewTx(tx);
    }
}

void CDelegateContext::AddNewTx(const CAssembledTx& tx)
{
    if (tx.sendTo == destDelegate)
    {
        if (tx.nType == CTransaction::TX_TOKEN && tx.destIn == destOwner)
        {
            uint256 txid = tx.GetHash();
            CDelegateTx& delegateTx = mapTx[txid];

            delegateTx = CDelegateTx(tx);
            mapUnspent.insert(make_pair(CTxOutPoint(txid, 0), &delegateTx));
        }
        else if (tx.nType == CTransaction::TX_CERT && tx.destIn == destDelegate)
        {
            uint256 txid = tx.GetHash();
            CDelegateTx& delegateTx = mapTx[txid];

            delegateTx = CDelegateTx(tx);
            mapUnspent.insert(make_pair(CTxOutPoint(txid, 0), &delegateTx));
            if (delegateTx.nChange != 0)
            {
                mapUnspent.insert(make_pair(CTxOutPoint(txid, 1), &delegateTx));
            }
        }
    }
    if (tx.destIn == destDelegate)
    {
        for (const CTxIn& txin : tx.vInput)
        {
            mapUnspent.erase(txin.prevout);
        }
    }
}

bool CDelegateContext::BuildEnrollTx(CTransaction& tx, int nBlockHeight, int64 nTime,
                                     const uint256& hashAnchor, int64 nTxFee, const vector<unsigned char>& vchData)
{
    tx.SetNull();
    tx.nType = CTransaction::TX_CERT;
    tx.nTimeStamp = nTime;
    tx.hashAnchor = hashAnchor;
    tx.sendTo = destDelegate;
    tx.nAmount = 0;
    tx.nTxFee = nTxFee;
    tx.vchData = vchData;

    int64 nValueIn = 0;
    for (map<CTxOutPoint, CDelegateTx*>::iterator it = mapUnspent.begin(); it != mapUnspent.end(); ++it)
    {
        const CTxOutPoint& txout = (*it).first;
        CDelegateTx* pTx = (*it).second;
        if (pTx->IsLocked(txout.n, nBlockHeight))
        {
            continue;
        }
        if (pTx->GetTxTime() > tx.GetTxTime())
        {
            continue;
        }
        tx.vInput.emplace_back(txout);
        nValueIn += (txout.n == 0 ? pTx->nAmount : pTx->nChange);
        if (nValueIn > nTxFee)
        {
            break;
        }
    }
    if (nValueIn <= nTxFee)
    {
        return false;
    }

    tx.nAmount = nValueIn - nTxFee;

    uint256 hash = tx.GetSignatureHash();
    vector<unsigned char> vchDelegateSig;
    if (!keyDelegate.Sign(hash, vchDelegateSig))
    {
        return false;
    }
    CTemplateDelegate* p = dynamic_cast<CTemplateDelegate*>(templDelegate.get());
    return p->BuildVssSignature(hash, vchDelegateSig, tx.vchSig);
}

//////////////////////////////
// CConsensus

CConsensus::CConsensus()
{
    pCoreProtocol = nullptr;
    pWorldLineCntrl = nullptr;
    pTxPoolCntrl = nullptr;
}

CConsensus::~CConsensus()
{
}

bool CConsensus::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol\n");
        return false;
    }

    if (!GetObject("worldlinecontroller", pWorldLineCntrl))
    {
        Error("Failed to request worldline\n");
        return false;
    }

    if (!GetObject("txpoolcontroller", pTxPoolCntrl))
    {
        Error("Failed to request txpool\n");
        return false;
    }

    if (MintConfig())
    {
        if (!MintConfig()->destMpvss.IsNull() && MintConfig()->keyMpvss != 0)
        {
            crypto::CKey key;
            key.SetSecret(crypto::CCryptoKeyData(MintConfig()->keyMpvss.begin(), MintConfig()->keyMpvss.end()));

            CDelegateContext ctxt(key, MintConfig()->destMpvss);
            mapContext.insert(make_pair(ctxt.GetDestination(), ctxt));

            delegate.AddNewDelegate(ctxt.GetDestination());

            Log("AddNew delegate : %s\n", CAddress(ctxt.GetDestination()).ToString().c_str());
        }
    }

    return true;
}

void CConsensus::HandleDeinitialize()
{
    mapContext.clear();

    pCoreProtocol = nullptr;
    pWorldLineCntrl = nullptr;
    pTxPoolCntrl = nullptr;
}

bool CConsensus::HandleInvoke()
{
    if (!StartActor())
    {
        return false;
    }

    boost::unique_lock<boost::mutex> lock(mutex);

    if (!delegate.Initialize())
    {
        Error("Failed to initialize delegate\n");
        return false;
    }

    if (!LoadDelegateTx())
    {
        Error("Failed to load delegate tx\n");
        return false;
    }

    if (!LoadChain())
    {
        Error("Failed to load chain\n");
        return false;
    }

    return true;
}

void CConsensus::HandleHalt()
{
    StopActor();

    boost::unique_lock<boost::mutex> lock(mutex);

    delegate.Deinitialize();
    for (map<CDestination, CDelegateContext>::iterator it = mapContext.begin(); it != mapContext.end(); ++it)
    {
        (*it).second.Clear();
    }
}

void CConsensus::PrimaryUpdate(const CWorldLineUpdate& update, const CTxSetChange& change, CDelegateRoutine& routine)
{
    boost::unique_lock<boost::mutex> lock(mutex);

    int nStartHeight = update.nLastBlockHeight - update.vBlockAddNew.size();
    if (!update.vBlockRemove.empty())
    {
        int nPrevBlockHeight = nStartHeight + update.vBlockRemove.size();
        delegate.Rollback(nPrevBlockHeight, nStartHeight);
    }

    for (map<CDestination, CDelegateContext>::iterator it = mapContext.begin(); it != mapContext.end(); ++it)
    {
        (*it).second.ChangeTxSet(change);
    }

    int nBlockHeight = nStartHeight + 1;

    for (int i = update.vBlockAddNew.size() - 1; i > 0; i--)
    {
        uint256 hash = update.vBlockAddNew[i].GetHash();

        CDelegateEnrolled enrolled;

        if (pWorldLineCntrl->GetBlockDelegateEnrolled(hash, enrolled))
        {
            delegate::CDelegateEvolveResult result;
            delegate.Evolve(nBlockHeight, enrolled.mapWeight, enrolled.mapEnrollData, result);
        }

        routine.vEnrolledWeight.emplace_back(hash, enrolled.mapWeight);

        nBlockHeight++;
    }

    if (!update.vBlockAddNew.empty())
    {
        uint256 hash = update.vBlockAddNew[0].GetHash();

        CDelegateEnrolled enrolled;

        if (pWorldLineCntrl->GetBlockDelegateEnrolled(hash, enrolled))
        {
            delegate::CDelegateEvolveResult result;
            delegate.Evolve(nBlockHeight, enrolled.mapWeight, enrolled.mapEnrollData, result);

            int nDistributeTargetHeight = nBlockHeight + CONSENSUS_DISTRIBUTE_INTERVAL + 1;
            int nPublishTargetHeight = nBlockHeight + 1;

            for (map<CDestination, vector<unsigned char>>::iterator it = result.mapEnrollData.begin();
                 it != result.mapEnrollData.end(); ++it)
            {
                map<CDestination, CDelegateContext>::iterator mi = mapContext.find((*it).first);
                if (mi != mapContext.end())
                {
                    CTransaction tx;
                    if ((*mi).second.BuildEnrollTx(tx, nBlockHeight, GetNetTime(), hash, 0, (*it).second))
                    {
                        routine.vEnrollTx.push_back(tx);
                    }
                }
            }
            for (map<CDestination, vector<unsigned char>>::iterator it = result.mapDistributeData.begin();
                 it != result.mapDistributeData.end(); ++it)
            {
                delegate.HandleDistribute(nDistributeTargetHeight, (*it).first, (*it).second);
            }
            routine.mapDistributeData = result.mapDistributeData;

            for (map<CDestination, vector<unsigned char>>::iterator it = result.mapPublishData.begin();
                 it != result.mapPublishData.end(); ++it)
            {
                bool fCompleted = false;
                delegate.HandlePublish(nPublishTargetHeight, (*it).first, (*it).second, fCompleted);
                routine.fPublishCompleted = (routine.fPublishCompleted || fCompleted);
            }
            routine.mapPublishData = result.mapPublishData;
        }
        routine.vEnrolledWeight.emplace_back(hash, enrolled.mapWeight);
    }
}

void CConsensus::AddNewTx(const CAssembledTx& tx)
{
    boost::unique_lock<boost::mutex> lock(mutex);
    for (map<CDestination, CDelegateContext>::iterator it = mapContext.begin(); it != mapContext.end(); ++it)
    {
        (*it).second.AddNewTx(tx);
    }
}

bool CConsensus::AddNewDistribute(int nAnchorHeight, const CDestination& destFrom, const vector<unsigned char>& vchDistribute)
{
    boost::unique_lock<boost::mutex> lock(mutex);
    int nDistributeTargetHeight = nAnchorHeight + CONSENSUS_DISTRIBUTE_INTERVAL + 1;
    return delegate.HandleDistribute(nDistributeTargetHeight, destFrom, vchDistribute);
}

bool CConsensus::AddNewPublish(int nAnchorHeight, const CDestination& destFrom, const vector<unsigned char>& vchPublish)
{
    boost::unique_lock<boost::mutex> lock(mutex);
    int nPublishTargetHeight = nAnchorHeight + 1;
    bool fCompleted = false;
    return delegate.HandlePublish(nPublishTargetHeight, destFrom, vchPublish, fCompleted);
}

void CConsensus::GetAgreement(int nTargetHeight, uint256& nAgreement, size_t& nWeight, vector<CDestination>& vBallot)
{
    boost::unique_lock<boost::mutex> lock(mutex);
    map<CDestination, size_t> mapBallot;
    delegate.GetAgreement(nTargetHeight, nAgreement, nWeight, mapBallot);
    pCoreProtocol->GetDelegatedBallot(nAgreement, nWeight, mapBallot, vBallot, nTargetHeight);
}

void CConsensus::GetProof(int nTargetHeight, vector<unsigned char>& vchProof)
{
    boost::unique_lock<boost::mutex> lock(mutex);
    delegate.GetProof(nTargetHeight, vchProof);
}

bool CConsensus::LoadDelegateTx()
{
    const uint256 hashGenesis = pCoreProtocol->GetGenesisBlockHash();
    for (map<CDestination, CDelegateContext>::iterator it = mapContext.begin(); it != mapContext.end(); ++it)
    {
        CDelegateTxFilter txFilter((*it).second);
        if (!pWorldLineCntrl->FilterTx(hashGenesis, txFilter) || !pTxPoolCntrl->FilterTx(hashGenesis, txFilter))
        {
            return false;
        }
    }
    return true;
}

bool CConsensus::LoadChain()
{
    int nLashBlockHeight = pWorldLineCntrl->GetBlockCount(pCoreProtocol->GetGenesisBlockHash()) - 1;
    int nStartHeight = nLashBlockHeight - CONSENSUS_ENROLL_INTERVAL + 1;
    if (nStartHeight < 0)
    {
        nStartHeight = 0;
    }
    for (int i = nStartHeight; i <= nLashBlockHeight; i++)
    {
        uint256 hashBlock;
        if (!pWorldLineCntrl->GetBlockHash(pCoreProtocol->GetGenesisBlockHash(), i, hashBlock))
        {
            return false;
        }
        CDelegateEnrolled enrolled;

        if (pWorldLineCntrl->GetBlockDelegateEnrolled(hashBlock, enrolled))
        {
            delegate::CDelegateEvolveResult result;
            delegate.Evolve(i, enrolled.mapWeight, enrolled.mapEnrollData, result);
        }
    }
    return true;
}

} // namespace bigbang
