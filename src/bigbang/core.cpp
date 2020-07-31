// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "core.h"

#include "../common/template/delegate.h"
#include "../common/template/exchange.h"
#include "../common/template/fork.h"
#include "../common/template/mint.h"
#include "../common/template/payment.h"
#include "../common/template/vote.h"
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

#ifdef BIGBANG_TESTNET
static const int64 BBCP_TOKEN_INIT = 300000000;
static const int64 BBCP_BASE_REWARD_TOKEN = 20;
static const int64 BBCP_INIT_REWARD_TOKEN = 20;
#else
static const int64 BBCP_TOKEN_INIT = 0;
static const int64 BBCP_YEAR_INC_REWARD_TOKEN = 50;

static const int64 BBCP_INIT_REWARD_TOKEN = BBCP_YEAR_INC_REWARD_TOKEN;
#endif

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

    block.nVersion = 1;
    block.nType = CBlock::BLOCK_GENESIS;
    block.nTimeStamp = 1575043200;
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
    // Basic checks that don't depend on any context
    // Don't allow CTransaction::TX_CERT type in v1.0.0
    /*if (tx.nType != CTransaction::TX_TOKEN && tx.nType != CTransaction::TX_GENESIS
        && tx.nType != CTransaction::TX_STAKE && tx.nType != CTransaction::TX_WORK)
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "tx type is invalid.\n");
    }*/
    if (tx.nType == CTransaction::TX_TOKEN
        && (tx.sendTo.IsPubKey()
            || (tx.sendTo.IsTemplate()
                && (tx.sendTo.GetTemplateId().GetType() == TEMPLATE_WEIGHTED || tx.sendTo.GetTemplateId().GetType() == TEMPLATE_MULTISIG)))
        && !tx.vchData.empty())
    {
        if (tx.vchData.size() < 21)
        { //vchData must contain 3 fields of UUID, timestamp, szDescription at least
            return DEBUG(ERR_TRANSACTION_INVALID, "tx vchData is less than 21 bytes.\n");
        }
        //check description field
        uint16 nPos = 20;
        uint8 szDesc = tx.vchData[nPos];
        if (szDesc > 0)
        {
            if ((nPos + 1 + szDesc) > tx.vchData.size())
            {
                return DEBUG(ERR_TRANSACTION_INVALID, "tx vchData is overflow.\n");
            }
            std::string strDescEncodedBase64(tx.vchData.begin() + nPos + 1, tx.vchData.begin() + nPos + 1 + szDesc);
            xengine::CHttpUtil util;
            std::string strDescDecodedBase64;
            if (!util.Base64Decode(strDescEncodedBase64, strDescDecodedBase64))
            {
                return DEBUG(ERR_TRANSACTION_INVALID, "tx vchData description base64 is not available.\n");
            }
        }
    }
    if (tx.vInput.empty() && tx.nType != CTransaction::TX_GENESIS && tx.nType != CTransaction::TX_WORK && tx.nType != CTransaction::TX_STAKE)
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "tx vin is empty\n");
    }
    if (!tx.vInput.empty() && (tx.nType == CTransaction::TX_GENESIS || tx.nType == CTransaction::TX_WORK || tx.nType == CTransaction::TX_STAKE))
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "tx vin is not empty for genesis or work tx\n");
    }
    if (!tx.vchSig.empty() && tx.IsMintTx())
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "invalid signature\n");
    }
    if (tx.sendTo.IsNull())
    {
        return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "send to null address\n");
    }
    if (!MoneyRange(tx.nAmount))
    {
        return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "amount overflow %ld\n", tx.nAmount);
    }

    if (!MoneyRange(tx.nTxFee)
        || (tx.nType != CTransaction::TX_TOKEN && tx.nTxFee != 0)
        || (tx.nType == CTransaction::TX_TOKEN
            && (tx.nTxFee < CalcMinTxFee(tx, MIN_TX_FEE))))
    {
        return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "txfee invalid %ld", tx.nTxFee);
    }

    if (nHeight != 0 /*&& !IsDposHeight(nHeight)*/)
    {
        if (tx.sendTo.IsTemplate())
        {
            CTemplateId tid;
            if (!tx.sendTo.GetTemplateId(tid))
            {
                return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "send to address invalid 1");
            }
            if (tid.GetType() == TEMPLATE_FORK
                || tid.GetType() == TEMPLATE_DELEGATE
                || tid.GetType() == TEMPLATE_VOTE)
            {
                return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "send to address invalid 2");
            }
        }
    }

    set<CTxOutPoint> setInOutPoints;
    for (const CTxIn& txin : tx.vInput)
    {
        if (txin.prevout.IsNull() || txin.prevout.n > 1)
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "prevout invalid\n");
        }
        if (!setInOutPoints.insert(txin.prevout).second)
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "duplicate inputs\n");
        }
    }

    if (GetSerializeSize(tx) > MAX_TX_SIZE)
    {
        return DEBUG(ERR_TRANSACTION_OVERSIZE, "%u\n", GetSerializeSize(tx));
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
        return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "%ld\n", block.GetBlockTime());
    }

    // validate vacant block
    if (block.nType == CBlock::BLOCK_VACANT)
    {
        return ValidateVacantBlock(block);
    }

    // Validate mint tx
    if (!block.txMint.IsMintTx() || ValidateTransaction(block.txMint, block.GetBlockHeight()) != OK)
    {
        return DEBUG(ERR_BLOCK_TRANSACTIONS_INVALID, "invalid mint tx\n");
    }

    size_t nBlockSize = GetSerializeSize(block);
    if (nBlockSize > MAX_BLOCK_SIZE)
    {
        return DEBUG(ERR_BLOCK_OVERSIZE, "size overflow size=%u vtx=%u\n", nBlockSize, block.vtx.size());
    }

    if (block.nType == CBlock::BLOCK_ORIGIN && !block.vtx.empty())
    {
        return DEBUG(ERR_BLOCK_TRANSACTIONS_INVALID, "origin block vtx is not empty\n");
    }

    vector<uint256> vMerkleTree;
    if (block.hashMerkle != block.BuildMerkleTree(vMerkleTree))
    {
        return DEBUG(ERR_BLOCK_TXHASH_MISMATCH, "tx merkeroot mismatched\n");
    }

    set<uint256> setTx;
    setTx.insert(vMerkleTree.begin(), vMerkleTree.begin() + block.vtx.size());
    if (setTx.size() != block.vtx.size())
    {
        return DEBUG(ERR_BLOCK_DUPLICATED_TRANSACTION, "duplicate tx\n");
    }

    for (const CTransaction& tx : block.vtx)
    {
        if (tx.IsMintTx() || ValidateTransaction(tx, block.GetBlockHeight()) != OK)
        {
            return DEBUG(ERR_BLOCK_TRANSACTIONS_INVALID, "invalid tx %s\n", tx.GetHash().GetHex().c_str());
        }
    }

    if (!CheckBlockSignature(block))
    {
        return DEBUG(ERR_BLOCK_SIGNATURE_INVALID, "\n");
    }
    return OK;
}

Errno CCoreProtocol::ValidateOrigin(const CBlock& block, const CProfile& parentProfile, CProfile& forkProfile)
{
    if (!forkProfile.Load(block.vchProof))
    {
        return DEBUG(ERR_BLOCK_INVALID_FORK, "load profile error\n");
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

    // if (IsDposHeight(block.GetBlockHeight()))
    // {
    //     uint32 nNextTimestamp = GetNextBlockTimeStamp(pIndexPrev->nMintType, pIndexPrev->GetBlockTime(), block.txMint.nType, block.GetBlockHeight());
    //     if (block.GetBlockTime() < nNextTimestamp)
    //     {
    //         return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "Verify proof work: Timestamp out of range 2, height: %d, block time: %d, next time: %d, prev minttype: 0x%x, prev time: %d, block: %s.",
    //                      block.GetBlockHeight(), block.GetBlockTime(), nNextTimestamp,
    //                      pIndexPrev->nMintType, pIndexPrev->GetBlockTime(), block.GetHash().GetHex().c_str());
    //     }
    // }
    // else
    // {
    if (block.GetBlockTime() < pIndexPrev->GetBlockTime())
    {
        return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "Timestamp out of range 1, height: %d, block time: %d, prev time: %d, block: %s.",
                     block.GetBlockHeight(), block.GetBlockTime(),
                     pIndexPrev->GetBlockTime(), block.GetHash().GetHex().c_str());
    }
    //    }

    CProofOfHashWorkCompact proof;
    proof.Load(block.vchProof);

    uint32 nBits = 0;
    int64 nReward = 0;
    if (!GetProofOfWorkTarget(pIndexPrev, proof.nAlgo, nBits, nReward))
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "get target fail.");
    }

    if (nBits != proof.nBits || proof.nAlgo != CM_CRYPTONIGHT)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "algo or bits error, nAlgo: %d, nBits: %d, vchProof size: %ld.", proof.nAlgo, proof.nBits, block.vchProof.size());
    }
    if (proof.destMint != block.txMint.sendTo)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "destMint error, destMint: %s.", proof.destMint.ToString().c_str());
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

Errno CCoreProtocol::VerifyDelegatedProofOfStake(const CBlock& block, const CBlockIndex* pIndexPrev,
                                                 const CDelegateAgreement& agreement)
{
    uint32 nTime = DPoSTimestamp(pIndexPrev);
    if (block.GetBlockTime() != nTime)
    {
        return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "Timestamp out of range. block time %d is not equal %u", block.GetBlockTime(), nTime);
    }

    if (block.txMint.sendTo != agreement.vBallot[0])
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_STAKE_INVALID, "txMint sendTo error.");
    }
    return OK;
}

Errno CCoreProtocol::VerifySubsidiary(const CBlock& block, const CBlockIndex* pIndexPrev, const CBlockIndex* pIndexRef,
                                      const CDelegateAgreement& agreement)
{
    if (block.GetBlockTime() < pIndexPrev->GetBlockTime())
    {
        return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "Timestamp out of range.");
    }

    if (!block.IsExtended())
    {
        if (block.GetBlockTime() != pIndexRef->GetBlockTime())
        {
            return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "Timestamp out of range.");
        }
    }
    else
    {
        if (block.GetBlockTime() <= pIndexRef->GetBlockTime()
            || block.GetBlockTime() >= pIndexRef->GetBlockTime() + BLOCK_TARGET_SPACING
            || block.GetBlockTime() != pIndexPrev->GetBlockTime() + EXTENDED_BLOCK_SPACING)
        {
            return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "Timestamp out of range.");
        }
    }

    if (block.txMint.sendTo != agreement.GetBallot(0))
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_STAKE_INVALID, "txMint sendTo error.");
    }

    return OK;
}

Errno CCoreProtocol::VerifyBlock(const CBlock& block, CBlockIndex* pIndexPrev)
{
    (void)block;
    (void)pIndexPrev;
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
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "tx time is ahead of input tx\n");
        }
        if (inctxt.IsLocked(pIndexPrev->GetBlockHeight()))
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "input is still locked\n");
        }
        nValueIn += inctxt.nAmount;
    }

    if (!MoneyRange(nValueIn))
    {
        return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein invalid %ld\n", nValueIn);
    }
    if (nValueIn < tx.nAmount + tx.nTxFee)
    {
        return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein is not enough (%ld : %ld)\n", nValueIn, tx.nAmount + tx.nTxFee);
    }

    if (tx.nType == CTransaction::TX_CERT)
    {
        if (VerifyCertTx(tx, destIn, fork) != OK)
        {
            return DEBUG(ERR_TRANSACTION_INVALID, "invalid cert tx");
        }
    }

    if (destIn.GetTemplateId().GetType() == TEMPLATE_VOTE || tx.sendTo.GetTemplateId().GetType() == TEMPLATE_VOTE)
    {
        if (VerifyVoteTx(tx, destIn, fork) != OK)
        {
            return DEBUG(ERR_TRANSACTION_INVALID, "invalid vote tx");
        }
    }

    // v1.0 function
    /*if (!tx.vchData.empty())
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "vchData not empty\n");
    }*/

    vector<uint8> vchSig;
    /*if (CTemplate::IsDestInRecorded(tx.sendTo))
    {
        CDestination recordedDestIn;
        if (!CSendToRecordedTemplate::ParseDestIn(tx.vchSig, recordedDestIn, vchSig) || recordedDestIn != destIn)
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid recoreded destination\n");
        }
    }
    else
    {
        vchSig = tx.vchSig;
    }*/
    if (destIn.IsTemplate() && destIn.GetTemplateId().GetType() == TEMPLATE_PAYMENT)
    {
        auto templatePtr = CTemplate::CreateTemplatePtr(TEMPLATE_PAYMENT, tx.vchSig);
        if (templatePtr == nullptr)
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature vchSig err\n");
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
                return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature\n");
            }
        }
        else
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature\n");
        }
    }

    // locked coin template: nValueIn >= tx.nAmount + tx.nTxFee + nLockedCoin
    if (CTemplate::IsLockedCoin(destIn))
    {
        // TODO: No redemption temporarily
        return DEBUG(ERR_TRANSACTION_INVALID, "invalid locked coin template destination\n");
        // CTemplatePtr ptr = CTemplate::CreateTemplatePtr(destIn.GetTemplateId(), vchSig);
        // if (!ptr)
        // {
        //     return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid locked coin template destination\n");
        // }
        // int64 nLockedCoin = boost::dynamic_pointer_cast<CLockedCoinTemplate>(ptr)->LockedCoin(tx.sendTo, nForkHeight);
        // if (nValueIn < tx.nAmount + tx.nTxFee + nLockedCoin)
        // {
        //     return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein is not enough to locked coin (%ld : %ld)\n", nValueIn, tx.nAmount + tx.nTxFee + nLockedCoin);
        // }
    }

    if (tx.sendTo.GetTemplateId().GetType() == TEMPLATE_FORK && tx.nAmount < CTemplateFork::CreatedCoin())
    {
        throw DEBUG(ERR_TRANSACTION_INPUT_INVALID, "creating fork nAmount must be at least %ld", CTemplateFork::CreatedCoin());
    }

    if (!VerifyDestRecorded(tx, vchSig))
    {
        return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid recoreded destination\n");
    }

    if (!destIn.VerifyTxSignature(tx.GetSignatureHash(), tx.nType, /*tx.hashAnchor*/ GetGenesisBlockHash(), tx.sendTo, vchSig, nForkHeight, fork))
    {
        return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature\n");
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
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "input destination mismatched\n");
        }
        if (output.nTxTime > tx.nTimeStamp)
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "tx time is ahead of input tx\n");
        }
        if (output.IsLocked(nForkHeight))
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "input is still locked\n");
        }
        nValueIn += output.nAmount;
    }
    if (!MoneyRange(nValueIn))
    {
        return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein invalid %ld\n", nValueIn);
    }
    if (nValueIn < tx.nAmount + tx.nTxFee)
    {
        return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein is not enough (%ld : %ld)\n", nValueIn, tx.nAmount + tx.nTxFee);
    }

    if (tx.nType == CTransaction::TX_CERT)
    {
        if (VerifyCertTx(tx, destIn, fork) != OK)
        {
            return DEBUG(ERR_TRANSACTION_INVALID, "invalid cert tx");
        }
    }

    if (destIn.GetTemplateId().GetType() == TEMPLATE_VOTE || tx.sendTo.GetTemplateId().GetType() == TEMPLATE_VOTE)
    {
        if (VerifyVoteTx(tx, destIn, fork) != OK)
        {
            return DEBUG(ERR_TRANSACTION_INVALID, "invalid vote tx");
        }
    }

    // v1.0 function
    /*if (!tx.vchData.empty())
    {
        return DEBUG(ERR_TRANSACTION_INVALID, "vchData not empty\n");
    }*/

    // record destIn in vchSig
    vector<uint8> vchSig;
    /*if (CTemplate::IsDestInRecorded(tx.sendTo))
    {
        CDestination recordedDestIn;
        if (!CSendToRecordedTemplate::ParseDestIn(tx.vchSig, recordedDestIn, vchSig) || recordedDestIn != destIn)
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid recoreded destination\n");
        }
    }
    else
    {
        vchSig = tx.vchSig;
    }*/
    if (!VerifyDestRecorded(tx, vchSig))
    {
        return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid recoreded destination\n");
    }

    if (!destIn.VerifyTxSignature(tx.GetSignatureHash(), tx.nType, /*tx.hashAnchor*/ GetGenesisBlockHash(), tx.sendTo, vchSig, nForkHeight, fork))
    {
        return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature\n");
    }

    if (destIn.IsTemplate() && destIn.GetTemplateId().GetType() == TEMPLATE_PAYMENT)
    {
        auto templatePtr = CTemplate::CreateTemplatePtr(TEMPLATE_PAYMENT, tx.vchSig);
        if (templatePtr == nullptr)
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature vchSig err\n");
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
                return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature\n");
            }
        }
        else
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature\n");
        }
    }

    // locked coin template: nValueIn >= tx.nAmount + tx.nTxFee + nLockedCoin
    if (CTemplate::IsLockedCoin(destIn))
    {
        // TODO: No redemption temporarily
        return DEBUG(ERR_TRANSACTION_INVALID, "invalid locked coin template destination\n");
        // CTemplatePtr ptr = CTemplate::CreateTemplatePtr(destIn.GetTemplateId(), vchSig);
        // if (!ptr)
        // {
        //     return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid locked coin template destination\n");
        // }
        // int64 nLockedCoin = boost::dynamic_pointer_cast<CLockedCoinTemplate>(ptr)->LockedCoin(tx.sendTo, nForkHeight);
        // if (nValueIn < tx.nAmount + tx.nTxFee + nLockedCoin)
        // {
        //     return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein is not enough to locked coin (%ld : %ld)\n", nValueIn, tx.nAmount + tx.nTxFee + nLockedCoin);
        // }
    }

    if (tx.sendTo.GetTemplateId().GetType() == TEMPLATE_FORK && tx.nAmount < CTemplateFork::CreatedCoin())
    {
        throw DEBUG(ERR_TRANSACTION_INPUT_INVALID, "creating fork nAmount must be at least %ld", CTemplateFork::CreatedCoin());
    }

    return OK;
}

bool CCoreProtocol::GetBlockTrust(const CBlock& block, uint256& nChainTrust, const CBlockIndex* pIndexPrev, const CDelegateAgreement& agreement, const CBlockIndex* pIndexRef, size_t nEnrollTrust)
{
    if (block.IsGenesis())
    {
        nChainTrust = uint64(0);
    }
    else if (block.IsVacant())
    {
        nChainTrust = uint64(0);
    }
    else if (block.IsPrimary())
    {
        if (block.IsProofOfWork())
        {
            CProofOfHashWorkCompact proof;
            proof.Load(block.vchProof);
            uint256 nTarget;
            nTarget.SetCompact(proof.nBits);
            // nChainTrust = 2**256 / (nTarget+1) = ~nTarget / (nTarget+1) + 1
            nChainTrust = (~nTarget / (nTarget + 1)) + 1;
        }
        else if (pIndexPrev != nullptr)
        {
            // if (!IsDposHeight(block.GetBlockHeight()))
            // {
            //     StdError("CCoreProtocol", "GetBlockTrust: not dpos height, height: %d", block.GetBlockHeight());
            //     return false;
            // }

            // Get the last PoW block nAlgo
            int nAlgo;
            const CBlockIndex* pIndex = pIndexPrev;
            while (!pIndex->IsProofOfWork() && (pIndex->pPrev != nullptr))
            {
                pIndex = pIndex->pPrev;
            }
            if (!pIndex->IsProofOfWork())
            {
                nAlgo = CM_CRYPTONIGHT;
            }
            else
            {
                nAlgo = pIndex->nProofAlgo;
            }

            uint32_t nBits;
            int64 nReward;
            if (GetProofOfWorkTarget(pIndexPrev, nAlgo, nBits, nReward))
            {
                if (agreement.nWeight == 0 || nBits <= 0)
                {
                    StdError("CCoreProtocol", "GetBlockTrust: nWeight or nBits error, nWeight: %lu, nBits: %d", agreement.nWeight, nBits);
                    return false;
                }
                if (nEnrollTrust <= 0)
                {
                    StdError("CCoreProtocol", "GetBlockTrust: nEnrollTrust error, nEnrollTrust: %lu", nEnrollTrust);
                    return false;
                }
                uint256 nTarget;
                nTarget.SetCompact(nBits);
                // nChainTrust = nEnrollTrust * (2**256 / (nTarget+1) = ~nTarget / (nTarget+1) + 1)
                nChainTrust = (~nTarget / (nTarget + 1)) + 1;
                nChainTrust *= uint256((uint64)nEnrollTrust);
            }
            else
            {
                StdError("CCoreProtocol", "GetBlockTrust: GetProofOfWorkTarget fail");
                return false;
            }
        }
        else
        {
            StdError("CCoreProtocol", "GetBlockTrust: Primary pIndexPrev is null");
            return false;
        }
    }
    else if (block.IsOrigin())
    {
        nChainTrust = uint64(0);
    }
    else if (block.IsSubsidiary() || block.IsExtended())
    {
        if (pIndexRef == nullptr)
        {
            StdError("CCoreProtocol", "GetBlockTrust: pIndexRef is null, block: %s", block.GetHash().GetHex().c_str());
            return false;
        }
        if (pIndexRef->pPrev == nullptr)
        {
            StdError("CCoreProtocol", "GetBlockTrust: Subsidiary or Extended block pPrev is null, block: %s", block.GetHash().GetHex().c_str());
            return false;
        }
        nChainTrust = pIndexRef->nChainTrust - pIndexRef->pPrev->nChainTrust;
    }
    else
    {
        StdError("CCoreProtocol", "GetBlockTrust: block type error");
        return false;
    }
    return true;
}

bool CCoreProtocol::GetProofOfWorkTarget(const CBlockIndex* pIndexPrev, int nAlgo, uint32_t& nBits, int64& nReward)
{
    if (nAlgo <= 0 || nAlgo >= CM_MAX)
    {
        StdLog("CCoreProtocol", "GetProofOfWorkTarget: nAlgo error, nAlgo: %d", nAlgo);
        return false;
    }
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

bool CCoreProtocol::DPoSConsensusCheckRepeated(int height)
{
    return true;
}

int64 CCoreProtocol::GetPrimaryMintWorkReward(const CBlockIndex* pIndexPrev)
{
    return BBCP_INIT_REWARD_TOKEN * COIN;
}

void CCoreProtocol::GetDelegatedBallot(const uint256& nAgreement, size_t nWeight, const map<CDestination, size_t>& mapBallot,
                                       const vector<pair<CDestination, int64>>& vecAmount, int64 nMoneySupply, vector<CDestination>& vBallot, size_t& nEnrollTrust, int nBlockHeight)
{
}

int64 CCoreProtocol::MinEnrollAmount()
{
    return 0;
}

uint32 CCoreProtocol::DPoSTimestamp(const CBlockIndex* pIndexPrev)
{
    if (pIndexPrev == nullptr || !pIndexPrev->IsPrimary())
    {
        return 0;
    }
    return pIndexPrev->GetBlockTime() + BLOCK_TARGET_SPACING;
}

uint32 CCoreProtocol::GetNextBlockTimeStamp(uint16 nPrevMintType, uint32 nPrevTimeStamp, uint16 nTargetMintType, int nTargetHeight)
{
    if (nPrevMintType == CTransaction::TX_WORK || nPrevMintType == CTransaction::TX_GENESIS)
    {
        if (nTargetMintType == CTransaction::TX_STAKE)
        {
            return nPrevTimeStamp + BLOCK_TARGET_SPACING;
        }
        return nPrevTimeStamp + PROOF_OF_WORK_BLOCK_SPACING;
    }
    return nPrevTimeStamp + BLOCK_TARGET_SPACING;
}

bool CCoreProtocol::CheckBlockSignature(const CBlock& block)
{
    if (block.GetHash() != GetGenesisBlockHash())
    {
        return block.txMint.sendTo.VerifyBlockSignature(block.GetHash(), block.vchSig);
    }
    return true;
}

Errno CCoreProtocol::ValidateVacantBlock(const CBlock& block)
{
    if (block.hashMerkle != 0 || block.txMint != CTransaction() || !block.vtx.empty())
    {
        return DEBUG(ERR_BLOCK_TRANSACTIONS_INVALID, "vacant block tx is not empty.");
    }

    if (!block.vchProof.empty() || !block.vchSig.empty())
    {
        return DEBUG(ERR_BLOCK_SIGNATURE_INVALID, "vacant block proof or signature is not empty.");
    }

    return OK;
}

bool CCoreProtocol::VerifyDestRecorded(const CTransaction& tx, vector<uint8>& vchSigOut)
{
    if (CTemplate::IsDestInRecorded(tx.sendTo))
    {
        CDestination sendToDelegateTemplate;
        CDestination sendToOwner;
        if (!CSendToRecordedTemplate::ParseDest(tx.vchSig, sendToDelegateTemplate, sendToOwner, vchSigOut))
        {
            StdError("Core", "Verify dest recorded: Parse dest fail, txid: %s", tx.GetHash().GetHex().c_str());
            return false;
        }
        if (sendToDelegateTemplate.IsNull() || sendToOwner.IsNull())
        {
            StdError("Core", "Verify dest recorded: sendTo dest is null, txid: %s", tx.GetHash().GetHex().c_str());
            return false;
        }
        CTemplateId tid;
        if (!(tx.sendTo.GetTemplateId(tid) && tid.GetType() == TEMPLATE_VOTE))
        {
            StdError("Core", "Verify dest recorded: sendTo not is template, txid: %s, sendTo: %s", tx.GetHash().GetHex().c_str(), CAddress(tx.sendTo).ToString().c_str());
            return false;
        }
        CTemplatePtr ptr = CTemplate::CreateTemplatePtr(new CTemplateVote(sendToDelegateTemplate, sendToOwner));
        if (ptr == nullptr)
        {
            StdError("Core", "Verify dest recorded: sendTo CreateTemplatePtr fail, txid: %s, sendTo: %s, delegate dest: %s, owner dest: %s",
                     tx.GetHash().GetHex().c_str(), CAddress(tx.sendTo).ToString().c_str(),
                     CAddress(sendToDelegateTemplate).ToString().c_str(), CAddress(sendToOwner).ToString().c_str());
            return false;
        }
        if (ptr->GetTemplateId() != tx.sendTo.GetTemplateId())
        {
            StdError("Core", "Verify dest recorded: sendTo error, txid: %s, sendTo: %s, delegate dest: %s, owner dest: %s",
                     tx.GetHash().GetHex().c_str(), CAddress(tx.sendTo).ToString().c_str(),
                     CAddress(sendToDelegateTemplate).ToString().c_str(), CAddress(sendToOwner).ToString().c_str());
            return false;
        }
    }
    else
    {
        vchSigOut = tx.vchSig;
    }
    return true;
}

Errno CCoreProtocol::VerifyCertTx(const CTransaction& tx, const CDestination& destIn, const uint256& fork)
{
    // CERT transaction must be on the main chain
    if (fork != GetGenesisBlockHash())
    {
        Log("VerifyCertTx CERT tx is not on the main chain, fork: %s", fork.ToString().c_str());
        return ERR_TRANSACTION_INVALID;
    }
    // the `from` address must be equal to the `to` address of cert tx
    if (destIn != tx.sendTo)
    {
        Log("VerifyCertTx the `from` address is not equal the `to` address of CERT tx, from: %s, to: %s\n",
            CAddress(destIn).ToString().c_str(), CAddress(tx.sendTo).ToString().c_str());
        return ERR_TRANSACTION_INVALID;
    }
    // the `to` address must be delegate template address
    if (tx.sendTo.GetTemplateId().GetType() != TEMPLATE_DELEGATE)
    {
        Log("VerifyCertTx the `to` address of CERT tx is not a delegate template address, to: %s\n", CAddress(tx.sendTo).ToString().c_str());
        return ERR_TRANSACTION_INVALID;
    }

    return OK;
}

Errno CCoreProtocol::VerifyVoteTx(const CTransaction& tx, const CDestination& destIn, const uint256& fork)
{
    // VOTE transaction must be on the main chain
    if (fork != GetGenesisBlockHash())
    {
        Log("VerifyVoteTx from or to vote template address tx is not on the main chain, fork: %s", fork.ToString().c_str());
        return ERR_TRANSACTION_INVALID;
    }

    return OK;
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

    block.nVersion = 1;
    block.nType = CBlock::BLOCK_GENESIS;
    block.nTimeStamp = 1575043200;
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

bool CProofOfWorkParam::DPoSConsensusCheckRepeated(int height)
{
    return true;
}

} // namespace bigbang
