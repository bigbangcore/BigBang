// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "core.h"

#include "../common/template/exchange.h"
#include "../common/template/mint.h"
#include "../common/template/payment.h"
#include "address.h"
#include "wallet.h"

using namespace std;
using namespace xengine;

#define DEBUG(err, ...) Debug((err), __FUNCTION__, __VA_ARGS__)

static const int64 MAX_CLOCK_DRIFT = 80;

static const int PROOF_OF_WORK_BITS_LOWER_LIMIT = 8;
static const int PROOF_OF_WORK_BITS_UPPER_LIMIT = 200;
static const int PROOF_OF_WORK_BITS_INIT_MAINNET = 10;
static const int PROOF_OF_WORK_BITS_INIT_TESTNET = 10;
static const uint32 PROOF_OF_WORK_DIFFICULTY_INTERVAL_MAINNET = 30;
static const uint32 PROOF_OF_WORK_DIFFICULTY_INTERVAL_TESTNET = 30;

static const int64 BBCP_TOKEN_INIT = 1000000000;
static const int64 BBCP_YEAR_INC_REWARD_TOKEN = 50;
static const int64 BBCP_INIT_REWARD_TOKEN = BBCP_TOKEN_INIT;

namespace bigbang
{
///////////////////////////////
// CCoreProtocol

CCoreProtocol::CCoreProtocol()
{
    nProofOfWorkLowerLimit = (~uint256(uint64(0)) >> PROOF_OF_WORK_BITS_LOWER_LIMIT);
    nProofOfWorkLowerLimit.SetCompact(nProofOfWorkLowerLimit.GetCompact());
    nProofOfWorkUpperLimit = (~uint256(uint64(0)) >> PROOF_OF_WORK_BITS_UPPER_LIMIT);
    nProofOfWorkUpperLimit.SetCompact(nProofOfWorkUpperLimit.GetCompact());
    nProofOfWorkInit = (~uint256(uint64(0)) >> PROOF_OF_WORK_BITS_INIT_MAINNET);
    nProofOfWorkDifficultyInterval = PROOF_OF_WORK_DIFFICULTY_INTERVAL_MAINNET;
    pBlockChain = nullptr;
}

CCoreProtocol::~CCoreProtocol()
{
}

bool CCoreProtocol::HandleInitialize()
{
    CBlock block;
    GetGenesisBlock(block);
    hashGenesisBlock = block.GetHash();
    destGenesis = block.txMint.sendTo;
    if (!GetObject("blockchain", pBlockChain))
    {
        return false;
    }
    return true;
}

Errno CCoreProtocol::Debug(const Errno& err, const char* pszFunc, const char* pszFormat, ...)
{
    string strFormat(pszFunc);
    strFormat += string(", ") + string(ErrorString(err)) + string(" : ") + string(pszFormat);
    va_list ap;
    va_start(ap, pszFormat);
    VDebug(strFormat.c_str(), ap);
    va_end(ap);
    return err;
}

const uint256& CCoreProtocol::GetGenesisBlockHash()
{
    return hashGenesisBlock;
}

const CDestination& CCoreProtocol::GetGenesisDestination()
{
    return destGenesis;
}

/*
PubKey : da915f7d9e1b1f6ed99fd816ff977a7d1f17cc95ba0209eef770fb9d00638b49
Secret : 9df809804369829983150491d1086b99f6493356f91ccc080e661a76a976a4ee

PubKey : e76226a3933711f195aa6c1879e2381976b33337bf7b3b296edd8dff105b24b5
Secret : c00f1c287f0d9c0931b1b3f540409e8f7ad362427df4a75286992cb9200096b1

PubKey : fe8455584d820639d140dad74d2644d742616ae2433e61c0423ec350c2226b78
Secret : 9f1e445c2a8e74fabbb7c53e31323b2316112990078cbd8d27b2cd7100a1648d
*/

void CCoreProtocol::GetGenesisBlock(CBlock& block)
{
    const CDestination destOwner = CDestination(bigbang::crypto::CPubKey(uint256("da915f7d9e1b1f6ed99fd816ff977a7d1f17cc95ba0209eef770fb9d00638b49")));

    block.SetNull();

    block.nVersion = CBlock::BLOCK_VERSION;
    block.nType = CBlock::BLOCK_GENESIS;
    block.nTimeStamp = 1598497200;
    block.hashPrev = 0;

    CTransaction& tx = block.txMint;
    tx.nType = CTransaction::TX_GENESIS;
    tx.nTimeStamp = block.nTimeStamp;
    tx.sendTo = destOwner;
    tx.nAmount = BBCP_INIT_REWARD_TOKEN * COIN;

    CProfile profile;
    profile.strName = "Market Finance";
    profile.strSymbol = "MKF";
    profile.destOwner = destOwner;
    profile.nAmount = tx.nAmount;
    profile.nMintReward = BBCP_INIT_REWARD_TOKEN * COIN;
    profile.nMinTxFee = MIN_TX_FEE;
    profile.nHalveCycle = 0;
    profile.SetFlag(true, false, false);

    profile.Save(block.vchProof);
}

Errno CCoreProtocol::ValidateTransaction(const CTransaction& tx, int nHeight)
{
    if (tx.vInput.empty() && tx.nType != CTransaction::TX_GENESIS && tx.nType != CTransaction::TX_WORK)
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "tx vin is empty");
    }
    if (!tx.vInput.empty() && (tx.nType == CTransaction::TX_GENESIS || tx.nType == CTransaction::TX_WORK))
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "tx vin is not empty for genesis or work tx");
    }
    if (!tx.vchSig.empty() && tx.IsMintTx())
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "invalid signature");
    }
    if (tx.sendTo.IsNull())
    {
        return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "send to null address");
    }
    if (!MoneyRange(tx.nAmount))
    {
        return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "amount overflow %ld", tx.nAmount);
    }

    if (!MoneyRange(tx.nTxFee)
        || (tx.nType != CTransaction::TX_TOKEN && tx.nTxFee != 0)
        || (tx.nType == CTransaction::TX_TOKEN
            && (tx.nTxFee < CalcMinTxFee(tx, MIN_TX_FEE))))
    {
        return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "txfee invalid %ld", tx.nTxFee);
    }

    if (tx.sendTo.IsTemplate())
    {
        CTemplateId tid;
        if (!tx.sendTo.GetTemplateId(tid))
        {
            return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "send to address invalid 1");
        }
    }

    set<CTxOutPoint> setInOutPoints;
    for (const CTxIn& txin : tx.vInput)
    {
        if (txin.prevout.IsNull() || txin.prevout.n > 1)
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "prevout invalid");
        }
        if (!setInOutPoints.insert(txin.prevout).second)
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "duplicate inputs");
        }
    }

    if (GetSerializeSize(tx) > MAX_TX_SIZE)
    {
        return DEBUG(ERR_TRANSACTION_OVERSIZE, "%u", GetSerializeSize(tx));
    }

    return OK;
}

Errno CCoreProtocol::ValidateBlock(const CBlock& block)
{
    // These are checks that are independent of context
    // Only allow CBlock::BLOCK_PRIMARY type in v1.0.0
    /*if (block.nType != CBlock::BLOCK_PRIMARY)
    {
        return DEBUG(ERR_BLOCK_TYPE_INVALID, "Block type error\n");
    }*/
    // Check timestamp
    if (block.GetBlockTime() > GetNetTime() + MAX_CLOCK_DRIFT)
    {
        return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "%ld", block.GetBlockTime());
    }

    // Validate mint tx
    if (!block.txMint.IsMintTx() || ValidateTransaction(block.txMint, block.GetBlockHeight()) != OK)
    {
        return DEBUG(ERR_BLOCK_TRANSACTIONS_INVALID, "invalid mint tx");
    }

    size_t nBlockSize = GetSerializeSize(block);
    if (nBlockSize > MAX_BLOCK_SIZE)
    {
        return DEBUG(ERR_BLOCK_OVERSIZE, "size overflow size=%u vtx=%u", nBlockSize, block.vtx.size());
    }

    if (block.nType == CBlock::BLOCK_ORIGIN && !block.vtx.empty())
    {
        return DEBUG(ERR_BLOCK_TRANSACTIONS_INVALID, "origin block vtx is not empty");
    }

    vector<uint256> vMerkleTree;
    if (block.hashMerkle != block.BuildMerkleTree(vMerkleTree))
    {
        return DEBUG(ERR_BLOCK_TXHASH_MISMATCH, "tx merkeroot mismatched");
    }

    set<uint256> setTx;
    setTx.insert(vMerkleTree.begin(), vMerkleTree.begin() + block.vtx.size());
    if (setTx.size() != block.vtx.size())
    {
        return DEBUG(ERR_BLOCK_DUPLICATED_TRANSACTION, "duplicate tx");
    }

    for (int i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction& tx = block.vtx[i];
        if (i == 0 && tx.nType == CTransaction::TX_GENESIS)
        {
            if (ValidateTransaction(tx, block.GetBlockHeight()) != OK)
            {
                return DEBUG(ERR_BLOCK_TRANSACTIONS_INVALID, "invalid tx %s", tx.GetHash().GetHex().c_str());
            }
            continue;
        }
        if (tx.IsMintTx() || ValidateTransaction(tx, block.GetBlockHeight()) != OK)
        {
            return DEBUG(ERR_BLOCK_TRANSACTIONS_INVALID, "invalid tx %s", tx.GetHash().GetHex().c_str());
        }
    }

    if (!CheckBlockSignature(block))
    {
        return DEBUG(ERR_BLOCK_SIGNATURE_INVALID, "CheckBlockSignature fail");
    }
    return OK;
}

Errno CCoreProtocol::ValidateOrigin(const CBlock& block, const CProfile& parentProfile, CProfile& forkProfile)
{
    if (!forkProfile.Load(block.vchProof))
    {
        return DEBUG(ERR_BLOCK_INVALID_FORK, "load profile error");
    }
    if (forkProfile.IsNull())
    {
        return DEBUG(ERR_BLOCK_INVALID_FORK, "invalid profile");
    }
    if (!MoneyRange(forkProfile.nAmount))
    {
        return DEBUG(ERR_BLOCK_INVALID_FORK, "invalid fork amount");
    }
    if (!RewardRange(forkProfile.nMintReward))
    {
        return DEBUG(ERR_BLOCK_INVALID_FORK, "invalid fork reward");
    }
    if (parentProfile.IsPrivate())
    {
        if (!forkProfile.IsPrivate() || parentProfile.destOwner != forkProfile.destOwner)
        {
            return DEBUG(ERR_BLOCK_INVALID_FORK, "permission denied");
        }
    }
    return OK;
}

Errno CCoreProtocol::VerifyProofOfWork(const CBlock& block, const CBlockIndex* pIndexPrev)
{
    if (block.vchProof.size() < CProofOfHashWorkCompact::PROOFHASHWORK_SIZE)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "vchProof size error.");
    }

    if (block.GetBlockTime() < pIndexPrev->GetBlockTime())
    {
        return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "Timestamp out of range 1, height: %d, block time: %d, prev time: %d, block: %s.",
                     block.GetBlockHeight(), block.GetBlockTime(),
                     pIndexPrev->GetBlockTime(), block.GetHash().GetHex().c_str());
    }

    CProofOfHashWorkCompact proof;
    proof.Load(block.vchProof);

    uint32 nBits = 0;
    int64 nReward = 0;
    uint8 nAlgo = 0;
    if (!GetProofOfWorkTarget(pIndexPrev, nAlgo, nBits, nReward))
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "get target fail.");
    }

    if (nBits != proof.nBits)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "algo or bits error, nAlgo: %d, nBits: %d, vchProof size: %ld.", nAlgo, proof.nBits, block.vchProof.size());
    }

    bool fNegative;
    bool fOverflow;
    uint256 hashTarget;
    hashTarget.SetCompact(nBits, &fNegative, &fOverflow);

    if (fNegative || hashTarget == 0 || fOverflow || hashTarget < nProofOfWorkUpperLimit || hashTarget > nProofOfWorkLowerLimit)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "nBits error, nBits: %u, hashTarget: %s, negative: %d, overflow: %d, upper: %s, lower: %s",
                     nBits, hashTarget.ToString().c_str(), fNegative, fOverflow, nProofOfWorkUpperLimit.ToString().c_str(), nProofOfWorkLowerLimit.ToString().c_str());
    }

    vector<unsigned char> vchProofOfWork;
    block.GetSerializedProofOfWorkData(vchProofOfWork);
    uint256 hash = crypto::CryptoPowHash(&vchProofOfWork[0], vchProofOfWork.size());
    if (hash > hashTarget)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "hash error: proof[%s] vs. target[%s] with bits[%d]",
                     hash.ToString().c_str(), hashTarget.ToString().c_str(), nBits);
    }

    return OK;
}

Errno CCoreProtocol::VerifyBlockTx(const CTransaction& tx, const CTxContxt& txContxt, CBlockIndex* pIndexPrev, int nForkHeight, const uint256& fork)
{
    const CDestination& destIn = txContxt.destIn;
    int64 nValueIn = 0;
    for (const CTxInContxt& inctxt : txContxt.vin)
    {
        if (inctxt.nTxTime > tx.nTimeStamp)
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "tx time is ahead of input tx");
        }
        if (inctxt.IsLocked(pIndexPrev->GetBlockHeight()))
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "input is still locked");
        }
        nValueIn += inctxt.nAmount;
    }

    if (!MoneyRange(nValueIn))
    {
        return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein invalid %ld", nValueIn);
    }
    if (nValueIn < tx.nAmount + tx.nTxFee)
    {
        return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein is not enough (%ld : %ld)", nValueIn, tx.nAmount + tx.nTxFee);
    }

    vector<uint8> vchSig;
    if (tx.sendTo.IsTemplate() && tx.sendTo.GetTemplateId().GetType() == TEMPLATE_INCREASECOIN)
    {
        CTemplatePtr ptr = CTemplate::CreateTemplatePtr(TEMPLATE_INCREASECOIN, tx.vchSig);
        if (!ptr)
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature vchSig err");
        }
        set<CDestination> setSubDest;
        if (!ptr->GetSignDestination(tx, tx.vchSig, setSubDest, vchSig))
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature vchSig err");
        }
    }
    else
    {
        vchSig = tx.vchSig;
    }

    if (destIn.IsTemplate() && destIn.GetTemplateId().GetType() == TEMPLATE_PAYMENT)
    {
        auto templatePtr = CTemplate::CreateTemplatePtr(TEMPLATE_PAYMENT, vchSig);
        if (templatePtr == nullptr)
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature vchSig err");
        }
        auto payment = boost::dynamic_pointer_cast<CTemplatePayment>(templatePtr);
        if (nForkHeight >= (payment->m_height_exec + payment->SafeHeight))
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature");
        }
        else
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature");
        }
    }

    if (!destIn.VerifyTxSignature(tx.GetSignatureHash(), tx.nType, /*tx.hashAnchor*/ GetGenesisBlockHash(), tx.sendTo, vchSig, nForkHeight, fork))
    {
        return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature");
    }

    return OK;
}

Errno CCoreProtocol::VerifyTransaction(const CTransaction& tx, const vector<CTxOut>& vPrevOutput,
                                       int nForkHeight, const uint256& fork)
{
    CDestination destIn = vPrevOutput[0].destTo;
    int64 nValueIn = 0;
    for (const CTxOut& output : vPrevOutput)
    {
        if (destIn != output.destTo)
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "input destination mismatched");
        }
        if (output.nTxTime > tx.nTimeStamp)
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "tx time is ahead of input tx");
        }
        if (output.IsLocked(nForkHeight))
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "input is still locked");
        }
        nValueIn += output.nAmount;
    }
    if (!MoneyRange(nValueIn))
    {
        return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein invalid %ld", nValueIn);
    }
    if (nValueIn < tx.nAmount + tx.nTxFee)
    {
        return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein is not enough (%ld : %ld)", nValueIn, tx.nAmount + tx.nTxFee);
    }

    vector<uint8> vchSig;
    if (tx.sendTo.IsTemplate() && tx.sendTo.GetTemplateId().GetType() == TEMPLATE_INCREASECOIN)
    {
        CTemplatePtr ptr = CTemplate::CreateTemplatePtr(TEMPLATE_INCREASECOIN, tx.vchSig);
        if (!ptr)
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature vchSig err");
        }
        set<CDestination> setSubDest;
        if (!ptr->GetSignDestination(tx, tx.vchSig, setSubDest, vchSig))
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature vchSig err");
        }
    }
    else
    {
        vchSig = tx.vchSig;
    }

    if (!destIn.VerifyTxSignature(tx.GetSignatureHash(), tx.nType, /*tx.hashAnchor*/ GetGenesisBlockHash(), tx.sendTo, vchSig, nForkHeight, fork))
    {
        return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature");
    }

    if (destIn.IsTemplate() && destIn.GetTemplateId().GetType() == TEMPLATE_PAYMENT)
    {
        auto templatePtr = CTemplate::CreateTemplatePtr(TEMPLATE_PAYMENT, vchSig);
        if (templatePtr == nullptr)
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature vchSig err");
        }
        auto payment = boost::dynamic_pointer_cast<CTemplatePayment>(templatePtr);
        if (nForkHeight >= (payment->m_height_exec + payment->SafeHeight))
        {
            CBlock block;
            std::multimap<int64, CDestination> mapVotes;
            CProofOfSecretShare dpos;
            // if (!pBlockChain->ListDelegatePayment(payment->m_height_exec, block, mapVotes) || !dpos.Load(block.vchProof))
            // {
            //     return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature vote err\n");
            // }
            if (!payment->VerifyTransaction(tx, nForkHeight, mapVotes, dpos.nAgreement, nValueIn))
            {
                return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature");
            }
        }
        else
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature");
        }
    }

    return OK;
}

bool CCoreProtocol::GetBlockTrust(const CBlock& block, uint256& nChainTrust)
{
    if (!block.IsPrimary())
    {
        StdError("CCoreProtocol", "GetBlockTrust: block type error");
        return false;
    }

    if (!block.IsProofOfWork())
    {
        StdError("CCoreProtocol", "GetBlockTrust: IsProofOfWork fail");
        return false;
    }

    CProofOfHashWorkCompact proof;
    proof.Load(block.vchProof);
    uint256 nTarget;
    nTarget.SetCompact(proof.nBits);
    // nChainTrust = 2**256 / (nTarget+1) = ~nTarget / (nTarget+1) + 1
    nChainTrust = (~nTarget / (nTarget + 1)) + 1;
    return true;
}

bool CCoreProtocol::GetProofOfWorkTarget(const CBlockIndex* pIndexPrev, int nAlgo, uint32_t& nBits, int64& nReward)
{
    nReward = GetPrimaryMintWorkReward(pIndexPrev);

    if (pIndexPrev->GetBlockHeight() == 0)
    {
        nBits = nProofOfWorkInit.GetCompact();
    }
    else if ((pIndexPrev->nHeight + 1) % nProofOfWorkDifficultyInterval != 0)
    {
        nBits = pIndexPrev->nProofBits;
    }
    else
    {
        // statistic the sum of nProofOfWorkDifficultyInterval blocks time
        const CBlockIndex* pIndexFirst = pIndexPrev;
        for (int i = 1; i < nProofOfWorkDifficultyInterval && pIndexFirst; i++)
        {
            pIndexFirst = pIndexFirst->pPrev;
        }

        if (!pIndexFirst || pIndexFirst->GetBlockHeight() != (pIndexPrev->GetBlockHeight() - (nProofOfWorkDifficultyInterval - 1)))
        {
            StdError("CCoreProtocol", "GetProofOfWorkTarget: first block of difficulty interval height is error");
            return false;
        }

        if (pIndexPrev == pIndexFirst)
        {
            StdError("CCoreProtocol", "GetProofOfWorkTarget: difficulty interval must be large than 1");
            return false;
        }

        // Limit adjustment step
        if (pIndexPrev->GetBlockTime() < pIndexFirst->GetBlockTime())
        {
            StdError("CCoreProtocol", "GetProofOfWorkTarget: prev block time [%ld] < first block time [%ld]", pIndexPrev->GetBlockTime(), pIndexFirst->GetBlockTime());
            return false;
        }
        uint64 nActualTimespan = pIndexPrev->GetBlockTime() - pIndexFirst->GetBlockTime();
        uint64 nTargetTimespan = (uint64)nProofOfWorkDifficultyInterval * BLOCK_TARGET_SPACING;
        if (nActualTimespan < nTargetTimespan / 4)
        {
            nActualTimespan = nTargetTimespan / 4;
        }
        if (nActualTimespan > nTargetTimespan * 4)
        {
            nActualTimespan = nTargetTimespan * 4;
        }

        uint256 newBits;
        newBits.SetCompact(pIndexPrev->nProofBits);
        // StdLog("CCoreProtocol", "newBits *= nActualTimespan, newBits: %s, nActualTimespan: %lu", newBits.ToString().c_str(), nActualTimespan);
        //newBits /= uint256(nTargetTimespan);
        // StdLog("CCoreProtocol", "newBits: %s", newBits.ToString().c_str());
        //newBits *= nActualTimespan;
        // StdLog("CCoreProtocol", "newBits /= nTargetTimespan, newBits: %s, nTargetTimespan: %lu", newBits.ToString().c_str(), nTargetTimespan);
        if (newBits >= uint256(nTargetTimespan))
        {
            newBits /= uint256(nTargetTimespan);
            newBits *= nActualTimespan;
        }
        else
        {
            newBits *= nActualTimespan;
            newBits /= uint256(nTargetTimespan);
        }
        if (newBits < nProofOfWorkUpperLimit)
        {
            newBits = nProofOfWorkUpperLimit;
        }
        if (newBits > nProofOfWorkLowerLimit)
        {
            newBits = nProofOfWorkLowerLimit;
        }
        nBits = newBits.GetCompact();
        // StdLog("CCoreProtocol", "newBits.GetCompact(), newBits: %s, nBits: %x", newBits.ToString().c_str(), nBits);
    }

    return true;
}

int64 CCoreProtocol::GetPrimaryMintWorkReward(const CBlockIndex* pIndexPrev)
{
    return BBCP_YEAR_INC_REWARD_TOKEN * COIN;
}

bool CCoreProtocol::VerifyIncreaseCoinTx(const uint256& hashBlock, const CBlock& block, int64 nIncreaseCoin)
{
    if (block.vtx.empty())
    {
        StdError("CCoreProtocol", "VerifyBlock: Increase coin block, not genesis tx, block: %s", hashBlock.GetHex().c_str());
        return false;
    }
    const CTransaction& tx = block.vtx[0];
    if (tx.nType != CTransaction::TX_GENESIS)
    {
        StdError("CCoreProtocol", "VerifyIncreaseCoinTx: Increase coin block, nType error, nType: %d, block: %s",
                 tx.nType, hashBlock.GetHex().c_str());
        return false;
    }
    if (tx.sendTo != GetGenesisDestination())
    {
        StdError("CCoreProtocol", "VerifyIncreaseCoinTx: Increase coin block, sendTo error, sendTo: %s, block: %s",
                 CAddress(tx.sendTo).ToString().c_str(), hashBlock.GetHex().c_str());
        return false;
    }
    if (tx.nAmount != nIncreaseCoin)
    {
        StdError("CCoreProtocol", "VerifyIncreaseCoinTx: Increase coin block, nAmount error, nAmount: %lu, block: %s",
                 tx.nAmount, hashBlock.GetHex().c_str());
        return false;
    }
    if (tx.nTxFee != 0)
    {
        StdError("CCoreProtocol", "VerifyIncreaseCoinTx: Increase coin block, nTxFee error, nTxFee: %lu, block: %s",
                 tx.nTxFee, hashBlock.GetHex().c_str());
        return false;
    }
    if (!tx.vInput.empty())
    {
        StdError("CCoreProtocol", "VerifyIncreaseCoinTx: Increase coin block, tx vin is not empty for genesis tx, block: %s",
                 hashBlock.GetHex().c_str());
        return false;
    }
    return true;
}

bool CCoreProtocol::CheckBlockSignature(const CBlock& block)
{
    if (block.GetHash() != GetGenesisBlockHash())
    {
        return block.txMint.sendTo.VerifyBlockSignature(block.GetHash(), block.vchSig);
    }
    return true;
}

///////////////////////////////
// CTestNetCoreProtocol

CTestNetCoreProtocol::CTestNetCoreProtocol()
{
    nProofOfWorkInit = (~uint256(uint64(0)) >> PROOF_OF_WORK_BITS_INIT_TESTNET);
    nProofOfWorkDifficultyInterval = PROOF_OF_WORK_DIFFICULTY_INTERVAL_TESTNET;
}

/*

PubKey : 68e4dca5989876ca64f16537e82d05c103e5695dfaf009a01632cb33639cc530
Secret : ab14e1de9a0e805df0c79d50e1b065304814a247e7d52fc51fd0782e0eec27d6

PubKey : 310be18f947a56f92541adbad67374facad61ab814c53fa5541488bea62fb47d
Secret : 14e1abd0802f7065b55f5076d0d2cfbea159abd540a977e8d3afd4b3061bf47f

*/
void CTestNetCoreProtocol::GetGenesisBlock(CBlock& block)
{
    using namespace boost::posix_time;
    using namespace boost::gregorian;
    const CDestination destOwner = CDestination(bigbang::crypto::CPubKey(uint256("68e4dca5989876ca64f16537e82d05c103e5695dfaf009a01632cb33639cc530")));

    block.SetNull();

    block.nVersion = CBlock::BLOCK_VERSION;
    block.nType = CBlock::BLOCK_GENESIS;
    block.nTimeStamp = 1598497200;
    block.hashPrev = 0;

    CTransaction& tx = block.txMint;
    tx.nType = CTransaction::TX_GENESIS;
    tx.nTimeStamp = block.nTimeStamp;
    tx.sendTo = destOwner;
    tx.nAmount = BBCP_INIT_REWARD_TOKEN * COIN; // initial number of token

    CProfile profile;
    profile.strName = "Market Finance Test";
    profile.strSymbol = "MKFTest";
    profile.destOwner = destOwner;
    profile.nAmount = tx.nAmount;
    profile.nMintReward = BBCP_INIT_REWARD_TOKEN * COIN;
    profile.nMinTxFee = MIN_TX_FEE;
    profile.nHalveCycle = 0;
    profile.SetFlag(true, false, false);

    profile.Save(block.vchProof);
}

///////////////////////////////
// CProofOfWorkParam

CProofOfWorkParam::CProofOfWorkParam(bool fTestnet)
{
    if (fTestnet)
    {
        CBlock block;
        CTestNetCoreProtocol core;
        core.GetGenesisBlock(block);
        hashGenesisBlock = block.GetHash();
    }
    else
    {
        CBlock block;
        CCoreProtocol core;
        core.GetGenesisBlock(block);
        hashGenesisBlock = block.GetHash();
    }

    nProofOfWorkLowerLimit = (~uint256(uint64(0)) >> PROOF_OF_WORK_BITS_LOWER_LIMIT);
    nProofOfWorkLowerLimit.SetCompact(nProofOfWorkLowerLimit.GetCompact());
    nProofOfWorkUpperLimit = (~uint256(uint64(0)) >> PROOF_OF_WORK_BITS_UPPER_LIMIT);
    nProofOfWorkUpperLimit.SetCompact(nProofOfWorkUpperLimit.GetCompact());
    if (fTestnet)
    {
        nProofOfWorkInit = (~uint256(uint64(0)) >> PROOF_OF_WORK_BITS_INIT_TESTNET);
        nProofOfWorkDifficultyInterval = PROOF_OF_WORK_DIFFICULTY_INTERVAL_MAINNET;
    }
    else
    {
        nProofOfWorkInit = (~uint256(uint64(0)) >> PROOF_OF_WORK_BITS_INIT_MAINNET);
        nProofOfWorkDifficultyInterval = PROOF_OF_WORK_DIFFICULTY_INTERVAL_TESTNET;
    }
}

bool CProofOfWorkParam::IsDposHeight(int height)
{
    return false;
}

} // namespace bigbang
