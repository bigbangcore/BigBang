// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto.h"

#include <boost/test/unit_test.hpp>
#include <sodium.h>

#include "crypto.h"
#include "curve25519/curve25519.h"
#include "test_big.h"
#include "util.h"

using namespace bigbang::crypto;
using namespace curve25519;
using namespace std;

BOOST_FIXTURE_TEST_SUITE(crypto_tests, BasicUtfSetup)

BOOST_AUTO_TEST_CASE(multisign)
{
    srand(time(0));

    int64_t signCount = 0, signTime = 0, verifyTime = 0;
    int count = 100;
    for (int i = 0; i < count; i++)
    {
        uint32_t nKey = CryptoGetRand32() % 256 + 1;
        uint32_t nPartKey = CryptoGetRand32() % nKey + 1;

        CCryptoKey* keys = new CCryptoKey[nKey];
        std::set<uint256> setPubKey;
        for (int i = 0; i < nKey; i++)
        {
            CryptoMakeNewKey(keys[i]);
            setPubKey.insert(keys[i].pubkey);
        }

        uint256 msg;
        CryptoGetRand256(msg);

        std::vector<uint8_t> vchSig;
        for (int i = 0; i < nPartKey; i++)
        {
            boost::posix_time::ptime t0 = boost::posix_time::microsec_clock::universal_time();
            BOOST_CHECK(CryptoMultiSign(setPubKey, keys[i], msg.begin(), msg.size(), vchSig));
            boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::universal_time();
            signTime += (t1 - t0).ticks();
            ++signCount;
        }

        std::set<uint256> setPartKey;
        boost::posix_time::ptime t0 = boost::posix_time::microsec_clock::universal_time();
        BOOST_CHECK(CryptoMultiVerify(setPubKey, msg.begin(), msg.size(), vchSig, setPartKey) && (setPartKey.size() == nPartKey));
        boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::universal_time();
        verifyTime += (t1 - t0).ticks();
    }

    std::cout << "multisign count : " << count << ", key count: " << signCount << std::endl;
    std::cout << "multisign sign count : " << signCount << "; average time : " << signTime / signCount << "us." << std::endl;
    std::cout << "multisign verify count : " << count << "; time per count : " << verifyTime / count << "us.; time per key: " << verifyTime / signCount << "us." << std::endl;
}

class CMultiSignDefectOld
{
    using CSC25519 = curve25519::CSC25519;
    using CEdwards25519 = curve25519::CEdwards25519;

public:
    static bool CryptoMultiSignDefect(const set<uint256>& setPubKey, const CCryptoKey& privkey, const uint8* pX, const size_t lenX,
                                      const uint8* pM, const size_t lenM, vector<uint8>& vchSig)
    {
        if (setPubKey.empty())
        {
            return false;
        }

        // unpack (index,R,S)
        CSC25519 S;
        CEdwards25519 R;
        int nIndexLen = (setPubKey.size() - 1) / 8 + 1;
        if (vchSig.empty())
        {
            vchSig.resize(nIndexLen + 64);
        }
        else
        {
            if (vchSig.size() != nIndexLen + 64)
            {
                return false;
            }
            if (!R.Unpack(&vchSig[nIndexLen]))
            {
                return false;
            }
            S.Unpack(&vchSig[nIndexLen + 32]);
        }
        uint8* pIndex = &vchSig[0];

        // apk
        uint256 apk;
        if (!MultiSignApkDefect(setPubKey, setPubKey, apk.begin()))
        {
            return false;
        }
        // H(X,apk,M)
        CSC25519 hash = MultiSignHashDefect(pX, lenX, apk.begin(), apk.size(), pM, lenM);

        // sign
        set<uint256>::const_iterator itPub = setPubKey.find(privkey.pubkey);
        if (itPub == setPubKey.end())
        {
            return false;
        }
        size_t index = distance(setPubKey.begin(), itPub);

        if (!(pIndex[index / 8] & (1 << (index % 8))))
        {
            // ri = H(H(si,pi),M)
            CSC25519 ri = MultiSignRDefect(privkey, pM, lenM);
            // hi = H(Ai,A1,...,An)
            vector<uint8> vecHash = MultiSignPreApkDefect(setPubKey);
            memcpy(&vecHash[0], itPub->begin(), itPub->size());
            CSC25519 hi = CSC25519(CryptoHash(&vecHash[0], vecHash.size()).begin());
            // si = H(privkey)
            CSC25519 si = ClampPrivKeyDefect(privkey.secret);
            // Si = ri + H(X,apk,M) * hi * si
            CSC25519 Si = ri + hash * hi * si;
            // S += Si
            S += Si;
            // R += Ri
            CEdwards25519 Ri;
            Ri.Generate(ri);
            R += Ri;

            pIndex[index / 8] |= (1 << (index % 8));
        }

        R.Pack(&vchSig[nIndexLen]);
        S.Pack(&vchSig[nIndexLen + 32]);

        return true;
    }

    static bool CryptoMultiVerifyDefect(const set<uint256>& setPubKey, const uint8* pX, const size_t lenX,
                                        const uint8* pM, const size_t lenM, const vector<uint8>& vchSig, set<uint256>& setPartKey)
    {
        if (setPubKey.empty())
        {
            return false;
        }

        if (vchSig.empty())
        {
            return true;
        }

        // unpack (index,R,S)
        CSC25519 S;
        CEdwards25519 R;
        int nIndexLen = (setPubKey.size() - 1) / 8 + 1;
        if (vchSig.size() != (nIndexLen + 64))
        {
            return false;
        }
        if (!R.Unpack(&vchSig[nIndexLen]))
        {
            return false;
        }
        S.Unpack(&vchSig[nIndexLen + 32]);
        const uint8* pIndex = &vchSig[0];

        // apk
        uint256 apk;
        if (!MultiSignApkDefect(setPubKey, setPubKey, apk.begin()))
        {
            return false;
        }
        // H(X,apk,M)
        CSC25519 hash = MultiSignHashDefect(pX, lenX, apk.begin(), apk.size(), pM, lenM);

        // A = hi*Ai + ... + aj*Aj
        CEdwards25519 A;
        vector<uint8> vecHash = MultiSignPreApkDefect(setPubKey);
        setPartKey.clear();
        int i = 0;

        for (auto itPub = setPubKey.begin(); itPub != setPubKey.end(); ++itPub, ++i)
        {
            if (pIndex[i / 8] & (1 << (i % 8)))
            {
                // hi = H(Ai,A1,...,An)
                memcpy(&vecHash[0], itPub->begin(), itPub->size());
                CSC25519 hi = CSC25519(CryptoHash(&vecHash[0], vecHash.size()).begin());
                // hi * Ai
                CEdwards25519 Ai;
                if (!Ai.Unpack(itPub->begin()))
                {
                    return false;
                }
                A += Ai.ScalarMult(hi);

                setPartKey.insert(*itPub);
            }
        }

        // SB = R + H(X,apk,M) * (hi*Ai + ... + aj*Aj)
        CEdwards25519 SB;
        SB.Generate(S);

        return SB == R + A.ScalarMult(hash);
    }

private:
    static vector<uint8> MultiSignPreApkDefect(const set<uint256>& setPubKey)
    {
        vector<uint8> vecHash;
        vecHash.resize((setPubKey.size() + 1) * uint256::size());

        int i = uint256::size();
        for (const uint256& key : setPubKey)
        {
            memcpy(&vecHash[i], key.begin(), key.size());
            i += key.size();
        }

        return vecHash;
    }

    // H(Ai,A1,...,An)*Ai + ... + H(Aj,A1,...,An)*Aj
    // setPubKey = [A1 ... An], setPartKey = [Ai ... Aj], 1 <= i <= j <= n
    static bool MultiSignApkDefect(const set<uint256>& setPubKey, const set<uint256>& setPartKey, uint8* md32)
    {
        vector<uint8> vecHash = MultiSignPreApkDefect(setPubKey);

        CEdwards25519 apk;
        for (const uint256& key : setPartKey)
        {
            memcpy(&vecHash[0], key.begin(), key.size());
            CSC25519 hash = CSC25519(CryptoHash(&vecHash[0], vecHash.size()).begin());

            CEdwards25519 point;
            if (!point.Unpack(key.begin()))
            {
                return false;
            }
            apk += point.ScalarMult(hash);
        }

        apk.Pack(md32);
        return true;
    }

    // hash = H(X,apk,M)
    static CSC25519 MultiSignHashDefect(const uint8* pX, const size_t lenX, const uint8* pApk, const size_t lenApk, const uint8* pM, const size_t lenM)
    {
        uint8 hash[32];

        crypto_generichash_blake2b_state state;
        crypto_generichash_blake2b_init(&state, nullptr, 0, 32);
        crypto_generichash_blake2b_update(&state, pX, lenX);
        crypto_generichash_blake2b_update(&state, pApk, lenApk);
        crypto_generichash_blake2b_update(&state, pM, lenM);
        crypto_generichash_blake2b_final(&state, hash, 32);

        return CSC25519(hash);
    }

    // r = H(H(sk,pk),M)
    static CSC25519 MultiSignRDefect(const CCryptoKey& key, const uint8* pM, const size_t lenM)
    {
        uint8 hash[32];

        crypto_generichash_blake2b_state state;
        crypto_generichash_blake2b_init(&state, NULL, 0, 32);
        uint256 keyHash = CryptoHash((uint8*)&key, 64);
        crypto_generichash_blake2b_update(&state, (uint8*)&keyHash, 32);
        crypto_generichash_blake2b_update(&state, pM, lenM);
        crypto_generichash_blake2b_final(&state, hash, 32);

        return CSC25519(hash);
    }

    static CSC25519 ClampPrivKeyDefect(const uint256& privkey)
    {
        uint8 hash[64];
        crypto_hash_sha512(hash, privkey.begin(), privkey.size());
        hash[0] &= 248;
        hash[31] &= 127;
        hash[31] |= 64;

        return CSC25519(hash);
    }
};

BOOST_AUTO_TEST_CASE(multisign_defect)
{
    srand(time(0));

    int64_t signCount = 0, signTime1 = 0, verifyTime1 = 0, signTime2 = 0, verifyTime2 = 0;
    int count = 5;
    for (int i = 0; i < count; i++)
    {
        uint32_t nKey = CryptoGetRand32() % 256 + 1;
        uint32_t nPartKey = CryptoGetRand32() % nKey + 1;

        CCryptoKey* keys = new CCryptoKey[nKey];
        std::set<uint256> setPubKey;
        for (int i = 0; i < nKey; i++)
        {
            CryptoMakeNewKey(keys[i]);
            setPubKey.insert(keys[i].pubkey);
        }

        uint256 msg;
        CryptoGetRand256(msg);
        uint256 seed;
        CryptoGetRand256(seed);

        std::vector<uint8_t> vchSig;
        for (int i = 0; i < nPartKey; i++)
        {
            boost::posix_time::ptime t0 = boost::posix_time::microsec_clock::universal_time();
            BOOST_CHECK(CryptoMultiSignDefect(setPubKey, keys[i], seed.begin(), seed.size(), msg.begin(), msg.size(), vchSig));
            boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::universal_time();
            signTime1 += (t1 - t0).ticks();
            ++signCount;
        }

        std::vector<uint8_t> vchSigOld;
        for (int i = 0; i < nPartKey; i++)
        {
            boost::posix_time::ptime t0 = boost::posix_time::microsec_clock::universal_time();
            BOOST_CHECK(CMultiSignDefectOld::CryptoMultiSignDefect(setPubKey, keys[i], seed.begin(), seed.size(), msg.begin(), msg.size(), vchSigOld));
            boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::universal_time();
            signTime2 += (t1 - t0).ticks();
        }
        BOOST_CHECK(xengine::ToHexString(vchSig) == xengine::ToHexString(vchSigOld));

        {
            boost::posix_time::ptime t0 = boost::posix_time::microsec_clock::universal_time();
            std::set<uint256> setPartKey;
            BOOST_CHECK(CryptoMultiVerifyDefect(setPubKey, seed.begin(), seed.size(), msg.begin(), msg.size(), vchSig, setPartKey) && (setPartKey.size() == nPartKey));
            boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::universal_time();
            verifyTime1 += (t1 - t0).ticks();
        }

        {
            boost::posix_time::ptime t0 = boost::posix_time::microsec_clock::universal_time();
            std::set<uint256> setPartKeyOld;
            BOOST_CHECK(CMultiSignDefectOld::CryptoMultiVerifyDefect(setPubKey, seed.begin(), seed.size(), msg.begin(), msg.size(), vchSigOld, setPartKeyOld) && (setPartKeyOld.size() == nPartKey));
            boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::universal_time();
            verifyTime2 += (t1 - t0).ticks();
        }
    }

    std::cout << "multisign count : " << count << ", key count: " << signCount << std::endl;
    std::cout << "multisign sign1 count : " << signCount << "; average time : " << signTime1 / signCount << "us." << std::endl;
    std::cout << "multisign verify1 count : " << count << "; time per count : " << verifyTime1 / count << "us.; time per key: " << verifyTime1 / signCount << "us." << std::endl;
    std::cout << "multisign sign2 count : " << signCount << "; average time : " << signTime2 / signCount << "us." << std::endl;
    std::cout << "multisign verify2 count : " << count << "; time per count : " << verifyTime2 / count << "us.; time per key: " << verifyTime2 / signCount << "us." << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()
