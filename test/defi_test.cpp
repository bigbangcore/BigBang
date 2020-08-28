// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "profile.h"
#include "forkcontext.h"
#include "defi.h"

#include <boost/test/unit_test.hpp>

#include "test_big.h"

using namespace xengine;
using namespace bigbang;
using namespace storage;

BOOST_FIXTURE_TEST_SUITE(defi_tests, BasicUtfSetup)


BOOST_AUTO_TEST_CASE(common_profile)
{
    CProfile profile;
    profile.strName = "BBC Test";
    profile.strSymbol = "BBCA";
    profile.nVersion = 1;
    profile.nMinTxFee = 100;
    profile.nMintReward = 1000;
    profile.nAmount = 100000;
    
    std::vector<uint8> vchProfile;
    BOOST_CHECK(profile.Save(vchProfile));

    CProfile profileLoad;
    try
    {
        BOOST_CHECK(profileLoad.Load(vchProfile));
    }
    catch(const std::exception& e)
    {
        BOOST_FAIL(e.what());
    }
    

    BOOST_CHECK(profileLoad.strName == profile.strName);
    BOOST_CHECK(profileLoad.strSymbol == profile.strSymbol);
    BOOST_CHECK(profileLoad.nVersion == profile.nVersion);
    BOOST_CHECK(profileLoad.nMinTxFee == profile.nMinTxFee);
    BOOST_CHECK(profileLoad.nMintReward == profile.nMintReward);
    BOOST_CHECK(profileLoad.nAmount == profile.nAmount);

    CForkContext forkContextWrite(uint256(), uint256(), uint256(), profile);
    CBufStream ss;
    ss << forkContextWrite;
    CForkContext forkContextRead;
    ss >> forkContextRead;

    BOOST_CHECK(forkContextRead.GetProfile().strName == profile.strName);
    BOOST_CHECK(forkContextRead.GetProfile().strSymbol == profile.strSymbol);
    BOOST_CHECK(forkContextRead.GetProfile().nVersion == profile.nVersion);
    BOOST_CHECK(forkContextRead.GetProfile().nMinTxFee == profile.nMinTxFee);
    BOOST_CHECK(forkContextRead.GetProfile().nMintReward == profile.nMintReward);
    BOOST_CHECK(forkContextRead.GetProfile().nAmount == profile.nAmount);
}

BOOST_AUTO_TEST_CASE(defi_profile)
{
    CProfile profile;
    profile.strName = "BBC Test";
    profile.strSymbol = "BBCA";
    profile.nVersion = 1;
    profile.nMinTxFee = 100;
    profile.nMintReward = 1000;
    profile.nAmount = 100000;
    profile.nForkType = FORK_TYPE_DEFI;
    profile.defi.nMaxSupply = 5;
    profile.defi.nDecayCycle = 10;
    profile.defi.nCoinbaseDecayPercent = 50;
    profile.defi.nInitCoinbasePercent = 15;
    profile.defi.nPromotionRewardPercent = 20;
    profile.defi.nRewardCycle = 25;
    profile.defi.nSupplyCycle = 55;
    profile.defi.nStakeMinToken = 100;
    profile.defi.nStakeRewardPercent = 30;
    profile.defi.mapPromotionTokenTimes.insert(std::make_pair(10, 11));
    profile.defi.mapPromotionTokenTimes.insert(std::make_pair(11, 12));
    
    std::vector<uint8> vchProfile;
    BOOST_CHECK(profile.Save(vchProfile));

    CProfile profileLoad;
    try
    {
        BOOST_CHECK(profileLoad.Load(vchProfile));
    }
    catch(const std::exception& e)
    {
        BOOST_FAIL(e.what());
    }
    

    BOOST_CHECK(profileLoad.strName == profile.strName);
    BOOST_CHECK(profileLoad.strSymbol == profile.strSymbol);
    BOOST_CHECK(profileLoad.nVersion == profile.nVersion);
    BOOST_CHECK(profileLoad.nMinTxFee == profile.nMinTxFee);
    BOOST_CHECK(profileLoad.nMintReward == profile.nMintReward);
    BOOST_CHECK(profileLoad.nAmount == profile.nAmount);

    BOOST_CHECK(profileLoad.nForkType == profile.nForkType);
    BOOST_CHECK(profileLoad.defi.nMaxSupply == profile.defi.nMaxSupply);
    BOOST_CHECK(profileLoad.defi.nDecayCycle == profile.defi.nDecayCycle);
    BOOST_CHECK(profileLoad.defi.nInitCoinbasePercent == profile.defi.nInitCoinbasePercent);
    BOOST_CHECK(profileLoad.defi.nPromotionRewardPercent == profile.defi.nPromotionRewardPercent);
    BOOST_CHECK(profileLoad.defi.nRewardCycle == profile.defi.nRewardCycle);
    BOOST_CHECK(profileLoad.defi.nSupplyCycle == profile.defi.nSupplyCycle);
    BOOST_CHECK(profileLoad.defi.nStakeMinToken == profile.defi.nStakeMinToken);
    BOOST_CHECK(profileLoad.defi.nStakeRewardPercent == profile.defi.nStakeRewardPercent);
    BOOST_CHECK(profileLoad.defi.mapPromotionTokenTimes.size() == profile.defi.mapPromotionTokenTimes.size());

    CForkContext forkContextWrite(uint256(), uint256(), uint256(), profile);
    CBufStream ss;
    ss << forkContextWrite;
    CForkContext forkContextRead;
    ss >> forkContextRead;

    BOOST_CHECK(forkContextRead.GetProfile().strName == profile.strName);
    BOOST_CHECK(forkContextRead.GetProfile().strSymbol == profile.strSymbol);
    BOOST_CHECK(forkContextRead.GetProfile().nVersion == profile.nVersion);
    BOOST_CHECK(forkContextRead.GetProfile().nMinTxFee == profile.nMinTxFee);
    BOOST_CHECK(forkContextRead.GetProfile().nMintReward == profile.nMintReward);
    BOOST_CHECK(forkContextRead.GetProfile().nAmount == profile.nAmount);

    BOOST_CHECK(forkContextRead.GetProfile().nForkType == profile.nForkType);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nDecayCycle == profile.defi.nDecayCycle);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nMaxSupply == profile.defi.nMaxSupply);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nInitCoinbasePercent == profile.defi.nInitCoinbasePercent);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nPromotionRewardPercent == profile.defi.nPromotionRewardPercent);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nRewardCycle == profile.defi.nRewardCycle);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nSupplyCycle == profile.defi.nSupplyCycle);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nStakeMinToken == profile.defi.nStakeMinToken);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nStakeRewardPercent == profile.defi.nStakeRewardPercent);
    BOOST_CHECK(forkContextRead.GetProfile().defi.mapPromotionTokenTimes.size() == profile.defi.mapPromotionTokenTimes.size());


    COldForkContext oldForkContextWrite(uint256(), uint256(), uint256(), profile);
    CBufStream ssFork;
    ssFork << oldForkContextWrite;
    CForkContext newForkContextRead;
    ssFork >> newForkContextRead;
    BOOST_CHECK(newForkContextRead.GetProfile().strName == profile.strName);
    BOOST_CHECK(newForkContextRead.GetProfile().strSymbol == profile.strSymbol);
    BOOST_CHECK(newForkContextRead.GetProfile().nVersion == profile.nVersion);
    BOOST_CHECK(newForkContextRead.GetProfile().nMinTxFee == profile.nMinTxFee);
    BOOST_CHECK(newForkContextRead.GetProfile().nMintReward == profile.nMintReward);
    BOOST_CHECK(newForkContextRead.GetProfile().nAmount == profile.nAmount);

    BOOST_CHECK(newForkContextRead.GetProfile().nForkType == FORK_TYPE_COMMON);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nDecayCycle != profile.defi.nDecayCycle);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nMaxSupply != profile.defi.nMaxSupply);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nInitCoinbasePercent != profile.defi.nInitCoinbasePercent);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nPromotionRewardPercent != profile.defi.nPromotionRewardPercent);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nRewardCycle != profile.defi.nRewardCycle);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nSupplyCycle != profile.defi.nSupplyCycle);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nStakeMinToken != profile.defi.nStakeMinToken);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nStakeRewardPercent != profile.defi.nStakeRewardPercent);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.mapPromotionTokenTimes.size() != profile.defi.mapPromotionTokenTimes.size());

}

static void RandGeneretor256(uint8_t* p)
{
    for (int i = 0; i < 32; i++)
    {
        *p++ = rand();
    }
}

BOOST_AUTO_TEST_CASE(defi_relation_graph)
{
    {
        CAddress A("1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h");
        CAddress A1("1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm");
        
        srand(time(0));
        uint8_t md32[32];
        RandGeneretor256(md32);
        std::map<CDestination, CAddrInfo> mapAddressTree{
            {A1, CAddrInfo(CDestination(), A, uint256((uint64_t*)md32))}
        };
        
        CDeFiRelationGraph defiGraph;
        BOOST_CHECK(defiGraph.ConstructRelationGraph(mapAddressTree) == true);
    }


    {
        CAddress A("1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h");
        CAddress A1("1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm");
        CAddress A2("1q71vfagprv5hqwckzbvhep0d0ct72j5j2heak2sgp4vptrtc2btdje3q");
        CAddress A3("1gbma6s21t4bcwymqz6h1dn1t7qy45019b1t00ywfyqymbvp90mqc1wmq");
        
        srand(time(0));
        uint8_t md32[32];
        RandGeneretor256(md32);
        std::map<CDestination, CAddrInfo> mapAddressTree;
        mapAddressTree.insert(std::make_pair(A1, CAddrInfo(CDestination(), A, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(A2, CAddrInfo(CDestination(), A, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(A3, CAddrInfo(CDestination(), A, uint256((uint64_t*)md32))));

        CDeFiRelationGraph defiGraph;
        BOOST_CHECK(defiGraph.ConstructRelationGraph(mapAddressTree) == true);
        BOOST_CHECK(defiGraph.setRoot.size() == 1);
    }
    
    {
        CAddress A("1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h");
        CAddress A1("1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm");
        CAddress A2("1q71vfagprv5hqwckzbvhep0d0ct72j5j2heak2sgp4vptrtc2btdje3q");
        CAddress A3("1gbma6s21t4bcwymqz6h1dn1t7qy45019b1t00ywfyqymbvp90mqc1wmq");
        CAddress AA11("1dq62d8y4fz20sfg63zzy4h4ayksswv1fgqjzvegde306bxxg5zygc27q");
        CAddress AA21("1awxt9zsbtjjxx4bk3q2j18s25kj00cajx3rj1bwg8beep7awmx1pkb8p");
        CAddress AA22("1t877w7b61wsx1rabkd69dbn2kgybpj4ayw2eycezg8qkyfekn97hrmgy");
        CAddress AAA111("18f2dv1vc6nv2xj7ak0e0yye4tx77205f5j73ep2a7a5w6szhjexkd5mj");
        CAddress AAA221("1yy76yav5mnah0jzew049a6gp5bs2ns67xzfvcengjkpqyymfb4n6vrda");
        CAddress AAA222("1g03a0775sbarkrazjrs7qymdepbkn3brn7375p7ysf0tnrcx408pj03n");
        
        srand(time(0));
        uint8_t md32[32];
        RandGeneretor256(md32);
        std::map<CDestination, CAddrInfo> mapAddressTree;
        mapAddressTree.insert(std::make_pair(A1, CAddrInfo(CDestination(), A, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(A2, CAddrInfo(CDestination(), A, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(A3, CAddrInfo(CDestination(), A, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(AA11, CAddrInfo(CDestination(), A1, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(AAA111, CAddrInfo(CDestination(), AA11, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(AA21, CAddrInfo(CDestination(), A2, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(AA22, CAddrInfo(CDestination(), A2, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(AAA221, CAddrInfo(CDestination(), AA22, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(AAA222, CAddrInfo(CDestination(), AA22, uint256((uint64_t*)md32))));
        
        CDeFiRelationGraph defiGraph;
        BOOST_CHECK(defiGraph.ConstructRelationGraph(mapAddressTree) == true);
        BOOST_CHECK(defiGraph.setRoot.size() == 1);
        BOOST_CHECK(defiGraph.setRoot.find(A) != defiGraph.setRoot.end());
    }

    {
        CAddress A("1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h");
        CAddress A1("1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm");
        CAddress A2("1q71vfagprv5hqwckzbvhep0d0ct72j5j2heak2sgp4vptrtc2btdje3q");
        CAddress A3("1gbma6s21t4bcwymqz6h1dn1t7qy45019b1t00ywfyqymbvp90mqc1wmq");
        CAddress AA11("1dq62d8y4fz20sfg63zzy4h4ayksswv1fgqjzvegde306bxxg5zygc27q");
        CAddress AA21("1awxt9zsbtjjxx4bk3q2j18s25kj00cajx3rj1bwg8beep7awmx1pkb8p");
        CAddress AA22("1t877w7b61wsx1rabkd69dbn2kgybpj4ayw2eycezg8qkyfekn97hrmgy");
        CAddress AAA111("18f2dv1vc6nv2xj7ak0e0yye4tx77205f5j73ep2a7a5w6szhjexkd5mj");
        CAddress AAA221("1yy76yav5mnah0jzew049a6gp5bs2ns67xzfvcengjkpqyymfb4n6vrda");
        CAddress AAA222("1g03a0775sbarkrazjrs7qymdepbkn3brn7375p7ysf0tnrcx408pj03n");

        CAddress B("1dt714q0p5143qhekgg0dx9qwnk6ww13f5v27xh6tpfmps25387ga2w5b");
        CAddress B1("1n1c0g6krvcvxhtgebptvz34rdq7qz2dcs6ngrphpnav465fdcpsmmbxj");
        CAddress B2("1m73jrn8np6ny50g3xr461yys6x3rme4yf1t7t9xa464v6n6p84ppzxa2");
        CAddress B3("1q284qfnpasxmkytpv5snda5ptqbpjxa9ryp2re0j1527qncmg38z7ar1");
        CAddress B4("134btp09511w3bnr1qq4de6btdkxkbp2y5x3zmr09g0m9hfn9frsa1k2f");
        
        srand(time(0));
        uint8_t md32[32];
        RandGeneretor256(md32);
        std::map<CDestination, CAddrInfo> mapAddressTree;
        mapAddressTree.insert(std::make_pair(A1, CAddrInfo(CDestination(), A, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(A2, CAddrInfo(CDestination(), A, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(A3, CAddrInfo(CDestination(), A, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(AA11, CAddrInfo(CDestination(), A1, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(AAA111, CAddrInfo(CDestination(), AA11, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(AA21, CAddrInfo(CDestination(), A2, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(AA22, CAddrInfo(CDestination(), A2, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(AAA221, CAddrInfo(CDestination(), AA22, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(AAA222, CAddrInfo(CDestination(), AA22, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(B1, CAddrInfo(CDestination(), B, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(B2, CAddrInfo(CDestination(), B, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(B3, CAddrInfo(CDestination(), B, uint256((uint64_t*)md32))));
        mapAddressTree.insert(std::make_pair(B4, CAddrInfo(CDestination(), B, uint256((uint64_t*)md32))));
        
        CDeFiRelationGraph defiGraph;
        BOOST_CHECK(defiGraph.ConstructRelationGraph(mapAddressTree) == true);
        BOOST_CHECK(defiGraph.setRoot.size() == 2);
        BOOST_CHECK(defiGraph.mapDestNode.size() == 15);
        BOOST_CHECK(defiGraph.setRoot.find(A) != defiGraph.setRoot.end());
        BOOST_CHECK(defiGraph.setRoot.find(B) != defiGraph.setRoot.end());
        BOOST_CHECK(defiGraph.mapDestNode.find(A) != defiGraph.mapDestNode.end());
        BOOST_CHECK(defiGraph.mapDestNode.find(B) != defiGraph.mapDestNode.end());
        BOOST_CHECK(defiGraph.setRoot.find(B3) == defiGraph.setRoot.end());
        BOOST_CHECK(defiGraph.setRoot.find(AA22) == defiGraph.setRoot.end());
    }

}

BOOST_AUTO_TEST_SUITE_END()
