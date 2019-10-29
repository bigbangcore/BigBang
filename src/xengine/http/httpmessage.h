// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_HTTP_HTTPMESSAGE_H
#define XENGINE_HTTP_HTTPMESSAGE_H

#include <map>
#include <string.h>
#include <vector>

#include "http/httpcookie.h"
#include "http/httptype.h"
#include "message/message.h"
#include "stream/stream.h"

namespace xengine
{

// class _iless
// {
// public:
//     bool operator()(const std::string& a, const std::string& b) const
//     {
//         return (strcasecmp(a.c_str(), b.c_str()) < 0);
//     }
// };

// typedef std::map<std::string, std::string> MAPKeyValue;
// typedef std::map<std::string, std::string, _iless> MAPIKeyValue;
// typedef std::map<std::string, CHttpCookie, _iless> MAPCookie;

// struct CHttpContent
// {
//     std::string strContentType;
//     std::string strContent;

// protected:
//     void Serialize(CStream& s, SaveType&)
//     {
//         s.Write(&strContent[0], strContent.size());
//     }
//     void Serialize(CStream& s, LoadType&)
//     {
//         std::size_t size = s.GetSize();
//         strContent.resize(size);
//         s.Read(&strContent[0], size);
//     }
//     void Serialize(CStream& s, std::size_t& serSize)
//     {
//         (void)s;
//         serSize += strContent.size();
//     }

// private:
//     friend class CStream;
// };

struct CHttpReqMessage : public CHttpContent, public CMessage
{
    GENERATE_MESSAGE_FUNCTION(CHttpReqMessage);

    uint64 nNonce;
    std::string strUser;
    MAPIKeyValue mapHeader;
    MAPKeyValue mapQuery;
    MAPIKeyValue mapCookie;
    int64 nTimeout;
};

struct CHttpReqDataMessage : public CHttpReqMessage
{
    GENERATE_MESSAGE_FUNCTION(CHttpReqDataMessage);

    std::string strIOModule;
    std::string strProtocol;
    std::string strPathCA;
    std::string strPathCert;
    std::string strPathPK;
    bool fVerifyPeer;
};

struct CHttpRspMessage : public CHttpContent, public CMessage
{
    GENERATE_MESSAGE_FUNCTION(CHttpRspMessage);

    uint64 nNonce;
    MAPIKeyValue mapHeader;
    MAPCookie mapCookie;
    int nStatusCode;
};

struct CHttpAbortMessage : public CMessage
{
    GENERATE_MESSAGE_FUNCTION(CHttpAbortMessage);

    std::string strIOModule;
    std::vector<uint64> vNonce;
};

struct CHttpBrokenMessage : public CMessage
{
    GENERATE_MESSAGE_FUNCTION(CHttpBrokenMessage);

    bool fEventStream;
};

} // namespace xengine

#endif //XENGINE_HTTP_HTTPMESSAGE_H
