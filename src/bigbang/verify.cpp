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
    pDispatcher = nullptr;
    pBlockChain = nullptr;
    pNetChannel = nullptr;
    pNetChannel = nullptr;
    nDposIndexCreate = 0;

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
}

bool CVerify::HandleInitialize()
{
    if (!GetObject("dispatcher", pDispatcher))
    {
        Error("Failed to request dispatcher");
        return false;
    }
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
    if (!GetObject("consensus", pConsensus))
    {
        Error("Failed to request consensus");
        return false;
    }
    return true;
}

void CVerify::HandleDeinitialize()
{
    pDispatcher = nullptr;
    pBlockChain = nullptr;
    pNetChannel = nullptr;
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
    Release();
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
    if (!qVerifyData.SetData(CVerifyData(nNonce, hashFork, pBlock), 120 * 1000))
    {
        Error("AddPowBlockVerify SetData fail");
        delete pBlock;
        return false;
    }
    return true;
}

bool CVerify::AddDposBlockVerify(const uint64& nNonce, const CDposVerify* pDpos)
{
    boost::unique_lock<boost::mutex> lock(mtxDposIndex);
    mapDposIndex.insert(make_pair(++nDposIndexCreate, nullptr));
    if (!qVerifyData.SetData(CVerifyData(nNonce, nDposIndexCreate, pDpos), 120 * 1000))
    {
        Error("AddDposBlockVerify SetData fail");
        mapDposIndex.erase(nDposIndexCreate);
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
        {
            CDposVerify* pDpos = (CDposVerify*)(verifyData.pData);
            for (int i = pDpos->updateBlockChain.vBlockAddNew.size() - 1; i >= 0; i--)
            {
                pConsensus->BlockEnroll(pDpos->updateBlockChain.vBlockAddNew[i].GetHash());
            }
            CommitDposVerify(verifyData);
            break;
        }
        case CVerifyData::VERIFY_TYPE_TX:
            break;
        }
    }
}

void CVerify::CommitDposVerify(CVerifyData& verifyData)
{
    boost::unique_lock<boost::mutex> lock(mtxDposIndex);

    if (verifyData.pData != nullptr)
    {
        auto it = mapDposIndex.find(verifyData.nIndex);
        if (it != mapDposIndex.end())
        {
            it->second = (CDposVerify*)(verifyData.pData);
        }
        else
        {
            delete (CDposVerify*)(verifyData.pData);
        }
    }

    auto it = mapDposIndex.begin();
    while (it != mapDposIndex.end())
    {
        if (it->second == nullptr)
        {
            break;
        }
        CDposVerify* pDpos = it->second;
        pDispatcher->ConsensusUpdate(pDpos->updateBlockChain, pDpos->changeTxSet, verifyData.nNonce);
        delete pDpos;
        mapDposIndex.erase(it++);
    }
}

void CVerify::Release()
{
    while (1)
    {
        CVerifyData verifyData;
        if (!qVerifyData.GetData(verifyData, 0))
        {
            break;
        }
        if (verifyData.pData != nullptr)
        {
            switch (verifyData.nType)
            {
            case CVerifyData::VERIFY_TYPE_BLOCK_POW:
                delete (CBlock*)(verifyData.pData);
                break;
            case CVerifyData::VERIFY_TYPE_BLOCK_DPOS:
                delete (CDposVerify*)(verifyData.pData);
                break;
            case CVerifyData::VERIFY_TYPE_TX:
                break;
            }
        }
    }
    {
        boost::unique_lock<boost::mutex> lock(mtxDposIndex);
        auto it = mapDposIndex.begin();
        while (it != mapDposIndex.end())
        {
            if (it->second != nullptr)
            {
                delete (CDposVerify*)(it->second);
            }
            ++it;
        }
    }
}

} // namespace bigbang
