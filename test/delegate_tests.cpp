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
        vote.Setup(MAX_DELEGATE_THRESH);
        vote.GetSetupData(result.mapEnrollData);
        std::cout << "Setup complete\n";
    }

    // enroll & distribute
    {
        vote.Enroll(mapWeight, result.mapEnrollData);
        std::cout << "Enroll success\n";

        vote.Distribute(result.mapDistributeData);
        BOOST_CHECK(!result.mapDistributeData.empty());
        std::cout << "Distribute complete\n";
    }

    // Accept
    {
        const vector<unsigned char>& vchDistributeData = result.mapDistributeData[destDelegate];
        BOOST_CHECK(vote.Accept(destDelegate, vchDistributeData));
        std::cout << "Accept complete\n";
    }

    // publish
    {
        vote.Publish(result.mapPublishData);
        BOOST_CHECK(!result.mapPublishData.empty());
        std::cout << "Publish complete\n";
    }

    // Collect
    {
        const vector<unsigned char>& vchPublishData = result.mapPublishData[destDelegate];
        bool fCompleted = false;
        BOOST_CHECK(vote.Collect(destDelegate, vchPublishData, fCompleted));
        std::cout << "Collect complete: fCompleted: " << (fCompleted ? "true" : "false") << endl;
    }

    // GetAgreement
    {
        uint256 nAgreement;
        size_t nWeight;
        map<CDestination, size_t> mapBallot;
        vote.GetAgreement(nAgreement, nWeight, mapBallot);

        std::cout << "nAgreement: " << nAgreement.GetHex() << ", nWeight: " << nWeight << ", mapBallot.size: " << mapBallot.size() << endl;
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
        vote1.Setup(MAX_DELEGATE_THRESH);
        vote1.GetSetupData(result.mapEnrollData);
        std::cout << "Setup complete\n";
    }

    // enroll & distribute
    {
        vote = vote1;

        vote.Enroll(mapWeight, result.mapEnrollData);
        std::cout << "Enroll complete\n";

        vote.Distribute(result.mapDistributeData);
        BOOST_CHECK(!result.mapDistributeData.empty());
        std::cout << "Distribute complete\n";
    }

    // Accept
    {
        const vector<unsigned char>& vchDistributeData = result.mapDistributeData[destDelegate];
        BOOST_CHECK(vote.Accept(destDelegate, vchDistributeData));
        std::cout << "Accept complete\n";
    }

    // publish
    {
        vote.Publish(result.mapPublishData);
        BOOST_CHECK(!result.mapPublishData.empty());
        std::cout << "Publish complete\n";
    }

    // Collect
    {
        const vector<unsigned char>& vchPublishData = result.mapPublishData[destDelegate];
        bool fCompleted = false;
        BOOST_CHECK(vote.Collect(destDelegate, vchPublishData, fCompleted));
        std::cout << "Collect complete: fCompleted: " << (fCompleted ? "true" : "false") << endl;
    }

    // GetAgreement
    {
        uint256 nAgreement;
        size_t nWeight;
        map<CDestination, size_t> mapBallot;
        vote.GetAgreement(nAgreement, nWeight, mapBallot);

        std::cout << "nAgreement: " << nAgreement.GetHex() << ", nWeight: " << nWeight << ", mapBallot.size: " << mapBallot.size() << endl;
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
        vote1.Setup(MAX_DELEGATE_THRESH);
        vote1.GetSetupData(result.mapEnrollData);
        std::cout << "Setup complete\n";
    }

    // enroll & distribute
    {
        vote.CreateDelegate(setDelegate);
        std::map<CDestination, std::vector<unsigned char>> mapEnrollData;
        vote.Setup(MAX_DELEGATE_THRESH);
        vote.GetSetupData(mapEnrollData);

        vote.Enroll(mapWeight, result.mapEnrollData);
        std::cout << "Enroll complete\n";

        vote.Distribute(result.mapDistributeData);
        BOOST_CHECK(!result.mapDistributeData.empty());
        std::cout << "Distribute complete\n";
    }

    // Accept
    {
        const vector<unsigned char>& vchDistributeData = result.mapDistributeData[destDelegate];
        BOOST_CHECK(!vote.Accept(destDelegate, vchDistributeData));
        std::cout << "Accept complete\n";
    }

    // publish
    {
        vote.Publish(result.mapPublishData);
        BOOST_CHECK(!result.mapPublishData.empty());
        std::cout << "Publish complete\n";
    }

    // Collect
    {
        const vector<unsigned char>& vchPublishData = result.mapPublishData[destDelegate];
        bool fCompleted = false;
        BOOST_CHECK(!vote.Collect(destDelegate, vchPublishData, fCompleted));
        std::cout << "Collect complete: fCompleted: " << (fCompleted ? "true" : "false") << endl;
    }

    // GetAgreement
    {
        uint256 nAgreement;
        size_t nWeight;
        map<CDestination, size_t> mapBallot;
        vote.GetAgreement(nAgreement, nWeight, mapBallot);

        std::cout << "nAgreement: " << nAgreement.GetHex() << ", nWeight: " << nWeight << ", mapBallot.size: " << mapBallot.size() << endl;
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
        std::cout << "Init data success\n";
    }

    // init vote
    {
        for (int i = 0; i < vDestDelegate.size(); i++)
        {
            mapVote[i].CreateDelegate(mapDelegate[i]);
            mapVote[i].Setup(MAX_DELEGATE_THRESH);
            mapVote[i].GetSetupData(mapResult[i].mapEnrollData);
            std::cout << "[" << i << "] Setup complete, enroll data size: " << mapResult[i].mapEnrollData[vDestDelegate[i]].size() << endl;
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
        std::cout << "Distribute complete\n";
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
            std::cout << "[" << i << "] Accept complete" << endl;
        }
    }

    // publish
    {
        for (int i = 0; i < vDestDelegate.size(); i++)
        {
            mapVote[i].Publish(mapResult[i].mapPublishData);
            BOOST_CHECK(!mapResult[i].mapPublishData.empty());
        }
        std::cout << "Publish complete\n";
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
            std::cout << "[" << i << "] Collect complete: fCompleted: " << (fCompleted ? "true" : "false") << endl;
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

            std::cout << "[" << i << "] nAgreement: " << nAgreement.GetHex() << ", nWeight: " << nWeight << ", mapBallot.size: " << mapBallot.size() << endl;
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
