// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "txpool.h"
#include "uint256.h"
#include "transaction.h"
#include "test_big.h"

using namespace std;
using namespace xengine;
using namespace bigbang;

BOOST_FIXTURE_TEST_SUITE(txpool_tests, BasicUtfSetup)

BOOST_AUTO_TEST_CASE(txcache_test)
{
    CTxCache cache(5);
    
    std::vector<CTransaction> vecTx;
    cache.AddNew(uint256(1, uint224()), vecTx);
    BOOST_CHECK(cache.Retrieve(uint256(1, uint224()), vecTx) == true);
    
    cache.AddNew(uint256(3, uint224()), vecTx);
    BOOST_CHECK(cache.Retrieve(uint256(1, uint224()), vecTx) == true);
    BOOST_CHECK(cache.Retrieve(uint256(3, uint224()), vecTx) == true);

    cache.AddNew(uint256(6, uint224()), vecTx);
    BOOST_CHECK(cache.Retrieve(uint256(1, uint224()), vecTx) == false);
    BOOST_CHECK(cache.Retrieve(uint256(3, uint224()), vecTx) == true);
    BOOST_CHECK(cache.Retrieve(uint256(6, uint224()), vecTx) == true);

    cache.AddNew(uint256(4, uint224()), vecTx);
    cache.AddNew(uint256(2, uint224()), vecTx);
    cache.AddNew(uint256(5, uint224()), vecTx);
    cache.AddNew(uint256(1, uint224()), vecTx);


    BOOST_CHECK(cache.Retrieve(uint256(1, uint224()), vecTx) == false);
    BOOST_CHECK(cache.Retrieve(uint256(2, uint224()), vecTx) == true);
    BOOST_CHECK(cache.Retrieve(uint256(6, uint224()), vecTx) == true);
    BOOST_CHECK(cache.Retrieve(uint256(4, uint224()), vecTx) == true);

    cache.Clear();
    vecTx.clear();

    cache.AddNew(uint256(125, uint224("1asfasf")), vecTx);
    cache.AddNew(uint256(125, uint224("awdaweawrawfasdadawd")), vecTx);
    cache.AddNew(uint256(126, uint224("126")), vecTx);
    BOOST_CHECK(cache.Retrieve(uint256(125, uint224("1asfasf")), vecTx) == true);
    BOOST_CHECK(cache.Retrieve(uint256(125, uint224("awdaweawrawfasdadawd")), vecTx) == true);

    cache.AddNew(uint256(130, uint224()), vecTx);
    cache.AddNew(uint256(120, uint224()), vecTx);
    cache.AddNew(uint256(110, uint224()), vecTx);
    cache.AddNew(uint256(110, uint224()), vecTx);
    cache.AddNew(uint256(115, uint224()), vecTx);


    BOOST_CHECK(cache.Retrieve(uint256(120, uint224()), vecTx) == false);
    BOOST_CHECK(cache.Retrieve(uint256(210, uint224()), vecTx) == false);
    BOOST_CHECK(cache.Retrieve(uint256(130, uint224()), vecTx) == true);
    BOOST_CHECK(cache.Retrieve(uint256(125, uint224()), vecTx) == false);
    BOOST_CHECK(cache.Retrieve(uint256(125, uint224("1asfasf")), vecTx) == false);
    BOOST_CHECK(cache.Retrieve(uint256(125, uint224("awdaweawrawfasdadawd")), vecTx) == false);

}

BOOST_AUTO_TEST_SUITE_END()