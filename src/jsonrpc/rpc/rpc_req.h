// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef JSONRPC_RPC_RPC_REQ_H
#define JSONRPC_RPC_RPC_REQ_H

#include "json/json_spirit_value.h"
#include "json/json_spirit_writer_template.h"
#include <boost/program_options.hpp>
#include <map>
#include <memory>
#include <vector>

#include "rpc/rpc_error.h"

namespace bigbang
{
namespace rpc
{
/**
 * @brief A mapping of JSON-RPC v2.0 request field "param", basic class.
 */
class CRPCParam
{
public:
    virtual std::string Method() const = 0;
    virtual json_spirit::Value ToJSON() const = 0;
    virtual CRPCParam& FromJSON(const json_spirit::Value&) = 0;
};

typedef std::shared_ptr<CRPCParam> CRPCParamPtr;

template <typename T>
inline
    typename std::enable_if<std::is_base_of<CRPCParam, T>::value, std::shared_ptr<T>>::type
    CastParamPtr(CRPCParamPtr spParam)
{
    auto sp = std::dynamic_pointer_cast<T>(spParam);
    if (!sp)
    {
        throw CRPCException(RPC_MISC_ERROR, "pointer cast error.");
    }
    return sp;
}

/**
 * @brief A mapping of JSON-RPC v2.0 request structure.
 *        JSON-RPC v2.0: https://www.jsonrpc.org/specification
 */
class CRPCReq final
{
public:
    CRPCReq();

    // create
    CRPCReq(const json_spirit::Value& id, const std::string& strMethod,
            CRPCParamPtr param = nullptr, const std::string& jsonrpc = "2.0");

    CRPCReq(const json_spirit::Value& id, CRPCParamPtr param, const std::string& jsonrpc = "2.0");

    // to json object
    json_spirit::Value ToJSON() const;

    // to string
    std::string Serialize(bool indent = false);

public:
    json_spirit::Value valID;
    std::string strJSONRPC;
    std::string strMethod;
    CRPCParamPtr spParam;
};

typedef std::shared_ptr<CRPCReq> CRPCReqPtr;
typedef std::vector<CRPCReqPtr> CRPCReqVec;

class CompJsonValue
{
public:
    bool operator()(const json_spirit::Value& lhs, const json_spirit::Value& rhs) const
    {
        return json_spirit::write_string(lhs, false, RPC_DOUBLE_PRECISION) < json_spirit::write_string(rhs, false, RPC_DOUBLE_PRECISION);
    }
};
typedef std::map<json_spirit::Value, CRPCReqPtr, CompJsonValue> CRPCReqMap;

// fast create shared_ptr<CRPCReq> object
template <typename... Args>
CRPCReqPtr MakeCRPCReqPtr(Args&&... args)
{
    return std::make_shared<CRPCReq>(std::forward<Args>(args)...);
}

// deserialize string to req objects
CRPCReqVec DeserializeCRPCReq(const std::string& str, bool& fArray);

// serialize a req vector to string
std::string SerializeCRPCReq(const CRPCReqVec& req, bool indent = false);

} // namespace rpc

} // namespace bigbang

#endif // JSONRPC_RPC_RPC_REQ_H
