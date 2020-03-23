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
#define WAIT_AGREEMENT_TIME (BLOCK_TARGET_SPACING - 5)
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
  : thrMaker("blockmaker", boost::bind(&CBlockMaker::BlockMakerThreadFunc, this))
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pForkManager = nullptr;
    pTxPool = nullptr;
    pDispatcher = nullptr;
    pConsensus = nullptr;
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

    mapWorkProfile.clear();
    mapDelegatedProfile.clear();
}

bool CBlockMaker::HandleInvoke()
{
    if (!IBlockMaker::HandleInvoke())
    {
        return false;
    }

    if (!mapWorkProfile.empty() || !mapDelegatedProfile.empty())
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
    else
    {
        fExit = true;
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
    lastStatus.nMoneySupply = data.nMoneySupply;

    if (lastStatus.nLastBlockHeight == currentStatus.nLastBlockHeight)
    {
        currentStatus = lastStatus;
    }

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

bool CBlockMaker::WaitLastBlock(const long nSeconds, const uint256& hashPrimary)
{
    boost::system_time const timeout = boost::get_system_time() + boost::posix_time::seconds(nSeconds);
    boost::unique_lock<boost::mutex> lock(mutex);
    while (!fExit && (hashPrimary == lastStatus.hashLastBlock))
    {
        if (!condBlock.timed_wait(lock, timeout))
        {
            break;
        }
    }
    return !fExit && (hashPrimary == lastStatus.hashLastBlock);
}

bool CBlockMaker::WaitCurrentBlock(const long nSeconds, const uint256& hashPrimary)
{
    boost::system_time const timeout = boost::get_system_time() + boost::posix_time::seconds(nSeconds);
    boost::unique_lock<boost::mutex> lock(mutex);
    while (!fExit && (hashPrimary == currentStatus.hashLastBlock))
    {
        if (!condBlock.timed_wait(lock, timeout))
        {
            break;
        }
    }
    return !fExit && (hashPrimary == currentStatus.hashLastBlock);
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
        // TODO: blockmaker need to handle failed, do not forget
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

    if (nWait > 0)
    {
        if (block.IsPrimary() && block.txMint.nType == CTransaction::TX_WORK)
        {
            if (!WaitLastBlock(nWait, block.hashPrev))
            {
                StdTrace("blockmaker", "Wait to dispatch primary PoW block failed. nWait: %d", nWait);
                return false;
            }
        }
        else if (block.IsPrimary() && block.txMint.nType == CTransaction::TX_STAKE)
        {
            if (!WaitCurrentBlock(nWait, block.hashPrev))
            {
                StdTrace("blockmaker", "Wait to dispatch primary DPoS block failed nWait: %d", nWait);
                return false;
            }
        }
        else if (block.IsSubsidiary() || block.IsExtended())
        {
            if (!WaitExit(nWait))
            {
                return false;
            }
        }
        else
        {
            Error("Dispatch new block failed : unknown block type, block type: %u, mint type: %u\n", block.nType, block.txMint.nType);
            return false;
        }
    }
    Errno err = pDispatcher->AddNewBlock(block);
    if (err != OK)
    {
        Error("Dispatch new block failed (%d) : %s\n", err, ErrorString(err));
        return false;
    }
    return true;
}

void CBlockMaker::ProcessDelegatedProofOfWork(const uint256& hashPrimaryBlock, const int64& nPrimaryBlockTime,
                                              const int& nPrimaryBlockHeight, const uint16& nPrimaryMintType,
                                              const CDelegateAgreement& agreement)
{
    CBlock block;
    PrepareBlock(block, hashPrimaryBlock, nPrimaryBlockTime, nPrimaryBlockHeight, agreement);

    int nConsensus = CM_CRYPTONIGHT;
    map<int, CBlockMakerProfile>::iterator it = mapWorkProfile.find(nConsensus);
    if (it == mapWorkProfile.end())
    {
        StdTrace("blockmaker", "did not find Work profile");
        return;
    }

    CBlockMakerProfile& profile = (*it).second;
    CDestination destSendTo = profile.GetDestination();

    int nAlgo = nConsensus;
    int nBits;
    int64 nReward;
    if (!pBlockChain->GetProofOfWorkTarget(block.hashPrev, nAlgo, nBits, nReward))
    {
        StdTrace("blockmaker", "Get PoW Target failed");
        return;
    }

    CTransaction& txMint = block.txMint;
    txMint.nType = CTransaction::TX_WORK;
    txMint.hashAnchor = block.hashPrev;
    txMint.sendTo = destSendTo;
    txMint.nAmount = nReward;

    block.vchProof.resize(block.vchProof.size() + CProofOfHashWorkCompact::PROOFHASHWORK_SIZE);
    CProofOfHashWorkCompact proof;
    proof.nAlgo = nAlgo;
    proof.nBits = nBits;
    proof.destMint = destSendTo;
    proof.nNonce = 0;
    proof.Save(block.vchProof);

    if (!CreateProofOfWork(block, mapHashAlgo[profile.nAlgo]))
    {
        StdTrace("blockmaker", "Create PoW failed");
        return;
    }

    txMint.nTimeStamp = block.nTimeStamp;
    ArrangeBlockTx(block, pCoreProtocol->GetGenesisBlockHash(), profile);
    if (!SignBlock(block, profile))
    {
        Error("Sign block failed.\n");
        return;
    }

    DispatchBlock(block);
}

void CBlockMaker::ProcessDelegatedProofOfStake(uint256& hashPrimaryBlock, int64& nPrimaryBlockTime,
                                               int& nPrimaryBlockHeight, uint16& nPrimaryMintType,
                                               const CDelegateAgreement& agreement)
{
    map<CDestination, CBlockMakerProfile>::iterator it = mapDelegatedProfile.find(agreement.vBallot[0]);
    if (it != mapDelegatedProfile.end())
    {
        CBlockMakerProfile& profile = (*it).second;

        for (;;)
        {
            CBlock block;
            PrepareBlock(block, hashPrimaryBlock, nPrimaryBlockTime, nPrimaryBlockHeight, agreement);

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
                // create sub fork blocks
                ProcessSubFork(profile, agreement, block.GetHash(), block.GetBlockTime(), nPrimaryBlockHeight, nPrimaryMintType);
                return;
            }
            else
            {
                boost::unique_lock<boost::mutex> lock(mutex);
                if (hashPrimaryBlock != currentStatus.hashLastBlock && nPrimaryBlockHeight == currentStatus.nLastBlockHeight)
                {
                    // switch prev block, create DPoS primary again
                    hashPrimaryBlock = currentStatus.hashLastBlock;
                    nPrimaryBlockTime = currentStatus.nLastBlockTime;
                    nPrimaryBlockHeight = currentStatus.nLastBlockHeight;
                    nPrimaryMintType = currentStatus.nMintType;
                    Log("Dispatch DPoS Primary block switch prev, old hashPrev: %s, new hashPrev: %s", block.hashPrev.ToString().c_str(), hashPrimaryBlock.ToString().c_str());
                }
                else
                {
                    Error("Dispatch DPoS Primary block error, hashPrev: %s", block.hashPrev.ToString().c_str());
                    return;
                }
            }
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
            mapBlocks.insert(make_pair(nRefBlockTime - GetNetTime(), make_pair(it->first, block)));
        }
    }

    while (!mapBlocks.empty())
    {
        auto it = mapBlocks.begin();
        int64 nSeconds = it->first;
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
                int nLastHeight;
                int64 nLastTime;
                uint16 nLastMintType;
                if (pBlockChain->GetLastBlock(hashFork, hashLastBlock, nLastHeight, nLastTime, nLastMintType))
                {
                    // last is PoW or last extended or timeouot
                    if (nLastHeight == nPrevHeight && nLastTime < nRefBlockTime
                        && (nPrevMintType != CTransaction::TX_STAKE
                            || nLastTime + EXTENDED_BLOCK_SPACING == nRefBlockTime
                            || GetNetTime() - nRefBlockTime >= WAIT_LAST_EXTENDED_TIME))
                    {
                        block.hashPrev = hashLastBlock;
                    }
                    else
                    {
                        // wait the last exteded block for 1s
                        mapBlocks.insert(make_pair(1, make_pair(hashFork, block)));
                    }
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
                    mapBlocks.insert(make_pair(extended.nTimeStamp - GetNetTime(), make_pair(hashFork, extended)));
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

bool CBlockMaker::CreateProofOfWork(CBlock& block, CBlockMakerHashAlgo* pHashAlgo)
{
    block.nTimeStamp = GetNetTime();

    CProofOfHashWorkCompact proof;
    proof.Load(block.vchProof);

    int nBits = proof.nBits;
    vector<unsigned char> vchProofOfWork;
    block.GetSerializedProofOfWorkData(vchProofOfWork);

    uint32& nTime = *((uint32*)&vchProofOfWork[4]);
    uint64_t& nNonce = *((uint64_t*)&vchProofOfWork[vchProofOfWork.size() - sizeof(uint64_t)]);

    int64& nHashRate = pHashAlgo->nHashRate;
    int64 nHashComputeCount = 0;
    int64 nHashComputeBeginTime = GetTime();

    Log("Proof-of-work: start hash compute, difficulty bits: (%d)", nBits);

    uint256 hashTarget = (~uint256(uint64(0)) >> nBits);
    while (!InterruptedPoW(block.hashPrev))
    {
        if (nHashRate == 0)
        {
            nHashRate = 1;
        }
        for (int i = 0; i < nHashRate; i++)
        {
            uint256 hash = pHashAlgo->Hash(vchProofOfWork);
            nHashComputeCount++;
            if (hash <= hashTarget)
            {
                block.nTimeStamp = nTime;
                proof.nNonce = nNonce;
                proof.Save(block.vchProof);

                int64 nDuration = GetTime() - nHashComputeBeginTime;
                int nCompHashRate = ((nDuration <= 0) ? 0 : (nHashComputeCount / nDuration));
                Log("Proof-of-work: block found (%s), compute: (rate:%ld, count:%ld, duration:%lds, hashrate:%ld), difficulty bits: (%d)\nhash :   %s\ntarget : %s",
                    pHashAlgo->strAlgo.c_str(), nHashRate, nHashComputeCount, nDuration, nCompHashRate, nBits,
                    hash.GetHex().c_str(), hashTarget.GetHex().c_str());
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
    Log("Proof-of-work: compute interrupted.");
    return false;
}

void CBlockMaker::BlockMakerThreadFunc()
{
    uint256 hashPrimaryBlock = uint64(0);
    int64 nPrimaryBlockTime = 0;
    int nPrimaryBlockHeight = 0;
    uint16 nPrimaryMintType = 0;

    while (!fExit)
    {
        while (WaitLastBlock(WAIT_NEWBLOCK_TIME, hashPrimaryBlock))
        {
        }

        {
            boost::unique_lock<boost::mutex> lock(mutex);

            currentStatus = lastStatus;
            hashPrimaryBlock = currentStatus.hashLastBlock;
            nPrimaryBlockTime = currentStatus.nLastBlockTime;
            nPrimaryBlockHeight = currentStatus.nLastBlockHeight;
            nPrimaryMintType = currentStatus.nMintType;

            StdTrace("blockmaker", "hashPrimaryBlock: %s, nPrimaryBlockTime: %ld, nPrimaryBlockHeight: %d, nPrimaryMintType: %u",
                     hashPrimaryBlock.ToString().c_str(), nPrimaryBlockTime, nPrimaryBlockHeight, nPrimaryMintType);
        }

        CDelegateAgreement agree;
        {
            int64 nWaitAgreement = nPrimaryBlockTime + WAIT_AGREEMENT_TIME - GetNetTime();
            if (nWaitAgreement <= 0)
            {
                nWaitAgreement = 1;
            }
            if (!WaitExit(nWaitAgreement))
            {
                break;
            }

            pConsensus->GetAgreement(nPrimaryBlockHeight + 1, agree.nAgreement, agree.nWeight, agree.vBallot);

            // log
            if (agree.IsProofOfWork())
            {
                Log("GetAgreement: height: %d, consensus: pow", nPrimaryBlockHeight + 1);
            }
            else
            {
                Log("GetAgreement: height: %d, consensus: dpos, ballot address: %s", nPrimaryBlockHeight + 1, CAddress(agree.vBallot[0]).ToString().c_str());
            }
        }

        try
        {

            if (agree.IsProofOfWork())
            {
                ProcessDelegatedProofOfWork(hashPrimaryBlock, nPrimaryBlockTime, nPrimaryBlockHeight, nPrimaryMintType, agree);
            }
            else
            {
                ProcessDelegatedProofOfStake(hashPrimaryBlock, nPrimaryBlockTime, nPrimaryBlockHeight, nPrimaryMintType, agree);
            }
        }
        catch (exception& e)
        {
            Error("Block maker error: %s\n", e.what());
            break;
        }
    }

    Log("Block maker exited");
}

} // namespace bigbang
