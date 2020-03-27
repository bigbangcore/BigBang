// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "delegate.h"

#include <boost/test/unit_test.hpp>

#include "address.h"
#include "test_big.h"

using namespace std;
using namespace xengine;
using namespace bigbang;
using namespace bigbang::delegate;

BOOST_FIXTURE_TEST_SUITE(delegate_tests, BasicUtfSetup)

BOOST_AUTO_TEST_CASE(delegate)
{
    std::cout << "delegate test..........\n";

    CDestination destDelegate = CAddress("20m053vhn4ygv9m8pzhevnjvtgbbqhgs66qv31ez39v9xbxvk0ynhfzer");

    std::set<CDestination> setDelegate;
    map<CDestination, size_t> mapWeight;
    CDelegateEvolveResult result;
    CDelegateVote vote;

    // data
    {
        setDelegate.insert(destDelegate);
        mapWeight.insert(make_pair(destDelegate, 21));
    }

    // init
    {
        vote.CreateDelegate(setDelegate);
        vote.Setup(MAX_DELEGATE_THRESH, result.mapEnrollData, uint256(1));
        printf("Setup complete\n");
    }

    // enroll & distribute
    {
        vote.Enroll(mapWeight, result.mapEnrollData);
        printf("Enroll success\n");

        vote.Distribute(result.mapDistributeData);
        BOOST_CHECK(!result.mapDistributeData.empty());
        printf("Distribute complete\n");
    }

    // Accept
    {
        const vector<unsigned char>& vchDistributeData = result.mapDistributeData[destDelegate];
        BOOST_CHECK(vote.Accept(destDelegate, vchDistributeData));
        printf("Accept complete\n");
    }

    // publish
    {
        vote.Publish(result.mapPublishData);
        BOOST_CHECK(!result.mapPublishData.empty());
        printf("Publish complete\n");
    }

    // Collect
    {
        const vector<unsigned char>& vchPublishData = result.mapPublishData[destDelegate];
        bool fCompleted = false;
        BOOST_CHECK(vote.Collect(destDelegate, vchPublishData, fCompleted));
        printf("Collect complete: fCompleted: %s\n", (fCompleted ? "true" : "false"));
    }

    // GetAgreement
    {
        uint256 nAgreement;
        size_t nWeight;
        map<CDestination, size_t> mapBallot;
        vote.GetAgreement(nAgreement, nWeight, mapBallot);

        printf("nAgreement: %s, nWeight: %ld, mapBallot.size: %ld\n", nAgreement.GetHex().c_str(), nWeight, mapBallot.size());
    }
}

BOOST_AUTO_TEST_CASE(delegate_success)
{
    std::cout << "delegate success..........\n";

    CDestination destDelegate = CAddress("20m053vhn4ygv9m8pzhevnjvtgbbqhgs66qv31ez39v9xbxvk0ynhfzer");

    std::set<CDestination> setDelegate;
    map<CDestination, size_t> mapWeight;
    CDelegateEvolveResult result;
    CDelegateVote vote;
    CDelegateVote vote1;

    // data
    {
        setDelegate.insert(destDelegate);
        mapWeight.insert(make_pair(destDelegate, 21));
    }

    // init
    {
        vote1.CreateDelegate(setDelegate);
        vote1.Setup(MAX_DELEGATE_THRESH, result.mapEnrollData, uint256(1));
        printf("Setup complete\n");
    }

    // enroll & distribute
    {
        vote = vote1;

        vote.Enroll(mapWeight, result.mapEnrollData);
        printf("Enroll complete\n");

        vote.Distribute(result.mapDistributeData);
        BOOST_CHECK(!result.mapDistributeData.empty());
        printf("Distribute complete\n");
    }

    // Accept
    {
        const vector<unsigned char>& vchDistributeData = result.mapDistributeData[destDelegate];
        BOOST_CHECK(vote.Accept(destDelegate, vchDistributeData));
        printf("Accept complete\n");
    }

    // publish
    {
        vote.Publish(result.mapPublishData);
        BOOST_CHECK(!result.mapPublishData.empty());
        printf("Publish complete\n");
    }

    // Collect
    {
        const vector<unsigned char>& vchPublishData = result.mapPublishData[destDelegate];
        bool fCompleted = false;
        BOOST_CHECK(vote.Collect(destDelegate, vchPublishData, fCompleted));
        printf("Collect complete: fCompleted: %s\n", (fCompleted ? "true" : "false"));
    }

    // GetAgreement
    {
        uint256 nAgreement;
        size_t nWeight;
        map<CDestination, size_t> mapBallot;
        vote.GetAgreement(nAgreement, nWeight, mapBallot);

        printf("nAgreement: %s, nWeight: %ld, mapBallot.size: %ld\n", nAgreement.GetHex().c_str(), nWeight, mapBallot.size());
    }
}

BOOST_AUTO_TEST_CASE(delegate_fail)
{
    std::cout << "delegate fail..........\n";

    CDestination destDelegate = CAddress("20m053vhn4ygv9m8pzhevnjvtgbbqhgs66qv31ez39v9xbxvk0ynhfzer");

    std::set<CDestination> setDelegate;
    map<CDestination, size_t> mapWeight;
    CDelegateEvolveResult result;
    CDelegateVote vote;
    CDelegateVote vote1;

    // data
    {
        setDelegate.insert(destDelegate);
        mapWeight.insert(make_pair(destDelegate, 21));
    }

    // init
    {
        vote1.CreateDelegate(setDelegate);
        vote1.Setup(MAX_DELEGATE_THRESH, result.mapEnrollData, uint256(1));
        printf("Setup complete\n");
    }

    // enroll & distribute
    {
        vote.CreateDelegate(setDelegate);
        std::map<CDestination, std::vector<unsigned char>> mapEnrollData;
        vote.Setup(MAX_DELEGATE_THRESH, mapEnrollData, uint256(1));

        vote.Enroll(mapWeight, result.mapEnrollData);
        printf("Enroll complete\n");

        vote.Distribute(result.mapDistributeData);
        BOOST_CHECK(!result.mapDistributeData.empty());
        printf("Distribute complete\n");
    }

    // Accept
    {
        const vector<unsigned char>& vchDistributeData = result.mapDistributeData[destDelegate];
        BOOST_CHECK(vote.Accept(destDelegate, vchDistributeData));
        printf("Accept complete\n");
    }

    // publish
    {
        vote.Publish(result.mapPublishData);
        BOOST_CHECK(!result.mapPublishData.empty());
        printf("Publish complete\n");
    }

    // Collect
    {
        const vector<unsigned char>& vchPublishData = result.mapPublishData[destDelegate];
        bool fCompleted = false;
        BOOST_CHECK(vote.Collect(destDelegate, vchPublishData, fCompleted));
        printf("Collect complete: fCompleted: %s\n", (fCompleted ? "true" : "false"));
    }

    // GetAgreement
    {
        uint256 nAgreement;
        size_t nWeight;
        map<CDestination, size_t> mapBallot;
        vote.GetAgreement(nAgreement, nWeight, mapBallot);

        printf("nAgreement: %s, nWeight: %ld, mapBallot.size: %ld\n", nAgreement.GetHex().c_str(), nWeight, mapBallot.size());
    }
}

BOOST_AUTO_TEST_CASE(delegate_more)
{
    std::cout << "delegate more..........\n";

    vector<CDestination> vDestDelegate = {
        CAddress("20m053vhn4ygv9m8pzhevnjvtgbbqhgs66qv31ez39v9xbxvk0ynhfzer"),
        CAddress("20m00c7gng4w63kjt8r9n6bpfmea7crw7pmkv9rshxf6d6vwhq9bbmfr4"),
        CAddress("20m0997062yn5vzk5ahjmfkyt2bjq8s6dt5xqynjq71k0rxcrbfc94w9x"),
        CAddress("20m0d0p9gaynwq6k815kj4n8w8ttydcf80m67sjcp3zsjymjfb7162307"),
    };
    map<int, set<CDestination>> mapDelegate;
    map<CDestination, size_t> mapWeight;
    map<int, CDelegateEvolveResult> mapResult;
    map<int, CDelegateVote> mapVote;

    // init data
    {
        for (int i = 0; i < vDestDelegate.size(); i++)
        {
            mapDelegate[i].insert(vDestDelegate[i]);
            mapWeight.insert(make_pair(vDestDelegate[i], 1));
        }
        printf("Init data success\n");
    }

    // init vote
    {
        for (int i = 0; i < vDestDelegate.size(); i++)
        {
            mapVote[i].CreateDelegate(mapDelegate[i]);
            mapVote[i].Setup(MAX_DELEGATE_THRESH, mapResult[i].mapEnrollData, uint256(i + 1));
            printf("[%d] Setup complete, enroll data size: %ld\n", i, mapResult[i].mapEnrollData[vDestDelegate[i]].size());
        }
    }

    // enroll & distribute
    {
        map<CDestination, std::vector<unsigned char>> mapEnrollDataTemp;
        for (int i = 0; i < vDestDelegate.size(); i++)
        {
            mapEnrollDataTemp.insert(mapResult[i].mapEnrollData.begin(), mapResult[i].mapEnrollData.end());
        }

        for (int i = 0; i < vDestDelegate.size(); i++)
        {
            mapVote[i].Enroll(mapWeight, mapEnrollDataTemp);

            mapVote[i].Distribute(mapResult[i].mapDistributeData);
            BOOST_CHECK(!mapResult[i].mapDistributeData.empty());
        }
        printf("Distribute complete\n");
    }

    // Accept
    {
        for (int i = 0; i < vDestDelegate.size(); i++)
        {
            for (int j = 0; j < vDestDelegate.size(); j++)
            {
                const vector<unsigned char>& vchDistributeData = mapResult[j].mapDistributeData[vDestDelegate[j]];
                BOOST_CHECK(mapVote[i].Accept(vDestDelegate[j], vchDistributeData));
            }
            printf("[%d] Accept complete\n", i);
        }
    }

    // publish
    {
        for (int i = 0; i < vDestDelegate.size(); i++)
        {
            mapVote[i].Publish(mapResult[i].mapPublishData);
            BOOST_CHECK(!mapResult[i].mapPublishData.empty());
        }
        printf("Publish complete\n");
    }

    // Collect
    {
        for (int i = 0; i < vDestDelegate.size(); i++)
        {
            bool fCompleted = false;
            for (int j = 0; j < vDestDelegate.size(); j++)
            {
                const vector<unsigned char>& vchPublishData = mapResult[j].mapPublishData[vDestDelegate[j]];
                BOOST_CHECK(mapVote[i].Collect(vDestDelegate[j], vchPublishData, fCompleted));
            }
            printf("[%d] Collect complete: fCompleted: %s\n", i, (fCompleted ? "true" : "false"));
        }
    }

    // GetAgreement
    {
        for (int i = 0; i < vDestDelegate.size(); i++)
        {
            uint256 nAgreement;
            size_t nWeight;
            map<CDestination, size_t> mapBallot;
            mapVote[i].GetAgreement(nAgreement, nWeight, mapBallot);

            printf("[%d] nAgreement: %s, nWeight: %ld, mapBallot.size: %ld\n", i, nAgreement.GetHex().c_str(), nWeight, mapBallot.size());
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
