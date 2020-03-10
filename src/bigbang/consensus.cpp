// Copyright (c) 2019-2020 The Bigbang developers
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
        StdTrace("CDelegateContext", "AddNewTx: sendto and unspent: [0] %s", txid.GetHex().c_str());
    }
    if (tx.destIn == destDelegate)
    {
        for (const CTxIn& txin : tx.vInput)
        {
            StdTrace("CDelegateContext", "AddNewTx: destIn erase unspent: [%d] %s", txin.prevout.n, txin.prevout.hash.GetHex().c_str());
            mapUnspent.erase(txin.prevout);
        }
        uint256 txid = tx.GetHash();
        CDelegateTx& delegateTx = mapTx[txid];
        if (!fAddTx)
            delegateTx = CDelegateTx(tx);
        if (delegateTx.nChange != 0)
        {
            mapUnspent.insert(make_pair(CTxOutPoint(txid, 1), &delegateTx));
            StdTrace("CDelegateContext", "AddNewTx: destIn add unspent: [1] %s", txid.GetHex().c_str());
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
    //tx.vchData = vchData;
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
            StdTrace("CDelegateContext", "BuildEnrollTx: IsLocked, nBlockHeight: %d", nBlockHeight);
            continue;
        }
        if (pTx->GetTxTime() > tx.GetTxTime())
        {
            StdTrace("CDelegateContext", "BuildEnrollTx: pTx->GetTxTime: %ld > tx.GetTxTime: %ld", pTx->GetTxTime(), tx.GetTxTime());
            continue;
        }
        tx.vInput.push_back(CTxIn(txout));
        nValueIn += (txout.n == 0 ? pTx->nAmount : pTx->nChange);
        /*if (nValueIn > nTxFee)
        {
            break;
        }*/
        StdTrace("CDelegateContext", "BuildEnrollTx: add unspent: [%d] %s", txout.n, txout.hash.GetHex().c_str());
        if (tx.vInput.size() >= MAX_TX_INPUT_COUNT)
        {
            break;
        }
    }
    if (nValueIn <= nTxFee)
    {
        StdLog("CDelegateContext", "BuildEnrollTx: nValueIn <= nTxFee, nValueIn: %.6f, unspent: %ld", ValueFromToken(nValueIn), mapUnspent.size());
        return false;
    }
    StdTrace("CDelegateContext", "BuildEnrollTx: arrange inputs success, nValueIn: %.6f, unspent.size: %ld, tx.vInput.size: %ld", ValueFromToken(nValueIn), mapUnspent.size(), tx.vInput.size());

    tx.nAmount = nValueIn - nTxFee;

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
    for (map<CDestination, CDelegateContext>::iterator it = mapContext.begin(); it != mapContext.end(); ++it)
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

    for (map<CDestination, CDelegateContext>::iterator it = mapContext.begin(); it != mapContext.end(); ++it)
    {
        (*it).second.ChangeTxSet(change);
    }

    /*for (int i = update.vBlockAddNew.size() - 1; i > 0; i--)
    {
        uint256 hash = update.vBlockAddNew[i].GetHash();
        int nBlockHeight = CBlock::GetBlockHeightByHash(hash);

        CDelegateEnrolled enrolled;

        if (pBlockChain->GetBlockDelegateEnrolled(hash, enrolled))
        {
            delegate::CDelegateEvolveResult result;
            delegate.Evolve(nBlockHeight, enrolled.mapWeight, enrolled.mapEnrollData, result, hash);
        }

        routine.vEnrolledWeight.push_back(make_pair(hash, enrolled.mapWeight));
    }*/

    //if (!update.vBlockAddNew.empty())
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
            int64 nDelegateWeightRatio = pBlockChain->GetDelegateWeightRatio(hash);
            bool fGetVote = pBlockChain->GetBlockDelegateVote(hash, mapDelegateVote);
            if (nDelegateWeightRatio < 0 || !fGetVote)
            {
                if (nDelegateWeightRatio < 0)
                {
                    StdError("CConsensus", "PrimaryUpdate: GetDelegateWeightRatio fail, nDelegateWeightRatio: %.6f, hash: %s",
                             ValueFromToken(nDelegateWeightRatio), hash.GetHex().c_str());
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
                    if (dt->second < nDelegateWeightRatio)
                    {
                        StdTrace("CConsensus", "PrimaryUpdate: not enough votes, vote: %.6f, weight ratio: %.6f, destDelegate: %s",
                                 ValueFromToken(dt->second), ValueFromToken(nDelegateWeightRatio), CAddress((*it).first).ToString().c_str());
                        continue;
                    }
                    CTransaction tx;
                    if ((*mi).second.BuildEnrollTx(tx, nBlockHeight, GetNetTime(), pCoreProtocol->GetGenesisBlockHash(), 0, (*it).second))
                    {
                        StdTrace("CConsensus", "PrimaryUpdate: BuildEnrollTx success, vote token: %.6f, weight ratio: %.6f, destDelegate: %s",
                                 ValueFromToken(dt->second), ValueFromToken(nDelegateWeightRatio), CAddress((*it).first).ToString().c_str());
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

            if (i == 0)
            {
                StdTrace("CConsensus", "result.mapPublishData size: %llu", result.mapPublishData.size());
                for (map<CDestination, vector<unsigned char>>::iterator it = result.mapPublishData.begin();
                     it != result.mapPublishData.end(); ++it)
                {
                    bool fCompleted = false;
                    delegate.HandlePublish(nPublishTargetHeight, hash, (*it).first, (*it).second, fCompleted);
                    routine.fPublishCompleted = (routine.fPublishCompleted || fCompleted);
                }
                routine.mapPublishData = result.mapPublishData;
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

bool CConsensus::AddNewDistribute(int nAnchorHeight, const uint256& hashDistributeAnchor, const CDestination& destFrom, const vector<unsigned char>& vchDistribute)
{
    boost::unique_lock<boost::mutex> lock(mutex);
    int nDistributeTargetHeight = nAnchorHeight + CONSENSUS_DISTRIBUTE_INTERVAL + 1;
    return delegate.HandleDistribute(nDistributeTargetHeight, hashDistributeAnchor, destFrom, vchDistribute);
}

bool CConsensus::AddNewPublish(int nAnchorHeight, const uint256& hashPublishAnchor, const CDestination& destFrom, const vector<unsigned char>& vchPublish)
{
    boost::unique_lock<boost::mutex> lock(mutex);
    int nPublishTargetHeight = nAnchorHeight + 1;
    bool fCompleted = false;
    return delegate.HandlePublish(nPublishTargetHeight, hashPublishAnchor, destFrom, vchPublish, fCompleted);
}

// 获得目标高度的共识结果(nAgreement nWeight vBallot)
void CConsensus::GetAgreement(int nTargetHeight, uint256& nAgreement, size_t& nWeight, vector<CDestination>& vBallot)
{
    if (nTargetHeight >= CONSENSUS_INTERVAL)
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        uint256 hashBlock;
        // 获得Enrolled完成以后的高度Block的Hash（同时也是Distribute阶段开始的Block hash），是为了拿Enroll结果
        if (!pBlockChain->GetBlockHash(pCoreProtocol->GetGenesisBlockHash(), nTargetHeight - CONSENSUS_DISTRIBUTE_INTERVAL - 1, hashBlock))
        {
            Error("GetAgreement CBlockChain::GetBlockHash error, distribution height: %d", nTargetHeight - CONSENSUS_DISTRIBUTE_INTERVAL - 1);
            return;
        }
        CDelegateEnrolled enrolled;
        // 通过Enroll完毕的Block hash拿到Enroll结果(各个Delegate模板地址上的权重和金额)
        if (!pBlockChain->GetBlockDelegateEnrolled(hashBlock, enrolled))
        {
            Error("GetAgreement CBlockChain::GetBlockDelegateEnrolled error, hash: %s", hashBlock.ToString().c_str());
            return;
        }
        // 得到Enrolled完毕后Mint奖励的累加值
        int64 nMoneySupply = pBlockChain->GetBlockMoneySupply(hashBlock);
        if (nMoneySupply < 0)
        {
            Error("GetAgreement GetBlockMoneySupply fail, hash: %s", hashBlock.ToString().c_str());
            return;
        }

        map<CDestination, size_t> mapBallot;
        // 拿到目标高度的共识结果(nWeight是各个Delegate模板地址的权重累加，mapBallot是各个模板地址对应的权重)
        delegate.GetAgreement(nTargetHeight, hashBlock, nAgreement, nWeight, mapBallot);
        // 主要是通过共识结果拿到vBallot，即一个共识后的Delegate地址列表，列表的第一个地址就是出TargetHeight高度块的Delegate地址
        pCoreProtocol->GetDelegatedBallot(nAgreement, nWeight, mapBallot, enrolled.vecAmount, nMoneySupply, vBallot, nTargetHeight);
    }
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
