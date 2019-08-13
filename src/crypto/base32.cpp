// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "base32.h"

#include <string.h>

#include "crc24q.h"

namespace bigbang
{
namespace crypto
{

static inline void Base32Encode5Bytes(const unsigned char* md5, std::string& strBase32)
{
    static const char* alphabet = "0123456789abcdefghjkmnpqrstvwxyz";
    strBase32.push_back(alphabet[(md5[0] >> 3) & 0x1F]);
    strBase32.push_back(alphabet[((md5[0] << 2) & 0x1C) | ((md5[1] >> 6) & 0x03)]);
    strBase32.push_back(alphabet[(md5[1] >> 1) & 0x1F]);
    strBase32.push_back(alphabet[((md5[1] << 4) & 0x10) | ((md5[2] >> 4) & 0x0F)]);
    strBase32.push_back(alphabet[((md5[2] << 1) & 0x1E) | ((md5[3] >> 7) & 0x01)]);
    strBase32.push_back(alphabet[(md5[3] >> 2) & 0x1F]);
    strBase32.push_back(alphabet[((md5[3] << 3) & 0x18) | ((md5[4] >> 5) & 0x07)]);
    strBase32.push_back(alphabet[(md5[4] & 0x1F)]);
}

void Base32Encode(const unsigned char* md32, std::string& strBase32)
{
    unsigned int crc = crc24q(md32, 32);
    strBase32.reserve(56);

    for (int i = 0; i < 30; i += 5)
    {
        Base32Encode5Bytes(md32 + i, strBase32);
    }
    unsigned char tail[5] = { md32[30], md32[31], (unsigned char)(crc >> 16), (unsigned char)(crc >> 8), (unsigned char)crc };
    Base32Encode5Bytes(tail, strBase32);
}

static inline bool Base32Decode5Bytes(const char* psz, unsigned char* md5)
{
    static const char digit[256] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, 16, 17, -1, 18, 19, -1, 20, 21, -1, 22, 23, 24, 25, 26, -1, 27, 28, 29, 30, 31, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, 16, 17, -1, 18, 19, -1, 20, 21, -1, 22, 23, 24, 25, 26, -1, 27, 28, 29, 30, 31, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

    char idx[8];
    int sum = 0;
    sum &= (idx[0] = digit[*psz++]);
    sum &= (idx[1] = digit[*psz++]);
    sum &= (idx[2] = digit[*psz++]);
    sum &= (idx[3] = digit[*psz++]);
    sum &= (idx[4] = digit[*psz++]);
    sum &= (idx[5] = digit[*psz++]);
    sum &= (idx[6] = digit[*psz++]);
    sum &= (idx[7] = digit[*psz++]);

    md5[0] = ((idx[0] << 3) & 0xF8) | ((idx[1] >> 2) & 0x07);
    md5[1] = ((idx[1] << 6) & 0xC0) | ((idx[2] << 1) & 0x3E) | ((idx[3] >> 4) & 0x01);
    md5[2] = ((idx[3] << 4) & 0xF0) | ((idx[4] >> 1) & 0x0F);
    md5[3] = ((idx[4] << 7) & 0x80) | ((idx[5] << 2) & 0x7C) | ((idx[6] >> 3) & 0x03);
    md5[4] = ((idx[6] << 5) & 0xE0) | ((idx[7] & 0x1F));

    return (!(sum >> 7));
}
bool Base32Decode(const std::string& strBase32, unsigned char* md32)
{
    unsigned char data[35];
    if (strBase32.size() != 56)
    {
        return false;
    }
    for (int i = 0; i < 7; i++)
    {
        if (!Base32Decode5Bytes(strBase32.c_str() + i * 8, data + i * 5))
        {
            return false;
        }
    }
    if (crc24q(data, 35) != 0)
    {
        return false;
    }

    memmove(md32, data, 32);
    return true;
}

} // namespace crypto
} // namespace bigbang
