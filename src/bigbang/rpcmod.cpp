// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcmod.h"

#include "json/json_spirit_reader_template.h"
#include <boost/assign/list_of.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/regex.hpp>
#include <regex>

#include "address.h"
#include "rpc/auto_protocol.h"
#include "template/proof.h"
#include "template/template.h"
#include "version.h"

using namespace std;
using namespace xengine;
using namespace json_spirit;
using namespace bigbang::rpc;
using namespace bigbang;
namespace fs = boost::filesystem;

///////////////////////////////
// static function

static int64 AmountFromValue(const double dAmount)
{
    if (dAmount <= 0.0 || dAmount > MAX_MONEY)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid amount");
    }
    int64 nAmount = (int64)(dAmount * COIN + 0.5);
    if (!MoneyRange(nAmount))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid amount");
    }
    return nAmount;
}

static double ValueFromAmount(int64 amount)
{
    return ((double)amount / (double)COIN);
}

static CBlockData BlockToJSON(const uint256& hashBlock, const CBlock& block, const uint256& hashFork, int nHeight)
{
    CBlockData data;
    data.strHash = hashBlock.GetHex();
    data.nVersion = block.nVersion;
    data.strType = GetBlockTypeStr(block.nType, block.txMint.nType);
    data.nTime = block.GetBlockTime();
    if (block.hashPrev != 0)
    {
        data.strPrev = block.hashPrev.GetHex();
    }
    data.strFork = hashFork.GetHex();
    data.nHeight = nHeight;

    data.strTxmint = block.txMint.GetHash().GetHex();
    for (const CTransaction& tx : block.vtx)
    {
        data.vecTx.push_back(tx.GetHash().GetHex());
    }
    return data;
}

static CTransactionData TxToJSON(const uint256& txid, const CTransaction& tx, const uint256& hashFork, int nDepth)
{
    CTransactionData ret;
    ret.strTxid = txid.GetHex();
    ret.nVersion = tx.nVersion;
    ret.strType = tx.GetTypeString();
    ret.nTime = tx.nTimeStamp;
    ret.nLockuntil = tx.nLockUntil;
    ret.strAnchor = tx.hashAnchor.GetHex();
    for (const CTxIn& txin : tx.vInput)
    {
        CTransactionData::CVin vin;
        vin.nVout = txin.prevout.n;
        vin.strTxid = txin.prevout.hash.GetHex();
        ret.vecVin.push_back(move(vin));
    }
    ret.strSendto = CAddress(tx.sendTo).ToString();
    ret.fAmount = ValueFromAmount(tx.nAmount);
    ret.fTxfee = ValueFromAmount(tx.nTxFee);

    ret.strData = xengine::ToHexString(tx.vchData);
    ret.strSig = xengine::ToHexString(tx.vchSig);
    ret.strFork = hashFork.GetHex();
    if (nDepth >= 0)
    {
        ret.nConfirmations = nDepth;
    }

    return ret;
}

static CWalletTxData WalletTxToJSON(const CWalletTx& wtx)
{
    CWalletTxData data;
    data.strTxid = wtx.txid.GetHex();
    data.strFork = wtx.hashFork.GetHex();
    if (wtx.nBlockHeight >= 0)
    {
        data.nBlockheight = wtx.nBlockHeight;
    }
    data.strType = wtx.GetTypeString();
    data.nTime = (boost::int64_t)wtx.nTimeStamp;
    data.fSend = wtx.IsFromMe();
    if (!wtx.IsMintTx())
    {
        data.strFrom = CAddress(wtx.destIn).ToString();
    }
    data.strTo = CAddress(wtx.sendTo).ToString();
    data.fAmount = ValueFromAmount(wtx.nAmount);
    data.fFee = ValueFromAmount(wtx.nTxFee);
    data.nLockuntil = (boost::int64_t)wtx.nLockUntil;
    return data;
}

namespace bigbang
{

///////////////////////////////
// CRPCMod

CRPCMod::CRPCMod()
  : CIOActor("rpcmod")
{
    pHttpServer = nullptr;
    pCoreProtocol = nullptr;
    pService = nullptr;
    pDataStat = nullptr;

    std::map<std::string, RPCFunc> temp_map = boost::assign::map_list_of
        /* System */
        ("help", &CRPCMod::RPCHelp)("stop", &CRPCMod::RPCStop)("version", &CRPCMod::RPCVersion)
        /* Network */
        ("getpeercount", &CRPCMod::RPCGetPeerCount)("listpeer", &CRPCMod::RPCListPeer)("addnode", &CRPCMod::RPCAddNode)("removenode", &CRPCMod::RPCRemoveNode)
        /* WorldLine & TxPool */
        ("getforkcount", &CRPCMod::RPCGetForkCount)("listfork", &CRPCMod::RPCListFork)("getgenealogy", &CRPCMod::RPCGetForkGenealogy)("getblocklocation", &CRPCMod::RPCGetBlockLocation)("getblockcount", &CRPCMod::RPCGetBlockCount)("getblockhash", &CRPCMod::RPCGetBlockHash)("getblock", &CRPCMod::RPCGetBlock)("gettxpool", &CRPCMod::RPCGetTxPool)("gettransaction", &CRPCMod::RPCGetTransaction)("sendtransaction", &CRPCMod::RPCSendTransaction)("getforkheight", &CRPCMod::RPCGetForkHeight)
        /* Wallet */
        ("listkey", &CRPCMod::RPCListKey)("getnewkey", &CRPCMod::RPCGetNewKey)("encryptkey", &CRPCMod::RPCEncryptKey)("lockkey", &CRPCMod::RPCLockKey)("unlockkey", &CRPCMod::RPCUnlockKey)("importprivkey", &CRPCMod::RPCImportPrivKey)("importkey", &CRPCMod::RPCImportKey)("exportkey", &CRPCMod::RPCExportKey)("addnewtemplate", &CRPCMod::RPCAddNewTemplate)("importtemplate", &CRPCMod::RPCImportTemplate)("exporttemplate", &CRPCMod::RPCExportTemplate)("validateaddress", &CRPCMod::RPCValidateAddress)("resyncwallet", &CRPCMod::RPCResyncWallet)("getbalance", &CRPCMod::RPCGetBalance)("listtransaction", &CRPCMod::RPCListTransaction)("sendfrom", &CRPCMod::RPCSendFrom)("createtransaction", &CRPCMod::RPCCreateTransaction)("signtransaction", &CRPCMod::RPCSignTransaction)("signmessage", &CRPCMod::RPCSignMessage)("listaddress", &CRPCMod::RPCListAddress)("exportwallet", &CRPCMod::RPCExportWallet)("importwallet", &CRPCMod::RPCImportWallet)("makeorigin", &CRPCMod::RPCMakeOrigin)
        /* Util */
        ("verifymessage", &CRPCMod::RPCVerifyMessage)("makekeypair", &CRPCMod::RPCMakeKeyPair)("getpubkeyaddress", &CRPCMod::RPCGetPubKeyAddress)("gettemplateaddress", &CRPCMod::RPCGetTemplateAddress)("maketemplate", &CRPCMod::RPCMakeTemplate)("decodetransaction", &CRPCMod::RPCDecodeTransaction)
        /* Mint */
        ("getwork", &CRPCMod::RPCGetWork)("submitwork", &CRPCMod::RPCSubmitWork)
        /* tool */
        ("querystat", &CRPCMod::RPCQueryStat);

    mapRPCFunc = temp_map;
    mapRPCMessageFunc["submitwork"] = &CRPCMod::RPCMsgSubmitWork;
}

CRPCMod::~CRPCMod()
{
}

bool CRPCMod::HandleInitialize()
{
    if (!GetObject("httpserver", pHttpServer))
    {
        ERROR("Failed to request httpserver");
        return false;
    }

    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        ERROR("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("service", pService))
    {
        ERROR("Failed to request service");
        return false;
    }

    if (!GetObject("datastat", pDataStat))
    {
        ERROR("Failed to request datastat");
        return false;
    }

    RegisterRefHandler<CHttpReqMessage>(boost::bind(&CRPCMod::HandleHttpReq, this, _1));
    RegisterRefHandler<CHttpBrokenMessage>(boost::bind(&CRPCMod::HandleHttpBroken, this, _1));
    RegisterRefHandler<CAddedTxMessage>(boost::bind(&CRPCMod::HandleAddedTxMsg, this, _1));
    RegisterRefHandler<CAddedBlockMessage>(boost::bind(&CRPCMod::HandleAddedBlockMsg, this, _1));

    return true;
}

bool CRPCMod::HandleInvoke()
{
    if (!StartActor())
    {
        ERROR("Failed to start actor");
        return false;
    }

    return true;
}

void CRPCMod::HandleHalt()
{
    StopActor();
}

void CRPCMod::HandleDeinitialize()
{
    pHttpServer = nullptr;
    pCoreProtocol = nullptr;
    pService = nullptr;
    pDataStat = nullptr;

    DeregisterHandler(CHttpReqMessage::MessageType());
    DeregisterHandler(CHttpBrokenMessage::MessageType());
    DeregisterHandler(CAddedTxMessage::MessageType());
    DeregisterHandler(CAddedBlockMessage::MessageType());
}

bool CRPCMod::EnterLoop()
{
    return true;
}

void CRPCMod::LeaveLoop()
{
}

void CRPCMod::HandleHttpReq(const CHttpReqMessage& msg)
{
    INFO("CRPCMod::HandleHttpReq");
    auto workPair = mapWork.insert(make_pair(msg.spNonce, CWork()));
    // Repeated request
    if (!workPair.second)
    {
        auto spError = MakeCRPCErrorPtr(RPC_INVALID_REQUEST, "Repeated request");
        CRPCResp resp(spError->valData, spError);
        JsonReply(msg.spNonce, resp.Serialize());
        return;
    }
    CWork& work = workPair.first->second;

    string strResult;
    try
    {
        // check version
        if (msg.mapHeader.count("url"))
        {
            string strVersion = msg.mapHeader.at("url").substr(1);
            if (!strVersion.empty() && !CheckVersion(strVersion))
            {
                throw CRPCException(RPC_VERSION_OUT_OF_DATE,
                                    string("Out of date version. Server version is v") + VERSION_STR
                                        + ", but client version is v" + strVersion);
            }
        }

        work.vecReq = DeserializeCRPCReq(msg.strContent, work.fArray);
        work.nRemainder = work.vecReq.size();
        work.vecResp.resize(work.nRemainder);
    }
    catch (CRPCException& e)
    {
        auto spError = MakeCRPCErrorPtr(e);
        CRPCResp resp(e.valData, spError);
        strResult = resp.Serialize();
    }
    catch (exception& e)
    {
        ERROR("RPC request other error: %s", e.what());
        auto spError = MakeCRPCErrorPtr(RPC_MISC_ERROR, e.what());
        CRPCResp resp(Value(), spError);
        strResult = resp.Serialize();
    }
    TRACE("response : %s", MaskSensitiveData(strResult).c_str());

    // no result means no return
    if (!strResult.empty())
    {
        JsonReply(msg.spNonce, strResult);
    }

    for (size_t i = 0; i < work.vecReq.size(); i++)
    {
        auto spResp = StartWork(msg.spNonce, work.vecReq[i]);
        CompletedWork(msg.spNonce, i, spResp);
    }
}

void CRPCMod::HandleHttpBroken(const CHttpBrokenMessage& msg)
{
    mapWork.erase(msg.spNonce);
}

void CRPCMod::HandleAddedBlockMsg(const CAddedBlockMessage& msg)
{
    HandleAddedMsg(msg.spNonce, 0, msg.block.GetHash(), msg);
}

void CRPCMod::HandleAddedTxMsg(const CAddedTxMessage& msg)
{
    HandleAddedMsg(msg.spNonce, 0, msg.tx.GetHash(), msg);
}

CRPCRespPtr CRPCMod::StartWork(CNoncePtr spNonce, CRPCReqPtr spReq)
{
    CRPCErrorPtr spError;
    CRPCResultPtr spResult;
    try
    {
        map<string, RPCFunc>::iterator it = mapRPCFunc.find(spReq->strMethod);
        if (it == mapRPCFunc.end())
        {
            throw CRPCException(RPC_METHOD_NOT_FOUND, "Method not found");
        }
        TRACE("request : %s", MaskSensitiveData(spReq->Serialize()).c_str());
        spResult = (this->*(*it).second)(spNonce, spReq->spParam);
    }
    catch (CRPCException& e)
    {
        spError = CRPCErrorPtr(new CRPCError(e));
    }
    catch (exception& e)
    {
        spError = CRPCErrorPtr(new CRPCError(RPC_MISC_ERROR, e.what()));
    }

    if (spError)
    {
        TRACE("Work returns error: %s", spError->Serialize().c_str());
        return MakeCRPCRespPtr(spReq->valID, spError);
    }
    else if (spResult)
    {
        TRACE("Work return result: %s", spResult->Serialize().c_str());
        return MakeCRPCRespPtr(spReq->valID, spResult);
    }
    else
    {
        TRACE("Work return null, method: %s", spReq->strMethod.c_str());
        return nullptr;
    }
}

void CRPCMod::HandleAddedMsg(CNoncePtr spNonce, size_t nIndex, const uint256& hash, const CMessage& msg)
{
    if (!NonceType(spNonce->nNonce, HTTP_NONCE_TYPE))
    {
        return;
    }

    // find work
    auto workIt = mapWork.find(spNonce);
    if (workIt == mapWork.end())
    {
        return;
    }
    CWork& work = workIt->second;

    // find index by hash
    if (!hash)
    {
        auto hashIt = work.mapHash.find(hash);
        if (hashIt == work.mapHash.end())
        {
            return;
        }

        nIndex = hashIt->second;
        work.mapHash.erase(hashIt);
    }

    // Call concrete message function
    TRACE("complete work req size (%u), index (%u)", work.vecReq.size(), nIndex);
    auto& spReq = work.vecReq[nIndex];
    CRPCRespPtr spResp;
    auto funIt = mapRPCMessageFunc.find(spReq->strMethod);
    if (funIt == mapRPCMessageFunc.end())
    {
        auto spError = CRPCErrorPtr(new CRPCError(RPC_METHOD_NOT_FOUND, string("RPC Message function (") + spReq->strMethod + ") not found"));
        spResp = MakeCRPCRespPtr(spReq->valID, spError);
    }
    else
    {
        auto spResult = (this->*(*funIt).second)(msg);
        spResp = MakeCRPCRespPtr(spReq->valID, spResult);
    }
    TRACE("complete work call function end");

    CompletedWork(spNonce, nIndex, spResp);
}

void CRPCMod::CompletedWork(xengine::CNoncePtr spNonce, size_t nIndex, rpc::CRPCRespPtr spResp)
{
    if (!spResp)
    {
        return;
    }

    auto workIt = mapWork.find(spNonce);
    if (workIt == mapWork.end())
    {
        return;
    }
    CWork& work = workIt->second;

    if (nIndex >= work.vecResp.size())
    {
        return;
    }

        work.vecResp[nIndex] = spResp;
        if (--work.nRemainder == 0)
        {
            string strResult;
            if (work.fArray)
            {
                strResult = SerializeCRPCResp(work.vecResp);
            }
            else
            {
                strResult = work.vecResp[0]->Serialize();
            }
            if (!strResult.empty())
            {
                JsonReply(spNonce, strResult);
            }

            mapWork.erase(workIt);
        }
}

void CRPCMod::JsonReply(CNoncePtr spNonce, const std::string& result)
{
    auto spMsg = CHttpRspMessage::Create();
    spMsg->spNonce = spNonce;
    spMsg->nStatusCode = 200;
    spMsg->mapHeader["content-type"] = "application/json";
    spMsg->mapHeader["connection"] = "Keep-Alive";
    spMsg->mapHeader["server"] = "bigbang-rpc";
    spMsg->strContent = result + "\n";
    PUBLISH_MESSAGE(spMsg);
}

bool CRPCMod::CheckWalletError(Errno err)
{
    switch (err)
    {
    case ERR_WALLET_NOT_FOUND:
        throw CRPCException(RPC_INVALID_REQUEST, "Missing wallet");
        break;
    case ERR_WALLET_IS_LOCKED:
        throw CRPCException(RPC_WALLET_UNLOCK_NEEDED,
                            "Wallet is locked,enter the wallet passphrase with walletpassphrase first.");
    case ERR_WALLET_IS_UNLOCKED:
        throw CRPCException(RPC_WALLET_ALREADY_UNLOCKED, "Wallet is already unlocked");
        break;
    case ERR_WALLET_IS_ENCRYPTED:
        throw CRPCException(RPC_WALLET_WRONG_ENC_STATE, "Running with an encrypted wallet, "
                                                        "but encryptwallet was called");
        break;
    case ERR_WALLET_IS_UNENCRYPTED:
        throw CRPCException(RPC_WALLET_WRONG_ENC_STATE, "Running with an unencrypted wallet, "
                                                        "but walletpassphrasechange/walletlock was called.");
        break;
    default:
        break;
    }
    return (err == OK);
}

crypto::CPubKey CRPCMod::GetPubKey(const string& addr)
{
    crypto::CPubKey pubkey;
    CAddress address(addr);
    if (!address.IsNull())
    {
        if (!address.GetPubKey(pubkey))
        {
            throw CRPCException(RPC_INVALID_PARAMETER, "Invalid address, should be pubkey address");
        }
    }
    else
    {
        pubkey.SetHex(addr);
    }
    return pubkey;
}

void CRPCMod::ListDestination(vector<CDestination>& vDestination)
{
    set<crypto::CPubKey> setPubKey;
    set<CTemplateId> setTid;
    pService->GetPubKeys(setPubKey);
    pService->GetTemplateIds(setTid);

    vDestination.clear();
    for (const crypto::CPubKey& pubkey : setPubKey)
    {
        vDestination.push_back(CDestination(pubkey));
    }
    for (const CTemplateId& tid : setTid)
    {
        vDestination.push_back(CDestination(tid));
    }
}

bool CRPCMod::CheckVersion(string& strVersion)
{
    int nMajor, nMinor, nRevision;
    if (!ResolveVersion(strVersion, nMajor, nMinor, nRevision))
    {
        return false;
    }

    strVersion = FormatVersion(nMajor, nMinor, nRevision);
    if (nMajor != VERSION_MAJOR || nMinor != VERSION_MINOR)
    {
        return false;
    }

    return true;
}

string CRPCMod::GetWidthString(const string& strIn, int nWidth)
{
    string str = strIn;
    int nCurLen = str.size();
    if (nWidth > nCurLen)
    {
        str.append(nWidth - nCurLen, ' ');
    }
    return str;
}

std::string CRPCMod::GetWidthString(uint64 nCount, int nWidth)
{
    char tempbuf[12] = { 0 };
    sprintf(tempbuf, "%2.2d", (int)(nCount % 100));
    return GetWidthString(std::to_string(nCount / 100) + std::string(".") + tempbuf, nWidth);
}

std::string CRPCMod::MaskSensitiveData(std::string strData)
{
    boost::regex ptnSec(R"raw(("privkey"|"passphrase"|"oldpassphrase")(\s*:\s*)(".*?"))raw", boost::regex::perl);
    return boost::regex_replace(strData, ptnSec, string(R"raw($1$2"***")raw"));
}

/* System */
CRPCResultPtr CRPCMod::RPCHelp(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CHelpParam>(param);
    string command = spParam->strCommand;
    return MakeCHelpResultPtr(RPCHelpInfo(EModeType::CONSOLE, command));
}

CRPCResultPtr CRPCMod::RPCStop(CNoncePtr spNonce, CRPCParamPtr param)
{
    pService->Stop();
    return MakeCStopResultPtr("bigbang server stopping");
}

CRPCResultPtr CRPCMod::RPCVersion(CNoncePtr spNonce, CRPCParamPtr param)
{
    string strVersion = string("Bigbang server version is v") + VERSION_STR;
    return MakeCVersionResultPtr(strVersion);
}

/* Network */
CRPCResultPtr CRPCMod::RPCGetPeerCount(CNoncePtr spNonce, CRPCParamPtr param)
{
    return MakeCGetPeerCountResultPtr(pService->GetPeerCount());
}

CRPCResultPtr CRPCMod::RPCListPeer(CNoncePtr spNonce, CRPCParamPtr param)
{
    vector<network::CBbPeerInfo> vPeerInfo;
    pService->GetPeers(vPeerInfo);

    auto spResult = MakeCListPeerResultPtr();
    for (const network::CBbPeerInfo& info : vPeerInfo)
    {
        CListPeerResult::CPeer peer;
        peer.strAddress = info.strAddress;
        peer.strServices = UIntToHexString(info.nService);
        peer.nLastsend = info.nLastSend;
        peer.nLastrecv = info.nLastRecv;
        peer.nConntime = info.nActive;
        peer.strVersion = FormatVersion(info.nVersion);
        peer.strSubver = info.strSubVer;
        peer.fInbound = info.fInBound;
        peer.nHeight = info.nStartingHeight;
        peer.nBanscore = info.nScore;
        spResult->vecPeer.push_back(peer);
    }

    return spResult;
}

CRPCResultPtr CRPCMod::RPCAddNode(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CAddNodeParam>(param);
    string strNode = spParam->strNode;

    if (!pService->AddNode(CNetHost(strNode, Config()->nPort)))
    {
        throw CRPCException(RPC_CLIENT_INVALID_IP_OR_SUBNET, "Failed to add node.");
    }

    return MakeCAddNodeResultPtr(string("Add node successfully: ") + strNode);
}

CRPCResultPtr CRPCMod::RPCRemoveNode(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CRemoveNodeParam>(param);
    string strNode = spParam->strNode;

    if (!pService->RemoveNode(CNetHost(strNode, Config()->nPort)))
    {
        throw CRPCException(RPC_CLIENT_INVALID_IP_OR_SUBNET, "Failed to remove node.");
    }

    return MakeCRemoveNodeResultPtr(string("Remove node successfully: ") + strNode);
}

CRPCResultPtr CRPCMod::RPCGetForkCount(CNoncePtr spNonce, CRPCParamPtr param)
{
    return MakeCGetForkCountResultPtr(pService->GetForkCount());
}

CRPCResultPtr CRPCMod::RPCListFork(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CListForkParam>(param);
    vector<pair<uint256, CProfile>> vFork;
    pService->ListFork(vFork, spParam->fAll);

    auto spResult = MakeCListForkResultPtr();
    for (size_t i = 0; i < vFork.size(); i++)
    {
        CProfile& profile = vFork[i].second;
        spResult->vecProfile.push_back({ vFork[i].first.GetHex(), profile.strName, profile.strSymbol,
                                         (double)(profile.nAmount) / COIN, (double)(profile.nMintReward) / COIN, (uint64)(profile.nHalveCycle),
                                         profile.IsIsolated(), profile.IsPrivate(), profile.IsEnclosed(),
                                         CAddress(profile.destOwner).ToString() });
    }

    return spResult;
}

CRPCResultPtr CRPCMod::RPCGetForkGenealogy(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetGenealogyParam>(param);

    //getgenealogy (-f="fork")
    uint256 fork;
    if (!GetForkHashOfDef(spParam->strFork, fork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid fork");
    }

    vector<pair<uint256, int>> vAncestry;
    vector<pair<int, uint256>> vSubline;
    if (!pService->GetForkGenealogy(fork, vAncestry, vSubline))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Unknown fork");
    }

    auto spResult = MakeCGetGenealogyResultPtr();
    for (int i = vAncestry.size(); i > 0; i--)
    {
        spResult->vecAncestry.push_back({ vAncestry[i - 1].first.GetHex(), vAncestry[i - 1].second });
    }
    for (std::size_t i = 0; i < vSubline.size(); i++)
    {
        spResult->vecSubline.push_back({ vSubline[i].second.GetHex(), vSubline[i].first });
    }
    return spResult;
}

CRPCResultPtr CRPCMod::RPCGetBlockLocation(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetBlockLocationParam>(param);

    //getblocklocation <"block">
    uint256 hashBlock;
    hashBlock.SetHex(spParam->strBlock);

    uint256 fork;
    int height;
    if (!pService->GetBlockLocation(hashBlock, fork, height))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Unknown block");
    }

    auto spResult = MakeCGetBlockLocationResultPtr();
    spResult->strFork = fork.GetHex();
    spResult->nHeight = height;
    return spResult;
}

CRPCResultPtr CRPCMod::RPCGetBlockCount(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetBlockCountParam>(param);

    //getblockcount (-f="fork")
    uint256 hashFork;
    if (!GetForkHashOfDef(spParam->strFork, hashFork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid fork");
    }

    if (!pService->HaveFork(hashFork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Unknown fork");
    }

    return MakeCGetBlockCountResultPtr(pService->GetBlockCount(hashFork));
}

CRPCResultPtr CRPCMod::RPCGetBlockHash(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetBlockHashParam>(param);

    //getblockhash <height> (-f="fork")
    int nHeight = spParam->nHeight;

    uint256 hashFork;
    if (!GetForkHashOfDef(spParam->strFork, hashFork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid fork");
    }

    if (!pService->HaveFork(hashFork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Unknown fork");
    }

    vector<uint256> vBlockHash;
    if (!pService->GetBlockHash(hashFork, nHeight, vBlockHash))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Block number out of range.");
    }

    auto spResult = MakeCGetBlockHashResultPtr();
    for (const uint256& hash : vBlockHash)
    {
        spResult->vecHash.push_back(hash.GetHex());
    }

    return spResult;
}

CRPCResultPtr CRPCMod::RPCGetBlock(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetBlockParam>(param);

    //getblock <"block">
    uint256 hashBlock;
    hashBlock.SetHex(spParam->strBlock);

    CBlock block;
    uint256 fork;
    int height;
    if (!pService->GetBlock(hashBlock, block, fork, height))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Unknown block");
    }

    return MakeCGetBlockResultPtr(BlockToJSON(hashBlock, block, fork, height));
}

CRPCResultPtr CRPCMod::RPCGetTxPool(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetTxPoolParam>(param);

    //gettxpool (-f="fork") (-d|-nod*detail*)
    uint256 hashFork;
    if (!GetForkHashOfDef(spParam->strFork, hashFork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid fork");
    }

    if (!pService->HaveFork(hashFork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Unknown fork");
    }

    bool fDetail = spParam->fDetail.IsValid() ? bool(spParam->fDetail) : false;

    vector<pair<uint256, size_t>> vTxPool;
    pService->GetTxPool(hashFork, vTxPool);

    auto spResult = MakeCGetTxPoolResultPtr();
    if (!fDetail)
    {
        size_t nTotalSize = 0;
        for (std::size_t i = 0; i < vTxPool.size(); i++)
        {
            nTotalSize += vTxPool[i].second;
        }
        spResult->nCount = vTxPool.size();
        spResult->nSize = nTotalSize;
    }
    else
    {
        for (std::size_t i = 0; i < vTxPool.size(); i++)
        {
            spResult->vecList.push_back({ vTxPool[i].first.GetHex(), vTxPool[i].second });
        }
    }

    return spResult;
}

CRPCResultPtr CRPCMod::RPCGetTransaction(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetTransactionParam>(param);
    uint256 txid;
    txid.SetHex(spParam->strTxid);

    CTransaction tx;
    uint256 hashFork;
    int nHeight;

    if (!pService->GetTransaction(txid, tx, hashFork, nHeight))
    {
        throw CRPCException(RPC_INVALID_REQUEST, "No information available about transaction");
    }

    auto spResult = MakeCGetTransactionResultPtr();
    if (spParam->fSerialized)
    {
        CBufStream ss;
        ss << tx;
        spResult->strSerialization = ToHexString((const unsigned char*)ss.GetData(), ss.GetSize());
        return spResult;
    }

    int nDepth = nHeight < 0 ? 0 : pService->GetBlockCount(hashFork) - nHeight;
    spResult->transaction = TxToJSON(txid, tx, hashFork, nDepth);
    return spResult;
}

CRPCResultPtr CRPCMod::RPCSendTransaction(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CSendTransactionParam>(param);

    vector<unsigned char> txData = ParseHexString(spParam->strTxdata);
    CBufStream ss;
    ss.Write((char*)&txData[0], txData.size());
    CTransaction rawTx;
    try
    {
        ss >> rawTx;
    }
    catch (const std::exception& e)
    {
        throw CRPCException(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    uint256 txid = rawTx.GetHash();
    auto it = mapWork.find(spNonce);
    if (it != mapWork.end())
    {
        it->second.mapHash.insert(make_pair(txid, it->second.vecResp.size()));
    }

    uint256 hashFork;
    if (!pService->SendTransaction(spNonce, hashFork, rawTx))
    {
        throw CRPCException(RPC_TRANSACTION_REJECTED, string("Tx rejected : hash anchor not exist"));
    }
    return nullptr;
}

CRPCResultPtr CRPCMod::RPCGetForkHeight(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetForkHeightParam>(param);

    //getforkheight (-f="fork")
    uint256 hashFork;
    if (!GetForkHashOfDef(spParam->strFork, hashFork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid fork");
    }

    if (!pService->HaveFork(hashFork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Unknown fork");
    }

    return MakeCGetForkHeightResultPtr(pService->GetForkHeight(hashFork));
}

/* Wallet */
CRPCResultPtr CRPCMod::RPCListKey(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CListKeyParam>(param);

    set<crypto::CPubKey> setPubKey;
    pService->GetPubKeys(setPubKey);

    auto spResult = MakeCListKeyResultPtr();
    for (const crypto::CPubKey& pubkey : setPubKey)
    {
        int nVersion;
        bool fLocked;
        int64 nAutoLockTime;
        if (pService->GetKeyStatus(pubkey, nVersion, fLocked, nAutoLockTime))
        {
            CListKeyResult::CPubkey p;
            p.strKey = pubkey.GetHex();
            p.nVersion = nVersion;
            p.fLocked = fLocked;
            if (!fLocked && nAutoLockTime > 0)
            {
                p.nTimeout = (nAutoLockTime - GetTime());
            }
            spResult->vecPubkey.push_back(p);
        }
    }
    return spResult;
}

CRPCResultPtr CRPCMod::RPCGetNewKey(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetNewKeyParam>(param);

    if (spParam->strPassphrase.empty())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Passphrase must be nonempty");
    }
    crypto::CCryptoString strPassphrase = spParam->strPassphrase.c_str();
    crypto::CPubKey pubkey;
    if (!pService->MakeNewKey(strPassphrase, pubkey))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed add new key.");
    }

    return MakeCGetNewKeyResultPtr(pubkey.ToString());
}

CRPCResultPtr CRPCMod::RPCEncryptKey(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CEncryptKeyParam>(param);

    //encryptkey <"pubkey"> <-new="passphrase"> <-old="oldpassphrase">
    crypto::CPubKey pubkey;
    pubkey.SetHex(spParam->strPubkey);

    if (spParam->strPassphrase.empty())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Passphrase must be nonempty");
    }
    crypto::CCryptoString strPassphrase = spParam->strPassphrase.c_str();

    if (spParam->strOldpassphrase.empty())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Old passphrase must be nonempty");
    }
    crypto::CCryptoString strOldPassphrase = spParam->strOldpassphrase.c_str();

    if (!pService->HaveKey(pubkey))
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Unknown key");
    }
    if (!pService->EncryptKey(pubkey, strPassphrase, strOldPassphrase))
    {
        throw CRPCException(RPC_WALLET_PASSPHRASE_INCORRECT, "The passphrase entered was incorrect.");
    }

    return MakeCEncryptKeyResultPtr(string("Encrypt key successfully: ") + spParam->strPubkey);
}

CRPCResultPtr CRPCMod::RPCLockKey(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CLockKeyParam>(param);

    CAddress address(spParam->strPubkey);
    if (address.IsTemplate())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "This method only accepts pubkey or pubkey address as parameter rather than template address you supplied.");
    }

    crypto::CPubKey pubkey;
    if (address.IsPubKey())
    {
        address.GetPubKey(pubkey);
    }
    else
    {
        pubkey.SetHex(spParam->strPubkey);
    }

    int nVersion;
    bool fLocked;
    int64 nAutoLockTime;
    if (!pService->GetKeyStatus(pubkey, nVersion, fLocked, nAutoLockTime))
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Unknown key");
    }
    if (!fLocked && !pService->Lock(pubkey))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to lock key");
    }
    return MakeCLockKeyResultPtr(string("Lock key successfully: ") + spParam->strPubkey);
}

CRPCResultPtr CRPCMod::RPCUnlockKey(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CUnlockKeyParam>(param);

    CAddress address(spParam->strPubkey);
    if (address.IsTemplate())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "This method only accepts pubkey or pubkey address as parameter rather than template address you supplied.");
    }

    crypto::CPubKey pubkey;
    if (address.IsPubKey())
    {
        address.GetPubKey(pubkey);
    }
    else
    {
        pubkey.SetHex(spParam->strPubkey);
    }

    if (spParam->strPassphrase.empty())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Passphrase must be nonempty");
    }

    crypto::CCryptoString strPassphrase = spParam->strPassphrase.c_str();
    int64 nTimeout = 0;
    if (spParam->nTimeout.IsValid())
    {
        nTimeout = spParam->nTimeout;
    }

    int nVersion;
    bool fLocked;
    int64 nAutoLockTime;
    if (!pService->GetKeyStatus(pubkey, nVersion, fLocked, nAutoLockTime))
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Unknown key");
    }

    if (!fLocked)
    {
        throw CRPCException(RPC_WALLET_ALREADY_UNLOCKED, "Key is already unlocked");
    }

    if (!pService->Unlock(pubkey, strPassphrase, nTimeout))
    {
        throw CRPCException(RPC_WALLET_PASSPHRASE_INCORRECT, "The passphrase entered was incorrect.");
    }

    return MakeCUnlockKeyResultPtr(string("Unlock key successfully: ") + spParam->strPubkey);
}

CRPCResultPtr CRPCMod::RPCImportPrivKey(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CImportPrivKeyParam>(param);

    //importprivkey <"privkey"> <"passphrase">
    uint256 nPriv;
    if (nPriv.SetHex(spParam->strPrivkey) != spParam->strPrivkey.size())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid private key");
    }

    if (spParam->strPassphrase.empty())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Passphrase must be nonempty");
    }

    crypto::CCryptoString strPassphrase = spParam->strPassphrase.c_str();

    crypto::CKey key;
    if (!key.SetSecret(crypto::CCryptoKeyData(nPriv.begin(), nPriv.end())))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid private key");
    }
    if (pService->HaveKey(key.GetPubKey()))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Already have key");
    }
    if (!strPassphrase.empty())
    {
        key.Encrypt(strPassphrase);
    }
    if (!pService->AddKey(key))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to add key");
    }
    if (!pService->SynchronizeWalletTx(CDestination(key.GetPubKey())))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to sync wallet tx");
    }

    return MakeCImportPrivKeyResultPtr(key.GetPubKey().GetHex());
}

CRPCResultPtr CRPCMod::RPCImportKey(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CImportKeyParam>(param);

    vector<unsigned char> vchKey = ParseHexString(spParam->strPubkey);
    crypto::CKey key;
    if (!key.Load(vchKey))
    {
        throw CRPCException(RPC_INVALID_PARAMS, "Failed to verify serialized key");
    }
    if (key.GetVersion() == 0)
    {
        throw CRPCException(RPC_INVALID_PARAMS, "Can't import the key with empty passphrase");
    }
    if (pService->HaveKey(key.GetPubKey()))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Already have key");
    }

    if (!pService->AddKey(key))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to add key");
    }
    if (!pService->SynchronizeWalletTx(CDestination(key.GetPubKey())))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to sync wallet tx");
    }

    return MakeCImportKeyResultPtr(key.GetPubKey().GetHex());
}

CRPCResultPtr CRPCMod::RPCExportKey(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CExportKeyParam>(param);

    crypto::CPubKey pubkey;
    pubkey.SetHex(spParam->strPubkey);

    if (!pService->HaveKey(pubkey))
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Unknown key");
    }
    vector<unsigned char> vchKey;
    if (!pService->ExportKey(pubkey, vchKey))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to export key");
    }

    return MakeCExportKeyResultPtr(ToHexString(vchKey));
}

CRPCResultPtr CRPCMod::RPCAddNewTemplate(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CAddNewTemplateParam>(param);
    CTemplatePtr ptr = CTemplate::CreateTemplatePtr(spParam->data, CAddress());
    if (ptr == nullptr)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid parameters,failed to make template");
    }
    if (!pService->AddTemplate(ptr))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to add template");
    }
    if (!pService->SynchronizeWalletTx(CDestination(ptr->GetTemplateId())))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to sync wallet tx");
    }

    return MakeCAddNewTemplateResultPtr(CAddress(ptr->GetTemplateId()).ToString());
}

CRPCResultPtr CRPCMod::RPCImportTemplate(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CImportTemplateParam>(param);
    vector<unsigned char> vchTemplate = ParseHexString(spParam->strData);
    CTemplatePtr ptr = CTemplate::Import(vchTemplate);
    if (ptr == nullptr)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid parameters,failed to make template");
    }
    if (pService->HaveTemplate(ptr->GetTemplateId()))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Already have this template");
    }
    if (!pService->AddTemplate(ptr))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to add template");
    }
    if (!pService->SynchronizeWalletTx(CDestination(ptr->GetTemplateId())))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to sync wallet tx");
    }

    return MakeCImportTemplateResultPtr(CAddress(ptr->GetTemplateId()).ToString());
}

CRPCResultPtr CRPCMod::RPCExportTemplate(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CExportTemplateParam>(param);
    CAddress address(spParam->strAddress);
    if (address.IsNull())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid address");
    }

    CTemplateId tid = address.GetTemplateId();
    if (!tid)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid address");
    }

    CTemplatePtr ptr = pService->GetTemplate(tid);
    if (!ptr)
    {
        throw CRPCException(RPC_WALLET_ERROR, "Unkown template");
    }

    vector<unsigned char> vchTemplate = ptr->Export();
    return MakeCExportTemplateResultPtr(ToHexString(vchTemplate));
}

CRPCResultPtr CRPCMod::RPCValidateAddress(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CValidateAddressParam>(param);

    CAddress address(spParam->strAddress);
    bool isValid = !address.IsNull();

    auto spResult = MakeCValidateAddressResultPtr();
    spResult->fIsvalid = isValid;
    if (isValid)
    {
        auto& addressData = spResult->addressdata;

        addressData.strAddress = address.ToString();
        if (address.IsPubKey())
        {
            crypto::CPubKey pubkey;
            address.GetPubKey(pubkey);
            bool isMine = pService->HaveKey(pubkey);
            addressData.fIsmine = isMine;
            addressData.strType = "pubkey";
            addressData.strPubkey = pubkey.GetHex();
        }
        else if (address.IsTemplate())
        {
            CTemplateId tid = address.GetTemplateId();
            uint16 nType = tid.GetType();
            CTemplatePtr ptr = pService->GetTemplate(tid);
            addressData.fIsmine = (ptr != nullptr);
            addressData.strType = "template";
            addressData.strTemplate = CTemplate::GetTypeName(nType);
            if (ptr)
            {
                auto& templateData = addressData.templatedata;

                templateData.strHex = ToHexString(ptr->Export());
                templateData.strType = ptr->GetName();
                ptr->GetTemplateData(templateData, CAddress());
            }
        }
    }
    return spResult;
}

CRPCResultPtr CRPCMod::RPCResyncWallet(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CResyncWalletParam>(param);
    if (spParam->strAddress.IsValid())
    {
        CAddress address(spParam->strAddress);
        if (address.IsNull())
        {
            throw CRPCException(RPC_INVALID_PARAMETER, "Invalid address");
        }
        if (!pService->SynchronizeWalletTx(static_cast<CDestination&>(address)))
        {
            throw CRPCException(RPC_WALLET_ERROR, "Failed to resync wallet tx");
        }
    }
    else
    {
        if (!pService->ResynchronizeWalletTx())
        {
            throw CRPCException(RPC_WALLET_ERROR, "Failed to resync wallet tx");
        }
    }
    return MakeCResyncWalletResultPtr("Resync wallet successfully.");
}

CRPCResultPtr CRPCMod::RPCGetBalance(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetBalanceParam>(param);

    //getbalance (-f="fork") (-a="address")
    uint256 hashFork;
    if (!GetForkHashOfDef(spParam->strFork, hashFork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid fork");
    }

    if (!pService->HaveFork(hashFork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Unknown fork");
    }

    vector<CDestination> vDest;
    if (spParam->strAddress.IsValid())
    {
        CAddress address(spParam->strAddress);
        if (address.IsNull())
        {
            throw CRPCException(RPC_INVALID_PARAMETER, "Invalid address");
        }
        vDest.push_back(static_cast<CDestination&>(address));
    }
    else
    {
        ListDestination(vDest);
    }

    auto spResult = MakeCGetBalanceResultPtr();
    for (const CDestination& dest : vDest)
    {
        CWalletBalance balance;
        if (pService->GetBalance(dest, hashFork, balance))
        {
            CGetBalanceResult::CBalance b;
            b.strAddress = CAddress(dest).ToString();
            b.fAvail = ValueFromAmount(balance.nAvailable);
            b.fLocked = ValueFromAmount(balance.nLocked);
            b.fUnconfirmed = ValueFromAmount(balance.nUnconfirmed);
            spResult->vecBalance.push_back(b);
        }
    }

    return spResult;
}

CRPCResultPtr CRPCMod::RPCListTransaction(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CListTransactionParam>(param);

    int nCount = GetUint(spParam->nCount, 10);
    int nOffset = GetInt(spParam->nOffset, 0);
    if (nCount <= 0)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Negative, zero or out of range count");
    }

    vector<CWalletTx> vWalletTx;
    if (!pService->ListWalletTx(nOffset, nCount, vWalletTx))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to list transactions");
    }

    auto spResult = MakeCListTransactionResultPtr();
    for (const CWalletTx& wtx : vWalletTx)
    {
        spResult->vecTransaction.push_back(WalletTxToJSON(wtx));
    }
    return spResult;
}

CRPCResultPtr CRPCMod::RPCSendFrom(CNoncePtr spNonce, CRPCParamPtr param)
{
    //sendfrom <"from"> <"to"> <$amount$> ($txfee$) (-f="fork") (-d="data")
    auto spParam = CastParamPtr<CSendFromParam>(param);
    CAddress from(spParam->strFrom);
    if (from.IsNull())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid from address");
    }

    CAddress to(spParam->strTo);
    if (to.IsNull())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid to address");
    }

    int64 nAmount = AmountFromValue(spParam->fAmount);

    int64 nTxFee = MIN_TX_FEE;
    if (spParam->fTxfee.IsValid())
    {
        nTxFee = AmountFromValue(spParam->fTxfee);
        if (nTxFee < MIN_TX_FEE)
        {
            nTxFee = MIN_TX_FEE;
        }
    }

    uint256 hashFork;
    if (!GetForkHashOfDef(spParam->strFork, hashFork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid fork");
    }

    if (!pService->HaveFork(hashFork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Unknown fork");
    }
    vector<unsigned char> vchData;
    if (spParam->strData.IsValid())
    {
        vchData = ParseHexString(spParam->strData);
    }

    CTransaction txNew;
    if (!pService->CreateTransaction(hashFork, from, to, nAmount, nTxFee, vchData, txNew))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to create transaction");
    }

    bool fCompleted = false;
    if (spParam->strSign_M.IsValid() && spParam->strSign_S.IsValid())
    {
        if (from.IsNull() || from.IsPubKey())
        {
            throw CRPCException(RPC_INVALID_PARAMETER, "Invalid from address,must be a template address");
        }
        else if (from.IsTemplate())
        {
            CTemplateId tid = from.GetTemplateId();
            uint16 nType = tid.GetType();
            if (nType != TEMPLATE_EXCHANGE)
            {
                throw CRPCException(RPC_INVALID_PARAMETER, "Invalid from address,must be a template address");
            }
            if (spParam->strSign_M == "" || spParam->strSign_S == "")
            {
                throw CRPCException(RPC_INVALID_PARAMETER, "Both SS and SM parameter cannot be null");
            }
            vector<unsigned char> vsm = ParseHexString(spParam->strSign_M);
            vector<unsigned char> vss = ParseHexString(spParam->strSign_S);
            txNew.vchSig.clear();
            CODataStream ds(txNew.vchSig);
            ds << vsm << vss << hashFork << pService->GetForkHeight(hashFork);
        }
        else
        {
            throw CRPCException(RPC_INVALID_PARAMETER, "Invalid from address");
        }
    }
    if (!pService->SignTransaction(txNew, fCompleted))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to sign transaction");
    }
    if (!fCompleted)
    {
        throw CRPCException(RPC_WALLET_ERROR, "The signature is not completed");
    }

    uint256 txid = txNew.GetHash();
    auto it = mapWork.find(spNonce);
    if (it != mapWork.end())
    {
        it->second.mapHash.insert(make_pair(txid, it->second.vecResp.size()));
    }

    if (!pService->SendTransaction(spNonce, hashFork, txNew))
    {
        throw CRPCException(RPC_TRANSACTION_REJECTED, string("Tx rejected : hash anchor not exist"));
    }
    return nullptr;
}

CRPCResultPtr CRPCMod::RPCCreateTransaction(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CCreateTransactionParam>(param);

    //createtransaction <"from"> <"to"> <$amount$> ($txfee$) (-f="fork") (-d="data")
    CAddress from(spParam->strFrom);
    if (from.IsNull())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid from address");
    }

    CAddress to(spParam->strTo);
    if (to.IsNull())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid to address");
    }

    int64 nAmount = AmountFromValue(spParam->fAmount);

    int64 nTxFee = MIN_TX_FEE;
    if (spParam->fTxfee.IsValid())
    {
        nTxFee = AmountFromValue(spParam->fTxfee);
        if (nTxFee < MIN_TX_FEE)
        {
            nTxFee = MIN_TX_FEE;
        }
    }

    uint256 hashFork;
    if (!GetForkHashOfDef(spParam->strFork, hashFork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid fork");
    }

    if (!pService->HaveFork(hashFork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Unknown fork");
    }

    vector<unsigned char> vchData;
    if (spParam->strData.IsValid())
    {
        vchData = ParseHexString(spParam->strData);
    }
    CTransaction txNew;
    if (!pService->CreateTransaction(hashFork, from, to, nAmount, nTxFee, vchData, txNew))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to create transaction");
    }

    CBufStream ss;
    ss << txNew;

    return MakeCCreateTransactionResultPtr(
        ToHexString((const unsigned char*)ss.GetData(), ss.GetSize()));
}

CRPCResultPtr CRPCMod::RPCSignTransaction(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CSignTransactionParam>(param);

    vector<unsigned char> txData = ParseHexString(spParam->strTxdata);
    CBufStream ss;
    ss.Write((char*)&txData[0], txData.size());
    CTransaction rawTx;
    try
    {
        ss >> rawTx;
    }
    catch (const std::exception& e)
    {
        throw CRPCException(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    bool fCompleted = false;
    if (!pService->SignTransaction(rawTx, fCompleted))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to sign transaction");
    }

    CBufStream ssNew;
    ssNew << rawTx;

    auto spResult = MakeCSignTransactionResultPtr();
    spResult->strHex = ToHexString((const unsigned char*)ssNew.GetData(), ssNew.GetSize());
    spResult->fCompleted = fCompleted;
    return spResult;
}

CRPCResultPtr CRPCMod::RPCSignMessage(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CSignMessageParam>(param);

    crypto::CPubKey pubkey;
    pubkey.SetHex(spParam->strPubkey);

    string strMessage = spParam->strMessage;

    int nVersion;
    bool fLocked;
    int64 nAutoLockTime;
    if (!pService->GetKeyStatus(pubkey, nVersion, fLocked, nAutoLockTime))
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Unknown key");
    }
    if (fLocked)
    {
        throw CRPCException(RPC_WALLET_UNLOCK_NEEDED, "Key is locked");
    }

    vector<unsigned char> vchSig;
    if (spParam->strAddr.IsValid())
    {
        CAddress addr(spParam->strMessage);
        std::string ss = addr.ToString();
        if (addr.IsNull() || addr.IsPubKey())
        {
            throw CRPCException(RPC_INVALID_PARAMETER, "Invalid address parameters");
        }
        if (!pService->SignSignature(pubkey, addr.GetTemplateId(), vchSig))
        {
            throw CRPCException(RPC_WALLET_ERROR, "Failed to sign message");
        }
    }
    else
    {
        const string strMessageMagic = "Bigbang Signed Message:\n";
        CBufStream ss;
        ss << strMessageMagic;
        ss << strMessage;
        if (!pService->SignSignature(pubkey, crypto::CryptoHash(ss.GetData(), ss.GetSize()), vchSig))
        {
            throw CRPCException(RPC_WALLET_ERROR, "Failed to sign message");
        }
    }
    return MakeCSignMessageResultPtr(ToHexString(vchSig));
}

CRPCResultPtr CRPCMod::RPCListAddress(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spResult = MakeCListAddressResultPtr();
    vector<CDestination> vDes;
    ListDestination(vDes);
    for (const auto& des : vDes)
    {
        CListAddressResult::CAddressdata addressData;
        addressData.strAddress = CAddress(des).ToString();
        if (des.IsPubKey())
        {
            addressData.strType = "pubkey";
            crypto::CPubKey pubkey;
            des.GetPubKey(pubkey);
            addressData.strPubkey = pubkey.GetHex();
        }
        else if (des.IsTemplate())
        {
            addressData.strType = "template";

            CTemplateId tid = des.GetTemplateId();
            uint16 nType = tid.GetType();
            CTemplatePtr ptr = pService->GetTemplate(tid);
            addressData.strTemplate = CTemplate::GetTypeName(nType);

            auto& templateData = addressData.templatedata;
            templateData.strHex = ToHexString(ptr->Export());
            templateData.strType = ptr->GetName();
            ptr->GetTemplateData(templateData, CAddress());
        }
        else
        {
            continue;
        }
        spResult->vecAddressdata.push_back(addressData);
    }

    return spResult;
}

CRPCResultPtr CRPCMod::RPCExportWallet(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CExportWalletParam>(param);

    fs::path pSave(string(spParam->strPath));
    //check if the file name given is available
    if (!pSave.is_absolute())
    {
        throw CRPCException(RPC_WALLET_ERROR, "Must be an absolute path.");
    }
    if (is_directory(pSave))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Cannot export to a folder.");
    }
    if (exists(pSave))
    {
        throw CRPCException(RPC_WALLET_ERROR, "File has been existed.");
    }
    if (pSave.filename() == "." || pSave.filename() == "..")
    {
        throw CRPCException(RPC_WALLET_ERROR, "Cannot export to a folder.");
    }

    if (!exists(pSave.parent_path()) && !create_directories(pSave.parent_path()))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to create directories.");
    }

    Array aAddr;
    vector<CDestination> vDes;
    ListDestination(vDes);
    for (const auto& des : vDes)
    {
        if (des.IsPubKey())
        {
            Object oKey;
            oKey.push_back(Pair("address", CAddress(des).ToString()));

            crypto::CPubKey pubkey;
            des.GetPubKey(pubkey);
            vector<unsigned char> vchKey;
            if (!pService->ExportKey(pubkey, vchKey))
            {
                throw CRPCException(RPC_WALLET_ERROR, "Failed to export key");
            }
            oKey.push_back(Pair("hex", ToHexString(vchKey)));
            aAddr.push_back(oKey);
        }

        if (des.IsTemplate())
        {
            Object oTemp;
            CAddress address(des);

            oTemp.push_back(Pair("address", address.ToString()));

            CTemplateId tid;
            if (!address.GetTemplateId(tid))
            {
                throw CRPCException(RPC_INVALID_PARAMETER, "Invalid template address");
            }
            CTemplatePtr ptr = pService->GetTemplate(tid);
            if (!ptr)
            {
                throw CRPCException(RPC_WALLET_ERROR, "Unkown template");
            }
            vector<unsigned char> vchTemplate = ptr->Export();

            oTemp.push_back(Pair("hex", ToHexString(vchTemplate)));

            aAddr.push_back(oTemp);
        }
    }
    //output them together to file
    try
    {
        std::ofstream ofs(pSave.string(), std::ios::out);
        if (!ofs)
        {
            throw runtime_error("write error");
        }

        write_stream(Value(aAddr), ofs, pretty_print);
        ofs.close();
    }
    catch (...)
    {
        throw CRPCException(RPC_WALLET_ERROR, "filesystem_error - failed to write.");
    }

    return MakeCExportWalletResultPtr(string("Wallet file has been saved at: ") + pSave.string());
}

CRPCResultPtr CRPCMod::RPCImportWallet(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CImportWalletParam>(param);

    fs::path pLoad(string(spParam->strPath));
    //check if the file name given is available
    if (!pLoad.is_absolute())
    {
        throw CRPCException(RPC_WALLET_ERROR, "Must be an absolute path.");
    }
    if (!exists(pLoad) || is_directory(pLoad))
    {
        throw CRPCException(RPC_WALLET_ERROR, "File name is invalid.");
    }

    Value vWallet;
    try
    {
        fs::ifstream ifs(pLoad);
        if (!ifs)
        {
            throw runtime_error("read error");
        }

        read_stream(ifs, vWallet);
        ifs.close();
    }
    catch (...)
    {
        throw CRPCException(RPC_WALLET_ERROR, "Filesystem_error - failed to read.");
    }

    if (array_type != vWallet.type())
    {
        throw CRPCException(RPC_WALLET_ERROR, "Wallet file exported is invalid, check it and try again.");
    }

    Array aAddr;
    uint32 nKey = 0;
    uint32 nTemp = 0;
    for (const auto& oAddr : vWallet.get_array())
    {
        if (oAddr.get_obj()[0].name_ != "address" || oAddr.get_obj()[1].name_ != "hex")
        {
            throw CRPCException(RPC_WALLET_ERROR, "Data format is not correct, check it and try again.");
        }
        string sAddr = oAddr.get_obj()[0].value_.get_str(); //"address" field
        string sHex = oAddr.get_obj()[1].value_.get_str();  //"hex" field

        CAddress addr(sAddr);
        if (addr.IsNull())
        {
            throw CRPCException(RPC_WALLET_ERROR, "Data format is not correct, check it and try again.");
        }

        //import keys
        if (addr.IsPubKey())
        {
            vector<unsigned char> vchKey = ParseHexString(sHex);
            crypto::CKey key;
            if (!key.Load(vchKey))
            {
                throw CRPCException(RPC_INVALID_PARAMS, "Failed to verify serialized key");
            }
            if (key.GetVersion() == 0)
            {
                throw CRPCException(RPC_INVALID_PARAMS, "Can't import the key with empty passphrase");
            }
            if (pService->HaveKey(key.GetPubKey()))
            {
                continue; //step to next one to continue importing
            }
            if (!pService->AddKey(key))
            {
                throw CRPCException(RPC_WALLET_ERROR, "Failed to add key");
            }
            if (!pService->SynchronizeWalletTx(CDestination(key.GetPubKey())))
            {
                throw CRPCException(RPC_WALLET_ERROR, "Failed to sync wallet tx");
            }
            aAddr.push_back(key.GetPubKey().GetHex());
            ++nKey;
        }

        //import templates
        if (addr.IsTemplate())
        {
            vector<unsigned char> vchTemplate = ParseHexString(sHex);
            CTemplatePtr ptr = CTemplate::Import(vchTemplate);
            if (ptr == nullptr)
            {
                throw CRPCException(RPC_INVALID_PARAMETER, "Invalid parameters,failed to make template");
            }
            if (pService->HaveTemplate(addr.GetTemplateId()))
            {
                continue; //step to next one to continue importing
            }
            if (!pService->AddTemplate(ptr))
            {
                throw CRPCException(RPC_WALLET_ERROR, "Failed to add template");
            }
            if (!pService->SynchronizeWalletTx(CDestination(ptr->GetTemplateId())))
            {
                throw CRPCException(RPC_WALLET_ERROR, "Failed to sync wallet tx");
            }
            aAddr.push_back(CAddress(ptr->GetTemplateId()).ToString());
            ++nTemp;
        }
    }

    return MakeCImportWalletResultPtr(string("Imported ") + std::to_string(nKey)
                                      + string(" keys and ") + std::to_string(nTemp) + string(" templates."));
}

CRPCResultPtr CRPCMod::RPCMakeOrigin(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CMakeOriginParam>(param);

    //makeorigin <"prev"> <"owner"> <$amount$> <"name"> <"symbol"> <$reward$> <halvecycle> (-i|-noi*isolated*) (-p|-nop*private*) (-e|-noe*enclosed*)
    uint256 hashPrev;
    hashPrev.SetHex(spParam->strPrev);

    CDestination destOwner = static_cast<CDestination>(CAddress(spParam->strOwner));
    if (destOwner.IsNull())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid owner");
    }

    int64 nAmount = AmountFromValue(spParam->fAmount);
    int64 nMintReward = AmountFromValue(spParam->fReward);

    if (spParam->strName.empty() || spParam->strName.size() > 128
        || spParam->strSymbol.empty() || spParam->strSymbol.size() > 16)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid name or symbol");
    }

    CBlock blockPrev;
    uint256 hashParent;
    int nJointHeight;
    if (!pService->GetBlock(hashPrev, blockPrev, hashParent, nJointHeight))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Unknown prev block");
    }

    if (blockPrev.IsExtended() || blockPrev.IsVacant())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Prev block should not be extended/vacant block");
    }

    CProfile profile;
    profile.strName = spParam->strName;
    profile.strSymbol = spParam->strSymbol;
    profile.destOwner = destOwner;
    profile.hashParent = hashParent;
    profile.nJointHeight = nJointHeight;
    profile.nAmount = nAmount;
    profile.nMintReward = nMintReward;
    profile.nMinTxFee = MIN_TX_FEE;
    profile.nHalveCycle = spParam->nHalvecycle;
    profile.SetFlag(spParam->fIsolated, spParam->fPrivate, spParam->fEnclosed);

    CBlock block;
    block.nVersion = 1;
    block.nType = CBlock::BLOCK_ORIGIN;
    block.nTimeStamp = blockPrev.nTimeStamp + BLOCK_TARGET_SPACING;
    block.hashPrev = hashPrev;
    profile.Save(block.vchProof);

    CTransaction& tx = block.txMint;
    tx.nType = CTransaction::TX_GENESIS;
    tx.nTimeStamp = block.nTimeStamp;
    tx.sendTo = destOwner;
    tx.nAmount = nAmount;
    tx.vchData.assign(profile.strName.begin(), profile.strName.end());

    crypto::CPubKey pubkey;
    if (!destOwner.GetPubKey(pubkey))
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Owner' address should be pubkey address");
    }

    int nVersion;
    bool fLocked;
    int64 nAutoLockTime;
    if (!pService->GetKeyStatus(pubkey, nVersion, fLocked, nAutoLockTime))
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Unknown key");
    }
    if (fLocked)
    {
        throw CRPCException(RPC_WALLET_UNLOCK_NEEDED, "Key is locked");
    }

    uint256 hashBlock = block.GetHash();
    if (!pService->SignSignature(pubkey, hashBlock, block.vchSig))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to sign message");
    }

    CBufStream ss;
    ss << block;

    auto spResult = MakeCMakeOriginResultPtr();
    spResult->strHash = hashBlock.GetHex();
    spResult->strHex = ToHexString((const unsigned char*)ss.GetData(), ss.GetSize());

    return spResult;
}

/* Util */
CRPCResultPtr CRPCMod::RPCVerifyMessage(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CVerifyMessageParam>(param);

    //verifymessage <"pubkey"> <"message"> <"sig">
    crypto::CPubKey pubkey;
    if (pubkey.SetHex(spParam->strPubkey) != spParam->strPubkey.size())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid pubkey");
    }

    string strMessage = spParam->strMessage;

    if (spParam->strSig.empty())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid sig");
    }
    vector<unsigned char> vchSig = ParseHexString(spParam->strSig);
    if (vchSig.size() == 0)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid sig");
    }
    if (spParam->strAddr.IsValid())
    {
        CAddress addr(spParam->strMessage);
        std::string ss = addr.ToString();
        if (addr.IsNull() || addr.IsPubKey())
        {
            throw CRPCException(RPC_INVALID_PARAMETER, "Invalid address parameters");
        }
        return MakeCVerifyMessageResultPtr(
            pubkey.Verify(addr.GetTemplateId(), vchSig));
    }
    else
    {
        const string strMessageMagic = "Bigbang Signed Message:\n";
        CBufStream ss;
        ss << strMessageMagic;
        ss << strMessage;
        return MakeCVerifyMessageResultPtr(
            pubkey.Verify(crypto::CryptoHash(ss.GetData(), ss.GetSize()), vchSig));
    }
}

CRPCResultPtr CRPCMod::RPCMakeKeyPair(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CMakeKeyPairParam>(param);

    crypto::CCryptoKey key;
    crypto::CryptoMakeNewKey(key);

    auto spResult = MakeCMakeKeyPairResultPtr();
    spResult->strPrivkey = key.secret.GetHex();
    spResult->strPubkey = key.pubkey.GetHex();
    return spResult;
}

CRPCResultPtr CRPCMod::RPCGetPubKeyAddress(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetPubkeyAddressParam>(param);
    crypto::CPubKey pubkey;
    if (pubkey.SetHex(spParam->strPubkey) != spParam->strPubkey.size())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid pubkey");
    }
    CDestination dest(pubkey);
    return MakeCGetPubkeyAddressResultPtr(CAddress(dest).ToString());
}

CRPCResultPtr CRPCMod::RPCGetTemplateAddress(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetTemplateAddressParam>(param);
    CTemplateId tid;
    if (tid.SetHex(spParam->strTid) != spParam->strTid.size())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid tid");
    }

    CDestination dest(tid);

    return MakeCGetTemplateAddressResultPtr(CAddress(dest).ToString());
}

CRPCResultPtr CRPCMod::RPCMakeTemplate(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CMakeTemplateParam>(param);
    CTemplatePtr ptr = CTemplate::CreateTemplatePtr(spParam->data, CAddress());
    if (ptr == nullptr)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid parameters,failed to make template");
    }

    auto spResult = MakeCMakeTemplateResultPtr();
    vector<unsigned char> vchTemplate = ptr->Export();
    spResult->strHex = ToHexString(vchTemplate);
    spResult->strAddress = CAddress(ptr->GetTemplateId()).ToString();
    return spResult;
}

CRPCResultPtr CRPCMod::RPCDecodeTransaction(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CDecodeTransactionParam>(param);
    vector<unsigned char> txData(ParseHexString(spParam->strTxdata));
    CBufStream ss;
    ss.Write((char*)&txData[0], txData.size());
    CTransaction rawTx;
    try
    {
        ss >> rawTx;
    }
    catch (const std::exception& e)
    {
        throw CRPCException(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    uint256 hashFork;
    int nHeight;
    if (!pService->GetBlockLocation(rawTx.hashAnchor, hashFork, nHeight))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Unknown anchor block");
    }

    return MakeCDecodeTransactionResultPtr(TxToJSON(rawTx.GetHash(), rawTx, hashFork, -1));
}

// /* Mint */
CRPCResultPtr CRPCMod::RPCGetWork(CNoncePtr spNonce, CRPCParamPtr param)
{
    //getwork <"spent"> <"privkey"> ("prev")
    auto spParam = CastParamPtr<CGetWorkParam>(param);

    CAddress addrSpent(spParam->strSpent);
    uint256 nPriv(spParam->strPrivkey);
    if (addrSpent.IsNull() || !addrSpent.IsPubKey())
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Invalid spent address");
    }
    crypto::CKey key;
    if (!key.SetSecret(crypto::CCryptoKeyData(nPriv.begin(), nPriv.end())))
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    }
    crypto::CPubKey pubkeySpent;
    if (addrSpent.GetPubKey(pubkeySpent) && pubkeySpent == key.GetPubKey())
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Invalid spent address or private key");
    }
    CTemplateMintPtr ptr = CTemplateMint::CreateTemplatePtr(new CTemplateProof(key.GetPubKey(), static_cast<CDestination&>(addrSpent)));
    if (ptr == nullptr)
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Invalid mint template");
    }

    auto spResult = MakeCGetWorkResultPtr();

    vector<unsigned char> vchWorkData;
    int nPrevBlockHeight;
    uint256 hashPrev;
    uint32 nPrevTime;
    int nAlgo, nBits;
    if (!pService->GetWork(vchWorkData, nPrevBlockHeight, hashPrev, nPrevTime, nAlgo, nBits, ptr))
    {
        spResult->fResult = false;
        return spResult;
    }

    spResult->fResult = true;

    spResult->work.nPrevblockheight = nPrevBlockHeight;
    spResult->work.strPrevblockhash = hashPrev.GetHex();
    spResult->work.nPrevblocktime = nPrevTime;
    spResult->work.nAlgo = nAlgo;
    spResult->work.nBits = nBits;
    spResult->work.strData = ToHexString(vchWorkData);

    return spResult;
}

CRPCResultPtr CRPCMod::RPCSubmitWork(CNoncePtr spNonce, CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CSubmitWorkParam>(param);
    vector<unsigned char> vchWorkData(ParseHexString(spParam->strData));
    CAddress addrSpent(spParam->strSpent);
    uint256 nPriv(spParam->strPrivkey);
    if (addrSpent.IsNull() || !addrSpent.IsPubKey())
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Invalid spent address");
    }
    crypto::CKey key;
    if (!key.SetSecret(crypto::CCryptoKeyData(nPriv.begin(), nPriv.end())))
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    }

    CTemplateMintPtr ptr = CTemplateMint::CreateTemplatePtr(new CTemplateProof(key.GetPubKey(), static_cast<CDestination&>(addrSpent)));
    if (ptr == nullptr)
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Invalid mint template");
    }
    CBlock block;
    Errno err = pService->SubmitWork(vchWorkData, ptr, key, block);
    if (err != OK)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, string("Block rejected : ") + ErrorString(err));
    }

    uint256 blockHash = block.GetHash();
    auto it = mapWork.find(spNonce);
    if (it != mapWork.end())
    {
        it->second.mapHash.insert(make_pair(blockHash, it->second.vecResp.size()));
    }

    pService->SendBlock(spNonce, pCoreProtocol->GetGenesisBlockHash(), blockHash, block);
    return nullptr;
}

CRPCResultPtr CRPCMod::RPCQueryStat(CNoncePtr spNonce, CRPCParamPtr param)
{
    enum
    {
        TYPE_NON,
        TYPE_MAKER,
        TYPE_P2PSYN
    } eType = TYPE_NON;
    uint32 nDefQueryCount = 20;
    uint256 hashFork;
    uint32 nBeginTimeValue = ((GetTime() - 60 * nDefQueryCount) % (24 * 60 * 60)) / 60;
    uint32 nGetCount = nDefQueryCount;
    bool fSetBegin = false;

    auto spParam = CastParamPtr<CQueryStatParam>(param);
    if (spParam->strType.empty())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid type: is empty");
    }
    if (spParam->strType == "maker")
    {
        eType = TYPE_MAKER;
    }
    else if (spParam->strType == "p2psyn")
    {
        eType = TYPE_P2PSYN;
    }
    else
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid type");
    }
    if (!spParam->strFork.empty())
    {
        if (hashFork.SetHex(spParam->strFork) != spParam->strFork.size())
        {
            throw CRPCException(RPC_INVALID_PARAMETER, "Invalid fork");
        }
    }
    if (!spParam->strBegin.empty() && spParam->strBegin.size() <= 8)
    {
        //HH:MM:SS
        std::string strTempTime = spParam->strBegin;
        std::size_t found_hour = strTempTime.find(":");
        if (found_hour != std::string::npos && found_hour > 0)
        {
            std::size_t found_min = strTempTime.find(":", found_hour + 1);
            if (found_min != std::string::npos && found_min > found_hour + 1)
            {
                int hour = std::stoi(strTempTime.substr(0, found_hour));
                int minute = std::stoi(strTempTime.substr(found_hour + 1, found_min - (found_hour + 1)));
                if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59)
                {
                    nBeginTimeValue = hour * 60 + minute;
                    int64 nTimeOffset = (GetTime() - GetLocalTimeSeconds()) / 60;
                    nTimeOffset += nBeginTimeValue;
                    if (nTimeOffset >= 0)
                    {
                        nBeginTimeValue = nTimeOffset % (24 * 60);
                    }
                    else
                    {
                        nBeginTimeValue = nTimeOffset + (24 * 60);
                    }
                    fSetBegin = true;
                }
            }
        }
    }
    if (spParam->nCount.IsValid())
    {
        nGetCount = GetUint(spParam->nCount, nDefQueryCount);
        if (nGetCount == 0)
        {
            throw CRPCException(RPC_INVALID_PARAMETER, "Invalid count");
        }
        if (nGetCount > 24 * 60)
        {
            nGetCount = 24 * 60;
        }
    }
    if (!fSetBegin && nGetCount != nDefQueryCount)
    {
        nBeginTimeValue = ((GetTime() - 60 * nGetCount) % (24 * 60 * 60)) / 60;
    }
    else
    {
        uint32 nTempCurTimeValue = (GetTime() % (24 * 60 * 60)) / 60;
        if (nTempCurTimeValue == nBeginTimeValue)
        {
            nGetCount = 0;
        }
        else
        {
            uint32 nTempCount = 0;
            if (nTempCurTimeValue > nBeginTimeValue)
            {
                nTempCount = nTempCurTimeValue - nBeginTimeValue;
            }
            else
            {
                nTempCount = (24 * 60) - (nBeginTimeValue - nTempCurTimeValue);
            }
            if (nGetCount > nTempCount)
            {
                nGetCount = nTempCount;
            }
        }
    }

    switch (eType)
    {
    case TYPE_MAKER:
    {
        std::vector<CStatItemBlockMaker> vStatData;
        if (nGetCount > 0)
        {
            if (!pDataStat->GetBlockMakerStatData(hashFork, nBeginTimeValue, nGetCount, vStatData))
            {
                throw CRPCException(RPC_INTERNAL_ERROR, "query error");
            }
        }

        int nTimeWidth = 8 + 2;                                 //hh:mm:ss + two spaces
        int nPowBlocksWidth = string("powblocks").size() + 2;   //+ two spaces
        int nDposBlocksWidth = string("dposblocks").size() + 2; //+ two spaces
        int nTxTPSWidth = string("tps").size() + 2;
        for (const CStatItemBlockMaker& item : vStatData)
        {
            int nTempValue;
            nTempValue = to_string(item.nPOWBlockCount).size() + 2; //+ two spaces (not decimal point)
            if (nTempValue > nPowBlocksWidth)
            {
                nPowBlocksWidth = nTempValue;
            }
            nTempValue = to_string(item.nDPOSBlockCount).size() + 2; //+ two spaces (not decimal point)
            if (nTempValue > nDposBlocksWidth)
            {
                nDposBlocksWidth = nTempValue;
            }
            nTempValue = to_string(item.nTxTPS).size() + 3; //+ one decimal point + two spaces
            if (nTempValue > nTxTPSWidth)
            {
                nTxTPSWidth = nTempValue;
            }
        }

        int64 nTimeOffset = GetLocalTimeSeconds() - GetTime();

        string strResult = "";
        strResult += GetWidthString("time", nTimeWidth);
        strResult += GetWidthString("powblocks", nPowBlocksWidth);
        strResult += GetWidthString("dposblocks", nDposBlocksWidth);
        strResult += GetWidthString("tps", nTxTPSWidth);
        strResult += string("\r\n");
        for (const CStatItemBlockMaker& item : vStatData)
        {
            int nLocalTimeValue = item.nTimeValue * 60 + nTimeOffset;
            if (nLocalTimeValue >= 0)
            {
                nLocalTimeValue %= (24 * 3600);
            }
            else
            {
                nLocalTimeValue += (24 * 3600);
            }
            char sTimeBuf[128] = { 0 };
            sprintf(sTimeBuf, "%2.2d:%2.2d:59", nLocalTimeValue / 3600, nLocalTimeValue % 3600 / 60);
            strResult += GetWidthString(sTimeBuf, nTimeWidth);
            strResult += GetWidthString(to_string(item.nPOWBlockCount), nPowBlocksWidth);
            strResult += GetWidthString(to_string(item.nDPOSBlockCount), nDposBlocksWidth);
            strResult += GetWidthString(item.nTxTPS, nTxTPSWidth);
            strResult += string("\r\n");
        }
        return MakeCQueryStatResultPtr(strResult);
    }
    case TYPE_P2PSYN:
    {
        std::vector<CStatItemP2pSyn> vStatData;
        if (nGetCount > 0)
        {
            if (!pDataStat->GetP2pSynStatData(hashFork, nBeginTimeValue, nGetCount, vStatData))
            {
                throw CRPCException(RPC_INTERNAL_ERROR, "query error");
            }
        }

        int nTimeWidth = 8 + 2;                                   //hh:mm:ss + two spaces
        int nRecvBlockTPSWidth = string("recvblocks").size() + 2; //+ two spaces
        int nRecvTxTPSWidth = string("recvtps").size() + 2;
        int nSendBlockTPSWidth = string("sendblocks").size() + 2;
        int nSendTxTPSWidth = string("sendtps").size() + 2;
        for (const CStatItemP2pSyn& item : vStatData)
        {
            int nTempValue;
            nTempValue = to_string(item.nRecvBlockCount).size() + 2; //+ two spaces (not decimal point)
            if (nTempValue > nRecvBlockTPSWidth)
            {
                nRecvBlockTPSWidth = nTempValue;
            }
            nTempValue = to_string(item.nSynRecvTxTPS).size() + 3; //+ one decimal point + two spaces
            if (nTempValue > nRecvTxTPSWidth)
            {
                nRecvTxTPSWidth = nTempValue;
            }
            nTempValue = to_string(item.nSendBlockCount).size() + 2; //+ two spaces (not decimal point)
            if (nTempValue > nSendBlockTPSWidth)
            {
                nSendBlockTPSWidth = nTempValue;
            }
            nTempValue = to_string(item.nSynSendTxTPS).size() + 3; //+ one decimal point + two spaces
            if (nTempValue > nSendTxTPSWidth)
            {
                nSendTxTPSWidth = nTempValue;
            }
        }

        int64 nTimeOffset = GetLocalTimeSeconds() - GetTime();

        string strResult;
        strResult += GetWidthString("time", nTimeWidth);
        strResult += GetWidthString("recvblocks", nRecvBlockTPSWidth);
        strResult += GetWidthString("recvtps", nRecvTxTPSWidth);
        strResult += GetWidthString("sendblocks", nSendBlockTPSWidth);
        strResult += GetWidthString("sendtps", nSendTxTPSWidth);
        strResult += string("\r\n");
        for (const CStatItemP2pSyn& item : vStatData)
        {
            int nLocalTimeValue = item.nTimeValue * 60 + nTimeOffset;
            if (nLocalTimeValue >= 0)
            {
                nLocalTimeValue %= (24 * 3600);
            }
            else
            {
                nLocalTimeValue += (24 * 3600);
            }
            char sTimeBuf[128] = { 0 };
            sprintf(sTimeBuf, "%2.2d:%2.2d:59", nLocalTimeValue / 3600, nLocalTimeValue % 3600 / 60);
            strResult += GetWidthString(sTimeBuf, nTimeWidth);
            strResult += GetWidthString(to_string(item.nRecvBlockCount), nRecvBlockTPSWidth);
            strResult += GetWidthString(item.nSynRecvTxTPS, nRecvTxTPSWidth);
            strResult += GetWidthString(to_string(item.nSendBlockCount), nSendBlockTPSWidth);
            strResult += GetWidthString(item.nSynSendTxTPS, nSendTxTPSWidth);
            strResult += string("\r\n");
        }
        return MakeCQueryStatResultPtr(strResult);
    }
    default:
        break;
    }

    return MakeCQueryStatResultPtr(string("error"));
}

CRPCResultPtr CRPCMod::RPCMsgSubmitWork(const CMessage& message)
{
    const CAddedBlockMessage& msg = static_cast<const CAddedBlockMessage&>(message);
    Errno err = (Errno)msg.nError;
    if (err != OK)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, string("Block rejected : ") + ErrorString(err));
    }
    return MakeCSubmitWorkResultPtr(msg.block.GetHash().GetHex());
}

CRPCResultPtr RPCMsgSendFrom(const CMessage& message)
{
    const CAddedTxMessage& msg = static_cast<const CAddedTxMessage&>(message);
    Errno err = (Errno)msg.nError;
    if (err != OK)
    {
        throw CRPCException(RPC_TRANSACTION_REJECTED, string("Tx rejected : ") + ErrorString(err));
    }
    return MakeCSendFromResultPtr(msg.tx.GetHash().GetHex());
}

CRPCResultPtr RPCMsgSendTransaction(const CMessage& message)
{
    const CAddedTxMessage& msg = static_cast<const CAddedTxMessage&>(message);
    Errno err = (Errno)msg.nError;
    if (err != OK)
    {
        throw CRPCException(RPC_TRANSACTION_REJECTED, string("Tx rejected : ") + ErrorString(err));
    }
    return MakeCSendTransactionResultPtr(msg.tx.GetHash().GetHex());
}

} // namespace bigbang
