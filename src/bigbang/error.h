// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_ERROR_H
#define BIGBANG_ERROR_H

namespace bigbang
{

typedef enum
{
    FAILED = -1,
    OK = 0,
    /* moduler */
    ERR_UNAVAILABLE, // 1
    /* container */
    ERR_NOT_FOUND,    // 2
    ERR_ALREADY_HAVE, // 3
    ERR_MISSING_PREV, // 4
    /* system */
    ERR_SYS_DATABASE_ERROR,    // 5
    ERR_SYS_OUT_OF_DISK_SPACE, // 6
    ERR_SYS_STORAGE_ERROR,     // 7
    ERR_SYS_OUT_OF_MEMORY,     // 8
    /* block */
    ERR_BLOCK_OVERSIZE,                // 9
    ERR_BLOCK_PROOF_OF_WORK_INVALID,   // 10
    ERR_BLOCK_PROOF_OF_STAKE_INVALID,  // 11
    ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE,  // 12
    ERR_BLOCK_COINBASE_INVALID,        // 13
    ERR_BLOCK_COINSTAKE_INVALID,       // 14
    ERR_BLOCK_TRANSACTIONS_INVALID,    // 15
    ERR_BLOCK_DUPLICATED_TRANSACTION,  // 16
    ERR_BLOCK_SIGOPCOUNT_OUT_OF_BOUND, // 17
    ERR_BLOCK_TXHASH_MISMATCH,         // 18
    ERR_BLOCK_SIGNATURE_INVALID,       // 19
    ERR_BLOCK_INVALID_FORK,            // 20
    /* transaction */
    ERR_TRANSACTION_INVALID,              // 21
    ERR_TRANSACTION_OVERSIZE,             // 22
    ERR_TRANSACTION_OUTPUT_INVALID,       // 23
    ERR_TRANSACTION_INPUT_INVALID,        // 24
    ERR_TRANSACTION_TIMESTAMP_INVALID,    // 25
    ERR_TRANSACTION_NOT_ENOUGH_FEE,       // 26
    ERR_TRANSACTION_STAKE_REWARD_INVALID, // 27
    ERR_TRANSACTION_SIGNATURE_INVALID,    // 28
    ERR_TRANSACTION_CONFLICTING_INPUT,    // 29
    /* wallet */
    ERR_WALLET_INVALID_AMOUNT,     // 30
    ERR_WALLET_INSUFFICIENT_FUNDS, // 31
    ERR_WALLET_SIGNATURE_FAILED,   // 32
    ERR_WALLET_TX_OVERSIZE,        // 33
    ERR_WALLET_NOT_FOUND,          // 34
    ERR_WALLET_IS_LOCKED,          // 35
    ERR_WALLET_IS_UNLOCKED,        // 36
    ERR_WALLET_IS_ENCRYPTED,       // 37
    ERR_WALLET_IS_UNENCRYPTED,     // 38
    ERR_WALLET_FAILED,             // 39
    /* count */
    ERR_MAX_COUNT
} Errno;

extern const char* ErrorString(const Errno& err);

} // namespace bigbang

#endif //BIGBANG_ERROR_H
