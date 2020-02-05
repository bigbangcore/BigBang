// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus.h"

#include "address.h"
#include "template/delegate.h"
#include "dispatcherevent.h"

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
        auto it = mapTx.find(txid);
        if (it != mapTx.end())
        {
            for (const CTxIn& txin : change.vTxRemove[i].second)
            {
                auto mi = mapTx.find(txin.prevout.hash);
                if (mi != mapTx.end())
                {
                    mapUnspent.insert(make_pair(txin.prevout, &(*mi).second));
                }
            }
            mapTx.erase(it);
        }
    }

    for (auto it = change.mapTxUpdate.begin(); it != change.mapTxUpdate.end(); ++it)
    {
        const uint256& txid = (*it).first;
        auto mi = mapTx.find(txid);
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
    for (auto it = mapUnspent.begin(); it != mapUnspent.end(); ++it)
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
        tx.vInput.emplace_back(CTxIn(txout));
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
    auto p = dynamic_cast<CTemplateDelegate*>(templDelegate.get());
    return p->BuildVssSignature(hash, vchDelegateSig, tx.vchSig);
}

//////////////////////////////
// CConsensus

CConsensus::CConsensus()
{
    pCoreProtocol = nullptr;
    pBlockChain   = nullptr;
    pTxPool       = nullptr;
}

bool CConsensus::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("blockchain", pBlockChain))
    {
        Error("Failed to request blockchain");
        return false;
    }

    if (!GetObject("txpool", pTxPool))
    {
        Error("Failed to request txpool");
        return false;
    }

    if (!GetObject("dispatcher", pDispatcher))
    {
        Error("Failed to request dispatcher");
        return false;
    }

    if (!MintConfig()->destMpvss.IsNull() && MintConfig()->keyMpvss != 0)
    {
        crypto::CKey key;
        key.SetSecret(crypto::CCryptoKeyData(MintConfig()->keyMpvss.begin(), MintConfig()->keyMpvss.end()));

        CDelegateContext ctxt(key, MintConfig()->destMpvss);
        mapContext.insert(make_pair(ctxt.GetDestination(), ctxt));

        delegate.AddNewDelegate(ctxt.GetDestination());

        Log("AddNew delegate : %s", CAddress(ctxt.GetDestination()).ToString().c_str());
    }

    return true;
}

void CConsensus::HandleDeinitialize()
{
    mapContext.clear();

    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pTxPool = nullptr;
    pDispatcher = nullptr;
}

bool CConsensus::HandleInvoke()
{
    boost::unique_lock<boost::mutex> lock(mutex);

    if (!delegate.Initialize())
    {
        Error("Failed to initialize delegate");
        return false;
    }

    if (!LoadDelegateTx())
    {
        Error("Failed to load delegate tx");
        return false;
    }

    if (!LoadChain())
    {
        Error("Failed to load chain");
        return false;
    }

    return true;
}

void CConsensus::HandleHalt()
{
    boost::unique_lock<boost::mutex> lock(mutex);

    delegate.Deinitialize();
    for (auto it = mapContext.begin(); it != mapContext.end(); ++it)
    {
        (*it).second.Clear();
    }
}

void CConsensus::PrimaryUpdate(const CBlockChainUpdate& update, const CTxSetChange& change, CDelegateRoutine& routine)
{
    boost::unique_lock<boost::mutex> lock(mutex);

    int nStartHeight = update.nLastBlockHeight - update.vBlockAddNew.size();
    if (!update.vBlockRemove.empty())
    {
        int nPrevBlockHeight = nStartHeight + update.vBlockRemove.size();
        delegate.Rollback(nPrevBlockHeight, nStartHeight);
    }

    for (auto it = mapContext.begin(); it != mapContext.end(); ++it)
    {
        (*it).second.ChangeTxSet(change);
    }

    int nBlockHeight = nStartHeight + 1;

    for (int i = update.vBlockAddNew.size() - 1; i > 0; i--)
    {
        uint256 hash = update.vBlockAddNew[i].GetHash();

        CDelegateEnrolled enrolled;

        if (pBlockChain->GetBlockDelegateEnrolled(hash, enrolled))
        {
            delegate::CDelegateEvolveResult result;
            delegate.Evolve(nBlockHeight, enrolled.mapWeight, enrolled.mapEnrollData, result, hash);
        }

        int height;
        uint256 fork = pCoreProtocol->GetGenesisBlockHash();
        pBlockChain->GetBlockLocation(hash, fork, height);
        routine.vEnrolledWeight.emplace_back(make_pair(height, enrolled.mapWeight));

        nBlockHeight++;
    }

    if (!update.vBlockAddNew.empty())
    {
        uint256 hash = update.vBlockAddNew[0].GetHash();

        CDelegateEnrolled enrolled;

        if (pBlockChain->GetBlockDelegateEnrolled(hash, enrolled))
        {
            delegate::CDelegateEvolveResult result;
            delegate.Evolve(nBlockHeight, enrolled.mapWeight, enrolled.mapEnrollData, result, hash);

            int nDistributeTargetHeight = nBlockHeight + CONSENSUS_DISTRIBUTE_INTERVAL + 1;
            int nPublishTargetHeight = nBlockHeight + 1;

            for (auto it = result.mapEnrollData.begin();
                 it != result.mapEnrollData.end(); ++it)
            {
                auto mi = mapContext.find((*it).first);
                if (mi != mapContext.end())
                {
                    CTransaction tx;
                    if ((*mi).second.BuildEnrollTx(tx, nBlockHeight, GetNetTime(), pCoreProtocol->GetGenesisBlockHash(), 0, (*it).second))
                    {
                        routine.vEnrollTx.push_back(tx);
                    }
                }
            }
            for (auto it = result.mapDistributeData.begin();
                 it != result.mapDistributeData.end(); ++it)
            {
                delegate.HandleDistribute(nDistributeTargetHeight, (*it).first, (*it).second);
            }
            routine.mapDistributeData = result.mapDistributeData;

            for (auto it = result.mapPublishData.begin();
                 it != result.mapPublishData.end(); ++it)
            {
                bool fCompleted = false;
                delegate.HandlePublish(nPublishTargetHeight, (*it).first, (*it).second, fCompleted);
                routine.fPublishCompleted = (routine.fPublishCompleted || fCompleted);
            }
            routine.mapPublishData = result.mapPublishData;
        }
        int height;
        uint256 fork = pCoreProtocol->GetGenesisBlockHash();
        pBlockChain->GetBlockLocation(hash, fork, height);
        routine.vEnrolledWeight.emplace_back(make_pair(height, enrolled.mapWeight));
    }

    map<CDestination, size_t> mapBallot;
    CDelegateAgreement agree;
    delegate.GetAgreement(update.nLastBlockHeight + 1, agree.nAgreement, agree.nWeight, agree.vBallot);
    delegate.GetAgreement(update.nLastBlockHeight + 1, nAgreement, nWeight, mapBallot);
    pCoreProtocol->GetDelegatedBallot(nAgreement, nWeight, mapBallot, vBallot, nTargetHeight);

    auto pEvent = new CEventDispatcherAgreement();
    if (pEvent != nullptr)
    {
        pDispatcher->PostEvent(pEvent);
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
    for (auto it = mapContext.begin(); it != mapContext.end(); ++it)
    {
        CDelegateTxFilter txFilter((*it).second);
        if (!pBlockChain->FilterTx(hashGenesis, txFilter) || !pTxPool->FilterTx(hashGenesis, txFilter))
        {
            return false;
        }
    }
    return true;
}

bool CConsensus::LoadChain()
{
    int nLashBlockHeight = pBlockChain->GetBlockCount(pCoreProtocol->GetGenesisBlockHash()) - 1;
    int nStartHeight = nLashBlockHeight - CONSENSUS_ENROLL_INTERVAL + 1;
    if (nStartHeight < 0)
    {
        nStartHeight = 0;
    }
    for (int i = nStartHeight; i <= nLashBlockHeight; i++)
    {
        uint256 hashBlock;
        if (!pBlockChain->GetBlockHash(pCoreProtocol->GetGenesisBlockHash(), i, hashBlock))
        {
            return false;
        }
        CDelegateEnrolled enrolled;

        if (pBlockChain->GetBlockDelegateEnrolled(hashBlock, enrolled))
        {
            delegate::CDelegateEvolveResult result;
            delegate.Evolve(i, enrolled.mapWeight, enrolled.mapEnrollData, result, hashBlock);
        }
    }
    return true;
}

} // namespace bigbang
