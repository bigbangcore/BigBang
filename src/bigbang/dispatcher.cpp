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
    pBlockChain = nullptr;
    pTxPool = nullptr;
    pForkManager = nullptr;
    pConsensus = nullptr;
    pWallet = nullptr;
    pService = nullptr;
    pBlockMaker = nullptr;
    pNetChannel = nullptr;
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

    if (!GetObject("blockchain", pBlockChain))
    {
        Error("Failed to request blockchain\n");
        return false;
    }

    if (!GetObject("txpool", pTxPool))
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

    if (!GetObject("netchannel", pNetChannel))
    {
        Error("Failed to request netchannel\n");
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
    pBlockChain = nullptr;
    pTxPool = nullptr;
    pForkManager = nullptr;
    pConsensus = nullptr;
    pWallet = nullptr;
    pService = nullptr;
    pBlockMaker = nullptr;
    pNetChannel = nullptr;
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
    if (!pBlockChain->Exists(block.hashPrev))
    {
        return ERR_MISSING_PREV;
    }

    CBlockChainUpdate updateBlockChain;
    if (!block.IsOrigin())
    {
        err = pBlockChain->AddNewBlock(block, updateBlockChain);
        if (err == OK && !block.IsVacant())
        {
            if (!nNonce)
            {
                pDataStat->AddBlockMakerStatData(updateBlockChain.hashFork, block.IsProofOfWork(), block.vtx.size());
            }
            else
            {
                pDataStat->AddP2pSynRecvStatData(updateBlockChain.hashFork, 1, block.vtx.size());
            }
        }
    }
    else
    {
        err = pBlockChain->AddNewOrigin(block, updateBlockChain);
    }

    if (err != OK || updateBlockChain.IsNull())
    {
        return err;
    }

    CTxSetChange changeTxSet;
    if (!pTxPool->SynchronizeBlockChain(updateBlockChain, changeTxSet))
    {
        return ERR_SYS_DATABASE_ERROR;
    }

    if (block.IsOrigin())
    {
        if (!pWallet->AddNewFork(updateBlockChain.hashFork, updateBlockChain.hashParent,
                                 updateBlockChain.nOriginHeight))
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
        pNetChannel->BroadcastBlockInv(updateBlockChain.hashFork, block.GetHash());
        pDataStat->AddP2pSynSendStatData(updateBlockChain.hashFork, 1, block.vtx.size());
    }

    pService->NotifyBlockChainUpdate(updateBlockChain);

    if (!block.IsVacant())
    {
        vector<uint256> vActive, vDeactive;
        pForkManager->ForkUpdate(updateBlockChain, vActive, vDeactive);

        for (const uint256 hashFork : vActive)
        {
            ActivateFork(hashFork, nNonce);
        }

        for (const uint256 hashFork : vDeactive)
        {
            pNetChannel->UnsubscribeFork(hashFork);
        }
    }

    if (block.IsPrimary())
    {
        UpdatePrimaryBlock(block, updateBlockChain, changeTxSet, nNonce);
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
    err = pTxPool->Push(tx, hashFork, destIn, nValueIn);
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
        pNetChannel->BroadcastTxInv(hashFork);
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
    if (pBlockChain->GetBlockLocation(hashAnchor, hashFork, nHeight) && hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        return pConsensus->AddNewDistribute(nHeight, dest, vchDistribute);
    }
    return false;
}

bool CDispatcher::AddNewPublish(const uint256& hashAnchor, const CDestination& dest, const vector<unsigned char>& vchPublish)
{
    uint256 hashFork;
    int nHeight;
    if (pBlockChain->GetBlockLocation(hashAnchor, hashFork, nHeight) && hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        return pConsensus->AddNewPublish(nHeight, dest, vchPublish);
    }
    return false;
}

void CDispatcher::UpdatePrimaryBlock(const CBlock& block, const CBlockChainUpdate& updateBlockChain, const CTxSetChange& changeTxSet, const uint64& nNonce)
{
    CDelegateRoutine routineDelegate;

    if (!pCoreProtocol->CheckFirstPow(updateBlockChain.nLastBlockHeight))
    {
        pConsensus->PrimaryUpdate(updateBlockChain, changeTxSet, routineDelegate);

        pDelegatedChannel->PrimaryUpdate(updateBlockChain.nLastBlockHeight - updateBlockChain.vBlockAddNew.size(),
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
        pBlockMakerUpdate->data.hashBlock = updateBlockChain.hashLastBlock;
        pBlockMakerUpdate->data.nBlockTime = updateBlockChain.nLastBlockTime;
        pBlockMakerUpdate->data.nBlockHeight = updateBlockChain.nLastBlockHeight;
        pBlockMakerUpdate->data.nAgreement = proof.nAgreement;
        pBlockMakerUpdate->data.nWeight = proof.nWeight;
        pBlockMakerUpdate->data.nMintType = block.txMint.nType;
        pBlockMaker->PostEvent(pBlockMakerUpdate);
    }

    SyncForkHeight(updateBlockChain.nLastBlockHeight);
}

void CDispatcher::ActivateFork(const uint256& hashFork, const uint64& nNonce)
{
    Log("Activating fork %s ...\n", hashFork.GetHex().c_str());
    if (!pBlockChain->Exists(hashFork))
    {
        CForkContext ctxt;
        if (!pBlockChain->GetForkContext(hashFork, ctxt))
        {
            Warn("Failed to find fork context %s\n", hashFork.GetHex().c_str());
            return;
        }

        CTransaction txFork;
        if (!pBlockChain->GetTransaction(ctxt.txidEmbedded, txFork))
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
    pNetChannel->SubscribeFork(hashFork, nNonce);
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
    pBlockChain->GetForkStatus(mapForkStatus);
    for (map<uint256, CForkStatus>::iterator it = mapForkStatus.begin(); it != mapForkStatus.end(); ++it)
    {
        const uint256& hashFork = (*it).first;
        CForkStatus& status = (*it).second;
        if (!pForkManager->IsAllowed(hashFork) || !pNetChannel->IsForkSynchronized(hashFork))
        {
            continue;
        }

        vector<int64> vTimeStamp;
        int nDepth = nPrimaryHeight - status.nLastBlockHeight;

        if (nDepth > 1 && hashFork != pCoreProtocol->GetGenesisBlockHash()
            && pBlockChain->GetLastBlockTime(pCoreProtocol->GetGenesisBlockHash(), nDepth, vTimeStamp))
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
