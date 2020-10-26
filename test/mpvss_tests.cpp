// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mpvss.h"

#include <boost/random.hpp>
#include <boost/test/unit_test.hpp>
#include <iterator>
#include <map>
#include <vector>

#include "boost/multiprecision/cpp_int.hpp"
#include "crypto.h"
#include "mpinterpolation.h"
#include "stream/datastream.h"
#include "test_big.h"
#include "util.h"

using namespace bigbang::crypto;
using namespace std;

#if BOOST_VERSION >= 106000
#define EXPORT_BITS(src, dest, size) boost::multiprecision::export_bits(src, dest, 8, false)
#else
#define EXPORT_BITS(src, dest, size) memcpy(dest, &src, size)
#endif

BOOST_FIXTURE_TEST_SUITE(mpvss_tests, BasicUtfSetup)

void RandGeneretor(uint8_t* p, size_t size = 32)
{
    for (int i = 0; i < size; i++)
    {
        *p++ = rand();
    }
}

void KeyGenerator(uint256& priv, uint256& pub)
{
    uint8_t md32[32];

    RandGeneretor(md32);
    md32[31] &= 0x0F;
    memcpy(priv.begin(), md32, 32);

    CEdwards25519 P;
    P.Generate(priv);
    P.Pack(pub.begin());
}

BOOST_AUTO_TEST_CASE(base25519)
{
    // n[0, 15] = 0
    // void Zero16(void* b);
    {
        uint8_t n[16];
        RandGeneretor(n, 16);
        curve25519::Zero16(n);
        uint8_t answer[16] = { 0 };
        BOOST_CHECK(memcmp(n, answer, 16) == 0);
    }

    // n[0, 31] = 0
    // void Zero32(void* b);
    {
        uint8_t n[32];
        RandGeneretor(n, 32);
        curve25519::Zero32(n);
        uint8_t answer[32] = { 0 };
        BOOST_CHECK(memcmp(n, answer, 32) == 0);
    }

    // n[0, 63] = 0
    // void Zero64(void* b);
    {
        uint8_t n[64];
        RandGeneretor(n, 64);
        curve25519::Zero64(n);
        uint8_t answer[64] = { 0 };
        BOOST_CHECK(memcmp(n, answer, 64) == 0);
    }

    // dest[0, 31] = src[0, 31]
    // void Copy32(void* dest, const void* src);
    {
        uint8_t src[32];
        RandGeneretor(src, 32);
        uint8_t dest[32];
        RandGeneretor(dest, 32);
        curve25519::Copy32(dest, src);
        uint8_t answer[32];
        memcpy(answer, src, 32);
        BOOST_CHECK(memcmp(dest, answer, 32) == 0);
    }

    // dest[0, 7] = u64, dest[8, 31] = u64
    // void Copy32(void* dest, const uint64_t u64);
    {
        uint64_t src;
        RandGeneretor((uint8_t*)&src, 8);
        uint8_t dest[32];
        RandGeneretor(dest, 32);
        curve25519::Copy32(dest, src);
        uint8_t answer[32] = { 0 };
        memcpy(answer, &src, 8);
        BOOST_CHECK(memcmp(dest, answer, 32) == 0);
    }

    // if uint(lhs[0, 31]) > uint(rhs[0, 31]), return 1
    // if uint(lhs[0, 31]) == uint(rhs[0, 31], return 0
    // if uint(lhs[0, 31]) < uint(rhs[0, 31], return -1
    // int Compare32(const void* lhs, const void* rhs);
    {
        uint8_t lhs[32];
        RandGeneretor(lhs, 32);
        uint8_t rhs[32];
        for (int i = 0; i < 32; i++)
        {
            curve25519::Copy32(rhs, lhs);
            if (lhs[i] > 0)
            {
                rhs[i]--;
                BOOST_CHECK(curve25519::Compare32(lhs, rhs) > 0);
                rhs[i]++;
                BOOST_CHECK(curve25519::Compare32(lhs, rhs) == 0);
                rhs[i]++;
                BOOST_CHECK(curve25519::Compare32(lhs, rhs) < 0);
            }
            else
            {
                rhs[i]--;
                BOOST_CHECK(curve25519::Compare32(lhs, rhs) < 0);
                rhs[i]++;
                BOOST_CHECK(curve25519::Compare32(lhs, rhs) == 0);
                rhs[i]++;
                BOOST_CHECK(curve25519::Compare32(lhs, rhs) < 0);
            }
        }
    }
    // if int(lhs[0, 31]) > int(rhs[0, 31]), return 1
    // if int(lhs[0, 31]) == int(rhs[0, 31], return 0
    // if int(lhs[0, 31]) < int(rhs[0, 31], return -1
    // int CompareSigned32(const void* lhs, const void* rhs);
    {
        uint8_t lhs[32];
        RandGeneretor(lhs, 32);
        uint8_t rhs[32];
        curve25519::Copy32(rhs, lhs);
        if (lhs[31] & 0x80)
        {
            rhs[31] &= 0x7F;
            BOOST_CHECK(curve25519::CompareSigned32(lhs, rhs) < 0);
        }
        else
        {
            rhs[31] |= 0x80;
            BOOST_CHECK(curve25519::CompareSigned32(lhs, rhs) > 0);
        }

        rhs[31] = lhs[31] = 0x00;
        rhs[31] = (uint8_t)((int8_t)rhs[31] - 1);
        BOOST_CHECK(curve25519::CompareSigned32(lhs, rhs) > 0);
        rhs[31] = lhs[31] = 0x00;
        rhs[31] = (uint8_t)((int8_t)rhs[31] + 1);
        BOOST_CHECK(curve25519::CompareSigned32(lhs, rhs) < 0);
        rhs[31] = lhs[31] = 0x71;
        rhs[31] = (uint8_t)((int8_t)rhs[31] - 1);
        BOOST_CHECK(curve25519::CompareSigned32(lhs, rhs) > 0);
        rhs[31] = lhs[31] = 0x81;
        rhs[31] = (uint8_t)((int8_t)rhs[31] - 1);
        BOOST_CHECK(curve25519::CompareSigned32(lhs, rhs) > 0);

        for (int i = 0; i < 31; i++)
        {
            curve25519::Copy32(rhs, lhs);
            if (lhs[i] > 0)
            {
                rhs[i]--;
                BOOST_CHECK(curve25519::CompareSigned32(lhs, rhs) > 0);
                rhs[i]++;
                BOOST_CHECK(curve25519::CompareSigned32(lhs, rhs) == 0);
                rhs[i]++;
                BOOST_CHECK(curve25519::CompareSigned32(lhs, rhs) < 0);
            }
            else
            {
                rhs[i]--;
                BOOST_CHECK(curve25519::CompareSigned32(lhs, rhs) < 0);
                rhs[i]++;
                BOOST_CHECK(curve25519::CompareSigned32(lhs, rhs) == 0);
                rhs[i]++;
                BOOST_CHECK(curve25519::CompareSigned32(lhs, rhs) < 0);
            }
        }
    }

    // if n >= prime, output = n - prime
    // if n < 0, output = n + prime
    // void ReduceSigned32(void* output, const void* n, const void* prime);
    {
        uint8_t prime[32];
        RandGeneretor(prime, 32);
        prime[31] &= 0x7F;
        prime[0] = 0x04;

        uint8_t n[32];
        uint8_t answer[32];

        // n = -1
        memset(n, 0xFF, 32);
        curve25519::ReduceSigned32(n, n, prime);
        curve25519::Copy32(answer, prime);
        answer[0] = 0x03;
        BOOST_CHECK(memcmp(n, answer, 32) == 0);

        // n = prime + 1;
        curve25519::Copy32(n, prime);
        n[0] = 0x05;
        curve25519::ReduceSigned32(n, n, prime);
        memset(answer, 0, 32);
        answer[0] = 0x01;
        BOOST_CHECK(memcmp(n, answer, 32) == 0);
    }

    // return n[0, 31] == 0
    // bool IsZero32(const void* n);
    {
        uint8_t n[32] = { 0 };
        BOOST_CHECK(curve25519::IsZero32(n));
        RandGeneretor(n, 32);
        for (int i = 0; i < 32; i++)
        {
            if (i & 0xFF)
            {
                BOOST_CHECK(!curve25519::IsZero32(n));
                break;
            }
        }
    }

    // return n[0, 31] == 1
    // bool IsOne32(const void* n);
    {
        uint8_t n[32] = { 0x01 };
        BOOST_CHECK(curve25519::IsOne32(n));
        RandGeneretor(n, 32);
        for (int i = 0; i < 31; i++)
        {
            if (i & 0xFF)
            {
                BOOST_CHECK(!curve25519::IsOne32(n));
                break;
            }
        }
    }

    // return n[0] & 1 == 0
    // bool IsEven(const void* n);
    {
        uint8_t n[32];
        RandGeneretor(n, 32);
        n[0] |= 0x01;
        BOOST_CHECK(!curve25519::IsEven(n));
        n[0] &= 0xFE;
        BOOST_CHECK(curve25519::IsEven(n));
    }

    // uint(n[0, 31]) /= 2
    // void Half32(void* n);
    {
        uint8_t n[32];
        RandGeneretor(n, 32);
        uint8_t answer[32];
        for (int i = 0; i < 31; i++)
        {
            answer[i] = (n[i] >> 1) | (n[i + 1] << 7);
        }
        answer[31] = n[31] >> 1;
        curve25519::Half32(n);
        BOOST_CHECK(memcmp(n, answer, 32) == 0);
    }

    // int(n[0, 31]) /= 2
    // void HalfSigned32(void* n);
    {
        uint8_t n[32];
        RandGeneretor(n, 32);
        uint8_t answer[32];

        n[31] |= 0x80;
        for (int i = 0; i < 31; i++)
        {
            answer[i] = (n[i] >> 1) | (n[i + 1] << 7);
        }
        answer[31] = (n[31] >> 1) | 0x80;
        curve25519::HalfSigned32(n);
        BOOST_CHECK(memcmp(n, answer, 32) == 0);

        n[31] &= 0x7F;
        for (int i = 0; i < 31; i++)
        {
            answer[i] = (n[i] >> 1) | (n[i + 1] << 7);
        }
        answer[31] = n[31] >> 1;
        curve25519::HalfSigned32(n);
        BOOST_CHECK(memcmp(n, answer, 32) == 0);
    }

    // uint(n[0, 31]) <<= b
    // void ShiftLeft32(void* output, const void* n, size_t b);
    {
        uint8_t n[32];
        RandGeneretor(n, 32);
        uint8_t output[32];
        uint8_t answer[32];
        size_t b;

        auto calc = [=](uint8_t* answer, size_t b) {
            for (int i = 31; i > (b / 8); i--)
            {
                answer[i] = (n[i - (b / 8)] << (b % 8)) | (n[i - (b / 8 + 1)] >> (8 - b % 8));
            }
            answer[b / 8] = (n[0] << (b % 8));
            memset(answer, 0, b / 8);
        };

        b = 0;
        curve25519::ShiftLeft32(output, n, b);
        memcpy(answer, n, 32);
        BOOST_CHECK(memcmp(output, answer, 32) == 0);

        b = 5;
        curve25519::ShiftLeft32(output, n, b);
        calc(answer, b);
        BOOST_CHECK(memcmp(output, answer, 32) == 0);

        // b = 64
        b = 64;
        curve25519::ShiftLeft32(output, n, b);
        memcpy(answer + 8, n, 24);
        memset(answer, 0, 8);
        BOOST_CHECK(memcmp(output, answer, 32) == 0);

        // b = (64, 128)
        b = rand() % 63 + 65;
        curve25519::ShiftLeft32(output, n, b);
        calc(answer, b);
        BOOST_CHECK(memcmp(output, answer, 32) == 0);

        // 128
        b = 128;
        curve25519::ShiftLeft32(output, n, b);
        memcpy(answer + 16, n, 16);
        memset(answer, 0, 16);
        BOOST_CHECK(memcmp(output, answer, 32) == 0);

        // b = (128, 192)
        b = rand() % 63 + 129;
        curve25519::ShiftLeft32(output, n, b);
        calc(answer, b);
        BOOST_CHECK(memcmp(output, answer, 32) == 0);

        // 192
        b = 192;
        curve25519::ShiftLeft32(output, n, b);
        memcpy(answer + 24, n, 8);
        memset(answer, 0, 24);
        BOOST_CHECK(memcmp(output, answer, 32) == 0);

        // b = (192, 256)
        b = rand() % 63 + 193;
        curve25519::ShiftLeft32(output, n, b);
        calc(answer, b);
        BOOST_CHECK(memcmp(output, answer, 32) == 0);

        // 256
        b = 256;
        curve25519::ShiftLeft32(output, n, b);
        memset(answer, 0, 32);
        BOOST_CHECK(memcmp(output, answer, 32) == 0);

        // (256, +)
        b = rand() + 257;
        curve25519::ShiftLeft32(output, n, b);
        memset(answer, 0, 32);
        BOOST_CHECK(memcmp(output, answer, 32) == 0);
    }

    // output[0, 31] = n1[0, 31] + n2[0, 31]. return carry = 0 or 1
    // uint32_t Add32(void* output, const void* n1, const void* n2);
    {
        uint8_t n1[32];
        uint8_t n2[32];
        uint8_t output[32];
        uint8_t answer[32];

        typedef boost::random::independent_bits_engine<boost::random::mt19937, 256, boost::multiprecision::uint256_t> generator_type;
        generator_type gen;
        boost::multiprecision::uint256_t m1 = gen();
        EXPORT_BITS(m1, n1, 32);

        boost::multiprecision::uint256_t m2 = gen();
        EXPORT_BITS(m2, n2, 32);

        boost::multiprecision::uint256_t m = m1 + m2;
        EXPORT_BITS(m, answer, 32);

        curve25519::Add32(output, n1, n2);
        BOOST_CHECK(memcmp(output, answer, 32) == 0);
    }

    // output[0, 31] = n1[0, 31] - n2[0, 31]. return borrow = 0 or 1
    // uint32_t Sub32(void* output, const void* n1, const void* n2);
    {
        uint8_t n1[32];
        uint8_t n2[32];
        uint8_t output[32];
        uint8_t answer[32];

        typedef boost::random::independent_bits_engine<boost::random::mt19937, 256, boost::multiprecision::uint256_t> generator_type;
        generator_type gen;
        boost::multiprecision::uint256_t m1 = gen();
        EXPORT_BITS(m1, n1, 32);

        boost::multiprecision::uint256_t m2 = gen();
        EXPORT_BITS(m2, n2, 32);

        boost::multiprecision::uint256_t m = m1 - m2;
        EXPORT_BITS(m, answer, 32);

        curve25519::Sub32(output, n1, n2);
        BOOST_CHECK(memcmp(output, answer, 32) == 0);
    }

    // output[0, 127] = n1[0, 31] * n2[0, 31].
    // output must be initialed by 0.
    // output[i*16, i*16+7] is 64-bits product, [i*16+8, i*16+15] is carry, 0 <= i < 8
    // void Mul32(void* output, const void* n1, const void* n2);

    // output[0, 15] = n1[0, 7] * n2[0, 7]
    // void Multiply8_8(void* output, const void* n1, const void* n2);
    {
        uint8_t n1[8] = { 0 };
        uint8_t n2[8] = { 0 };
        uint8_t output[16];
        uint8_t answer[16] = { 0 };

        uint64_t m1;
        RandGeneretor((uint8_t*)&m1, 8);
        memcpy(n1, &m1, 8);

        uint64_t m2;
        RandGeneretor((uint8_t*)&m2, 8);
        memcpy(n2, &m2, 8);

        boost::multiprecision::uint128_t m = boost::multiprecision::uint128_t(m1) * m2;
        EXPORT_BITS(m, answer, 16);

        curve25519::Multiply8_8(output, n1, n2);
        BOOST_CHECK(memcmp(output, answer, 16) == 0);
    }

    // output[0, 31] = n1[0, 15] * n2[0, 7]
    // void Multiply16_8(void* output, const void* n1, const void* n2);
    {
        uint8_t n1[16] = { 0 };
        uint8_t n2[8] = { 0 };
        uint8_t output[32];
        uint8_t answer[32] = { 0 };

        typedef boost::random::independent_bits_engine<boost::random::mt19937, 256, boost::multiprecision::uint128_t> generator_type;
        generator_type gen;
        boost::multiprecision::uint128_t m1 = gen();
        EXPORT_BITS(m1, n1, 16);

        uint64_t m2;
        RandGeneretor((uint8_t*)&m2, 8);
        memcpy(n2, &m2, 8);

        boost::multiprecision::uint256_t m = (boost::multiprecision::uint256_t)m1 * m2;
        EXPORT_BITS(m, answer, 24);

        curve25519::Multiply16_8(output, n1, n2);
        BOOST_CHECK(memcmp(output, answer, 32) == 0);
    }

    // output[0, 63] = n1[0, 31] * n2[0, 31]
    // void Multiply32_32(void* output, const void* n1, const void* n2);
    {
        uint8_t n1[32] = { 0 };
        uint8_t n2[32] = { 0 };
        uint8_t output[64];
        uint8_t answer[64] = { 0 };

        typedef boost::random::independent_bits_engine<boost::random::mt19937, 256, boost::multiprecision::uint256_t> generator_type;
        generator_type gen;
        boost::multiprecision::uint256_t m1 = gen();
        EXPORT_BITS(m1, n1, 32);

        boost::multiprecision::uint256_t m2 = gen();
        EXPORT_BITS(m2, n2, 32);

        boost::multiprecision::uint512_t m = (boost::multiprecision::uint512_t)m1 * m2;
        EXPORT_BITS(m, answer, 64);

        curve25519::Multiply32_32(output, n1, n2);
        BOOST_CHECK(memcmp(output, answer, 64) == 0);
    }
}

BOOST_AUTO_TEST_CASE(fp25519)
{
    srand(time(0));
    uint8_t md32[32];

    // add, minus
    for (int i = 0; i < 10; i++)
    {
        RandGeneretor(md32);
        CFP25519 add1(md32);
        RandGeneretor(md32);
        CFP25519 add2(md32);
        CFP25519 fpSum = add1 + add2;
        BOOST_CHECK(add1 == fpSum - add2);
        BOOST_CHECK(add2 == fpSum - add1);
    }

    // multiply, divide
    for (int i = 0; i < 10; i++)
    {
        RandGeneretor(md32);
        CFP25519 mul1(md32);
        RandGeneretor(md32);
        CFP25519 mul2(md32);
        CFP25519 fpProduct = mul1 * mul2;
        BOOST_CHECK(mul1 == fpProduct / mul2);
        BOOST_CHECK(mul2 == fpProduct / mul1);
    }

    // inverse
    for (int i = 0; i < 10; i++)
    {
        RandGeneretor(md32);
        CFP25519 fp(md32);
        CFP25519 fpInverse = fp.Inverse();
        BOOST_CHECK(CFP25519(1) == fp * fpInverse);
    }

    // sqrt square
    for (int i = 0; i < 10; i++)
    {
        RandGeneretor(md32);
        CFP25519 fp1(md32);
        CFP25519 fp2(md32);
        fp2 = fp2.Square().Sqrt();
        BOOST_CHECK(fp2.Square() == fp1.Square());
    }
}

BOOST_AUTO_TEST_CASE(sc25519)
{
    srand(time(0));
    uint8_t md32[32];

    // add, minus
    for (int i = 0; i < 10; i++)
    {
        RandGeneretor(md32);
        CSC25519 add1(md32);
        RandGeneretor(md32);
        CSC25519 add2(md32);
        CSC25519 scSum = add1 + add2;
        BOOST_CHECK(add1 == scSum - add2);
        BOOST_CHECK(add2 == scSum - add1);
    }

    // multiply
    for (int i = 0; i < 10; i++)
    {
        RandGeneretor(md32);
        CSC25519 sc(md32);
        RandGeneretor(md32);
        uint32_t n = 10;
        CSC25519 scProduct = sc * n;
        for (int i = 0; i < n - 1; i++)
        {
            scProduct -= sc;
        }
        BOOST_CHECK(scProduct == sc);
    }

    // negative
    for (int i = 0; i < 10; i++)
    {
        RandGeneretor(md32);
        CSC25519 sc(md32);
        CSC25519 scNegative1 = -sc;
        CSC25519 scNegative2(sc);
        scNegative2.Negative();
        BOOST_CHECK(scNegative1 == scNegative2);
        BOOST_CHECK(CSC25519() == sc + scNegative1);
    }
}

BOOST_AUTO_TEST_CASE(ed25519)
{
    srand(time(0));

    // sign, verify
    uint8_t md32[32];
    for (int i = 0; i < 10; i++)
    {
        uint256 priv1, pub1;
        uint256 priv2, pub2;
        KeyGenerator(priv1, pub1);
        KeyGenerator(priv2, pub2);

        RandGeneretor(md32);
        CSC25519 hash(md32);

        CSC25519 sign = CSC25519(priv1.begin()) + CSC25519(priv2.begin()) * hash;

        CEdwards25519 P, R, S;
        P.Unpack(pub1.begin());
        R.Unpack(pub2.begin());
        S.Generate(sign);

        BOOST_CHECK(S == (P + R.ScalarMult(hash)));
    }
}

BOOST_AUTO_TEST_CASE(interpolation)
{
    srand(time(0));
    uint8_t md32[32];

    // lagrange, newton
    for (int i = 0; i < 1; i++)
    {
        vector<uint32_t> vX;
        for (int i = 1; i < 51; i++)
        {
            vX.push_back(i);
        }

        vector<pair<uint32_t, uint256>> vShare;
        for (int i = 0; i < 26; i++)
        {
            int index = rand() % vX.size();
            uint32_t x = vX[index];
            vX.erase(vX.begin() + index);

            RandGeneretor(md32);
            uint256 y((uint64_t*)md32);
            vShare.push_back(make_pair(x, y));
        }

        BOOST_CHECK(MPLagrange(vShare) == MPNewton(vShare));
    }
}

const uint256 GetHash(const map<uint256, vector<uint256>>& mapShare)
{
    vector<unsigned char> vch;
    xengine::CODataStream os(vch);
    os << mapShare;
    return CryptoHash(&vch[0], vch.size());
}

BOOST_AUTO_TEST_CASE(mpvss)
{
    srand(time(0));
    for (size_t count = 23; count <= 23; count++)
    {
        uint256 nInitValue;
        vector<uint256> vID;
        map<uint256, CMPSecretShare> mapSS;
        vector<CMPSealedBox> vSBox;
        vector<CMPCandidate> vCandidate;

        CMPSecretShare ssWitness;

        boost::posix_time::ptime t0;
        cout << "Test mpvss begin: count " << count << "\n{\n";
        vID.resize(count);
        vSBox.resize(count);
        vCandidate.resize(count);
        //Setup
        t0 = boost::posix_time::microsec_clock::universal_time();
        for (int i = 0; i < count; i++)
        {
            vID[i] = uint256(i + 1);
            mapSS[vID[i]] = CMPSecretShare(vID[i]);

            mapSS[vID[i]].Setup(count + 1, vSBox[i]);
            vCandidate[i] = CMPCandidate(vID[i], 1, vSBox[i]);

            nInitValue = nInitValue ^ mapSS[vID[i]].myBox.vCoeff[0];
        }
        cout << "\tSetup : " << ((boost::posix_time::microsec_clock::universal_time() - t0).ticks() / count) << "\n";
        cout << "\tInit value = " << nInitValue.GetHex() << "\n";
        {
            CMPSealedBox box;
            ssWitness.Setup(count + 1, box);
        }

        //Enroll
        t0 = boost::posix_time::microsec_clock::universal_time();
        for (int i = 0; i < count; i++)
        {
            mapSS[vID[i]].Enroll(vCandidate);
        }
        cout << "\tEnroll : " << ((boost::posix_time::microsec_clock::universal_time() - t0).ticks() / count) << "\n";
        ssWitness.Enroll(vCandidate);

        // Distribute
        t0 = boost::posix_time::microsec_clock::universal_time();
        for (int i = 0; i < count; i++)
        {
            map<uint256, vector<uint256>> mapShare;
            mapSS[vID[i]].Distribute(mapShare);

            uint256 hash, nR, nS;
            hash = GetHash(mapShare);
            mapSS[vID[i]].Signature(hash, nR, nS);

            for (int j = 0; j < count; j++)
            {
                if (i != j)
                {
                    // check sign
                    BOOST_CHECK(mapSS[vID[j]].VerifySignature(vID[i], hash, nR, nS));
                    // accept
                    BOOST_CHECK(mapSS[vID[j]].Accept(vID[i], mapShare[vID[j]]));
                }
            }
        }
        cout << "\tDistribute : " << ((boost::posix_time::microsec_clock::universal_time() - t0).ticks() / count) << "\n";

        // Publish
        t0 = boost::posix_time::microsec_clock::universal_time();
        vector<map<uint256, vector<uint256>>> vecMapShare;
        vecMapShare.resize(count);

        vector<tuple<uint256, uint256, uint256>> vecPublishSign;
        vecPublishSign.resize(count);
        for (int i = 0; i < count; i++)
        {
            mapSS[vID[i]].Publish(vecMapShare[i]);

            // sign
            uint256 hash, nR, nS;
            hash = GetHash(vecMapShare[i]);
            mapSS[vID[i]].Signature(hash, nR, nS);
            vecPublishSign[i] = make_tuple(hash, nR, nS);
        }

        for (int i = 0; i < count; i++)
        {
            for (int j = 0; j < count; j++)
            {
                int indexFrom = (i + j) % count;
                // check sign
                auto& sign = vecPublishSign[indexFrom];
                BOOST_CHECK(mapSS[vID[i]].VerifySignature(vID[indexFrom], get<0>(sign), get<1>(sign), get<2>(sign)));
                // collect
                BOOST_CHECK(mapSS[vID[i]].Collect(vID[indexFrom], vecMapShare[indexFrom]));
            }
            BOOST_CHECK(mapSS[vID[i]].IsCollectCompleted());
        }

        cout << "\tPublish : " << ((boost::posix_time::microsec_clock::universal_time() - t0).ticks() / count) << "\n";

        for (int i = 0; i < count && !ssWitness.IsCollectCompleted(); i++)
        {
            // check sign
            auto& sign = vecPublishSign[i];
            BOOST_CHECK(mapSS[vID[i]].VerifySignature(vID[i], get<0>(sign), get<1>(sign), get<2>(sign)));
            // collect
            BOOST_CHECK(ssWitness.Collect(vID[i], vecMapShare[i]));
        }
        BOOST_CHECK(ssWitness.IsCollectCompleted());

        // Reconstruct
        t0 = boost::posix_time::microsec_clock::universal_time();
        for (int i = 0; i < count; i++)
        {
            uint256 nRecValue;
            map<uint256, pair<uint256, size_t>> mapSecret;
            mapSS[vID[i]].Reconstruct(mapSecret);
            for (map<uint256, pair<uint256, size_t>>::iterator it = mapSecret.begin(); it != mapSecret.end(); ++it)
            {
                nRecValue = nRecValue ^ (*it).second.first;
            }
            BOOST_CHECK(nRecValue == nInitValue);
        }
        cout << "\tReconstruct : " << ((boost::posix_time::microsec_clock::universal_time() - t0).ticks() / count) << "\n";
        ;

        uint256 nRecValue;
        map<uint256, pair<uint256, size_t>> mapSecret;
        ssWitness.Reconstruct(mapSecret);
        for (map<uint256, pair<uint256, size_t>>::iterator it = mapSecret.begin(); it != mapSecret.end(); ++it)
        {
            nRecValue = nRecValue ^ (*it).second.first;
        }
        cout << "\tReconstruct witness : " << nRecValue.GetHex() << "\n";
        cout << "}\n";

        BOOST_CHECK(nRecValue == nInitValue);
    }
}

BOOST_AUTO_TEST_SUITE_END()

