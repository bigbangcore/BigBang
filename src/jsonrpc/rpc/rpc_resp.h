// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef JSONRPC_RPC_RPC_RESP_H
#define JSONRPC_RPC_RPC_RESP_H

#include "json/json_spirit_value.h"

#include "rpc/rpc_error.h"
#include "rpc/rpc_req.h"

namespace bigbang
{
namespace rpc
{
/**
 * @brief A mapping of JSON-RPC v2.0 request field "param", basic class.
 */
class CRPCResult
{
public:
    virtual std::string Method() const = 0;
    virtual json_spirit::Value ToJSON() const = 0;
    virtual CRPCResult& FromJSON(const json_spirit::Value&) = 0;

public:
    std::string Serialize(bool indent = false)
    {
        auto val = ToJSON();
        if (val.type() == json_spirit::str_type)
        {
            return val.get_str();
        }
        else
        {
            return json_spirit::write_string<json_spirit::Value>(val, indent, RPC_DOUBLE_PRECISION);
        }
    }
};

typedef std::shared_ptr<CRPCResult> CRPCResultPtr;

template <typename T>
typename std::enable_if<std::is_base_of<CRPCResult, T>::value, std::shared_ptr<T>>::type
CastResultPtr(CRPCResultPtr sp)
{
    return std::dynamic_pointer_cast<T>(sp);
}

/**
 * Common result
 */
class CRPCCommonResult : public CRPCResult
{
public:
    json_spirit::Value val;

public:
    virtual std::string Method() const
    {
        return "";
    }
    virtual json_spirit::Value ToJSON() const
    {
        return val;
    }
    virtual CRPCCommonResult& FromJSON(const json_spirit::Value& v)
    {
        val = v;
        return *this;
    }
    virtual ~CRPCCommonResult() = default;
};

/**
 * @brief A mapping of JSON-RPC v2.0 response structure.
 *        JSON-RPC v2.0: https://www.jsonrpc.org/specification
 */
class CRPCResp final
{
public:
    CRPCResp();

    // success
    CRPCResp(const json_spirit::Value& id, CRPCResultPtr result,
             const std::string& jsonrpc = "2.0");

    // error
    CRPCResp(const json_spirit::Value& id, CRPCErrorPtr err,
             const std::string& jsonrpc = "2.0");

    // to json
    json_spirit::Value ToJSON() const;

    // to string
    std::string Serialize(bool indent = false) const;

    // spError != nullptr
    bool IsError() const;

    // spResult != nullptr
    bool IsSuccessful() const;

public:
    json_spirit::Value valID;
    std::string strJSONRPC;
    CRPCErrorPtr spError;
    CRPCResultPtr spResult;
};

typedef std::shared_ptr<CRPCResp> CRPCRespPtr;
typedef std::vector<CRPCRespPtr> CRPCRespVec;

// fast create shared_ptr<CRPCResp> object
template <typename... Args>
CRPCRespPtr MakeCRPCRespPtr(Args&&... args)
{
    return std::make_shared<CRPCResp>(std::forward<Args>(args)...);
}

// deserialize string to resp objects
CRPCRespPtr DeserializeCRPCResp(const std::string& method, const std::string& str);
CRPCRespPtr DeserializeCRPCResp(CRPCReqPtr req, const std::string& str);
CRPCRespVec DeserializeCRPCResp(const CRPCReqMap& req, const std::string& str);

template <typename T>
CRPCRespPtr DeserializeCRPCResp(const std::string& str)
{
    typename std::enable_if<std::is_base_of<CRPCResult, T>::value, T>::type obj;
    return DeserializeCRPCResp(obj.Method(), str);
}

// serialize a resp vector to string
std::string SerializeCRPCResp(const CRPCRespVec& resp, bool indent = false);

} // namespace rpc

} // namespace bigbang

#endif // JSONRPC_RPC_RPC_RESP_H
