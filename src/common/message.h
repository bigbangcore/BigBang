// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_MESSAGE_H
#define COMMON_MESSAGE_H

#include <uint256.h>
#include <vector>
#include <map>
#include <memory>

#include "block.h"
#include "message/message.h"
#include "peerevent.h"
#include "proto.h"
#include "struct.h"

/////////// Peer-to-Peer /////////////

struct CPeerBasicMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerBasicMessage);
    uint64 nNonce;
    uint256 hashFork;
};

struct CPeerActiveMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerActiveMessage);
    bigbang::network::CAddress address;
};

struct CPeerDeactiveMessage : public CPeerBasicMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerDeactiveMessage);
    bigbang::network::CAddress address;
};

struct CPeerSubscribeMessageInBound : public CPeerBasicMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerSubscribeMessageInBound);
    std::vector<uint256> vecForks;
};

struct CPeerSubscribeMessageOutBound : public CPeerSubscribeMessageInBound
{
    GENERATE_MESSAGE_FUNCTION(CPeerSubscribeMessageOutBound);
};

struct CPeerUnsubscribeMessageInBound : public CPeerBasicMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerUnsubscribeMessageInBound);
    std::vector<uint256> vecForks;
};

struct CPeerUnsubscribeMessageOutBound : public CPeerUnsubscribeMessageInBound
{
    GENERATE_MESSAGE_FUNCTION(CPeerUnsubscribeMessageOutBound);
};

struct CPeerGetBlocksMessageInBound : public CPeerBasicMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerGetBlocksMessageInBound);
    CBlockLocator blockLocator;
};

struct CPeerGetBlocksMessageOutBound : public CPeerGetBlocksMessageInBound
{
    GENERATE_MESSAGE_FUNCTION(CPeerGetBlocksMessageOutBound);
};

struct CPeerGetDataMessageInBound : public CPeerBasicMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerGetDataMessageInBound);
    std::vector<bigbang::network::CInv> vecInv;
};

struct CPeerGetDataMessageOutBound : public CPeerGetDataMessageInBound
{
    GENERATE_MESSAGE_FUNCTION(CPeerGetDataMessageOutBound);
};

struct CPeerInvMessageInBound : public CPeerBasicMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerInvMessageInBound);
    std::vector<bigbang::network::CInv> vecInv;
};

struct CPeerInvMessageOutBound : public CPeerInvMessageInBound
{
    GENERATE_MESSAGE_FUNCTION(CPeerInvMessageOutBound);
};

struct CPeerTxMessageInBound : public CPeerBasicMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerTxMessageInBound);
    CTransaction tx;
};

struct CPeerTxMessageOutBound : public CPeerTxMessageInBound
{
    GENERATE_MESSAGE_FUNCTION(CPeerTxMessageOutBound);
};

struct CPeerBlockMessageInBound : public CPeerBasicMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerBlockMessageInBound);
    CBlock block;
};

struct CPeerBlockMessageOutBound : public CPeerBlockMessageInBound
{
    GENERATE_MESSAGE_FUNCTION(CPeerBlockMessageOutBound);
};

struct CBroadcastBlockInvMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CBroadcastBlockInvMessage);
    CBroadcastBlockInvMessage(const uint256& hashForkIn, const uint256& hashBlockIn)
      : hashFork(hashForkIn), hashBlock(hashBlockIn) {}
    uint256 hashFork;
    uint256 hashBlock;
};

struct CBroadcastTxInvMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CBroadcastTxInvMessage);
    CBroadcastTxInvMessage(const uint256& hashForkIn)
      : hashFork(hashForkIn) {}
    uint256 hashFork;
};

struct CSubscribeForkMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CSubscribeForkMessage);
    CSubscribeForkMessage(const uint256& hashForkIn, uint64 nNonceIn)
      : hashFork(hashForkIn), nNonce(nNonceIn) {}
    uint256 hashFork;
    uint64 nNonce;
};

struct CUnsubscribeForkMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CUnsubscribeForkMessage);
    CUnsubscribeForkMessage(const uint256& hashForkIn)
      : hashFork(hashForkIn) {}
    uint256 hashFork;
};

////////////////// Delegate /////////////////////

struct CPeerDelegateBasicMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerDelegateBasicMessage);
    uint64 nNonce;
    uint256 hashAnchor;
};

struct CPeerBulletinMessageInBound : public CPeerDelegateBasicMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerBulletinMessageInBound);
    bigbang::network::CEventPeerDelegatedBulletin deletegatedBulletin;
};

struct CPeerBulletinMessageOutBound : public CPeerBulletinMessageInBound
{
    GENERATE_MESSAGE_FUNCTION(CPeerBulletinMessageOutBound);
};

struct CPeerGetDelegatedMessageInBound : public CPeerDelegateBasicMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerGetDelegatedMessageInBound);
    bigbang::network::CEventPeerDelegatedGetData delegatedGetData;
};

struct CPeerGetDelegatedMessageOutBound : public CPeerGetDelegatedMessageInBound
{
    GENERATE_MESSAGE_FUNCTION(CPeerGetDelegatedMessageOutBound);
};

struct CPeerDistributeMessageInBound : public CPeerDelegateBasicMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerDistributeMessageInBound);
    bigbang::network::CEventPeerDelegatedData delegatedData;
};

struct CPeerDistributeMessageOutBound : public CPeerDistributeMessageInBound
{
    GENERATE_MESSAGE_FUNCTION(CPeerDistributeMessageOutBound);
};

struct CPeerPublishMessageInBound : public CPeerDelegateBasicMessage
{
    GENERATE_MESSAGE_FUNCTION(CPeerPublishMessageInBound);
    bigbang::network::CEventPeerDelegatedData delegatedData;
};

struct CPeerPublishMessageOutBound : public CPeerPublishMessageInBound
{
    GENERATE_MESSAGE_FUNCTION(CPeerPublishMessageOutBound);
};

struct CCDelegateRoutineMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CCDelegateRoutineMessage);
    int nStartHeight;
    CDelegateRoutine routine;
};

struct CAddNewDistributeMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CAddNewDistributeMessage);
    uint64 nNonce;
    uint256 hashAnchor;
    CDestination dest;
    std::vector<unsigned char> vchDistribute;
};

struct CAddedNewDistributeMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CAddedNewDistributeMessage);
    uint64 nNonce;
    uint256 hashAnchor;
    CDestination dest;
    std::vector<unsigned char> vchDistribute;
    bool fResult;
};

struct CAddNewPublishMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CAddNewPublishMessage);
    uint64 nNonce;
    uint256 hashAnchor;
    CDestination dest;
    std::vector<unsigned char> vchPublish;
};

struct CAddedNewPublishMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CAddedNewPublishMessage);
    uint64 nNonce;
    uint256 hashAnchor;
    CDestination dest;
    std::vector<unsigned char> vchPublish;
    bool fResult;
};

//////////// Transaction ///////////////////

/// Add an unconfirmed transaction.
struct CAddTxMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CAddTxMessage);
    std::shared_ptr<CNonce> spNonce;
    uint256 hashFork;
    CTransaction tx;
};

/// Added an unconfirmed transaction.
struct CAddedTxMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CAddedTxMessage);
    std::shared_ptr<CNonce> spNonce;
    int nErrno;
    uint256 hashFork;
    CAssembledTx tx;
    CDestination destIn;
    int64 nValueIn;
};

/// Remove an unconfirmed transaction.
struct CRemoveTxMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CRemoveTxMessage);
    uint256 hashFork;
    uint256 txId;
};

/// Clear unconfirmed transactions. If hashFork equals 0, clear all forks.
struct CClearTxMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CClearTxMessage);
    uint256 hashFork;
};

/// Synchronize changed transactions.
struct CSyncTxChangeMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CSyncTxChangeMessage);
    uint256 hashFork;
    CTxSetChange change;
    CWorldLineUpdate update;
};

////////////////// Block /////////////////////

/// Add a new block.
struct CAddBlockMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CAddBlockMessage);
    std::shared_ptr<CNonce> spNonce;
    uint256 hashFork;
    CBlock block;
};

/// Added a new block.
struct CAddedBlockMessage : public xengine::CMessage
{
    GENERATE_MESSAGE_FUNCTION(CAddedBlockMessage);
    std::shared_ptr<CNonce> spNonce;
    uint256 hashFork;
    CBlock block;
    int nErrno;
    CWorldLineUpdate update;
};

#endif // COMMON_MESSAGE_H
