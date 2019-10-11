// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_BLOCKMAKER_H
#define BIGBANG_BLOCKMAKER_H

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
    const std::string strAlgo;
    int64 nHashRate;

public:
    virtual uint256 Hash(const std::vector<unsigned char>& vchData) = 0;
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
    bool Interrupted()
    {
        return (nMakerStatus != MAKER_RUN);
    }
    bool Wait(long nSeconds);
    bool Wait(long nSeconds, const uint256& hashPrimaryBlock);
    void PrepareBlock(CBlock& block, const uint256& hashPrev, int64 nPrevTime, int nPrevHeight, const CDelegateAgreement& agreement);
    void ArrangeBlockTx(CBlock& block, const uint256& hashFork, const CBlockMakerProfile& profile);
    bool SignBlock(CBlock& block, const CBlockMakerProfile& profile);
    bool DispatchBlock(CBlock& block);
    bool CreateProofOfWorkBlock(CBlock& block);
    void ProcessDelegatedProofOfStake(CBlock& block, const CDelegateAgreement& agreement, const int32 nPrevHeight);
    void ProcessExtended(const CDelegateAgreement& agreement, const uint256& hashPrimaryBlock,
                         int64 nPrimaryBlockTime, const int32 nPrimaryBlockHeight);
    bool CreateDelegatedBlock(CBlock& block, const uint256& hashFork, const CBlockMakerProfile& profile, std::size_t nWeight);
    bool CreateProofOfWork(CBlock& block, CBlockMakerHashAlgo* pHashAlgo);
    void CreatePiggyback(const CBlockMakerProfile& profile, const CDelegateAgreement& agreement,
                         const uint256& hashRefBlock, int64 nRefBlockTime, const int32 nPrevHeight);
    void CreateExtended(const CBlockMakerProfile& profile, const CDelegateAgreement& agreement,
                        const uint256& hashRefBlock, const std::set<uint256>& setFork, int nPrimaryBlockHeight, int64 nTime);
    bool GetAvailiableDelegatedProfile(const std::vector<CDestination>& vBallot, std::vector<CBlockMakerProfile*>& vProfile);
    bool GetAvailiableExtendedFork(std::set<uint256>& setFork);

private:
    enum
    {
        MAKER_RUN = 0,
        MAKER_RESET = 1,
        MAKER_EXIT = 2,
        MAKER_HOLD = 3
    };
    void BlockMakerThreadFunc();
    void ExtendedMakerThreadFunc();

protected:
    mutable boost::shared_mutex rwAccess;
    xengine::CThread thrMaker;
    xengine::CThread thrExtendedMaker;
    boost::mutex mutex;
    boost::condition_variable cond;
    int nMakerStatus;
    uint256 hashLastBlock;
    int64 nLastBlockTime;
    int nLastBlockHeight;
    uint256 nLastAgreement;
    std::size_t nLastWeight;
    CDelegateAgreement currentAgreement;
    std::map<int, CBlockMakerHashAlgo*> mapHashAlgo;
    std::map<int, CBlockMakerProfile> mapWorkProfile;
    std::map<CDestination, CBlockMakerProfile> mapDelegatedProfile;
    ICoreProtocol* pCoreProtocol;
    IWorldLineController* pWorldLineCtrl;
    IForkManager* pForkManager;
    ITxPoolController* pTxPoolCtrl;
    IDispatcher* pDispatcher;
    IConsensus* pConsensus;
};

} // namespace bigbang

#endif //BIGBANG_BLOCKMAKER_H
