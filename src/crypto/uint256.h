// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CRYPTO_UINT256_H
#define CRYPTO_UINT256_H

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "stream/datastream.h"
#include "stream/stream.h"

/** Base class without constructors for uint256 and uint160.
 * This makes the compiler let u use it in a union.
 */
template <unsigned int BITS>
class base_uint
{
    friend class xengine::CStream;

protected:
    enum
    {
        WIDTH = BITS / 32
    };
    unsigned int pn[WIDTH];

public:
    bool operator!() const
    {
        for (int i = 0; i < WIDTH; i++)
            if (pn[i] != 0)
                return false;
        return true;
    }

    const base_uint operator~() const
    {
        base_uint ret;
        for (int i = 0; i < WIDTH; i++)
            ret.pn[i] = ~pn[i];
        return ret;
    }

    const base_uint operator-() const
    {
        base_uint ret;
        for (int i = 0; i < WIDTH; i++)
            ret.pn[i] = ~pn[i];
        ++ret;
        return ret;
    }

    base_uint& operator=(uint64 b)
    {
        pn[0] = (unsigned int)b;
        pn[1] = (unsigned int)(b >> 32);
        for (int i = 2; i < WIDTH; i++)
            pn[i] = 0;
        return *this;
    }

    base_uint& operator^=(const base_uint& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] ^= b.pn[i];
        return *this;
    }

    base_uint& operator&=(const base_uint& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] &= b.pn[i];
        return *this;
    }

    base_uint& operator|=(const base_uint& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] |= b.pn[i];
        return *this;
    }

    base_uint& operator^=(uint64 b)
    {
        pn[0] ^= (unsigned int)b;
        pn[1] ^= (unsigned int)(b >> 32);
        return *this;
    }

    base_uint& operator|=(uint64 b)
    {
        pn[0] |= (unsigned int)b;
        pn[1] |= (unsigned int)(b >> 32);
        return *this;
    }

    base_uint& operator<<=(unsigned int shift)
    {
        base_uint a(*this);
        for (int i = 0; i < WIDTH; i++)
            pn[i] = 0;
        int k = shift / 32;
        shift = shift % 32;
        for (int i = 0; i < WIDTH; i++)
        {
            if (i + k + 1 < WIDTH && shift != 0)
                pn[i + k + 1] |= (a.pn[i] >> (32 - shift));
            if (i + k < WIDTH)
                pn[i + k] |= (a.pn[i] << shift);
        }
        return *this;
    }

    base_uint& operator>>=(unsigned int shift)
    {
        base_uint a(*this);
        for (int i = 0; i < WIDTH; i++)
            pn[i] = 0;
        int k = shift / 32;
        shift = shift % 32;
        for (int i = 0; i < WIDTH; i++)
        {
            if (i - k - 1 >= 0 && shift != 0)
                pn[i - k - 1] |= (a.pn[i] << (32 - shift));
            if (i - k >= 0)
                pn[i - k] |= (a.pn[i] >> shift);
        }
        return *this;
    }

    base_uint& operator+=(const base_uint& b)
    {
        uint64 carry = 0;
        for (int i = 0; i < WIDTH; i++)
        {
            uint64 n = carry + pn[i] + b.pn[i];
            pn[i] = n & 0xffffffff;
            carry = n >> 32;
        }
        return *this;
    }

    base_uint& operator-=(const base_uint& b)
    {
        *this += -b;
        return *this;
    }

    base_uint& operator+=(uint64 b64)
    {
        base_uint b;
        b = b64;
        *this += b;
        return *this;
    }

    base_uint& operator-=(uint64 b64)
    {
        base_uint b;
        b = b64;
        *this += -b;
        return *this;
    }

    base_uint& operator++()
    {
        // prefix operator
        int i = 0;
        while (++pn[i] == 0 && i < WIDTH - 1)
            i++;
        return *this;
    }

    const base_uint operator++(int)
    {
        // postfix operator
        const base_uint ret = *this;
        ++(*this);
        return ret;
    }

    base_uint& operator--()
    {
        // prefix operator
        int i = 0;
        while (--pn[i] == -1 && i < WIDTH - 1)
            i++;
        return *this;
    }

    const base_uint operator--(int)
    {
        // postfix operator
        const base_uint ret = *this;
        --(*this);
        return ret;
    }

    base_uint& operator*=(uint32_t b32)
    {
        uint64_t carry = 0;
        for (int i = 0; i < WIDTH; i++)
        {
            uint64_t n = carry + (uint64_t)b32 * pn[i];
            pn[i] = n & 0xffffffff;
            carry = n >> 32;
        }
        return *this;
    }

    base_uint& operator*=(const base_uint& b)
    {
        base_uint<BITS> a;
        for (int j = 0; j < WIDTH; j++)
        {
            uint64_t carry = 0;
            for (int i = 0; i + j < WIDTH; i++)
            {
                uint64_t n = carry + a.pn[i + j] + (uint64_t)pn[j] * b.pn[i];
                a.pn[i + j] = n & 0xffffffff;
                carry = n >> 32;
            }
        }
        *this = a;
        return *this;
    }

    base_uint& operator/=(const base_uint& b)
    {
        base_uint div = b;     // make a copy, so we can shift.
        base_uint num = *this; // make a copy, so we can subtract.
        *this = 0;             // the quotient.
        int num_bits = num.Bits();
        int div_bits = div.Bits();
        if (div_bits == 0 || div_bits > num_bits)
        {
            return *this;
        }

        int shift = num_bits - div_bits;
        div <<= shift; // shift so that div and num align.
        while (shift >= 0)
        {
            if (num >= div)
            {
                num -= div;
                pn[shift / 32] |= (1 << (shift & 31)); // set a bit of the result.
            }
            div >>= 1; // shift back.
            shift--;
        }
        // num now contains the remainder of the division.
        return *this;
    }

    unsigned int& operator[](size_t pos)
    {
        return pn[pos];
    }

    const unsigned int& operator[](size_t pos) const
    {
        return pn[pos];
    }

    friend inline bool operator<(const base_uint& a, const base_uint& b)
    {
        for (int i = base_uint::WIDTH - 1; i >= 0; i--)
        {
            if (a.pn[i] < b.pn[i])
                return true;
            else if (a.pn[i] > b.pn[i])
                return false;
        }
        return false;
    }

    friend inline bool operator<=(const base_uint& a, const base_uint& b)
    {
        for (int i = base_uint::WIDTH - 1; i >= 0; i--)
        {
            if (a.pn[i] < b.pn[i])
                return true;
            else if (a.pn[i] > b.pn[i])
                return false;
        }
        return true;
    }

    friend inline bool operator>(const base_uint& a, const base_uint& b)
    {
        for (int i = base_uint::WIDTH - 1; i >= 0; i--)
        {
            if (a.pn[i] > b.pn[i])
                return true;
            else if (a.pn[i] < b.pn[i])
                return false;
        }
        return false;
    }

    friend inline bool operator>=(const base_uint& a, const base_uint& b)
    {
        for (int i = base_uint::WIDTH - 1; i >= 0; i--)
        {
            if (a.pn[i] > b.pn[i])
                return true;
            else if (a.pn[i] < b.pn[i])
                return false;
        }
        return true;
    }

    friend inline bool operator==(const base_uint& a, const base_uint& b)
    {
        for (int i = 0; i < base_uint::WIDTH; i++)
            if (a.pn[i] != b.pn[i])
                return false;
        return true;
    }

    friend inline bool operator==(const base_uint& a, uint64 b)
    {
        if (a.pn[0] != (unsigned int)b)
            return false;
        if (a.pn[1] != (unsigned int)(b >> 32))
            return false;
        for (int i = 2; i < base_uint::WIDTH; i++)
            if (a.pn[i] != 0)
                return false;
        return true;
    }

    friend inline bool operator!=(const base_uint& a, const base_uint& b)
    {
        return (!(a == b));
    }

    friend inline bool operator!=(const base_uint& a, uint64 b)
    {
        return (!(a == b));
    }

    std::string GetHex() const
    {
        char psz[sizeof(pn) * 2 + 1];
        for (unsigned int i = 0; i < sizeof(pn); i++)
            sprintf(psz + i * 2, "%02x", ((unsigned char*)pn)[sizeof(pn) - i - 1]);
        return std::string(psz, psz + sizeof(pn) * 2);
    }

    size_t SetHex(const char* psz)
    {
        size_t len = 0;
        const char* in = psz;

        for (int i = 0; i < WIDTH; i++)
            pn[i] = 0;

        // skip leading spaces
        while (isspace(*psz))
            psz++;

        // skip 0x
        if (psz[0] == '0' && tolower(psz[1]) == 'x')
            psz += 2;

        // hex string to uint
        static unsigned char phexdigit[256] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        const char* pbegin = psz;
        while (phexdigit[(unsigned char)*psz] || *psz == '0')
            psz++;
        len = psz - in;
        psz--;
        unsigned char* p1 = (unsigned char*)pn;
        unsigned char* pend = p1 + WIDTH * 4;
        while (psz >= pbegin && p1 < pend)
        {
            *p1 = phexdigit[(unsigned char)*psz--];
            if (psz >= pbegin)
            {
                *p1 |= (phexdigit[(unsigned char)*psz--] << 4);
                p1++;
            }
        }
        return len;
    }

    size_t SetHex(const std::string& str)
    {
        return SetHex(str.c_str());
    }

    std::string ToString() const
    {
        return (GetHex());
    }

    unsigned char* begin()
    {
        return (unsigned char*)&pn[0];
    }

    unsigned char* end()
    {
        return (unsigned char*)&pn[WIDTH];
    }

    const unsigned char* begin() const
    {
        return (unsigned char*)&pn[0];
    }

    const unsigned char* end() const
    {
        return (unsigned char*)&pn[WIDTH];
    }

    static unsigned int size()
    {
        return sizeof(pn);
    }

    uint64 Get64(int n = 0) const
    {
        return pn[2 * n] | (uint64)pn[2 * n + 1] << 32;
    }

    uint32 Get32(int n = 0) const
    {
        return pn[n];
    }

    void ToDataStream(xengine::CODataStream& os) const
    {
        os.Push(pn, sizeof(pn));
    }

    void FromDataStream(xengine::CIDataStream& is)
    {
        is.Pop(pn, sizeof(pn));
    }

    unsigned int Bits() const
    {
        for (int pos = WIDTH - 1; pos >= 0; pos--)
        {
            if (pn[pos])
            {
                for (int nbits = 31; nbits > 0; nbits--)
                {
                    if (pn[pos] & 1U << nbits)
                        return 32 * pos + nbits + 1;
                }
                return 32 * pos + 1;
            }
        }
        return 0;
    }

    uint64_t GetLow64() const
    {
        if (WIDTH >= 2)
        {
            return pn[0] | (uint64_t)pn[1] << 32;
        }
        else if (WIDTH == 1)
        {
            return (uint64_t)pn[0];
        }
        else
        {
            return 0;
        }
    }

protected:
    void Serialize(xengine::CStream& s, xengine::SaveType&) const
    {
        s.Write((char*)pn, sizeof(pn));
    }
    void Serialize(xengine::CStream& s, xengine::LoadType&)
    {
        s.Read((char*)pn, sizeof(pn));
    }
    void Serialize(xengine::CStream& s, std::size_t& serSize) const
    {
        (void)s;
        serSize += sizeof(pn);
    }

    friend class uint160;
    friend class uint256;
    friend class uint224;
    friend inline int Testuint256AdHoc(std::vector<std::string> vArg);
};

typedef base_uint<160> base_uint160;
typedef base_uint<256> base_uint256;
typedef base_uint<224> base_uint224;

//
// uint160 and uint256 could be implemented as templates, but to keep
// compile errors and debugging cleaner, they're copy and pasted.
//

//////////////////////////////////////////////////////////////////////////////
//
// uint160
//

/** 160-bit unsigned integer */
class uint160 : public base_uint160
{
public:
    typedef base_uint160 basetype;

    uint160()
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] = 0;
    }

    uint160(const basetype& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] = b.pn[i];
    }

    uint160& operator=(const basetype& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] = b.pn[i];
        return *this;
    }

    uint160(uint64 b)
    {
        pn[0] = (unsigned int)b;
        pn[1] = (unsigned int)(b >> 32);
        for (int i = 2; i < WIDTH; i++)
            pn[i] = 0;
    }

    uint160& operator=(uint64 b)
    {
        pn[0] = (unsigned int)b;
        pn[1] = (unsigned int)(b >> 32);
        for (int i = 2; i < WIDTH; i++)
            pn[i] = 0;
        return *this;
    }

    explicit uint160(const std::string& str)
    {
        SetHex(str);
    }

    explicit uint160(const std::vector<unsigned char>& vch)
    {
        if (vch.size() == sizeof(pn))
            memcpy(pn, &vch[0], sizeof(pn));
        else
            *this = 0;
    }
};

inline bool operator==(const uint160& a, uint64 b)
{
    return (base_uint160)a == b;
}
inline bool operator!=(const uint160& a, uint64 b)
{
    return (base_uint160)a != b;
}
inline const uint160 operator<<(const base_uint160& a, unsigned int shift)
{
    return uint160(a) <<= shift;
}
inline const uint160 operator>>(const base_uint160& a, unsigned int shift)
{
    return uint160(a) >>= shift;
}
inline const uint160 operator<<(const uint160& a, unsigned int shift)
{
    return uint160(a) <<= shift;
}
inline const uint160 operator>>(const uint160& a, unsigned int shift)
{
    return uint160(a) >>= shift;
}

inline const uint160 operator^(const base_uint160& a, const base_uint160& b)
{
    return uint160(a) ^= b;
}
inline const uint160 operator&(const base_uint160& a, const base_uint160& b)
{
    return uint160(a) &= b;
}
inline const uint160 operator|(const base_uint160& a, const base_uint160& b)
{
    return uint160(a) |= b;
}
inline const uint160 operator+(const base_uint160& a, const base_uint160& b)
{
    return uint160(a) += b;
}
inline const uint160 operator-(const base_uint160& a, const base_uint160& b)
{
    return uint160(a) -= b;
}

inline bool operator<(const base_uint160& a, const uint160& b)
{
    return (base_uint160)a < (base_uint160)b;
}
inline bool operator<=(const base_uint160& a, const uint160& b)
{
    return (base_uint160)a <= (base_uint160)b;
}
inline bool operator>(const base_uint160& a, const uint160& b)
{
    return (base_uint160)a > (base_uint160)b;
}
inline bool operator>=(const base_uint160& a, const uint160& b)
{
    return (base_uint160)a >= (base_uint160)b;
}
inline bool operator==(const base_uint160& a, const uint160& b)
{
    return (base_uint160)a == (base_uint160)b;
}
inline bool operator!=(const base_uint160& a, const uint160& b)
{
    return (base_uint160)a != (base_uint160)b;
}
inline const uint160 operator^(const base_uint160& a, const uint160& b)
{
    return (base_uint160)a ^ (base_uint160)b;
}
inline const uint160 operator&(const base_uint160& a, const uint160& b)
{
    return (base_uint160)a & (base_uint160)b;
}
inline const uint160 operator|(const base_uint160& a, const uint160& b)
{
    return (base_uint160)a | (base_uint160)b;
}
inline const uint160 operator+(const base_uint160& a, const uint160& b)
{
    return (base_uint160)a + (base_uint160)b;
}
inline const uint160 operator-(const base_uint160& a, const uint160& b)
{
    return (base_uint160)a - (base_uint160)b;
}

inline bool operator<(const uint160& a, const base_uint160& b)
{
    return (base_uint160)a < (base_uint160)b;
}
inline bool operator<=(const uint160& a, const base_uint160& b)
{
    return (base_uint160)a <= (base_uint160)b;
}
inline bool operator>(const uint160& a, const base_uint160& b)
{
    return (base_uint160)a > (base_uint160)b;
}
inline bool operator>=(const uint160& a, const base_uint160& b)
{
    return (base_uint160)a >= (base_uint160)b;
}
inline bool operator==(const uint160& a, const base_uint160& b)
{
    return (base_uint160)a == (base_uint160)b;
}
inline bool operator!=(const uint160& a, const base_uint160& b)
{
    return (base_uint160)a != (base_uint160)b;
}
inline const uint160 operator^(const uint160& a, const base_uint160& b)
{
    return (base_uint160)a ^ (base_uint160)b;
}
inline const uint160 operator&(const uint160& a, const base_uint160& b)
{
    return (base_uint160)a & (base_uint160)b;
}
inline const uint160 operator|(const uint160& a, const base_uint160& b)
{
    return (base_uint160)a | (base_uint160)b;
}
inline const uint160 operator+(const uint160& a, const base_uint160& b)
{
    return (base_uint160)a + (base_uint160)b;
}
inline const uint160 operator-(const uint160& a, const base_uint160& b)
{
    return (base_uint160)a - (base_uint160)b;
}

inline bool operator<(const uint160& a, const uint160& b)
{
    return (base_uint160)a < (base_uint160)b;
}
inline bool operator<=(const uint160& a, const uint160& b)
{
    return (base_uint160)a <= (base_uint160)b;
}
inline bool operator>(const uint160& a, const uint160& b)
{
    return (base_uint160)a > (base_uint160)b;
}
inline bool operator>=(const uint160& a, const uint160& b)
{
    return (base_uint160)a >= (base_uint160)b;
}
inline bool operator==(const uint160& a, const uint160& b)
{
    return (base_uint160)a == (base_uint160)b;
}
inline bool operator!=(const uint160& a, const uint160& b)
{
    return (base_uint160)a != (base_uint160)b;
}
inline const uint160 operator^(const uint160& a, const uint160& b)
{
    return (base_uint160)a ^ (base_uint160)b;
}
inline const uint160 operator&(const uint160& a, const uint160& b)
{
    return (base_uint160)a & (base_uint160)b;
}
inline const uint160 operator|(const uint160& a, const uint160& b)
{
    return (base_uint160)a | (base_uint160)b;
}
inline const uint160 operator+(const uint160& a, const uint160& b)
{
    return (base_uint160)a + (base_uint160)b;
}
inline const uint160 operator-(const uint160& a, const uint160& b)
{
    return (base_uint160)a - (base_uint160)b;
}

//////////////////////////////////////////////////////////////////////////////
//
// uint256
//

/** 256-bit unsigned integer */
class uint256 : public base_uint256
{
public:
    typedef base_uint256 basetype;

    uint256()
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] = 0;
    }

    uint256(const basetype& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] = b.pn[i];
    }

    uint256(const uint32 h, const base_uint224& l)
    {
        for (int i = 0; i < WIDTH - 1; i++)
            pn[i] = l.pn[i];
        pn[WIDTH - 1] = h;
    }

    uint256& operator=(const basetype& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] = b.pn[i];
        return *this;
    }

    uint256(const uint64 b)
    {
        pn[0] = (unsigned int)b;
        pn[1] = (unsigned int)(b >> 32);
        for (int i = 2; i < WIDTH; i++)
            pn[i] = 0;
    }

    uint256(const uint64 b[4])
    {
        for (int i = 0; i < 4; i++)
        {
            pn[2 * i] = (unsigned int)b[i];
            pn[2 * i + 1] = (unsigned int)(b[i] >> 32);
        }
    }

    uint256& operator=(uint64 b)
    {
        pn[0] = (unsigned int)b;
        pn[1] = (unsigned int)(b >> 32);
        for (int i = 2; i < WIDTH; i++)
            pn[i] = 0;
        return *this;
    }

    explicit uint256(const std::string& str)
    {
        SetHex(str);
    }

    explicit uint256(const std::vector<unsigned char>& vch)
    {
        if (vch.size() == sizeof(pn))
            memcpy(pn, &vch[0], sizeof(pn));
        else
            *this = 0;
    }

    uint256& SetCompact(uint32_t nCompact, bool* pfNegative = nullptr, bool* pfOverflow = nullptr)
    {
        int nSize = nCompact >> 24;
        uint32_t nWord = nCompact & 0x007fffff;
        if (nSize <= 3)
        {
            nWord >>= 8 * (3 - nSize);
            *this = nWord;
        }
        else
        {
            *this = nWord;
            *this <<= 8 * (nSize - 3);
        }
        if (pfNegative)
            *pfNegative = nWord != 0 && (nCompact & 0x00800000) != 0;
        if (pfOverflow)
            *pfOverflow = nWord != 0 && ((nSize > 34) || (nWord > 0xff && nSize > 33) || (nWord > 0xffff && nSize > 32));
        return *this;
    }

    uint32_t GetCompact(bool fNegative = false) const
    {
        int nSize = (Bits() + 7) / 8;
        uint32_t nCompact = 0;
        if (nSize <= 3)
        {
            nCompact = GetLow64() << 8 * (3 - nSize);
        }
        else
        {
            uint256 bn = *this;
            bn >>= 8 * (nSize - 3);
            nCompact = bn.GetLow64();
        }
        // The 0x00800000 bit denotes the sign.
        // Thus, if it is already set, divide the mantissa by 256 and increase the exponent.
        if (nCompact & 0x00800000)
        {
            nCompact >>= 8;
            nSize++;
        }
        assert((nCompact & ~0x007fffff) == 0);
        assert(nSize < 256);
        nCompact |= nSize << 24;
        nCompact |= (fNegative && (nCompact & 0x007fffff) ? 0x00800000 : 0);
        return nCompact;
    }
};

inline bool operator==(const uint256& a, uint64 b)
{
    return (base_uint256)a == b;
}
inline bool operator!=(const uint256& a, uint64 b)
{
    return (base_uint256)a != b;
}
inline const uint256 operator<<(const base_uint256& a, unsigned int shift)
{
    return uint256(a) <<= shift;
}
inline const uint256 operator>>(const base_uint256& a, unsigned int shift)
{
    return uint256(a) >>= shift;
}
inline const uint256 operator<<(const uint256& a, unsigned int shift)
{
    return uint256(a) <<= shift;
}
inline const uint256 operator>>(const uint256& a, unsigned int shift)
{
    return uint256(a) >>= shift;
}

inline const uint256 operator^(const base_uint256& a, const base_uint256& b)
{
    return uint256(a) ^= b;
}
inline const uint256 operator&(const base_uint256& a, const base_uint256& b)
{
    return uint256(a) &= b;
}
inline const uint256 operator|(const base_uint256& a, const base_uint256& b)
{
    return uint256(a) |= b;
}
inline const uint256 operator+(const base_uint256& a, const base_uint256& b)
{
    return uint256(a) += b;
}
inline const uint256 operator-(const base_uint256& a, const base_uint256& b)
{
    return uint256(a) -= b;
}
inline const uint256 operator*(const base_uint256& a, const base_uint256& b)
{
    return uint256(a) *= b;
}
inline const uint256 operator/(const base_uint256& a, const base_uint256& b)
{
    return uint256(a) /= b;
}

inline bool operator<(const base_uint256& a, const uint256& b)
{
    return (base_uint256)a < (base_uint256)b;
}
inline bool operator<=(const base_uint256& a, const uint256& b)
{
    return (base_uint256)a <= (base_uint256)b;
}
inline bool operator>(const base_uint256& a, const uint256& b)
{
    return (base_uint256)a > (base_uint256)b;
}
inline bool operator>=(const base_uint256& a, const uint256& b)
{
    return (base_uint256)a >= (base_uint256)b;
}
inline bool operator==(const base_uint256& a, const uint256& b)
{
    return (base_uint256)a == (base_uint256)b;
}
inline bool operator!=(const base_uint256& a, const uint256& b)
{
    return (base_uint256)a != (base_uint256)b;
}
inline const uint256 operator^(const base_uint256& a, const uint256& b)
{
    return (base_uint256)a ^ (base_uint256)b;
}
inline const uint256 operator&(const base_uint256& a, const uint256& b)
{
    return (base_uint256)a & (base_uint256)b;
}
inline const uint256 operator|(const base_uint256& a, const uint256& b)
{
    return (base_uint256)a | (base_uint256)b;
}
inline const uint256 operator+(const base_uint256& a, const uint256& b)
{
    return (base_uint256)a + (base_uint256)b;
}
inline const uint256 operator-(const base_uint256& a, const uint256& b)
{
    return (base_uint256)a - (base_uint256)b;
}

inline bool operator<(const uint256& a, const base_uint256& b)
{
    return (base_uint256)a < (base_uint256)b;
}
inline bool operator<=(const uint256& a, const base_uint256& b)
{
    return (base_uint256)a <= (base_uint256)b;
}
inline bool operator>(const uint256& a, const base_uint256& b)
{
    return (base_uint256)a > (base_uint256)b;
}
inline bool operator>=(const uint256& a, const base_uint256& b)
{
    return (base_uint256)a >= (base_uint256)b;
}
inline bool operator==(const uint256& a, const base_uint256& b)
{
    return (base_uint256)a == (base_uint256)b;
}
inline bool operator!=(const uint256& a, const base_uint256& b)
{
    return (base_uint256)a != (base_uint256)b;
}
inline const uint256 operator^(const uint256& a, const base_uint256& b)
{
    return (base_uint256)a ^ (base_uint256)b;
}
inline const uint256 operator&(const uint256& a, const base_uint256& b)
{
    return (base_uint256)a & (base_uint256)b;
}
inline const uint256 operator|(const uint256& a, const base_uint256& b)
{
    return (base_uint256)a | (base_uint256)b;
}
inline const uint256 operator+(const uint256& a, const base_uint256& b)
{
    return (base_uint256)a + (base_uint256)b;
}
inline const uint256 operator-(const uint256& a, const base_uint256& b)
{
    return (base_uint256)a - (base_uint256)b;
}

inline bool operator<(const uint256& a, const uint256& b)
{
    return (base_uint256)a < (base_uint256)b;
}
inline bool operator<=(const uint256& a, const uint256& b)
{
    return (base_uint256)a <= (base_uint256)b;
}
inline bool operator>(const uint256& a, const uint256& b)
{
    return (base_uint256)a > (base_uint256)b;
}
inline bool operator>=(const uint256& a, const uint256& b)
{
    return (base_uint256)a >= (base_uint256)b;
}
inline bool operator==(const uint256& a, const uint256& b)
{
    return (base_uint256)a == (base_uint256)b;
}
inline bool operator!=(const uint256& a, const uint256& b)
{
    return (base_uint256)a != (base_uint256)b;
}
inline const uint256 operator^(const uint256& a, const uint256& b)
{
    return (base_uint256)a ^ (base_uint256)b;
}
inline const uint256 operator&(const uint256& a, const uint256& b)
{
    return (base_uint256)a & (base_uint256)b;
}
inline const uint256 operator|(const uint256& a, const uint256& b)
{
    return (base_uint256)a | (base_uint256)b;
}
inline const uint256 operator+(const uint256& a, const uint256& b)
{
    return (base_uint256)a + (base_uint256)b;
}
inline const uint256 operator-(const uint256& a, const uint256& b)
{
    return (base_uint256)a - (base_uint256)b;
}

//////////////////////////////////////////////////////////////////////////////
//
// uint224
//

/** 224-bit unsigned integer */
class uint224 : public base_uint224
{
public:
    typedef base_uint224 basetype;

    uint224()
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] = 0;
    }

    uint224(const basetype& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] = b.pn[i];
    }

    uint224(const uint256& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] = b.pn[i];
    }

    uint224& operator=(const basetype& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] = b.pn[i];
        return *this;
    }

    uint224(const uint64 b)
    {
        pn[0] = (unsigned int)b;
        pn[1] = (unsigned int)(b >> 32);
        for (int i = 2; i < WIDTH; i++)
            pn[i] = 0;
    }

    uint224& operator=(uint64 b)
    {
        pn[0] = (unsigned int)b;
        pn[1] = (unsigned int)(b >> 32);
        for (int i = 2; i < WIDTH; i++)
            pn[i] = 0;
        return *this;
    }

    explicit uint224(const std::string& str)
    {
        SetHex(str);
    }

    explicit uint224(const std::vector<unsigned char>& vch)
    {
        if (vch.size() == sizeof(pn))
            memcpy(pn, &vch[0], sizeof(pn));
        else
            *this = 0;
    }
};

inline bool operator<(const base_uint224& a, const uint224& b)
{
    return (base_uint224)a < (base_uint224)b;
}
inline bool operator<=(const base_uint224& a, const uint224& b)
{
    return (base_uint224)a <= (base_uint224)b;
}
inline bool operator>(const base_uint224& a, const uint224& b)
{
    return (base_uint224)a > (base_uint224)b;
}
inline bool operator>=(const base_uint224& a, const uint224& b)
{
    return (base_uint224)a >= (base_uint224)b;
}
inline bool operator==(const base_uint224& a, const uint224& b)
{
    return (base_uint224)a == (base_uint224)b;
}
inline bool operator!=(const base_uint224& a, const uint224& b)
{
    return (base_uint224)a != (base_uint224)b;
}
inline bool operator<(const uint224& a, const base_uint224& b)
{
    return (base_uint224)a < (base_uint224)b;
}
inline bool operator<=(const uint224& a, const base_uint224& b)
{
    return (base_uint224)a <= (base_uint224)b;
}
inline bool operator>(const uint224& a, const base_uint224& b)
{
    return (base_uint224)a > (base_uint224)b;
}
inline bool operator>=(const uint224& a, const base_uint224& b)
{
    return (base_uint224)a >= (base_uint224)b;
}
inline bool operator==(const uint224& a, const base_uint224& b)
{
    return (base_uint224)a == (base_uint224)b;
}
inline bool operator!=(const uint224& a, const base_uint224& b)
{
    return (base_uint224)a != (base_uint224)b;
}
inline bool operator<(const uint224& a, const uint224& b)
{
    return (base_uint224)a < (base_uint224)b;
}
inline bool operator<=(const uint224& a, const uint224& b)
{
    return (base_uint224)a <= (base_uint224)b;
}
inline bool operator>(const uint224& a, const uint224& b)
{
    return (base_uint224)a > (base_uint224)b;
}
inline bool operator>=(const uint224& a, const uint224& b)
{
    return (base_uint224)a >= (base_uint224)b;
}
inline bool operator==(const uint224& a, const uint224& b)
{
    return (base_uint224)a == (base_uint224)b;
}
inline bool operator!=(const uint224& a, const uint224& b)
{
    return (base_uint224)a != (base_uint224)b;
}

#endif // CRYPTO_UINT256_H
