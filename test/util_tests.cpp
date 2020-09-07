// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.h"

#include <boost/test/unit_test.hpp>

#include "test_big.h"

using namespace xengine;

BOOST_FIXTURE_TEST_SUITE(util_tests, BasicUtfSetup)

BOOST_AUTO_TEST_CASE(util)
{
    double a = -0.1, b = 0.1;
    BOOST_CHECK(!IsDoubleEqual(a, b));

    a = 1.23456, b = 1.23455;
    BOOST_CHECK(!IsDoubleEqual(a, b));

    a = -1, b = -1.0;
    BOOST_CHECK(IsDoubleEqual(a, b));

    a = -1, b = -1;
    BOOST_CHECK(IsDoubleEqual(a, b));

    a = 0.1234567, b = 0.1234567;
    BOOST_CHECK(IsDoubleEqual(a, b));
}

BOOST_AUTO_TEST_SUITE_END()
