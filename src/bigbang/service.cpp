// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "service.h"

#include "event.h"

using namespace std;
using namespace xengine;

extern void Shutdown();

namespace bigbang
{

//////////////////////////////
// CService

CService::CService()
  : pCoreProtocol(nullptr), pWorldLineCtrl(nullptr), pTxPoolCtrl(nullptr), pDispatcher(nullptr), pWallet(nullptr), pForkManager(nullptr)
{
}

CService::~CService()
{
}

bool CService::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        ERROR("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("worldlinecontroller", pWorldLineCtrl))
    {
        ERROR("Failed to request worldline");
        return false;
    }

    if (!GetObject("txpoolcontroller", pTxPoolCtrl))
    {
        ERROR("Failed to request txpool");
        return false;
    }

    if (!GetObject("dispatcher", pDispatcher))
    {
        ERROR("Failed to request dispatcher");
        return false;
    }

    if (!GetObject("wallet", pWallet))
    {
        ERROR("Failed to request wallet");
        return false;
    }

    if (!GetObject("forkmanager", pForkManager))
    {
        ERROR("Failed to request forkmanager");
        return false;
    }

    return true;
}

void CService::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pWorldLineCtrl = nullptr;
    pTxPoolCtrl = nullptr;
    pDispatcher = nullptr;
    pWallet = nullptr;
    pForkManager = nullptr;
}

bool CService::HandleInvoke()
{
    if (!StartActor())
    {
        ERROR("Failed to start actor");
        return false;
    }

    {
        boost::unique_lock<boost::shared_mutex> wlock(rwForkStatus);
        pWorldLineCtrl->GetForkStatus(mapForkStatus);
    }
    return true;
}

void CService::HandleHalt()
{
    StopActor();

    {
        boost::unique_lock<boost::shared_mutex> wlock(rwForkStatus);
        mapForkStatus.clear();
    }
}

void CService::NotifyWorldLineUpdate(const CWorldLineUpdate& update)
{
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwForkStatus);
        map<uint256, CForkStatus>::iterator it = mapForkStatus.find(update.hashFork);
        if (it == mapForkStatus.end())
        {
            it = mapForkStatus.insert(make_pair(update.hashFork, CForkStatus(update.hashFork, update.hashParent, update.nOriginHeight))).first;
            if (update.hashParent != 0)
            {
                mapForkStatus[update.hashParent].mapSubline.insert(make_pair(update.nOriginHeight, update.hashFork));
            }
        }

        CForkStatus& status = (*it).second;
        status.hashLastBlock = update.hashLastBlock;
        status.nLastBlockTime = update.nLastBlockTime;
        status.nLastBlockHeight = update.nLastBlockHeight;
        status.nMoneySupply = update.nMoneySupply;
    }
}

void CService::NotifyNetworkPeerUpdate(const CNetworkPeerUpdate& update)
{
    (void)update;
}

void CService::NotifyTransactionUpdate(const CTransactionUpdate& update)
{
    (void)update;
}

void CService::Stop()
{
    Shutdown();
}

int CService::GetPeerCount()
{
    std::promise<std::size_t> promiseCount;
    std::future<std::size_t> futureCount = promiseCount.get_future();
    auto spGetPeerCountMsg = CPeerNetGetCountMessage::Create(std::move(promiseCount));
    PUBLISH_MESSAGE(spGetPeerCountMsg);

    return futureCount.get();
}

void CService::GetPeers(vector<network::CBbPeerInfo>& vPeerInfo)
{
    vPeerInfo.clear();

    std::promise<boost::ptr_vector<CPeerInfo>> promiseResult;
    auto futureResult = promiseResult.get_future();
    auto spGetPeersMsg = CPeerNetGetPeersMessage::Create(std::move(promiseResult));
    PUBLISH_MESSAGE(spGetPeersMsg);

    auto resultValue = futureResult.get();
    vPeerInfo.reserve(resultValue.size());
    for (unsigned int i = 0; i < resultValue.size(); i++)
    {
        vPeerInfo.push_back(static_cast<network::CBbPeerInfo&>(resultValue[i]));
    }
}

bool CService::AddNode(const CNetHost& node)
{
    std::promise<bool> promiseRet;
    std::future<bool> futureRet = promiseRet.get_future();

    auto spAddNodeMsg = CPeerNetAddNodeMessage::Create(std::move(promiseRet));
    spAddNodeMsg->host = node;
    PUBLISH_MESSAGE(spAddNodeMsg);

    return futureRet.get();
}

bool CService::RemoveNode(const CNetHost& node)
{
    std::promise<bool> promiseRet;
    std::future<bool> futureRet = promiseRet.get_future();

    auto spRemoveNodeMsg = CPeerNetRemoveNodeMessage::Create(std::move(promiseRet));
    spRemoveNodeMsg->host = node;
    PUBLISH_MESSAGE(spRemoveNodeMsg);

    return futureRet.get();
}

int CService::GetForkCount()
{
    return mapForkStatus.size();
}

bool CService::HaveFork(const uint256& hashFork)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwForkStatus);

    map<uint256, CForkStatus>::iterator it = mapForkStatus.find(hashFork);
    if (it != mapForkStatus.end())
    {
        return true;
    }

    return false;
}

int CService::GetForkHeight(const uint256& hashFork)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwForkStatus);

    map<uint256, CForkStatus>::iterator it = mapForkStatus.find(hashFork);
    if (it != mapForkStatus.end())
    {
        return ((*it).second.nLastBlockHeight);
    }
    return 0;
}

void CService::ListFork(std::vector<std::pair<uint256, CProfile>>& vFork, bool fAll)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwForkStatus);
    if (fAll)
    {
        vector<uint256> vForkHash;
        pForkManager->GetForkList(vForkHash);
        vFork.reserve(vForkHash.size());
        for (vector<uint256>::iterator it = vForkHash.begin(); it != vForkHash.end(); ++it)
        {
            CForkContext ctx;
            if (pWorldLineCtrl->GetForkContext(*it, ctx))
            {
                vFork.push_back(make_pair(*it, ctx.GetProfile()));
            }
        }
    }
    else
    {
        vFork.reserve(mapForkStatus.size());
        for (map<uint256, CForkStatus>::iterator it = mapForkStatus.begin(); it != mapForkStatus.end(); ++it)
        {
            CProfile profile;
            if (pWorldLineCtrl->GetForkProfile((*it).first, profile))
            {
                vFork.push_back(make_pair((*it).first, profile));
            }
        }
    }
}

bool CService::GetForkGenealogy(const uint256& hashFork, vector<pair<uint256, int>>& vAncestry, vector<pair<int, uint256>>& vSubline)
{
    boost::shared_lock<boost::shared_mutex> rlock(rwForkStatus);

    uint256 hashParent, hashJoint;
    int nJointHeight;
    if (!pForkManager->GetJoint(hashFork, hashParent, hashJoint, nJointHeight))
    {
        return false;
    }

    while (hashParent != 0)
    {
        vAncestry.push_back(make_pair(hashParent, nJointHeight));
        pForkManager->GetJoint(hashParent, hashParent, hashJoint, nJointHeight);
    }

    pForkManager->GetSubline(hashFork, vSubline);
    return true;
}

bool CService::GetBlockLocation(const uint256& hashBlock, uint256& hashFork, int& nHeight)
{
    return pWorldLineCtrl->GetBlockLocation(hashBlock, hashFork, nHeight);
}

int CService::GetBlockCount(const uint256& hashFork)
{
    if (hashFork == pCoreProtocol->GetGenesisBlockHash())
    {
        return (GetForkHeight(hashFork) + 1);
    }
    return pWorldLineCtrl->GetBlockCount(hashFork);
}

bool CService::GetBlockHash(const uint256& hashFork, int nHeight, uint256& hashBlock)
{
    if (nHeight < 0)
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwForkStatus);
        map<uint256, CForkStatus>::iterator it = mapForkStatus.find(hashFork);
        if (it == mapForkStatus.end())
        {
            return false;
        }
        hashBlock = (*it).second.hashLastBlock;
        return true;
    }
    return pWorldLineCtrl->GetBlockHash(hashFork, nHeight, hashBlock);
}

bool CService::GetBlockHash(const uint256& hashFork, int nHeight, vector<uint256>& vBlockHash)
{
    return pWorldLineCtrl->GetBlockHash(hashFork, nHeight, vBlockHash);
}

bool CService::GetBlock(const uint256& hashBlock, CBlock& block, uint256& hashFork, int& nHeight)
{
    return pWorldLineCtrl->GetBlock(hashBlock, block)
           && pWorldLineCtrl->GetBlockLocation(hashBlock, hashFork, nHeight);
}

bool CService::GetBlockEx(const uint256& hashBlock, CBlockEx& block, uint256& hashFork, int& nHeight)
{
    return pWorldLineCtrl->GetBlockEx(hashBlock, block)
           && pWorldLineCtrl->GetBlockLocation(hashBlock, hashFork, nHeight);
}

void CService::GetTxPool(const uint256& hashFork, vector<pair<uint256, size_t>>& vTxPool)
{
    vTxPool.clear();
    pTxPoolCtrl->ListTx(hashFork, vTxPool);
}

bool CService::GetTransaction(const uint256& txid, CTransaction& tx, uint256& hashFork, int& nHeight)
{
    if (pTxPoolCtrl->Get(txid, tx))
    {
        int nAnchorHeight;
        if (!pWorldLineCtrl->GetBlockLocation(tx.hashAnchor, hashFork, nAnchorHeight))
        {
            return false;
        }
        nHeight = -1;
        return true;
    }
    if (!pWorldLineCtrl->GetTransaction(txid, tx))
    {
        return false;
    }
    return pWorldLineCtrl->GetTxLocation(txid, hashFork, nHeight);
}

bool CService::SendTransaction(CNoncePtr spNonce, uint256& hashFork, const CTransaction& tx)
{
    if (!hashFork)
    {
        int nAnchorHeight;
        uint256 hashFork;
        if (!pWorldLineCtrl->GetBlockLocation(tx.hashAnchor, hashFork, nAnchorHeight))
        {
            return false;
        }
    }

    auto spAddTxMsg = CAddedTxMessage::Create();
    spAddTxMsg->spNonce = spNonce;
    spAddTxMsg->hashFork = hashFork;
    spAddTxMsg->tx = tx;
    return true;
}

bool CService::RemovePendingTx(const uint256& txid)
{
    if (!pTxPoolCtrl->Exists(txid))
    {
        return false;
    }
    pTxPoolCtrl->Pop(txid);
    return true;
}

bool CService::HaveKey(const crypto::CPubKey& pubkey)
{
    return pWallet->Have(pubkey);
}

void CService::GetPubKeys(set<crypto::CPubKey>& setPubKey)
{
    pWallet->GetPubKeys(setPubKey);
}

bool CService::GetKeyStatus(const crypto::CPubKey& pubkey, int& nVersion, bool& fLocked, int64& nAutoLockTime)
{
    return pWallet->GetKeyStatus(pubkey, nVersion, fLocked, nAutoLockTime);
}

bool CService::MakeNewKey(const crypto::CCryptoString& strPassphrase, crypto::CPubKey& pubkey)
{
    crypto::CKey key;
    if (!key.Renew())
    {
        return false;
    }
    if (!strPassphrase.empty())
    {
        if (!key.Encrypt(strPassphrase))
        {
            return false;
        }
        key.Lock();
    }
    pubkey = key.GetPubKey();
    return pWallet->AddKey(key);
}

bool CService::AddKey(const crypto::CKey& key)
{
    return pWallet->AddKey(key);
}

bool CService::ImportKey(const vector<unsigned char>& vchKey, crypto::CPubKey& pubkey)
{
    return pWallet->Import(vchKey, pubkey);
}

bool CService::ExportKey(const crypto::CPubKey& pubkey, vector<unsigned char>& vchKey)
{
    return pWallet->Export(pubkey, vchKey);
}

bool CService::EncryptKey(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase,
                          const crypto::CCryptoString& strCurrentPassphrase)
{
    return pWallet->Encrypt(pubkey, strPassphrase, strCurrentPassphrase);
}

bool CService::Lock(const crypto::CPubKey& pubkey)
{
    return pWallet->Lock(pubkey);
}

bool CService::Unlock(const crypto::CPubKey& pubkey, const crypto::CCryptoString& strPassphrase, int64 nTimeout)
{
    return pWallet->Unlock(pubkey, strPassphrase, nTimeout);
}

bool CService::SignSignature(const crypto::CPubKey& pubkey, const uint256& hash, vector<unsigned char>& vchSig)
{
    return pWallet->Sign(pubkey, hash, vchSig);
}

bool CService::SignTransaction(CTransaction& tx, bool& fCompleted)
{
    uint256 hashFork;
    int nHeight;
    if (!pWorldLineCtrl->GetBlockLocation(tx.hashAnchor, hashFork, nHeight))
    {
        return false;
    }
    vector<CTxOut> vUnspent;
    if (!pTxPoolCtrl->FetchInputs(hashFork, tx, vUnspent) || vUnspent.empty())
    {
        return false;
    }

    const CDestination& destIn = vUnspent[0].destTo;
    if (!pWallet->SignTransaction(destIn, tx, fCompleted))
    {
        return false;
    }
    return (!fCompleted
            || (pCoreProtocol->ValidateTransaction(tx) == OK
                && pCoreProtocol->VerifyTransaction(tx, vUnspent, nHeight, hashFork) == OK));
}

bool CService::HaveTemplate(const CTemplateId& tid)
{
    return pWallet->Have(tid);
}

void CService::GetTemplateIds(set<CTemplateId>& setTid)
{
    pWallet->GetTemplateIds(setTid);
}

bool CService::AddTemplate(CTemplatePtr& ptr)
{
    return pWallet->AddTemplate(ptr);
}

CTemplatePtr CService::GetTemplate(const CTemplateId& tid)
{
    return pWallet->GetTemplate(tid);
}

bool CService::GetBalance(const CDestination& dest, const uint256& hashFork, CWalletBalance& balance)
{
    int nForkHeight = GetForkHeight(hashFork);
    if (nForkHeight <= 0)
    {
        return false;
    }
    return pWallet->GetBalance(dest, hashFork, nForkHeight, balance);
}

bool CService::ListWalletTx(int nOffset, int nCount, vector<CWalletTx>& vWalletTx)
{
    if (nOffset < 0)
    {
        nOffset = pWallet->GetTxCount() - nCount;
        if (nOffset < 0)
        {
            nOffset = 0;
        }
    }
    return pWallet->ListTx(nOffset, nCount, vWalletTx);
}

bool CService::CreateTransaction(const uint256& hashFork, const CDestination& destFrom,
                                 const CDestination& destSendTo, int64 nAmount, int64 nTxFee,
                                 const vector<unsigned char>& vchData, CTransaction& txNew)
{
    int nForkHeight = 0;
    txNew.SetNull();
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwForkStatus);
        map<uint256, CForkStatus>::iterator it = mapForkStatus.find(hashFork);
        if (it == mapForkStatus.end())
        {
            return false;
        }
        nForkHeight = it->second.nLastBlockHeight;
        txNew.hashAnchor = it->second.hashLastBlock;
    }
    txNew.nType = CTransaction::TX_TOKEN;
    txNew.nTimeStamp = GetNetTime();
    txNew.nLockUntil = 0;
    txNew.sendTo = destSendTo;
    txNew.nAmount = nAmount;
    txNew.nTxFee = nTxFee;
    txNew.vchData = vchData;

    return pWallet->ArrangeInputs(destFrom, hashFork, nForkHeight, txNew);
}

bool CService::SynchronizeWalletTx(const CDestination& destNew)
{
    return pWallet->SynchronizeWalletTx(destNew);
}

bool CService::ResynchronizeWalletTx()
{
    return pWallet->ResynchronizeWalletTx();
}

bool CService::GetWork(vector<unsigned char>& vchWorkData, int& nPrevBlockHeight, uint256& hashPrev, uint32& nPrevTime, int& nAlgo, int& nBits, CTemplateMintPtr& templMint)
{
    CBlock block;
    block.nType = CBlock::BLOCK_PRIMARY;

    {
        boost::shared_lock<boost::shared_mutex> rlock(rwForkStatus);
        map<uint256, CForkStatus>::iterator it = mapForkStatus.find(pCoreProtocol->GetGenesisBlockHash());
        if (it == mapForkStatus.end())
        {
            return false;
        }
        hashPrev = (*it).second.hashLastBlock;
        nPrevTime = (*it).second.nLastBlockTime;
        nPrevBlockHeight = (*it).second.nLastBlockHeight;
        block.hashPrev = hashPrev;
        block.nTimeStamp = nPrevTime + BLOCK_TARGET_SPACING - 10;
    }

    nAlgo = CM_CRYPTONIGHT;
    int64 nReward;
    if (!pWorldLineCtrl->GetProofOfWorkTarget(block.hashPrev, nAlgo, nBits, nReward))
    {
        return false;
    }

    CProofOfHashWork proof;
    proof.nWeight = 0;
    proof.nAgreement = 0;
    proof.nAlgo = nAlgo;
    proof.nBits = nBits;
    proof.destMint = CDestination(templMint->GetTemplateId());
    proof.nNonce = 0;
    proof.Save(block.vchProof);

    block.GetSerializedProofOfWorkData(vchWorkData);
    return true;
}

Errno CService::SubmitWork(const vector<unsigned char>& vchWorkData, CTemplateMintPtr& templMint, crypto::CKey& keyMint, CBlock& block)
{
    if (vchWorkData.empty())
    {
        return FAILED;
    }
    CProofOfHashWorkCompact proof;
    CBufStream ss;
    ss.Write((const char*)&vchWorkData[0], vchWorkData.size());
    try
    {
        ss >> block.nVersion >> block.nType >> block.nTimeStamp >> block.hashPrev >> block.vchProof;
        proof.Load(block.vchProof);
        if (proof.nAlgo != CM_CRYPTONIGHT)
        {
            return ERR_BLOCK_PROOF_OF_WORK_INVALID;
        }
    }
    catch (exception& e)
    {
        ERROR(e.what());
        return FAILED;
    }
    int nBits;
    int64 nReward;
    if (!pWorldLineCtrl->GetProofOfWorkTarget(block.hashPrev, proof.nAlgo, nBits, nReward))
    {
        return FAILED;
    }

    CTransaction& txMint = block.txMint;
    txMint.nType = CTransaction::TX_WORK;
    txMint.nTimeStamp = block.nTimeStamp;
    txMint.hashAnchor = block.hashPrev;
    txMint.sendTo = CDestination(templMint->GetTemplateId());
    txMint.nAmount = nReward;

    size_t nSigSize = templMint->GetTemplateData().size() + 64 + 2;
    size_t nMaxTxSize = MAX_BLOCK_SIZE - GetSerializeSize(block) - nSigSize;
    int64 nTotalTxFee = 0;
    pTxPoolCtrl->ArrangeBlockTx(pCoreProtocol->GetGenesisBlockHash(), block.nTimeStamp, nMaxTxSize, block.vtx, nTotalTxFee);
    block.hashMerkle = block.CalcMerkleTreeRoot();
    block.txMint.nAmount += nTotalTxFee;

    uint256 hashBlock = block.GetHash();
    vector<unsigned char> vchMintSig;
    if (!keyMint.Sign(hashBlock, vchMintSig)
        || !templMint->BuildBlockSignature(hashBlock, vchMintSig, block.vchSig))
    {
        return ERR_BLOCK_SIGNATURE_INVALID;
    }

    Errno err = pCoreProtocol->ValidateBlock(block);
    if (err != OK)
    {
        return err;
    }

    return OK;
}

bool CService::SendBlock(CNoncePtr spNonce, const uint256& hashFork, const uint256 blockHash, const CBlock& block)
{
    auto spAddBlockMsg = CAddBlockMessage::Create();
    spAddBlockMsg->spNonce = spNonce;
    spAddBlockMsg->hashFork = hashFork;
    spAddBlockMsg->block = block;
    PUBLISH_MESSAGE(spAddBlockMsg);

    return true;
}

} // namespace bigbang
