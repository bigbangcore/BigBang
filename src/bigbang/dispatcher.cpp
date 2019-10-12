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
    pWorldLineCntrl = nullptr;
    pTxPoolCntrl = nullptr;
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

    if (!GetObject("forkmanager", pForkManager))
    {
        Error("Failed to request forkmanager\n");
        return false;
    }

    if (!GetObject("consensus", pConsensus))
    {
        Error("Failed to request consensus\n");
        return false;
    }

    if (!GetObject("wallet", pWallet))
    {
        Error("Failed to request wallet\n");
        return false;
    }

    if (!GetObject("service", pService))
    {
        Error("Failed to request service\n");
        return false;
    }

    if (!GetObject("blockmaker", pBlockMaker))
    {
        Error("Failed to request blockmaker\n");
        return false;
    }

    if (!GetObject("netchannelmodel", pNetChannelModel))
    {
        Error("Failed to request netchannel model\n");
        return false;
    }

    if (!GetObject("delegatedchannel", pDelegatedChannel))
    {
        Error("Failed to request delegatedchannel\n");
        return false;
    }

    if (!GetObject("datastat", pDataStat))
    {
        Error("Failed to request datastat\n");
        return false;
    }

    return true;
}

void CDispatcher::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pWorldLineCntrl = nullptr;
    pTxPoolCntrl = nullptr;
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
        Error("Failed to load for context\n");
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
    if (!pWorldLineCntrl->Exists(block.hashPrev))
    {
        return ERR_MISSING_PREV;
    }

    CWorldLineUpdate updateWorldLine;
    if (!block.IsOrigin())
    {
        err = pWorldLineCntrl->AddNewBlock(block, updateWorldLine);
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
        err = pWorldLineCntrl->AddNewOrigin(block, updateWorldLine);
    }

    if (err != OK || updateWorldLine.IsNull())
    {
        return err;
    }

    CTxSetChange changeTxSet;
    if (!pTxPoolCntrl->SynchronizeWorldLine(updateWorldLine, changeTxSet))
    {
        return ERR_SYS_DATABASE_ERROR;
    }

    if (block.IsOrigin())
    {
        if (!pWallet->AddNewFork(updateWorldLine.hashFork, updateWorldLine.hashParent,
                                 updateWorldLine.nOriginHeight))
        {
            return ERR_SYS_DATABASE_ERROR;
        }
    }

    if (!pWallet->SynchronizeTxSet(changeTxSet))
    {
        return ERR_SYS_DATABASE_ERROR;
    }

    if (!block.IsOrigin() && !block.IsVacant())
    {
        auto spBroadcastBlockInvMsg = CBroadcastBlockInvMessage::Create(updateWorldLine.hashFork, block.GetHash());
        PUBLISH_MESSAGE(spBroadcastBlockInvMsg);
        pDataStat->AddP2pSynSendStatData(updateWorldLine.hashFork, 1, block.vtx.size());
    }

    pService->NotifyWorldLineUpdate(updateWorldLine);

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
    err = pTxPoolCntrl->Push(tx, hashFork, destIn, nValueIn);
    if (err != OK)
    {
        return err;
    }

    pDataStat->AddP2pSynTxSynStatData(hashFork, !!nNonce);

    CAssembledTx assembledTx(tx, -1, destIn, nValueIn);
    if (!pWallet->AddNewTx(hashFork, assembledTx))
    {
        return ERR_SYS_DATABASE_ERROR;
    }

    CTransactionUpdate updateTransaction;
    updateTransaction.hashFork = hashFork;
    updateTransaction.txUpdate = tx;
    updateTransaction.nChange = assembledTx.GetChange();
    pService->NotifyTransactionUpdate(updateTransaction);

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
    if (pWorldLineCntrl->GetBlockLocation(hashAnchor, hashFork, nHeight) && hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        return pConsensus->AddNewDistribute(nHeight, dest, vchDistribute);
    }
    return false;
}

bool CDispatcher::AddNewPublish(const uint256& hashAnchor, const CDestination& dest, const vector<unsigned char>& vchPublish)
{
    uint256 hashFork;
    int nHeight;
    if (pWorldLineCntrl->GetBlockLocation(hashAnchor, hashFork, nHeight) && hashFork == pCoreProtocol->GetGenesisBlockHash())
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
            Log("Send DelegateTx %s (%s)\n", ErrorString(err), tx.GetHash().GetHex().c_str());
        }
    }

    CEventBlockMakerUpdate* pBlockMakerUpdate = new CEventBlockMakerUpdate(0);
    if (pBlockMakerUpdate != nullptr)
    {
        CProofOfSecretShare proof;
        proof.Load(block.vchProof);
        pBlockMakerUpdate->data.hashBlock = updateWorldLine.hashLastBlock;
        pBlockMakerUpdate->data.nBlockTime = updateWorldLine.nLastBlockTime;
        pBlockMakerUpdate->data.nBlockHeight = updateWorldLine.nLastBlockHeight;
        pBlockMakerUpdate->data.nAgreement = proof.nAgreement;
        pBlockMakerUpdate->data.nWeight = proof.nWeight;
        pBlockMakerUpdate->data.nMintType = block.txMint.nType;
        pBlockMaker->PostEvent(pBlockMakerUpdate);
    }

    SyncForkHeight(updateWorldLine.nLastBlockHeight);
}

void CDispatcher::ActivateFork(const uint256& hashFork, const uint64& nNonce)
{
    Log("Activating fork %s ...\n", hashFork.GetHex().c_str());
    if (!pWorldLineCntrl->Exists(hashFork))
    {
        CForkContext ctxt;
        if (!pWorldLineCntrl->GetForkContext(hashFork, ctxt))
        {
            Warn("Failed to find fork context %s\n", hashFork.GetHex().c_str());
            return;
        }

        CTransaction txFork;
        if (!pWorldLineCntrl->GetTransaction(ctxt.txidEmbedded, txFork))
        {
            Warn("Failed to find tx fork %s\n", hashFork.GetHex().c_str());
            return;
        }

        if (!ProcessForkTx(ctxt.txidEmbedded, txFork))
        {
            return;
        }
        Log("Add origin block in tx (%s), hash=%s\n", ctxt.txidEmbedded.GetHex().c_str(),
            hashFork.GetHex().c_str());
    }

    auto spSubscribeForkMsg = CSubscribeForkMessage::Create(hashFork, nNonce);
    PUBLISH_MESSAGE(spSubscribeForkMsg);

    Log("Activated fork %s ...\n", hashFork.GetHex().c_str());
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
        Warn("Invalid orign block found in tx (%s)\n", txid.GetHex().c_str());
        return false;
    }

    Errno err = AddNewBlock(block);
    if (err != OK)
    {
        Log("Add origin block in tx (%s) failed : %s\n", txid.GetHex().c_str(),
            ErrorString(err));
        return false;
    }
    return true;
}

void CDispatcher::SyncForkHeight(int nPrimaryHeight)
{
    map<uint256, CForkStatus> mapForkStatus;
    pWorldLineCntrl->GetForkStatus(mapForkStatus);
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
            && pWorldLineCntrl->GetLastBlockTime(pCoreProtocol->GetGenesisBlockHash(), nDepth, vTimeStamp))
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
