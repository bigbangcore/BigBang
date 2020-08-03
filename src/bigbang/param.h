// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_PARAM_H
#define BIGBANG_PARAM_H

#include "uint256.h"

static const int64 COIN = 1000000;
static const int64 CENT = 10000;
static const int64 OLD_MIN_TX_FEE = CENT / 100;
static const int64 NEW_MIN_TX_FEE = CENT;
static const int64 MAX_MONEY = 1000000000000 * COIN;
inline bool MoneyRange(int64 nValue)
{
    return (nValue >= 0 && nValue <= MAX_MONEY);
}
static const int64 MAX_REWARD_MONEY = 10000 * COIN;
inline bool RewardRange(int64 nValue)
{
    return (nValue >= 0 && nValue <= MAX_REWARD_MONEY);
}

static const unsigned int MAX_BLOCK_SIZE = 2000000;
static const unsigned int MAX_TX_SIZE = (MAX_BLOCK_SIZE / 20);
static const unsigned int MAX_SIGNATURE_SIZE = 2048;
static const unsigned int MAX_TX_INPUT_COUNT = (MAX_TX_SIZE - MAX_SIGNATURE_SIZE - 4) / 33;

static const unsigned int BLOCK_TARGET_SPACING = 60; // 1-minute block spacing
static const unsigned int EXTENDED_BLOCK_SPACING = 2;
static const unsigned int PROOF_OF_WORK_DECAY_STEP = BLOCK_TARGET_SPACING;
static const unsigned int PROOF_OF_WORK_BLOCK_SPACING = 20;

static const unsigned int MINT_MATURITY = 120; // 120 blocks about 2 hours
static const unsigned int MIN_TOKEN_TX_SIZE = 196;

#ifdef BIGBANG_TESTNET
static const unsigned int MIN_CREATE_FORK_INTERVAL_HEIGHT = 0;
static const unsigned int MAX_JOINT_FORK_INTERVAL_HEIGHT = 0x7FFFFFFF;
#else
static const unsigned int MIN_CREATE_FORK_INTERVAL_HEIGHT = 30;
static const unsigned int MAX_JOINT_FORK_INTERVAL_HEIGHT = 1440;
#endif

enum ConsensusMethod
{
    CM_MPVSS = 0,
    CM_CRYPTONIGHT = 1,
    CM_MAX
};

#endif //BIGBANG_PARAM_H
