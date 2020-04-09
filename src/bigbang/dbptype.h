// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_DBP_TYPE_H
#define BIGBANG_DBP_TYPE_H

#include <boost/any.hpp>

namespace bigbang
{

class CBbDbpContent
{
public:
};

class CBbDbpRequest : public CBbDbpContent
{
public:
};

class CBbDbpRespond : public CBbDbpContent
{
public:
};

class CBbDbpConnect : public CBbDbpRequest
{
public:
    bool isReconnect;
    std::string session;
    int32 version;
    std::string client;
    std::string forks; // supre node child node fork ids
};

class CBbDbpSub : public CBbDbpRequest
{
public:
    std::string id;
    std::string name;
};

class CBbDbpUnSub : public CBbDbpRequest
{
public:
    std::string id;
};

class CBbDbpNoSub : public CBbDbpRespond
{
public:
    std::string id;
    std::string error;
};

class CBbDbpReady : public CBbDbpRespond
{
public:
    std::string id;
};


class CBbDbpTxIn
{
public:
    std::vector<uint8> hash;
    uint32 n;
};

class CBbDbpDestination
{
public:
    enum PREFIX
    {
        PREFIX_NULL = 0,
        PREFIX_PUBKEY = 1,
        PREFIX_TEMPLATE = 2,
        PREFIX_MAX = 3
    };

public:
    uint32 prefix;
    std::vector<uint8> data;
    uint32 size; //设置为33
};

class CBbDbpTransaction
{
public:
    uint32 nVersion;               //版本号,目前交易版本为 0x0001
    uint32 nType;                  //类型, 区分公钥地址交易、模板地址交易、即时业务交易和跨分支交易
    uint32 nLockUntil;             //交易冻结至高度为 nLockUntil 区块
    std::vector<uint8> hashAnchor; //交易有效起始区块 HASH
    std::vector<CBbDbpTxIn> vInput;
    CBbDbpDestination cDestination; // 输出地址
    int64 nAmount;                  //输出金额
    int64 nTxFee;                   //网络交易费
    int64 nChange;                  //余额
    std::vector<uint8> vchData;     //输出参数(模板地址参数、跨分支交易共轭交易)
    std::vector<uint8> vchSig;      //交易签名
    std::vector<uint8> hash;
};

class CBbDbpBlock
{
public:
    uint32 nVersion;
    uint32 nType;                       // 类型,区分创世纪块、主链区块、业务区块和业务子区块
    uint32 nTimeStamp;                  //时间戳，采用UTC秒单位
    std::vector<uint8> hashPrev;        //前一区块的hash
    std::vector<uint8> hashMerkle;      //Merkle tree的根
    std::vector<uint8> vchProof;        //用于校验共识合法性数据
    CBbDbpTransaction txMint;           // 出块奖励交易
    std::vector<CBbDbpTransaction> vtx; //区块打包的所有交易
    std::vector<uint8> vchSig;          //区块签名
    uint32 nHeight;                     // 当前区块高度
    std::vector<uint8> hash;            //当前区块的hash
};

class CBbDbpAdded : public CBbDbpRespond
{
public:
    std::string name;
    std::string id;
    std::string forkid;
    boost::any anyAddedObj; // busniess object (block,tx...)
};

class CBbDbpMethod : public CBbDbpRequest
{
public:
    enum Method
    {
        GET_BLOCKS,
        GET_TRANSACTION,
        SEND_TRANSACTION,
        SEND_TX, // supernode 
        REGISTER_FORK,
        SEND_BLOCK
    };

    // param name => param value
    typedef std::map<std::string, boost::any> ParamMap;

public:
    Method method;
    std::string id;
    ParamMap params;
};

class CBbDbpSendTransactionRet
{
public:
    std::string hash;
    std::string result;
    std::string reason;
};

class CBbDbpRegisterForkIDRet
{
public:
    std::string forkid;
};

class CBbDbpSendBlockRet
{
public:
    std::string hash;
};

class CBbDbpSendTxRet
{
public:
    std::string hash;
};

class CBbDbpRegisterForkID : public CBbDbpRequest
{
public:
    std::string forkid;
};

class CBbDbpSendBlock : public CBbDbpRequest
{
public:
    boost::any block;
};

class CBbDbpSendTx : public CBbDbpRequest
{
public:
    boost::any tx;
};

class CBbDbpMethodResult : public CBbDbpRespond
{
public:
    std::string id;
    std::string error;
    std::vector<boost::any> anyResultObjs; // blocks,tx,send_tx_ret
};

class CBbDbpError : public CBbDbpRespond
{
public:
};

class CBbDbpConnected : public CBbDbpRespond
{
public:
    std::string session;
};

class CBbDbpPing : public CBbDbpRequest, public CBbDbpRespond
{
public:
    std::string id;
};

class CBbDbpPong : public CBbDbpRequest, public CBbDbpRespond
{
public:
    std::string id;
};

class CBbDbpFailed : public CBbDbpRespond
{
public:
    std::string reason;  // failed reason
    std::string session; // for delete session map
    std::vector<int32> versions;
};

class CBbDbpBroken
{
public:
    std::string session;
};

class CBbDbpRemoveSession
{
public:
    std::string session;
};
} // namespace bigbang
#endif //BIGBANG_DBP_TYPE_H
