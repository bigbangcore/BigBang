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
        printf("Setup success\n");
    }

    // enroll & distribute
    {
        vote.Enroll(mapWeight, result.mapEnrollData);
        printf("Enroll success\n");

        vote.Distribute(result.mapDistributeData);
        if (result.mapDistributeData.empty())
        {
            printf("Distribute fail\n");
            return;
        }
        printf("Distribute success\n");
    }

    // Accept
    {
        const vector<unsigned char>& vchDistributeData = result.mapDistributeData[destDelegate];
        if (!vote.Accept(destDelegate, vchDistributeData))
        {
            printf("Accept fail\n");
            return;
        }
        printf("Accept success\n");
    }

    // publish
    {
        vote.Publish(result.mapPublishData);
        if (result.mapPublishData.empty())
        {
            printf("Publish fail\n");
            return;
        }
        printf("Publish success\n");
    }

    // Collect
    {
        const vector<unsigned char>& vchPublishData = result.mapPublishData[destDelegate];
        bool fCompleted = false;
        if (!vote.Collect(destDelegate, vchPublishData, fCompleted))
        {
            printf("Collect fail\n");
            return;
        }
        printf("Collect success, fCompleted: %s\n", (fCompleted ? "true" : "false"));
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

BOOST_AUTO_TEST_CASE(delegate_test1)
{
    std::cout << "delegate test1..........\n";

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
        printf("Setup success\n");
    }

    // enroll & distribute
    {
        vote = vote1;

        vote.Enroll(mapWeight, result.mapEnrollData);
        printf("Enroll success\n");

        vote.Distribute(result.mapDistributeData);
        if (result.mapDistributeData.empty())
        {
            printf("Distribute fail\n");
            return;
        }
        printf("Distribute success\n");
    }

    // Accept
    {
        const vector<unsigned char>& vchDistributeData = result.mapDistributeData[destDelegate];
        if (!vote.Accept(destDelegate, vchDistributeData))
        {
            printf("Accept fail\n");
            return;
        }
        printf("Accept success\n");
    }

    // publish
    {
        vote.Publish(result.mapPublishData);
        if (result.mapPublishData.empty())
        {
            printf("Publish fail\n");
            return;
        }
        printf("Publish success\n");
    }

    // Collect
    {
        const vector<unsigned char>& vchPublishData = result.mapPublishData[destDelegate];
        bool fCompleted = false;
        if (!vote.Collect(destDelegate, vchPublishData, fCompleted))
        {
            printf("Collect fail\n");
            return;
        }
        printf("Collect success, fCompleted: %s\n", (fCompleted ? "true" : "false"));
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

BOOST_AUTO_TEST_CASE(delegate_test2)
{
    std::cout << "delegate test2..........\n";

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
            printf("[%d] Setup success, enroll data size: %ld\n", i, mapResult[i].mapEnrollData[vDestDelegate[i]].size());
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
            if (mapResult[i].mapDistributeData.empty())
            {
                printf("[%d] Distribute fail\n", i);
                return;
            }
        }
        printf("Distribute success\n");
    }

    // Accept
    {
        for (int i = 0; i < vDestDelegate.size(); i++)
        {
            for (int j = 0; j < vDestDelegate.size(); j++)
            {
                const vector<unsigned char>& vchDistributeData = mapResult[j].mapDistributeData[vDestDelegate[j]];
                if (!mapVote[i].Accept(vDestDelegate[j], vchDistributeData))
                {
                    printf("Accept fail\n");
                    return;
                }
            }
            printf("[%d] Accept success\n", i);
        }
    }

    // publish
    {
        for (int i = 0; i < vDestDelegate.size(); i++)
        {
            mapVote[i].Publish(mapResult[i].mapPublishData);
            if (mapResult[i].mapPublishData.empty())
            {
                printf("Publish fail\n");
                return;
            }
        }
        printf("Publish success\n");
    }

    // Collect
    {
        for (int i = 0; i < vDestDelegate.size(); i++)
        {
            bool fCompleted = false;
            for (int j = 0; j < vDestDelegate.size(); j++)
            {
                const vector<unsigned char>& vchPublishData = mapResult[j].mapPublishData[vDestDelegate[j]];
                if (!mapVote[i].Collect(vDestDelegate[j], vchPublishData, fCompleted))
                {
                    printf("Collect fail\n");
                    return;
                }
            }
            printf("[%d] Collect success, fCompleted: %s\n", i, (fCompleted ? "true" : "false"));
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
