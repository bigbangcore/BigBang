// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockmaker.h"

#include <thread>

#include "address.h"
#include "template/delegate.h"
#include "template/mint.h"
#include "template/proof.h"
#include "util.h"

using namespace std;
using namespace xengine;

#define INITIAL_HASH_RATE (8)
#define WAIT_AGREEMENT_TIME_OFFSET -5
#define WAIT_NEWBLOCK_TIME (BLOCK_TARGET_SPACING + 5)
#define WAIT_LAST_EXTENDED_TIME (BLOCK_TARGET_SPACING - 10)

namespace bigbang
{

//////////////////////////////
// CBlockMakerHashAlgo

class CHashAlgo_Cryptonight : public bigbang::CBlockMakerHashAlgo
{
public:
    CHashAlgo_Cryptonight(int64 nHashRateIn)
      : CBlockMakerHashAlgo("Cryptonight", nHashRateIn) {}
    uint256 Hash(const std::vector<unsigned char>& vchData)
    {
        return crypto::CryptoPowHash(&vchData[0], vchData.size());
    }
};

//////////////////////////////
// CBlockMakerProfile
bool CBlockMakerProfile::BuildTemplate()
{
    crypto::CPubKey pubkey;
    if (destMint.GetPubKey(pubkey) && pubkey == keyMint.GetPubKey())
    {
        return false;
    }
    if (nAlgo == CM_MPVSS)
    {
        templMint = CTemplateMint::CreateTemplatePtr(new CTemplateDelegate(keyMint.GetPubKey(), destMint));
    }
    else if (nAlgo < CM_MAX)
    {
        templMint = CTemplateMint::CreateTemplatePtr(new CTemplateProof(keyMint.GetPubKey(), destMint));
    }
    return (templMint != nullptr);
}

//////////////////////////////
// CBlockMaker

CBlockMaker::CBlockMaker()
  : thrMaker("blockmaker", boost::bind(&CBlockMaker::BlockMakerThreadFunc, this)),
    thrPow("powmaker", boost::bind(&CBlockMaker::PowThreadFunc, this))
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pForkManager = nullptr;
    pTxPool = nullptr;
    pDispatcher = nullptr;
    pConsensus = nullptr;
    pService = nullptr;
    mapHashAlgo[CM_CRYPTONIGHT] = new CHashAlgo_Cryptonight(INITIAL_HASH_RATE);
}

CBlockMaker::~CBlockMaker()
{
    for (map<int, CBlockMakerHashAlgo*>::iterator it = mapHashAlgo.begin(); it != mapHashAlgo.end(); ++it)
    {
        delete ((*it).second);
    }
    mapHashAlgo.clear();
}

bool CBlockMaker::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol\n");
        return false;
    }

    if (!GetObject("blockchain", pBlockChain))
    {
        Error("Failed to request blockchain\n");
        return false;
    }

    if (!GetObject("forkmanager", pForkManager))
    {
        Error("Failed to request forkmanager\n");
        return false;
    }

    if (!GetObject("txpool", pTxPool))
    {
        Error("Failed to request txpool\n");
        return false;
    }

    if (!GetObject("dispatcher", pDispatcher))
    {
        Error("Failed to request dispatcher\n");
        return false;
    }

    if (!GetObject("consensus", pConsensus))
    {
        Error("Failed to request consensus\n");
        return false;
    }

    if (!GetObject("service", pService))
    {
        Error("Failed to request service");
        return false;
    }

    if (!MintConfig()->destMpvss.IsNull() && MintConfig()->keyMpvss != 0)
    {
        CBlockMakerProfile profile(CM_MPVSS, MintConfig()->destMpvss, MintConfig()->keyMpvss);
        if (profile.IsValid())
        {
            mapDelegatedProfile.insert(make_pair(profile.GetDestination(), profile));
        }
    }

    if (!MintConfig()->destCryptonight.IsNull() && MintConfig()->keyCryptonight != 0)
    {
        CBlockMakerProfile profile(CM_CRYPTONIGHT, MintConfig()->destCryptonight, MintConfig()->keyCryptonight);
        if (profile.IsValid())
        {
            mapWorkProfile.insert(make_pair(CM_CRYPTONIGHT, profile));
        }
    }

    // print log
    const char* ConsensusMethodName[CM_MAX] = { "mpvss", "cryptonight" };
    Log("Block maker started");
    for (map<int, CBlockMakerProfile>::iterator it = mapWorkProfile.begin(); it != mapWorkProfile.end(); ++it)
    {
        CBlockMakerProfile& profile = (*it).second;
        Log("Profile [%s] : dest=%s,pubkey=%s",
            ConsensusMethodName[(*it).first],
            CAddress(profile.destMint).ToString().c_str(),
            profile.keyMint.GetPubKey().GetHex().c_str());
    }
    for (map<CDestination, CBlockMakerProfile>::iterator it = mapDelegatedProfile.begin();
         it != mapDelegatedProfile.end(); ++it)
    {
        CBlockMakerProfile& profile = (*it).second;
        Log("Profile [%s] : dest=%s,pubkey=%s\n",
            ConsensusMethodName[CM_MPVSS],
            CAddress(profile.destMint).ToString().c_str(),
            profile.keyMint.GetPubKey().GetHex().c_str());
    }

    return true;
}

void CBlockMaker::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pForkManager = nullptr;
    pTxPool = nullptr;
    pDispatcher = nullptr;
    pConsensus = nullptr;
    pService = nullptr;

    mapWorkProfile.clear();
    mapDelegatedProfile.clear();
}

bool CBlockMaker::HandleInvoke()
{
    if (!IBlockMaker::HandleInvoke())
    {
        return false;
    }

    fExit = true;
    if (!mapDelegatedProfile.empty())
    {
        fExit = false;
        {
            boost::unique_lock<boost::mutex> lock(mutex);
            pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), lastStatus.hashLastBlock,
                                      lastStatus.nLastBlockHeight, lastStatus.nLastBlockTime, lastStatus.nMintType);
        }
        if (!ThreadDelayStart(thrMaker))
        {
            return false;
        }
    }
    if (!mapWorkProfile.empty())
    {
        fExit = false;
        if (!ThreadDelayStart(thrPow))
        {
            return false;
        }
    }
    return true;
}

void CBlockMaker::HandleHalt()
{
    fExit = true;
    condExit.notify_all();
    condBlock.notify_all();

    thrMaker.Interrupt();
    ThreadExit(thrMaker);

    thrPow.Interrupt();
    ThreadExit(thrPow);

    IBlockMaker::HandleHalt();
}

bool CBlockMaker::HandleEvent(CEventBlockMakerUpdate& eventUpdate)
{
    if (fExit)
    {
        return true;
    }

    boost::unique_lock<boost::mutex> lock(mutex);

    CBlockMakerUpdate& data = eventUpdate.data;
    lastStatus.hashLastBlock = data.hashBlock;
    lastStatus.nLastBlockTime = data.nBlockTime;
    lastStatus.nLastBlockHeight = data.nBlockHeight;
    lastStatus.nMintType = data.nMintType;

    condBlock.notify_all();
    return true;
}

bool CBlockMaker::InterruptedPoW(const uint256& hashPrimary)
{
    boost::unique_lock<boost::mutex> lock(mutex);
    return fExit || (hashPrimary != lastStatus.hashLastBlock);
}

bool CBlockMaker::WaitExit(const long nSeconds)
{
    if (nSeconds <= 0)
    {
        return !fExit;
    }
    boost::system_time const timeout = boost::get_system_time() + boost::posix_time::seconds(nSeconds);
    boost::unique_lock<boost::mutex> lock(mutex);
    while (!fExit)
    {
        if (!condExit.timed_wait(lock, timeout))
        {
            break;
        }
    }
    return !fExit;
}

bool CBlockMaker::WaitUpdateEvent(const long nSeconds)
{
    boost::system_time const timeout = boost::get_system_time() + boost::posix_time::seconds(nSeconds);
    boost::unique_lock<boost::mutex> lock(mutex);
    while (!fExit)
    {
        if (!condBlock.timed_wait(lock, timeout))
        {
            break;
        }
    }
    return !fExit;
}

void CBlockMaker::PrepareBlock(CBlock& block, const uint256& hashPrev, const uint64& nPrevTime,
                               const uint32& nPrevHeight, const CDelegateAgreement& agreement)
{
    block.SetNull();
    block.nType = CBlock::BLOCK_PRIMARY;
    block.hashPrev = hashPrev;
    CProofOfSecretShare proof;
    proof.nWeight = agreement.nWeight;
    proof.nAgreement = agreement.nAgreement;
    proof.Save(block.vchProof);
    if (agreement.nAgreement != 0)
    {
        pConsensus->GetProof(nPrevHeight + 1, block.vchProof);
    }
}

void CBlockMaker::ArrangeBlockTx(CBlock& block, const uint256& hashFork, const CBlockMakerProfile& profile)
{
    size_t nMaxTxSize = MAX_BLOCK_SIZE - GetSerializeSize(block) - profile.GetSignatureSize();
    int64 nTotalTxFee = 0;
    if (!pTxPool->ArrangeBlockTx(hashFork, block.hashPrev, block.GetBlockTime(), nMaxTxSize, block.vtx, nTotalTxFee))
    {
        Error("ArrangeBlockTx error, block: %s", block.GetHash().ToString().c_str());
    }
    block.hashMerkle = block.CalcMerkleTreeRoot();
    block.txMint.nAmount += nTotalTxFee;
}

bool CBlockMaker::SignBlock(CBlock& block, const CBlockMakerProfile& profile)
{
    uint256 hashSig = block.GetHash();
    vector<unsigned char> vchMintSig;
    if (!profile.keyMint.Sign(hashSig, vchMintSig))
    {
        StdTrace("blockmaker", "keyMint Sign failed. hashSig: %s", hashSig.ToString().c_str());
        return false;
    }
    return profile.templMint->BuildBlockSignature(hashSig, vchMintSig, block.vchSig);
}

bool CBlockMaker::DispatchBlock(const CBlock& block)
{
    Debug("Dispatching block: %s, type: %u", block.GetHash().ToString().c_str(), block.nType);
    int nWait = block.nTimeStamp - GetNetTime();
    if (nWait > 0 && !WaitExit(nWait))
    {
        return false;
    }
    Errno err = pDispatcher->AddNewBlock(block);
    if (err != OK)
    {
        Error("Dispatch new block failed (%d) : %s\n", err, ErrorString(err));
        return false;
    }
    return true;
}

void CBlockMaker::ProcessDelegatedProofOfStake(const CAgreementBlock& consParam)
{
    map<CDestination, CBlockMakerProfile>::iterator it = mapDelegatedProfile.find(consParam.agreement.vBallot[0]);
    if (it != mapDelegatedProfile.end())
    {
        CBlockMakerProfile& profile = (*it).second;

        CBlock block;
        PrepareBlock(block, consParam.hashPrev, consParam.nPrevTime, consParam.nPrevHeight, consParam.agreement);

        // get block time
        block.nTimeStamp = pBlockChain->DPoSTimestamp(block.hashPrev);
        if (block.nTimeStamp == 0)
        {
            Error("Get DPoSTimestamp error, hashPrev: %s", block.hashPrev.ToString().c_str());
            return;
        }

        // create DPoS primary block
        if (!CreateDelegatedBlock(block, pCoreProtocol->GetGenesisBlockHash(), profile))
        {
            Error("CreateDelegatedBlock error, hashPrev: %s", block.hashPrev.ToString().c_str());
            return;
        }

        // dispatch DPoS primary block
        if (DispatchBlock(block))
        {
            pDispatcher->SetConsensus(consParam);

            // create sub fork blocks
            ProcessSubFork(profile, consParam.agreement, block.GetHash(), block.GetBlockTime(), consParam.nPrevHeight, consParam.nPrevMintType);
        }
    }
}

void CBlockMaker::ProcessSubFork(const CBlockMakerProfile& profile, const CDelegateAgreement& agreement,
                                 const uint256& hashRefBlock, int64 nRefBlockTime, const int32 nPrevHeight, const uint16 nPrevMintType)
{
    map<uint256, CForkStatus> mapForkStatus;
    pBlockChain->GetForkStatus(mapForkStatus);

    // create subsidiary task
    multimap<int64, pair<uint256, CBlock>> mapBlocks;
    for (map<uint256, CForkStatus>::iterator it = mapForkStatus.begin(); it != mapForkStatus.end(); ++it)
    {
        if (it->first != pCoreProtocol->GetGenesisBlockHash())
        {
            CBlock block;
            PreparePiggyback(block, agreement, hashRefBlock, nRefBlockTime, nPrevHeight, it->second, nPrevMintType);
            mapBlocks.insert(make_pair(nRefBlockTime, make_pair(it->first, block)));
        }
    }

    while (!mapBlocks.empty())
    {
        auto it = mapBlocks.begin();
        int64 nSeconds = it->first - GetNetTime();
        const uint256 hashFork = it->second.first;
        CBlock block = it->second.second;
        mapBlocks.erase(it);

        if (!WaitExit(nSeconds))
        {
            break;
        }

        bool fCreateExtendedTask = false;
        if (block.IsSubsidiary())
        {
            // query previous last extended block
            if (block.hashPrev == 0)
            {
                uint256 hashLastBlock;
                int64 nLastTime;

                bool fInWaitTime = (nPrevMintType == CTransaction::TX_STAKE) && (GetNetTime() - nRefBlockTime < WAIT_LAST_EXTENDED_TIME);
                if (pBlockChain->GetLastBlockOfHeight(hashFork, nPrevHeight, hashLastBlock, nLastTime)
                    && (!fInWaitTime || (nLastTime + EXTENDED_BLOCK_SPACING == nRefBlockTime)))
                {
                    // last is PoW or last extended or timeout
                    block.hashPrev = hashLastBlock;
                }
                else if (fInWaitTime)
                {
                    // wait the last exteded block for 1s
                    mapBlocks.insert(make_pair(GetNetTime() + 1, make_pair(hashFork, block)));
                }
                else
                {
                    Error("ProcessSubFork get last block error, fork: %s", hashFork.ToString().c_str());
                }
            }

            // make subsidiary block
            if (block.hashPrev != 0)
            {
                if (CreateDelegatedBlock(block, hashFork, profile))
                {
                    if (DispatchBlock(block))
                    {
                        fCreateExtendedTask = true;
                    }
                    else
                    {
                        Error("ProcessSubFork dispatch subsidiary block error, fork: %s, block: %s", hashFork.ToString().c_str(), block.GetHash().ToString().c_str());
                    }
                }
                else
                {
                    Error("ProcessSubFork CreateDelegatedBlock error, fork: %s, block: %s", hashFork.ToString().c_str(), block.GetHash().ToString().c_str());
                }
            }
        }
        else
        {
            // make extended block
            if (DispatchBlock(block))
            {
                fCreateExtendedTask = true;
            }
            else
            {
                Error("ProcessSubFork dispatch subsidiary block error, fork: %s, block: %s", hashFork.ToString().c_str(), block.GetHash().ToString().c_str());
            }
        }

        // create next extended task
        if (fCreateExtendedTask)
        {
            if (block.nTimeStamp + EXTENDED_BLOCK_SPACING < nRefBlockTime + BLOCK_TARGET_SPACING)
            {
                CBlock extended;
                if (CreateExtended(extended, profile, agreement, hashRefBlock, hashFork, block.GetHash(), block.nTimeStamp + EXTENDED_BLOCK_SPACING))
                {
                    mapBlocks.insert(make_pair(extended.nTimeStamp, make_pair(hashFork, extended)));
                }
                else
                {
                    Error("ProcessSubFork create extended block task error, fork: %s, block: %s, seq: %d", hashFork.ToString().c_str(), block.GetHash().ToString().c_str(), ((int64)(extended.nTimeStamp) - nRefBlockTime) / EXTENDED_BLOCK_SPACING);
                }
            }
        }
    }
}

bool CBlockMaker::CreateDelegatedBlock(CBlock& block, const uint256& hashFork, const CBlockMakerProfile& profile)
{
    CDestination destSendTo = profile.GetDestination();

    int64 nReward;
    if (!pBlockChain->GetBlockMintReward(block.hashPrev, nReward))
    {
        Error("GetBlockMintReward error, hashPrev: %s", block.hashPrev.ToString().c_str());
        return false;
    }

    CTransaction& txMint = block.txMint;
    txMint.nType = CTransaction::TX_STAKE;
    txMint.nTimeStamp = block.nTimeStamp;
    txMint.hashAnchor = hashFork;
    txMint.sendTo = destSendTo;
    txMint.nAmount = nReward;

    ArrangeBlockTx(block, hashFork, profile);

    return SignBlock(block, profile);
}

void CBlockMaker::PreparePiggyback(CBlock& block, const CDelegateAgreement& agreement, const uint256& hashRefBlock,
                                   int64 nRefBlockTime, const int32 nPrevHeight, const CForkStatus& status, const uint16 nPrevMintType)
{
    CProofOfPiggyback proof;
    proof.nWeight = agreement.nWeight;
    proof.nAgreement = agreement.nAgreement;
    proof.hashRefBlock = hashRefBlock;

    block.nType = CBlock::BLOCK_SUBSIDIARY;
    block.nTimeStamp = nRefBlockTime;
    proof.Save(block.vchProof);
    if (status.nLastBlockHeight == nPrevHeight && status.nLastBlockTime < nRefBlockTime)
    {
        // last is PoW or last extended or timeouot
        if (nPrevMintType != CTransaction::TX_STAKE
            || status.nLastBlockTime + EXTENDED_BLOCK_SPACING == nRefBlockTime
            || GetNetTime() - nRefBlockTime >= WAIT_LAST_EXTENDED_TIME)
        {
            block.hashPrev = status.hashLastBlock;
        }
    }
}

bool CBlockMaker::CreateExtended(CBlock& block, const CBlockMakerProfile& profile, const CDelegateAgreement& agreement,
                                 const uint256& hashRefBlock, const uint256& hashFork, const uint256& hashLastBlock, int64 nTime)
{
    CProofOfPiggyback proof;
    proof.nWeight = agreement.nWeight;
    proof.nAgreement = agreement.nAgreement;
    proof.hashRefBlock = hashRefBlock;

    block.nType = CBlock::BLOCK_EXTENDED;
    block.nTimeStamp = nTime;
    block.hashPrev = hashLastBlock;
    proof.Save(block.vchProof);

    CTransaction& txMint = block.txMint;
    txMint.nType = CTransaction::TX_STAKE;
    txMint.nTimeStamp = block.nTimeStamp;
    txMint.hashAnchor = hashFork;
    txMint.sendTo = profile.GetDestination();
    txMint.nAmount = 0;

    ArrangeBlockTx(block, hashFork, profile);
    return SignBlock(block, profile);
}

bool CBlockMaker::CreateProofOfWork()
{
    int nConsensus = CM_CRYPTONIGHT;
    map<int, CBlockMakerProfile>::iterator it = mapWorkProfile.find(nConsensus);
    if (it == mapWorkProfile.end())
    {
        StdError("blockmaker", "did not find Work profile");
        return false;
    }
    CBlockMakerProfile& profile = (*it).second;
    CBlockMakerHashAlgo* pHashAlgo = mapHashAlgo[profile.nAlgo];
    if (pHashAlgo == nullptr)
    {
        StdError("blockmaker", "pHashAlgo is null");
        return false;
    }

    vector<unsigned char> vchWorkData;
    int nPrevBlockHeight = 0;
    uint256 hashPrev;
    uint32 nPrevTime = 0;
    int nAlgo = 0, nBits = 0;
    if (!pService->GetWork(vchWorkData, nPrevBlockHeight, hashPrev, nPrevTime, nAlgo, nBits, profile.templMint))
    {
        //StdTrace("blockmaker", "GetWork fail");
        return false;
    }

    uint32& nTime = *((uint32*)&vchWorkData[4]);
    uint64_t& nNonce = *((uint64_t*)&vchWorkData[vchWorkData.size() - sizeof(uint64_t)]);
    nNonce = (GetTime() % 0xFFFFFF) << 40;

    int64& nHashRate = pHashAlgo->nHashRate;
    int64 nHashComputeCount = 0;
    int64 nHashComputeBeginTime = GetTime();

    Log("Proof-of-work: start hash compute, target height: %d, difficulty bits: (%d)", nPrevBlockHeight + 1, nBits);

    uint256 hashTarget = (~uint256(uint64(0)) >> nBits);
    while (!InterruptedPoW(hashPrev))
    {
        if (nHashRate == 0)
        {
            nHashRate = 1;
        }
        for (int i = 0; i < nHashRate; i++)
        {
            uint256 hash = pHashAlgo->Hash(vchWorkData);
            nHashComputeCount++;
            if (hash <= hashTarget)
            {
                int64 nDuration = GetTime() - nHashComputeBeginTime;
                int nCompHashRate = ((nDuration <= 0) ? 0 : (nHashComputeCount / nDuration));

                Log("Proof-of-work: block found (%s), target height: %d, compute: (rate:%ld, count:%ld, duration:%lds, hashrate:%ld), difficulty bits: (%d)\nhash :   %s\ntarget : %s",
                    pHashAlgo->strAlgo.c_str(), nPrevBlockHeight + 1, nHashRate, nHashComputeCount, nDuration, nCompHashRate, nBits,
                    hash.GetHex().c_str(), hashTarget.GetHex().c_str());

                uint256 hashBlock;
                Errno err = pService->SubmitWork(vchWorkData, profile.templMint, profile.keyMint, hashBlock);
                if (err != OK)
                {
                    return false;
                }
                return true;
            }
            nNonce++;
        }

        int64 nNetTime = GetNetTime();
        if (nTime + 1 < nNetTime)
        {
            nHashRate /= (nNetTime - nTime);
            nTime = nNetTime;
        }
        else if (nTime == nNetTime)
        {
            nHashRate *= 2;
        }
    }
    Log("Proof-of-work: target height: %d, compute interrupted.", nPrevBlockHeight + 1);
    return false;
}

void CBlockMaker::BlockMakerThreadFunc()
{
    uint256 hashCachePrev;
    bool fCachePow = false;
    int64 nWaitTime = 1;
    while (!fExit)
    {
        if (nWaitTime < 1)
        {
            nWaitTime = 1;
        }
        if (!WaitUpdateEvent(nWaitTime))
        {
            break;
        }

        CAgreementBlock consParam;
        if (!pConsensus->GetNextConsensus(consParam))
        {
            StdDebug("BlockMaker", "BlockMakerThreadFunc: GetNextConsensus fail, target height: %d, wait time: %ld, last height: %d, prev block: %s",
                     consParam.nPrevHeight + 1, consParam.nWaitTime, lastStatus.nLastBlockHeight, consParam.hashPrev.GetHex().c_str());
            nWaitTime = consParam.nWaitTime;
            continue;
        }
        StdDebug("BlockMaker", "BlockMakerThreadFunc: GetNextConsensus success, target height: %d, wait time: %ld, last height: %d, prev block: %s",
                 consParam.nPrevHeight + 1, consParam.nWaitTime, lastStatus.nLastBlockHeight, consParam.hashPrev.GetHex().c_str());
        nWaitTime = consParam.nWaitTime;

        if (hashCachePrev != consParam.hashPrev || fCachePow != consParam.agreement.IsProofOfWork())
        {
            hashCachePrev = consParam.hashPrev;
            fCachePow = consParam.agreement.IsProofOfWork();
            if (consParam.agreement.IsProofOfWork())
            {
                Log("GetAgreement: height: %d, consensus: pow", consParam.nPrevHeight + 1);
            }
            else
            {
                Log("GetAgreement: height: %d, consensus: dpos, ballot address: %s", consParam.nPrevHeight + 1, CAddress(consParam.agreement.vBallot[0]).ToString().c_str());
            }
        }
        try
        {
            if (!consParam.agreement.IsProofOfWork())
            {
                ProcessDelegatedProofOfStake(consParam);
            }
            pDispatcher->SetConsensus(consParam);
        }
        catch (exception& e)
        {
            Error("Block maker error: %s", e.what());
            break;
        }
    }
    Log("Block maker exited");
}

void CBlockMaker::PowThreadFunc()
{
    if (!WaitExit(5))
    {
        Log("Pow exited non");
        return;
    }
    while (WaitExit(1))
    {
        CreateProofOfWork();
    }
    Log("Pow exited");
}

} // namespace bigbang
