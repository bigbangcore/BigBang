// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txpoolcontroller.h"

using namespace std;

namespace bigbang
{

//////////////////////////////
// CTxPoolController

CTxPoolController::CTxPoolController()
{
}

CTxPoolController::~CTxPoolController()
{
}

bool CTxPoolController::HandleInitialize()
{
    if (!ITxPoolController::HandleInitialize())
    {
        return false;
    }

    if (!GetObject("txpool", pTxPool))
    {
        ERROR("Failed to request txpool");
        return false;
    }

    // TODO: Uncomment it when upgrade CDispatcher
    RegisterHandler({
        PTR_HANDLER(CAddTxMessage, boost::bind(&CTxPoolController::HandleAddTx, this, _1), true),
        PTR_HANDLER(CRemoveTxMessage, boost::bind(&CTxPoolController::HandleRemoveTx, this, _1), true),
        PTR_HANDLER(CClearTxMessage, boost::bind(&CTxPoolController::HandleClearTx, this, _1), true),
        PTR_HANDLER(CAddedBlockMessage, boost::bind(&CTxPoolController::HandleAddedBlock, this, _1), true),
    });

    return true;
}

void CTxPoolController::HandleDeinitialize()
{
    // TODO: Uncomment it when upgrade CDispatcher
    DeregisterHandler();

    pTxPool = nullptr;

    ITxPoolController::HandleDeinitialize();
}

bool CTxPoolController::HandleInvoke()
{
    if (!StartActor())
    {
        ERROR("Failed to start actor");
        return false;
    }

    return true;
}

void CTxPoolController::HandleHalt()
{
    StopActor();

    ClearTxPool();
}

bool CTxPoolController::Exists(const uint256& txid)
{
    return pTxPool->Exists(txid);
}

void CTxPoolController::Clear()
{
    ClearTxPool();
}

size_t CTxPoolController::Count(const uint256& fork) const
{
    return pTxPool->Count(fork);
}

Errno CTxPoolController::Push(const CTransaction& tx, uint256& hashFork, CDestination& destIn, int64& nValueIn)
{
    return PushIntoTxPool(tx, hashFork, destIn, nValueIn);
}

void CTxPoolController::Pop(const uint256& txid)
{
    return PopFromTxPool(txid);
}

bool CTxPoolController::Get(const uint256& txid, CTransaction& tx) const
{
    return pTxPool->Get(txid, tx);
}

void CTxPoolController::ListTx(const uint256& hashFork, vector<pair<uint256, size_t>>& vTxPool)
{
    pTxPool->ListTx(hashFork, vTxPool);
}

void CTxPoolController::ListTx(const uint256& hashFork, vector<uint256>& vTxPool)
{
    pTxPool->ListTx(hashFork, vTxPool);
}

bool CTxPoolController::FilterTx(const uint256& hashFork, CTxFilter& filter)
{
    return pTxPool->FilterTx(hashFork, filter);
}

void CTxPoolController::ArrangeBlockTx(const uint256& hashFork, int64 nBlockTime, size_t nMaxSize,
                                       vector<CTransaction>& vtx, int64& nTotalTxFee)
{
    pTxPool->ArrangeBlockTx(hashFork, nBlockTime, nMaxSize, vtx, nTotalTxFee);
}

bool CTxPoolController::FetchInputs(const uint256& hashFork, const CTransaction& tx, vector<CTxOut>& vUnspent)
{
    return pTxPool->FetchInputs(hashFork, tx, vUnspent);
}

bool CTxPoolController::SynchronizeWorldLine(const CWorldLineUpdate& update, CTxSetChange& change)
{
    return SynchronizeWorldLineWithTxPool(update, change);
}

void CTxPoolController::HandleAddTx(const shared_ptr<CAddTxMessage> spMsg)
{
    auto spAddedMsg = CAddedTxMessage::Create();
    spAddedMsg->spNonce = spMsg->spNonce;
    Push(spMsg->tx, spAddedMsg->hashFork, spAddedMsg->destIn, spAddedMsg->nValueIn);
    spAddedMsg->tx = CAssembledTx(spMsg->tx, -1, spAddedMsg->destIn, spAddedMsg->nValueIn);
    PUBLISH(spAddedMsg);
}

void CTxPoolController::HandleRemoveTx(const shared_ptr<CRemoveTxMessage> spMsg)
{
    auto& txId = spMsg->txId;
    Pop(txId);
}

void CTxPoolController::HandleClearTx(const shared_ptr<CClearTxMessage> spMsg)
{
    if (spMsg->hashFork == 0)
    {
        ClearTxPool();
    }
    else
    {
        // TODO: remove one fork tx;
    }
}

void CTxPoolController::HandleAddedBlock(const shared_ptr<CAddedBlockMessage> spMsg)
{
    if (spMsg->nErrno == OK && !spMsg->update.IsNull())
    {
        auto spSyncMsg = CSyncTxChangeMessage::Create();
        spSyncMsg->hashFork = spMsg->hashFork;
        spSyncMsg->update = spMsg->update;
        SynchronizeWorldLineWithTxPool(spMsg->update, spSyncMsg->change);
        PUBLISH(spSyncMsg);
    }
}

} // namespace bigbang
