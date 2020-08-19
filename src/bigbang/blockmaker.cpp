// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockmaker.h"

#include <thread>

#include "address.h"
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
    templMint = CTemplateMint::CreateTemplatePtr(new CTemplateProof(keyMint.GetPubKey(), destMint));
    return (templMint != nullptr);
}

//////////////////////////////
// CBlockMaker

CBlockMaker::CBlockMaker()
  : thrPow("powmaker", boost::bind(&CBlockMaker::PowThreadFunc, this))
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
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

    if (!GetObject("service", pService))
    {
        Error("Failed to request service");
        return false;
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
    const char* ConsensusMethodName[CM_MAX] = { "cryptonight" };
    Log("Block maker started");
    for (map<int, CBlockMakerProfile>::iterator it = mapWorkProfile.begin(); it != mapWorkProfile.end(); ++it)
    {
        CBlockMakerProfile& profile = (*it).second;
        Log("Profile [%s] : dest=%s,pubkey=%s,pow=%s",
            ConsensusMethodName[(*it).first],
            CAddress(profile.destMint).ToString().c_str(),
            profile.keyMint.GetPubKey().GetHex().c_str(),
            CAddress(profile.templMint->GetTemplateId()).ToString().c_str());
    }
    return true;
}

void CBlockMaker::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pService = nullptr;

    mapWorkProfile.clear();
}

bool CBlockMaker::HandleInvoke()
{
    if (!IBlockMaker::HandleInvoke())
    {
        return false;
    }

    fExit = true;
    if (!mapWorkProfile.empty())
    {
        if (!ThreadDelayStart(thrPow))
        {
            return false;
        }
        fExit = false;
    }
    return true;
}

void CBlockMaker::HandleHalt()
{
    fExit = true;
    condExit.notify_all();

    thrPow.Interrupt();
    ThreadExit(thrPow);

    IBlockMaker::HandleHalt();
}

bool CBlockMaker::HandleEvent(CEventBlockMakerUpdate& eventUpdate)
{
    return true;
}

bool CBlockMaker::InterruptedPoW(const uint256& hashPrimary)
{
    boost::unique_lock<boost::mutex> lock(mutex);
    return fExit;
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
    int nAlgo = 0;
    uint32 nBits = 0;
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

    Log("Proof-of-work: start hash compute, target height: %d, difficulty bits: (0x%x)", nPrevBlockHeight + 1, nBits);

    uint256 hashTarget;
    hashTarget.SetCompact(nBits);
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

                Log("Proof-of-work: block found (%s), target height: %d, compute: (rate:%ld, count:%ld, duration:%lds, hashrate:%ld), difficulty bits: (0x%x)\nhash :   %s\ntarget : %s",
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
