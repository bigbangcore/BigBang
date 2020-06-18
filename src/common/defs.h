// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_DEFS_H
#define BIGBANG_DEFS_H

#include <stdbool.h>

extern bool TESTNET;

#define SWITCH_PARAM(MAINNET_PARAM, TESTNET_PARAM) (TESTNET ? TESTNET_PARAM : MAINNET_PARAM)

//hard fork: change of hash algorithm and its input, and update of template address of multiple signature
static const unsigned int HEIGHT_HASH_MULTI_SIGNER_MAINNET = 78256;
static const unsigned int HEIGHT_HASH_MULTI_SIGNER_TESTNET = 20;
#define HEIGHT_HASH_MULTI_SIGNER SWITCH_PARAM(HEIGHT_HASH_MULTI_SIGNER_MAINNET, HEIGHT_HASH_MULTI_SIGNER_TESTNET)

static const unsigned int HEIGHT_HASH_TX_DATA_MAINNET = 133060;
static const unsigned int HEIGHT_HASH_TX_DATA_TESTNET = 40;
#define HEIGHT_HASH_TX_DATA SWITCH_PARAM(HEIGHT_HASH_TX_DATA_MAINNET, HEIGHT_HASH_TX_DATA_TESTNET)

#endif //BIGBANG_DEFS_H
