// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txpool.h"

#include <boost/test/unit_test.hpp>
#include <map>

#include "test_big.h"

using namespace bigbang;

BOOST_FIXTURE_TEST_SUITE(txpool_tests, BasicUtfSetup)

BOOST_AUTO_TEST_CASE(multi_index_tx_seq)
{
    CPooledTxLinkSet setTxLinkIndex;
    std::map<uint8, uint16> mapType{
        { 0, CTransaction::TX_TOKEN },
        { 1, CTransaction::TX_CERT },
        { 2, CTransaction::TX_GENESIS },
        { 3, CTransaction::TX_STAKE },
        { 4, CTransaction::TX_WORK }
    };

    uint32 timestamp = 1579021270;
    for (int i = 10; i >= 0; --i)
    {
        CTransaction tx;
        tx.SetNull();
        tx.nTimeStamp = timestamp++;
        tx.nType = mapType[i % 5];
        tx.nLockUntil = i;
        CPooledTx pooledTx(tx, -1, i);
        setTxLinkIndex.insert(CPooledTxLink(&pooledTx));
    }

    BOOST_CHECK(setTxLinkIndex.size() == 11);

    const CPooledTxLinkSetBySequenceNumber& idxTxLinkSeq = setTxLinkIndex.get<1>();
    CPooledTxLinkSetBySequenceNumber::iterator iter = idxTxLinkSeq.begin();

    int k = 0;
    for (; iter != idxTxLinkSeq.end(); ++iter)
    {
        BOOST_CHECK(k == iter->nSequenceNumber);
        k++;
    }
}

BOOST_AUTO_TEST_CASE(multi_index_tx_score)
{
    CPooledTxLinkSet setTxLinkIndex;
    std::map<uint8, uint16> mapType{
        { 0, CTransaction::TX_TOKEN },
        { 1, CTransaction::TX_CERT },
        { 2, CTransaction::TX_GENESIS },
        { 3, CTransaction::TX_STAKE },
        { 4, CTransaction::TX_WORK }
    };

    uint32 timestamp = 1579021270;
    for (int i = 10; i >= 0; --i)
    {
        CTransaction tx;
        tx.SetNull();
        tx.nTimeStamp = timestamp++;
        tx.nType = mapType[i % 5];
        tx.nLockUntil = i;
        CPooledTx pooledTx(tx, -1, i);
        setTxLinkIndex.insert(CPooledTxLink(&pooledTx));
    }

    BOOST_CHECK(setTxLinkIndex.size() == 11);

    const CPooledTxLinkSetByTxScore& idxTxLinkScore = setTxLinkIndex.get<tx_score>();
    CPooledTxLinkSetByTxScore::iterator iter = idxTxLinkScore.begin();

    std::vector<CPooledTxLinkSetByTxScore::iterator> iterVec;
    for (; iter != idxTxLinkScore.end(); ++iter)
    {
        // std::cout << "type " << std::hex << iter->nType << std::dec << " seq " << iter->nSequenceNumber << std::endl;
        iterVec.push_back(iter);
    }

    BOOST_CHECK(iterVec.size() == setTxLinkIndex.size());

    BOOST_CHECK(iterVec[0]->nType == CTransaction::TX_CERT && iterVec[1]->nType == CTransaction::TX_CERT);
    BOOST_CHECK(iterVec[0]->nSequenceNumber == 1 && iterVec[1]->nSequenceNumber == 6);

    BOOST_CHECK(iterVec[4]->nType == CTransaction::TX_STAKE && iterVec[5]->nType == CTransaction::TX_STAKE);
    BOOST_CHECK(iterVec[4]->nSequenceNumber == 3 && iterVec[5]->nSequenceNumber == 8);

    BOOST_CHECK(iterVec[8]->nType == CTransaction::TX_TOKEN && iterVec[9]->nType == CTransaction::TX_TOKEN && iterVec[10]->nType == CTransaction::TX_TOKEN);
    BOOST_CHECK(iterVec[8]->nSequenceNumber == 0 && iterVec[9]->nSequenceNumber == 5 && iterVec[10]->nSequenceNumber == 10);
}

BOOST_AUTO_TEST_CASE(multi_index_enroll_tx_first)
{
    CPooledTxLinkSet setTxLinkIndex;
    std::map<uint8, uint16> mapType{
        { 0, CTransaction::TX_TOKEN },
        { 1, CTransaction::TX_CERT },
        { 2, CTransaction::TX_GENESIS },
        { 3, CTransaction::TX_STAKE },
        { 4, CTransaction::TX_WORK }
    };

    uint32 timestamp = 1579021270;
    for (int i = 10; i >= 0; --i)
    {
        CTransaction tx;
        tx.SetNull();
        tx.nTimeStamp = timestamp++;
        tx.nType = mapType[i % 5];
        tx.nLockUntil = i;
        CPooledTx pooledTx(tx, -1, i);
        setTxLinkIndex.insert(CPooledTxLink(&pooledTx));
    }

    BOOST_CHECK(setTxLinkIndex.size() == 11);

    const CPooledTxLinkSetByEnrollTx& idxTxLinkEnrollTx = setTxLinkIndex.get<enroll_tx_first>();
    CPooledTxLinkSetByEnrollTx::iterator iter = idxTxLinkEnrollTx.begin();

    std::vector<CPooledTxLinkSetByEnrollTx::iterator> iterVec;
    for (; iter != idxTxLinkEnrollTx.end(); ++iter)
    {
        // std::cout << "type " << std::hex << iter->nType << std::dec << " seq " << iter->nSequenceNumber << std::endl;
        iterVec.push_back(iter);
    }

    BOOST_CHECK(iterVec[0]->nType == CTransaction::TX_CERT && iterVec[1]->nType == CTransaction::TX_CERT);
    BOOST_CHECK(iterVec[0]->nSequenceNumber == 1 && iterVec[1]->nSequenceNumber == 6);

    int k = 2;
    for (int i = 3; i < 6; ++i)
    {
        BOOST_CHECK(iterVec[i]->nSequenceNumber == k);
        k++;
    }

    k = 7;
    for (int i = 7; i < iterVec.size(); ++i)
    {
        BOOST_CHECK(iterVec[i]->nSequenceNumber == k);
        k++;
    }
}

BOOST_AUTO_TEST_SUITE_END()