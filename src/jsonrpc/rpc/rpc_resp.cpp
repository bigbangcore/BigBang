// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/rpc_resp.h"

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
// CRPCResp

CRPCResp::CRPCResp()
  : strJSONRPC("2.0") {}

CRPCResp::CRPCResp(const json_spirit::Value& id, CRPCResultPtr result,
                   const std::string& jsonrpc)
  : valID(id), spResult(result), strJSONRPC(jsonrpc)
{
}

CRPCResp::CRPCResp(const json_spirit::Value& id, CRPCErrorPtr err,
                   const std::string& jsonrpc)
  : valID(id), spError(err), strJSONRPC(jsonrpc)
{
}

json_spirit::Value CRPCResp::ToJSON() const
{
    json_spirit::Object obj;
    obj.push_back(json_spirit::Pair("id", valID));
    obj.push_back(json_spirit::Pair("jsonrpc", strJSONRPC));
    if (spError)
    {
        obj.push_back(json_spirit::Pair("error", spError->ToJSON()));
    }
    else if (spResult)
    {
        obj.push_back(json_spirit::Pair("result", spResult->ToJSON()));
    }

    return obj;
}

std::string CRPCResp::Serialize(bool indent) const
{
    return json_spirit::write_string<json_spirit::Value>(ToJSON(), indent, RPC_DOUBLE_PRECISION);
}

bool CRPCResp::IsError() const
{
    return (bool)spError;
}

bool CRPCResp::IsSuccessful() const
{
    return (bool)spResult;
}

CRPCRespPtr DeserializeCRPCResp(const std::string& method, const std::string& str)
{
    CRPCReqPtr req;
    if (!method.empty())
    {
        req = MakeCRPCReqPtr(json_spirit::Value(), method);
    }
    return DeserializeCRPCResp(req, str);
}

CRPCRespPtr DeserializeCRPCResp(CRPCReqPtr req, const std::string& str)
{
    CRPCReqMap m;
    if (req)
    {
        m[req->valID] = req;
    }
    CRPCRespVec vec = DeserializeCRPCResp(m, str);
    return vec[0];
}

CRPCRespVec DeserializeCRPCResp(const CRPCReqMap& req, const std::string& str)
{
    CRPCRespVec vec;

    // read from string
    json_spirit::Value valResponse;
    if (!json_spirit::read_string(str, valResponse))
    {
        throw CRPCException(RPC_PARSE_ERROR,
                            "Parse Error: response must be json string.");
    }

    // top level json type must be array or object
    if (valResponse.type() != json_spirit::array_type && valResponse.type() != json_spirit::obj_type)
    {
        throw CRPCException(
            RPC_PARSE_ERROR,
            "Parse error: response must be an object or array.");
    }

    // if top level is array, it must contain objects
    if (valResponse.type() == json_spirit::array_type)
    {
        for (auto& obj : valResponse.get_array())
        {
            if (obj.type() != json_spirit::obj_type)
            {
                throw CRPCException(
                    RPC_PARSE_ERROR,
                    "Parse error: response array must contain objects.");
            }
        }
    }

    // response number
    int respNum = 1;
    if (valResponse.type() == json_spirit::array_type)
    {
        respNum = valResponse.get_array().size();
    }

    // loop to deal every response object
    for (int i = 0; i < respNum; ++i)
    {
        const json_spirit::Object& response = (valResponse.type() == json_spirit::array_type)
                                                  ? valResponse.get_array()[i].get_obj()
                                                  : valResponse.get_obj();

        CRPCRespPtr resp = std::make_shared<CRPCResp>();

        // Parse id now so errors from here on will have the id
        resp->valID = find_value(response, "id");

        // Parse jsonrpc
        json_spirit::Value valJsonRPC = find_value(response, "jsonrpc");
        if (valJsonRPC.type() == json_spirit::str_type)
        {
            resp->strJSONRPC = valJsonRPC.get_str();
        }

        // Parse result, error
        json_spirit::Value valResult = find_value(response, "result");
        json_spirit::Value valError = find_value(response, "error");
        if (valError.is_null() && !valResult.is_null())
        {
            std::string method;
            auto it = req.find(resp->valID);
            if (it != req.end())
            {
                method = it->second->strMethod;
            }
            else if ((it = req.find(json_spirit::Value())) != req.end())
            {
                method = it->second->strMethod;
            }
            resp->spResult = CreateCRPCResult(method, valResult);
        }
        else if (valError.type() == json_spirit::obj_type && valResult.is_null())
        {
            json_spirit::Object err = valError.get_obj();
            resp->spError = std::make_shared<CRPCError>();
            try
            {
                resp->spError->nCode = find_value(err, "code").get_int();
                resp->spError->strMessage = find_value(err, "message").get_str();
                resp->spError->valData = find_value(err, "data");
            }
            catch (std::runtime_error& e)
            {
                throw CRPCException(RPC_PARSE_ERROR, e, resp->valID);
            }
        }
        else
        {
            throw CRPCException(
                RPC_PARSE_ERROR,
                std::string(
                    "'result' and 'error' must exsit one, but result type: ")
                    + json_spirit::value_type_to_string(valResult.type()) + ", error type: " + json_spirit::value_type_to_string(valError.type()),
                resp->valID);
        }

        vec.push_back(resp);
    }
    return vec;
}

std::string SerializeCRPCResp(const CRPCRespVec& resp, bool indent)
{
    json_spirit::Array arr;
    for (auto& r : resp)
    {
        arr.push_back(r->ToJSON());
    }

    return json_spirit::write_string<json_spirit::Value>(arr, indent, RPC_DOUBLE_PRECISION);
}

} // namespace rpc

} // namespace bigbang
