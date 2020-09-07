// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus.h"

#include "address.h"
#include "block.h"
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
            const std::vector<CTxIn>& vTxIn = change.vTxRemove[i].second;
            for (const CTxIn& txin : vTxIn)
            {
                map<uint256, CDelegateTx>::iterator mi = mapTx.find(txin.prevout.hash);
                if (mi != mapTx.end())
                {
                    mapUnspent.insert(make_pair(txin.prevout, &(*mi).second));
                    StdTrace("CDelegateContext", "ChangeTxSet: Remove tx: add unspent: [%d] %s",
                             txin.prevout.n, txin.prevout.hash.GetHex().c_str());
                }
            }

            mapUnspent.erase(CTxOutPoint(txid, 0));
            StdTrace("CDelegateContext", "ChangeTxSet: Remove tx: erase unspent: [0] %s", txid.GetHex().c_str());

            mapUnspent.erase(CTxOutPoint(txid, 1));
            StdTrace("CDelegateContext", "ChangeTxSet: Remove tx: erase unspent: [1] %s", txid.GetHex().c_str());

            mapTx.erase(it);
            StdTrace("CDelegateContext", "ChangeTxSet: Remove tx: txid: %s", txid.GetHex().c_str());
        }
    }

    for (map<uint256, int>::const_iterator it = change.mapTxUpdate.begin(); it != change.mapTxUpdate.end(); ++it)
    {
        const uint256& txid = (*it).first;
        map<uint256, CDelegateTx>::iterator mi = mapTx.find(txid);
        if (mi != mapTx.end())
        {
            StdTrace("CDelegateContext", "ChangeTxSet: Update tx: txid: %s", txid.GetHex().c_str());
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
    /*if (tx.sendTo == destDelegate)
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
    }*/

    bool fAddTx = false;
    if (tx.sendTo == destDelegate)
    {
        uint256 txid = tx.GetHash();
        CDelegateTx& delegateTx = mapTx[txid];
        delegateTx = CDelegateTx(tx);
        fAddTx = true;
        mapUnspent.insert(make_pair(CTxOutPoint(txid, 0), &delegateTx));
        //StdTrace("CDelegateContext", "AddNewTx: sendto and unspent: [0] %s", txid.GetHex().c_str());
    }
    if (tx.destIn == destDelegate)
    {
        for (const CTxIn& txin : tx.vInput)
        {
            //StdTrace("CDelegateContext", "AddNewTx: destIn erase unspent: [%d] %s", txin.prevout.n, txin.prevout.hash.GetHex().c_str());
            mapUnspent.erase(txin.prevout);
        }
        uint256 txid = tx.GetHash();
        CDelegateTx& delegateTx = mapTx[txid];
        if (!fAddTx)
            delegateTx = CDelegateTx(tx);
        if (delegateTx.nChange != 0)
        {
            mapUnspent.insert(make_pair(CTxOutPoint(txid, 1), &delegateTx));
            //StdTrace("CDelegateContext", "AddNewTx: destIn add unspent: [1] %s", txid.GetHex().c_str());
        }
    }
}

bool CDelegateContext::BuildEnrollTx(CTransaction& tx, int nBlockHeight, int64 nTime,
                                     const uint256& hashAnchor, const vector<unsigned char>& vchData)
{
    tx.SetNull();
    tx.nType = CTransaction::TX_CERT;
    tx.nTimeStamp = nTime;
    tx.hashAnchor = hashAnchor;
    tx.sendTo = destDelegate;
    tx.nAmount = 1;
    tx.nTxFee = 0;
    {
        CODataStream os(tx.vchData, sizeof(int) + vchData.size());
        os << nBlockHeight;
        os.Push(&vchData[0], vchData.size());
    }

    int64 nValueIn = 0;
    for (map<CTxOutPoint, CDelegateTx*>::iterator it = mapUnspent.begin(); it != mapUnspent.end(); ++it)
    {
        const CTxOutPoint& txout = (*it).first;
        CDelegateTx* pTx = (*it).second;
        if (pTx->IsLocked(txout.n, nBlockHeight))
        {
            StdTrace("CDelegateContext", "BuildEnrollTx 1: IsLocked, nBlockHeight: %d", nBlockHeight);
            continue;
        }
        if (pTx->GetTxTime() > tx.GetTxTime())
        {
            StdTrace("CDelegateContext", "BuildEnrollTx 1: pTx->GetTxTime: %ld > tx.GetTxTime: %ld", pTx->GetTxTime(), tx.GetTxTime());
            continue;
        }
        if (pTx->nType == CTransaction::TX_CERT && txout.n == 0)
        {
            tx.vInput.push_back(CTxIn(txout));
            nValueIn += (txout.n == 0 ? pTx->nAmount : pTx->nChange);
            StdTrace("CDelegateContext", "BuildEnrollTx 1: add unspent: [%d] %s", txout.n, txout.hash.GetHex().c_str());
            if (tx.vInput.size() >= MAX_TX_INPUT_COUNT)
            {
                break;
            }
        }
    }
    if (nValueIn < tx.nAmount)
    {
        for (map<CTxOutPoint, CDelegateTx*>::iterator it = mapUnspent.begin(); it != mapUnspent.end(); ++it)
        {
            const CTxOutPoint& txout = (*it).first;
            CDelegateTx* pTx = (*it).second;
            if (pTx->IsLocked(txout.n, nBlockHeight))
            {
                StdTrace("CDelegateContext", "BuildEnrollTx 2: IsLocked, nBlockHeight: %d", nBlockHeight);
                continue;
            }
            if (pTx->GetTxTime() > tx.GetTxTime())
            {
                StdTrace("CDelegateContext", "BuildEnrollTx 2: pTx->GetTxTime: %ld > tx.GetTxTime: %ld", pTx->GetTxTime(), tx.GetTxTime());
                continue;
            }
            if (!(pTx->nType == CTransaction::TX_CERT && txout.n == 0))
            {
                tx.vInput.push_back(CTxIn(txout));
                nValueIn += (txout.n == 0 ? pTx->nAmount : pTx->nChange);
                StdTrace("CDelegateContext", "BuildEnrollTx 2: add unspent: [%d] %s", txout.n, txout.hash.GetHex().c_str());
                if (nValueIn >= tx.nAmount || tx.vInput.size() >= MAX_TX_INPUT_COUNT)
                {
                    break;
                }
            }
        }
        if (nValueIn < tx.nAmount)
        {
            StdLog("CDelegateContext", "BuildEnrollTx: nValueIn < 0.000001, nValueIn: %.6f, unspent: %ld", ValueFromToken(nValueIn), mapUnspent.size());
            return false;
        }
    }
    StdTrace("CDelegateContext", "BuildEnrollTx: arrange inputs success, nValueIn: %.6f, unspent.size: %ld, tx.vInput.size: %ld", ValueFromToken(nValueIn), mapUnspent.size(), tx.vInput.size());

    uint256 hash = tx.GetSignatureHash();
    vector<unsigned char> vchDelegateSig;
    if (!keyDelegate.Sign(hash, vchDelegateSig))
    {
        StdLog("CDelegateContext", "BuildEnrollTx: Sign fail");
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
    pBlockChain = nullptr;
    pTxPool = nullptr;
}

CConsensus::~CConsensus()
{
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

    if (!MintConfig()->destMpvss.IsNull() && MintConfig()->keyMpvss != 0)
    {
        crypto::CKey key;
        key.SetSecret(crypto::CCryptoKeyData(MintConfig()->keyMpvss.begin(), MintConfig()->keyMpvss.end()));

        CDelegateContext ctxt(key, MintConfig()->destMpvss);
        mapContext.insert(make_pair(ctxt.GetDestination(), ctxt));

        delegate.AddNewDelegate(ctxt.GetDestination());

        pTxPool->AddDestDelegate(ctxt.GetDestination());

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

    if (!datVoteSave.Initialize(Config()->pathData))
    {
        Error("Failed to initialize vote data");
        return false;
    }

    if (!datVoteSave.Load(delegate))
    {
        Error("Failed to load vote data");
        return false;
    }

    return true;
}

void CConsensus::HandleHalt()
{
    boost::unique_lock<boost::mutex> lock(mutex);

    datVoteSave.Save(delegate);

    delegate.Deinitialize();
    for (map<CDestination, CDelegateContext>::iterator it = mapContext.begin(); it != mapContext.end(); ++it)
    {
        (*it).second.Clear();
    }
}

void CConsensus::PrimaryUpdate(const CBlockChainUpdate& update, const CTxSetChange& change, CDelegateRoutine& routine)
{
    boost::unique_lock<boost::mutex> lock(mutex);

    for (map<CDestination, CDelegateContext>::iterator it = mapContext.begin(); it != mapContext.end(); ++it)
    {
        (*it).second.ChangeTxSet(change);
    }

    for (int i = update.vBlockAddNew.size() - 1; i >= 0; i--)
    {
        uint256 hash = update.vBlockAddNew[i].GetHash();
        int nBlockHeight = CBlock::GetBlockHeightByHash(hash);

        CDelegateEnrolled enrolled;
        if (!pBlockChain->GetBlockDelegateEnrolled(hash, enrolled))
        {
            StdError("CConsensus", "PrimaryUpdate: GetBlockDelegateEnrolled fail, hash: %s", hash.GetHex().c_str());
        }
        else
        {
            delegate::CDelegateEvolveResult result;
            delegate.Evolve(nBlockHeight, enrolled.mapWeight, enrolled.mapEnrollData, result, hash);

            std::map<CDestination, int64> mapDelegateVote;
            int64 nDelegateMinAmount = pBlockChain->GetDelegateMinEnrollAmount(hash);
            bool fGetVote = pBlockChain->GetBlockDelegateVote(hash, mapDelegateVote);
            if (nDelegateMinAmount < 0 || !fGetVote)
            {
                if (nDelegateMinAmount < 0)
                {
                    StdError("CConsensus", "PrimaryUpdate: GetDelegateMinEnrollAmount fail, nDelegateMinAmount: %.6f, hash: %s",
                             ValueFromToken(nDelegateMinAmount), hash.GetHex().c_str());
                }
                if (!fGetVote)
                {
                    StdError("CConsensus", "PrimaryUpdate: GetBlockDelegateVote fail, hash: %s", hash.GetHex().c_str());
                }
            }
            else
            {
                for (map<CDestination, vector<unsigned char>>::iterator it = result.mapEnrollData.begin();
                     it != result.mapEnrollData.end(); ++it)
                {
                    StdTrace("CConsensus", "PrimaryUpdate: destDelegate: %s", CAddress((*it).first).ToString().c_str());
                    map<CDestination, CDelegateContext>::iterator mi = mapContext.find((*it).first);
                    if (mi == mapContext.end())
                    {
                        StdTrace("CConsensus", "PrimaryUpdate: mapContext find fail, destDelegate: %s", CAddress((*it).first).ToString().c_str());
                        continue;
                    }
                    std::map<CDestination, int64>::iterator dt = mapDelegateVote.find((*it).first);
                    if (dt == mapDelegateVote.end())
                    {
                        StdTrace("CConsensus", "PrimaryUpdate: mapDelegateVote find fail, destDelegate: %s", CAddress((*it).first).ToString().c_str());
                        continue;
                    }
                    if (dt->second < nDelegateMinAmount)
                    {
                        StdTrace("CConsensus", "PrimaryUpdate: not enough votes, vote: %.6f, weight ratio: %.6f, destDelegate: %s",
                                 ValueFromToken(dt->second), ValueFromToken(nDelegateMinAmount), CAddress((*it).first).ToString().c_str());
                        continue;
                    }
                    CTransaction tx;
                    if ((*mi).second.BuildEnrollTx(tx, nBlockHeight, GetNetTime(), pCoreProtocol->GetGenesisBlockHash(), (*it).second))
                    {
                        StdTrace("CConsensus", "PrimaryUpdate: BuildEnrollTx success, vote token: %.6f, weight ratio: %.6f, destDelegate: %s",
                                 ValueFromToken(dt->second), ValueFromToken(nDelegateMinAmount), CAddress((*it).first).ToString().c_str());
                        routine.vEnrollTx.push_back(tx);
                    }
                }
            }

            int nDistributeTargetHeight = nBlockHeight + CONSENSUS_DISTRIBUTE_INTERVAL + 1;
            int nPublishTargetHeight = nBlockHeight + 1;

            StdTrace("CConsensus", "result.mapDistributeData size: %llu", result.mapDistributeData.size());
            for (map<CDestination, vector<unsigned char>>::iterator it = result.mapDistributeData.begin();
                 it != result.mapDistributeData.end(); ++it)
            {
                delegate.HandleDistribute(nDistributeTargetHeight, hash, (*it).first, (*it).second);
            }
            routine.vDistributeData.push_back(make_pair(hash, result.mapDistributeData));

            if (i == 0 && result.mapPublishData.size() > 0)
            {
                StdTrace("CConsensus", "result.mapPublishData size: %llu", result.mapPublishData.size());
                for (map<CDestination, vector<unsigned char>>::iterator it = result.mapPublishData.begin();
                     it != result.mapPublishData.end(); ++it)
                {
                    bool fCompleted = false;
                    delegate.HandlePublish(nPublishTargetHeight, result.hashDistributeOfPublish, (*it).first, (*it).second, fCompleted);
                    routine.fPublishCompleted = (routine.fPublishCompleted || fCompleted);
                }
                routine.mapPublishData = result.mapPublishData;
                routine.hashDistributeOfPublish = result.hashDistributeOfPublish;
            }

            routine.vEnrolledWeight.push_back(make_pair(hash, enrolled.mapWeight));
        }
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

bool CConsensus::AddNewDistribute(const uint256& hashDistributeAnchor, const CDestination& destFrom, const vector<unsigned char>& vchDistribute)
{
    boost::unique_lock<boost::mutex> lock(mutex);
    int nTargetHeight = CBlock::GetBlockHeightByHash(hashDistributeAnchor) + CONSENSUS_DISTRIBUTE_INTERVAL + 1;
    return delegate.HandleDistribute(nTargetHeight, hashDistributeAnchor, destFrom, vchDistribute);
}

bool CConsensus::AddNewPublish(const uint256& hashDistributeAnchor, const CDestination& destFrom, const vector<unsigned char>& vchPublish)
{
    boost::unique_lock<boost::mutex> lock(mutex);
    int nTargetHeight = CBlock::GetBlockHeightByHash(hashDistributeAnchor) + CONSENSUS_DISTRIBUTE_INTERVAL + 1;
    bool fCompleted = false;
    return delegate.HandlePublish(nTargetHeight, hashDistributeAnchor, destFrom, vchPublish, fCompleted);
}

void CConsensus::GetAgreement(int nTargetHeight, uint256& nAgreement, size_t& nWeight, vector<CDestination>& vBallot)
{
    if (nTargetHeight >= CONSENSUS_INTERVAL && pCoreProtocol->IsDposHeight(nTargetHeight))
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        uint256 hashBlock;
        if (!pBlockChain->GetBlockHash(pCoreProtocol->GetGenesisBlockHash(), nTargetHeight - CONSENSUS_DISTRIBUTE_INTERVAL - 1, hashBlock))
        {
            Error("GetAgreement CBlockChain::GetBlockHash error, distribution height: %d", nTargetHeight - CONSENSUS_DISTRIBUTE_INTERVAL - 1);
            return;
        }

        map<CDestination, size_t> mapBallot;
        delegate.GetAgreement(nTargetHeight, hashBlock, nAgreement, nWeight, mapBallot);

        if (nAgreement != 0 && mapBallot.size() > 0)
        {
            CDelegateEnrolled enrolled;
            if (!pBlockChain->GetBlockDelegateEnrolled(hashBlock, enrolled))
            {
                Error("GetAgreement CBlockChain::GetBlockDelegateEnrolled error, hash: %s", hashBlock.ToString().c_str());
                return;
            }
            int64 nMoneySupply = pBlockChain->GetBlockMoneySupply(hashBlock);
            if (nMoneySupply < 0)
            {
                Error("GetAgreement GetBlockMoneySupply fail, hash: %s", hashBlock.ToString().c_str());
                return;
            }

            size_t nEnrollTrust = 0;
            pCoreProtocol->GetDelegatedBallot(nAgreement, nWeight, mapBallot, enrolled.vecAmount, nMoneySupply, vBallot, nEnrollTrust, nTargetHeight);
        }
    }
}

void CConsensus::GetProof(int nTargetHeight, vector<unsigned char>& vchProof)
{
    boost::unique_lock<boost::mutex> lock(mutex);
    delegate.GetProof(nTargetHeight, vchProof);
}

bool CConsensus::GetNextConsensus(CAgreementBlock& consParam)
{
    boost::unique_lock<boost::mutex> lock(mutex);

    consParam.nWaitTime = 1;
    consParam.ret = false;

    uint256 hashLastBlock;
    int nLastHeight;
    int64 nLastTime;
    uint16 nLastMintType;
    if (!pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashLastBlock, nLastHeight, nLastTime, nLastMintType))
    {
        Error("GetNextConsensus CBlockChain::GetLastBlock fail");
        return false;
    }
    consParam.hashPrev = hashLastBlock;
    consParam.nPrevTime = nLastTime;
    consParam.nPrevHeight = nLastHeight;
    consParam.nPrevMintType = nLastMintType;
    consParam.agreement.Clear();

    if (!pCoreProtocol->IsDposHeight(nLastHeight + 1))
    {
        consParam.nWaitTime = 0;
        consParam.fCompleted = true;
        consParam.ret = true;
        return true;
    }

    int64 nNextBlockTime = pCoreProtocol->GetNextBlockTimeStamp(nLastMintType, nLastTime, CTransaction::TX_WORK, nLastHeight + 1);
    consParam.nWaitTime = nNextBlockTime - 2 - GetNetTime();
    if (consParam.nWaitTime >= -60)
    {
        int64 nAgreementWaitTime = GetAgreementWaitTime(nLastHeight + 1);
        if (nAgreementWaitTime > 0 && consParam.nWaitTime < nAgreementWaitTime)
        {
            consParam.nWaitTime = nAgreementWaitTime;
        }
    }
    if (consParam.nWaitTime > 0)
    {
        return false;
    }

    if (hashLastBlock != cacheAgreementBlock.hashPrev)
    {
        if (!GetInnerAgreement(nLastHeight + 1, consParam.agreement.nAgreement, consParam.agreement.nWeight,
                               consParam.agreement.vBallot, consParam.fCompleted))
        {
            Error("GetNextConsensus GetInnerAgreement fail");
            return false;
        }
        cacheAgreementBlock = consParam;
    }
    else
    {
        if (cacheAgreementBlock.agreement.IsProofOfWork() && !cacheAgreementBlock.fCompleted)
        {
            if (!GetInnerAgreement(nLastHeight + 1, cacheAgreementBlock.agreement.nAgreement, cacheAgreementBlock.agreement.nWeight,
                                   cacheAgreementBlock.agreement.vBallot, cacheAgreementBlock.fCompleted))
            {
                Error("GetNextConsensus GetInnerAgreement fail");
                return false;
            }
            if (!cacheAgreementBlock.agreement.IsProofOfWork())
            {
                StdDebug("CConsensus", "GetNextConsensus: consensus change dpos, target height: %d", cacheAgreementBlock.nPrevHeight + 1);
            }
        }
        consParam = cacheAgreementBlock;
        consParam.nWaitTime = nNextBlockTime - 2 - GetNetTime();
    }
    if (!cacheAgreementBlock.agreement.IsProofOfWork())
    {
        nNextBlockTime = pCoreProtocol->GetNextBlockTimeStamp(nLastMintType, nLastTime, CTransaction::TX_STAKE, nLastHeight + 1);
        consParam.nWaitTime = nNextBlockTime - 2 - GetNetTime();
        if (consParam.nWaitTime > 0)
        {
            return false;
        }
    }
    consParam.ret = true;
    return true;
}

bool CConsensus::LoadConsensusData(int& nStartHeight, CDelegateRoutine& routine)
{
    int nLashBlockHeight = pBlockChain->GetBlockCount(pCoreProtocol->GetGenesisBlockHash()) - 1;
    nStartHeight = nLashBlockHeight - CONSENSUS_DISTRIBUTE_INTERVAL;
    if (nStartHeight < 1)
    {
        nStartHeight = 1;
    }

    for (int i = nStartHeight; i <= nLashBlockHeight; i++)
    {
        uint256 hashBlock;
        if (!pBlockChain->GetBlockHash(pCoreProtocol->GetGenesisBlockHash(), i, hashBlock))
        {
            StdError("CConsensus", "LoadConsensusData: GetBlockHash fail, height: %d", i);
            return false;
        }

        CDelegateEnrolled enrolled;
        if (!pBlockChain->GetBlockDelegateEnrolled(hashBlock, enrolled))
        {
            StdError("CConsensus", "LoadConsensusData: GetBlockDelegateEnrolled fail, height: %d, block: %s", i, hashBlock.GetHex().c_str());
            return false;
        }

        delegate::CDelegateEvolveResult result;
        delegate.GetEvolveData(i, result, hashBlock);

        routine.vDistributeData.push_back(make_pair(hashBlock, result.mapDistributeData));
        if (i == nLashBlockHeight && result.mapPublishData.size() > 0)
        {
            routine.mapPublishData = result.mapPublishData;
            routine.hashDistributeOfPublish = result.hashDistributeOfPublish;
        }

        routine.vEnrolledWeight.push_back(make_pair(hashBlock, enrolled.mapWeight));
    }

    return true;
}

bool CConsensus::LoadDelegateTx()
{
    const uint256 hashGenesis = pCoreProtocol->GetGenesisBlockHash();
    for (map<CDestination, CDelegateContext>::iterator it = mapContext.begin(); it != mapContext.end(); ++it)
    {
        CDelegateTxFilter txFilter((*it).second);
        if (!pBlockChain->FilterTx(hashGenesis, txFilter) || !pTxPool->FilterTx(hashGenesis, txFilter))
        {
            return false;
        }
    }
    return true;
}

bool CConsensus::GetInnerAgreement(int nTargetHeight, uint256& nAgreement, size_t& nWeight, vector<CDestination>& vBallot, bool& fCompleted)
{
    if (nTargetHeight >= CONSENSUS_INTERVAL && pCoreProtocol->IsDposHeight(nTargetHeight))
    {
        uint256 hashBlock;
        if (!pBlockChain->GetBlockHash(pCoreProtocol->GetGenesisBlockHash(), nTargetHeight - CONSENSUS_DISTRIBUTE_INTERVAL - 1, hashBlock))
        {
            Error("GetAgreement CBlockChain::GetBlockHash error, distribution height: %d", nTargetHeight - CONSENSUS_DISTRIBUTE_INTERVAL - 1);
            return false;
        }

        fCompleted = delegate.IsCompleted(nTargetHeight);

        map<CDestination, size_t> mapBallot;
        delegate.GetAgreement(nTargetHeight, hashBlock, nAgreement, nWeight, mapBallot);

        if (nAgreement != 0 && mapBallot.size() > 0)
        {
            CDelegateEnrolled enrolled;
            if (!pBlockChain->GetBlockDelegateEnrolled(hashBlock, enrolled))
            {
                Error("GetAgreement CBlockChain::GetBlockDelegateEnrolled error, hash: %s", hashBlock.ToString().c_str());
                return false;
            }
            int64 nMoneySupply = pBlockChain->GetBlockMoneySupply(hashBlock);
            if (nMoneySupply < 0)
            {
                Error("GetAgreement GetBlockMoneySupply fail, hash: %s", hashBlock.ToString().c_str());
                return false;
            }
            size_t nEnrollTrust = 0;
            pCoreProtocol->GetDelegatedBallot(nAgreement, nWeight, mapBallot, enrolled.vecAmount, nMoneySupply, vBallot, nEnrollTrust, nTargetHeight);
        }
    }
    else
    {
        fCompleted = true;
    }
    return true;
}

int64 CConsensus::GetAgreementWaitTime(int nTargetHeight)
{
    if (nTargetHeight >= CONSENSUS_INTERVAL && pCoreProtocol->IsDposHeight(nTargetHeight))
    {
        int64 nPublishedTime = delegate.GetPublishedTime(nTargetHeight);
        if (nPublishedTime <= 0)
        {
            return -1;
        }
        return nPublishedTime + WAIT_AGREEMENT_PUBLISH_TIMEOUT - GetTime();
    }
    return 0;
}

} // namespace bigbang
