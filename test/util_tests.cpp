// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.h"
#include "profile.h"

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

BOOST_AUTO_TEST_CASE(profile)
{
    CProfile profile;
    profile.strName = "BBC Test";
    profile.strSymbol = "BBCA";
    profile.nVersion = 2;
    profile.nMinTxFee = 100;
    profile.nMintReward = 1000;
    profile.nJointHeight = 5;
    profile.nAmount = 100000;
    
    std::vector<uint8> vchProfile;
    BOOST_CHECK(profile.Save(vchProfile));

    CProfile profileLoad;
    BOOST_CHECK(profileLoad.Load(vchProfile));

    BOOST_CHECK(profileLoad.strName == profile.strName);
    BOOST_CHECK(profileLoad.strSymbol == profile.strSymbol);
    BOOST_CHECK(profileLoad.nVersion == profile.nVersion);
    BOOST_CHECK(profileLoad.nMinTxFee == profile.nMinTxFee);
    BOOST_CHECK(profileLoad.nMintReward = profile.nMintReward);
    BOOST_CHECK(profileLoad.nJointHeight = profile.nJointHeight);
    BOOST_CHECK(profileLoad.nAmount = profile.nAmount);
}

BOOST_AUTO_TEST_SUITE_END()
