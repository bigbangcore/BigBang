// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_NONCE_H
#define XENGINE_NONCE_H

#include <atomic>
#include <openssl/rand.h>

#include "type.h"

namespace xengine
{

const uint8 HTTP_NONCE_TYPE = 0x0F;
const uint8 PEER_NONCE_TYPE = 0xFF;

/**
 * @brief Create a random uint64 nonce. It's highest byte is nType
 * @param nType Hightest byte of nonce
 * @return Random nonce
 */
inline uint64 CreateNonce(const uint8 nType)
{
    uint64 nNonce = 0;
    unsigned char* p = (unsigned char*)&nNonce;
    RAND_bytes(p, 7);
    *(p + 7) = nType;
    return nNonce;
}

/**
 * @brief Judge the highest byte of nNonce to be nType
 * @param nNonce uint64 nonce
 * @param nType The hightest byte of nonce
 * @return The nonce belongs to nType or not
 */
inline bool NonceType(const uint64 nNonce, const uint8 nType)
{
    return *((unsigned char*)&nNonce + 7) == nType;
}

/**
 * @brief A wrapper of nonce. Common used with shared_ptr on multi-thread to judge nonce to be valid or not
 */
class CNonce
{
public:
    uint64 nNonce;
    std::atomic<bool> fValid;

    CNonce(const uint64 nNonceIn = 0)
      : nNonce(nNonceIn), fValid(true) {}

    static std::shared_ptr<CNonce> Create(const uint64 nNonceIn = 0)
    {
        return std::make_shared<CNonce>(nNonceIn);
    }
};

typedef std::shared_ptr<CNonce> CNoncePtr;

inline bool operator==(const CNoncePtr& lhs, const CNoncePtr& rhs)
{
    return lhs->nNonce == rhs->nNonce;
}

inline bool operator<(const CNoncePtr& lhs, const CNoncePtr& rhs)
{
    return lhs->nNonce < rhs->nNonce;
}

inline bool operator<(const CNoncePtr& lhs, const uint64 rhs)
{
    return lhs->nNonce < rhs;
}

inline bool operator<(const uint64 lhs, const CNoncePtr& rhs)
{
    return lhs < rhs->nNonce;
}

} // namespace xengine

#endif //XENGINE_NONCE_H
