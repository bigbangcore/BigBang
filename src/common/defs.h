// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_DEFS_H
#define BIGBANG_DEFS_H

//hard fork: change of hash algorithm and its input, and update of template address of multiple signature

#ifdef __cplusplus
extern "C"
{
#endif
    static const int HEIGHT_HASH_MULTI_SIGNER_MAINNET = 78256;
    static const int HEIGHT_HASH_MULTI_SIGNER_TESTNET = 20;
    extern unsigned int HEIGHT_HASH_MULTI_SIGNER;

    static const int HEIGHT_HASH_TX_DATA_MAINNET = 133060;
    static const int HEIGHT_HASH_TX_DATA_TESTNET = 40;
    extern unsigned int HEIGHT_HASH_TX_DATA;
#ifdef __cplusplus
}
#endif

#endif //BIGBANG_DEFS_H
