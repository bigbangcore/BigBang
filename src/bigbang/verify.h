// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_VERIFY_H
#define BIGBANG_VERIFY_H

#include "base.h"
#include "nbase/mthbase.h"
#include "peernet.h"
#include "struct.h"

namespace bigbang
{

class CVerify : public IVerify
{
public:
    CVerify();
    ~CVerify();

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;

public:
    bool AddPowBlockVerify(const uint64& nNonce, const uint256& hashFork, const CBlock& block) override;

protected:
    void VerifyThreadFunc();
    void VerifyData(CVerifyData& verifyData);

protected:
    IBlockChain* pBlockChain;
    network::INetChannel* pNetChannel;

    uint32 nVerifyThreadCount;
    std::vector<xengine::CThread> vVerifyThread;
    bool fExit;

    xengine::CMthQueue<CVerifyData> qVerifyData;
};

} // namespace bigbang

#endif // BIGBANG_VERIFY_H
