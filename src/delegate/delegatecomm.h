// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DELEGATE_DELEGATECOMM_H
#define DELEGATE_DELEGATECOMM_H

#include "destination.h"
#include "mpvss.h"
#include "uint256.h"

namespace bigbang
{
namespace delegate
{

#ifdef BIGBANG_TESTNET
#define CONSENSUS_DISTRIBUTE_INTERVAL 3
#define CONSENSUS_ENROLL_INTERVAL 6
#else
#define CONSENSUS_DISTRIBUTE_INTERVAL 15
#define CONSENSUS_ENROLL_INTERVAL 30
#endif
#define CONSENSUS_INTERVAL (CONSENSUS_DISTRIBUTE_INTERVAL + CONSENSUS_ENROLL_INTERVAL + 1)

#define MAX_DELEGATE_THRESH (23)

/* 
   Consensus phase:

   T : target height (consensus index)
   B : begin of enrolling
   E : end of enrolling
   P : previous block height

   B                                    E                   P       T
   |____________________________________|___________________|_______|
                 enroll                       distribute     publish
   
   P = T - 1
   E = P - CONSENSUS_DISTRIBUTE_INTERVAL
   B = E - CONSENSUS_ENROLL_INTERVAL

   state:
   h = current block height
   x = consensus index
   Tx = x, 
   Px = x - 1,
   Ex = x - CONSENSUS_DISTRIBUTE_INTERVAL - 1
   Bx = x - CONSENSUS_INTERVAL 

   init       : 
       h == Bx
       x == h + CONSENSUS_INTERVAL
   enroll     :
       h <= Ex && h > Bx
       x > h + CONSENSUS_DISTRIBUTE_INTERVAL && x < h + CONSENSUS_INTERVAL
   distribute : 
       h < Px && h >= Ex
       x > h + 1 && x <= h + CONSENSUS_DISTRIBUTE_INTERVAL + 1
   publish     :
       h == Px
       x == h + 1
   stall      :
       h >= Px + 1
       x <= h
*/

inline const CDestination DestFromIdentUInt256(const uint256& nIdent)
{
    return CDestination(CTemplateId(nIdent));
}

inline const uint256 DestToIdentUInt256(const CDestination& dest)
{
    return dest.GetTemplateId();
}

} // namespace delegate
} // namespace bigbang

#endif //DELEGATE_DELEGATE_COMM_H
