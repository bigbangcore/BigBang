// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_RPCMOD_H
#define BIGBANG_RPCMOD_H

#include "json/json_spirit.h"
#include <boost/function.hpp>
#include <map>

#include "base.h"
#include "message.h"
#include "rpc/rpc.h"
#include "xengine.h"

namespace bigbang
{

class CRPCMod : public xengine::CIOActor
{
protected:
    typedef rpc::CRPCResultPtr (CRPCMod::*RPCFunc)(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    typedef rpc::CRPCResultPtr (CRPCMod::*RPCMessageFunc)(const xengine::CMessage& msg);

public:
    CRPCMod();
    ~CRPCMod();

protected:
    void HandleHttpReq(const xengine::CHttpReqMessage& msg);
    void HandleHttpBroken(const xengine::CHttpBrokenMessage& msg);

    void HandleAddedBlockMsg(const CAddedBlockMessage& msg);
    void HandleAddedTxMsg(const CAddedTxMessage& msg);

    rpc::CRPCRespPtr StartWork(CNoncePtr spNonce, rpc::CRPCReqPtr spReq);
    void HandleAddedMsg(xengine::CNoncePtr spNonce, size_t nIndex, const uint256& hash, const CMessage& msg);
    void CompletedWork(xengine::CNoncePtr spNonce, size_t nIndex, rpc::CRPCRespPtr spResp);

protected:
    bool HandleInitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;
    void HandleDeinitialize() override;
    bool EnterLoop() override;
    void LeaveLoop() override;

    const CNetworkConfig* Config()
    {
        return dynamic_cast<const CNetworkConfig*>(xengine::IBase::Config());
    }
    const CRPCServerConfig* RPCServerConfig()
    {
        return dynamic_cast<const CRPCServerConfig*>(IBase::Config());
    }

    void JsonReply(xengine::CNoncePtr spNonce, const std::string& result);

    int GetInt(const rpc::CRPCInt64& i, int valDefault)
    {
        return i.IsValid() ? int(i) : valDefault;
    }
    unsigned int GetUint(const rpc::CRPCUint64& i, unsigned int valDefault)
    {
        return i.IsValid() ? uint64(i) : valDefault;
    }
    bool GetForkHashOfDef(const rpc::CRPCString& hex, uint256& hashFork)
    {
        if (!hex.empty())
        {
            if (hashFork.SetHex(hex) != hex.size())
            {
                return false;
            }
        }
        else
        {
            hashFork = pCoreProtocol->GetGenesisBlockHash();
        }
        return true;
    }
    bool CheckWalletError(Errno err);
    bigbang::crypto::CPubKey GetPubKey(const std::string& addr);
    void ListDestination(std::vector<CDestination>& vDestination);
    bool CheckVersion(std::string& strVersion);
    std::string GetWidthString(const std::string& strIn, int nWidth);
    std::string GetWidthString(uint64 nCount, int nWidth);

    /// Remove all sensible information such as private key or passphrass from log content
    std::string MaskSensitiveData(std::string strData);

protected:
    /* System */
    rpc::CRPCResultPtr RPCHelp(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCStop(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCVersion(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    /* Network */
    rpc::CRPCResultPtr RPCGetPeerCount(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCListPeer(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCAddNode(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCRemoveNode(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    /* Worldline & TxPool */
    rpc::CRPCResultPtr RPCGetForkCount(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCListFork(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCGetForkGenealogy(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCGetBlockLocation(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCGetBlockCount(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCGetBlockHash(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCGetBlock(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCGetTxPool(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    // CRPCResultPtr RPCRemovePendingTx(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCGetTransaction(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCSendTransaction(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCGetForkHeight(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    /* Wallet */
    rpc::CRPCResultPtr RPCListKey(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCGetNewKey(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCEncryptKey(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCLockKey(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCUnlockKey(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCImportPrivKey(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCImportKey(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCExportKey(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCAddNewTemplate(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCImportTemplate(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCExportTemplate(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCValidateAddress(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCResyncWallet(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCGetBalance(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCListTransaction(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCSendFrom(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCCreateTransaction(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCSignTransaction(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCSignMessage(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCListAddress(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCExportWallet(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCImportWallet(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCMakeOrigin(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    /* Util */
    rpc::CRPCResultPtr RPCVerifyMessage(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCMakeKeyPair(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCGetPubKeyAddress(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCGetTemplateAddress(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCMakeTemplate(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCDecodeTransaction(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    /* Mint */
    rpc::CRPCResultPtr RPCGetWork(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCSubmitWork(CNoncePtr spNonce, rpc::CRPCParamPtr param);
    rpc::CRPCResultPtr RPCQueryStat(CNoncePtr spNonce, rpc::CRPCParamPtr param);

protected:
    rpc::CRPCResultPtr RPCMsgSubmitWork(const xengine::CMessage& msg);
    rpc::CRPCResultPtr RPCMsgSendFrom(const xengine::CMessage& msg);
    rpc::CRPCResultPtr RPCMsgSendTransaction(const xengine::CMessage& msg);

protected:
    xengine::CIOProc* pHttpServer;
    ICoreProtocol* pCoreProtocol;
    IService* pService;
    IDataStat* pDataStat;

    std::map<std::string, RPCFunc> mapRPCFunc;
    std::map<std::string, RPCMessageFunc> mapRPCMessageFunc;

protected:
    struct CWork
    {
        size_t nRemainder;
        bool fArray;
        rpc::CRPCReqVec vecReq;
        rpc::CRPCRespVec vecResp;
        std::multimap<uint256, size_t> mapHash;
    };

    std::map<xengine::CNoncePtr, CWork> mapWork;
};

} // namespace bigbang

#endif //BIGBANG_RPCMOD_H
