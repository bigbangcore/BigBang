// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/filesystem/path.hpp>
#include <boost/test/unit_test.hpp>
#include <map>

#include "defi.h"
#include "forkcontext.h"
#include "param.h"
#include "profile.h"
#include "test_big.h"

using namespace std;
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
    catch (const std::exception& e)
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
    profile.defi.nMintHeight = -1;
    profile.defi.nMaxSupply = 5;
    profile.defi.nCoinbaseType = SPECIFIC_DEFI_COINBASE_TYPE;
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
    profile.defi.mapCoinbasePercent.insert(std::make_pair(5, 10));
    profile.defi.mapCoinbasePercent.insert(std::make_pair(15, 11));

    std::vector<uint8> vchProfile;
    BOOST_CHECK(profile.Save(vchProfile));

    CProfile profileLoad;
    try
    {
        BOOST_CHECK(profileLoad.Load(vchProfile));
    }
    catch (const std::exception& e)
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
    BOOST_CHECK(profileLoad.defi.nMintHeight == profile.defi.nMintHeight);
    BOOST_CHECK(profileLoad.defi.nMaxSupply == profile.defi.nMaxSupply);
    BOOST_CHECK(profileLoad.defi.nCoinbaseType == profile.defi.nCoinbaseType);
    BOOST_CHECK(profileLoad.defi.nDecayCycle == profile.defi.nDecayCycle);
    BOOST_CHECK(profileLoad.defi.nInitCoinbasePercent == profile.defi.nInitCoinbasePercent);
    BOOST_CHECK(profileLoad.defi.nPromotionRewardPercent == profile.defi.nPromotionRewardPercent);
    BOOST_CHECK(profileLoad.defi.nRewardCycle == profile.defi.nRewardCycle);
    BOOST_CHECK(profileLoad.defi.nSupplyCycle == profile.defi.nSupplyCycle);
    BOOST_CHECK(profileLoad.defi.nStakeMinToken == profile.defi.nStakeMinToken);
    BOOST_CHECK(profileLoad.defi.nStakeRewardPercent == profile.defi.nStakeRewardPercent);
    BOOST_CHECK(profileLoad.defi.mapPromotionTokenTimes.size() == profile.defi.mapPromotionTokenTimes.size());
    BOOST_CHECK(profileLoad.defi.mapCoinbasePercent.size() == profile.defi.mapCoinbasePercent.size());

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
    BOOST_CHECK(forkContextRead.GetProfile().defi.nMintHeight == profile.defi.nMintHeight);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nCoinbaseType == profile.defi.nCoinbaseType);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nDecayCycle == profile.defi.nDecayCycle);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nMaxSupply == profile.defi.nMaxSupply);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nInitCoinbasePercent == profile.defi.nInitCoinbasePercent);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nPromotionRewardPercent == profile.defi.nPromotionRewardPercent);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nRewardCycle == profile.defi.nRewardCycle);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nSupplyCycle == profile.defi.nSupplyCycle);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nStakeMinToken == profile.defi.nStakeMinToken);
    BOOST_CHECK(forkContextRead.GetProfile().defi.nStakeRewardPercent == profile.defi.nStakeRewardPercent);
    BOOST_CHECK(forkContextRead.GetProfile().defi.mapPromotionTokenTimes.size() == profile.defi.mapPromotionTokenTimes.size());
    BOOST_CHECK(forkContextRead.GetProfile().defi.mapCoinbasePercent.size() == profile.defi.mapCoinbasePercent.size());

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
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nMintHeight != profile.defi.nMintHeight);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nCoinbaseType != SPECIFIC_DEFI_COINBASE_TYPE);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nDecayCycle != profile.defi.nDecayCycle);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nMaxSupply != profile.defi.nMaxSupply);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nInitCoinbasePercent != profile.defi.nInitCoinbasePercent);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nPromotionRewardPercent != profile.defi.nPromotionRewardPercent);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nRewardCycle != profile.defi.nRewardCycle);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nSupplyCycle != profile.defi.nSupplyCycle);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nStakeMinToken != profile.defi.nStakeMinToken);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.nStakeRewardPercent != profile.defi.nStakeRewardPercent);
    BOOST_CHECK(newForkContextRead.GetProfile().defi.mapPromotionTokenTimes.size() != profile.defi.mapPromotionTokenTimes.size());
    BOOST_CHECK(newForkContextRead.GetProfile().defi.mapCoinbasePercent.size() != profile.defi.mapCoinbasePercent.size());
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
            { A1, CAddrInfo(CDestination(), A, uint256((uint64_t*)md32)) }
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

BOOST_AUTO_TEST_CASE(reward1)
{
    // STD_DEBUG = true;
    // boost::filesystem::path logPath = boost::filesystem::absolute(boost::unit_test::framework::master_test_suite().argv[0]).parent_path();
    // InitLog(logPath, true, true, 1024, 1024);

    CDeFiForkReward r;
    uint256 forkid1;
    RandGeneretor256(forkid1.begin());
    uint256 forkid2;
    RandGeneretor256(forkid2.begin());

    // test ExistFork and AddFork
    BOOST_CHECK(!r.ExistFork(forkid1));

    CProfile profile1;
    profile1.strName = "BBC Test1";
    profile1.strSymbol = "BBCA";
    profile1.nVersion = 1;
    profile1.nMinTxFee = NEW_MIN_TX_FEE;
    profile1.nMintReward = 0;
    profile1.nAmount = 21000000 * COIN;
    profile1.nJointHeight = 150;
    profile1.nForkType = FORK_TYPE_DEFI;
    profile1.defi.nMintHeight = -1;
    profile1.defi.nMaxSupply = 2100000000 * COIN;
    profile1.defi.nCoinbaseType = FIXED_DEFI_COINBASE_TYPE;
    profile1.defi.nDecayCycle = 1036800;
    profile1.defi.nCoinbaseDecayPercent = 50;
    profile1.defi.nInitCoinbasePercent = 10;
    profile1.defi.nPromotionRewardPercent = 50;
    profile1.defi.nRewardCycle = 1440;
    profile1.defi.nSupplyCycle = 43200;
    profile1.defi.nStakeMinToken = 100 * COIN;
    profile1.defi.nStakeRewardPercent = 50;
    profile1.defi.mapPromotionTokenTimes.insert(std::make_pair(10000, 10));
    r.AddFork(forkid1, profile1);

    CProfile profile2 = profile1;
    profile2.strName = "BBC Test2";
    profile2.strSymbol = "BBCB";
    profile2.nVersion = 1;
    profile2.nMinTxFee = NEW_MIN_TX_FEE;
    profile2.nMintReward = 0;
    profile2.nAmount = 10000000 * COIN;
    profile2.nJointHeight = 150;
    profile2.nForkType = FORK_TYPE_DEFI;
    profile2.defi.nMintHeight = 1500;
    profile2.defi.nMaxSupply = 1000000000 * COIN;
    profile2.defi.nCoinbaseType = SPECIFIC_DEFI_COINBASE_TYPE;
    profile2.defi.mapCoinbasePercent = { { 259200, 10 }, { 777600, 8 }, { 1814400, 5 }, { 3369600, 3 }, { 5184000, 2 } };
    profile2.defi.nRewardCycle = 1440;
    profile2.defi.nSupplyCycle = 43200;
    profile2.defi.nStakeMinToken = 100 * COIN;
    profile2.defi.nStakeRewardPercent = 50;
    profile2.defi.mapPromotionTokenTimes.insert(std::make_pair(10000, 10));
    r.AddFork(forkid2, profile2);

    BOOST_CHECK(r.ExistFork(forkid1));
    BOOST_CHECK(r.GetForkProfile(forkid1).strSymbol == "BBCA");

    // test PrevRewardHeight
    BOOST_CHECK(r.PrevRewardHeight(forkid1, -10) == -1);
    BOOST_CHECK(r.PrevRewardHeight(forkid1, 0) == -1);
    BOOST_CHECK(r.PrevRewardHeight(forkid1, 151) == -1);
    BOOST_CHECK(r.PrevRewardHeight(forkid1, 152) == 151);
    BOOST_CHECK(r.PrevRewardHeight(forkid1, 1591) == 151);
    BOOST_CHECK(r.PrevRewardHeight(forkid1, 1592) == 1591);
    BOOST_CHECK(r.PrevRewardHeight(forkid1, 100000) == 99511);
    BOOST_CHECK(r.PrevRewardHeight(forkid1, 10000000) == 9999511);

    // test coinbase
    // fixed coinbase
    BOOST_CHECK(r.GetSectionReward(forkid1, uint256(0, uint224(0))) == 0);
    BOOST_CHECK(r.GetSectionReward(forkid1, uint256(151, uint224(0))) == 0);
    BOOST_CHECK(r.GetSectionReward(forkid1, uint256(152, uint224(0))) == 48611111);
    BOOST_CHECK(r.GetSectionReward(forkid1, uint256(1591, uint224(0))) == 70000000000);
    BOOST_CHECK(r.GetSectionReward(forkid1, uint256(43352, uint224(0))) == 53472222);
    BOOST_CHECK(r.GetSectionReward(forkid1, uint256(100000, uint224(0))) == 28762708333);
    // supply = 2179010403.498881 = 21000000*(1.1^24)*(1.05^24)*(1.025^24)*(1.0125^24)*(1.00625^24)*(1.003125^24)*(1.0015625^24)*(1.00078125^24)*(1.000390625^24)*(1.0001953125^15)
    // reward = 2179010403.498881 * 0.0001953125/43200 * (10000000 - 151 - 9 * 1036800 - 15 * 43200 - 14 * 1440)
    BOOST_CHECK(r.GetSectionReward(forkid1, uint256(10000000, uint224(0))) == 4817419376);
    int64 nReward = r.GetSectionReward(forkid1, uint256(10000000, uint224(0)));

    // specific coinbase
    BOOST_CHECK(r.GetSectionReward(forkid2, uint256(0, uint224(0))) == 0);
    BOOST_CHECK(r.GetSectionReward(forkid2, uint256(1499, uint224(0))) == 0);
    BOOST_CHECK(r.GetSectionReward(forkid2, uint256(1500, uint224(0))) == 23148148);
    BOOST_CHECK(r.GetSectionReward(forkid2, uint256(2939, uint224(0))) == 33333333333);
    BOOST_CHECK(r.GetSectionReward(forkid2, uint256(44700, uint224(0))) == 25462962);
    BOOST_CHECK(r.GetSectionReward(forkid2, uint256(260700, uint224(0))) == 32806685);
    BOOST_CHECK(r.GetSectionReward(forkid2, uint256(1001348, uint224(0))) == 32224247817);
    BOOST_CHECK(r.GetSectionReward(forkid2, uint256(2001348, uint224(0))) == 126959353755);
    // supply = 550207933.870525 = 10000000*(1.1^6)*(1.08^12)*(1.05^24)*(1.03^36)*(1.02^14)
    // reward = 550207933.870525 * 0.02/43200 * ((4001348 - (1500 - 1) - (6+12+24+36+14) * 43200) % 1440)
    BOOST_CHECK(r.GetSectionReward(forkid2, uint256(4001348, uint224(0))) == 246829392555);
    BOOST_CHECK(r.GetSectionReward(forkid2, uint256(10001348, uint224(0))) == 0);

    CAddress A("1632srrskscs1d809y3x5ttf50f0gabf86xjz2s6aetc9h9ewwhm58dj3");
    CAddress a1("1f1nj5gjgrcz45g317s1y4tk18bbm89jdtzd41m9s0t14tp2ngkz4cg0x");
    CAddress a11("1pmj5p47zhqepwa9vfezkecxkerckhrks31pan5fh24vs78s6cbkrqnxp");
    CAddress a111("1bvaag2t23ybjmasvjyxnzetja0awe5d1pyw361ea3jmkfdqt5greqvfq");
    CAddress a2("1ab1sjh07cz7xpb0xdkpwykfm2v91cvf2j1fza0gj07d2jktdnrwtyd57");
    CAddress a21("1782a5jv06egd6pb2gjgp2p664tgej6n4gmj79e1hbvgfgy3t006wvwzt");
    CAddress a22("1c7s095dcvzdsedrkpj6y5kjysv5sz3083xkahvyk7ry3tag4ddyydbv4");
    CAddress a221("1ytysehvcj13ka9r1qhh9gken1evjdp9cn00cz0bhqjqgzwc6sdmse548");
    CAddress a222("16aaahq32cncamxbmvedjy5jqccc2hcpk7rc0h2zqcmmrnm1g2213t2k3");
    CAddress a3("1vz7k0748t840bg3wgem4mhf9pyvptf1z2zqht6bc2g9yn6cmke8h4dwe");
    CAddress B("1fpt2z9nyh0a5999zrmabg6ppsbx78wypqapm29fsasx993z11crp6zm7");
    CAddress b1("1rampdvtmzmxfzr3crbzyz265hbr9a8y4660zgpbw6r7qt9hdy535zed7");
    CAddress b2("1w0k188jwq5aenm6sfj6fdx9f3d96k20k71612ddz81qrx6syr4bnp5m9");
    CAddress b3("196wx05mee1zavws828vfcap72tebtskw094tp5sztymcy30y7n9varfa");
    CAddress b4("19k8zjwdntjp8avk41c8aek0jxrs1fgyad5q9f1gd1q2fdd0hafmm549d");
    CAddress C("1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm");

    // test stake reward
    // B = 1/1
    map<CDestination, int64> reward;
    map<CDestination, int64> balance;
    balance = map<CDestination, int64>{
        { A, 0 },
        { B, 100 * COIN },
    };
    reward = r.ComputeStakeReward(profile1.defi.nStakeMinToken, nReward, balance);
    BOOST_CHECK(reward.size() == 1);
    auto it = reward.begin();
    BOOST_CHECK(it->first == B && it->second == nReward);

    // a1 = 1/5, a11 = 3/5, a111 = 1/5
    balance = map<CDestination, int64>{
        { A, 0 },
        { a1, 100 * COIN },
        { a11, 1000 * COIN },
        { a111, 100 * COIN },
    };
    reward = r.ComputeStakeReward(profile1.defi.nStakeMinToken, nReward, balance);
    BOOST_CHECK(reward.size() == 3);
    it = reward.find(a1);
    BOOST_CHECK(it != reward.end() && it->second == 963483875);
    it = reward.find(a11);
    BOOST_CHECK(it != reward.end() && it->second == 2890451625);
    it = reward.find(a111);
    BOOST_CHECK(it != reward.end() && it->second == 963483875);

    // test promotion reward
    balance = map<CDestination, int64>{
        { A, 10000 * COIN },
        { a1, 100000 * COIN },
        { a11, 100000 * COIN },
        { a111, 100000 * COIN },
        { a2, 1 * COIN },
        { a21, 1 * COIN },
        { a22, 12000 * COIN },
        { a221, 18000 * COIN },
        { a222, 5000 * COIN },
        { a3, 1000000 * COIN },
        { B, 10000 * COIN },
        { b1, 10000 * COIN },
        { b2, 11000 * COIN },
        { b3, 5000 * COIN },
        { b4, 50000 * COIN },
        { C, 19568998 * COIN },
    };

    map<CDestination, CAddrInfo> mapAddress;
    mapAddress = map<CDestination, CAddrInfo>{
        { a1, CAddrInfo(A, A, uint256(1)) },
        { a11, CAddrInfo(A, a1, uint256(1)) },
        { a111, CAddrInfo(A, a11, uint256(1)) },
        { a2, CAddrInfo(A, A, uint256(1)) },
        { a21, CAddrInfo(A, a2, uint256(1)) },
        { a22, CAddrInfo(A, a2, uint256(1)) },
        { a221, CAddrInfo(A, a22, uint256(1)) },
        { a222, CAddrInfo(A, a22, uint256(1)) },
        { a3, CAddrInfo(A, A, uint256(1)) },
        { b1, CAddrInfo(B, B, uint256(1)) },
        { b2, CAddrInfo(B, B, uint256(1)) },
        { b3, CAddrInfo(B, B, uint256(1)) },
        { b4, CAddrInfo(B, B, uint256(1)) },
    };

    CDeFiRelationGraph relation;
    BOOST_CHECK(relation.ConstructRelationGraph(mapAddress));
    reward = r.ComputePromotionReward(nReward, balance, profile1.defi.mapPromotionTokenTimes, relation);

    BOOST_CHECK(reward.size() == 6);
    it = reward.find(A);
    BOOST_CHECK(it != reward.end() && it->second == 3039845494);
    it = reward.find(a1);
    BOOST_CHECK(it != reward.end() && it->second == 342283);
    it = reward.find(a11);
    BOOST_CHECK(it != reward.end() && it->second == 271466);
    it = reward.find(a2);
    BOOST_CHECK(it != reward.end() && it->second == 253762);
    it = reward.find(a22);
    BOOST_CHECK(it != reward.end() && it->second == 295225626);
    it = reward.find(B);
    BOOST_CHECK(it != reward.end() && it->second == 1481480742);

    // test all reward
    nReward = r.GetSectionReward(forkid2, uint256(2939, uint224(0)));
    CDeFiRelationGraph relationReward;
    reward = r.ComputePromotionReward(nReward / 2, balance, profile2.defi.mapPromotionTokenTimes, relationReward);
    for (auto& x : reward)
    {
        cout << "promotion reward, destination: " << CAddress(x.first).ToString() << ", reward: " << x.second << endl;
    }

    // boost::filesystem::remove_all(logPath);
}

BOOST_AUTO_TEST_CASE(reward2)
{
    CDeFiForkReward r;
    uint256 forkid;
    RandGeneretor256(forkid.begin());

    // test ExistFork and AddFork
    BOOST_CHECK(!r.ExistFork(forkid));

    CProfile profile;
    profile.strName = "BBC Test2";
    profile.strSymbol = "BBCB";
    profile.nVersion = 1;
    profile.nMinTxFee = NEW_MIN_TX_FEE;
    profile.nMintReward = 0;
    profile.nAmount = 10000000 * COIN; // 首期发行一千万母币
    profile.nJointHeight = 15;
    profile.nForkType = FORK_TYPE_DEFI;
    profile.defi.nMintHeight = 20;
    profile.defi.nMaxSupply = 1000000000 * COIN;  // BTCA 总共发行十亿枚
    profile.defi.nCoinbaseType = SPECIFIC_DEFI_COINBASE_TYPE;
    profile.defi.mapCoinbasePercent = { { 259200, 10 }, { 777600, 8 }, { 1814400, 5 }, { 3369600, 3 }, { 5184000, 2 } }; // 发行阶段，半年，1年（用高度表示），参考BTCA白皮书，月增长原有基数的10%，8%
    profile.defi.nRewardCycle = 1440; // 60 * 24  per day once reward 
    profile.defi.nSupplyCycle = 43200; // 60 * 24 * 30  per month once supply
    profile.defi.nStakeMinToken = 100 * COIN; // min token required, >= 100, can be required to join this defi game
    profile.defi.nStakeRewardPercent = 50; // 50% of supply amount per day
    profile.defi.mapPromotionTokenTimes.insert(std::make_pair(10000, 10)); // 用于推广收益，小于等于10000的部分，要放大10倍
    r.AddFork(forkid, profile);

    BOOST_CHECK(r.ExistFork(forkid));
    BOOST_CHECK(r.GetForkProfile(forkid).strSymbol == "BBCB");

    int64 nReward = r.GetSectionReward(forkid, uint256(2939, uint224(0)));

    CAddress A("1632srrskscs1d809y3x5ttf50f0gabf86xjz2s6aetc9h9ewwhm58dj3");
    CAddress a1("1f1nj5gjgrcz45g317s1y4tk18bbm89jdtzd41m9s0t14tp2ngkz4cg0x");
    CAddress a11("1pmj5p47zhqepwa9vfezkecxkerckhrks31pan5fh24vs78s6cbkrqnxp");
    CAddress a111("1bvaag2t23ybjmasvjyxnzetja0awe5d1pyw361ea3jmkfdqt5greqvfq");
    CAddress a2("1ab1sjh07cz7xpb0xdkpwykfm2v91cvf2j1fza0gj07d2jktdnrwtyd57");
    CAddress a21("1782a5jv06egd6pb2gjgp2p664tgej6n4gmj79e1hbvgfgy3t006wvwzt");
    CAddress a22("1c7s095dcvzdsedrkpj6y5kjysv5sz3083xkahvyk7ry3tag4ddyydbv4");
    CAddress a221("1ytysehvcj13ka9r1qhh9gken1evjdp9cn00cz0bhqjqgzwc6sdmse548");
    CAddress a222("16aaahq32cncamxbmvedjy5jqccc2hcpk7rc0h2zqcmmrnm1g2213t2k3");
    CAddress a3("1vz7k0748t840bg3wgem4mhf9pyvptf1z2zqht6bc2g9yn6cmke8h4dwe");
    CAddress B("1fpt2z9nyh0a5999zrmabg6ppsbx78wypqapm29fsasx993z11crp6zm7");
    CAddress b1("1rampdvtmzmxfzr3crbzyz265hbr9a8y4660zgpbw6r7qt9hdy535zed7");
    CAddress b2("1w0k188jwq5aenm6sfj6fdx9f3d96k20k71612ddz81qrx6syr4bnp5m9");
    CAddress b3("196wx05mee1zavws828vfcap72tebtskw094tp5sztymcy30y7n9varfa");
    CAddress b4("19k8zjwdntjp8avk41c8aek0jxrs1fgyad5q9f1gd1q2fdd0hafmm549d");
    CAddress C("1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm");

    // test stake reward
    // B = 1/1
    map<CDestination, int64> reward;
    map<CDestination, int64> balance;
    balance = map<CDestination, int64>{
        { A, 0 },
        { B, 100 * COIN },
    };
    std::cout << "nReward " << nReward << std::endl;
    reward = r.ComputeStakeReward(profile.defi.nStakeMinToken, (nReward / 2), balance);
    BOOST_CHECK(reward.size() == 1);
    auto it = reward.begin();
    
    // B的持币量排名 1
    // 各个地址的排名相加 1
    // nReward = 925925925
    // 1 / 1 * nReward * 50%
    BOOST_CHECK(it->first == B && it->second == (nReward / 2));

    balance = map<CDestination, int64>{
        { A, 0 },
        { a1, 100 * COIN },          //  rank 1
        { a11, 1000 * COIN },        // rank 5
        { a111, 100 * COIN },        // rank 1
        { a221, 105 * COIN },        // rank 4
        { a222, 100 * COIN },        // rank 1
    };
    reward = r.ComputeStakeReward(profile.defi.nStakeMinToken, nReward / 2, balance);
    BOOST_CHECK(reward.size() == 5);
    it = reward.find(a1);
    // 1 / 12 * nReward 
    BOOST_CHECK(it != reward.end() && it->second == (77160493 / 2));
    it = reward.find(a11);
    // 5 / 12 * nReward 
    BOOST_CHECK(it != reward.end() && it->second == (385802468 / 2));
    it = reward.find(a111);
    // 1 / 12 * nReward 
    BOOST_CHECK(it != reward.end() && it->second == (77160493 / 2));
    it = reward.find(a221);
    // 4 / 12 * nReward 
    BOOST_CHECK(it != reward.end() && it->second == (308641975 / 2));

    it = reward.find(a222);
    // 1 / 12 * nReward 
    BOOST_CHECK(it != reward.end() && it->second == (77160493 / 2));
    

    // test promotion reward
    balance = map<CDestination, int64>{
        { A, 10000 * COIN },
        { a1, 100000 * COIN },
        { a11, 100000 * COIN },
        { a111, 100000 * COIN },
        { a2, 1 * COIN },
        { a21, 1 * COIN },
        { a22, 12000 * COIN },
        { a221, 18000 * COIN },
        { a222, 5000 * COIN },
        { a3, 1000000 * COIN },
        { B, 10000 * COIN },
        { b1, 10000 * COIN },
        { b2, 11000 * COIN },
        { b3, 5000 * COIN },
        { b4, 50000 * COIN },
        { C, 19568998 * COIN },
    };

    map<CDestination, CAddrInfo> mapAddress;
    mapAddress = map<CDestination, CAddrInfo>{
        { a1, CAddrInfo(A, A, uint256(1)) },
        { a11, CAddrInfo(A, a1, uint256(1)) },
        { a111, CAddrInfo(A, a11, uint256(1)) },
        { a2, CAddrInfo(A, A, uint256(1)) },
        { a21, CAddrInfo(A, a2, uint256(1)) },
        { a22, CAddrInfo(A, a2, uint256(1)) },
        { a221, CAddrInfo(A, a22, uint256(1)) },
        { a222, CAddrInfo(A, a22, uint256(1)) },
        { a3, CAddrInfo(A, A, uint256(1)) },
        { b1, CAddrInfo(B, B, uint256(1)) },
        { b2, CAddrInfo(B, B, uint256(1)) },
        { b3, CAddrInfo(B, B, uint256(1)) },
        { b4, CAddrInfo(B, B, uint256(1)) },
    };

    CDeFiRelationGraph relation;
    BOOST_CHECK(relation.ConstructRelationGraph(mapAddress));
    reward = r.ComputePromotionReward(nReward, balance, profile.defi.mapPromotionTokenTimes, relation);

    BOOST_CHECK(reward.size() == 6);
    it = reward.find(A);
    BOOST_CHECK(it != reward.end() && it->second == 3039845494);
    it = reward.find(a1);
    BOOST_CHECK(it != reward.end() && it->second == 342283);
    it = reward.find(a11);
    BOOST_CHECK(it != reward.end() && it->second == 271466);
    it = reward.find(a2);
    BOOST_CHECK(it != reward.end() && it->second == 253762);
    it = reward.find(a22);
    BOOST_CHECK(it != reward.end() && it->second == 295225626);
    it = reward.find(B);
    BOOST_CHECK(it != reward.end() && it->second == 1481480742);

    // test all reward
    nReward = r.GetSectionReward(forkid, uint256(2939, uint224(0)));
    CDeFiRelationGraph relationReward;
    reward = r.ComputePromotionReward(nReward / 2, balance, profile.defi.mapPromotionTokenTimes, relationReward);
    for (auto& x : reward)
    {
        cout << "promotion reward, destination: " << CAddress(x.first).ToString() << ", reward: " << x.second << endl;
    }
}
BOOST_AUTO_TEST_SUITE_END()
