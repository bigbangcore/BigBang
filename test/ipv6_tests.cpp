#include <boost/asio.hpp>
#include <boost/regex.hpp>
#include <boost/test/unit_test.hpp>
#include <exception>

#include "netio/nethost.h"
#include "netio/iocontainer.h"
#include "test_big.h"
#include "xengine.h"

using namespace xengine;

BOOST_FIXTURE_TEST_SUITE(ipv6_tests, BasicUtfSetup)


class CIOInBoundTest : public CIOInBound
{
public:
    CIOInBoundTest() : CIOInBound(new CIOProc("test")) {}
    void TestIPv6()
    {
        try 
        {
            std::vector<std::string> vAllowMask{ "192.168.199.*", "2001:db8:0:f101::*",
                                         "fe80::95c3:6a70:f4e4:5c2a", "fe80::95c3:6a70:*:fc2a" };
            if(!BuildWhiteList(vAllowMask))
            {
                BOOST_FAIL("BuildWhiteList return false");
            }
            
            int tempPort = 10;
            
            {
                boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("192.168.199.1"),tempPort);
                BOOST_CHECK(IsAllowedRemote(ep) == true);
            }
            
            {
                boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("192.168.196.34"),tempPort);
                BOOST_CHECK(IsAllowedRemote(ep) == false);
            }
            
            {
                boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("2001:db8:0:f101::1"),tempPort);
                BOOST_CHECK(IsAllowedRemote(ep) == true);
            }
            
            {
                boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("2001:db8:0:f102::23"),tempPort);
                BOOST_CHECK(IsAllowedRemote(ep) == false);
            }
            
            {
                boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("::1"),tempPort);
                BOOST_CHECK(IsAllowedRemote(ep) == false);
            }
            
            {
                boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("fe80::95c3:6a70:f4e4:5c2a"),tempPort);
                BOOST_CHECK(IsAllowedRemote(ep) == true);
            }
            
            {
                boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("fe80::95c3:6a70:f4e4:5c2f"),tempPort);
                BOOST_CHECK(IsAllowedRemote(ep) == false);
            }
            
            {
                boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("fe80::95c3:6a70:a231:fc2a"),tempPort);
                BOOST_CHECK(IsAllowedRemote(ep) == true);
            }
            
            {
                boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("fe80::95c3:6a70:a231:fc21"),tempPort);
                BOOST_CHECK(IsAllowedRemote(ep) == false);
            }
            
        }
        catch(const std::exception& exp)
        {
            BOOST_FAIL("Regex Error.");
        }
    }
};

BOOST_AUTO_TEST_CASE(white_list_regex)
{
    CIOInBoundTest ioInBoundTest;
    ioInBoundTest.TestIPv6();
}

BOOST_AUTO_TEST_SUITE_END()
