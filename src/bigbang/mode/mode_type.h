// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_MODE_MODE_TYPE_H
#define BIGBANG_MODE_MODE_TYPE_H

namespace bigbang
{
// mode type
enum class EModeType
{
    ERROR = 0, // ERROR type
    SERVER,    // server
    CONSOLE,   // console
    MINER,     // miner
};

} // namespace bigbang

#endif // BIGBANG_MODE_MODE_TYPE_H
