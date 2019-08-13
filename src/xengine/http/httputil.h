// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_HTTP_HTTPUTIL_H
#define XENGINE_HTTP_HTTPUTIL_H

#include <boost/algorithm/string.hpp>
#include <boost/date_time.hpp>
#include <map>
#include <string>

#include "http/httpcookie.h"
#include "http/httptype.h"

namespace xengine
{

class CHttpUtil
{
public:
    const boost::posix_time::ptime ParseRFC1123DayTime(const std::string& strDayTime);
    const std::string FormatRFC1123DayTime(const boost::posix_time::ptime& pt = boost::posix_time::second_clock::universal_time());
    void Base64Encode(const std::string& strDecoded, std::string& strEncoded);
    bool Base64Decode(const std::string& strEncoded, std::string& strDecoded);
    void PercentEncode(const std::string& strDecoded, std::string& strEncoded,
                       const std::string& strResvExt = "");
    bool PercentDecode(const std::string& strEncoded, std::string& strDecoded);
    bool ParseEncodedPair(const std::string& strEncoded, std::string& strKey, std::string& strValue);
    bool ParseRequestQuery(const std::string& strQuery, MAPKeyValue& mapQuery);
    bool ParseRequestHeader(std::istream& is, MAPIKeyValue& mapHeader, MAPKeyValue& mapQuery,
                            MAPIKeyValue& mapCookie);
    bool ParseResponseHeader(std::istream& is, MAPIKeyValue& mapHeader, MAPCookie& mapCookie);
    bool ParseChunked(std::istream& is, std::string& strContent, std::string& strResidue, bool& fContinue);
    std::string BuildRequestHeader(MAPIKeyValue& mapHeader, MAPKeyValue& mapQuery,
                                   MAPIKeyValue& mapCookie, std::size_t nContentLength);
    std::string BuildResponseHeader(int nStatusCode, MAPIKeyValue& mapHeader, MAPCookie& mapCookie,
                                    std::size_t nContentLength);

protected:
    bool ParseCommandLine(const std::string& strLine, MAPIKeyValue& mapHeader, MAPKeyValue& mapQuery);
    bool ParseStatusLine(const std::string& strLine, MAPIKeyValue& mapHeader);
    bool ParseFieldLine(const std::string& strLine, MAPIKeyValue& mapHeader);
    bool ParseUrl(const std::string& strEncoded, std::string& strUrl, MAPKeyValue& mapQuery);
    bool ParseCookie(const std::string& strCookie, MAPIKeyValue& mapCookie);
    bool ParseSetCookie(const std::string& strCookie, MAPCookie& mapCookie);
    std::string BuildUrl(MAPIKeyValue& mapHeader, MAPKeyValue& mapQuery);
    std::string BuildCookie(const MAPIKeyValue& mapCookie);
};

} // namespace xengine
#endif //XENGINE_HTTP_HTTPUTIL_H
