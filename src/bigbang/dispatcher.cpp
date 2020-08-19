// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dispatcher.h"

#include <chrono>
#include <future>
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
    pWallet = nullptr;
    pService = nullptr;
    pBlockMaker = nullptr;
    pNetChannel = nullptr;
    pDataStat = nullptr;
}

CDispatcher::~CDispatcher()
{
}

bool CDispatcher::HandleInitialize()
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

    if (!GetObject("forkmanager", pForkManager))
    {
        Error("Failed to request forkmanager");
        return false;
    }

    if (!GetObject("wallet", pWallet))
    {
        Error("Failed to request wallet");
        return false;
    }

    if (!GetObject("service", pService))
    {
        Error("Failed to request service");
        return false;
    }

    if (!GetObject("blockmaker", pBlockMaker))
    {
        Error("Failed to request blockmaker");
        return false;
    }

    if (!GetObject("netchannel", pNetChannel))
    {
        Error("Failed to request netchannel");
        return false;
    }

    if (!GetObject("datastat", pDataStat))
    {
        Error("Failed to request datastat");
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
    pWallet = nullptr;
    pService = nullptr;
    pBlockMaker = nullptr;
    pNetChannel = nullptr;
    pDataStat = nullptr;
}

bool CDispatcher::HandleInvoke()
{
    vector<uint256> vActive;
    if (!pForkManager->LoadForkContext(vActive))
    {
        Error("Failed to load for context");
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
        StdError("CDispatcher", "AddNewBlock: prev block not exist, block: %s, prev: %s", block.GetHash().GetHex().c_str(), block.hashPrev.GetHex().c_str());
        return ERR_MISSING_PREV;
    }
    if (block.IsOrigin())
    {
        StdError("CDispatcher", "AddNewBlock: block is origin, block: %s, prev: %s", block.GetHash().GetHex().c_str(), block.hashPrev.GetHex().c_str());
        return ERR_BLOCK_TYPE_INVALID;
    }

    CBlockChainUpdate updateBlockChain;
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

    if (err != OK || updateBlockChain.IsNull())
    {
        return err;
    }

    CTxSetChange changeTxSet;
    if (!pTxPool->SynchronizeBlockChain(updateBlockChain, changeTxSet))
    {
        StdError("CDispatcher", "AddNewBlock: TxPool SynchronizeBlockChain fail, block: %s", block.GetHash().GetHex().c_str());
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
        StdError("CDispatcher", "AddNewBlock: Wallet SynchronizeTxSet fail, block: %s", block.GetHash().GetHex().c_str());
        return ERR_SYS_DATABASE_ERROR;
    }

    pNetChannel->BroadcastBlockInv(updateBlockChain.hashFork, block.GetHash());
    pDataStat->AddP2pSynSendStatData(updateBlockChain.hashFork, 1, block.vtx.size());

    pService->NotifyBlockChainUpdate(updateBlockChain);

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

    return OK;
}

Errno CDispatcher::AddNewTx(const CTransaction& tx, uint64 nNonce)
{
    Errno err = OK;
    uint256 hashBlock;
    int nHeight = 0;
    int64 nTime = 0;
    uint16 nMintType = 0;
    if (!pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashBlock, nHeight, nTime, nMintType))
    {
        StdError("CDispatcher", "AddNewTx: GetLastBlock fail, fork: %s", pCoreProtocol->GetGenesisBlockHash().GetHex().c_str());
        return ERR_NOT_FOUND;
    }

    err = pCoreProtocol->ValidateTransaction(tx, nHeight);
    if (err != OK)
    {
        StdError("CDispatcher", "AddNewTx: ValidateTransaction fail, txid: %s", tx.GetHash().GetHex().c_str());
        return err;
    }

    uint256 hashFork;
    CDestination destIn;
    int64 nValueIn;
    err = pTxPool->Push(tx, hashFork, destIn, nValueIn);
    if (err != OK)
    {
        StdError("CDispatcher", "AddNewTx: TxPool Push fail, txid: %s", tx.GetHash().GetHex().c_str());
        return err;
    }

    pDataStat->AddP2pSynTxSynStatData(hashFork, !!nNonce);

    CAssembledTx assembledTx(tx, -1, destIn, nValueIn);
    if (!pWallet->AddNewTx(hashFork, assembledTx))
    {
        StdError("CDispatcher", "AddNewTx: Wallet AddNewTx fail, txid: %s", tx.GetHash().GetHex().c_str());
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

    return OK;
}

void CDispatcher::ActivateFork(const uint256& hashFork, const uint64& nNonce)
{
    Log("Activating fork %s ...", hashFork.GetHex().c_str());
    if (!pBlockChain->Exists(hashFork))
    {
        CForkContext ctxt;
        if (!pBlockChain->GetForkContext(hashFork, ctxt))
        {
            Warn("Failed to find fork context %s", hashFork.GetHex().c_str());
            return;
        }

        CTransaction txFork;
        if (!pBlockChain->GetTransaction(ctxt.txidEmbedded, txFork))
        {
            Warn("Failed to find tx fork %s", hashFork.GetHex().c_str());
            return;
        }

        if (!ProcessForkTx(ctxt.txidEmbedded, txFork))
        {
            return;
        }
        Log("Add origin block in tx (%s), hash=%s", ctxt.txidEmbedded.GetHex().c_str(),
            hashFork.GetHex().c_str());
    }
    pNetChannel->SubscribeFork(hashFork, nNonce);
    Log("Activated fork %s ...", hashFork.GetHex().c_str());
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
        Warn("Invalid orign block found in tx (%s)", txid.GetHex().c_str());
        return false;
    }

    Errno err = AddNewBlock(block);
    if (err != OK)
    {
        Log("Add origin block in tx (%s) failed : %s", txid.GetHex().c_str(),
            ErrorString(err));
        return false;
    }
    return true;
}

} // namespace bigbang
