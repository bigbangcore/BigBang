// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <thread>

#include "address.h"
#include "blockmaker.h"
#include "template/delegate.h"
#include "template/mint.h"
#include "template/proof.h"

using namespace std;
using namespace xengine;

#define INITIAL_HASH_RATE (8)
#define WAIT_AGREEMENT_TIME (BLOCK_TARGET_SPACING / 2)
#define WAIT_NEWBLOCK_TIME (BLOCK_TARGET_SPACING + 5)

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
    thrExtendedMaker("extendedmaker", boost::bind(&CBlockMaker::ExtendedMakerThreadFunc, this)),
    nMakerStatus(MAKER_HOLD), hashLastBlock(uint64(0)), nLastBlockTime(0),
    nLastBlockHeight(uint64(0)), nLastAgreement(uint64(0)), nLastWeight(0)
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

    if (!pBlockChain->GetLastBlock(pCoreProtocol->GetGenesisBlockHash(), hashLastBlock, nLastBlockHeight, nLastBlockTime))
    {
        return false;
    }

    if (!mapWorkProfile.empty() || !mapDelegatedProfile.empty())
    {
        nMakerStatus = MAKER_HOLD;

        if (!ThreadDelayStart(thrMaker))
        {
            return false;
        }
        if (!ThreadDelayStart(thrExtendedMaker))
        {
            return false;
        }
    }

    return true;
}

void CBlockMaker::HandleHalt()
{
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        nMakerStatus = MAKER_EXIT;
    }
    cond.notify_all();

    thrMaker.Interrupt();
    ThreadExit(thrMaker);

    thrExtendedMaker.Interrupt();
    ThreadExit(thrExtendedMaker);
    IBlockMaker::HandleHalt();
}

bool CBlockMaker::HandleEvent(CEventBlockMakerUpdate& eventUpdate)
{
    boost::unique_lock<boost::mutex> lock(mutex);

    //if (eventUpdate.data.nBlockHeight <= nLastBlockHeight)
    //{
    //    return true;
    //}

    if (Interrupted() || currentAgreement.IsProofOfWork() || (eventUpdate.data.nMintType == CTransaction::TX_STAKE))
    {
        nMakerStatus = MAKER_RESET;
        hashLastBlock = eventUpdate.data.hashBlock;
        nLastBlockTime = eventUpdate.data.nBlockTime;
        nLastBlockHeight = eventUpdate.data.nBlockHeight;
        nLastAgreement = eventUpdate.data.nAgreement;
        nLastWeight = eventUpdate.data.nWeight;
        cond.notify_all();
    }

    return true;
}

bool CBlockMaker::Wait(long nSeconds)
{
    boost::system_time const timeout = boost::get_system_time()
                                       + boost::posix_time::seconds(nSeconds);
    boost::unique_lock<boost::mutex> lock(mutex);
    while (nMakerStatus == MAKER_RUN)
    {
        if (!cond.timed_wait(lock, timeout))
        {
            break;
        }
    }
    return (nMakerStatus == MAKER_RUN);
}

bool CBlockMaker::Wait(long nSeconds, const uint256& hashPrimaryBlock)
{
    boost::system_time const timeout = boost::get_system_time()
                                       + boost::posix_time::seconds(nSeconds);
    boost::unique_lock<boost::mutex> lock(mutex);
    while (hashPrimaryBlock == hashLastBlock && nMakerStatus != MAKER_EXIT)
    {
        if (!cond.timed_wait(lock, timeout))
        {
            return (hashPrimaryBlock == hashLastBlock && nMakerStatus != MAKER_EXIT);
        }
    }
    return false;
}

void CBlockMaker::PrepareBlock(CBlock& block, const uint256& hashPrev, int64 nPrevTime, const int32 nPrevHeight, const CDelegateAgreement& agreement)
{
    block.SetNull();
    block.nType = CBlock::BLOCK_PRIMARY;
    block.nTimeStamp = nPrevTime + BLOCK_TARGET_SPACING;
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

    if (hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        size_t nMaxTxSize = MAX_BLOCK_SIZE - GetSerializeSize(block) - profile.GetSignatureSize();
        int64 nTotalTxFee = 0;
        pTxPool->ArrangeBlockTx(hashFork, block.GetBlockTime(), nMaxTxSize, block.vtx, nTotalTxFee);
        std::set<uint256> setTx;
        for (const auto& obj : block.vtx)
        {
            if (obj.nType == obj.TX_CERT)
            {
                setTx.insert(obj.GetHash());
            }
        }
        for (int i = 0; i < block.vtx.size(); ++i)
        {
            if (block.vtx[i].nType == block.vtx[i].TX_CERT)
            {
                if (setTx.count(block.vtx[i].vInput[0].prevout.hash))
                {
                    nTotalTxFee -= block.vtx[i].nTxFee;
                    block.vtx.erase(block.vtx.begin() + i);
                }
            }
        }
        block.hashMerkle = block.CalcMerkleTreeRoot();
        block.txMint.nAmount += nTotalTxFee;
    }
    else
    {
        size_t nMaxTxSize = MAX_BLOCK_SIZE - GetSerializeSize(block) - profile.GetSignatureSize();
        int64 nTotalTxFee = 0;
        pTxPool->ArrangeBlockTx(hashFork, block.GetBlockTime(), nMaxTxSize, block.vtx, nTotalTxFee);
        block.hashMerkle = block.CalcMerkleTreeRoot();
        block.txMint.nAmount += nTotalTxFee;
    }
}

bool CBlockMaker::SignBlock(CBlock& block, const CBlockMakerProfile& profile)
{
    uint256 hashSig = block.GetHash();
    vector<unsigned char> vchMintSig;
    if (!profile.keyMint.Sign(hashSig, vchMintSig))
    {
        return false;
    }
    return profile.templMint->BuildBlockSignature(hashSig, vchMintSig, block.vchSig);
}

bool CBlockMaker::DispatchBlock(CBlock& block)
{
    int nWait = block.nTimeStamp - GetNetTime();
    if (nWait > 0 && !Wait(nWait))
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

bool CBlockMaker::CreateProofOfWorkBlock(CBlock& block)
{
    int nConsensus = CM_CRYPTONIGHT;
    map<int, CBlockMakerProfile>::iterator it = mapWorkProfile.find(nConsensus);
    if (it == mapWorkProfile.end())
    {
        return false;
    }

    CBlockMakerProfile& profile = (*it).second;
    CDestination destSendTo = profile.GetDestination();

    int nAlgo = nConsensus;
    int nBits;
    int64 nReward;
    if (!pBlockChain->GetProofOfWorkTarget(block.hashPrev, nAlgo, nBits, nReward))
    {
        return false;
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
        return false;
    }

    txMint.nTimeStamp = block.nTimeStamp;

    ArrangeBlockTx(block, pCoreProtocol->GetGenesisBlockHash(), profile);

    return SignBlock(block, profile);
}

void CBlockMaker::ProcessDelegatedProofOfStake(CBlock& block, const CDelegateAgreement& agreement, const int32 nPrevHeight)
{
    map<CDestination, CBlockMakerProfile>::iterator it = mapDelegatedProfile.find(agreement.vBallot[0]);
    if (it != mapDelegatedProfile.end())
    {
        CBlockMakerProfile& profile = (*it).second;
        if (CreateDelegatedBlock(block, pCoreProtocol->GetGenesisBlockHash(), profile, agreement.nWeight))
        {
            if (DispatchBlock(block))
            {
                CreatePiggyback(profile, agreement, block.GetHash(), block.GetBlockTime(), nPrevHeight);
            }
        }
    }
}

void CBlockMaker::ProcessExtended(const CDelegateAgreement& agreement,
                                  const uint256& hashPrimaryBlock, int64 nPrimaryBlockTime, const int32 nPrimaryBlockHeight)
{
    vector<CBlockMakerProfile*> vProfile;
    set<uint256> setFork;

    if (!GetAvailiableDelegatedProfile(agreement.vBallot, vProfile) || !GetAvailiableExtendedFork(setFork))
    {
        return;
    }

    int64 nTime = nPrimaryBlockTime + EXTENDED_BLOCK_SPACING * ((GetNetTime() - nPrimaryBlockTime + (EXTENDED_BLOCK_SPACING - 1)) / EXTENDED_BLOCK_SPACING);
    if (nTime < nPrimaryBlockTime + EXTENDED_BLOCK_SPACING)
    {
        nTime = nPrimaryBlockTime + EXTENDED_BLOCK_SPACING;
    }
    while (nTime - nPrimaryBlockTime < BLOCK_TARGET_SPACING)
    {
        int nIndex = (nTime - nPrimaryBlockTime) / EXTENDED_BLOCK_SPACING;
        const CBlockMakerProfile* pProfile = vProfile[nIndex % vProfile.size()];
        if (pProfile != nullptr)
        {
            if (!Wait(nTime - GetNetTime(), hashPrimaryBlock))
            {
                return;
            }

            CreateExtended(*pProfile, agreement, hashPrimaryBlock, setFork, nPrimaryBlockHeight, nTime);
        }
        nTime += EXTENDED_BLOCK_SPACING;
    }
}

bool CBlockMaker::CreateDelegatedBlock(CBlock& block, const uint256& hashFork, const CBlockMakerProfile& profile, size_t nWeight)
{
    CDestination destSendTo = profile.GetDestination();

    int64 nReward;
    if (!pBlockChain->GetBlockMintReward(block.hashPrev, nReward))
    {
        return false;
    }

    CTransaction& txMint = block.txMint;
    txMint.nType = CTransaction::TX_STAKE;
    txMint.nTimeStamp = block.nTimeStamp;
    txMint.hashAnchor = block.hashPrev;
    txMint.sendTo = destSendTo;
    txMint.nAmount = nReward;

    ArrangeBlockTx(block, hashFork, profile);

    return SignBlock(block, profile);
}

void CBlockMaker::CreatePiggyback(const CBlockMakerProfile& profile, const CDelegateAgreement& agreement,
                                  const uint256& hashRefBlock, int64 nRefBlockTime, const int32 nPrevHeight)
{
    CProofOfPiggyback proof;
    proof.nWeight = agreement.nWeight;
    proof.nAgreement = agreement.nAgreement;
    proof.hashRefBlock = hashRefBlock;

    map<uint256, CForkStatus> mapForkStatus;
    pBlockChain->GetForkStatus(mapForkStatus);
    for (map<uint256, CForkStatus>::iterator it = mapForkStatus.begin(); it != mapForkStatus.end(); ++it)
    {
        const uint256& hashFork = (*it).first;
        CForkStatus& status = (*it).second;
        if (hashFork != pCoreProtocol->GetGenesisBlockHash()
            && status.nLastBlockHeight == nPrevHeight
            && status.nLastBlockTime < nRefBlockTime)
        {
            CBlock block;
            block.nType = CBlock::BLOCK_SUBSIDIARY;
            block.nTimeStamp = nRefBlockTime;
            block.hashPrev = status.hashLastBlock;
            proof.Save(block.vchProof);

            if (CreateDelegatedBlock(block, hashFork, profile, agreement.nWeight))
            {
                DispatchBlock(block);
            }
        }
    }
}

void CBlockMaker::CreateExtended(const CBlockMakerProfile& profile, const CDelegateAgreement& agreement,
                                 const uint256& hashRefBlock, const set<uint256>& setFork, const int32 nPrimaryBlockHeight, int64 nTime)
{
    CProofOfPiggyback proof;
    proof.nWeight = agreement.nWeight;
    proof.nAgreement = agreement.nAgreement;
    proof.hashRefBlock = hashRefBlock;
    for (const uint256& hashFork : setFork)
    {
        uint256 hashLastBlock;
        int nLastBlockHeight;
        int64 nLastBlockTime;
        if (pTxPool->Count(hashFork)
            && pBlockChain->GetLastBlock(hashFork, hashLastBlock, nLastBlockHeight, nLastBlockTime)
            && nPrimaryBlockHeight == nLastBlockHeight
            && nLastBlockTime < nTime)
        {
            CBlock block;
            block.nType = CBlock::BLOCK_EXTENDED;
            block.nTimeStamp = nTime;
            block.hashPrev = hashLastBlock;
            proof.Save(block.vchProof);

            CTransaction& txMint = block.txMint;
            txMint.nType = CTransaction::TX_STAKE;
            txMint.nTimeStamp = block.nTimeStamp;
            txMint.hashAnchor = hashLastBlock;
            txMint.sendTo = profile.GetDestination();
            txMint.nAmount = 0;

            ArrangeBlockTx(block, hashFork, profile);
            if (!block.vtx.empty() && SignBlock(block, profile))
            {
                DispatchBlock(block);
            }
        }
    }
}

bool CBlockMaker::CreateProofOfWork(CBlock& block, CBlockMakerHashAlgo* pHashAlgo)
{
    const int64 nTimePrev = block.nTimeStamp - BLOCK_TARGET_SPACING;
    block.nTimeStamp -= 25;
    if (GetNetTime() > block.nTimeStamp)
    {
        block.nTimeStamp = GetNetTime();
    }

    CProofOfHashWorkCompact proof;
    proof.Load(block.vchProof);

    int nBits = proof.nBits;
    Log("difficulty : (%d)", nBits);
    vector<unsigned char> vchProofOfWork;
    block.GetSerializedProofOfWorkData(vchProofOfWork);

    uint32& nTime = *((uint32*)&vchProofOfWork[4]);
    uint256& nNonce = *((uint256*)&vchProofOfWork[vchProofOfWork.size() - 32]);

    int64& nHashRate = pHashAlgo->nHashRate;

    while (!Interrupted())
    {
        uint256 hashTarget = (~uint256(uint64(0)) >> pCoreProtocol->GetProofOfWorkRunTimeBits(nBits, nTime, nTimePrev));
        if (nHashRate == 0)
            nHashRate = 1;
        for (int i = 0; i < nHashRate; i++)
        {
            uint256 hash = pHashAlgo->Hash(vchProofOfWork);
            if (hash <= hashTarget)
            {
                block.nTimeStamp = nTime;
                proof.nNonce = nNonce;
                proof.Save(block.vchProof);

                Log("Proof-of-work(%s) block found (%ld)\nhash : %s\ntarget : %s\n",
                    pHashAlgo->strAlgo.c_str(), nHashRate, hash.GetHex().c_str(), hashTarget.GetHex().c_str());
                return true;
            }
            nNonce += 256;
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
    return false;
}

bool CBlockMaker::GetAvailiableDelegatedProfile(const vector<CDestination>& vBallot, vector<CBlockMakerProfile*>& vProfile)
{
    int nAvailProfile = 0;
    vProfile.reserve(vBallot.size());
    for (const CDestination& dest : vBallot)
    {
        map<CDestination, CBlockMakerProfile>::iterator it = mapDelegatedProfile.find(dest);
        if (it != mapDelegatedProfile.end())
        {
            vProfile.push_back(&(*it).second);
            ++nAvailProfile;
        }
        else
        {
            vProfile.push_back((CBlockMakerProfile*)nullptr);
        }
    }

    return (!!nAvailProfile);
}

bool CBlockMaker::GetAvailiableExtendedFork(set<uint256>& setFork)
{
    map<uint256, CForkStatus> mapForkStatus;
    pBlockChain->GetForkStatus(mapForkStatus);
    for (map<uint256, CForkStatus>::iterator it = mapForkStatus.begin(); it != mapForkStatus.end(); ++it)
    {
        CProfile profile;
        const uint256& hashFork = (*it).first;
        if (hashFork != pCoreProtocol->GetGenesisBlockHash()
            && pForkManager->IsAllowed(hashFork)
            && pBlockChain->GetForkProfile(hashFork, profile) && !profile.IsEnclosed())
        {
            setFork.insert(hashFork);
        }
    }
    return (!setFork.empty());
}

void CBlockMaker::BlockMakerThreadFunc()
{
    const char* ConsensusMethodName[CM_MAX] = { "mpvss", "cryptonight" };
    Log("Block maker started\n");
    for (map<int, CBlockMakerProfile>::iterator it = mapWorkProfile.begin(); it != mapWorkProfile.end(); ++it)
    {
        CBlockMakerProfile& profile = (*it).second;
        Log("Profile [%s] : dest=%s,pubkey=%s\n",
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
    uint256 hashPrimaryBlock = uint64(0);
    int64 nPrimaryBlockTime = 0;
    int nPrimaryBlockHeight = 0;

    {
        boost::unique_lock<boost::mutex> lock(mutex);
        hashPrimaryBlock = hashLastBlock;
        nPrimaryBlockTime = nLastBlockTime;
        nPrimaryBlockHeight = nLastBlockHeight;
    }

    for (;;)
    {
        CDelegateAgreement agree;
        {
            boost::unique_lock<boost::mutex> lock(mutex);

            int64 nWaitBlockTime = nPrimaryBlockTime + WAIT_NEWBLOCK_TIME - GetNetTime();
            if (nWaitBlockTime <= 0)
            {
                nWaitBlockTime = 1;
            }
            boost::system_time const toWaitBlock = boost::get_system_time() + boost::posix_time::seconds(nWaitBlockTime);

            while (hashPrimaryBlock == hashLastBlock && nMakerStatus == MAKER_HOLD)
            {
                if (!cond.timed_wait(lock, toWaitBlock))
                {
                    break;
                }
            }

            if (nMakerStatus == MAKER_EXIT)
            {
                break;
            }

            if (hashPrimaryBlock != hashLastBlock)
            {
                hashPrimaryBlock = hashLastBlock;
                nPrimaryBlockTime = nLastBlockTime;
                nPrimaryBlockHeight = nLastBlockHeight;
                int64 nWaitAgreement = nPrimaryBlockTime + WAIT_AGREEMENT_TIME - GetNetTime();
                if (nWaitAgreement <= 0)
                {
                    nWaitAgreement = 1;
                }
                boost::system_time const toWaitAgree = boost::get_system_time() + boost::posix_time::seconds(nWaitAgreement);
                while (hashPrimaryBlock == hashLastBlock && nMakerStatus != MAKER_EXIT)
                {
                    if (!cond.timed_wait(lock, toWaitAgree))
                    {
                        pConsensus->GetAgreement(nLastBlockHeight + 1, agree.nAgreement, agree.nWeight, agree.vBallot);
                        currentAgreement = agree;
                        Log("GetAgreement : %s at height=%d, weight=%lu, consensus: %s.", agree.nAgreement.GetHex().c_str(),
                            nLastBlockHeight + 1, agree.nWeight,
                            agree.IsProofOfWork() ? "pow" : "dpos");
                        break;
                    }
                }
                if (nMakerStatus == MAKER_EXIT)
                {
                    break;
                }
                if (hashPrimaryBlock != hashLastBlock)
                {
                    continue;
                }
            }
            nMakerStatus = MAKER_RUN;
        }

        CBlock block;
        try
        {
            int nNextStatus = MAKER_HOLD;
            PrepareBlock(block, hashPrimaryBlock, nPrimaryBlockTime, nPrimaryBlockHeight, agree);

            if (agree.IsProofOfWork())
            {
                if (CreateProofOfWorkBlock(block))
                {
                    if (Interrupted() || !DispatchBlock(block))
                    {
                        nNextStatus = MAKER_RESET;
                    }
                }
            }
            else
            {
                ProcessDelegatedProofOfStake(block, agree, nPrimaryBlockHeight);
            }

            {
                boost::unique_lock<boost::mutex> lock(mutex);
                if (nMakerStatus == MAKER_RUN)
                {
                    nMakerStatus = nNextStatus;
                }
            }
        }
        catch (exception& e)
        {
            Error("Block maker error: %s\n", e.what());
            break;
        }
    }

    Log("Block maker exited\n");
}

void CBlockMaker::ExtendedMakerThreadFunc()
{
    uint256 hashPrimaryBlock = uint64(0);
    int64 nPrimaryBlockTime = 0;
    int nPrimaryBlockHeight = 0;

    {
        boost::unique_lock<boost::mutex> lock(mutex);
        hashPrimaryBlock = hashLastBlock;
    }

    Log("Extened block maker started, initial primary block hash = %s\n", hashPrimaryBlock.GetHex().c_str());

    for (;;)
    {
        CDelegateAgreement agree;
        {
            boost::unique_lock<boost::mutex> lock(mutex);

            while (hashPrimaryBlock == hashLastBlock && nMakerStatus != MAKER_EXIT)
            {
                cond.wait(lock);
            }
            if (nMakerStatus == MAKER_EXIT)
            {
                break;
            }

            if (currentAgreement.IsProofOfWork()
                || currentAgreement.nAgreement != nLastAgreement
                || currentAgreement.nWeight != nLastWeight)
            {
                hashPrimaryBlock = hashLastBlock;
                continue;
            }

            agree = currentAgreement;
            hashPrimaryBlock = hashLastBlock;
            nPrimaryBlockTime = nLastBlockTime;
            nPrimaryBlockHeight = nLastBlockHeight;
        }

        try
        {
            ProcessExtended(agree, hashPrimaryBlock, nPrimaryBlockTime, nPrimaryBlockHeight);
        }
        catch (exception& e)
        {
            Error("Extended block maker error: %s\n", e.what());
            break;
        }
    }
    Log("Extended block maker exited\n");
}

} // namespace bigbang
