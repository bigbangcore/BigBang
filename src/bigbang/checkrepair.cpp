// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "checkrepair.h"

#include "param.h"

using namespace std;
using namespace xengine;
using namespace boost::filesystem;

#define BLOCKFILE_PREFIX "block"

namespace bigbang
{

/////////////////////////////////////////////////////////////////////////
// CCheckForkUnspentWalker

bool CCheckForkUnspentWalker::Walk(const CTxOutPoint& txout, const CTxOut& output)
{
    if (!output.IsNull())
    {
        if (!mapForkUnspent.insert(make_pair(txout, CCheckTxOut(output))).second)
        {
            StdError("check", "Insert leveldb unspent fail, unspent: [%d] %s.", txout.n, txout.hash.GetHex().c_str());
            return false;
        }
    }
    else
    {
        StdLog("check", "Leveldb unspent is NULL, unspent: [%d] %s.", txout.n, txout.hash.GetHex().c_str());
    }
    return true;
}

bool CCheckForkUnspentWalker::CheckForkUnspent(map<CTxOutPoint, CCheckTxOut>& mapBlockForkUnspent)
{
    {
        map<CTxOutPoint, CCheckTxOut>::iterator it = mapBlockForkUnspent.begin();
        for (; it != mapBlockForkUnspent.end(); ++it)
        {
            map<CTxOutPoint, CCheckTxOut>::iterator mt = mapForkUnspent.find(it->first);
            if (mt == mapForkUnspent.end())
            {
                StdLog("check", "Check block unspent: find utxo fail, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                mapForkUnspent.insert(*it);
                vAddUpdate.push_back(CTxUnspent(it->first, static_cast<const CTxOut&>(it->second)));
            }
            else if (it->second != mt->second)
            {
                StdLog("check", "Check block unspent: txout error, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                mt->second = it->second;
                vAddUpdate.push_back(CTxUnspent(it->first, static_cast<const CTxOut&>(it->second)));
            }
        }
    }

    {
        map<CTxOutPoint, CCheckTxOut>::iterator it = mapForkUnspent.begin();
        for (; it != mapForkUnspent.end();)
        {
            map<CTxOutPoint, CCheckTxOut>::iterator mt = mapBlockForkUnspent.find(it->first);
            if (mt == mapBlockForkUnspent.end())
            {
                StdLog("check", "Check db unspent: find utxo fail, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                vRemove.push_back(it->first);
                mapForkUnspent.erase(it++);
                continue;
            }
            else if (it->second != mt->second)
            {
                StdLog("check", "Check db unspent: txout error, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                it->second = mt->second;
                vAddUpdate.push_back(CTxUnspent(it->first, static_cast<const CTxOut&>(it->second)));
            }
            ++it;
        }
    }
    return !(vAddUpdate.size() > 0 || vRemove.size() > 0);
}

/////////////////////////////////////////////////////////////////////////
// CCheckForkManager

CCheckForkManager::~CCheckForkManager()
{
    dbFork.Deinitialize();
}

bool CCheckForkManager::SetParam(const string& strDataPathIn, bool fTestnetIn, bool fOnlyCheckIn, const uint256& hashGenesisBlockIn)
{
    strDataPath = strDataPathIn;
    fTestnet = fTestnetIn;
    fOnlyCheck = fOnlyCheckIn;
    hashGenesisBlock = hashGenesisBlockIn;

#ifdef BIGBANG_TESTNET
    mapCheckPoints.insert(make_pair(0, hashGenesisBlockIn));
#else
    for (const auto& vd : vCheckPoints)
    {
        mapCheckPoints.insert(make_pair(vd.first, vd.second));
    }
#endif

    if (!dbFork.Initialize(path(strDataPath)))
    {
        StdError("check", "Fork manager set param: Initialize fail");
        return false;
    }
    return true;
}

bool CCheckForkManager::FetchForkStatus()
{
    uint256 hashGetGenesisBlock;
    if (!dbFork.GetGenesisBlockHash(hashGetGenesisBlock) || hashGetGenesisBlock != hashGenesisBlock)
    {
        StdError("check", "Fetch fork status: Get genesis block hash fail");
        if (fOnlyCheck)
        {
            return false;
        }
        if (!dbFork.WriteGenesisBlockHash(hashGenesisBlock))
        {
            StdError("check", "Fetch fork status: Write genesis block hash fail");
            return false;
        }
    }

    bool fCheckGenesisRet = true;
    uint256 hashRefFdBlock;
    map<uint256, int> mapValidFork;
    if (!dbFork.RetrieveValidForkHash(hashGenesisBlock, hashRefFdBlock, mapValidFork))
    {
        fCheckGenesisRet = false;
    }
    else
    {
        if (hashRefFdBlock != 0)
        {
            fCheckGenesisRet = false;
        }
        else
        {
            const auto it = mapValidFork.find(hashGenesisBlock);
            if (it == mapValidFork.end() || it->second != 0)
            {
                fCheckGenesisRet = false;
            }
        }
    }
    if (!fCheckGenesisRet)
    {
        StdError("check", "Fetch fork status: Check GenesisBlock valid fork fail");
        if (fOnlyCheck)
        {
            return false;
        }
        mapValidFork.insert(make_pair(hashGenesisBlock, 0));
        if (!dbFork.AddValidForkHash(hashGenesisBlock, uint256(), mapValidFork))
        {
            StdError("check", "Fetch fork status: Add valid fork fail");
            return false;
        }
    }

    vector<pair<uint256, uint256>> vFork;
    if (!dbFork.ListFork(vFork))
    {
        StdError("check", "Fetch fork status: ListFork fail");
        return false;
    }
    StdLog("check", "Fetch fork status: fork size: %lu", vFork.size());

    for (const auto& fork : vFork)
    {
        StdLog("check", "Fetch fork status: fork: %s", fork.first.GetHex().c_str());
        const uint256 hashFork = fork.first;
        CCheckForkStatus& status = mapForkStatus[hashFork];
        status.hashLastBlock = fork.second;

        if (!dbFork.RetrieveForkContext(hashFork, status.ctxt))
        {
            StdError("check", "Fetch fork status: RetrieveForkContext fail");
            return false;
        }

        if (status.ctxt.hashParent != 0)
        {
            mapForkStatus[status.ctxt.hashParent].InsertSubline(status.ctxt.nJointHeight, hashFork);
        }
    }

    return true;
}

void CCheckForkManager::GetForkList(vector<uint256>& vForkList)
{
    set<uint256> setFork;

    vForkList.clear();
    vForkList.push_back(hashGenesisBlock);
    setFork.insert(hashGenesisBlock);

    for (int i = 0; i < vForkList.size(); i++)
    {
        map<uint256, CCheckForkStatus>::iterator it = mapForkStatus.find(vForkList[i]);
        if (it != mapForkStatus.end())
        {
            for (auto mt = it->second.mapSubline.begin(); mt != it->second.mapSubline.end(); ++mt)
            {
                if (setFork.count(mt->second) == 0 && mapForkStatus.count(mt->second) > 0)
                {
                    vForkList.push_back(mt->second);
                    setFork.insert(mt->second);
                }
            }
        }
        else
        {
            StdLog("check", "Fork manager get fork list: find fork fail, fork: %s", vForkList[i].GetHex().c_str());
        }
    }
}

void CCheckForkManager::GetTxFork(const uint256& hashFork, int nHeight, vector<uint256>& vFork)
{
    vector<pair<uint256, CCheckForkStatus*>> vForkPtr;
    {
        map<uint256, CCheckForkStatus>::iterator it = mapForkStatus.find(hashFork);
        if (it != mapForkStatus.end())
        {
            vForkPtr.push_back(make_pair(hashFork, &(*it).second));
        }
        else
        {
            StdLog("check", "GetTxFork: find fork fail, fork: %s", hashFork.GetHex().c_str());
        }
    }
    if (nHeight >= 0)
    {
        for (size_t i = 0; i < vForkPtr.size(); i++)
        {
            CCheckForkStatus* pFork = vForkPtr[i].second;
            for (multimap<int, uint256>::iterator mi = pFork->mapSubline.lower_bound(nHeight); mi != pFork->mapSubline.end(); ++mi)
            {
                map<uint256, CCheckForkStatus>::iterator it = mapForkStatus.find((*mi).second);
                if (it != mapForkStatus.end() && !(*it).second.ctxt.IsIsolated())
                {
                    vForkPtr.push_back(make_pair((*it).first, &(*it).second));
                }
            }
        }
    }
    vFork.reserve(vForkPtr.size());
    for (size_t i = 0; i < vForkPtr.size(); i++)
    {
        vFork.push_back(vForkPtr[i].first);
    }
}

bool CCheckForkManager::AddBlockForkContext(const CBlockEx& blockex)
{
    uint256 hashBlock = blockex.GetHash();
    if (hashBlock == hashGenesisBlock)
    {
        uint256 hashRefFdBlock;
        map<uint256, int> mapValidFork;
        vector<CForkContext> vForkCtxt;

        CProfile profile;
        if (!profile.Load(blockex.vchProof))
        {
            StdTrace("check", "Load genesis %s block Proof failed", hashGenesisBlock.ToString().c_str());
            return false;
        }
        vForkCtxt.push_back(CForkContext(hashGenesisBlock, uint64(0), uint64(0), profile));

        mapValidFork.insert(make_pair(hashGenesisBlock, 0));
        if (!AddForkContext(uint256(), hashGenesisBlock, vForkCtxt, true, hashRefFdBlock, mapValidFork))
        {
            StdLog("check", "AddBlockForkContext: AddForkContext fail, block: %s", hashBlock.ToString().c_str());
            return false;
        }
        return true;
    }

    vector<CForkContext> vForkCtxt;
    for (int i = 0; i < blockex.vtx.size(); i++)
    {
        const CTransaction& tx = blockex.vtx[i];
        const CTxContxt& txContxt = blockex.vTxContxt[i];
        if (tx.sendTo != txContxt.destIn)
        {
            if (tx.sendTo.GetTemplateId().GetType() == TEMPLATE_FORK)
            {
                if (!VerifyBlockForkTx(blockex.hashPrev, tx, vForkCtxt))
                {
                    StdLog("check", "AddBlockForkContext: VerifyBlockForkTx fail, block: %s", hashBlock.ToString().c_str());
                    return false;
                }
            }
            if (txContxt.destIn.GetTemplateId().GetType() == TEMPLATE_FORK)
            {
                CDestination destRedeem;
                uint256 hashFork;
                if (!GetTxForkRedeemParam(tx, txContxt.destIn, destRedeem, hashFork))
                {
                    StdLog("check", "AddBlockForkContext: Get redeem param fail, block: %s, dest: %s",
                           hashBlock.ToString().c_str(), CAddress(txContxt.destIn).ToString().c_str());
                    return false;
                }
                auto it = vForkCtxt.begin();
                while (it != vForkCtxt.end())
                {
                    if (it->hashFork == hashFork)
                    {
                        StdLog("check", "AddBlockForkContext: cancel fork, block: %s, fork: %s, dest: %s",
                               hashBlock.ToString().c_str(), it->hashFork.ToString().c_str(),
                               CAddress(txContxt.destIn).ToString().c_str());
                        vForkCtxt.erase(it++);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
        }
    }

    bool fCheckPointBlock = false;
    const auto it = mapCheckPoints.find(CBlock::GetBlockHeightByHash(hashBlock));
    if (it != mapCheckPoints.end() && it->second == hashBlock)
    {
        fCheckPointBlock = true;
    }

    uint256 hashRefFdBlock;
    map<uint256, int> mapValidFork;
    if (!AddForkContext(blockex.hashPrev, hashBlock, vForkCtxt, fCheckPointBlock, hashRefFdBlock, mapValidFork))
    {
        StdLog("check", "AddBlockForkContext: AddForkContext fail, block: %s", hashBlock.ToString().c_str());
        return false;
    }

    if (!CheckDbValidFork(hashBlock, hashRefFdBlock, mapValidFork))
    {
        StdLog("check", "AddBlockForkContext: Check db valid fork fail, block: %s", hashBlock.ToString().c_str());
        if (fOnlyCheck)
        {
            return false;
        }
        if (!AddDbValidForkHash(hashBlock, hashRefFdBlock, mapValidFork))
        {
            StdLog("check", "AddBlockForkContext: Add db valid fork fail, block: %s", hashBlock.ToString().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckForkManager::VerifyBlockForkTx(const uint256& hashPrev, const CTransaction& tx, vector<CForkContext>& vForkCtxt)
{
    if (tx.vchData.empty())
    {
        StdLog("check", "Verify block fork tx: invalid vchData, tx: %s", tx.GetHash().ToString().c_str());
        return false;
    }

    CBlock block;
    CProfile profile;
    try
    {
        CBufStream ss;
        ss.Write((const char*)&tx.vchData[0], tx.vchData.size());
        ss >> block;
        if (!block.IsOrigin() || block.IsPrimary())
        {
            StdLog("check", "Verify block fork tx: invalid block, tx: %s", tx.GetHash().ToString().c_str());
            return false;
        }
        if (!profile.Load(block.vchProof))
        {
            StdLog("check", "Verify block fork tx: invalid profile, tx: %s", tx.GetHash().ToString().c_str());
            return false;
        }
    }
    catch (...)
    {
        StdLog("check", "Verify block fork tx: invalid vchData, tx: %s", tx.GetHash().ToString().c_str());
        return false;
    }
    uint256 hashNewFork = block.GetHash();

    do
    {
        CForkContext ctxtParent;
        if (!GetForkContext(profile.hashParent, ctxtParent))
        {
            bool fFindParent = false;
            for (const auto& vd : vForkCtxt)
            {
                if (vd.hashFork == profile.hashParent)
                {
                    ctxtParent = vd;
                    fFindParent = true;
                    break;
                }
            }
            if (!fFindParent)
            {
                StdLog("check", "Verify block fork tx: Retrieve parent context, tx: %s", tx.GetHash().ToString().c_str());
                break;
            }
        }

        CProfile forkProfile;
        if (!ValidateOrigin(block, ctxtParent.GetProfile(), forkProfile))
        {
            StdLog("check", "Verify block fork tx: Validate origin, tx: %s", tx.GetHash().ToString().c_str());
            break;
        }

        if (!VerifyValidFork(hashPrev, hashNewFork, profile.strName))
        {
            StdLog("check", "Verify block fork tx: verify fork fail, tx: %s", tx.GetHash().ToString().c_str());
            break;
        }
        bool fCheckRet = true;
        for (const auto& vd : vForkCtxt)
        {
            if (vd.hashFork == hashNewFork || vd.strName == profile.strName)
            {
                StdLog("check", "Verify block fork tx: fork exist, tx: %s", tx.GetHash().ToString().c_str());
                fCheckRet = false;
                break;
            }
        }
        if (!fCheckRet)
        {
            break;
        }

        vForkCtxt.push_back(CForkContext(block.GetHash(), block.hashPrev, tx.GetHash(), profile));
    } while (0);

    return true;
}

bool CCheckForkManager::GetTxForkRedeemParam(const CTransaction& tx, const CDestination& destIn, CDestination& destRedeem, uint256& hashFork)
{
    if (destIn.GetTemplateId().GetType() != TEMPLATE_FORK)
    {
        StdError("check", "Get fork redeem param: Template type error, type: %d, tx: %s",
                 destIn.GetTemplateId().GetType(), tx.GetHash().GetHex().c_str());
        return false;
    }
    vector<uint8> vchSig;
    if (CTemplate::IsDestInRecorded(tx.sendTo))
    {
        CDestination sendToDelegateTemplate;
        CDestination sendToOwner;
        if (!CSendToRecordedTemplate::ParseDest(tx.vchSig, sendToDelegateTemplate, sendToOwner, vchSig))
        {
            StdError("check", "Get fork redeem param: ParseDest fail, tx: %s", tx.GetHash().GetHex().c_str());
            return false;
        }
    }
    else
    {
        vchSig = tx.vchSig;
    }
    auto templatePtr = CTemplate::CreateTemplatePtr(TEMPLATE_FORK, vchSig);
    if (templatePtr == nullptr || templatePtr->GetTemplateId() != destIn.GetTemplateId())
    {
        StdError("check", "Get fork redeem param: CreateTemplatePtr fail, tx: %s", tx.GetHash().GetHex().c_str());
        return false;
    }
    boost::dynamic_pointer_cast<CTemplateFork>(templatePtr)->GetForkParam(destRedeem, hashFork);
    return true;
}

bool CCheckForkManager::AddForkContext(const uint256& hashPrevBlock, const uint256& hashNewBlock, const vector<CForkContext>& vForkCtxt,
                                       bool fCheckPointBlock, uint256& hashRefFdBlock, map<uint256, int>& mapValidFork)
{
    CCheckValidFdForkId& fd = mapBlockValidFork[hashNewBlock];
    if (fCheckPointBlock)
    {
        fd.mapForkId.clear();
        if (hashPrevBlock != 0)
        {
            if (!GetValidFdForkId(hashPrevBlock, fd.mapForkId))
            {
                StdError("check", "Add fork context: Get Valid forkid fail, prev: %s", hashPrevBlock.GetHex().c_str());
                mapBlockValidFork.erase(hashNewBlock);
                return false;
            }
        }
        fd.hashRefFdBlock = uint256();
    }
    else
    {
        const auto it = mapBlockValidFork.find(hashPrevBlock);
        if (it == mapBlockValidFork.end())
        {
            StdError("check", "Add fork context: Find Valid forkid fail, prev: %s", hashPrevBlock.GetHex().c_str());
            mapBlockValidFork.erase(hashNewBlock);
            return false;
        }
        const CCheckValidFdForkId& prevfd = it->second;
        fd.mapForkId.clear();
        if (prevfd.hashRefFdBlock == 0)
        {
            fd.hashRefFdBlock = hashPrevBlock;
        }
        else
        {
            fd.mapForkId.insert(prevfd.mapForkId.begin(), prevfd.mapForkId.end());
            fd.hashRefFdBlock = prevfd.hashRefFdBlock;
        }
    }
    for (const CForkContext& ctxt : vForkCtxt)
    {
        mapForkSched[ctxt.hashFork].ctxtFork = ctxt;
        if (ctxt.hashParent != 0)
        {
            mapForkSched[ctxt.hashParent].AddNewJoint(ctxt.hashJoint, ctxt.hashFork);
        }
        fd.mapForkId.insert(make_pair(ctxt.hashFork, CBlock::GetBlockHeightByHash(hashNewBlock)));
    }
    hashRefFdBlock = fd.hashRefFdBlock;
    mapValidFork.clear();
    mapValidFork.insert(fd.mapForkId.begin(), fd.mapForkId.end());
    return true;
}

bool CCheckForkManager::GetForkContext(const uint256& hashFork, CForkContext& ctxt)
{
    const auto it = mapForkSched.find(hashFork);
    if (it != mapForkSched.end())
    {
        ctxt = it->second.ctxtFork;
        return true;
    }
    return false;
}

bool CCheckForkManager::ValidateOrigin(const CBlock& block, const CProfile& parentProfile, CProfile& forkProfile)
{
    if (!forkProfile.Load(block.vchProof))
    {
        StdLog("check", "Validate origin: load profile error");
        return false;
    }
    if (forkProfile.IsNull())
    {
        StdLog("check", "Validate origin: invalid profile");
        return false;
    }
    if (!MoneyRange(forkProfile.nAmount))
    {
        StdLog("check", "Validate origin: invalid fork amount");
        return false;
    }
    if (!RewardRange(forkProfile.nMintReward))
    {
        StdLog("check", "Validate origin: invalid fork reward");
        return false;
    }
    if (block.txMint.sendTo != forkProfile.destOwner)
    {
        StdLog("check", "Validate origin: invalid fork sendTo");
        return false;
    }
    if (parentProfile.IsPrivate())
    {
        if (!forkProfile.IsPrivate() || parentProfile.destOwner != forkProfile.destOwner)
        {
            StdLog("check", "Validate origin: permission denied");
            return false;
        }
    }
    return true;
}

bool CCheckForkManager::VerifyValidFork(const uint256& hashPrevBlock, const uint256& hashFork, const string& strForkName)
{
    map<uint256, int> mapValidFork;
    if (GetValidFdForkId(hashPrevBlock, mapValidFork))
    {
        if (mapValidFork.count(hashFork) > 0)
        {
            StdLog("check", "Verify valid fork: Fork existed, fork: %s", hashFork.GetHex().c_str());
            return false;
        }
        for (const auto& vd : mapValidFork)
        {
            const auto mt = mapForkSched.find(vd.first);
            if (mt != mapForkSched.end() && mt->second.ctxtFork.strName == strForkName)
            {
                StdLog("check", "Verify valid fork: Fork name repeated, new fork: %s, valid fork: %s, name: %s",
                       hashFork.GetHex().c_str(), vd.first.GetHex().c_str(), strForkName.c_str());
                return false;
            }
        }
    }
    return true;
}

bool CCheckForkManager::GetValidFdForkId(const uint256& hashBlock, map<uint256, int>& mapFdForkIdOut)
{
    const auto it = mapBlockValidFork.find(hashBlock);
    if (it != mapBlockValidFork.end())
    {
        if (it->second.hashRefFdBlock != 0)
        {
            const auto mt = mapBlockValidFork.find(it->second.hashRefFdBlock);
            if (mt != mapBlockValidFork.end() && !mt->second.mapForkId.empty())
            {
                mapFdForkIdOut.insert(mt->second.mapForkId.begin(), mt->second.mapForkId.end());
            }
        }
        if (!it->second.mapForkId.empty())
        {
            mapFdForkIdOut.insert(it->second.mapForkId.begin(), it->second.mapForkId.end());
        }
        return true;
    }
    return false;
}

int CCheckForkManager::GetValidForkCreatedHeight(const uint256& hashBlock, const uint256& hashFork)
{
    const auto it = mapBlockValidFork.find(hashBlock);
    if (it != mapBlockValidFork.end())
    {
        int nCreaatedHeight = it->second.GetCreatedHeight(hashFork);
        if (nCreaatedHeight >= 0)
        {
            return nCreaatedHeight;
        }
        if (it->second.hashRefFdBlock != 0)
        {
            const auto mt = mapBlockValidFork.find(it->second.hashRefFdBlock);
            if (mt != mapBlockValidFork.end())
            {
                return mt->second.GetCreatedHeight(hashFork);
            }
        }
    }
    return -1;
}

bool CCheckForkManager::CheckDbValidFork(const uint256& hashBlock, const uint256& hashRefFdBlock, const map<uint256, int>& mapValidFork)
{
    uint256 hashRefFdBlockGet;
    map<uint256, int> mapValidForkGet;
    if (!dbFork.RetrieveValidForkHash(hashBlock, hashRefFdBlockGet, mapValidForkGet))
    {
        StdLog("check", "CheckDbValidFork: RetrieveValidForkHash fail, block: %s", hashBlock.GetHex().c_str());
        return false;
    }
    if (hashRefFdBlockGet != hashRefFdBlock || mapValidForkGet.size() != mapValidFork.size())
    {
        StdLog("check", "CheckDbValidFork: hashRefFdBlock or mapValidFork error, block: %s", hashBlock.GetHex().c_str());
        return false;
    }
    for (const auto vd : mapValidForkGet)
    {
        const auto it = mapValidFork.find(vd.first);
        if (it == mapValidFork.end() || it->second != vd.second)
        {
            StdLog("check", "CheckDbValidFork: mapValidFork error, block: %s", hashBlock.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckForkManager::AddDbValidForkHash(const uint256& hashBlock, const uint256& hashRefFdBlock, const map<uint256, int>& mapValidFork)
{
    return dbFork.AddValidForkHash(hashBlock, hashRefFdBlock, mapValidFork);
}

bool CCheckForkManager::AddDbForkContext(const CForkContext& ctxt)
{
    return dbFork.AddNewForkContext(ctxt);
}

bool CCheckForkManager::UpdateDbForkLast(const uint256& hashFork, const uint256& hashLastBlock)
{
    return dbFork.UpdateFork(hashFork, hashLastBlock);
}

bool CCheckForkManager::GetValidForkContext(const uint256& hashPrimaryLastBlock, const uint256& hashFork, CForkContext& ctxt)
{
    if (GetValidForkCreatedHeight(hashPrimaryLastBlock, hashFork) < 0)
    {
        StdLog("check", "GetValidForkContext: find valid fork fail, fork: %s", hashFork.GetHex().c_str());
        return false;
    }
    const auto it = mapForkSched.find(hashFork);
    if (it == mapForkSched.end())
    {
        StdLog("check", "GetValidForkContext: find fork context fail, fork: %s", hashFork.GetHex().c_str());
        return false;
    }
    ctxt = it->second.ctxtFork;
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckWalletForkUnspent

bool CCheckWalletForkUnspent::LocalTxExist(const uint256& txid)
{
    map<uint256, CCheckWalletTx>::iterator it = mapWalletTx.find(txid);
    if (it == mapWalletTx.end())
    {
        StdLog("check", "Wallet fork LocalTxExist: find tx fail, txid: %s", txid.GetHex().c_str());
        return false;
    }
    if (it->second.hashFork != hashFork)
    {
        StdLog("check", "Wallet fork LocalTxExist: fork error, txid: %s, wtx fork: %s, wallet fork: %s",
               txid.GetHex().c_str(), it->second.hashFork.GetHex().c_str(), hashFork.GetHex().c_str());
        return false;
    }
    return true;
}

CCheckWalletTx* CCheckWalletForkUnspent::GetLocalWalletTx(const uint256& txid)
{
    map<uint256, CCheckWalletTx>::iterator it = mapWalletTx.find(txid);
    if (it == mapWalletTx.end())
    {
        StdLog("check", "Wallet fork GetLocalWalletTx: find tx fail, txid: %s, forkid: %s", txid.GetHex().c_str(), hashFork.GetHex().c_str());
        return nullptr;
    }
    if (it->second.hashFork != hashFork)
    {
        StdLog("check", "Wallet fork GetLocalWalletTx: fork error, txid: %s, wtx fork: %s, wallet fork: %s",
               txid.GetHex().c_str(), it->second.hashFork.GetHex().c_str(), hashFork.GetHex().c_str());
        return nullptr;
    }
    return &(it->second);
}

bool CCheckWalletForkUnspent::AddTx(const CWalletTx& wtx)
{
    if (mapWalletTx.find(wtx.txid) != mapWalletTx.end())
    {
        StdDebug("check", "Add wallet tx: tx is exist, wtx txid: %s, wtx at forkid: %s, forkid: %s",
                 wtx.txid.GetHex().c_str(), wtx.hashFork.GetHex().c_str(), hashFork.GetHex().c_str());
        return true;
    }
    map<uint256, CCheckWalletTx>::iterator it = mapWalletTx.insert(make_pair(wtx.txid, CCheckWalletTx(wtx, ++nSeqCreate))).first;
    if (it == mapWalletTx.end())
    {
        StdLog("check", "Add wallet tx: add fail, txid: %s", wtx.txid.GetHex().c_str());
        return false;
    }
    CWalletTxLinkSetByTxHash& idxTx = setWalletTxLink.get<0>();
    if (idxTx.find(wtx.txid) != idxTx.end())
    {
        setWalletTxLink.erase(wtx.txid);
    }
    if (!setWalletTxLink.insert(CWalletTxLink(&(it->second))).second)
    {
        StdLog("check", "Add wallet tx: add link fail, txid: %s", wtx.txid.GetHex().c_str());
        return false;
    }
    return true;
}

void CCheckWalletForkUnspent::RemoveTx(const uint256& txid)
{
    mapWalletTx.erase(txid);
    setWalletTxLink.erase(txid);
}

bool CCheckWalletForkUnspent::UpdateUnspent()
{
    CWalletTxLinkSetBySequenceNumber& idxTx = setWalletTxLink.get<1>();
    {
        CWalletTxLinkSetBySequenceNumber::iterator it = idxTx.begin();
        for (; it != idxTx.end(); ++it)
        {
            const CCheckWalletTx& wtx = *(it->ptx);
            if (wtx.IsMine())
            {
                if (!AddWalletUnspent(CTxOutPoint(wtx.txid, 0), wtx.GetOutput(0)))
                {
                    StdError("check", "Wallet update unspent: add unspent 0 fail");
                    return false;
                }
            }
            if (wtx.IsFromMe())
            {
                if (!AddWalletUnspent(CTxOutPoint(wtx.txid, 1), wtx.GetOutput(1)))
                {
                    StdError("check", "Wallet update unspent: add unspent 1 fail");
                    return false;
                }
            }
        }
    }
    {
        CWalletTxLinkSetBySequenceNumber::iterator it = idxTx.begin();
        for (; it != idxTx.end(); ++it)
        {
            const CCheckWalletTx& wtx = *(it->ptx);
            if (wtx.IsFromMe())
            {
                for (int n = 0; n < wtx.vInput.size(); n++)
                {
                    if (!AddWalletSpent(wtx.vInput[n].prevout, wtx.txid, wtx.sendTo))
                    {
                        StdError("check", "Wallet update unspent: spent fail");
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

bool CCheckWalletForkUnspent::AddWalletSpent(const CTxOutPoint& txPoint, const uint256& txidSpent, const CDestination& sendTo)
{
    map<CTxOutPoint, CCheckTxOut>::iterator it = mapWalletUnspent.find(txPoint);
    if (it == mapWalletUnspent.end())
    {
        StdError("check", "AddWalletSpent find fail, utxo: [%d] %s.", txPoint.n, txPoint.hash.GetHex().c_str());
        return false;
    }
    mapWalletUnspent.erase(it);
    return true;
}

bool CCheckWalletForkUnspent::AddWalletUnspent(const CTxOutPoint& txPoint, const CTxOut& txOut)
{
    if (!txOut.IsNull())
    {
        if (!mapWalletUnspent.insert(make_pair(txPoint, CCheckTxOut(txOut))).second)
        {
            StdError("check", "insert wallet unspent fail, utxo: [%d] %s.", txPoint.n, txPoint.hash.GetHex().c_str());
            return false;
        }
    }
    return true;
}

int CCheckWalletForkUnspent::GetTxAtBlockHeight(const uint256& txid)
{
    map<uint256, CCheckWalletTx>::iterator it = mapWalletTx.find(txid);
    if (it == mapWalletTx.end())
    {
        return -1;
    }
    return it->second.nBlockHeight;
}

bool CCheckWalletForkUnspent::CheckWalletUnspent(const CTxOutPoint& point, const CCheckTxOut& out)
{
    map<CTxOutPoint, CCheckTxOut>::iterator mt = mapWalletUnspent.find(point);
    if (mt == mapWalletUnspent.end())
    {
        StdLog("check", "CheckWalletUnspent: find unspent fail, utxo: [%d] %s.", point.n, point.hash.GetHex().c_str());
        return false;
    }
    if (mt->second != out)
    {
        StdLog("check", "CheckWalletUnspent: out error, utxo: [%d] %s.", point.n, point.hash.GetHex().c_str());
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckDelegateDB

bool CCheckDelegateDB::CheckDelegate(const uint256& hashBlock)
{
    CDelegateContext ctxtDelegate;
    return Retrieve(hashBlock, ctxtDelegate);
}

bool CCheckDelegateDB::UpdateDelegate(const uint256& hashBlock, CBlockEx& block, uint32 nBlockFile, uint32 nBlockOffset)
{
    if (block.IsGenesis())
    {
        CDelegateContext ctxtDelegate;
        if (!AddNew(hashBlock, ctxtDelegate))
        {
            StdTrace("check", "Update genesis delegate fail, block: %s", hashBlock.ToString().c_str());
            return false;
        }
        return true;
    }

    CDelegateContext ctxtDelegate;
    map<CDestination, int64>& mapDelegate = ctxtDelegate.mapVote;
    map<int, map<CDestination, CDiskPos>>& mapEnrollTx = ctxtDelegate.mapEnrollTx;
    if (!RetrieveDelegatedVote(block.hashPrev, mapDelegate))
    {
        StdError("check", "Update delegate vote: RetrieveDelegatedVote fail, hashPrev: %s", block.hashPrev.GetHex().c_str());
        return false;
    }

    {
        CTemplateId tid;
        if (block.txMint.sendTo.GetTemplateId(tid) && tid.GetType() == TEMPLATE_DELEGATE)
        {
            mapDelegate[block.txMint.sendTo] += block.txMint.nAmount;
        }
    }

    CBufStream ss;
    CVarInt var(block.vtx.size());
    uint32 nOffset = nBlockOffset + block.GetTxSerializedOffset()
                     + ss.GetSerializeSize(block.txMint)
                     + ss.GetSerializeSize(var);
    for (int i = 0; i < block.vtx.size(); i++)
    {
        CTransaction& tx = block.vtx[i];
        CDestination destInDelegateTemplate;
        CDestination sendToDelegateTemplate;
        CTxContxt& txContxt = block.vTxContxt[i];
        if (!CTemplate::ParseDelegateDest(txContxt.destIn, tx.sendTo, tx.vchSig, destInDelegateTemplate, sendToDelegateTemplate))
        {
            StdLog("check", "Update delegate vote: parse delegate dest fail, destIn: %s, sendTo: %s, block: %s, txid: %s",
                   CAddress(txContxt.destIn).ToString().c_str(), CAddress(tx.sendTo).ToString().c_str(), hashBlock.GetHex().c_str(), tx.GetHash().GetHex().c_str());
            return false;
        }
        if (!sendToDelegateTemplate.IsNull())
        {
            mapDelegate[sendToDelegateTemplate] += tx.nAmount;
        }
        if (!destInDelegateTemplate.IsNull())
        {
            mapDelegate[destInDelegateTemplate] -= (tx.nAmount + tx.nTxFee);
        }
        if (tx.nType == CTransaction::TX_CERT)
        {
            if (destInDelegateTemplate.IsNull())
            {
                StdLog("check", "Update delegate vote: TX_CERT destInDelegate is null, destInDelegate: %s, block: %s, txid: %s",
                       CAddress(destInDelegateTemplate).ToString().c_str(), hashBlock.GetHex().c_str(), tx.GetHash().GetHex().c_str());
                return false;
            }
            int nCertAnchorHeight = 0;
            try
            {
                CIDataStream is(tx.vchData);
                is >> nCertAnchorHeight;
            }
            catch (...)
            {
                StdLog("check", "Update delegate vote: TX_CERT vchData error, destInDelegate: %s, block: %s, txid: %s",
                       CAddress(destInDelegateTemplate).ToString().c_str(), hashBlock.GetHex().c_str(), tx.GetHash().GetHex().c_str());
                return false;
            }
            mapEnrollTx[nCertAnchorHeight].insert(make_pair(destInDelegateTemplate, CDiskPos(nBlockFile, nOffset)));
        }
        nOffset += ss.GetSerializeSize(tx);
    }

    if (!AddNew(hashBlock, ctxtDelegate))
    {
        StdError("check", "Update delegate context failed, block: %s", hashBlock.ToString().c_str());
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckWalletTxWalker

bool CCheckWalletTxWalker::Walk(const CWalletTx& wtx)
{
    if (wtx.hashFork == 0)
    {
        StdError("check", "Wallet Walk: fork is 0.");
        return false;
    }
    return AddWalletTx(wtx);
}

bool CCheckWalletTxWalker::Exist(const uint256& hashFork, const uint256& txid)
{
    if (hashFork == 0)
    {
        StdError("check", "Wallet Exist: hashFork is 0.");
        return false;
    }
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
    if (it == mapWalletFork.end())
    {
        StdError("check", "Wallet Exist: hashFork find fail, fork: %s.", hashFork.GetHex().c_str());
        return false;
    }
    return it->second.LocalTxExist(txid);
}

CCheckWalletTx* CCheckWalletTxWalker::GetWalletTx(const uint256& hashFork, const uint256& txid)
{
    if (hashFork == 0)
    {
        StdError("check", "Wallet get tx: hashFork is 0.");
        return nullptr;
    }
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
    if (it == mapWalletFork.end())
    {
        StdError("check", "Wallet get tx: hashFork find fail, fork: %s.", hashFork.GetHex().c_str());
        return nullptr;
    }
    return it->second.GetLocalWalletTx(txid);
}

bool CCheckWalletTxWalker::AddWalletTx(const CWalletTx& wtx)
{
    if (pCheckForkManager == nullptr)
    {
        StdError("check", "Wallet add tx: pCheckForkManager is null.");
        return false;
    }
    if (wtx.hashFork == 0)
    {
        StdError("check", "Wallet add tx: wtx fork is 0.");
        return false;
    }

    vector<uint256> vFork;
    pCheckForkManager->GetTxFork(wtx.hashFork, wtx.nBlockHeight, vFork);
    for (const uint256& hashFork : vFork)
    {
        map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
        if (it == mapWalletFork.end())
        {
            it = mapWalletFork.insert(make_pair(hashFork, CCheckWalletForkUnspent(hashFork))).first;
            if (it == mapWalletFork.end())
            {
                StdError("check", "Wallet add tx: add fork fail.");
                return false;
            }
        }
        if (!it->second.AddTx(wtx))
        {
            StdError("check", "Wallet add tx: add fail.");
            return false;
        }
    }

    nWalletTxCount++;
    return true;
}

void CCheckWalletTxWalker::RemoveWalletTx(const uint256& hashFork, int nHeight, const uint256& txid)
{
    if (hashFork == 0)
    {
        StdError("check", "Wallet remove tx: hashFork is 0.");
        return;
    }

    vector<uint256> vFork;
    pCheckForkManager->GetTxFork(hashFork, nHeight, vFork);
    for (const uint256& hash : vFork)
    {
        map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hash);
        if (it == mapWalletFork.end())
        {
            StdError("check", "Wallet remove tx: fork find fail, fork: %s.", hash.GetHex().c_str());
        }
        else
        {
            it->second.RemoveTx(txid);
        }
    }

    nWalletTxCount--;
}

bool CCheckWalletTxWalker::UpdateUnspent()
{
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.begin();
    for (; it != mapWalletFork.end(); ++it)
    {
        if (!it->second.UpdateUnspent())
        {
            StdError("check", "Wallet update unspent fail, fork: %s.", it->first.GetHex().c_str());
            return false;
        }
    }
    return true;
}

int CCheckWalletTxWalker::GetTxAtBlockHeight(const uint256& hashFork, const uint256& txid)
{
    if (hashFork == 0)
    {
        StdError("check", "Wallet get block height: hashFork is 0.");
        return -1;
    }
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
    if (it == mapWalletFork.end())
    {
        StdError("check", "Wallet get block height: fork find fail, fork: %s.", hashFork.GetHex().c_str());
        return -1;
    }
    return it->second.GetTxAtBlockHeight(txid);
}

bool CCheckWalletTxWalker::CheckWalletUnspent(const uint256& hashFork, const CTxOutPoint& point, const CCheckTxOut& out)
{
    map<uint256, CCheckWalletForkUnspent>::iterator it = mapWalletFork.find(hashFork);
    if (it == mapWalletFork.end())
    {
        StdError("check", "Wallet check unspent: fork find fail, fork: %s.", hashFork.GetHex().c_str());
        return false;
    }
    return it->second.CheckWalletUnspent(point, out);
}

/////////////////////////////////////////////////////////////////////////
// CCheckForkTxPool

bool CCheckForkTxPool::AddTx(const uint256& txid, const CAssembledTx& tx)
{
    vTx.push_back(make_pair(txid, tx));

    if (!mapTxPoolTx.insert(make_pair(txid, tx)).second)
    {
        StdError("check", "TxPool AddTx: Add tx failed, txid: %s.", txid.GetHex().c_str());
        return false;
    }
    for (int i = 0; i < tx.vInput.size(); i++)
    {
        if (!Spent(tx.vInput[i].prevout, txid, tx.sendTo))
        {
            StdError("check", "TxPool AddTx: Spent fail, txid: %s.", txid.GetHex().c_str());
            return false;
        }
    }
    if (!Unspent(CTxOutPoint(txid, 0), tx.GetOutput(0)))
    {
        StdError("check", "TxPool AddTx: Add unspent 0 fail, txid: %s.", txid.GetHex().c_str());
        return false;
    }
    if (!tx.IsMintTx())
    {
        if (!Unspent(CTxOutPoint(txid, 1), tx.GetOutput(1)))
        {
            StdError("check", "TxPool AddTx: Add unspent 1 fail, txid: %s.", txid.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckForkTxPool::Spent(const CTxOutPoint& point, const uint256& txidSpent, const CDestination& sendTo)
{
    map<CTxOutPoint, CCheckTxOut>::iterator it = mapTxPoolUnspent.find(point);
    if (it == mapTxPoolUnspent.end())
    {
        StdError("check", "TxPool Spent: find fail, utxo: [%d] %s", point.n, point.hash.GetHex().c_str());
        return false;
    }
    mapTxPoolUnspent.erase(it);
    return true;
}

bool CCheckForkTxPool::Unspent(const CTxOutPoint& point, const CTxOut& out)
{
    if (!out.IsNull())
    {
        if (!mapTxPoolUnspent.insert(make_pair(point, CCheckTxOut(out))).second)
        {
            StdError("check", "TxPool Unspent: insert unspent fail, utxo: [%d] %s.", point.n, point.hash.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckForkTxPool::CheckTxExist(const uint256& txid)
{
    return (mapTxPoolTx.find(txid) != mapTxPoolTx.end());
}

bool CCheckForkTxPool::CheckTxPoolUnspent(const CTxOutPoint& point, const CCheckTxOut& out)
{
    map<CTxOutPoint, CCheckTxOut>::iterator it = mapTxPoolUnspent.find(point);
    if (it == mapTxPoolUnspent.end())
    {
        StdLog("check", "TxPool CheckTxPoolUnspent: find fail, utxo: [%d] %s", point.n, point.hash.GetHex().c_str());
        return false;
    }
    if (it->second != out)
    {
        StdLog("check", "TxPool CheckTxPoolUnspent: out error, utxo: [%d] %s", point.n, point.hash.GetHex().c_str());
        return false;
    }
    return true;
}

bool CCheckForkTxPool::GetWalletTx(const uint256& hashFork, const set<CDestination>& setAddress, vector<CWalletTx>& vWalletTx)
{
    for (int i = 0; i < vTx.size(); i++)
    {
        const CAssembledTx& atx = vTx[i].second;
        bool fIsMine = (setAddress.find(atx.sendTo) != setAddress.end());
        bool fFromMe = (setAddress.find(atx.destIn) != setAddress.end());
        if (fIsMine || fFromMe)
        {
            CWalletTx wtx(vTx[i].first, atx, hashFork, fIsMine, fFromMe);
            vWalletTx.push_back(wtx);
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckTxPoolData

void CCheckTxPoolData::AddForkUnspent(const uint256& hashFork, const map<CTxOutPoint, CCheckTxOut>& mapUnspent)
{
    if (hashFork != 0)
    {
        mapForkTxPool[hashFork].mapTxPoolUnspent = mapUnspent;
    }
}

bool CCheckTxPoolData::FetchTxPool(const string& strPath)
{
    CTxPoolData datTxPool;
    if (!datTxPool.Initialize(path(strPath)))
    {
        StdLog("check", "TxPool: Failed to initialize txpool data");
        return false;
    }

    vector<pair<uint256, pair<uint256, CAssembledTx>>> vTx;
    if (!datTxPool.LoadCheck(vTx))
    {
        StdLog("check", "TxPool: Load txpool data failed");
        return false;
    }

    for (int i = 0; i < vTx.size(); i++)
    {
        if (vTx[i].first == 0)
        {
            StdError("check", "TxPool: tx fork hash is 0, txid: %s", vTx[i].second.first.GetHex().c_str());
            return false;
        }
        if (!mapForkTxPool[vTx[i].first].AddTx(vTx[i].second.first, vTx[i].second.second))
        {
            StdError("check", "TxPool: Add tx fail, txid: %s", vTx[i].second.first.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckTxPoolData::CheckTxExist(const uint256& hashFork, const uint256& txid)
{
    map<uint256, CCheckForkTxPool>::iterator it = mapForkTxPool.find(hashFork);
    if (it == mapForkTxPool.end())
    {
        return false;
    }
    return it->second.CheckTxExist(txid);
}

bool CCheckTxPoolData::CheckTxPoolUnspent(const uint256& hashFork, const CTxOutPoint& point, const CCheckTxOut& out)
{
    map<uint256, CCheckForkTxPool>::iterator it = mapForkTxPool.find(hashFork);
    if (it == mapForkTxPool.end())
    {
        return false;
    }
    return it->second.CheckTxPoolUnspent(point, out);
}

bool CCheckTxPoolData::GetTxPoolWalletTx(const set<CDestination>& setAddress, vector<CWalletTx>& vWalletTx)
{
    map<uint256, CCheckForkTxPool>::iterator it = mapForkTxPool.begin();
    for (; it != mapForkTxPool.end(); ++it)
    {
        if (!it->second.GetWalletTx(it->first, setAddress, vWalletTx))
        {
            return false;
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckBlockFork

void CCheckBlockFork::UpdateMaxTrust(CBlockIndex* pBlockIndex)
{
    if (pBlockIndex->IsOrigin())
    {
        if (pOrigin != nullptr)
        {
            StdLog("check", "CCheckBlockFork pOrigin is not NULL");
            return;
        }
        pOrigin = pBlockIndex;
    }
    if (pLast == nullptr
        || !(pLast->nChainTrust > pBlockIndex->nChainTrust
             || (pLast->nChainTrust == pBlockIndex->nChainTrust && !pBlockIndex->IsEquivalent(pLast))))
    {
        pLast = pBlockIndex;
    }
}

bool CCheckBlockFork::AddBlockTx(const CTransaction& txIn, const CTxContxt& contxtIn, int nHeight, const uint256& hashAtForkIn, uint32 nFileNoIn, uint32 nOffsetIn)
{
    const uint256 txid = txIn.GetHash();
    map<uint256, CCheckBlockTx>::iterator mt = mapBlockTx.insert(make_pair(txid, CCheckBlockTx(txIn, contxtIn, nHeight, hashAtForkIn, nFileNoIn, nOffsetIn))).first;
    if (mt == mapBlockTx.end())
    {
        StdLog("check", "AddBlockTx: add block tx fail, txid: %s.", txid.GetHex().c_str());
        return false;
    }
    for (int i = 0; i < txIn.vInput.size(); i++)
    {
        if (!AddBlockSpent(txIn.vInput[i].prevout, txid, txIn.sendTo))
        {
            StdLog("check", "AddBlockTx: block spent fail, txid: %s.", txid.GetHex().c_str());
            return false;
        }
    }
    if (!AddBlockUnspent(CTxOutPoint(txid, 0), CTxOut(txIn)))
    {
        StdLog("check", "AddBlockTx: add block unspent 0 fail, txid: %s.", txid.GetHex().c_str());
        return false;
    }
    if (!txIn.IsMintTx())
    {
        if (!AddBlockUnspent(CTxOutPoint(txid, 1), CTxOut(txIn, contxtIn.destIn, contxtIn.GetValueIn())))
        {
            StdLog("check", "AddBlockTx: add block unspent 1 fail, txid: %s.", txid.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckBlockFork::AddBlockSpent(const CTxOutPoint& txPoint, const uint256& txidSpent, const CDestination& sendTo)
{
    map<CTxOutPoint, CCheckTxOut>::iterator it = mapBlockUnspent.find(txPoint);
    if (it == mapBlockUnspent.end())
    {
        StdLog("check", "AddBlockSpent: utxo find fail, utxo: [%d] %s.", txPoint.n, txPoint.hash.GetHex().c_str());
        return false;
    }
    mapBlockUnspent.erase(it);
    return true;
}

bool CCheckBlockFork::AddBlockUnspent(const CTxOutPoint& txPoint, const CTxOut& txOut)
{
    if (!txOut.IsNull())
    {
        if (!mapBlockUnspent.insert(make_pair(txPoint, CCheckTxOut(txOut))).second)
        {
            StdLog("check", "AddBlockUnspent: Add block unspent fail, utxo: [%d] %s.", txPoint.n, txPoint.hash.GetHex().c_str());
            return false;
        }
    }
    return true;
}

bool CCheckBlockFork::CheckTxExist(const uint256& txid, int& nHeight)
{
    map<uint256, CCheckBlockTx>::iterator it = mapBlockTx.find(txid);
    if (it == mapBlockTx.end())
    {
        StdLog("check", "CheckBlockFork: Check tx exist, find tx fail, txid: %s", txid.GetHex().c_str());
        return false;
    }
    nHeight = it->second.txIndex.nBlockHeight;
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckBlockWalker

CCheckBlockWalker::~CCheckBlockWalker()
{
    ClearBlockIndex();
    dbBlockIndex.Deinitialize();
}

bool CCheckBlockWalker::Initialize(const string& strPath, CCheckForkManager* pForkMn)
{
    pCheckForkManager = pForkMn;
    if (!objDelegateDB.Initialize(path(strPath)))
    {
        StdError("check", "Block walker: Delegate db initialize fail, path: %s.", strPath.c_str());
        return false;
    }

    if (!objTsBlock.Initialize(path(strPath) / "block", BLOCKFILE_PREFIX))
    {
        StdError("check", "Block walker: TsBlock initialize fail, path: %s.", strPath.c_str());
        return false;
    }

    if (!dbBlockIndex.Initialize(path(strPath)))
    {
        StdLog("check", "dbBlockIndex Initialize fail");
        return false;
    }
    dbBlockIndex.WalkThroughBlock(objBlockIndexWalker);
    StdLog("check", "Fetch block index success, count: %ld", objBlockIndexWalker.mapBlockIndex.size());
    return true;
}

bool CCheckBlockWalker::Walk(const CBlockEx& block, uint32 nFile, uint32 nOffset)
{
    const uint256 hashBlock = block.GetHash();
    if (mapBlock.count(hashBlock) > 0)
    {
        StdError("check", "Block walk: block exist, hash: %s.", hashBlock.GetHex().c_str());
        return true;
    }

    CBlockEx* pPrevBlock = nullptr;
    CBlockIndex* pIndexPrev = nullptr;
    if (block.hashPrev != 0)
    {
        map<uint256, CBlockEx>::iterator it = mapBlock.find(block.hashPrev);
        if (it != mapBlock.end())
        {
            pPrevBlock = &(it->second);
        }
        map<uint256, CBlockIndex*>::iterator mt = mapBlockIndex.find(block.hashPrev);
        if (mt != mapBlockIndex.end())
        {
            pIndexPrev = mt->second;
        }
    }
    if (!block.IsGenesis() && pPrevBlock == nullptr)
    {
        StdError("check", "Block walk: Prev block not exist, block: %s.", hashBlock.GetHex().c_str());
        return false;
    }

    map<uint256, CBlockEx>::iterator mt = mapBlock.insert(make_pair(hashBlock, block)).first;
    if (mt == mapBlock.end())
    {
        StdError("check", "Block walk: Insert block fail, block: %s.", hashBlock.GetHex().c_str());
        return false;
    }
    CBlockEx& checkBlock = mt->second;

    if (block.IsPrimary())
    {
        if (!objDelegateDB.CheckDelegate(hashBlock))
        {
            StdError("check", "Block walk: Check delegate vote fail, block: %s", hashBlock.GetHex().c_str());
            if (!fOnlyCheck)
            {
                if (!objDelegateDB.UpdateDelegate(hashBlock, checkBlock, nFile, nOffset))
                {
                    StdError("check", "Block walk: Update delegate fail, block: %s.", hashBlock.GetHex().c_str());
                    return false;
                }
            }
        }
        if (!pCheckForkManager->AddBlockForkContext(checkBlock))
        {
            StdError("check", "Block walk: Add block fork fail, block: %s", hashBlock.GetHex().c_str());
            return false;
        }
    }

    CBlockIndex* pNewBlockIndex = nullptr;
    CBlockOutline* pBlockOutline = objBlockIndexWalker.GetBlockOutline(hashBlock);
    if (pBlockOutline == nullptr)
    {
        uint256 nChainTrust;
        if (pIndexPrev != nullptr && !block.IsOrigin() && !block.IsNull())
        {
            if (block.IsProofOfWork())
            {
                if (!GetBlockTrust(block, nChainTrust))
                {
                    StdError("check", "Block walk: Get block trust fail 1, block: %s.", hashBlock.GetHex().c_str());
                    return false;
                }
            }
            else
            {
                CBlockIndex* pIndexRef = nullptr;
                CDelegateAgreement agreement;
                size_t nEnrollTrust = 0;
                if (block.IsPrimary())
                {
                    if (!GetBlockDelegateAgreement(hashBlock, checkBlock, pIndexPrev, agreement, nEnrollTrust))
                    {
                        StdError("check", "Block walk: GetBlockDelegateAgreement fail, block: %s.", hashBlock.GetHex().c_str());
                        return false;
                    }
                }
                else if (block.IsSubsidiary() || block.IsExtended())
                {
                    CProofOfPiggyback proof;
                    proof.Load(block.vchProof);
                    if (proof.hashRefBlock != 0)
                    {
                        map<uint256, CBlockIndex*>::iterator mt = mapBlockIndex.find(proof.hashRefBlock);
                        if (mt != mapBlockIndex.end())
                        {
                            pIndexRef = mt->second;
                        }
                    }
                    if (pIndexRef == nullptr)
                    {
                        StdError("check", "Block walk: refblock find fail, refblock: %s, block: %s.", proof.hashRefBlock.GetHex().c_str(), hashBlock.GetHex().c_str());
                        return false;
                    }
                    if (pIndexRef->pPrev == nullptr)
                    {
                        StdError("check", "Block walk: pIndexRef->pPrev is null, refblock: %s, block: %s.", proof.hashRefBlock.GetHex().c_str(), hashBlock.GetHex().c_str());
                        return false;
                    }
                }
                if (!GetBlockTrust(block, nChainTrust, pIndexPrev, agreement, pIndexRef, nEnrollTrust))
                {
                    StdError("check", "Block walk: Get block trust fail 2, block: %s.", hashBlock.GetHex().c_str());
                    return false;
                }
            }
        }
        pNewBlockIndex = AddNewIndex(hashBlock, static_cast<const CBlock&>(checkBlock), nFile, nOffset, nChainTrust);
        if (pNewBlockIndex == nullptr)
        {
            StdError("check", "Block walk: Add new block index fail 1, block: %s.", hashBlock.GetHex().c_str());
            return false;
        }
    }
    else
    {
        pNewBlockIndex = AddNewIndex(hashBlock, *pBlockOutline);
        if (pNewBlockIndex == nullptr)
        {
            StdError("check", "Block walk: Add new block index fail 2, block: %s.", hashBlock.GetHex().c_str());
            return false;
        }
        if (pNewBlockIndex->nFile != nFile || pNewBlockIndex->nOffset != nOffset)
        {
            StdLog("check", "Block walk: Block index param error, block: %s, File: %d - %d, Offset: %d - %d.",
                   hashBlock.GetHex().c_str(), pNewBlockIndex->nFile, nFile, pNewBlockIndex->nOffset, nOffset);
            pNewBlockIndex->nFile = nFile;
            pNewBlockIndex->nOffset = nOffset;
        }
    }
    if (pNewBlockIndex->GetOriginHash() == 0)
    {
        StdError("check", "Block walk: Get block origin hash is 0, block: %s.", hashBlock.GetHex().c_str());
        return false;
    }
    mapCheckFork[pNewBlockIndex->GetOriginHash()].UpdateMaxTrust(pNewBlockIndex);

    if (block.IsGenesis())
    {
        if (block.GetHash() != objProofParam.hashGenesisBlock)
        {
            StdError("check", "Block walk: genesis block error, block hash: %s, genesis hash: %s.",
                     hashBlock.GetHex().c_str(), objProofParam.hashGenesisBlock.GetHex().c_str());
            return false;
        }
        hashGenesis = hashBlock;
    }

    nBlockCount++;

    return true;
}

bool CCheckBlockWalker::GetBlockTrust(const CBlockEx& block, uint256& nChainTrust, const CBlockIndex* pIndexPrev, const CDelegateAgreement& agreement, const CBlockIndex* pIndexRef, size_t nEnrollTrust)
{
    if (block.IsGenesis())
    {
        nChainTrust = uint64(0);
    }
    else if (block.IsVacant())
    {
        nChainTrust = uint64(0);
    }
    else if (block.IsPrimary())
    {
        if (block.IsProofOfWork())
        {
            // PoW difficulty = 2 ^ nBits
            CProofOfHashWorkCompact proof;
            proof.Load(block.vchProof);
            uint256 v(1);
            nChainTrust = v << proof.nBits;
        }
        else if (pIndexPrev != nullptr)
        {
            if (!objProofParam.IsDposHeight(block.GetBlockHeight()))
            {
                StdError("check", "GetBlockTrust: not dpos height, height: %d", block.GetBlockHeight());
                return false;
            }

            // Get the last PoW block nAlgo
            int nAlgo;
            const CBlockIndex* pIndex = pIndexPrev;
            while (!pIndex->IsProofOfWork() && (pIndex->pPrev != nullptr))
            {
                pIndex = pIndex->pPrev;
            }
            if (!pIndex->IsProofOfWork())
            {
                nAlgo = CM_CRYPTONIGHT;
            }
            else
            {
                nAlgo = pIndex->nProofAlgo;
            }

            // DPoS difficulty = weight * (2 ^ nBits)
            int nBits;
            if (GetProofOfWorkTarget(pIndexPrev, nAlgo, nBits))
            {
                if (agreement.nWeight == 0 || nBits <= 0)
                {
                    StdError("check", "GetBlockTrust: nWeight or nBits error, nWeight: %lu, nBits: %d", agreement.nWeight, nBits);
                    return false;
                }
                if (nEnrollTrust <= 0)
                {
                    StdError("check", "GetBlockTrust: nEnrollTrust error, nEnrollTrust: %lu", nEnrollTrust);
                    return false;
                }
                nChainTrust = uint256(uint64(nEnrollTrust)) << nBits;
            }
            else
            {
                StdError("check", "GetBlockTrust: GetProofOfWorkTarget fail");
                return false;
            }
        }
        else
        {
            StdError("check", "GetBlockTrust: Primary pIndexPrev is null");
            return false;
        }
    }
    else if (block.IsOrigin())
    {
        nChainTrust = uint64(0);
    }
    else if ((block.IsSubsidiary() || block.IsExtended()) && (pIndexRef != nullptr))
    {
        if (pIndexRef->pPrev == nullptr)
        {
            StdError("check", "GetBlockTrust: Subsidiary or Extended block pPrev is null, block: %s", block.GetHash().GetHex().c_str());
            return false;
        }
        nChainTrust = pIndexRef->nChainTrust - pIndexRef->pPrev->nChainTrust;
    }
    else
    {
        StdError("check", "GetBlockTrust: block type error");
        return false;
    }
    return true;
}

bool CCheckBlockWalker::GetProofOfWorkTarget(const CBlockIndex* pIndexPrev, int nAlgo, int& nBits)
{
    if (nAlgo <= 0 || nAlgo >= CM_MAX || !pIndexPrev->IsPrimary())
    {
        return false;
    }

    const CBlockIndex* pIndex = pIndexPrev;
    while ((!pIndex->IsProofOfWork() || pIndex->nProofAlgo != nAlgo) && pIndex->pPrev != nullptr)
    {
        pIndex = pIndex->pPrev;
    }

    // first
    if (!pIndex->IsProofOfWork())
    {
        nBits = objProofParam.nProofOfWorkInit;
        return true;
    }

    nBits = pIndex->nProofBits;
    int64 nSpacing = 0;
    int64 nWeight = 0;
    int nWIndex = objProofParam.nProofOfWorkAdjustCount - 1;
    while (pIndex->IsProofOfWork())
    {
        nSpacing += (pIndex->GetBlockTime() - pIndex->pPrev->GetBlockTime()) << nWIndex;
        nWeight += (1ULL) << nWIndex;
        if (!nWIndex--)
        {
            break;
        }
        pIndex = pIndex->pPrev;
        while ((!pIndex->IsProofOfWork() || pIndex->nProofAlgo != nAlgo) && pIndex->pPrev != nullptr)
        {
            pIndex = pIndex->pPrev;
        }
    }
    nSpacing /= nWeight;
    if (objProofParam.IsDposHeight(pIndexPrev->GetBlockHeight() + 1))
    {
        if (nSpacing > objProofParam.nProofOfWorkUpperTargetOfDpos && nBits > objProofParam.nProofOfWorkLowerLimit)
        {
            nBits--;
        }
        else if (nSpacing < objProofParam.nProofOfWorkLowerTargetOfDpos && nBits < objProofParam.nProofOfWorkUpperLimit)
        {
            nBits++;
        }
    }
    else
    {
        if (nSpacing > objProofParam.nProofOfWorkUpperTarget && nBits > objProofParam.nProofOfWorkLowerLimit)
        {
            nBits--;
        }
        else if (nSpacing < objProofParam.nProofOfWorkLowerTarget && nBits < objProofParam.nProofOfWorkUpperLimit)
        {
            nBits++;
        }
    }
    return true;
}

bool CCheckBlockWalker::GetBlockDelegateAgreement(const uint256& hashBlock, const CBlock& block, CBlockIndex* pIndexPrev, CDelegateAgreement& agreement, size_t& nEnrollTrust)
{
    agreement.Clear();
    if (pIndexPrev->GetBlockHeight() < CONSENSUS_INTERVAL - 1)
    {
        return true;
    }

    CBlockIndex* pIndex = pIndexPrev;
    for (int i = 0; i < CONSENSUS_DISTRIBUTE_INTERVAL; i++)
    {
        if (pIndex == nullptr)
        {
            StdLog("check", "GetBlockDelegateAgreement : pIndex is null, block: %s", hashBlock.ToString().c_str());
            return false;
        }
        pIndex = pIndex->pPrev;
    }

    CDelegateEnrolled enrolled;
    if (!GetBlockDelegateEnrolled(pIndex->GetBlockHash(), pIndex, enrolled))
    {
        StdLog("check", "GetBlockDelegateAgreement : GetBlockDelegateEnrolled fail, block: %s", hashBlock.ToString().c_str());
        return false;
    }

    delegate::CDelegateVerify verifier(enrolled.mapWeight, enrolled.mapEnrollData);
    map<CDestination, size_t> mapBallot;
    if (!verifier.VerifyProof(block.vchProof, agreement.nAgreement, agreement.nWeight, mapBallot, objProofParam.DPoSConsensusCheckRepeated(block.GetBlockHeight())))
    {
        StdLog("check", "GetBlockDelegateAgreement : Invalid block proof, block: %s", hashBlock.ToString().c_str());
        return false;
    }

    nEnrollTrust = 0;
    for (auto& amount : enrolled.vecAmount)
    {
        if (mapBallot.find(amount.first) != mapBallot.end())
        {
            nEnrollTrust += (size_t)(min(amount.second, objProofParam.nDelegateProofOfStakeEnrollMaximumAmount));
        }
    }
    nEnrollTrust /= objProofParam.nDelegateProofOfStakeEnrollMinimumAmount;
    return true;
}

bool CCheckBlockWalker::GetBlockDelegateEnrolled(const uint256& hashBlock, CBlockIndex* pIndex, CDelegateEnrolled& enrolled)
{
    enrolled.Clear();
    if (pIndex == nullptr)
    {
        StdLog("check", "GetBlockDelegateEnrolled : pIndex is null, block: %s", hashBlock.ToString().c_str());
        return false;
    }

    if (pIndex->GetBlockHeight() < CONSENSUS_ENROLL_INTERVAL)
    {
        return true;
    }

    vector<uint256> vBlockRange;
    for (int i = 0; i < CONSENSUS_ENROLL_INTERVAL; i++)
    {
        if (pIndex == nullptr)
        {
            StdLog("check", "GetBlockDelegateEnrolled : pIndex is null 2, block: %s", hashBlock.ToString().c_str());
            return false;
        }
        vBlockRange.push_back(pIndex->GetBlockHash());
        pIndex = pIndex->pPrev;
    }

    if (!RetrieveAvailDelegate(hashBlock, pIndex->GetBlockHeight(), vBlockRange, objProofParam.nDelegateProofOfStakeEnrollMinimumAmount,
                               enrolled.mapWeight, enrolled.mapEnrollData, enrolled.vecAmount))
    {
        StdLog("check", "GetBlockDelegateEnrolled : Retrieve Avail Delegate Error, block: %s", hashBlock.ToString().c_str());
        return false;
    }
    return true;
}

bool CCheckBlockWalker::RetrieveAvailDelegate(const uint256& hash, int height, const vector<uint256>& vBlockRange,
                                              int64 nMinEnrollAmount,
                                              map<CDestination, size_t>& mapWeight,
                                              map<CDestination, vector<unsigned char>>& mapEnrollData,
                                              vector<pair<CDestination, int64>>& vecAmount)
{
    map<CDestination, int64> mapVote;
    if (!objDelegateDB.RetrieveDelegatedVote(hash, mapVote))
    {
        StdLog("check", "RetrieveAvailDelegate: RetrieveDelegatedVote fail, block: %s", hash.ToString().c_str());
        return false;
    }

    map<CDestination, CDiskPos> mapEnrollTxPos;
    if (!objDelegateDB.RetrieveEnrollTx(height, vBlockRange, mapEnrollTxPos))
    {
        StdLog("check", "RetrieveAvailDelegate: RetrieveEnrollTx fail, block: %s, height: %d", hash.ToString().c_str(), height);
        return false;
    }

    map<pair<int64, CDiskPos>, pair<CDestination, vector<uint8>>> mapSortEnroll;
    for (map<CDestination, int64>::iterator it = mapVote.begin(); it != mapVote.end(); ++it)
    {
        if ((*it).second >= nMinEnrollAmount)
        {
            const CDestination& dest = (*it).first;
            map<CDestination, CDiskPos>::iterator mi = mapEnrollTxPos.find(dest);
            if (mi != mapEnrollTxPos.end())
            {
                CTransaction tx;
                if (!objTsBlock.Read(tx, (*mi).second))
                {
                    StdLog("check", "RetrieveAvailDelegate: Read tx fail, txid: %s", tx.GetHash().ToString().c_str());
                    return false;
                }

                if (tx.vchData.size() <= sizeof(int))
                {
                    StdLog("check", "RetrieveAvailDelegate: tx.vchData error, txid: %s", tx.GetHash().ToString().c_str());
                    return false;
                }
                std::vector<uint8> vchCertData;
                vchCertData.assign(tx.vchData.begin() + sizeof(int), tx.vchData.end());

                mapSortEnroll.insert(make_pair(make_pair(it->second, mi->second), make_pair(dest, vchCertData)));
            }
        }
    }

    // first 23 destination sorted by amount and sequence
    for (auto it = mapSortEnroll.rbegin(); it != mapSortEnroll.rend() && mapWeight.size() < MAX_DELEGATE_THRESH; it++)
    {
        mapWeight.insert(make_pair(it->second.first, 1));
        mapEnrollData.insert(make_pair(it->second.first, it->second.second));
        vecAmount.push_back(make_pair(it->second.first, it->first.first));
    }
    return true;
}

bool CCheckBlockWalker::UpdateBlockNext()
{
    map<uint256, CCheckBlockFork>::iterator it = mapCheckFork.begin();
    while (it != mapCheckFork.end())
    {
        CCheckBlockFork& checkFork = it->second;
        if (checkFork.pOrigin == nullptr || checkFork.pLast == nullptr)
        {
            if (checkFork.pOrigin == nullptr)
            {
                StdError("check", "UpdateBlockNext: pOrigin is null, fork: %s", it->first.GetHex().c_str());
            }
            else
            {
                StdError("check", "UpdateBlockNext: pLast is null, fork: %s", it->first.GetHex().c_str());
            }
            mapCheckFork.erase(it++);
            continue;
        }
        CBlockIndex* pBlockIndex = checkFork.pLast;
        while (pBlockIndex && !pBlockIndex->IsOrigin())
        {
            if (pBlockIndex->pPrev)
            {
                pBlockIndex->pPrev->pNext = pBlockIndex;
            }
            pBlockIndex = pBlockIndex->pPrev;
        }
        if (it->first == hashGenesis)
        {
            nMainChainHeight = checkFork.pLast->GetBlockHeight();
        }
        ++it;
    }
    return true;
}

bool CCheckBlockWalker::CheckRepairFork()
{
    if (hashGenesis == 0)
    {
        StdLog("check", "Check Repair Fork: hashGenesis is 0");
        return false;
    }
    const auto gt = mapCheckFork.find(hashGenesis);
    if (gt == mapCheckFork.end())
    {
        StdLog("check", "Check Repair Fork: find genesis fork fail");
        return false;
    }
    if (gt->second.pLast == nullptr)
    {
        StdLog("check", "Check Repair Fork: genesis fork pLast is null");
        return false;
    }
    uint256 hashPrimaryLastBlock = gt->second.pLast->GetBlockHash();

    map<uint256, CCheckBlockFork>::iterator it = mapCheckFork.begin();
    while (it != mapCheckFork.end())
    {
        const uint256& hashFork = it->first;
        CCheckBlockFork& checkFork = it->second;
        if (checkFork.pLast == nullptr)
        {
            StdLog("check", "Check Repair Fork: pLast is null, fork: %s", hashFork.GetHex().c_str());
            return false;
        }
        uint256 hashLastBlock = checkFork.pLast->GetBlockHash();

        CForkContext ctxt;
        if (!pCheckForkManager->GetValidForkContext(hashPrimaryLastBlock, hashFork, ctxt))
        {
            StdLog("check", "Check Repair Fork: GetValidForkContext fail, fork: %s", hashFork.GetHex().c_str());
            mapCheckFork.erase(it++);
            continue;
        }

        auto mt = pCheckForkManager->mapForkStatus.find(hashFork);
        if (mt == pCheckForkManager->mapForkStatus.end())
        {
            StdLog("check", "Check Repair Fork: find fork fail, fork: %s", hashFork.GetHex().c_str());
            if (fOnlyCheck)
            {
                return false;
            }

            mt = pCheckForkManager->mapForkStatus.insert(make_pair(hashFork, CCheckForkStatus())).first;
            if (mt == pCheckForkManager->mapForkStatus.end())
            {
                StdLog("check", "Check Repair Fork: insert fail, fork: %s", hashFork.GetHex().c_str());
                return false;
            }
            mt->second.ctxt = ctxt;

            if (!pCheckForkManager->AddDbForkContext(mt->second.ctxt))
            {
                StdLog("check", "Check Repair Fork: Add db fork context fail, fork: %s", hashFork.GetHex().c_str());
                return false;
            }
        }
        if (mt->second.hashLastBlock != hashLastBlock)
        {
            StdLog("check", "Check Repair Fork: last block error, fork: %s, fork last: %s, block last: %s",
                   hashFork.GetHex().c_str(), mt->second.hashLastBlock.GetHex().c_str(), hashLastBlock.GetHex().c_str());
            if (fOnlyCheck)
            {
                return false;
            }
            mt->second.hashLastBlock = hashLastBlock;

            if (!pCheckForkManager->UpdateDbForkLast(hashFork, hashLastBlock))
            {
                StdLog("check", "Check Repair Fork: Update db fork last fail, fork: %s", hashFork.GetHex().c_str());
                return false;
            }
        }

        ++it;
    }

    auto ht = pCheckForkManager->mapForkStatus.begin();
    while (ht != pCheckForkManager->mapForkStatus.end())
    {
        if (ht->second.ctxt.hashParent != 0)
        {
            auto mt = pCheckForkManager->mapForkStatus.find(ht->second.ctxt.hashParent);
            if (mt == pCheckForkManager->mapForkStatus.end())
            {
                StdLog("check", "Check Repair Fork: find parent fork fail, parent fork: %s", ht->second.ctxt.hashParent.GetHex().c_str());
                return false;
            }
            mt->second.InsertSubline(ht->second.ctxt.nJointHeight, ht->first);
        }
        ++ht;
    }
    return true;
}

bool CCheckBlockWalker::UpdateBlockTx()
{
    if (hashGenesis == 0)
    {
        StdError("check", "UpdateBlockTx: hashGenesis is 0");
        return false;
    }

    vector<uint256> vForkList;
    pCheckForkManager->GetForkList(vForkList);

    for (const uint256& hashFork : vForkList)
    {
        map<uint256, CCheckBlockFork>::iterator it = mapCheckFork.find(hashFork);
        if (it != mapCheckFork.end())
        {
            CCheckBlockFork& checkFork = it->second;
            if (checkFork.pOrigin == nullptr)
            {
                StdError("check", "UpdateBlockTx: pOrigin is null, fork: %s", it->first.GetHex().c_str());
                continue;
            }
            CBlockIndex* pIndex = checkFork.pOrigin;
            while (pIndex)
            {
                map<uint256, CBlockEx>::iterator it = mapBlock.find(pIndex->GetBlockHash());
                if (it == mapBlock.end())
                {
                    StdError("check", "UpdateBlockTx: Find block fail, block: %s", pIndex->GetBlockHash().GetHex().c_str());
                    return false;
                }
                const CBlockEx& block = it->second;
                if (!block.IsNull() && (!block.IsVacant() || !block.txMint.sendTo.IsNull()))
                {
                    vector<uint256> vFork;
                    pCheckForkManager->GetTxFork(hashFork, pIndex->GetBlockHeight(), vFork);

                    CBufStream ss;
                    CTxContxt txContxt;
                    txContxt.destIn = block.txMint.sendTo;
                    uint32 nTxOffset = pIndex->nOffset + block.GetTxSerializedOffset();
                    if (!AddBlockTx(block.txMint, txContxt, block.GetBlockHeight(), hashFork, pIndex->nFile, nTxOffset, vFork))
                    {
                        StdError("check", "UpdateBlockTx: Add mint tx fail, txid: %s, block: %s",
                                 block.txMint.GetHash().GetHex().c_str(), pIndex->GetBlockHash().GetHex().c_str());
                        return false;
                    }
                    nTxOffset += ss.GetSerializeSize(block.txMint);

                    CVarInt var(block.vtx.size());
                    nTxOffset += ss.GetSerializeSize(var);
                    for (int i = 0; i < block.vtx.size(); i++)
                    {
                        if (!AddBlockTx(block.vtx[i], block.vTxContxt[i], block.GetBlockHeight(), hashFork, pIndex->nFile, nTxOffset, vFork))
                        {
                            StdError("check", "UpdateBlockTx: Add tx fail, txid: %s, block: %s",
                                     block.vtx[i].GetHash().GetHex().c_str(), pIndex->GetBlockHash().GetHex().c_str());
                            return false;
                        }
                        nTxOffset += ss.GetSerializeSize(block.vtx[i]);
                    }
                }
                pIndex = pIndex->pNext;
            }
            if (it->first == hashGenesis)
            {
                nMainChainTxCount = checkFork.mapBlockTx.size();
            }
        }
    }
    return true;
}

bool CCheckBlockWalker::AddBlockTx(const CTransaction& txIn, const CTxContxt& contxtIn, int nHeight, const uint256& hashAtForkIn, uint32 nFileNoIn, uint32 nOffsetIn, const vector<uint256>& vFork)
{
    for (const uint256& hashFork : vFork)
    {
        map<uint256, CCheckBlockFork>::iterator it = mapCheckFork.find(hashFork);
        if (it != mapCheckFork.end())
        {
            if (!it->second.AddBlockTx(txIn, contxtIn, nHeight, hashAtForkIn, nFileNoIn, nOffsetIn))
            {
                StdError("check", "Block add tx: Add fail, txid: %s, fork: %s",
                         txIn.GetHash().GetHex().c_str(), hashFork.GetHex().c_str());
                return false;
            }
        }
    }
    return true;
}

CBlockIndex* CCheckBlockWalker::AddNewIndex(const uint256& hash, const CBlock& block, uint32 nFile, uint32 nOffset, uint256 nChainTrust)
{
    CBlockIndex* pIndexNew = new CBlockIndex(block, nFile, nOffset);
    if (pIndexNew != nullptr)
    {
        map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.insert(make_pair(hash, pIndexNew)).first;
        if (mi == mapBlockIndex.end())
        {
            StdError("check", "AddNewIndex: insert fail, block: %s", hash.GetHex().c_str());
            return nullptr;
        }
        pIndexNew->phashBlock = &((*mi).first);

        int64 nMoneySupply = block.GetBlockMint();
        uint64 nRandBeacon = block.GetBlockBeacon();
        CBlockIndex* pIndexPrev = nullptr;
        map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(block.hashPrev);
        if (miPrev != mapBlockIndex.end())
        {
            pIndexPrev = (*miPrev).second;
            pIndexNew->pPrev = pIndexPrev;
            if (!pIndexNew->IsOrigin())
            {
                pIndexNew->pOrigin = pIndexPrev->pOrigin;
                nRandBeacon ^= pIndexNew->pOrigin->nRandBeacon;
            }
            nMoneySupply += pIndexPrev->nMoneySupply;
            nChainTrust += pIndexPrev->nChainTrust;
        }
        pIndexNew->nMoneySupply = nMoneySupply;
        pIndexNew->nChainTrust = nChainTrust;
        pIndexNew->nRandBeacon = nRandBeacon;
    }
    else
    {
        StdError("check", "AddNewIndex: new fail, block: %s", hash.GetHex().c_str());
    }
    return pIndexNew;
}

CBlockIndex* CCheckBlockWalker::AddNewIndex(const uint256& hash, const CBlockOutline& objBlockOutline)
{
    CBlockIndex* pIndexNew = new CBlockIndex(static_cast<const CBlockIndex&>(objBlockOutline));
    if (pIndexNew != nullptr)
    {
        map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.insert(make_pair(hash, pIndexNew)).first;
        if (mi == mapBlockIndex.end())
        {
            StdError("check", "AddNewIndex: insert fail, block: %s", hash.GetHex().c_str());
            delete pIndexNew;
            return nullptr;
        }
        pIndexNew->phashBlock = &((*mi).first);

        if (objBlockOutline.hashPrev != 0)
        {
            map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(objBlockOutline.hashPrev);
            if (miPrev == mapBlockIndex.end())
            {
                StdError("check", "AddNewIndex: find prev fail, prev: %s", objBlockOutline.hashPrev.GetHex().c_str());
                mapBlockIndex.erase(hash);
                delete pIndexNew;
                return nullptr;
            }
            pIndexNew->pPrev = miPrev->second;
        }

        if (objBlockOutline.hashOrigin != 0)
        {
            map<uint256, CBlockIndex*>::iterator miOri = mapBlockIndex.find(objBlockOutline.hashOrigin);
            if (miOri == mapBlockIndex.end())
            {
                StdError("check", "AddNewIndex: find origin fail, origin: %s", objBlockOutline.hashOrigin.GetHex().c_str());
                mapBlockIndex.erase(hash);
                delete pIndexNew;
                return nullptr;
            }
            pIndexNew->pOrigin = miOri->second;
        }
    }
    else
    {
        StdError("check", "AddNewIndex: new fail, block: %s", hash.GetHex().c_str());
    }
    return pIndexNew;
}

void CCheckBlockWalker::ClearBlockIndex()
{
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.begin();
    for (; mi != mapBlockIndex.end(); ++mi)
    {
        delete (*mi).second;
    }
    mapBlockIndex.clear();
    mapBlock.clear();
}

bool CCheckBlockWalker::CheckTxExist(const uint256& hashFork, const uint256& txid, int& nHeight)
{
    map<uint256, CCheckBlockFork>::iterator it = mapCheckFork.find(hashFork);
    if (it == mapCheckFork.end())
    {
        StdError("check", "CheckTxExist: find fork fail, fork: %s, txid: %s", hashFork.GetHex().c_str(), txid.GetHex().c_str());
        return false;
    }
    return it->second.CheckTxExist(txid, nHeight);
}

bool CCheckBlockWalker::GetBlockWalletTx(const set<CDestination>& setAddress, vector<CWalletTx>& vWalletTx)
{
    map<uint256, CCheckBlockFork>::iterator mt = mapCheckFork.begin();
    for (; mt != mapCheckFork.end(); ++mt)
    {
        const uint256& hashFork = mt->first;
        CCheckBlockFork& checkFork = mt->second;
        if (checkFork.pOrigin == nullptr)
        {
            StdError("check", "GetBlockWalletTx: pOrigin is null, fork: %s", hashFork.GetHex().c_str());
            return false;
        }
        CBlockIndex* pBlockIndex = checkFork.pOrigin;
        while (pBlockIndex)
        {
            map<uint256, CBlockEx>::iterator it = mapBlock.find(pBlockIndex->GetBlockHash());
            if (it == mapBlock.end())
            {
                StdError("check", "GetBlockWalletTx: find block fail, block: %s, fork: %s", pBlockIndex->GetBlockHash().GetHex().c_str(), hashFork.GetHex().c_str());
                return false;
            }
            const CBlockEx& block = it->second;
            for (int i = 0; i < block.vtx.size(); i++)
            {
                //CAssembledTx(const CTransaction& tx, int nBlockHeightIn, const CDestination& destInIn = CDestination(), int64 nValueInIn = 0)
                //CWalletTx(const uint256& txidIn, const CAssembledTx& tx, const uint256& hashForkIn, bool fIsMine, bool fFromMe)

                bool fIsMine = (setAddress.find(block.vtx[i].sendTo) != setAddress.end());
                bool fFromMe = (setAddress.find(block.vTxContxt[i].destIn) != setAddress.end());
                if (fIsMine || fFromMe)
                {
                    CAssembledTx atx(block.vtx[i], block.GetBlockHeight(), block.vTxContxt[i].destIn, block.vTxContxt[i].GetValueIn());
                    CWalletTx wtx(block.vtx[i].GetHash(), atx, hashFork, fIsMine, fFromMe);
                    vWalletTx.push_back(wtx);
                }
            }
            if (!block.txMint.sendTo.IsNull() && setAddress.find(block.txMint.sendTo) != setAddress.end())
            {
                CAssembledTx atx(block.txMint, block.GetBlockHeight(), CDestination(), 0);
                CWalletTx wtx(block.txMint.GetHash(), atx, hashFork, true, false);
                vWalletTx.push_back(wtx);
            }
            pBlockIndex = pBlockIndex->pNext;
        }
    }
    return true;
}

bool CCheckBlockWalker::CheckBlockIndex()
{
    for (map<uint256, CBlockIndex*>::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); ++it)
    {
        if (!objBlockIndexWalker.CheckBlock(CBlockOutline(it->second)))
        {
            StdLog("check", "CheckBlockIndex: Find block index fail, add block index, block: %s.", it->first.GetHex().c_str());
            if (!fOnlyCheck)
            {
                dbBlockIndex.AddNewBlock(CBlockOutline(it->second));
            }
        }
    }
    for (map<uint256, CBlockOutline>::iterator it = objBlockIndexWalker.mapBlockIndex.begin(); it != objBlockIndexWalker.mapBlockIndex.end(); ++it)
    {
        const CBlockOutline& blockOut = it->second;
        if (mapBlockIndex.find(blockOut.hashBlock) == mapBlockIndex.end())
        {
            StdLog("check", "CheckBlockIndex: Find block hash fail, remove block index, block: %s.", blockOut.hashBlock.GetHex().c_str());
            if (!fOnlyCheck)
            {
                dbBlockIndex.RemoveBlock(blockOut.hashBlock);
            }
        }
    }
    return true;
}

bool CCheckBlockWalker::CheckRefBlock()
{
    auto nt = mapCheckFork.find(hashGenesis);
    if (nt == mapCheckFork.end())
    {
        StdError("check", "CheckRefBlock: find primary fork fail, hashGenesis: %s", hashGenesis.GetHex().c_str());
        return false;
    }
    if (nt->second.pLast == nullptr)
    {
        StdError("check", "CheckRefBlock: primary fork last is null, hashGenesis: %s", hashGenesis.GetHex().c_str());
        return false;
    }
    CBlockIndex* pPrimaryLast = nt->second.pLast;

    bool fCheckRet = true;
    map<uint256, CCheckBlockFork>::iterator mt = mapCheckFork.begin();
    for (; mt != mapCheckFork.end(); ++mt)
    {
        if (mt->first == hashGenesis)
        {
            continue;
        }
        const uint256& hashFork = mt->first;
        CCheckBlockFork& checkFork = mt->second;
        if (checkFork.pOrigin == nullptr)
        {
            StdError("check", "CheckRefBlock: pOrigin is null, fork: %s", hashFork.GetHex().c_str());
            return false;
        }
        CBlockIndex* pBlockIndex = checkFork.pOrigin;
        while (pBlockIndex)
        {
            map<uint256, CBlockEx>::iterator it = mapBlock.find(pBlockIndex->GetBlockHash());
            if (it == mapBlock.end())
            {
                StdError("check", "CheckRefBlock: find block fail, block: %s, fork: %s", pBlockIndex->GetBlockHash().GetHex().c_str(), hashFork.GetHex().c_str());
                return false;
            }
            const CBlockEx& block = it->second;

            if (block.IsSubsidiary() || block.IsExtended())
            {
                bool fRet = true;
                do
                {
                    CProofOfPiggyback proof;
                    if (!proof.Load(block.vchProof) || proof.hashRefBlock == 0)
                    {
                        StdError("check", "CheckRefBlock: subfork vchProof error, block: %s, fork: %s", pBlockIndex->GetBlockHash().GetHex().c_str(), hashFork.GetHex().c_str());
                        fRet = false;
                        break;
                    }
                    auto it = mapBlockIndex.find(proof.hashRefBlock);
                    if (it == mapBlockIndex.end())
                    {
                        StdError("check", "CheckRefBlock: find ref index fail, refblock: %s, block: %s, fork: %s",
                                 proof.hashRefBlock.GetHex().c_str(), pBlockIndex->GetBlockHash().GetHex().c_str(), hashFork.GetHex().c_str());
                        fRet = false;
                        break;
                    }
                    CBlockIndex* pRefIndex = it->second;
                    if (pRefIndex == nullptr)
                    {
                        StdError("check", "CheckRefBlock: ref index is null, refblock: %s, block: %s, fork: %s",
                                 proof.hashRefBlock.GetHex().c_str(), pBlockIndex->GetBlockHash().GetHex().c_str(), hashFork.GetHex().c_str());
                        fRet = false;
                        break;
                    }
                    if (!pRefIndex->IsPrimary())
                    {
                        StdError("check", "CheckRefBlock: ref block not is primary chain, refblock: %s, block: %s, fork: %s",
                                 proof.hashRefBlock.GetHex().c_str(), pBlockIndex->GetBlockHash().GetHex().c_str(), hashFork.GetHex().c_str());
                        fRet = false;
                        break;
                    }
                    if (pRefIndex != pPrimaryLast && pRefIndex->pNext == nullptr)
                    {
                        StdError("check", "CheckRefBlock: ref block is short chain, refblock: %s, block: %s, fork: %s",
                                 proof.hashRefBlock.GetHex().c_str(), pBlockIndex->GetBlockHash().GetHex().c_str(), hashFork.GetHex().c_str());
                        fRet = false;
                        break;
                    }
                } while (0);

                if (!fRet)
                {
                    fCheckRet = false;
                    if (!fOnlyCheck)
                    {
                        checkFork.pLast = pBlockIndex->pPrev;
                    }
                    break;
                }
            }
            pBlockIndex = pBlockIndex->pNext;
        }
    }
    if (!fCheckRet && fOnlyCheck)
    {
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////
// CCheckRepairData

bool CCheckRepairData::FetchBlockData()
{
    CTimeSeriesCached tsBlock;
    if (!tsBlock.Initialize(path(strDataPath) / "block", BLOCKFILE_PREFIX))
    {
        StdError("check", "tsBlock Initialize fail");
        return false;
    }

    if (!objBlockWalker.Initialize(strDataPath, &objForkManager))
    {
        StdError("check", "objBlockWalker Initialize fail");
        return false;
    }

    uint32 nLastFileRet = 0;
    uint32 nLastPosRet = 0;

    StdLog("check", "Fetch block and tx......");
    if (!tsBlock.WalkThrough(objBlockWalker, nLastFileRet, nLastPosRet, !fOnlyCheck))
    {
        StdError("check", "Fetch block and tx fail.");
        return false;
    }
    StdLog("check", "Fetch block and tx success, block: %ld.", objBlockWalker.nBlockCount);

    if (objBlockWalker.nBlockCount > 0)
    {
        if (objBlockWalker.hashGenesis == 0)
        {
            StdError("check", "Not genesis block");
            return false;
        }

        StdLog("check", "Update blockchain......");
        if (!objBlockWalker.UpdateBlockNext())
        {
            StdError("check", "Fetch block and tx, update block next fail");
            return false;
        }
        StdLog("check", "Update blockchain success, main chain height: %d.", objBlockWalker.nMainChainHeight);

        StdLog("check", "Check repair fork......");
        if (!objBlockWalker.CheckRepairFork())
        {
            StdError("check", "Fetch block and tx, check repair fork");
            return false;
        }
        StdLog("check", "Check repair fork success.");

        StdLog("check", "Check refblock......");
        if (!objBlockWalker.CheckRefBlock())
        {
            StdError("check", "Fetch block and tx, check refblock fail");
            return false;
        }
        StdLog("check", "Check refblock success, main chain height: %d.", objBlockWalker.nMainChainHeight);

        StdLog("check", "Update block tx......");
        if (!objBlockWalker.UpdateBlockTx())
        {
            StdError("check", "Fetch block and tx, update block tx fail");
            return false;
        }
        StdLog("check", "Update block tx success, main chain tx count: %ld.", objBlockWalker.nMainChainTxCount);
    }
    return true;
}

bool CCheckRepairData::FetchUnspent()
{
    CUnspentDB dbUnspent;
    if (!dbUnspent.Initialize(path(strDataPath)))
    {
        StdError("check", "FetchUnspent: dbUnspent Initialize fail");
        return false;
    }

    map<uint256, CCheckBlockFork>::iterator it = objBlockWalker.mapCheckFork.begin();
    for (; it != objBlockWalker.mapCheckFork.end(); ++it)
    {
        if (!dbUnspent.AddNewFork(it->first))
        {
            StdError("check", "FetchUnspent: dbUnspent AddNewFork fail.");
            dbUnspent.Deinitialize();
            return false;
        }
        if (!dbUnspent.WalkThrough(it->first, mapForkUnspentWalker[it->first]))
        {
            StdError("check", "FetchUnspent: dbUnspent WalkThrough fail.");
            dbUnspent.Deinitialize();
            return false;
        }
    }

    dbUnspent.Deinitialize();
    return true;
}

bool CCheckRepairData::FetchTxPool()
{
    map<uint256, CCheckBlockFork>::iterator it = objBlockWalker.mapCheckFork.begin();
    for (; it != objBlockWalker.mapCheckFork.end(); ++it)
    {
        objTxPoolData.AddForkUnspent(it->first, it->second.mapBlockUnspent);
    }
    if (!objTxPoolData.FetchTxPool(strDataPath))
    {
        StdError("check", "FetchTxPool: Fetch tx pool fail.");
        return false;
    }
    return true;
}

bool CCheckRepairData::FetchWalletAddress()
{
    CWalletDB dbWallet;
    if (!dbWallet.Initialize(path(strDataPath) / "wallet"))
    {
        StdLog("check", "dbWallet Initialize fail");
        return false;
    }
    if (!dbWallet.WalkThroughAddress(objWalletAddressWalker))
    {
        StdLog("check", "WalkThroughAddress fail");
        dbWallet.Deinitialize();
        return false;
    }
    dbWallet.Deinitialize();
    return true;
}

bool CCheckRepairData::FetchWalletTx()
{
    CWalletDB dbWallet;
    if (!dbWallet.Initialize(path(strDataPath) / "wallet"))
    {
        StdLog("check", "dbWallet Initialize fail");
        return false;
    }
    if (!dbWallet.WalkThroughTx(objWalletTxWalker))
    {
        StdLog("check", "WalkThroughTx fail");
        dbWallet.Deinitialize();
        return false;
    }
    dbWallet.Deinitialize();
    return true;
}

bool CCheckRepairData::CheckBlockUnspent()
{
    bool fCheckResult = true;
    map<uint256, CCheckBlockFork>::iterator mt = objBlockWalker.mapCheckFork.begin();
    for (; mt != objBlockWalker.mapCheckFork.end(); ++mt)
    {
        if (!mapForkUnspentWalker[mt->first].CheckForkUnspent(mt->second.mapBlockUnspent))
        {
            fCheckResult = false;
        }
    }
    return fCheckResult;
}

bool CCheckRepairData::CheckWalletTx(vector<CWalletTx>& vAddTx, vector<uint256>& vRemoveTx)
{
    //tx check
    {
        map<uint256, CCheckBlockFork>::iterator mt = objBlockWalker.mapCheckFork.begin();
        for (; mt != objBlockWalker.mapCheckFork.end(); ++mt)
        {
            const uint256& hashFork = mt->first;
            CCheckBlockFork& objBlockFork = mt->second;
            map<uint256, CCheckBlockTx>::iterator it = objBlockFork.mapBlockTx.begin();
            for (; it != objBlockFork.mapBlockTx.end(); ++it)
            {
                const uint256& txid = it->first;
                const CCheckBlockTx& cacheTx = it->second;
                if (cacheTx.hashAtFork == hashFork && cacheTx.tx.nAmount > 0)
                {
                    bool fIsMine = objWalletAddressWalker.CheckAddress(cacheTx.tx.sendTo);
                    bool fFromMe = objWalletAddressWalker.CheckAddress(cacheTx.txContxt.destIn);
                    if (fIsMine || fFromMe)
                    {
                        CCheckWalletTx* pWalletTx = objWalletTxWalker.GetWalletTx(hashFork, txid);
                        if (pWalletTx == nullptr)
                        {
                            StdLog("check", "CheckWalletTx: [block tx] find wallet tx fail, txid: %s, block height: %d, fork: %s",
                                   txid.GetHex().c_str(), cacheTx.txIndex.nBlockHeight, hashFork.GetHex().c_str());
                            //CAssembledTx(const CTransaction& tx, int nBlockHeightIn, const CDestination& destInIn = CDestination(), int64 nValueInIn = 0)
                            //CWalletTx(const uint256& txidIn, const CAssembledTx& tx, const uint256& hashForkIn, bool fIsMine, bool fFromMe)
                            CAssembledTx atx(cacheTx.tx, cacheTx.txIndex.nBlockHeight, cacheTx.txContxt.destIn, cacheTx.txContxt.GetValueIn());
                            CWalletTx wtx(txid, atx, hashFork, fIsMine, fFromMe);
                            objWalletTxWalker.AddWalletTx(wtx);
                            vAddTx.push_back(wtx);
                        }
                        else
                        {
                            if (pWalletTx->nBlockHeight != cacheTx.txIndex.nBlockHeight)
                            {
                                StdLog("check", "CheckWalletTx: [block tx] wallet tx height error, wtx height: %d, block height: %d, txid: %s",
                                       pWalletTx->nBlockHeight, cacheTx.txIndex.nBlockHeight, txid.GetHex().c_str());
                                pWalletTx->nBlockHeight = cacheTx.txIndex.nBlockHeight;
                                vAddTx.push_back(*pWalletTx);
                            }
                        }
                    }
                }
            }
        }
    }
    {
        map<uint256, CCheckForkTxPool>::iterator mt = objTxPoolData.mapForkTxPool.begin();
        for (; mt != objTxPoolData.mapForkTxPool.end(); ++mt)
        {
            const uint256& hashFork = mt->first;
            CCheckForkTxPool& objTxPoolFork = mt->second;
            map<uint256, CAssembledTx>::iterator it = objTxPoolFork.mapTxPoolTx.begin();
            for (; it != objTxPoolFork.mapTxPoolTx.end(); ++it)
            {
                const uint256& txid = it->first;
                const CAssembledTx& tx = it->second;
                bool fIsMine = objWalletAddressWalker.CheckAddress(tx.sendTo);
                bool fFromMe = objWalletAddressWalker.CheckAddress(tx.destIn);
                if (fIsMine || fFromMe)
                {
                    CCheckWalletTx* pWalletTx = objWalletTxWalker.GetWalletTx(hashFork, txid);
                    if (pWalletTx == nullptr)
                    {
                        StdLog("check", "CheckWalletTx: [pool tx] find wallet tx fail, txid: %s", txid.GetHex().c_str());
                        //CWalletTx(const uint256& txidIn, const CAssembledTx& tx, const uint256& hashForkIn, bool fIsMine, bool fFromMe)
                        CWalletTx wtx(txid, tx, hashFork, fIsMine, fFromMe);
                        objWalletTxWalker.AddWalletTx(wtx);
                        vAddTx.push_back(wtx);
                    }
                    else
                    {
                        if (pWalletTx->nBlockHeight != -1)
                        {
                            StdLog("check", "CheckWalletTx: [pool tx] wallet tx height error, wtx height: %d, txid: %s",
                                   pWalletTx->nBlockHeight, txid.GetHex().c_str());
                            pWalletTx->nBlockHeight = -1;
                            vAddTx.push_back(*pWalletTx);
                        }
                    }
                }
            }
        }
    }
    {
        map<uint256, CCheckWalletForkUnspent>::iterator mt = objWalletTxWalker.mapWalletFork.begin();
        for (; mt != objWalletTxWalker.mapWalletFork.end(); ++mt)
        {
            vector<pair<int, uint256>> vForkRemoveTx;
            const uint256& hashWalletFork = mt->first;
            CCheckWalletForkUnspent& objWalletFork = mt->second;
            map<uint256, CCheckWalletTx>::iterator it = objWalletFork.mapWalletTx.begin();
            for (; it != objWalletFork.mapWalletTx.end(); ++it)
            {
                CCheckWalletTx& wtx = it->second;
                if (wtx.hashFork == hashWalletFork)
                {
                    if (wtx.nBlockHeight >= 0)
                    {
                        int nBlockHeight = 0;
                        if (!objBlockWalker.CheckTxExist(wtx.hashFork, wtx.txid, nBlockHeight))
                        {
                            StdLog("check", "CheckWalletTx: [wallet tx] find block tx fail, txid: %s", wtx.txid.GetHex().c_str());
                            vForkRemoveTx.push_back(make_pair(wtx.nBlockHeight, wtx.txid));
                            vRemoveTx.push_back(wtx.txid);
                        }
                        else if (wtx.nBlockHeight != nBlockHeight)
                        {
                            StdLog("check", "CheckWalletTx: [wallet tx] block tx height error, wtx height: %d, block height: %d, txid: %s",
                                   wtx.nBlockHeight, nBlockHeight, wtx.txid.GetHex().c_str());
                            wtx.nBlockHeight = nBlockHeight;
                            vAddTx.push_back(wtx);
                        }
                    }
                    else
                    {
                        if (!objTxPoolData.CheckTxExist(wtx.hashFork, wtx.txid))
                        {
                            StdLog("check", "CheckWalletTx: [wallet tx] find txpool tx fail, txid: %s", wtx.txid.GetHex().c_str());
                            vForkRemoveTx.push_back(make_pair(wtx.nBlockHeight, wtx.txid));
                            vRemoveTx.push_back(wtx.txid);
                        }
                    }
                }
            }
            for (const auto& txd : vForkRemoveTx)
            {
                objWalletTxWalker.RemoveWalletTx(mt->first, txd.first, txd.second);
            }
        }
    }

    //update unspent
    if (!objWalletTxWalker.UpdateUnspent())
    {
        StdError("check", "CheckWalletTx UpdateUnspent fail.");
        return false;
    }

    //unspent check
    {
        map<uint256, CCheckWalletForkUnspent>::iterator mt = objWalletTxWalker.mapWalletFork.begin();
        for (; mt != objWalletTxWalker.mapWalletFork.end(); ++mt)
        {
            const uint256& hashFork = mt->first;
            CCheckWalletForkUnspent& fork = mt->second;
            map<CTxOutPoint, CCheckTxOut>::iterator it = fork.mapWalletUnspent.begin();
            for (; it != fork.mapWalletUnspent.end(); ++it)
            {
                if (!objTxPoolData.CheckTxPoolUnspent(hashFork, it->first, it->second))
                {
                    StdLog("check", "Check wallet unspent 1: spent fail, height: %d, utxo: [%d] %s.",
                           objWalletTxWalker.GetTxAtBlockHeight(hashFork, it->first.hash), it->first.n, it->first.hash.GetHex().c_str());
                    return false;
                }
            }
        }
    }
    {
        map<uint256, CCheckForkTxPool>::iterator mt = objTxPoolData.mapForkTxPool.begin();
        for (; mt != objTxPoolData.mapForkTxPool.end(); ++mt)
        {
            const uint256& hashFork = mt->first;
            CCheckForkTxPool& fork = mt->second;
            map<CTxOutPoint, CCheckTxOut>::iterator it = fork.mapTxPoolUnspent.begin();
            for (; it != fork.mapTxPoolUnspent.end(); ++it)
            {
                if (objWalletAddressWalker.CheckAddress(it->second.destTo))
                {
                    if (!objWalletTxWalker.CheckWalletUnspent(hashFork, it->first, it->second))
                    {
                        StdLog("check", "Check wallet unspent 2: check unspent fail, utxo: [%d] %s.", it->first.n, it->first.hash.GetHex().c_str());
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

bool CCheckRepairData::CheckTxIndex()
{
    CTxIndexDB dbTxIndex;
    if (!dbTxIndex.Initialize(path(strDataPath)))
    {
        StdLog("check", "dbTxIndex Initialize fail");
        return false;
    }

    map<uint256, vector<pair<uint256, CTxIndex>>> mapTxNew;
    map<uint256, CCheckBlockFork>::iterator mt = objBlockWalker.mapCheckFork.begin();
    for (; mt != objBlockWalker.mapCheckFork.end(); ++mt)
    {
        if (!dbTxIndex.LoadFork(mt->first))
        {
            StdLog("check", "dbTxIndex LoadFork fail");
            dbTxIndex.Deinitialize();
            return false;
        }
        CBlockIndex* pBlockIndex = mt->second.pLast;
        while (pBlockIndex)
        {
            const uint256& hashBlock = pBlockIndex->GetBlockHash();
            uint256 hashFork = pBlockIndex->GetOriginHash();
            if (hashFork == 0)
            {
                StdLog("check", "CheckTxIndex: fork is 0");
                pBlockIndex = pBlockIndex->pNext;
                continue;
            }
            map<uint256, CBlockEx>::iterator at = objBlockWalker.mapBlock.find(hashBlock);
            if (at == objBlockWalker.mapBlock.end())
            {
                StdLog("check", "CheckTxIndex: find block fail");
                return false;
            }
            const CBlockEx& block = at->second;
            if (!block.IsNull() && (!block.IsVacant() || !block.txMint.sendTo.IsNull()))
            {
                CBufStream ss;
                CTxIndex txIndex;

                uint32 nTxOffset = pBlockIndex->nOffset + block.GetTxSerializedOffset();
                if (!dbTxIndex.Retrieve(hashFork, block.txMint.GetHash(), txIndex))
                {
                    StdLog("check", "Retrieve db tx index fail, height: %d, block: %s, mint tx: %s.",
                           block.GetBlockHeight(), block.GetHash().GetHex().c_str(), block.txMint.GetHash().GetHex().c_str());

                    mapTxNew[hashFork].push_back(make_pair(block.txMint.GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                }
                else
                {
                    if (!(txIndex.nFile == pBlockIndex->nFile && txIndex.nOffset == nTxOffset))
                    {
                        StdLog("check", "Check tx index fail, height: %d, block: %s, mint tx: %s, db offset: %d, block offset: %d.",
                               block.GetBlockHeight(), block.GetHash().GetHex().c_str(),
                               block.txMint.GetHash().GetHex().c_str(), txIndex.nOffset, nTxOffset);

                        mapTxNew[hashFork].push_back(make_pair(block.txMint.GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                    }
                }
                nTxOffset += ss.GetSerializeSize(block.txMint);

                CVarInt var(block.vtx.size());
                nTxOffset += ss.GetSerializeSize(var);
                for (int i = 0; i < block.vtx.size(); i++)
                {
                    if (!dbTxIndex.Retrieve(pBlockIndex->GetOriginHash(), block.vtx[i].GetHash(), txIndex))
                    {
                        StdLog("check", "Retrieve db tx index fail, height: %d, block: %s, txid: %s.",
                               block.GetBlockHeight(), block.GetHash().GetHex().c_str(), block.vtx[i].GetHash().GetHex().c_str());

                        mapTxNew[hashFork].push_back(make_pair(block.vtx[i].GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                    }
                    else
                    {
                        if (!(txIndex.nFile == pBlockIndex->nFile && txIndex.nOffset == nTxOffset))
                        {
                            StdLog("check", "Check tx index fail, height: %d, block: %s, txid: %s, db offset: %d, block offset: %d.",
                                   block.GetBlockHeight(), block.GetHash().GetHex().c_str(), block.vtx[i].GetHash().GetHex().c_str(), txIndex.nOffset, nTxOffset);

                            mapTxNew[hashFork].push_back(make_pair(block.vtx[i].GetHash(), CTxIndex(block.GetBlockHeight(), pBlockIndex->nFile, nTxOffset)));
                        }
                    }
                    nTxOffset += ss.GetSerializeSize(block.vtx[i]);
                }
            }
            if (block.IsOrigin() || pBlockIndex == mt->second.pOrigin)
            {
                break;
            }
            pBlockIndex = pBlockIndex->pPrev;
        }
    }

    // repair
    if (!fOnlyCheck && !mapTxNew.empty())
    {
        StdLog("check", "Repair tx index starting");
        for (const auto& fork : mapTxNew)
        {
            if (!dbTxIndex.Update(fork.first, fork.second, vector<uint256>()))
            {
                StdLog("check", "Repair tx index update fail");
            }
            dbTxIndex.Flush(fork.first);
        }
        StdLog("check", "Repair tx index success");
    }

    dbTxIndex.Deinitialize();
    return true;
}

bool CCheckRepairData::RemoveTxPoolFile()
{
    CTxPoolData datTxPool;
    if (!datTxPool.Initialize(path(strDataPath)))
    {
        StdLog("check", "Remove txpool file: Failed to initialize txpool data");
        return false;
    }
    if (!datTxPool.Remove())
    {
        StdLog("check", "Remove txpool file: Remove failed");
        return false;
    }
    return true;
}

bool CCheckRepairData::RepairUnspent()
{
    CUnspentDB dbUnspent;
    if (!dbUnspent.Initialize(path(strDataPath)))
    {
        StdLog("check", "dbUnspent Initialize fail");
        return false;
    }

    map<uint256, CCheckForkUnspentWalker>::iterator it = mapForkUnspentWalker.begin();
    for (; it != mapForkUnspentWalker.end(); ++it)
    {
        if (!it->second.vAddUpdate.empty() || !it->second.vRemove.empty())
        {
            if (!dbUnspent.AddNewFork(it->first))
            {
                StdLog("check", "dbUnspent AddNewFork fail.");
                dbUnspent.Deinitialize();
                return false;
            }
            dbUnspent.RepairUnspent(it->first, it->second.vAddUpdate, it->second.vRemove);
        }
    }

    dbUnspent.Deinitialize();
    return true;
}

bool CCheckRepairData::RepairWalletTx(const vector<CWalletTx>& vAddTx, const vector<uint256>& vRemoveTx)
{
    CWalletDB dbWallet;
    if (!dbWallet.Initialize(path(strDataPath) / "wallet"))
    {
        StdLog("check", "dbWallet Initialize fail");
        return false;
    }
    if (!dbWallet.UpdateTx(vAddTx, vRemoveTx))
    {
        StdLog("check", "Wallet UpdateTx fail");
        dbWallet.Deinitialize();
        return false;
    }
    dbWallet.Deinitialize();
    return true;
}

bool CCheckRepairData::RestructureWalletTx()
{
    vector<CWalletTx> vAddTx;
    if (!objBlockWalker.GetBlockWalletTx(objWalletAddressWalker.setAddress, vAddTx))
    {
        StdLog("check", "Restructure wallet tx: Get block wallet tx fail");
        return false;
    }
    int64 nBlockWalletTxCount = vAddTx.size();
    StdLog("check", "Restructure wallet tx, block tx count: %ld", nBlockWalletTxCount);

    if (!objTxPoolData.GetTxPoolWalletTx(objWalletAddressWalker.setAddress, vAddTx))
    {
        StdLog("check", "Restructure wallet tx: Get txpool wallet tx fail");
        return false;
    }
    StdLog("check", "Restructure wallet tx, txpool tx count: %ld, total tx count: %ld",
           vAddTx.size() - nBlockWalletTxCount, vAddTx.size());

    CWalletDB dbWallet;
    if (!dbWallet.Initialize(path(strDataPath) / "wallet"))
    {
        StdLog("check", "Restructure wallet tx: dbWallet Initialize fail");
        return false;
    }

    dbWallet.ClearTx();
    if (!dbWallet.UpdateTx(vAddTx, vector<uint256>()))
    {
        StdLog("check", "Restructure wallet tx: Wallet UpdateTx fail");
        dbWallet.Deinitialize();
        return false;
    }

    dbWallet.Deinitialize();
    return true;
}

////////////////////////////////////////////////////////////////
bool CCheckRepairData::CheckRepairData()
{
    StdLog("check", "Start check and repair, path: %s", strDataPath.c_str());

    objWalletTxWalker.SetForkManager(&objForkManager);

    if (!objForkManager.SetParam(strDataPath, fTestnet, fOnlyCheck, objProofOfWorkParam.hashGenesisBlock))
    {
        StdLog("check", "Fork manager set param fail");
        return false;
    }
    if (!objForkManager.FetchForkStatus())
    {
        StdLog("check", "Fetch fork status fail");
        return false;
    }
    StdLog("check", "Fetch fork status success");

    if (!FetchBlockData())
    {
        StdLog("check", "Fetch block data fail");
        return false;
    }
    StdLog("check", "Fetch block data success");

    if (!FetchUnspent())
    {
        StdLog("check", "Fetch unspent fail");
        return false;
    }
    StdLog("check", "Fetch unspent success");

    if (!FetchTxPool())
    {
        StdLog("check", "Fetch txpool data fail");
        if (!fOnlyCheck)
        {
            if (!RemoveTxPoolFile())
            {
                StdLog("check", "Remove txpool file fail");
                return false;
            }
        }
    }
    StdLog("check", "Fetch txpool data success");

    if (!FetchWalletAddress())
    {
        StdLog("check", "Fetch wallet address fail");
        return false;
    }
    StdLog("check", "Fetch wallet address success");

    if (!FetchWalletTx())
    {
        StdLog("check", "Fetch wallet tx fail");
        return false;
    }
    StdLog("check", "Fetch wallet tx success, wallet tx count: %ld", objWalletTxWalker.nWalletTxCount);

    StdLog("check", "Check block unspent starting");
    if (!CheckBlockUnspent())
    {
        StdLog("check", "Check block unspent fail");
        if (!fOnlyCheck)
        {
            if (!RepairUnspent())
            {
                StdLog("check", "Repair unspent fail");
                return false;
            }
            StdLog("check", "Repair block unspent success");
        }
    }
    else
    {
        StdLog("check", "Check block unspent success");
    }

    StdLog("check", "Check wallet tx starting");
    vector<CWalletTx> vAddTx;
    vector<uint256> vRemoveTx;
    if (CheckWalletTx(vAddTx, vRemoveTx))
    {
        if (!vAddTx.empty() || !vRemoveTx.empty())
        {
            if (!fOnlyCheck)
            {
                StdLog("check", "Check wallet tx fail, start repair wallet tx, add tx: %ld, remove tx: %ld", vAddTx.size(), vRemoveTx.size());
                if (!RepairWalletTx(vAddTx, vRemoveTx))
                {
                    StdLog("check", "Repair wallet tx fail");
                    return false;
                }
                StdLog("check", "Repair wallet tx success");
            }
            else
            {
                StdLog("check", "Check wallet tx fail, add tx: %ld, remove tx: %ld", vAddTx.size(), vRemoveTx.size());
            }
        }
        else
        {
            StdLog("check", "Check wallet tx success");
        }
    }
    else
    {
        if (!fOnlyCheck)
        {
            StdLog("check", "Check wallet tx fail, start restructure wallet tx");
            if (!RestructureWalletTx())
            {
                StdLog("check", "Restructure wallet tx fail");
                return false;
            }
            StdLog("check", "Restructure wallet tx success");
        }
        else
        {
            StdLog("check", "Check wallet tx fail, need to restructure wallet tx");
        }
    }

    StdLog("check", "Check block index starting");
    if (!objBlockWalker.CheckBlockIndex())
    {
        StdLog("check", "Check block index fail");
        return false;
    }
    StdLog("check", "Check block index complete");

    StdLog("check", "Check tx index starting");
    if (!CheckTxIndex())
    {
        StdLog("check", "Check tx index fail");
        return false;
    }
    StdLog("check", "Check tx index complete");
    return true;
}

} // namespace bigbang
