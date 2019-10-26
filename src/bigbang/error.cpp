// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "error.h"

using namespace bigbang;

static const char* _ErrorFailed = "operation failed";
static const char* _ErrorUnknown = "unkown error";
static const char* _ErrorString[] = {
    // OK = 0,
    "",
    // ERR_UNAVAILABLE,                           // 1
    "unavailable",
    /* container */
    //ERR_NOT_FOUND,                              // 2
    "not found",
    //ERR_ALREADY_HAVE,                           // 3
    "already have",
    //ERR_MISSING_PREV,                           // 4
    "missing previous",
    /* system */
    //ERR_SYS_DATABASE_ERROR,                     // 5
    "database error",
    //ERR_SYS_OUT_OF_DISK_SPACE,                  // 6
    "out of disk space",
    //ERR_SYS_STORAGE_ERROR,                      // 7
    "storage error",
    //ERR_SYS_OUT_OF_MEMORY,                      // 8
    "out of memory",
    /* block */
    //ERR_BLOCK_OVERSIZE,                         // 9
    "block oversize",
    //ERR_BLOCK_PROOF_OF_WORK_INVALID,            // 10
    "block proof-of-work is invalid",
    //ERR_BLOCK_PROOF_OF_STAKE_INVALID,           // 11
    "block proof-of-stake is invalid",
    //ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE,           // 12
    "block timestamp is out of range",
    //ERR_BLOCK_HEIGHT_INVALID,                   // 13
    "block height is error",
    //ERR_BLOCK_COINBASE_INVALID,                 // 14
    "block coinbase is invalid",
    //ERR_BLOCK_COINSTAKE_INVALID,                // 15
    "block coinstake is invalid",
    //ERR_BLOCK_TRANSACTIONS_INVALID,             // 16
    "block transactions are invalid",
    //ERR_BLOCK_DUPLICATED_TRANSACTION,           // 17
    "block contain duplicated transaction",
    //ERR_BLOCK_SIGOPCOUNT_OUT_OF_BOUND,          // 18
    "block sigopt is out of bound",
    //ERR_BLOCK_TXHASH_MISMATCH,                  // 19
    "block txid is mismatch",
    //ERR_BLOCK_SIGNATURE_INVALID,                // 20
    "block signature is invalid",
    //ERR_BLOCK_INVALID_FORK,                     // 21
    "block found invalid fork",
    /* transaction */
    //ERR_TRANSACTION_INVALID,                    // 22
    "transaction invalid",
    //ERR_TRANSACTION_OVERSIZE,                   // 23
    "transaction oversize",
    //ERR_TRANSACTION_OUTPUT_INVALID,             // 24
    "transaction outputs are invalid",
    //ERR_TRANSACTION_INPUT_INVALID,              // 25
    "transaction inputs are invalid",
    //ERR_TRANSACTION_TIMESTAMP_INVALID,          // 26
    "transaction timestamp is invalid",
    //ERR_TRANSACTION_NOT_ENOUGH_FEE,             // 27
    "transaction fee is not enough",
    //ERR_TRANSACTION_STAKE_REWARD_INVALID,       // 28
    "transaction stake reward is invalid",
    //ERR_TRANSACTION_SIGNATURE_INVALID,          // 29
    "transaction signature is invalid",
    //ERR_TRANSACTION_CONFLICTING_INPUT,          // 30
    "transaction inputs are conflicting",
    /* wallet */
    //ERR_WALLET_INVALID_AMOUNT,                  // 31
    "wallet amount is invalid",
    //ERR_WALLET_INSUFFICIENT_FUNDS,              // 32
    "wallet funds is insufficient",
    //ERR_WALLET_SIGNATURE_FAILED,                // 33
    "wallet failed to signature",
    //ERR_WALLET_TX_OVERSIZE,                     // 34
    "wallet transaction is oversize",
    //ERR_WALLET_NOT_FOUND,                       // 35
    "wallet is missing",
    //ERR_WALLET_IS_LOCKED,	                      // 36
    "wallet is locked",
    //ERR_WALLET_IS_UNLOCKED,                     // 37
    "wallet is unlocked",
    //ERR_WALLET_IS_ENCRYPTED,                    // 38
    "wallet is encrypted",
    //ERR_WALLET_IS_UNENCRYPTED,                  // 39
    "wallet is unencrypted",
    //ERR_WALLET_FAILED,                          // 40
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
