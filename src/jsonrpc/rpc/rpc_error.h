// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef JSONRPC_RPC_RPC_ERROR_H
#define JSONRPC_RPC_RPC_ERROR_H

#include "json/json_spirit_writer_template.h"
#include <exception>
#include <memory>

namespace bigbang
{
namespace rpc
{

#define RPC_DOUBLE_PRECISION 6
#define RPC_MAX_DEPTH 64

enum RPCErrorCode
{
    //! Standard JSON-RPC 2.0 errors
    RPC_INVALID_REQUEST = -32600,
    RPC_METHOD_NOT_FOUND = -32601,
    RPC_INVALID_PARAMS = -32602,
    RPC_INTERNAL_ERROR = -32603,
    RPC_PARSE_ERROR = -32700,

    //! General application defined errors
    RPC_MISC_ERROR = -1,               //!< std::exception thrown in command handling
    RPC_FORBIDDEN_BY_SAFE_MODE = -2,   //!< Server is in safe mode, and command
                                       //!< is not allowed in safe mode
    RPC_TYPE_ERROR = -3,               //!< Unexpected type was passed as parameter
    RPC_INVALID_ADDRESS_OR_KEY = -4,   //!< Invalid address or key
    RPC_OUT_OF_MEMORY = -5,            //!< Ran out of memory during operation
    RPC_INVALID_PARAMETER = -6,        //!< Invalid, missing or duplicate parameter
    RPC_DATABASE_ERROR = -7,           //!< Database error
    RPC_DESERIALIZATION_ERROR = -8,    //!< Error parsing or validating structure in raw format
    RPC_VERIFY_ERROR = -9,             //!< General error during transaction or block submission
    RPC_VERIFY_REJECTED = -10,         //!< Transaction or block was rejected by network rules
    RPC_VERIFY_ALREADY_IN_CHAIN = -11, //!< Transaction already in chain
    RPC_IN_WARMUP = -12,               //!< Client still warming up
    RPC_REQUEST_ID_NOT_FOUND = -13,    //!< Request id is missing when get response
    RPC_VERSION_OUT_OF_DATE = -14,     //!< Request version is out of date

    //! Aliases for backward compatibility
    RPC_TRANSACTION_ERROR = RPC_VERIFY_ERROR,
    RPC_TRANSACTION_REJECTED = RPC_VERIFY_REJECTED,
    RPC_TRANSACTION_ALREADY_IN_CHAIN = RPC_VERIFY_ALREADY_IN_CHAIN,

    //! P2P client errors
    RPC_CLIENT_NOT_CONNECTED = -201,        //!< Bitcoin is not connected
    RPC_CLIENT_IN_INITIAL_DOWNLOAD = -202,  //!< Still downloading initial blocks
    RPC_CLIENT_NODE_ALREADY_ADDED = -203,   //!< Node is already added
    RPC_CLIENT_NODE_NOT_ADDED = -204,       //!< Node has not been added before
    RPC_CLIENT_NODE_NOT_CONNECTED = -205,   //!< Node to disconnect not found in connected nodes
    RPC_CLIENT_INVALID_IP_OR_SUBNET = -206, //!< Invalid IP/Subnet
    RPC_CLIENT_P2P_DISABLED = -207,         //!< No valid connection manager instance found

    //! Wallet errors
    RPC_WALLET_ERROR = -401,                //!< Unspecified problem with wallet (key not found etc.)
    RPC_WALLET_INSUFFICIENT_FUNDS = -402,   //!< Not enough funds in wallet or account
    RPC_WALLET_INVALID_ACCOUNT_NAME = -403, //!< Invalid account name
    RPC_WALLET_KEYPOOL_RAN_OUT = -404,      //!< Keypool ran out, call keypoolrefill first
    RPC_WALLET_UNLOCK_NEEDED = -405,        //!< Enter the wallet passphrase with walletpassphrase first
    RPC_WALLET_PASSPHRASE_INCORRECT = -406, //!< The wallet passphrase entered was incorrect
    RPC_WALLET_WRONG_ENC_STATE = -407,      //!< Command given in wrong wallet encryption state (encrypting an //!< encrypted wallet etc.)
    RPC_WALLET_ENCRYPTION_FAILED = -408,    //!< Failed to encrypt the wallet
    RPC_WALLET_ALREADY_UNLOCKED = -409,     //!< Wallet is already unlocked
    RPC_WALLET_INVALID_LABEL_NAME = -410,   //!< Invalid label name
    RPC_WALLET_INVALID_PASSPHRASE = -411    //!< Invalid passphrase
};

/**
 * @brief A mapping of JSON-RPC v2.0 response field "error", basic class.
 */
class CRPCError
{
public:
    CRPCError();
    CRPCError(const CRPCError& e);
    CRPCError(int code, std::string message = "",
              json_spirit::Value data = json_spirit::Value());

    json_spirit::Value ToJSON() const;
    std::string Serialize(bool indent = false) const;

public:
    int nCode;
    std::string strMessage;
    json_spirit::Value valData;
};
typedef std::shared_ptr<CRPCError> CRPCErrorPtr;

// fast create shared_ptr<CRPCReq> object
template <typename... Args>
CRPCErrorPtr MakeCRPCErrorPtr(Args&&... args)
{
    return std::make_shared<CRPCError>(std::forward<Args>(args)...);
}

/**
 * json rpc excetion
 */
class CRPCException : public std::exception, public CRPCError
{
public:
    CRPCException() = delete;
    CRPCException(const CRPCException& e);
    CRPCException(int error_code, const std::string& msg,
                  json_spirit::Value data = json_spirit::Value());
    CRPCException(int error_code, const std::runtime_error& err,
                  json_spirit::Value data = json_spirit::Value());

    virtual const char* what() const noexcept;

private:
    std::string strError;
};

} // namespace rpc

} // namespace bigbang

#endif // JSONRPC_RPC_RPC_ERROR_H
