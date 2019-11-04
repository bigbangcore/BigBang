// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dispatcher.h"

#include <chrono>
#include <thread>

#include "event.h"

using namespace std;
using namespace xengine;

namespace bigbang
{

//////////////////////////////
// CDispatcher

CDispatcher::CDispatcher()
{
    pCoreProtocol = nullptr;
    pWorldLineCtrl = nullptr;
    pTxPoolCtrl = nullptr;
    pForkManager = nullptr;
    pConsensus = nullptr;
    pWallet = nullptr;
    pService = nullptr;
    pBlockMaker = nullptr;
    pNetChannelModel = nullptr;
    pDelegatedChannel = nullptr;
    pDataStat = nullptr;
}

CDispatcher::~CDispatcher()
{
}

bool CDispatcher::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        ERROR("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("worldlinecontroller", pWorldLineCtrl))
    {
        ERROR("Failed to request worldline");
        return false;
    }

    if (!GetObject("txpoolcontroller", pTxPoolCtrl))
    {
        ERROR("Failed to request txpool");
        return false;
    }

    if (!GetObject("forkmanager", pForkManager))
    {
        ERROR("Failed to request forkmanager");
        return false;
    }

    if (!GetObject("consensus", pConsensus))
    {
        ERROR("Failed to request consensus");
        return false;
    }

    if (!GetObject("wallet", pWallet))
    {
        ERROR("Failed to request wallet");
        return false;
    }

    if (!GetObject("service", pService))
    {
        ERROR("Failed to request service");
        return false;
    }

    if (!GetObject("blockmaker", pBlockMaker))
    {
        ERROR("Failed to request blockmaker");
        return false;
    }

    if (!GetObject("netchannelmodel", pNetChannelModel))
    {
        ERROR("Failed to request netchannel model");
        return false;
    }

    if (!GetObject("delegatedchannel", pDelegatedChannel))
    {
        ERROR("Failed to request delegatedchannel");
        return false;
    }

    if (!GetObject("datastat", pDataStat))
    {
        ERROR("Failed to request datastat");
        return false;
    }

    return true;
}

void CDispatcher::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pWorldLineCtrl = nullptr;
    pTxPoolCtrl = nullptr;
    pForkManager = nullptr;
    pConsensus = nullptr;
    pWallet = nullptr;
    pService = nullptr;
    pBlockMaker = nullptr;
    pNetChannelModel = nullptr;
    pDelegatedChannel = nullptr;
    pDataStat = nullptr;
}

bool CDispatcher::HandleInvoke()
{
    vector<uint256> vActive;
    if (!pForkManager->LoadForkContext(vActive))
    {
        ERROR("Failed to load for context");
        return false;
    }

    for (const uint256& hashFork : vActive)
    {
        ActivateFork(hashFork, 0);
    }

    return true;
}

void CDispatcher::HandleHalt()
{
}

Errno CDispatcher::AddNewBlock(const CBlock& block, uint64 nNonce)
{
    Errno err = OK;
    if (!pWorldLineCtrl->Exists(block.hashPrev))
    {
        return ERR_MISSING_PREV;
    }

    CWorldLineUpdate updateWorldLine;
    if (!block.IsOrigin())
    {
        err = pWorldLineCtrl->AddNewBlock(block, updateWorldLine, nNonce);
        if (err == OK && !block.IsVacant())
        {
            if (!nNonce)
            {
                pDataStat->AddBlockMakerStatData(updateWorldLine.hashFork, block.IsProofOfWork(), block.vtx.size());
            }
            else
            {
                pDataStat->AddP2pSynRecvStatData(updateWorldLine.hashFork, 1, block.vtx.size());
            }
        }
    }
    else
    {
        err = pWorldLineCtrl->AddNewOrigin(block, updateWorldLine, nNonce);
    }

    if (err != OK || updateWorldLine.IsNull())
    {
        return err;
    }

    CTxSetChange changeTxSet;
    if (!pTxPoolCtrl->SynchronizeWorldLine(updateWorldLine, changeTxSet))
    {
        return ERR_SYS_DATABASE_ERROR;
    }

    if (block.IsOrigin())
    {
        // if (!pWallet->AddNewFork(updateWorldLine.hashFork, updateWorldLine.hashParent,
        //                          updateWorldLine.nOriginHeight))
        // {
        //     return ERR_SYS_DATABASE_ERROR;
        // }
    }

    // if (!pWallet->SynchronizeTxSet(changeTxSet))
    // {
    //     return ERR_SYS_DATABASE_ERROR;
    // }

    if (!block.IsOrigin() && !block.IsVacant())
    {
        auto spBroadcastBlockInvMsg = CBroadcastBlockInvMessage::Create(updateWorldLine.hashFork, block.GetHash());
        PUBLISH_MESSAGE(spBroadcastBlockInvMsg);
        pDataStat->AddP2pSynSendStatData(updateWorldLine.hashFork, 1, block.vtx.size());
    }

    // pService->NotifyWorldLineUpdate(updateWorldLine);

    if (!block.IsVacant())
    {
        vector<uint256> vActive, vDeactive;
        pForkManager->ForkUpdate(updateWorldLine, vActive, vDeactive);

        for (const uint256 hashFork : vActive)
        {
            ActivateFork(hashFork, nNonce);
        }

        for (const uint256 hashFork : vDeactive)
        {
            auto spUnsubscribeForkMsg = CUnsubscribeForkMessage::Create(hashFork);
            PUBLISH_MESSAGE(spUnsubscribeForkMsg);
        }
    }

    if (block.IsPrimary())
    {
        UpdatePrimaryBlock(block, updateWorldLine, changeTxSet, nNonce);
    }

    return OK;
}

Errno CDispatcher::AddNewTx(const CTransaction& tx, uint64 nNonce)
{
    Errno err = OK;
    err = pCoreProtocol->ValidateTransaction(tx);
    if (err != OK)
    {
        return err;
    }

    uint256 hashFork;
    CDestination destIn;
    int64 nValueIn;
    err = pTxPoolCtrl->Push(tx, hashFork, destIn, nValueIn);
    if (err != OK)
    {
        return err;
    }

    pDataStat->AddP2pSynTxSynStatData(hashFork, !!nNonce);

    CAssembledTx assembledTx(tx, -1, destIn, nValueIn);
    // if (!pWallet->AddNewTx(hashFork, assembledTx))
    // {
    //     return ERR_SYS_DATABASE_ERROR;
    // }

    // CTransactionUpdate updateTransaction;
    // updateTransaction.hashFork = hashFork;
    // updateTransaction.txUpdate = tx;
    // updateTransaction.nChange = assembledTx.GetChange();
    // pService->NotifyTransactionUpdate(updateTransaction);

    if (!nNonce)
    {
        auto spBroadcastTxInvMsg = CBroadcastTxInvMessage::Create(hashFork);
        PUBLISH_MESSAGE(spBroadcastTxInvMsg);
    }

    if (hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        pConsensus->AddNewTx(CAssembledTx(tx, -1, destIn, nValueIn));
    }

    return OK;
}

bool CDispatcher::AddNewDistribute(const uint256& hashAnchor, const CDestination& dest, const vector<unsigned char>& vchDistribute)
{
    uint256 hashFork;
    int nHeight;
    if (pWorldLineCtrl->GetBlockLocation(hashAnchor, hashFork, nHeight) && hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        return pConsensus->AddNewDistribute(nHeight, dest, vchDistribute);
    }
    return false;
}

bool CDispatcher::AddNewPublish(const uint256& hashAnchor, const CDestination& dest, const vector<unsigned char>& vchPublish)
{
    uint256 hashFork;
    int nHeight;
    if (pWorldLineCtrl->GetBlockLocation(hashAnchor, hashFork, nHeight) && hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        return pConsensus->AddNewPublish(nHeight, dest, vchPublish);
    }
    return false;
}

void CDispatcher::UpdatePrimaryBlock(const CBlock& block, const CWorldLineUpdate& updateWorldLine, const CTxSetChange& changeTxSet, const uint64& nNonce)
{
    CDelegateRoutine routineDelegate;

    if (!pCoreProtocol->CheckFirstPow(updateWorldLine.nLastBlockHeight))
    {
        pConsensus->PrimaryUpdate(updateWorldLine, changeTxSet, routineDelegate);

        pDelegatedChannel->PrimaryUpdate(updateWorldLine.nLastBlockHeight - updateWorldLine.vBlockAddNew.size(),
                                         routineDelegate.vEnrolledWeight, routineDelegate.mapDistributeData, routineDelegate.mapPublishData);

        for (const CTransaction& tx : routineDelegate.vEnrollTx)
        {
            Errno err = AddNewTx(tx, nNonce);
            INFO("Send DelegateTx %s (%s)", ErrorString(err), tx.GetHash().GetHex().c_str());
        }
    }

    SyncForkHeight(updateWorldLine.nLastBlockHeight);
}

void CDispatcher::ActivateFork(const uint256& hashFork, const uint64& nNonce)
{
    INFO("Activating fork %s ..", hashFork.GetHex().c_str());
    if (!pWorldLineCtrl->Exists(hashFork))
    {
        CForkContext ctxt;
        if (!pWorldLineCtrl->GetForkContext(hashFork, ctxt))
        {
            WARN("Failed to find fork context %s", hashFork.GetHex().c_str());
            return;
        }

        CTransaction txFork;
        if (!pWorldLineCtrl->GetTransaction(ctxt.txidEmbedded, txFork))
        {
            WARN("Failed to find tx fork %s", hashFork.GetHex().c_str());
            return;
        }

        if (!ProcessForkTx(ctxt.txidEmbedded, txFork))
        {
            return;
        }
        INFO("Add origin block in tx (%s), hash=%s", ctxt.txidEmbedded.GetHex().c_str(),
             hashFork.GetHex().c_str());
    }

    auto spSubscribeForkMsg = CSubscribeForkMessage::Create(hashFork, nNonce);
    PUBLISH_MESSAGE(spSubscribeForkMsg);

    INFO("Activated fork %s ..", hashFork.GetHex().c_str());
}

bool CDispatcher::ProcessForkTx(const uint256& txid, const CTransaction& tx)
{
    CBlock block;
    try
    {
        CBufStream ss;
        ss.Write((const char*)&tx.vchData[0], tx.vchData.size());
        ss >> block;
        if (!block.IsOrigin() || block.IsPrimary())
        {
            throw std::runtime_error("invalid block");
        }
    }
    catch (...)
    {
        WARN("Invalid orign block found in tx (%s)", txid.GetHex().c_str());
        return false;
    }

    Errno err = AddNewBlock(block);
    if (err != OK)
    {
        INFO("Add origin block in tx (%s) failed : %s", txid.GetHex().c_str(),
             ErrorString(err));
        return false;
    }
    return true;
}

void CDispatcher::SyncForkHeight(int nPrimaryHeight)
{
    map<uint256, CForkStatus> mapForkStatus;
    pWorldLineCtrl->GetForkStatus(mapForkStatus);
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
            && pWorldLineCtrl->GetLastBlockTime(pCoreProtocol->GetGenesisBlockHash(), nDepth, vTimeStamp))
        {
            uint256 hashPrev = status.hashLastBlock;
            for (int nHeight = status.nLastBlockHeight + 1; nHeight < nPrimaryHeight; nHeight++)
            {
                CBlock block;
                block.nType = CBlock::BLOCK_VACANT;
                block.hashPrev = hashPrev;
                block.nTimeStamp = vTimeStamp[nPrimaryHeight - nHeight];
                if (AddNewBlock(block) != OK)
                {
                    break;
                }
                hashPrev = block.GetHash();
            }
        }
    }
}

} // namespace bigbang
