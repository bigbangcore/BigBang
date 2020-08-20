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

#ifndef BIGBANG_TESTNET
static std::vector<std::pair<int, uint256>> vCheckPoints = { { 0, uint256("00000000b0a9be545f022309e148894d1e1c853ccac3ef04cb6f5e5c70f41a70") },
                                                             { 100, uint256("000000649ec479bb9944fb85905822cb707eb2e5f42a5d58e598603b642e225d") },
                                                             { 1000, uint256("000003e86cc97e8b16aaa92216a66c2797c977a239bbd1a12476bad68580be73") },
                                                             { 2000, uint256("000007d07acd442c737152d0cd9d8e99b6f0177781323ccbe20407664e01da8f") },
                                                             { 5000, uint256("00001388dbb69842b373352462b869126b9fe912b4d86becbb3ad2bf1d897840") },
                                                             { 10000, uint256("00002710c3f3cd6c931f568169c645e97744943e02b0135aae4fcb3139c0fa6f") },
                                                             { 16000, uint256("00003e807c1e13c95e8601d7e870a1e13bc708eddad137a49ba6c0628ce901df") },
                                                             { 23000, uint256("000059d889977b9d0cd3d3fa149aa4c6e9c9da08c05c016cb800d52b2ecb620c") },
                                                             { 31000, uint256("000079188913bbe13cb3ff76df2ba2f9d2180854750ab9a37dc8d197668d2215") },
                                                             { 40000, uint256("00009c40c22952179a522909e8bec05617817952f3b9aebd1d1e096413fead5b") },
                                                             { 50000, uint256("0000c3506e5e7fae59bee39965fb45e284f86c993958e5ce682566810832e7e8") },
                                                             { 70000, uint256("000111701e15e979b4633e45b762067c6369e6f0ca8284094f6ce476b10f50de") },
                                                             { 90000, uint256("00015f902819ebe9915f30f0faeeb08e7cd063b882d9066af898a1c67257927c") },
                                                             { 110000, uint256("0001adb06ed43e55b0f960a212590674c8b10575de7afa7dc0bb0e53e971f21b") },
                                                             { 130000, uint256("0001fbd054458ec9f75e94d6779def1ee6c6d009dbbe2f7759f5c6c75c4f9630") },
                                                             { 150000, uint256("000249f070fe5b5fcb1923080c5dcbd78a6f31182ae32717df84e708b225370b") },
                                                             { 170000, uint256("00029810ac925d321a415e2fb83d703dcb2ebb2d42b66584c3666eb5795d8ad6") },
                                                             { 190000, uint256("0002e6304834d0f859658c939b77f9077073f42e91bf3f512bee644bd48180e1") },
                                                             { 210000, uint256("000334508ed90eb9419392e1fce660467973d3dede5ca51f6e457517d03f2138") },
                                                             { 230000, uint256("00038270812d3b2f338b5f8c9d00edfd084ae38580c6837b6278f20713ff20cc") },
                                                             { 238000, uint256("0003a1b031248f0c0060fd8afd807f30ba34f81b6fcbbe84157e380d2d7119bc") },
                                                             { 285060, uint256("00045984ae81f672b42525e0465dd05239c742fe0b6723a15c4fd03215362eae") },
                                                             { 366692, uint256("00059864d72f76565cb8aa190c1e73ab4eb449d6641d73f7eba4e6a849589453") } };
#endif

#endif //BIGBANG_PARAM_H
