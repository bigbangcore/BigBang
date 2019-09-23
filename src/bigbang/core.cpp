// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "core.h"

#include "../common/template/exchange.h"
#include "address.h"
#include "wallet.h"

using namespace std;
using namespace xengine;

#define DEBUG(err, ...) Debug((err), __FUNCTION__, __VA_ARGS__)

#define BBCP_SET_TOKEN_DISTRIBUTION

static const int64 MAX_CLOCK_DRIFT = 10 * 60;

static const int PROOF_OF_WORK_BITS_LIMIT = 8;
static const int PROOF_OF_WORK_BITS_INIT = 10;
static const int PROOF_OF_WORK_ADJUST_COUNT = 8;
static const int PROOF_OF_WORK_ADJUST_DEBOUNCE = 10;
static const int PROOF_OF_WORK_TARGET_SPACING = BLOCK_TARGET_SPACING; // + BLOCK_TARGET_SPACING / 2;

static const int64 BBCP_TOKEN_INIT = 300000000;
#ifndef BBCP_SET_TOKEN_DISTRIBUTION
static const int64 BBCP_BASE_REWARD_TOKEN_COIN = 15 * COIN;
#else
static const int64 BBCP_YEAR_INC_REWARD_TOKEN_COIN = 20 * COIN; //19025875; //1% : (1000000000*0.010000)/(60*24*365)*1000000=19,025875.190258751902587519025875
static const int64 BBCP_SPECIAL_HEIGHT_SECT = 10080;            //60*24*30*3;

static const int64 BBCP_END_HEIGHT[7] = {
    10080, //129600,         //CPOW             : 60*24*30*3=129600
    20160, //367200,         //CPOW_EDPOS_1_085 : 129600+60*24*30*5.5=367200
    30240, //604800,         //CPOW_EDPOS_2_140 : 367200+60*24*30*5.5=604800
    40320, //842400,         //CPOW_EDPOS_3_195 : 604800+60*24*30*5.5=842400
    50400, //1080000,        //CPOW_EDPOS_4_250 : 842400+60*24*30*5.5=1080000
    60480, //1317600,        //CPOW_EDPOS_5_305 : 1080000+60*24*30*5.5=1317600
    70560  //1555200         //CPOW_EDPOS_6_360 : 1317600+60*24*30*5.5=1555200
};
static const int64 BBCP_REWARD_TOKEN_COIN[7] = {
    10000000000, //925925925,  //CPOW             : (1000000000*0.1200)/(60*24*30*3.0)*1000000=925,925925.92592592592592592592593
    30000000000, //1132996632, //CPOW_EDPOS_1_085 : (1000000000*0.2692)/(60*24*30*5.5)*1000000=1132,996632.9966329966329966329966
    15000000000, //566498316,  //CPOW_EDPOS_2_140 : (1000000000*0.1346)/(60*24*30*5.5)*1000000=566,498316.49831649831649831649832
    7500000000,  //283249158,  //CPOW_EDPOS_3_195 : (1000000000*0.0673)/(60*24*30*5.5)*1000000=283,249158.24915824915824915824916
    3750000000,  //141835016,  //CPOW_EDPOS_4_250 : (1000000000*0.0337)/(60*24*30*5.5)*1000000=141,835016.83501683501683501683502
    1875000000,  //70707070,   //CPOW_EDPOS_5_305 : (1000000000*0.0168)/(60*24*30*5.5)*1000000=70,707070.707070707070707070707071
    937000000    //35353535    //CPOW_EDPOS_6_360 : (1000000000*0.0084)/(60*24*30*5.5)*1000000=35,353535.353535353535353535353535
};
static const int64 BBCP_SPECIAL_FOUNDATION_TECH_TOKEN_COIN[10] = {
    20000000000000, //2%      : (1000000000*0.020000)*1000000=20000000,000000
    15000000000000, //1.5%    : (1000000000*0.015000)*1000000=15000000,000000
    10000000000000, //1%      : (1000000000*0.010000)*1000000=10000000,000000
    7857000000000,  //0.7857% : (1000000000*0.007857)*1000000=7857000,000000
    7857000000000,  //0.7857% : (1000000000*0.007857)*1000000=7857000,000000
    7857000000000,  //0.7857% : (1000000000*0.007857)*1000000=7857000,000000
    7857000000000,  //0.7857% : (1000000000*0.007857)*1000000=7857000,000000
    7857000000000,  //0.7857% : (1000000000*0.007857)*1000000=7857000,000000
    7857000000000,  //0.7857% : (1000000000*0.007857)*1000000=7857000,000000
    7857000000000   //0.7857% : (1000000000*0.007857)*1000000=7857000,000000
};
static const int64 BBCP_SPECIAL_MARKET_TOKEN_COIN[10] = {
    15000000000000, //1.5%    : (1000000000*0.015000)*1000000=15000000,000000
    10000000000000, //1%      : (1000000000*0.010000)*1000000=10000000,000000
    8000000000000,  //0.8%    : (1000000000*0.008000)*1000000=8000000,000000
    5285000000000,  //0.5285% : (1000000000*0.005285)*1000000=5285000,000000
    5285000000000,  //0.5285% : (1000000000*0.005285)*1000000=5285000,000000
    5285000000000,  //0.5285% : (1000000000*0.005285)*1000000=5285000,000000
    5285000000000,  //0.5285% : (1000000000*0.005285)*1000000=5285000,000000
    5285000000000,  //0.5285% : (1000000000*0.005285)*1000000=5285000,000000
    5285000000000,  //0.5285% : (1000000000*0.005285)*1000000=5285000,000000
    5285000000000   //0.5285% : (1000000000*0.005285)*1000000=5285000,000000
};
static const int64 BBCP_SPECIAL_INSTITUTION_TOKEN_COIN = 20000000000000; //2% : (1000000000*0.020000)*1000000=20000000,000000

static const std::string BBCP_SPECIAL_REWARD_TEMPLATE_ADDRESS[4] = {
    "20g053vhn4ygv9m8pzhevnjvtgbbqhgs66qv31ez39v9xbxvk0ynqmeyf", //Foundation
    "20g00c7gng4w63kjt8r9n6bpfmea7crw7pmkv9rshxf6d6vwhq9bdfy8k", //Technical team
    "20g0997062yn5vzk5ahjmfkyt2bjq8s6dt5xqynjq71k0rxcrbfcfzdsa", //Market operation
    "20g0d0p9gaynwq6k815kj4n8w8ttydcf80m67sjcp3zsjymjfb710sjgg"  //Institution
};
#endif

namespace bigbang
{
///////////////////////////////
// CCoreProtocol

CCoreProtocol::CCoreProtocol()
{
    nProofOfWorkLimit = PROOF_OF_WORK_BITS_LIMIT;
    nProofOfWorkInit = PROOF_OF_WORK_BITS_INIT;
    nProofOfWorkUpperTarget = PROOF_OF_WORK_TARGET_SPACING + PROOF_OF_WORK_ADJUST_DEBOUNCE;
    nProofOfWorkLowerTarget = PROOF_OF_WORK_TARGET_SPACING - PROOF_OF_WORK_ADJUST_DEBOUNCE;
}

CCoreProtocol::~CCoreProtocol()
{
}

bool CCoreProtocol::HandleInitialize()
{
    CBlock block;
    GetGenesisBlock(block);
    hashGenesisBlock = block.GetHash();
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
    block.nTimeStamp = 1515745156;
    block.hashPrev = 0;

    CTransaction& tx = block.txMint;
    tx.nType = CTransaction::TX_GENESIS;
    tx.nTimeStamp = block.nTimeStamp;
    tx.sendTo = destOwner;
    tx.nAmount = BBCP_TOKEN_INIT * COIN; // initial number of token

    CProfile profile;
    profile.strName = "BigBang Network";
    profile.strSymbol = "BIG";
    profile.destOwner = destOwner;
    profile.nAmount = tx.nAmount;
    profile.nMintReward = 15 * COIN;
    profile.nMinTxFee = MIN_TX_FEE;
    profile.nHalveCycle = 0;
    profile.SetFlag(true, false, false);

    profile.Save(block.vchProof);
}

Errno CCoreProtocol::ValidateTransaction(const CTransaction& tx)
{
    // Basic checks that don't depend on any context
    if (tx.nType == CTransaction::TX_TOKEN
        && (tx.sendTo.IsPubKey()
            || (tx.sendTo.IsTemplate()
                && (tx.sendTo.GetTemplateId() == TEMPLATE_WEIGHTED || tx.sendTo.GetTemplateId() == TEMPLATE_MULTISIG)))
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
        || (tx.nType == CTransaction::TX_TOKEN && tx.nTxFee < MIN_TX_FEE))
    {
        return DEBUG(ERR_TRANSACTION_OUTPUT_INVALID, "txfee invalid %ld", tx.nTxFee);
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
    // Check timestamp
    if (block.GetBlockTime() > GetNetTime() + MAX_CLOCK_DRIFT)
    {
        return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "%ld\n", block.GetBlockTime());
    }

    // Extended block should be not empty
    if (block.nType == CBlock::BLOCK_EXTENDED && block.vtx.empty())
    {
        return DEBUG(ERR_BLOCK_TRANSACTIONS_INVALID, "empty extended block\n");
    }

    // validate vacant block
    if (block.nType == CBlock::BLOCK_VACANT)
    {
        return ValidateVacantBlock(block);
    }

    // Validate mint tx
    if (!block.txMint.IsMintTx() || ValidateTransaction(block.txMint) != OK)
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
        if (tx.IsMintTx() || ValidateTransaction(tx) != OK)
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
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "vchProof size error.\n");
    }

    if (block.GetBlockTime() < pIndexPrev->GetBlockTime())
    {
        return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "Timestamp out of range.\n");
    }

    CProofOfHashWorkCompact proof;
    proof.Load(block.vchProof);

    int nBits = 0;
    int64 nReward = 0;
    if (!GetProofOfWorkTarget(pIndexPrev, proof.nAlgo, nBits, nReward))
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "get target fail.\n");
    }

    if (nBits != proof.nBits || proof.nAlgo != CM_CRYPTONIGHT)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "algo or bits error, nAlgo: %d, nBits: %d, vchProof size: %ld.\n", proof.nAlgo, proof.nBits, block.vchProof.size());
    }
    if (proof.destMint != block.txMint.sendTo)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "destMint error, destMint: %s.\n", proof.destMint.ToString().c_str());
    }
    if ((proof.nNonce & 0xff) != 0)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "nNonce error, nNonce: %ld.\n", proof.nNonce);
    }

    uint256 hashTarget = (~uint256(uint64(0)) >> GetProofOfWorkRunTimeBits(nBits, block.GetBlockTime(), pIndexPrev->GetBlockTime()));

    vector<unsigned char> vchProofOfWork;
    block.GetSerializedProofOfWorkData(vchProofOfWork);
    uint256 hash = crypto::CryptoPowHash(&vchProofOfWork[0], vchProofOfWork.size());

    if (hash > hashTarget)
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_WORK_INVALID, "hash error.\n");
    }

    return OK;
}

Errno CCoreProtocol::VerifyDelegatedProofOfStake(const CBlock& block, const CBlockIndex* pIndexPrev,
                                                 const CDelegateAgreement& agreement)
{
    if (block.GetBlockTime() < pIndexPrev->GetBlockTime() + BLOCK_TARGET_SPACING
        || block.GetBlockTime() >= pIndexPrev->GetBlockTime() + BLOCK_TARGET_SPACING * 3 / 2)
    {
        return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "Timestamp out of range.\n");
    }

    if (block.txMint.sendTo != agreement.vBallot[0])
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_STAKE_INVALID, "txMint sendTo error.\n");
    }

    return OK;
}

Errno CCoreProtocol::VerifySubsidiary(const CBlock& block, const CBlockIndex* pIndexPrev, const CBlockIndex* pIndexRef,
                                      const CDelegateAgreement& agreement)
{
    if (block.GetBlockTime() < pIndexPrev->GetBlockTime())
    {
        return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "Timestamp out of range.\n");
    }

    if (!block.IsExtended())
    {
        if (block.GetBlockTime() != pIndexRef->GetBlockTime())
        {
            return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "Timestamp out of range.\n");
        }
    }
    else
    {
        if (block.GetBlockTime() <= pIndexRef->GetBlockTime()
            || block.GetBlockTime() >= pIndexRef->GetBlockTime() + BLOCK_TARGET_SPACING)
        {
            return DEBUG(ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE, "Timestamp out of range.\n");
        }
    }

    int nIndex = (block.GetBlockTime() - pIndexRef->GetBlockTime()) / EXTENDED_BLOCK_SPACING;
    if (block.txMint.sendTo != agreement.GetBallot(nIndex))
    {
        return DEBUG(ERR_BLOCK_PROOF_OF_STAKE_INVALID, "txMint sendTo error.\n");
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

    vector<uint8> vchSig;
    if (CTemplate::IsDestInRecorded(tx.sendTo))
    {
        CDestination recordedDestIn;
        if (!CDestInRecordedTemplate::ParseDestIn(tx.vchSig, recordedDestIn, vchSig) || recordedDestIn != destIn)
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid recoreded destination\n");
        }
    }
    else
    {
        vchSig = tx.vchSig;
    }
    if (!destIn.VerifyTxSignature(tx.GetSignatureHash(), tx.hashAnchor, tx.sendTo, vchSig, nForkHeight, fork))
    {
        return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature\n");
    }
    return OK;
}

Errno CCoreProtocol::VerifyTransaction(const CTransaction& tx, const vector<CTxOut>& vPrevOutput, int nForkHeight, const uint256& fork)
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
        if (output.nLockUntil != 0 && output.nLockUntil < nForkHeight)
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

    // record destIn in vchSig
    vector<uint8> vchSig;
    if (CTemplate::IsDestInRecorded(tx.sendTo))
    {
        CDestination recordedDestIn;
        if (!CDestInRecordedTemplate::ParseDestIn(tx.vchSig, recordedDestIn, vchSig) || recordedDestIn != destIn)
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid recoreded destination\n");
        }
    }
    else
    {
        vchSig = tx.vchSig;
    }

    if (!destIn.VerifyTxSignature(tx.GetSignatureHash(), tx.hashAnchor, tx.sendTo, vchSig, nForkHeight, fork))
    {
        return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid signature\n");
    }

    // locked coin template: nValueIn >= tx.nAmount + tx.nTxFee + nLockedCoin
    if (CTemplate::IsLockedCoin(destIn))
    {
        CTemplatePtr ptr = CTemplate::CreateTemplatePtr(destIn.GetTemplateId(), vchSig);
        if (!ptr)
        {
            return DEBUG(ERR_TRANSACTION_SIGNATURE_INVALID, "invalid locked coin template destination\n");
        }
        int64 nLockedCoin = boost::dynamic_pointer_cast<CLockedCoinTemplate>(ptr)->LockedCoin(tx.sendTo, nForkHeight);
        if (nValueIn < tx.nAmount + tx.nTxFee + nLockedCoin)
        {
            return DEBUG(ERR_TRANSACTION_INPUT_INVALID, "valuein is not enough to locked coin (%ld : %ld)\n", nValueIn, tx.nAmount + tx.nTxFee + nLockedCoin);
        }
    }
    return OK;
}

bool CCoreProtocol::GetProofOfWorkTarget(const CBlockIndex* pIndexPrev, int nAlgo, int& nBits, int64& nReward)
{
    if (nAlgo <= 0 || nAlgo >= CM_MAX || !pIndexPrev->IsPrimary())
    {
        return false;
    }
    nReward = GetPrimaryMintWorkReward(pIndexPrev);

    const CBlockIndex* pIndex = pIndexPrev;
    while ((!pIndex->IsProofOfWork() || pIndex->nProofAlgo != nAlgo) && pIndex->pPrev != nullptr)
    {
        pIndex = pIndex->pPrev;
    }

    // first
    if (!pIndex->IsProofOfWork())
    {
        nBits = nProofOfWorkInit;
        return true;
    }

    nBits = pIndex->nProofBits;
    int64 nSpacing = 0;
    int64 nWeight = 0;
    int nWIndex = PROOF_OF_WORK_ADJUST_COUNT - 1;
    while (pIndex->IsProofOfWork())
    {
        nSpacing += (pIndex->GetBlockTime() - pIndex->pPrev->GetBlockTime()) << nWIndex;
        nWeight += (1ULL) << nWIndex;
        if (!nWIndex--)
        {
            break;
        }
        pIndex = pIndex->pPrev;
        while ((!pIndex->IsProofOfWork() || pIndex->nProofAlgo != nAlgo) && pIndex->pPrev != nullptr)
        {
            pIndex = pIndex->pPrev;
        }
    }
    nSpacing /= nWeight;
    if (nSpacing > nProofOfWorkUpperTarget && nBits > nProofOfWorkLimit)
    {
        nBits--;
    }
    else if (nSpacing < nProofOfWorkLowerTarget)
    {
        nBits++;
    }
    return true;
}

int CCoreProtocol::GetProofOfWorkRunTimeBits(int nBits, int64 nTime, int64 nPrevTime)
{
    if (nTime - nPrevTime < BLOCK_TARGET_SPACING)
    {
        return (nBits + 1);
    }

    nBits -= (nTime - nPrevTime - BLOCK_TARGET_SPACING) / PROOF_OF_WORK_DECAY_STEP;
    if (nBits < nProofOfWorkLimit)
    {
        nBits = nProofOfWorkLimit;
    }
    return nBits;
}

int64 CCoreProtocol::GetPrimaryMintWorkReward(const CBlockIndex* pIndexPrev)
{
#ifdef BBCP_SET_TOKEN_DISTRIBUTION
    int nBlockHeight = pIndexPrev->GetBlockHeight() + 1;
    int nSpecialIndex = (nBlockHeight - 1) % BBCP_SPECIAL_HEIGHT_SECT;
    int nSpecialSect = (nBlockHeight - 1) / BBCP_SPECIAL_HEIGHT_SECT;

    if (nSpecialSect >= 1 && nSpecialSect <= 10)
    {
        if (nSpecialIndex == 0 || nSpecialIndex == 1)
        {

            return BBCP_SPECIAL_FOUNDATION_TECH_TOKEN_COIN[nSpecialSect - 1];
        }
        else if (nSpecialIndex == 2)
        {
            return BBCP_SPECIAL_MARKET_TOKEN_COIN[nSpecialSect - 1];
        }
        else if (nSpecialIndex == 3 && nSpecialSect <= 4)
        {
            return BBCP_SPECIAL_INSTITUTION_TOKEN_COIN;
        }
    }

    for (int i = 0; i < 7; i++)
    {
        if (nBlockHeight <= BBCP_END_HEIGHT[i])
        {
            return BBCP_REWARD_TOKEN_COIN[i];
        }
    }

    return BBCP_YEAR_INC_REWARD_TOKEN_COIN;
#else
    return BBCP_BASE_REWARD_TOKEN_COIN;
#endif
}

bool CCoreProtocol::CheckFirstPow(int nBlockHeight)
{
#ifdef BBCP_SET_TOKEN_DISTRIBUTION
    if (nBlockHeight <= BBCP_END_HEIGHT[0])
    {
        return true;
    }
#endif
    return false;
}

bool CCoreProtocol::CheckSpecialHeight(int nBlockHeight)
{
#ifdef BBCP_SET_TOKEN_DISTRIBUTION
    int nSpecialIndex = (nBlockHeight - 1) % BBCP_SPECIAL_HEIGHT_SECT;
    int nSpecialSect = (nBlockHeight - 1) / BBCP_SPECIAL_HEIGHT_SECT;

    if ((nSpecialIndex < 4 && (nSpecialSect >= 1 && nSpecialSect <= 10)) && (nSpecialIndex != 3 || nSpecialSect <= 4))
    {
        return true;
    }
#endif
    return false;
}

bool CCoreProtocol::VerifySpecialAddress(int nBlockHeight, const CBlock& block)
{
#ifdef BBCP_SET_TOKEN_DISTRIBUTION
    int nSpecialIndex = (nBlockHeight - 1) % BBCP_SPECIAL_HEIGHT_SECT;
    int nSpecialSect = (nBlockHeight - 1) / BBCP_SPECIAL_HEIGHT_SECT;

    if ((nSpecialIndex < 4 && (nSpecialSect >= 1 && nSpecialSect <= 10)) && (nSpecialIndex != 3 || nSpecialSect <= 4))
    {
        CAddress destSpecial(BBCP_SPECIAL_REWARD_TEMPLATE_ADDRESS[nSpecialIndex]);
        if (block.txMint.sendTo != destSpecial)
        {
            return false;
        }
    }
#endif
    return true;
}

void CCoreProtocol::GetDelegatedBallot(const uint256& nAgreement, size_t nWeight,
                                       const map<CDestination, size_t>& mapBallot, vector<CDestination>& vBallot, int nBlockHeight)
{
    vBallot.clear();

    if (CheckFirstPow(nBlockHeight) || CheckSpecialHeight(nBlockHeight))
    {
        return;
    }

    int nSelected = 0;
    for (const unsigned char* p = nAgreement.begin(); p != nAgreement.end(); ++p)
    {
        nSelected ^= *p;
    }
    size_t nWeightWork = ((DELEGATE_THRESH - nWeight) * (DELEGATE_THRESH - nWeight) * (DELEGATE_THRESH - nWeight))
                         / (DELEGATE_THRESH * DELEGATE_THRESH);
    if (nSelected >= nWeightWork * 256 / (nWeightWork + nWeight))
    {
        size_t nTrust = nWeight;
        for (const unsigned char* p = nAgreement.begin(); p != nAgreement.end() && nTrust != 0; ++p)
        {
            nSelected += *p;
            size_t n = nSelected % nWeight;
            for (map<CDestination, size_t>::const_iterator it = mapBallot.begin(); it != mapBallot.end(); ++it)
            {
                if (n < (*it).second)
                {
                    vBallot.push_back((*it).first);
                    break;
                }
                n -= (*it).second;
            }
            nTrust >>= 1;
        }
    }
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

///////////////////////////////
// CTestNetCoreProtocol

CTestNetCoreProtocol::CTestNetCoreProtocol()
{
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
    const CDestination destOwner = CDestination(bigbang::crypto::CPubKey(uint256("62a401ff58234d2c7543859ed4917a9e85fb431ffb650a4d7694a301234771c4")));

    block.SetNull();

    block.nVersion = 1;
    block.nType = CBlock::BLOCK_GENESIS;
    block.nTimeStamp = (ptime(date(2019, 9, 23), time_duration(15 - 8, 0, 0)) - ptime(date(1970, 1, 1))).total_seconds();
    block.hashPrev = 0;

    CTransaction& tx = block.txMint;
    tx.nType = CTransaction::TX_GENESIS;
    tx.nTimeStamp = block.nTimeStamp;
    tx.sendTo = destOwner;
    tx.nAmount = BBCP_TOKEN_INIT * COIN; // initial number of token

    CProfile profile;
    profile.strName = "BigBang Test Network";
    profile.strSymbol = "BigTest";
    profile.destOwner = destOwner;
    profile.nAmount = tx.nAmount;
    profile.nMintReward = 15 * COIN;
    profile.nMinTxFee = MIN_TX_FEE;
    profile.nHalveCycle = 0;
    profile.SetFlag(true, false, false);

    profile.Save(block.vchProof);
}

} // namespace bigbang
