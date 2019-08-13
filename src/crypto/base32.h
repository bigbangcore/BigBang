// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CRYPTO_BASE32_H
#define CRYPTO_BASE32_H

#include <string>

namespace bigbang
{
namespace crypto
{

// Crockford's Base32 without symbols check
// http://www.crockford.com/wrmg/base32.html
// alphabet : 0123456789abcdefghjkmnpqrstvwxyz, upper and lower case letters are accepted.
// only handle 32bytes data encode & decode, includes crc24 checksum

void Base32Encode(const unsigned char* md32, std::string& strBase32);
bool Base32Decode(const std::string& strBase32, unsigned char* md32);

} // namespace crypto
} // namespace bigbang

#endif // CRYPTO_BASE32_H
