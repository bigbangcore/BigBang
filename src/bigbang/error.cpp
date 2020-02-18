// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "error.h"

using namespace bigbang;

static const char* _ErrorFailed = "operation failed";
static const char* _ErrorUnknown = "unkown error";
static const char* _ErrorString[] = {
    // OK,
    "",
    // ERR_UNAVAILABLE,
    "unavailable",
    /* container */
    //ERR_NOT_FOUND,
    "not found",
    //ERR_ALREADY_HAVE,
    "already have",
    //ERR_MISSING_PREV,
    "missing previous",
    /* system */
    //ERR_SYS_DATABASE_ERROR,
    "database error",
    //ERR_SYS_OUT_OF_DISK_SPACE,
    "out of disk space",
    //ERR_SYS_STORAGE_ERROR,
    "storage error",
    //ERR_SYS_OUT_OF_MEMORY,
    "out of memory",
    /* block */
    //ERR_BLOCK_TYPE_INVALID,
    "block type is invalid",
    //ERR_BLOCK_OVERSIZE,
    "block oversize",
    //ERR_BLOCK_PROOF_OF_WORK_INVALID,
    "block proof-of-work is invalid",
    //ERR_BLOCK_PROOF_OF_STAKE_INVALID,
    "block proof-of-stake is invalid",
    //ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE,
    "block timestamp is out of range",
    //ERR_BLOCK_COINBASE_INVALID,
    "block coinbase is invalid",
    //ERR_BLOCK_COINSTAKE_INVALID,
    "block coinstake is invalid",
    //ERR_BLOCK_TRANSACTIONS_INVALID,
    "block transactions are invalid",
    //ERR_BLOCK_DUPLICATED_TRANSACTION,
    "block contain duplicated transaction",
    //ERR_BLOCK_SIGOPCOUNT_OUT_OF_BOUND,
    "block sigopt is out of bound",
    //ERR_BLOCK_TXHASH_MISMATCH,
    "block txid is mismatch",
    //ERR_BLOCK_SIGNATURE_INVALID,
    "block signature is invalid",
    //ERR_BLOCK_INVALID_FORK,
    "block found invalid fork",
    //ERR_BLOCK_CERTTX_OUT_OF_BOUND
    "cert transaction quantity out of bounds",
    /* transaction */
    //ERR_TRANSACTION_INVALID,
    "transaction invalid",
    //ERR_TRANSACTION_OVERSIZE,
    "transaction oversize",
    //ERR_TRANSACTION_OUTPUT_INVALID,
    "transaction outputs are invalid",
    //ERR_TRANSACTION_INPUT_INVALID,
    "transaction inputs are invalid",
    //ERR_TRANSACTION_TIMESTAMP_INVALID,
    "transaction timestamp is invalid",
    //ERR_TRANSACTION_NOT_ENOUGH_FEE,
    "transaction fee is not enough",
    //ERR_TRANSACTION_STAKE_REWARD_INVALID,
    "transaction stake reward is invalid",
    //ERR_TRANSACTION_SIGNATURE_INVALID,
    "transaction signature is invalid",
    //ERR_TRANSACTION_CONFLICTING_INPUT,
    "transaction inputs are conflicting",
    /* wallet */
    //ERR_WALLET_INVALID_AMOUNT,
    "wallet amount is invalid",
    //ERR_WALLET_INSUFFICIENT_FUNDS,
    "wallet funds is insufficient",
    //ERR_WALLET_SIGNATURE_FAILED,
    "wallet failed to signature",
    //ERR_WALLET_TX_OVERSIZE,
    "wallet transaction is oversize",
    //ERR_WALLET_NOT_FOUND,
    "wallet is missing",
    //ERR_WALLET_IS_LOCKED,
    "wallet is locked",
    //ERR_WALLET_IS_UNLOCKED,
    "wallet is unlocked",
    //ERR_WALLET_IS_ENCRYPTED,
    "wallet is encrypted",
    //ERR_WALLET_IS_UNENCRYPTED,
    "wallet is unencrypted",
    //ERR_WALLET_FAILED,
    "wallet operation is failed"
};

const char* bigbang::ErrorString(const Errno& err)
{
    if (err < OK)
    {
        return _ErrorFailed;
    }
    else if (err >= ERR_MAX_COUNT)
    {
        return _ErrorUnknown;
    }
    return _ErrorString[err];
}
