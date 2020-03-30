// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mpvss.h"

#include <boost/test/unit_test.hpp>
#include <map>
#include <vector>

#include "crypto.h"
#include "mpinterpolation.h"
#include "stream/datastream.h"
#include "test_big.h"

using curve25519::Print32;
using namespace bigbang::crypto;
using namespace std;

BOOST_FIXTURE_TEST_SUITE(mpvss_tests, BasicUtfSetup)

void RandGeneretor(uint8_t* p)
{
    for (int i = 0; i < 32; i++)
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

// BOOST_AUTO_TEST_CASE(fp25519)
// {
//     srand(time(0));
//     uint8_t md32[32];

//     // add, minus
//     for (int i = 0; i < 10; i++)
//     {
//         RandGeneretor(md32);
//         CFP25519 add1(md32);
//         RandGeneretor(md32);
//         CFP25519 add2(md32);
//         CFP25519 fpSum = add1 + add2;
//         BOOST_CHECK(add1 == fpSum - add2);
//         BOOST_CHECK(add2 == fpSum - add1);
//     }

//     // multiply, divide
//     for (int i = 0; i < 10; i++)
//     {
//         RandGeneretor(md32);
//         CFP25519 mul1(md32);
//         RandGeneretor(md32);
//         CFP25519 mul2(md32);
//         CFP25519 fpProduct = mul1 * mul2;
//         BOOST_CHECK(mul1 == fpProduct / mul2);
//         BOOST_CHECK(mul2 == fpProduct / mul1);
//     }

//     // inverse
//     for (int i = 0; i < 10; i++)
//     {
//         RandGeneretor(md32);
//         CFP25519 fp(md32);
//         CFP25519 fpInverse = fp.Inverse();
//         BOOST_CHECK(CFP25519(1) == fp * fpInverse);
//     }

//     // sqrt square
//     for (int i = 0; i < 10; i++)
//     {
//         RandGeneretor(md32);
//         CFP25519 fp1(md32);
//         CFP25519 fp2(md32);
//         fp2 = fp2.Square().Sqrt();
//         BOOST_CHECK(fp2.Square() == fp1.Square());
//     }
// }

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
    uint64_t md32[4] = { 0x23753fb0451ab230, 0xa03e286ea4e97588, 0x513ae37812fb6579, 0x687709b0bc557364 };
    CSC25519 k((uint8*)md32);
    CEdwards25519 P;
    P.Generate(k);
    P.Pack((uint8*)md32);
    cout << uint256(md32).ToString() << endl;

    // sign, verify
    // for (int i = 0; i < 10; i++)
    // {
    //     uint256 priv1, pub1;
    //     uint256 priv2, pub2;
    //     KeyGenerator(priv1, pub1);
    //     KeyGenerator(priv2, pub2);

    //     RandGeneretor(md32);
    //     CSC25519 hash(md32);

    //     CSC25519 sign = CSC25519(priv1.begin()) + CSC25519(priv2.begin()) * hash;

    //     CEdwards25519 P,R,S;
    //     P.Unpack(pub1.begin());
    //     R.Unpack(pub2.begin());
    //     S.Generate(sign);

    //     BOOST_CHECK( S == (P + R.ScalarMult(hash)) );
    // }
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
            bool fCompleted = false;
            for (int j = 0; j < count; j++)
            {
                int indexFrom = (i + j) % count;
                // check sign
                auto& sign = vecPublishSign[indexFrom];
                BOOST_CHECK(mapSS[vID[i]].VerifySignature(vID[indexFrom], get<0>(sign), get<1>(sign), get<2>(sign)));
                // collect
                BOOST_CHECK(mapSS[vID[i]].Collect(vID[indexFrom], vecMapShare[indexFrom], fCompleted));
            }

            BOOST_CHECK(fCompleted);
        }

        bool fCompleted = false;
        for (int i = 0; i < count; i++)
        {
            // check sign
            auto& sign = vecPublishSign[i];
            BOOST_CHECK(mapSS[vID[i]].VerifySignature(vID[i], get<0>(sign), get<1>(sign), get<2>(sign)));
            // collect
            BOOST_CHECK(ssWitness.Collect(vID[i], vecMapShare[i], fCompleted));
        }
        BOOST_CHECK(fCompleted);

        cout << "\tPublish : " << ((boost::posix_time::microsec_clock::universal_time() - t0).ticks() / count) << "\n";

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
