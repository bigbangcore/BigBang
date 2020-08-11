// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcmod.h"

#include "json/json_spirit_reader_template.h"
#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/format.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/regex.hpp>
#include <regex>
//#include <algorithm>

#include "address.h"
#include "rpc/auto_protocol.h"
#include "template/fork.h"
#include "template/proof.h"
#include "template/template.h"
#include "util.h"
#include "version.h"

using namespace std;
using namespace xengine;
using namespace json_spirit;
using namespace bigbang::rpc;
using namespace bigbang;
namespace fs = boost::filesystem;

#define UNLOCKKEY_RELEASE_DEFAULT_TIME 60

const char* GetGitVersion();

///////////////////////////////
// static function

static int64 AmountFromValue(const double dAmount)
{
    if (IsDoubleEqual(dAmount, -1.0))
    {
        return -1;
    }

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
    data.strHashprev = block.hashPrev.GetHex();
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

static CTransactionData TxToJSON(const uint256& txid, const CTransaction& tx,
                                 const uint256& hashFork, const uint256& blockHash, int nDepth, const string& fromAddr = string())
{
    CTransactionData ret;
    ret.strTxid = txid.GetHex();
    ret.nVersion = tx.nVersion;
    ret.strType = tx.GetTypeString();
    ret.nTime = tx.nTimeStamp;
    ret.nLockuntil = tx.nLockUntil;
    ret.strAnchor = tx.hashAnchor.GetHex();
    ret.strBlockhash = (!blockHash) ? std::string() : blockHash.GetHex();
    for (const CTxIn& txin : tx.vInput)
    {
        CTransactionData::CVin vin;
        vin.nVout = txin.prevout.n;
        vin.strTxid = txin.prevout.hash.GetHex();
        ret.vecVin.push_back(move(vin));
    }
    ret.strSendfrom = fromAddr;
    ret.strSendto = CAddress(tx.sendTo).ToString();
    ret.dAmount = ValueFromAmount(tx.nAmount);
    ret.dTxfee = ValueFromAmount(tx.nTxFee);

    std::string str(tx.vchData.begin(), tx.vchData.end());
    if (str.substr(0, 4) == "msg:")
    {
        ret.strData = str;
    }
    else
    {
        ret.strData = xengine::ToHexString(tx.vchData);
    }
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
    data.dAmount = ValueFromAmount(wtx.nAmount);
    data.dFee = ValueFromAmount(wtx.nTxFee);
    data.nLockuntil = (boost::int64_t)wtx.nLockUntil;
    return data;
}

static CUnspentData UnspentToJSON(const CTxUnspent& unspent)
{
    CUnspentData data;
    data.strTxid = unspent.hash.ToString();
    data.nOut = unspent.n;
    data.dAmount = ValueFromAmount(unspent.output.nAmount);
    data.nTime = unspent.output.nTxTime;
    data.nLockuntil = unspent.output.nLockUntil;
    return data;
}

namespace bigbang
{

///////////////////////////////
// CRPCMod

CRPCMod::CRPCMod()
  : IIOModule("rpcmod")
{
    pHttpServer = nullptr;
    pCoreProtocol = nullptr;
    pService = nullptr;
    pDataStat = nullptr;
    pForkManager = nullptr;

    std::map<std::string, RPCFunc> temp_map = boost::assign::map_list_of
        /* System */
        ("help", &CRPCMod::RPCHelp)
        //
        ("stop", &CRPCMod::RPCStop)
        //
        ("version", &CRPCMod::RPCVersion)
        /* Network */
        ("getpeercount", &CRPCMod::RPCGetPeerCount)
        //
        ("listpeer", &CRPCMod::RPCListPeer)
        //
        ("addnode", &CRPCMod::RPCAddNode)
        //
        ("removenode", &CRPCMod::RPCRemoveNode)
        /* Blockchain & TxPool */
        ("getforkcount", &CRPCMod::RPCGetForkCount)
        //
        ("listfork", &CRPCMod::RPCListFork)
        //
        ("getgenealogy", &CRPCMod::RPCGetForkGenealogy)
        //
        ("getblocklocation", &CRPCMod::RPCGetBlockLocation)
        //
        ("getblockcount", &CRPCMod::RPCGetBlockCount)
        //
        ("getblockhash", &CRPCMod::RPCGetBlockHash)
        //
        ("getblock", &CRPCMod::RPCGetBlock)
        //
        ("getblockdetail", &CRPCMod::RPCGetBlockDetail)
        //
        ("gettxpool", &CRPCMod::RPCGetTxPool)
        //
        ("gettransaction", &CRPCMod::RPCGetTransaction)
        //
        ("sendtransaction", &CRPCMod::RPCSendTransaction)
        //
        ("getforkheight", &CRPCMod::RPCGetForkHeight)
        //
        ("getvotes", &CRPCMod::RPCGetVotes)
        //
        ("listdelegate", &CRPCMod::RPCListDelegate)
        /* Wallet */
        ("listkey", &CRPCMod::RPCListKey)
        //
        ("getnewkey", &CRPCMod::RPCGetNewKey)
        //
        ("encryptkey", &CRPCMod::RPCEncryptKey)
        //
        ("lockkey", &CRPCMod::RPCLockKey)
        //
        ("unlockkey", &CRPCMod::RPCUnlockKey)
        //
        ("importprivkey", &CRPCMod::RPCImportPrivKey)
        //
        ("importpubkey", &CRPCMod::RPCImportPubKey)
        //
        ("importkey", &CRPCMod::RPCImportKey)
        //
        ("exportkey", &CRPCMod::RPCExportKey)
        //
        ("addnewtemplate", &CRPCMod::RPCAddNewTemplate)
        //
        ("importtemplate", &CRPCMod::RPCImportTemplate)
        //
        ("exporttemplate", &CRPCMod::RPCExportTemplate)
        //
        ("validateaddress", &CRPCMod::RPCValidateAddress)
        //
        ("resyncwallet", &CRPCMod::RPCResyncWallet)
        //
        ("getbalance", &CRPCMod::RPCGetBalance)
        //
        ("listtransaction", &CRPCMod::RPCListTransaction)
        //
        ("sendfrom", &CRPCMod::RPCSendFrom)
        //
        ("createtransaction", &CRPCMod::RPCCreateTransaction)
        //
        ("signtransaction", &CRPCMod::RPCSignTransaction)
        //
        ("signmessage", &CRPCMod::RPCSignMessage)
        //
        ("listaddress", &CRPCMod::RPCListAddress)
        //
        ("exportwallet", &CRPCMod::RPCExportWallet)
        //
        ("importwallet", &CRPCMod::RPCImportWallet)
        //
        ("makeorigin", &CRPCMod::RPCMakeOrigin)
        //
        ("signrawtransactionwithwallet", &CRPCMod::RPCSignRawTransactionWithWallet)
        //
        ("sendrawtransaction", &CRPCMod::RPCSendRawTransaction)
        /* Util */
        ("verifymessage", &CRPCMod::RPCVerifyMessage)
        //
        ("makekeypair", &CRPCMod::RPCMakeKeyPair)
        //
        ("getpubkeyaddress", &CRPCMod::RPCGetPubKeyAddress)
        //
        ("gettemplateaddress", &CRPCMod::RPCGetTemplateAddress)
        //
        ("maketemplate", &CRPCMod::RPCMakeTemplate)
        //
        ("decodetransaction", &CRPCMod::RPCDecodeTransaction)
        //
        ("gettxfee", &CRPCMod::RPCGetTxFee)
        //
        ("listunspent", &CRPCMod::RPCListUnspent)
        /* Mint */
        ("getwork", &CRPCMod::RPCGetWork)
        //
        ("submitwork", &CRPCMod::RPCSubmitWork)
        /* tool */
        ("querystat", &CRPCMod::RPCQueryStat);
    mapRPCFunc = temp_map;
    fWriteRPCLog = true;
}

CRPCMod::~CRPCMod()
{
}

bool CRPCMod::HandleInitialize()
{
    if (!GetObject("httpserver", pHttpServer))
    {
        Error("Failed to request httpserver");
        return false;
    }

    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("service", pService))
    {
        Error("Failed to request service");
        return false;
    }

    if (!GetObject("datastat", pDataStat))
    {
        Error("Failed to request datastat");
        return false;
    }
    if (!GetObject("forkmanager", pForkManager))
    {
        Error("Failed to request forkmanager");
        return false;
    }
    fWriteRPCLog = RPCServerConfig()->fRPCLogEnable;

    return true;
}

void CRPCMod::HandleDeinitialize()
{
    pHttpServer = nullptr;
    pCoreProtocol = nullptr;
    pService = nullptr;
    pDataStat = nullptr;
    pForkManager = nullptr;
}

bool CRPCMod::HandleEvent(CEventHttpReq& eventHttpReq)
{
    auto lmdMask = [](const string& data) -> string {
        //remove all sensible information such as private key
        // or passphrass from log content

        //log for debug mode
        boost::regex ptnSec(R"raw(("privkey"|"passphrase"|"oldpassphrase")(\s*:\s*)(".*?"))raw", boost::regex::perl);
        return boost::regex_replace(data, ptnSec, string(R"raw($1$2"***")raw"));
    };

    uint64 nNonce = eventHttpReq.nNonce;

    string strResult;
    try
    {
        // check version
        string strVersion = eventHttpReq.data.mapHeader["url"].substr(1);
        if (!strVersion.empty())
        {
            if (!CheckVersion(strVersion))
            {
                throw CRPCException(RPC_VERSION_OUT_OF_DATE,
                                    string("Out of date version. Server version is v") + VERSION_STR
                                        + ", but client version is v" + strVersion);
            }
        }

        bool fArray;
        CRPCReqVec vecReq = DeserializeCRPCReq(eventHttpReq.data.strContent, fArray);
        CRPCRespVec vecResp;
        for (auto& spReq : vecReq)
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

                if (fWriteRPCLog)
                {
                    Debug("request : %s ", lmdMask(spReq->Serialize()).c_str());
                }

                spResult = (this->*(*it).second)(spReq->spParam);
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
                vecResp.push_back(MakeCRPCRespPtr(spReq->valID, spError));
            }
            else if (spResult)
            {
                vecResp.push_back(MakeCRPCRespPtr(spReq->valID, spResult));
            }
            else
            {
                // no result means no return
            }
        }

        if (fArray)
        {
            strResult = SerializeCRPCResp(vecResp);
        }
        else if (vecResp.size() > 0)
        {
            strResult = vecResp[0]->Serialize();
        }
        else
        {
            // no result means no return
        }
    }
    catch (CRPCException& e)
    {
        auto spError = MakeCRPCErrorPtr(e);
        CRPCResp resp(e.valData, spError);
        strResult = resp.Serialize();
    }
    catch (exception& e)
    {
        cout << "error: " << e.what() << endl;
        auto spError = MakeCRPCErrorPtr(RPC_MISC_ERROR, e.what());
        CRPCResp resp(Value(), spError);
        strResult = resp.Serialize();
    }

    if (fWriteRPCLog)
    {
        Debug("response : %s ", lmdMask(strResult).c_str());
    }

    // no result means no return
    if (!strResult.empty())
    {
        JsonReply(nNonce, strResult);
    }

    return true;
}

bool CRPCMod::HandleEvent(CEventHttpBroken& eventHttpBroken)
{
    (void)eventHttpBroken;
    return true;
}

void CRPCMod::JsonReply(uint64 nNonce, const std::string& result)
{
    CEventHttpRsp eventHttpRsp(nNonce);
    eventHttpRsp.data.nStatusCode = 200;
    eventHttpRsp.data.mapHeader["content-type"] = "application/json";
    eventHttpRsp.data.mapHeader["connection"] = "Keep-Alive";
    eventHttpRsp.data.mapHeader["server"] = "bigbang-rpc";
    eventHttpRsp.data.strContent = result + "\n";

    pHttpServer->DispatchEvent(&eventHttpRsp);
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

/* System */
CRPCResultPtr CRPCMod::RPCHelp(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CHelpParam>(param);
    string command = spParam->strCommand;
    return MakeCHelpResultPtr(RPCHelpInfo(EModeType::CONSOLE, command));
}

CRPCResultPtr CRPCMod::RPCStop(CRPCParamPtr param)
{
    pService->Stop();
    return MakeCStopResultPtr("bigbang server stopping");
}

CRPCResultPtr CRPCMod::RPCVersion(CRPCParamPtr param)
{
    string strVersion = string("Bigbang server version is v") + VERSION_STR + string(", git commit id is ") + GetGitVersion();
    return MakeCVersionResultPtr(strVersion);
}

/* Network */
CRPCResultPtr CRPCMod::RPCGetPeerCount(CRPCParamPtr param)
{
    return MakeCGetPeerCountResultPtr(pService->GetPeerCount());
}

CRPCResultPtr CRPCMod::RPCListPeer(CRPCParamPtr param)
{
    vector<network::CBbPeerInfo> vPeerInfo;
    pService->GetPeers(vPeerInfo);

    auto spResult = MakeCListPeerResultPtr();
    for (const network::CBbPeerInfo& info : vPeerInfo)
    {
        CListPeerResult::CPeer peer;
        peer.strAddress = info.strAddress;
        if (info.nService == 0)
        {
            // Handshaking
            peer.strServices = "NON";
        }
        else
        {
            if (info.nService & network::NODE_NETWORK)
            {
                peer.strServices = "NODE_NETWORK";
            }
            if (info.nService & network::NODE_DELEGATED)
            {
                if (peer.strServices.empty())
                {
                    peer.strServices = "NODE_DELEGATED";
                }
                else
                {
                    peer.strServices = peer.strServices + ",NODE_DELEGATED";
                }
            }
            if (peer.strServices.empty())
            {
                peer.strServices = string("OTHER:") + to_string(info.nService);
            }
        }
        peer.strLastsend = GetTimeString(info.nLastSend);
        peer.strLastrecv = GetTimeString(info.nLastRecv);
        peer.strConntime = GetTimeString(info.nActive);
        peer.nPingtime = info.nPingPongTimeDelta;
        peer.strVersion = FormatVersion(info.nVersion);
        peer.strSubver = info.strSubVer;
        peer.fInbound = info.fInBound;
        peer.nHeight = info.nStartingHeight;
        peer.nBanscore = info.nScore;
        spResult->vecPeer.push_back(peer);
    }

    return spResult;
}

CRPCResultPtr CRPCMod::RPCAddNode(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CAddNodeParam>(param);
    string strNode = spParam->strNode;

    if (!pService->AddNode(CNetHost(strNode, Config()->nPort)))
    {
        throw CRPCException(RPC_CLIENT_INVALID_IP_OR_SUBNET, "Failed to add node.");
    }

    return MakeCAddNodeResultPtr(string("Add node successfully: ") + strNode);
}

CRPCResultPtr CRPCMod::RPCRemoveNode(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CRemoveNodeParam>(param);
    string strNode = spParam->strNode;

    if (!pService->RemoveNode(CNetHost(strNode, Config()->nPort)))
    {
        throw CRPCException(RPC_CLIENT_INVALID_IP_OR_SUBNET, "Failed to remove node.");
    }

    return MakeCRemoveNodeResultPtr(string("Remove node successfully: ") + strNode);
}

CRPCResultPtr CRPCMod::RPCGetForkCount(CRPCParamPtr param)
{
    return MakeCGetForkCountResultPtr(pService->GetForkCount());
}

CRPCResultPtr CRPCMod::RPCListFork(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CListForkParam>(param);
    vector<pair<uint256, CProfile>> vFork;
    pService->ListFork(vFork, spParam->fAll);
    auto spResult = MakeCListForkResultPtr();
    for (size_t i = 0; i < vFork.size(); i++)
    {
        CProfile& profile = vFork[i].second;
        //auto c = std::count(pForkManager->ForkConfig()->vFork.begin(), pForkManager->ForkConfig()->vFork.end(), vFork[i].first.GetHex());
        //if (pForkManager->ForkConfig()->fAllowAnyFork || vFork[i].first == pCoreProtocol->GetGenesisBlockHash() || c > 0)
        if (pForkManager->IsAllowed(vFork[i].first))
        {
            spResult->vecProfile.push_back({ vFork[i].first.GetHex(), profile.strName, profile.strSymbol,
                                             (double)(profile.nAmount) / COIN, (double)(profile.nMintReward) / COIN, (uint64)(profile.nHalveCycle),
                                             profile.IsIsolated(), profile.IsPrivate(), profile.IsEnclosed(),
                                             CAddress(profile.destOwner).ToString() });
        }
    }

    return spResult;
}

CRPCResultPtr CRPCMod::RPCGetForkGenealogy(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCGetBlockLocation(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCGetBlockCount(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCGetBlockHash(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCGetBlock(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCGetBlockDetail(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CgetblockdetailParam>(param);

    //getblockdetail <"block">
    uint256 hashBlock;
    hashBlock.SetHex(spParam->strBlock);

    CBlockEx block;
    uint256 fork;
    int height;
    if (!pService->GetBlockEx(hashBlock, block, fork, height))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Unknown block");
    }

    Cblockdatadetail data;

    data.strHash = hashBlock.GetHex();
    data.strHashprev = block.hashPrev.GetHex();
    data.nVersion = block.nVersion;
    data.strType = GetBlockTypeStr(block.nType, block.txMint.nType);
    data.nTime = block.GetBlockTime();
    if (block.hashPrev != 0)
    {
        data.strPrev = block.hashPrev.GetHex();
    }
    data.strFork = fork.GetHex();
    data.nHeight = height;
    int nDepth = height < 0 ? 0 : pService->GetForkHeight(fork) - height;
    if (fork != pCoreProtocol->GetGenesisBlockHash())
    {
        nDepth = nDepth * 30;
    }
    data.txmint = TxToJSON(block.txMint.GetHash(), block.txMint, fork, hashBlock, nDepth, CAddress().ToString());
    if (block.IsProofOfWork())
    {
        CProofOfHashWorkCompact proof;
        proof.Load(block.vchProof);
        data.nBits = proof.nBits;
    }
    else
    {
        data.nBits = 0;
    }
    for (int i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction& tx = block.vtx[i];
        data.vecTx.push_back(TxToJSON(tx.GetHash(), tx, fork, hashBlock, nDepth, CAddress(block.vTxContxt[i].destIn).ToString()));
    }
    return MakeCgetblockdetailResultPtr(data);
}

CRPCResultPtr CRPCMod::RPCGetTxPool(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCGetTransaction(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetTransactionParam>(param);
    uint256 txid;
    txid.SetHex(spParam->strTxid);
    if (txid == 0)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid txid");
    }
    if (txid == CTransaction().GetHash())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid txid");
    }

    CTransaction tx;
    uint256 hashFork;
    int nHeight;
    uint256 hashBlock;
    CDestination destIn;

    if (!pService->GetTransaction(txid, tx, hashFork, nHeight, hashBlock, destIn))
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

    int nDepth = nHeight < 0 ? 0 : pService->GetForkHeight(hashFork) - nHeight;
    if (hashFork != pCoreProtocol->GetGenesisBlockHash())
    {
        nDepth = nDepth * 30;
    }

    spResult->transaction = TxToJSON(txid, tx, hashFork, hashBlock, nDepth, CAddress(destIn).ToString());
    return spResult;
}

CRPCResultPtr CRPCMod::RPCSendTransaction(CRPCParamPtr param)
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
    Errno err = pService->SendTransaction(rawTx);
    if (err != OK)
    {
        throw CRPCException(RPC_TRANSACTION_REJECTED, string("Tx rejected : ")
                                                          + ErrorString(err));
    }

    return MakeCSendTransactionResultPtr(rawTx.GetHash().GetHex());
}

CRPCResultPtr CRPCMod::RPCGetForkHeight(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCGetVotes(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetVotesParam>(param);

    CAddress destDelegate(spParam->strAddress);
    if (destDelegate.IsNull())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid to address");
    }

    int64 nVotesToken;
    string strFailCause;
    if (!pService->GetVotes(destDelegate, nVotesToken, strFailCause))
    {
        throw CRPCException(RPC_INTERNAL_ERROR, strFailCause);
    }

    return MakeCGetVotesResultPtr(ValueFromAmount(nVotesToken));
}

CRPCResultPtr CRPCMod::RPCListDelegate(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CListDelegateParam>(param);

    std::multimap<int64, CDestination> mapVotes;
    if (!pService->ListDelegate(spParam->nCount, mapVotes))
    {
        throw CRPCException(RPC_INTERNAL_ERROR, "Query fail");
    }

    auto spResult = MakeCListDelegateResultPtr();
    for (const auto& d : boost::adaptors::reverse(mapVotes))
    {
        CListDelegateResult::CDelegate delegateData;
        delegateData.strAddress = CAddress(d.second).ToString();
        delegateData.dVotes = ValueFromAmount(d.first);
        spResult->vecDelegate.push_back(delegateData);
    }
    return spResult;
}

/* Wallet */
CRPCResultPtr CRPCMod::RPCListKey(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CListKeyParam>(param);

    set<crypto::CPubKey> setPubKey;
    pService->GetPubKeys(setPubKey);

    auto spResult = MakeCListKeyResultPtr();
    for (const crypto::CPubKey& pubkey : setPubKey)
    {
        int nVersion;
        bool fLocked, fPublic;
        int64 nAutoLockTime;
        if (pService->GetKeyStatus(pubkey, nVersion, fLocked, nAutoLockTime, fPublic))
        {
            CListKeyResult::CPubkey p;
            p.strKey = pubkey.GetHex();
            p.nVersion = nVersion;
            p.fPublic = fPublic;
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

CRPCResultPtr CRPCMod::RPCGetNewKey(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetNewKeyParam>(param);

    if (spParam->strPassphrase.empty())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Passphrase must be nonempty");
    }

    crypto::CCryptoString strPassphrase = spParam->strPassphrase.c_str();
    crypto::CPubKey pubkey;
    auto strErr = pService->MakeNewKey(strPassphrase, pubkey);
    if (strErr)
    {
        throw CRPCException(RPC_WALLET_ERROR, std::string("Failed add new key: ") + *strErr);
    }

    return MakeCGetNewKeyResultPtr(pubkey.ToString());
}

CRPCResultPtr CRPCMod::RPCEncryptKey(CRPCParamPtr param)
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

    if (!pService->HaveKey(pubkey, crypto::CKey::PRIVATE_KEY))
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Unknown key");
    }
    if (!pService->EncryptKey(pubkey, strPassphrase, strOldPassphrase))
    {
        throw CRPCException(RPC_WALLET_PASSPHRASE_INCORRECT, "The passphrase entered was incorrect.");
    }

    return MakeCEncryptKeyResultPtr(string("Encrypt key successfully: ") + spParam->strPubkey);
}

CRPCResultPtr CRPCMod::RPCLockKey(CRPCParamPtr param)
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
    bool fLocked, fPublic;
    int64 nAutoLockTime;
    if (!pService->GetKeyStatus(pubkey, nVersion, fLocked, nAutoLockTime, fPublic))
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Unknown key");
    }
    if (fPublic)
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Can't lock public key");
    }
    if (!fLocked && !pService->Lock(pubkey))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to lock key");
    }
    return MakeCLockKeyResultPtr(string("Lock key successfully: ") + spParam->strPubkey);
}

CRPCResultPtr CRPCMod::RPCUnlockKey(CRPCParamPtr param)
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
    else if (!RPCServerConfig()->fDebug)
    {
        nTimeout = UNLOCKKEY_RELEASE_DEFAULT_TIME;
    }

    int nVersion;
    bool fLocked, fPublic;
    int64 nAutoLockTime;
    if (!pService->GetKeyStatus(pubkey, nVersion, fLocked, nAutoLockTime, fPublic))
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Unknown key");
    }
    if (fPublic)
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Can't unlock public key");
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

CRPCResultPtr CRPCMod::RPCImportPrivKey(CRPCParamPtr param)
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
    if (!pService->HaveKey(key.GetPubKey(), crypto::CKey::PRIVATE_KEY))
    {
        if (!strPassphrase.empty())
        {
            key.Encrypt(strPassphrase);
        }
        auto strErr = pService->AddKey(key);
        if (strErr)
        {
            throw CRPCException(RPC_WALLET_ERROR, std::string("Failed to add key: ") + *strErr);
        }
        if (spParam->fSynctx && !pService->SynchronizeWalletTx(CDestination(key.GetPubKey())))
        {
            throw CRPCException(RPC_WALLET_ERROR, "Failed to sync wallet tx");
        }
    }

    return MakeCImportPrivKeyResultPtr(key.GetPubKey().GetHex());
}

CRPCResultPtr CRPCMod::RPCImportPubKey(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CImportPubKeyParam>(param);

    //importpubkey <"pubkey"> or importpubkey <"pubkeyaddress">
    CAddress address(spParam->strPubkey);
    if (address.IsTemplate())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Template id is not allowed");
    }

    crypto::CPubKey pubkey;
    if (address.IsPubKey())
    {
        address.GetPubKey(pubkey);
    }
    else if (pubkey.SetHex(spParam->strPubkey) != spParam->strPubkey.size())
    {
        pubkey.SetHex(spParam->strPubkey);
    }

    crypto::CKey key;
    key.Load(pubkey, crypto::CKey::PUBLIC_KEY, crypto::CCryptoCipher());
    if (!pService->HaveKey(key.GetPubKey()))
    {
        auto strErr = pService->AddKey(key);
        if (strErr)
        {
            throw CRPCException(RPC_WALLET_ERROR, std::string("Failed to add key: ") + *strErr);
        }
        if (!pService->SynchronizeWalletTx(CDestination(key.GetPubKey())))
        {
            throw CRPCException(RPC_WALLET_ERROR, "Failed to sync wallet tx");
        }
    }

    CDestination dest(pubkey);
    return MakeCImportPubKeyResultPtr(CAddress(dest).ToString());
}

CRPCResultPtr CRPCMod::RPCImportKey(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CImportKeyParam>(param);

    vector<unsigned char> vchKey = ParseHexString(spParam->strPubkey);
    crypto::CKey key;
    if (!key.Load(vchKey))
    {
        throw CRPCException(RPC_INVALID_PARAMS, "Failed to verify serialized key");
    }
    if (key.GetVersion() == crypto::CKey::INIT)
    {
        throw CRPCException(RPC_INVALID_PARAMS, "Can't import the key with empty passphrase");
    }
    if ((key.IsPrivKey() && !pService->HaveKey(key.GetPubKey(), crypto::CKey::PRIVATE_KEY))
        || (key.IsPubKey() && !pService->HaveKey(key.GetPubKey())))
    {
        auto strErr = pService->AddKey(key);
        if (strErr)
        {
            throw CRPCException(RPC_WALLET_ERROR, std::string("Failed to add key: ") + *strErr);
        }
        if (spParam->fSynctx && !pService->SynchronizeWalletTx(CDestination(key.GetPubKey())))
        {
            throw CRPCException(RPC_WALLET_ERROR, "Failed to sync wallet tx");
        }
    }

    return MakeCImportKeyResultPtr(key.GetPubKey().GetHex());
}

CRPCResultPtr CRPCMod::RPCExportKey(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCAddNewTemplate(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CAddNewTemplateParam>(param);
    CTemplatePtr ptr = CTemplate::CreateTemplatePtr(spParam->data, CAddress());
    if (ptr == nullptr)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid parameters,failed to make template");
    }
    if (!pService->HaveTemplate(ptr->GetTemplateId()))
    {
        if (!pService->AddTemplate(ptr))
        {
            throw CRPCException(RPC_WALLET_ERROR, "Failed to add template");
        }
        if (spParam->data.fSynctx && !pService->SynchronizeWalletTx(CDestination(ptr->GetTemplateId())))
        {
            throw CRPCException(RPC_WALLET_ERROR, "Failed to sync wallet tx");
        }
    }

    return MakeCAddNewTemplateResultPtr(CAddress(ptr->GetTemplateId()).ToString());
}

CRPCResultPtr CRPCMod::RPCImportTemplate(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CImportTemplateParam>(param);
    vector<unsigned char> vchTemplate = ParseHexString(spParam->strData);
    CTemplatePtr ptr = CTemplate::Import(vchTemplate);
    if (ptr == nullptr)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid parameters,failed to make template");
    }
    if (!pService->HaveTemplate(ptr->GetTemplateId()))
    {
        if (!pService->AddTemplate(ptr))
        {
            throw CRPCException(RPC_WALLET_ERROR, "Failed to add template");
        }
        if (spParam->fSynctx && !pService->SynchronizeWalletTx(CDestination(ptr->GetTemplateId())))
        {
            throw CRPCException(RPC_WALLET_ERROR, "Failed to sync wallet tx");
        }
    }

    return MakeCImportTemplateResultPtr(CAddress(ptr->GetTemplateId()).ToString());
}

CRPCResultPtr CRPCMod::RPCExportTemplate(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCValidateAddress(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCResyncWallet(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCGetBalance(CRPCParamPtr param)
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
            b.dAvail = ValueFromAmount(balance.nAvailable);
            b.dLocked = ValueFromAmount(balance.nLocked);
            b.dUnconfirmed = ValueFromAmount(balance.nUnconfirmed);
            spResult->vecBalance.push_back(b);
        }
    }

    return spResult;
}

CRPCResultPtr CRPCMod::RPCListTransaction(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CListTransactionParam>(param);

    const CRPCString& strFork = spParam->strFork;
    const CRPCString& strAddress = spParam->strAddress;

    CAddress address(strAddress);
    uint256 fork;
    if (!strFork.empty() && !GetForkHashOfDef(strFork, fork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid fork");
    }

    if (!strAddress.empty() && !address.ParseString(strAddress))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid address");
    }

    int nCount = GetUint(spParam->nCount, 10);
    int nOffset = GetInt(spParam->nOffset, 0);
    if (nCount <= 0)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Negative, zero or out of range count");
    }

    vector<CWalletTx> vWalletTx;
    if (!pService->ListWalletTx(fork, address, nOffset, nCount, vWalletTx))
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

CRPCResultPtr CRPCMod::RPCSendFrom(CRPCParamPtr param)
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

    int64 nAmount = AmountFromValue(spParam->dAmount);

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
        auto strDataTmp = spParam->strData;
        if (((std::string)strDataTmp).substr(0, 4) == "msg:")
        {
            auto hex = xengine::ToHexString((const unsigned char*)strDataTmp.c_str(), strlen(strDataTmp.c_str()));
            vchData = ParseHexString(hex);
        }
        else
        {
            vchData = ParseHexString(strDataTmp);
        }
    }

    int64 nTxFee = CalcMinTxFee(vchData.size(), NEW_MIN_TX_FEE);
    if (spParam->dTxfee.IsValid())
    {
        int64 nUserTxFee = AmountFromValue(spParam->dTxfee);
        if (nUserTxFee > nTxFee)
        {
            nTxFee = nUserTxFee;
        }
        StdTrace("[SendFrom]", "txudatasize : %d ; mintxfee : %d", vchData.size(), nTxFee);
    }

    CWalletBalance balance;
    if (!pService->GetBalance(from, hashFork, balance))
    {
        throw CRPCException(RPC_WALLET_ERROR, "GetBalance failed");
    }
    if (nAmount == -1)
    {
        if (balance.nAvailable <= nTxFee)
        {
            throw CRPCException(RPC_WALLET_ERROR, "Your amount not enough for txfee");
        }
        nAmount = balance.nAvailable - nTxFee;
    }

    if (from.IsTemplate() && from.GetTemplateId().GetType() == TEMPLATE_PAYMENT)
    {
        nAmount -= nTxFee;
    }

    CTransaction txNew;
    auto strErr = pService->CreateTransaction(hashFork, from, to, nAmount, nTxFee, vchData, txNew);
    if (strErr)
    {
        boost::format fmt = boost::format(" Balance: %1% TxFee: %2%") % balance.nAvailable % txNew.nTxFee;
        throw CRPCException(RPC_WALLET_ERROR, std::string("Failed to create transaction: ") + *strErr + fmt.str());
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

    if (from.IsTemplate() && from.GetTemplateId().GetType() == TEMPLATE_PAYMENT)
    {
        txNew.vchSig.clear();
        CODataStream ds(txNew.vchSig);
        ds << pService->GetForkHeight(hashFork) << (txNew.nTxFee + txNew.nAmount);
    }

    vector<uint8> vchSendToData;
    if (to.IsTemplate() && spParam->strSendtodata.IsValid())
    {
        vchSendToData = ParseHexString(spParam->strSendtodata);
    }

    if (!pService->SignTransaction(txNew, vchSendToData, fCompleted))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to sign transaction");
    }
    if (!fCompleted)
    {
        throw CRPCException(RPC_WALLET_ERROR, "The signature is not completed");
    }

    Errno err = pService->SendTransaction(txNew);
    if (err != OK)
    {
        throw CRPCException(RPC_TRANSACTION_REJECTED, string("Tx rejected : ")
                                                          + ErrorString(err));
    }
    std::stringstream ss;
    for (auto& obj : txNew.vInput)
    {
        ss << (int)obj.prevout.n << ":" << obj.prevout.hash.GetHex().c_str() << ";";
    }

    StdDebug("[SendFrom][DEBUG]", "txNew hash:%s; input:%s", txNew.GetHash().GetHex().c_str(), ss.str().c_str());
    return MakeCSendFromResultPtr(txNew.GetHash().GetHex());
}

CRPCResultPtr CRPCMod::RPCCreateTransaction(CRPCParamPtr param)
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

    int64 nAmount = AmountFromValue(spParam->dAmount);

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

    int64 nTxFee = CalcMinTxFee(vchData.size(), NEW_MIN_TX_FEE);
    if (spParam->dTxfee.IsValid())
    {
        nTxFee = AmountFromValue(spParam->dTxfee);

        int64 nFee = CalcMinTxFee(vchData.size(), NEW_MIN_TX_FEE);
        if (nTxFee < nFee)
        {
            nTxFee = nFee;
        }
        StdTrace("[CreateTransaction]", "txudatasize : %d ; mintxfee : %d", vchData.size(), nTxFee);
    }

    CWalletBalance balance;
    if (!pService->GetBalance(from, hashFork, balance))
    {
        throw CRPCException(RPC_WALLET_ERROR, "GetBalance failed");
    }
    if (nAmount == -1)
    {
        if (balance.nAvailable <= nTxFee)
        {
            throw CRPCException(RPC_WALLET_ERROR, "Your amount not enough for txfee");
        }
        nAmount = balance.nAvailable - nTxFee;
    }

    CTemplateId tid;
    if (to.GetTemplateId(tid) && tid.GetType() == TEMPLATE_FORK && nAmount < CTemplateFork::CreatedCoin())
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "create transaction nAmount must be at least " + std::to_string(CTemplateFork::CreatedCoin() / COIN) + " for creating fork");
    }

    CTransaction txNew;
    auto strErr = pService->CreateTransaction(hashFork, from, to, nAmount, nTxFee, vchData, txNew);
    if (strErr)
    {
        boost::format fmt = boost::format(" Balance: %1% TxFee: %2%") % balance.nAvailable % txNew.nTxFee;
        throw CRPCException(RPC_WALLET_ERROR, std::string("Failed to create transaction: ") + *strErr + fmt.str());
    }

    CBufStream ss;
    ss << txNew;

    return MakeCCreateTransactionResultPtr(
        ToHexString((const unsigned char*)ss.GetData(), ss.GetSize()));
}

CRPCResultPtr CRPCMod::RPCSignTransaction(CRPCParamPtr param)
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

    vector<uint8> vchSendToData;
    if (rawTx.sendTo.IsTemplate() && spParam->strSendtodata.IsValid())
    {
        vchSendToData = ParseHexString(spParam->strSendtodata);
    }

    bool fCompleted = false;
    if (!pService->SignTransaction(rawTx, vchSendToData, fCompleted))
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

CRPCResultPtr CRPCMod::RPCSignMessage(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CSignMessageParam>(param);

    crypto::CPubKey pubkey;
    pubkey.SetHex(spParam->strPubkey);

    string strMessage = spParam->strMessage;

    int nVersion;
    bool fLocked, fPublic;
    int64 nAutoLockTime;
    if (!pService->GetKeyStatus(pubkey, nVersion, fLocked, nAutoLockTime, fPublic))
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Unknown key");
    }
    if (fPublic)
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Can't sign message by public key");
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

CRPCResultPtr CRPCMod::RPCListAddress(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCExportWallet(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CExportWalletParam>(param);

#ifdef BOOST_CYGWIN_FS_PATH
    std::string strCygWinPathPrefix = "/cygdrive";
    std::size_t found = string(spParam->strPath).find(strCygWinPathPrefix);
    if (found != std::string::npos)
    {
        strCygWinPathPrefix = "";
    }
#else
    std::string strCygWinPathPrefix;
#endif

    fs::path pSave(string(strCygWinPathPrefix + spParam->strPath));
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

CRPCResultPtr CRPCMod::RPCImportWallet(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CImportWalletParam>(param);

#ifdef BOOST_CYGWIN_FS_PATH
    std::string strCygWinPathPrefix = "/cygdrive";
    std::size_t found = string(spParam->strPath).find(strCygWinPathPrefix);
    if (found != std::string::npos)
    {
        strCygWinPathPrefix = "";
    }
#else
    std::string strCygWinPathPrefix;
#endif

    fs::path pLoad(string(strCygWinPathPrefix + spParam->strPath));
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

        read_stream(ifs, vWallet, RPC_MAX_DEPTH);
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
            if (key.GetVersion() == crypto::CKey::INIT)
            {
                throw CRPCException(RPC_INVALID_PARAMS, "Can't import the key with empty passphrase");
            }
            if ((key.IsPrivKey() && pService->HaveKey(key.GetPubKey(), crypto::CKey::PRIVATE_KEY))
                || (key.IsPubKey() && pService->HaveKey(key.GetPubKey())))
            {
                continue; //step to next one to continue importing
            }
            auto strErr = pService->AddKey(key);
            if (strErr)
            {
                throw CRPCException(RPC_WALLET_ERROR, std::string("Failed to add key: ") + *strErr);
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

CRPCResultPtr CRPCMod::RPCMakeOrigin(CRPCParamPtr param)
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

    int64 nAmount = AmountFromValue(spParam->dAmount);
    int64 nMintReward = AmountFromValue(spParam->dReward);
    if (!RewardRange(nMintReward))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid reward");
    }

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

    int nForkHeight = pService->GetForkHeight(hashParent);
    if (nForkHeight < nJointHeight + MIN_CREATE_FORK_INTERVAL_HEIGHT)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, string("The minimum confirmed height of the previous block is ") + to_string(MIN_CREATE_FORK_INTERVAL_HEIGHT));
    }
    if ((int64)nForkHeight > (int64)nJointHeight + MAX_JOINT_FORK_INTERVAL_HEIGHT)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, string("Maximum fork spacing height is ") + to_string(MAX_JOINT_FORK_INTERVAL_HEIGHT));
    }

    uint256 hashBlockRef;
    int64 nTimeRef;
    if (!pService->GetLastBlockOfHeight(pCoreProtocol->GetGenesisBlockHash(), nJointHeight + 1, hashBlockRef, nTimeRef))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Failed to query main chain reference block");
    }

    CProfile profile;
    profile.strName = spParam->strName;
    profile.strSymbol = spParam->strSymbol;
    profile.destOwner = destOwner;
    profile.hashParent = hashParent;
    profile.nJointHeight = nJointHeight;
    profile.nAmount = nAmount;
    profile.nMintReward = nMintReward;
    profile.nMinTxFee = NEW_MIN_TX_FEE;
    profile.nHalveCycle = spParam->nHalvecycle;
    profile.SetFlag(spParam->fIsolated, spParam->fPrivate, spParam->fEnclosed);

    CBlock block;
    block.nVersion = 1;
    block.nType = CBlock::BLOCK_ORIGIN;
    block.nTimeStamp = nTimeRef;
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
    bool fLocked, fPublic;
    int64 nAutoLockTime;
    if (!pService->GetKeyStatus(pubkey, nVersion, fLocked, nAutoLockTime, fPublic))
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Unknown key");
    }
    if (fPublic)
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Can't sign origin block by public key");
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

CRPCResultPtr CRPCMod::RPCSignRawTransactionWithWallet(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CSignRawTransactionWithWalletParam>(param);

    CAddress addr(spParam->strAddrin);
    crypto::CPubKey pubkey;
    CTemplateId tid;
    bool fPubkey = true;
    if (addr.IsPubKey())
    {
        pubkey = addr.data;
    }
    else if (addr.IsTemplate())
    {
        tid = addr.data;
        fPubkey = false;
    }

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
    CDestination destIn;
    if (fPubkey)
    {
        destIn.SetPubKey(pubkey);
    }
    else
    {
        destIn.SetTemplateId(tid);
    }

    if (!pService->SignOfflineTransaction(destIn, rawTx, fCompleted))
    {
        throw CRPCException(RPC_WALLET_ERROR, "Failed to sign offline transaction");
    }

    CBufStream ssNew;
    ssNew << rawTx;

    auto spResult = MakeCSignRawTransactionWithWalletResultPtr();
    spResult->strHex = ToHexString((const unsigned char*)ssNew.GetData(), ssNew.GetSize());
    spResult->fCompleted = fCompleted;
    return spResult;
}

CRPCResultPtr CRPCMod::RPCSendRawTransaction(rpc::CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CSendRawTransactionParam>(param);

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
        throw CRPCException(RPC_DESERIALIZATION_ERROR, "Signed offline raw tx decode failed");
    }

    Errno err = pService->SendOfflineSignedTransaction(rawTx);
    if (err != OK)
    {
        throw CRPCException(RPC_TRANSACTION_REJECTED, string("Tx rejected : ")
                                                          + ErrorString(err));
    }

    return MakeCSendRawTransactionResultPtr(rawTx.GetHash().GetHex());
}

/* Util */
CRPCResultPtr CRPCMod::RPCVerifyMessage(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCMakeKeyPair(CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CMakeKeyPairParam>(param);

    crypto::CCryptoKey key;
    crypto::CryptoMakeNewKey(key);

    auto spResult = MakeCMakeKeyPairResultPtr();
    spResult->strPrivkey = key.secret.GetHex();
    spResult->strPubkey = key.pubkey.GetHex();
    return spResult;
}

CRPCResultPtr CRPCMod::RPCGetPubKeyAddress(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCGetTemplateAddress(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCMakeTemplate(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCDecodeTransaction(CRPCParamPtr param)
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

    uint256 hashFork = rawTx.hashAnchor;
    /*int nHeight;
    if (!pService->GetBlockLocation(rawTx.hashAnchor, hashFork, nHeight))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Unknown anchor block");
    }*/

    return MakeCDecodeTransactionResultPtr(TxToJSON(rawTx.GetHash(), rawTx, hashFork, uint256(), -1, string()));
}

CRPCResultPtr CRPCMod::RPCGetTxFee(rpc::CRPCParamPtr param)
{
    auto spParam = CastParamPtr<CGetTransactionFeeParam>(param);
    int64 nTxFee = CalcMinTxFee(ParseHexString(spParam->strHexdata).size(), NEW_MIN_TX_FEE);
    auto spResult = MakeCGetTransactionFeeResultPtr();
    spResult->dTxfee = ValueFromAmount(nTxFee);
    return spResult;
}

CRPCResultPtr CRPCMod::RPCListUnspent(CRPCParamPtr param)
{
    auto lmdImport = [](const string& pathFile, vector<CAddress>& addresses) -> bool {
        ifstream inFile(pathFile);

        if (!inFile)
        {
            return false;
        }

        // iterate addresses from input file
        const uint32 MAX_LISTUNSPENT_INPUT = 10000;
        uint32 nCount = 1;
        string strAddr;
        while (getline(inFile, strAddr) && nCount <= MAX_LISTUNSPENT_INPUT)
        {
            boost::trim(strAddr);
            if (strAddr.size() != CAddress::ADDRESS_LEN)
            {
                continue;
            }

            CAddress addr(strAddr);
            if (!addr.IsNull())
            {
                addresses.emplace_back(addr);
                ++nCount;
            }
        }

        auto last = unique(addresses.begin(), addresses.end());
        addresses.erase(last, addresses.end());

        return true;
    };

    auto spParam = CastParamPtr<CListUnspentParam>(param);

    uint256 fork;
    if (!GetForkHashOfDef(spParam->strFork, fork))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid fork");
    }

    vector<CAddress> vAddr;

    CAddress addr(spParam->strAddress);
    if (!addr.IsNull())
    {
        vAddr.emplace_back(addr);
    }

    if (spParam->strFile.IsValid() && !lmdImport(spParam->strFile, vAddr))
    {
        throw CRPCException(RPC_INVALID_PARAMETER, "Invalid import file");
    }

    if (vAddr.empty())
    {
        throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Available address as argument should be provided.");
    }

    std::map<CDestination, std::vector<CTxUnspent>> mapDest;
    for (const auto& i : vAddr)
    {
        mapDest.emplace(std::make_pair(static_cast<CDestination>(i), std::vector<CTxUnspent>()));
    }

    if (vAddr.size() > 1)
    {
        if (!pService->ListForkUnspentBatch(fork, spParam->nMax, mapDest))
        {
            throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Acquiring batch unspent list failed.");
        }
    }
    else if (1 == vAddr.size())
    {
        if (!pService->ListForkUnspent(fork, static_cast<CDestination&>(vAddr[0]),
                                       spParam->nMax, mapDest[static_cast<CDestination>(vAddr[0])]))
        {
            throw CRPCException(RPC_INVALID_ADDRESS_OR_KEY, "Acquiring unspent list failed.");
        }
    }

    auto spResult = MakeCListUnspentResultPtr();
    double dTotal = 0.0f;
    for (auto& iAddr : mapDest)
    {
        CAddress dest(iAddr.first);

        typename CListUnspentResult::CAddresses a;
        a.strAddress = dest.ToString();

        double dSum = 0.0f;
        for (const auto& unspent : iAddr.second)
        {
            CUnspentData data = UnspentToJSON(unspent);
            a.vecUnspents.push_back(data);
            dSum += data.dAmount;
        }

        a.dSum = dSum;

        spResult->vecAddresses.push_back(a);

        dTotal += dSum;
    }

    spResult->dTotal = dTotal;

    return spResult;
}

// /* Mint */
CRPCResultPtr CRPCMod::RPCGetWork(CRPCParamPtr param)
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

CRPCResultPtr CRPCMod::RPCSubmitWork(CRPCParamPtr param)
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
    uint256 hashBlock;
    Errno err = pService->SubmitWork(vchWorkData, ptr, key, hashBlock);
    if (err != OK)
    {
        throw CRPCException(RPC_INVALID_PARAMETER, string("Block rejected : ") + ErrorString(err));
    }

    return MakeCSubmitWorkResultPtr(hashBlock.GetHex());
}

CRPCResultPtr CRPCMod::RPCQueryStat(rpc::CRPCParamPtr param)
{
    enum
    {
        TYPE_NON,
        TYPE_MAKER,
        TYPE_P2PSYN
    } eType
        = TYPE_NON;
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

} // namespace bigbang
