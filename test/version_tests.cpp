// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "version.h"

#include <boost/test/unit_test.hpp>

#include "test_big.h"

BOOST_FIXTURE_TEST_SUITE(version_tests, BasicUtfSetup)

BOOST_AUTO_TEST_CASE(format)
{
    int nVersion;
    std::string strVersion;

    // FormatVersion
    // case 1
    nVersion = 106600;
    strVersion = FormatVersion(nVersion);
    BOOST_CHECK(strVersion == "10.66.0");

    // case 2
    nVersion = 1;
    strVersion = FormatVersion(nVersion);
    BOOST_CHECK(strVersion == "0.0.1");
}

BOOST_AUTO_TEST_CASE(resolve)
{
    int nVersion, nMajor, nMinor, nRevision;
    std::string strVersion;
    bool fRet;

    // void ResolveVersion(const int nVersion, int& nMajor, int& nMinor, int& nRevision)
    // case 1
    nVersion = 106600;
    ResolveVersion(nVersion, nMajor, nMinor, nRevision);
    BOOST_CHECK(nMajor == 10);
    BOOST_CHECK(nMinor == 66);
    BOOST_CHECK(nRevision == 0);

    // case 2
    nVersion = 1;
    ResolveVersion(nVersion, nMajor, nMinor, nRevision);
    BOOST_CHECK(nMajor == 0);
    BOOST_CHECK(nMinor == 0);
    BOOST_CHECK(nRevision == 1);

    // bool ResolveVersion(const std::string& strVersion, int& nMajor, int& nMinor, int& nRevision)
    // case 1
    strVersion = "106600";
    fRet = ResolveVersion(strVersion, nMajor, nMinor, nRevision);
    BOOST_CHECK(fRet);
    BOOST_CHECK(nMajor == 10);
    BOOST_CHECK(nMinor == 66);
    BOOST_CHECK(nRevision == 0);

    // case 2
    strVersion = "1";
    fRet = ResolveVersion(strVersion, nMajor, nMinor, nRevision);
    BOOST_CHECK(fRet);
    BOOST_CHECK(nMajor == 0);
    BOOST_CHECK(nMinor == 0);
    BOOST_CHECK(nRevision == 1);

    // case 3
    strVersion = "10.66.00";
    fRet = ResolveVersion(strVersion, nMajor, nMinor, nRevision);
    BOOST_CHECK(fRet);
    BOOST_CHECK(nMajor == 10);
    BOOST_CHECK(nMinor == 66);
    BOOST_CHECK(nRevision == 0);

    // case 4
    strVersion = "0.0.1";
    fRet = ResolveVersion(strVersion, nMajor, nMinor, nRevision);
    BOOST_CHECK(fRet);
    BOOST_CHECK(nMajor == 0);
    BOOST_CHECK(nMinor == 0);
    BOOST_CHECK(nRevision == 1);

    // case 5
    strVersion = "0.1";
    fRet = ResolveVersion(strVersion, nMajor, nMinor, nRevision);
    BOOST_CHECK(!fRet);

    // case 6
    strVersion = "0.1.2.3";
    fRet = ResolveVersion(strVersion, nMajor, nMinor, nRevision);
    BOOST_CHECK(!fRet);
}

BOOST_AUTO_TEST_SUITE_END()
