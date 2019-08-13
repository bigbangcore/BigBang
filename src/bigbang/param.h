// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_PARAM_H
#define BIGBANG_PARAM_H

#include "uint256.h"

static const int64 COIN = 1000000;
static const int64 CENT = 10000;
static const int64 MIN_TX_FEE = CENT / 100;
static const int64 MAX_MONEY = 1000000000 * COIN;
inline bool MoneyRange(int64 nValue)
{
    return (nValue >= 0 && nValue <= MAX_MONEY);
}

static const unsigned int MAX_BLOCK_SIZE = 2000000;
static const unsigned int MAX_TX_SIZE = (MAX_BLOCK_SIZE / 20);

static const unsigned int BLOCK_TARGET_SPACING = 60; // 1-minute block spacing
static const unsigned int EXTENDED_BLOCK_SPACING = 1;
static const unsigned int PROOF_OF_WORK_DECAY_STEP = BLOCK_TARGET_SPACING;

static const unsigned int MINT_MATURITY = 120; // 120 blocks about 2 hours
static const unsigned int MIN_TOKEN_TX_SIZE = 196;

static const unsigned int DELEGATE_THRESH = 50;

enum ConsensusMethod
{
    CM_VSSMP = 0,
    CM_CRYPTONIGHT = 1,
    CM_MAX
};

#endif //BIGBANG_PARAM_H
