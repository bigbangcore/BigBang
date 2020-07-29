// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_BLOCKMAKER_H
#define BIGBANG_BLOCKMAKER_H

#include <atomic>

#include "base.h"
#include "event.h"
#include "key.h"

namespace bigbang
{

class CBlockMakerHashAlgo
{
public:
    CBlockMakerHashAlgo(const std::string& strAlgoIn, int64 nHashRateIn)
      : strAlgo(strAlgoIn), nHashRate(nHashRateIn) {}
    virtual ~CBlockMakerHashAlgo() {}
    virtual uint256 Hash(const std::vector<unsigned char>& vchData) = 0;

public:
    const std::string strAlgo;
    int64 nHashRate;
};

class CBlockMakerProfile
{
public:
    CBlockMakerProfile() {}
    CBlockMakerProfile(int nAlgoIn, const CDestination& dest, const uint256& nPrivKey)
      : nAlgo(nAlgoIn), destMint(dest)
    {
        keyMint.SetSecret(crypto::CCryptoKeyData(nPrivKey.begin(), nPrivKey.end()));
        BuildTemplate();
    }

    bool IsValid() const
    {
        return (templMint != nullptr);
    }
    bool BuildTemplate();
    std::size_t GetSignatureSize() const
    {
        std::size_t size = templMint->GetTemplateData().size() + 64;
        xengine::CVarInt var(size);
        return (size + xengine::GetSerializeSize(var));
    }
    const CDestination GetDestination() const
    {
        return (CDestination(templMint->GetTemplateId()));
    }

public:
    int nAlgo;
    CDestination destMint;
    crypto::CKey keyMint;
    CTemplateMintPtr templMint;
};

class CBlockMaker : public IBlockMaker, virtual public CBlockMakerEventListener
{
public:
    CBlockMaker();
    ~CBlockMaker();
    bool HandleEvent(CEventBlockMakerUpdate& eventUpdate) override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;
    bool InterruptedPoW(const uint256& hashPrimary);
    bool WaitExit(const long nSeconds);
    bool WaitUpdateEvent(const long nSeconds);
    void PrepareBlock(CBlock& block, const uint256& hashPrev, const uint64& nPrevTime,
                      const uint32& nPrevHeight, const CDelegateAgreement& agreement);
    void ArrangeBlockTx(CBlock& block, const uint256& hashFork, const CBlockMakerProfile& profile);
    bool SignBlock(CBlock& block, const CBlockMakerProfile& profile);
    bool DispatchBlock(const CBlock& block);
    //void ProcessDelegatedProofOfStake(const CAgreementBlock& consParam);
    void ProcessSubFork(const CBlockMakerProfile& profile, const CDelegateAgreement& agreement,
                        const uint256& hashRefBlock, int64 nRefBlockTime, const int32 nPrevHeight, const uint16 nPrevMintType);
    //bool CreateDelegatedBlock(CBlock& block, const uint256& hashFork, const CBlockMakerProfile& profile);
    bool CreateProofOfWork();
   // void PreparePiggyback(CBlock& block, const CDelegateAgreement& agreement, const uint256& hashRefBlock,
                         // int64 nRefBlockTime, const int32 nPrevHeight, const CForkStatus& status, const uint16 nPrevMintType);
    //bool CreateExtended(CBlock& block, const CBlockMakerProfile& profile, const CDelegateAgreement& agreement,
                      //  const uint256& hashRefBlock, const uint256& hashFork, const uint256& hashLastBlock, int64 nTime);

private:
    void BlockMakerThreadFunc();
    void PowThreadFunc();

protected:
    xengine::CThread thrMaker;
    xengine::CThread thrPow;
    boost::mutex mutex;
    boost::condition_variable condExit;
    boost::condition_variable condBlock;
    std::atomic<bool> fExit;
    CForkStatus lastStatus;
    std::map<int, CBlockMakerHashAlgo*> mapHashAlgo;
    std::map<int, CBlockMakerProfile> mapWorkProfile;
    std::map<CDestination, CBlockMakerProfile> mapDelegatedProfile;
    ICoreProtocol* pCoreProtocol;
    IBlockChain* pBlockChain;
    IForkManager* pForkManager;
    ITxPool* pTxPool;
    IDispatcher* pDispatcher;
    IConsensus* pConsensus;
    IService* pService;
};

} // namespace bigbang

#endif //BIGBANG_BLOCKMAKER_H
