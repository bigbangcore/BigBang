// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "verify.h"

#include <thread>

#include "util.h"

using namespace std;
using namespace xengine;

namespace bigbang
{

CVerify::CVerify()
{
    pBlockChain = nullptr;
    pNetChannel = nullptr;

    fExit = false;
    nVerifyThreadCount = boost::thread::hardware_concurrency();
    if (nVerifyThreadCount == 0)
    {
        nVerifyThreadCount = 1;
    }
    for (int i = 0; i < nVerifyThreadCount; i++)
    {
        vVerifyThread.push_back(CThread(string("verify-") + to_string(i), boost::bind(&CVerify::VerifyThreadFunc, this)));
    }
}

CVerify::~CVerify()
{
    pBlockChain = nullptr;
    pNetChannel = nullptr;
}

bool CVerify::HandleInitialize()
{
    if (!GetObject("blockchain", pBlockChain))
    {
        Error("Failed to request blockchain");
        return false;
    }
    if (!GetObject("netchannel", pNetChannel))
    {
        Error("Failed to request netchannel");
        return false;
    }
    return true;
}

void CVerify::HandleDeinitialize()
{
    pBlockChain = nullptr;
    pNetChannel = nullptr;
}

bool CVerify::HandleInvoke()
{
    fExit = false;
    for (int i = 0; i < nVerifyThreadCount; i++)
    {
        if (!ThreadDelayStart(vVerifyThread[i]))
        {
            Error("Failed to start thread");
            return false;
        }
    }
    if (!IVerify::HandleInvoke())
    {
        Error("Failed to HandleInvoke");
        return false;
    }
    return true;
}

void CVerify::HandleHalt()
{
    fExit = true;
    for (int i = 0; i < nVerifyThreadCount; i++)
    {
        ThreadExit(vVerifyThread[i]);
    }
    IVerify::HandleHalt();
}

bool CVerify::AddPowBlockVerify(const uint64& nNonce, const uint256& hashFork, const CBlock& block)
{
    CBlock* pBlock = new CBlock(block);
    if (pBlock == nullptr)
    {
        Error("AddPowBlockVerify new fail");
        return false;
    }
    if (!qVerifyData.SetData(CVerifyData(nNonce, hashFork, CVerifyData::VERIFY_TYPE_BLOCK_POW, pBlock), 120 * 1000))
    {
        Error("AddPowBlockVerify SetData fail");
        delete pBlock;
        return false;
    }
    return true;
}

void CVerify::VerifyThreadFunc()
{
    while (!fExit)
    {
        CVerifyData verifyData;
        if (qVerifyData.GetData(verifyData, 1000))
        {
            VerifyData(verifyData);
        }
    }
}

void CVerify::VerifyData(CVerifyData& verifyData)
{
    if (verifyData.pData != nullptr)
    {
        switch (verifyData.nType)
        {
        case CVerifyData::VERIFY_TYPE_BLOCK_POW:
        {
            pBlockChain->GetPowHash(*(CBlock*)(verifyData.pData));

            network::CEventPeerBlock* pEvent = new network::CEventPeerBlock(verifyData.nNonce, verifyData.hashFork);
            if (pEvent != nullptr)
            {
                pEvent->data = *(CBlock*)(verifyData.pData);
                pNetChannel->PostEvent(pEvent);
            }

            delete (CBlock*)(verifyData.pData);
            break;
        }
        case CVerifyData::VERIFY_TYPE_BLOCK_DPOS:
            break;
        case CVerifyData::VERIFY_TYPE_TX:
            break;
        }
    }
}

} // namespace bigbang
