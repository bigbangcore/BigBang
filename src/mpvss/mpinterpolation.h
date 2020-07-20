// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MPVSS_MPLAGRANGE_H
#define MPVSS_MPLAGRANGE_H

#include <vector>

#include "uint256.h"

const uint256 MPLagrange(std::vector<std::pair<uint32_t, uint256>>& vShare);

const uint256 MPNewton(std::vector<std::pair<uint32_t, uint256>>& vShare);

#endif // MPVSS_MPLAGRANGE_H
