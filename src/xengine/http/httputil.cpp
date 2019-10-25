// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "httputil.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <locale>
#include <sstream>

#include "logger.h"
#include "version.h"

using namespace std;
using namespace boost::posix_time;

namespace xengine
{

///////////////////////////////
// CHttpUtil

const ptime CHttpUtil::ParseRFC1123DayTime(const string& strDayTime)
{
    try
    {
        time_input_facet* facet = new time_input_facet("%a, %d %b %Y %H:%M:%S");
        stringstream ss;
        ss.imbue(locale(locale("C"), facet));
        ss.str(strDayTime);
        ptime pt(boost::date_time::not_a_date_time);
        string strTimeZone;
        ss >> pt >> strTimeZone;
        if (strTimeZone == "GMT")
        {
            return pt;
        }
    }
    catch (exception& e)
    {
        LOG_ERROR("CHttpUtil", "Parse RFC1123 time (%s) error: %s", strDayTime.c_str(), e.what());
    }
    return ptime(boost::date_time::not_a_date_time);
}

const string CHttpUtil::FormatRFC1123DayTime(const ptime& pt)
{
    try
    {
        if (!pt.is_special())
        {
            time_facet* facet = new time_facet("%a, %d %b %Y %H:%M:%S GMT");
            stringstream ss;
            ss.imbue(locale(locale("C"), facet));
            ss << pt;
            return ss.str();
        }
    }
    catch (exception& e)
    {
        LOG_ERROR("CHttpUtil", "Format RFC1123 time (%s) error: %s", to_simple_string(pt).c_str(), e.what());
    }
    return "";
}

void CHttpUtil::Base64Encode(const std::string& strDecoded, std::string& strEncoded)
{
    const string base64_padding[] = { "", "==", "=" };
    using namespace boost::archive::iterators;
    typedef base64_from_binary<transform_width<string::const_iterator, 6, 8>> base64enc;
    strEncoded = string(base64enc(strDecoded.begin()), base64enc(strDecoded.end()));
    strEncoded += base64_padding[strDecoded.size() % 3];
}

bool CHttpUtil::Base64Decode(const std::string& strEncoded, std::string& strDecoded)
{
    const string zero_padding[] = { "", "A", "AA" };
    using namespace boost::archive::iterators;
    typedef transform_width<binary_from_base64<string::const_iterator>, 8, 6> base64dec;
    size_t nPadding = count(strEncoded.begin(), strEncoded.end(), '=');
    if (nPadding != 0 && (nPadding > 2 || strEncoded.find('=') != strEncoded.size() - nPadding))
    {
        return false;
    }

    string s = strEncoded.substr(0, strEncoded.size() - nPadding) + zero_padding[nPadding];
    try
    {
        strDecoded = string(base64dec(s.begin()), base64dec(s.end()));
        strDecoded.resize(strDecoded.size() - nPadding);
    }
    catch (exception& e)
    {
        LOG_ERROR("CHttpUtil", "Decode base64 string (%s) error: %s", strEncoded.c_str(), e.what());
        return false;
    }
    return true;
}

void CHttpUtil::PercentEncode(const string& strDecoded, string& strEncoded, const string& strResvExt)
{
    const char hexc[17] = "0123456789ABCDEF";
    const string strResv = "-_.~";
    strEncoded.clear();
    strEncoded.reserve(strDecoded.size() * 3);
    for (size_t i = 0; i < strDecoded.size(); i++)
    {
        if (isalnum((int)strDecoded[i]) || strResv.find(strDecoded[i]) != string::npos
            || strResvExt.find(strDecoded[i]) != string::npos)
        {
            strEncoded += strDecoded[i];
        }
        else if (strDecoded[i] == ' ')
        {
            strEncoded += '+';
        }
        else
        {
            strEncoded += '%';
            strEncoded += hexc[((unsigned char)strDecoded[i]) >> 4];
            strEncoded += hexc[((unsigned char)strDecoded[i]) & 0x0F];
        }
    }
}

bool CHttpUtil::PercentDecode(const string& strEncoded, string& strDecoded)
{
#define FROMHEX(c) (int)((c) >= '0' && (c) <= '9' ? (c)                                        \
                                                  : ((c) >= 'a' && (c) <= 'f' ? (c) - 'a' + 10 \
                                                                              : ((c) >= 'A' && (c) <= 'F' ? (c) - 'A' + 10 : -1)))
    strDecoded.clear();
    strDecoded.reserve(strEncoded.size());
    for (size_t i = 0; i < strEncoded.size(); i++)
    {
        if (strEncoded[i] == '%')
        {
            if (i + 2 < strEncoded.size())
            {
                int h = FROMHEX(strEncoded[i + 1]);
                int l = FROMHEX(strEncoded[i + 2]);
                if (h >= 0 && l >= 0)
                {
                    strDecoded += (char)((h << 4) | l);
                    i += 2;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        else if (strEncoded[i] == '+')
        {
            strDecoded += ' ';
        }
        else
        {
            strDecoded += strEncoded[i];
        }
    }
    return true;
}

bool CHttpUtil::ParseEncodedPair(const string& strEncoded, string& strKey, string& strValue)
{
    strKey.clear();
    strValue.clear();
    if (!strEncoded.empty())
    {
        size_t eq = strEncoded.find('=');
        if (!PercentDecode(strEncoded.substr(0, eq), strKey))
        {
            return false;
        }
        if (eq != string::npos
            && !PercentDecode(strEncoded.substr(eq + 1), strValue))
        {
            return false;
        }
        return true;
    }
    return false;
}

bool CHttpUtil::ParseRequestQuery(const string& strQuery, MAPKeyValue& mapQuery)
{
    const char delimiter = '&';
    size_t sp = strQuery.find(delimiter);
    {
        string strKey, strValue;
        if (!ParseEncodedPair(strQuery.substr(0, sp), strKey, strValue))
        {
            return false;
        }
        mapQuery[strKey] = strValue;
    }
    while (sp != string::npos)
    {
        size_t start = sp + 1;
        string strKey, strValue;
        sp = strQuery.find(delimiter, start);
        size_t len = (sp != string::npos ? sp - start : sp);
        if (!ParseEncodedPair(strQuery.substr(start, len), strKey, strValue))
        {
            return false;
        }
        mapQuery[strKey] = strValue;
    }
    return true;
}

bool CHttpUtil::ParseRequestHeader(istream& is, MAPIKeyValue& mapHeader, MAPKeyValue& mapQuery,
                                   MAPIKeyValue& mapCookie)
{
    mapHeader.clear();
    mapQuery.clear();
    mapCookie.clear();

    string line;
    getline(is, line);
    if (!ParseCommandLine(line, mapHeader, mapQuery))
    {
        return false;
    }

    for (;;)
    {
        getline(is, line);
        if (line.empty() || line == "\r")
        {
            break;
        }
        if (!ParseFieldLine(line, mapHeader))
        {
            return false;
        }
    }

    if (mapHeader.count("cookie"))
    {
        return ParseCookie(mapHeader["cookie"], mapCookie);
    }

    return true;
}

bool CHttpUtil::ParseResponseHeader(istream& is, MAPIKeyValue& mapHeader, MAPCookie& mapCookie)
{
    mapHeader.clear();
    mapCookie.clear();
    string line;
    getline(is, line);
    if (!ParseStatusLine(line, mapHeader))
    {
        return false;
    }

    for (;;)
    {
        getline(is, line);
        if (line.empty() || line == "\r")
        {
            break;
        }
        else if (!ParseFieldLine(line, mapHeader))
        {
            return false;
        }
    }

    if (mapHeader.count("set-cookie"))
    {
        return ParseSetCookie(mapHeader["set-cookie"], mapCookie);
    }
    return true;
}

bool CHttpUtil::ParseChunked(std::istream& is, string& strContent, string& strResidue, bool& fContinue)
{
    fContinue = true;
    string line;
    char* endptr;
    strResidue.clear();

    while (!is.eof())
    {
        if (getline(is, line).eof())
        {
            strResidue = line;
            break;
        }
        strResidue = line + "\n";
        size_t len = strtoul(line.c_str(), &endptr, 16);
        if (endptr == nullptr || strcmp("\r", endptr) != 0 || !isxdigit(line[0]))
        {
            return false;
        }

        string data;
        data.reserve(len);
        while (!getline(is, line).eof())
        {
            size_t left = len - data.size();
            if (line.size() < left)
            {
                data += line + "\n";
            }
            else
            {
                data += line.substr(0, left);
                if (line.substr(left) != "\r")
                {
                    return false;
                }
                break;
            }
        }

        if (is.eof())
        {
            strResidue += data + line;
            break;
        }
        if (len == 0)
        {
            fContinue = false;
            break;
        }
        strContent += data;

        strResidue.clear();
    }
    return true;
}

string CHttpUtil::BuildRequestHeader(MAPIKeyValue& mapHeader, MAPKeyValue& mapQuery,
                                     MAPIKeyValue& mapCookie, size_t nContentLength)
{
    ostringstream oss;
    oss << mapHeader["method"] << " " << BuildUrl(mapHeader, mapQuery) << " HTTP/1.1\r\n"
        << "HOST: " << mapHeader["host"] << "\r\n";

    if (mapHeader.count("user-agent"))
    {
        oss << "User-Agent: " << mapHeader["user-agent"] << "\r\n";
    }
    else
    {
        oss << "User-Agent: "
               "XEngine/" VERSION_STRING
            << "\r\n";
    }

    if (mapHeader.count("authorization"))
    {
        oss << "Authorization: " << mapHeader["authorization"] << "\r\n";
    }

    if (!mapCookie.empty())
    {
        oss << "Cookie: " << BuildCookie(mapCookie) << "\r\n";
    }
    if (nContentLength != 0)
    {
        if (mapHeader.count("content-type"))
        {
            oss << "Content-Type: " << mapHeader["content-type"] << "\r\n";
        }
        oss << "Content-Length: " << nContentLength << "\r\n";
    }
    oss << "Accept: " << mapHeader["accept"] << "\r\n"
        << "Connection: " << mapHeader["connection"] << "\r\n\r\n";

    return oss.str();
}

string CHttpUtil::BuildResponseHeader(int nStatusCode, MAPIKeyValue& mapHeader, MAPCookie& mapCookie,
                                      size_t nContentLength)
{
    string strStatus;
    if (nStatusCode == 200)
        strStatus = "OK";
    if (nStatusCode == 201)
        strStatus = "Created";
    if (nStatusCode == 202)
        strStatus = "Accepted";
    if (nStatusCode == 203)
        strStatus = "Non-Authoritative Information";
    if (nStatusCode == 204)
        strStatus = "No Content";
    if (nStatusCode == 400)
        strStatus = "Bad Request";
    if (nStatusCode == 401)
        strStatus = "Authorization Required";
    if (nStatusCode == 403)
        strStatus = "Forbidden";
    if (nStatusCode == 404)
        strStatus = "Not Found";
    if (nStatusCode == 405)
        strStatus = "Method Not Allowed";
    if (nStatusCode == 406)
        strStatus = "Not Acceptable";
    if (nStatusCode == 410)
        strStatus = "Gone";
    if (nStatusCode == 500)
        strStatus = "Internal Server Error";
    if (nStatusCode == 503)
        strStatus = "Service Unavailable";

    ostringstream oss;
    oss << "HTTP/1.1 " << nStatusCode << " " << strStatus << "\r\n"
        << "Date: " << FormatRFC1123DayTime(second_clock::universal_time()) << "\r\n";
    if (mapHeader.count("server"))
    {
        oss << "Server: " << mapHeader["server"] << "\r\n";
    }
    else
    {
        oss << "Server: "
               "XEngine/" VERSION_STRING
            << "\r\n";
    }

    for (MAPCookie::iterator it = mapCookie.begin(); it != mapCookie.end(); ++it)
    {
        oss << "Set-Cookie: " << (*it).second.BuildSetCookie() << "\r\n";
    }

    if (mapHeader.count("connection"))
    {
        oss << "Connection: " << mapHeader["connection"] << "\r\n";
    }
    else
    {
        oss << "Connection: Close\r\n";
    }

    if (mapHeader.count("content-type") && !mapHeader["content-type"].empty())
    {
        oss << "Content-Type: " << mapHeader["content-type"] << "\r\n";
        if (mapHeader["content-type"] == "text/event-stream")
        {
            oss << "Cache-Control: no-cache\r\n";
        }
    }

    oss << "Content-Length: " << nContentLength << "\r\n";

    if (nStatusCode == 401)
    {
        oss << "WWW-Authenticate: Basic realm=\"xengine\"\r\n";
    }
    oss << "\r\n";
    return oss.str();
}

bool CHttpUtil::ParseCommandLine(const string& strLine, MAPIKeyValue& mapHeader, MAPKeyValue& mapQuery)
{
    size_t start, sp;

    sp = strLine.find(' ');
    mapHeader["method"] = strLine.substr(0, sp);
    if ((mapHeader["method"] != "POST" && mapHeader["method"] != "GET")
        || sp == string::npos)
    {
        return false;
    }

    start = sp + 1;
    sp = strLine.find(' ', start);
    if (sp == string::npos || strLine[start] != '/')
    {
        return false;
    }

    if (!ParseUrl(strLine.substr(start, sp - start), mapHeader["url"], mapQuery))
    {
        return false;
    }

    if (strLine.substr(sp + 1, 5) != "HTTP/")
    {
        return false;
    }
    start = sp + 6;
    sp = strLine.find('\r', start);
    if (sp == string::npos)
    {
        return false;
    }
    mapHeader["version"] = strLine.substr(start, sp - start);

    return true;
}

bool CHttpUtil::ParseStatusLine(const string& strLine, MAPIKeyValue& mapHeader)
{
    size_t start, sp;
    if (strLine.substr(0, 5) != "HTTP/")
    {
        return false;
    }
    start = 5;
    sp = strLine.find(' ', start);
    if (sp == string::npos)
    {
        return false;
    }
    mapHeader["version"] = strLine.substr(start, sp - start);

    start = sp + 1;
    sp = strLine.find(' ');
    if (sp == string::npos)
    {
        return false;
    }
    mapHeader["status"] = strLine.substr(start, sp - start);
    mapHeader["statusmsg"] = strLine.substr(sp + 1);

    return true;
}

bool CHttpUtil::ParseFieldLine(const string& strLine, MAPIKeyValue& mapHeader)
{
    size_t sp = strLine.find(':');
    if (sp == string::npos)
    {
        return false;
    }

    string strKey = strLine.substr(0, sp);
    boost::trim(strKey);
    boost::to_lower(strKey);
    string strValue = strLine.substr(sp + 1);
    boost::trim(strValue);
    if (strKey != "set-cookie" || !mapHeader.count("set-cookie"))
    {
        mapHeader.insert(make_pair(strKey, strValue));
    }
    else
    {
        mapHeader[strKey] += string(";;") + strValue;
    }
    return true;
}

bool CHttpUtil::ParseUrl(const string& strEncoded, string& strUrl, MAPKeyValue& mapQuery)
{
    size_t sp = strEncoded.find('?');
    if (!PercentDecode(strEncoded.substr(0, sp), strUrl))
    {
        return false;
    }

    if (sp != string::npos && sp + 1 != strEncoded.size())
    {
        return ParseRequestQuery(strEncoded.substr(sp + 1), mapQuery);
    }
    return true;
}

bool CHttpUtil::ParseCookie(const string& strCookie, MAPIKeyValue& mapCookie)
{
    const char delimiter = ';';
    size_t sp = strCookie.find(delimiter);
    {
        string strKey, strValue;
        if (!ParseEncodedPair(strCookie.substr(0, sp), strKey, strValue))
        {
            return false;
        }
        mapCookie[strKey] = strValue;
    }
    while (sp != string::npos)
    {
        size_t start = sp + 1;
        while (strCookie[start] == ' ')
        {
            start++;
        }

        string strKey, strValue;
        sp = strCookie.find(delimiter, start);
        size_t len = (sp != string::npos ? sp - start : sp);
        if (!ParseEncodedPair(strCookie.substr(start, len), strKey, strValue))
        {
            return false;
        }
        mapCookie[strKey] = strValue;
    }
    return true;
}

bool CHttpUtil::ParseSetCookie(const string& strCookie, MAPCookie& mapCookie)
{
    size_t sp = strCookie.find(";;");
    {
        CHttpCookie cookie(strCookie.substr(0, sp));
        if (cookie.IsNull() || mapCookie.count(cookie.strName))
        {
            return false;
        }
        mapCookie.insert(make_pair(cookie.strName, cookie));
    }
    while (sp != string::npos)
    {
        size_t start = sp + 2;
        sp = strCookie.find(";;", start);
        size_t len = (sp != string::npos ? sp - start : sp);
        CHttpCookie cookie(strCookie.substr(start, len));
        if (cookie.IsNull() || mapCookie.count(cookie.strName))
        {
            return false;
        }
        mapCookie.insert(make_pair(cookie.strName, cookie));
    }
    return true;
}

string CHttpUtil::BuildUrl(MAPIKeyValue& mapHeader, MAPKeyValue& mapQuery)
{
    string strUrl;
    PercentEncode(mapHeader["url"], strUrl, "/");
    if (strUrl.empty())
    {
        strUrl = "/";
    }
    if (mapHeader["method"] == "GET" && !mapQuery.empty())
    {
        string strKey, strValue;
        MAPKeyValue::iterator it = mapQuery.begin();
        PercentEncode((*it).first, strKey);
        PercentEncode((*it).second, strValue);
        strUrl += string("?") + strKey + "=" + strValue;
        while (++it != mapQuery.end())
        {
            PercentEncode((*it).first, strKey);
            PercentEncode((*it).second, strValue);
            strUrl += string("&") + strKey + "=" + strValue;
        }
    }
    return strUrl;
}

string CHttpUtil::BuildCookie(const MAPIKeyValue& mapCookie)
{
    ostringstream oss;
    if (!mapCookie.empty())
    {
        string strKey, strValue;
        MAPIKeyValue::const_iterator it = mapCookie.begin();
        PercentEncode((*it).first, strKey);
        PercentEncode((*it).second, strValue);
        oss << strKey << "=" << strValue;
        while (++it != mapCookie.end())
        {
            PercentEncode((*it).first, strKey);
            PercentEncode((*it).second, strValue);
            oss << "; " << strKey << "=" << strValue;
        }
    }
    return oss.str();
}

} // namespace xengine
