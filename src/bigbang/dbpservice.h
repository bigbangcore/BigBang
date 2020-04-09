// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_DBP_SERVICE_H
#define BIGBANG_DBP_SERVICE_H

#include "base.h"
#include "dbpserver.h"
#include "event.h"
#include "peernet.h"
//#include "blockhead.h"

#include <set>
#include <utility>
#include <unordered_map>

using namespace xengine;

namespace bigbang
{

using namespace network;

class CDbpService : public IIOModule, virtual public CDBPEventListener, virtual public CBbDBPEventListener
{
public:
    CDbpService();
    virtual ~CDbpService() noexcept;

    bool HandleEvent(CBbEventDbpConnect& event) override;
    bool HandleEvent(CBbEventDbpSub& event) override;
    bool HandleEvent(CBbEventDbpUnSub& event) override;
    bool HandleEvent(CBbEventDbpMethod& event) override;
    bool HandleEvent(CBbEventDbpPong& event) override;
    bool HandleEvent(CBbEventDbpBroken& event) override;
    bool HandleEvent(CBbEventDbpAdded& event) override;
    bool HandleEvent(CBbEventDbpRemoveSession& event) override;
    // client post evnet register fork id
    bool HandleEvent(CBbEventDbpRegisterForkID& event) override;

    // notify add msg(block tx ...) to event handler
    bool HandleEvent(CBbEventDbpUpdateNewBlock& event) override;
    bool HandleEvent(CBbEventDbpUpdateNewTx& event) override;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;

private:
    void CreateDbpBlock(const CBlockEx& blockDetail, const uint256& forkHash,
                      int blockHeight, CBbDbpBlock& block);
    void CreateDbpTransaction(const CTransaction& tx, int64 nChange, CBbDbpTransaction& dbptx);
    bool CalcForkPoints(const uint256& forkHash);
    void TrySwitchFork(const uint256& blockHash, uint256& forkHash);
    bool GetBlocks(const uint256& forkHash, const uint256& startHash, int32 n, std::vector<CBbDbpBlock>& blocks);
    bool IsEmpty(const uint256& hash);
    bool IsForkHash(const uint256& hash);
    void HandleGetBlocks(CBbEventDbpMethod& event);
    void HandleGetTransaction(CBbEventDbpMethod& event);
    void HandleSendTransaction(CBbEventDbpMethod& event);
    void HandleRegisterFork(CBbEventDbpMethod& event);
    void HandleSendBlock(CBbEventDbpMethod& event);
    void HandleSendTx(CBbEventDbpMethod& event);

    bool IsTopicExist(const std::string& topic);
    bool IsHaveSubedTopicOf(const std::string& id);

    void SubTopic(const std::string& id, const std::string& session, const std::string& topic);
    void UnSubTopic(const std::string& id);
    void RemoveSession(const std::string& session);

    void PushBlock(const std::string& forkid, const CBbDbpBlock& block);
    void PushTx(const std::string& forkid, const CBbDbpTransaction& dbptx);

    ///////////  super node  ////////////
    void UpdateChildNodeForks(const std::string& session, const std::string& forks);
protected:
    IIOProc* pDbpServer;
    IIOProc* pDbpClient;
    IService* pService;
    ICoreProtocol* pCoreProtocol;
    IWallet* pWallet;
    INetChannel* pNetChannel;

private:
    std::map<std::string, std::string> mapIdSubedTopic; // id => subed topic

    std::set<std::string> setSubedAllBlocksIds; // block ids
    std::set<std::string> setSubedAllTxIds;     // tx ids

    typedef std::set<std::string> ForksType;
    std::map<std::string, ForksType> mapSessionChildNodeForks; // session => child node forks
    ForksType setThisNodeForks;    // this node support forks

    std::map<std::string, std::string> mapIdSubedSession;       // id => session
    std::unordered_map<std::string, bool> mapCurrentTopicExist; // topic => enabled

    std::unordered_map<std::string, std::pair<uint256,uint256>> mapForkPoint; // fork point hash => (fork hash, fork point hash)
};

} // namespace bigbang

#endif //BIGBANG_DBP_SERVICE_H
