// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto.h"

#include <boost/test/unit_test.hpp>

#include "test_big.h"

using namespace bigbang::crypto;

BOOST_FIXTURE_TEST_SUITE(crypto_tests, BasicUtfSetup)

BOOST_AUTO_TEST_CASE(multisign)
{
    srand(time(0));

    int64_t keyCount = 0;
    int64_t signCount = 0, signTime = 0, verifyTime = 0;
    int count = 100;
    for (int i = 0; i < count; i++)
    {
        uint32_t nKey = CryptoGetRand32() % 16 + 1;
        uint32_t nPartKey = CryptoGetRand32() % nKey + 1;

        keyCount += nKey;

        CCryptoKey* keys = new CCryptoKey[nKey];
        std::set<uint256> setPubKey;
        for (int i = 0; i < nKey; i++)
        {
            CryptoMakeNewKey(keys[i]);
            setPubKey.insert(keys[i].pubkey);
        }

        uint256 anchor;
        CryptoGetRand256(anchor);
        uint256 msg;
        CryptoGetRand256(msg);

        std::vector<uint8_t> vchSig;
        for (int i = 0; i < nPartKey; i++)
        {
            boost::posix_time::ptime t0 = boost::posix_time::microsec_clock::universal_time();
            BOOST_CHECK(CryptoMultiSign(setPubKey, keys[i], anchor.begin(), anchor.size(), msg.begin(), msg.size(), vchSig));
            boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::universal_time();
            signTime = (t1 - t0).ticks();
            ++signCount;
        }

        std::set<uint256> setPartKey;
        boost::posix_time::ptime t0 = boost::posix_time::microsec_clock::universal_time();
        BOOST_CHECK(CryptoMultiVerify(setPubKey, anchor.begin(), anchor.size(), msg.begin(), msg.size(), vchSig, setPartKey) && (setPartKey.size() == nPartKey));
        boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::universal_time();
        verifyTime += (t1 - t0).ticks();
    }

    std::cout << "multisign key count : " << keyCount << std::endl;
    std::cout << "multisign sign count : " << signCount << "; average time : " << signTime / signCount << "us." << std::endl;
    std::cout << "multisign verify count : " << count << "; time per count : " << verifyTime / count << "us."
              << " time per key count: " << verifyTime / keyCount << "us." << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()
