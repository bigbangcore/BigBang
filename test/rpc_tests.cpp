// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//#include "rpcmod.h"
#include <boost/test/unit_test.hpp>

#include "test_big.h"
using namespace boost;

struct RPCSetup
{
    //    bigbang::CRPCMod rpcmdl;
    RPCSetup()
    {
        //        rpcmdl.Initialize();
    }
    ~RPCSetup()
    {
        //        rpcmdl.Deinitialize();
    }
    void CallRPCAPI(const std::string& params)
    {
        /*        xengine::CEventHttpReq eventHttpReq((uint64)0);
        try
        {
            bool ret = rpcmdl.HandleEvent(eventHttpReq);
        }
        catch(...)
        {
            throw std::runtime_error("error occured!");
        }*/
    }
};

BOOST_FIXTURE_TEST_SUITE(rpc_tests, RPCSetup)

BOOST_AUTO_TEST_CASE(rpc_listkey)
{
    //    BOOST_CHECK_THROW(CallRPCAPI("listkey xxx"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_getblock)
{
    //    BOOST_CHECK_THROW(CallRPCAPI("getblock"), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
