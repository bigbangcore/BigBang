// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/rpc_req.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_utils.h"
#include <exception>
#include <memory>

#include "rpc/auto_protocol.h"

namespace bigbang
{
namespace rpc
{
///////////////////////////////////////////////////
// CRPCReq

CRPCReq::CRPCReq()
  : strJSONRPC("2.0") {}

CRPCReq::CRPCReq(const json_spirit::Value& id, const std::string& method,
                 CRPCParamPtr param, const std::string& jsonrpc)
  : valID(id), strMethod(method), spParam(param), strJSONRPC(jsonrpc)
{
}

CRPCReq::CRPCReq(const json_spirit::Value& id, CRPCParamPtr param,
                 const std::string& jsonrpc)
  : valID(id), spParam(param), strJSONRPC(jsonrpc)
{
    if (spParam)
    {
        strMethod = spParam->Method();
    }
}

json_spirit::Value CRPCReq::ToJSON() const
{
    json_spirit::Object obj;
    obj.push_back(json_spirit::Pair("id", valID));
    obj.push_back(json_spirit::Pair("method", strMethod));
    obj.push_back(json_spirit::Pair("jsonrpc", strJSONRPC));
    if (spParam)
    {
        obj.push_back(json_spirit::Pair("params", spParam->ToJSON()));
    }

    return obj;
}

std::string CRPCReq::Serialize(bool indent)
{
    return json_spirit::write_string<json_spirit::Value>(ToJSON(), indent, RPC_DOUBLE_PRECISION);
}

CRPCReqVec DeserializeCRPCReq(const std::string& str, bool& fArray)
{
    CRPCReqVec vec;

    // read from string
    json_spirit::Value valRequest;
    if (!json_spirit::read_string(str, valRequest))
    {
        throw CRPCException(RPC_PARSE_ERROR,
                            "Parse Error: request must be json string.");
    }

    // top level json type must be array or object
    if (valRequest.type() != json_spirit::array_type && valRequest.type() != json_spirit::obj_type)
    {
        throw CRPCException(RPC_PARSE_ERROR,
                            "Parse error: request must be an object or array.");
    }

    // if top level is array, it must contain objects
    if (valRequest.type() == json_spirit::array_type)
    {
        for (auto& obj : valRequest.get_array())
        {
            if (obj.type() != json_spirit::obj_type)
            {
                throw CRPCException(
                    RPC_PARSE_ERROR,
                    "Parse error: request array must contain objects.");
            }
        }
    }

    // request number
    int reqNum = 1;
    if (valRequest.type() == json_spirit::array_type)
    {
        reqNum = valRequest.get_array().size();
    }
    if (reqNum == 0)
    {
        throw CRPCException(RPC_INVALID_REQUEST, "Empty request");
    }

    // loop to deal every request object
    for (int i = 0; i < reqNum; ++i)
    {
        const json_spirit::Object& request = (valRequest.type() == json_spirit::array_type)
                                                 ? valRequest.get_array()[i].get_obj()
                                                 : valRequest.get_obj();

        CRPCReqPtr req = std::make_shared<CRPCReq>();

        // Parse id now so errors from here on will have the id
        req->valID = find_value(request, "id");
        if (req->valID.type() != json_spirit::int_type && req->valID.type() != json_spirit::str_type && !req->valID.is_null())
        {
            throw CRPCException(RPC_INVALID_REQUEST, "ID type error");
        }

        // Parse jsonrpc
        json_spirit::Value valJsonRPC = find_value(request, "jsonrpc");
        if (valJsonRPC.type() == json_spirit::str_type)
        {
            req->strJSONRPC = valJsonRPC.get_str();
        }

        // Parse method
        json_spirit::Value valMethod = find_value(request, "method");
        if (valMethod.is_null())
        {
            throw CRPCException(RPC_INVALID_REQUEST, "Missing method", req->valID);
        }
        if (valMethod.type() != json_spirit::str_type)
        {
            throw CRPCException(RPC_INVALID_REQUEST,
                                std::string("'method' must be a string, but it's ") + json_spirit::value_type_to_string(valMethod.type()), req->valID);
        }
        req->strMethod = valMethod.get_str();

        // Parse params
        json_spirit::Value valParams = find_value(request, "params");
        try
        {
            req->spParam = CreateCRPCParam(req->strMethod, valParams);
        }
        catch (CRPCException& e)
        {
            e.valData = req->valID;
            throw e;
        }
        catch (std::runtime_error& e)
        {
            throw CRPCException(RPC_INVALID_REQUEST, e, req->valID);
        }

        vec.push_back(req);
    }

    fArray = (valRequest.type() == json_spirit::array_type);
    return vec;
}

std::string SerializeCRPCReq(const CRPCReqVec& req, bool indent)
{
    json_spirit::Array arr;
    for (auto& r : req)
    {
        arr.push_back(r->ToJSON());
    }
    return json_spirit::write_string<json_spirit::Value>(arr, indent, RPC_DOUBLE_PRECISION);
}

} // namespace rpc

} // namespace bigbang
