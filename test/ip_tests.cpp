#include <boost/asio.hpp>
#include <boost/regex.hpp>
#include <boost/test/unit_test.hpp>
#include <exception>

#include "netio/iocontainer.h"
#include "netio/nethost.h"
#include "test_big.h"
#include "xengine.h"

using namespace xengine;

BOOST_FIXTURE_TEST_SUITE(ip_tests, BasicUtfSetup)

class CTestIOInBound : public CIOInBound
{
public:
    CTestIOInBound(CIOProc* pProcIn)
      : CIOInBound(pProcIn) {}

    bool TestBuildWhiteList(const std::vector<std::string>& vAllowMask)
    {
        return BuildWhiteList(vAllowMask);
    }

    bool TestIsAllowRemote(const std::string& address)
    {
        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(address), 6666);
        return IsAllowedRemote(ep);
    }
};

BOOST_AUTO_TEST_CASE(white_list_regex)
{
    std::vector<std::string> vAllowMask{ "192.168.199.*", "2001:db8:0:f101::*",
                                         "fe80::95c3:6a70:f4e4:5c2a", "fe80::95c3:6a70:*:fc2a" };

    CIOProc* pProc = new CIOProc("TestProc");
    std::shared_ptr<CTestIOInBound> spIOInBound = std::make_shared<CTestIOInBound>(pProc);
    BOOST_CHECK(spIOInBound->TestBuildWhiteList(vAllowMask));

    std::string targetAddress("192.168.199.1");
    BOOST_CHECK(spIOInBound->TestIsAllowRemote(targetAddress) == true);

    targetAddress = "192.168.196.34";
    BOOST_CHECK(spIOInBound->TestIsAllowRemote(targetAddress) == false);

    targetAddress = "2001:db8:0:f101::1";
    BOOST_CHECK(spIOInBound->TestIsAllowRemote(targetAddress) == true);

    targetAddress = "2001:db8:0:f102::23";
    BOOST_CHECK(spIOInBound->TestIsAllowRemote(targetAddress) == false);

    targetAddress = "::1";
    BOOST_CHECK(spIOInBound->TestIsAllowRemote(targetAddress) == false);

    targetAddress = "fe80::95c3:6a70:f4e4:5c2a";
    BOOST_CHECK(spIOInBound->TestIsAllowRemote(targetAddress) == true);

    targetAddress = "fe80::95c3:6a70:f4e4:5c2f";
    BOOST_CHECK(spIOInBound->TestIsAllowRemote(targetAddress) == false);

    targetAddress = "fe80::95c3:6a70:a231:fc2a";
    BOOST_CHECK(spIOInBound->TestIsAllowRemote(targetAddress) == true);

    targetAddress = "fe80::95c3:6a70:a231:fc21";
    BOOST_CHECK(spIOInBound->TestIsAllowRemote(targetAddress) == false);
}

BOOST_AUTO_TEST_SUITE_END()
