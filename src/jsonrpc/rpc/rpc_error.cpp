// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/rpc_error.h"

namespace bigbang
{
namespace rpc
{

///////////////////////////////////////////////////////
// CRPCError

CRPCError::CRPCError() {}

CRPCError::CRPCError(const CRPCError& e)
  : nCode(e.nCode), strMessage(e.strMessage), valData(e.valData)
{
}

CRPCError::CRPCError(int code, std::string message, json_spirit::Value data)
  : nCode(code), strMessage(message), valData(data)
{
}

json_spirit::Value CRPCError::ToJSON() const
{
    json_spirit::Object obj;
    obj.push_back(json_spirit::Pair("code", nCode));
    obj.push_back(json_spirit::Pair("message", strMessage));
    if (!valData.is_null())
    {
        obj.push_back(json_spirit::Pair("data", valData));
    }
    return obj;
}

std::string CRPCError::Serialize(bool indent) const
{
    auto val = ToJSON();
    return json_spirit::write_string<json_spirit::Value>(val, indent, RPC_DOUBLE_PRECISION);
}

///////////////////////////////////////////////////////
// CRPCException

CRPCException::CRPCException(const CRPCException& e)
  : strError(e.strError), CRPCError(e)
{
}

CRPCException::CRPCException(int error_code, const std::string& msg,
                             json_spirit::Value data)
  : CRPCError(error_code, msg, data)
{
    strError = Serialize();
}

CRPCException::CRPCException(int error_code, const std::runtime_error& err,
                             json_spirit::Value data)
  : CRPCException(error_code, err.what(), data)
{
}

const char* CRPCException::what() const noexcept
{
    return strError.c_str();
}

} // namespace rpc

} // namespace bigbang
