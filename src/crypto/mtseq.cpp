// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mtseq.h"

#define MATRIX_A 0xB5026F5AA96619E9ULL
#define UM 0xFFFFFFFF80000000ULL /* Most significant 33 bits */
#define LM 0x7FFFFFFFULL         /* Least significant 31 bits */

namespace bigbang
{
namespace crypto
{

//////////////////////////////
// CMTSeq

CMTSeq::CMTSeq(const uint64 nSeed)
{
    MakeSeq(nSeed);
}

void CMTSeq::MakeSeq(const uint64 nSeed)
{
    uint64 mt[NN];
    mt[0] = nSeed;
    for (int i = 1; i < NN; i++)
    {
        mt[i] = (6364136223846793005ULL * (mt[i - 1] ^ (mt[i - 1] >> 62)) + i);
    }
}

} // namespace crypto
} // namespace bigbang
