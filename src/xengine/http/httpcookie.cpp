// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "httpcookie.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <sstream>

#include "httputil.h"

using namespace std;
using namespace boost::posix_time;

namespace xengine
{

///////////////////////////////
// CHttpCookie

const ptime CHttpCookie::INVALID_PTIME = ptime(boost::date_time::not_a_date_time);

CHttpCookie::CHttpCookie()
  : ptExpires(INVALID_PTIME)
{
    SetNull();
}

CHttpCookie::CHttpCookie(const string& strFromResp)
  : ptExpires(INVALID_PTIME)
{
    if (!LoadFromResp(strFromResp))
    {
        SetNull();
    }
}

CHttpCookie::CHttpCookie(const string& strNameIn, const string& strValueIn,
                         const string& strDomainIn, const string& strPathIn,
                         const ptime& ptExpiresIn, bool fSecureIn, bool fHttpOnlyIn)
  : strName(strNameIn), strValue(strValueIn), strDomain(strDomainIn), strPath(strPathIn),
    ptExpires(ptExpiresIn), fSecure(fSecureIn), fHttpOnly(fHttpOnlyIn)
{
}

CHttpCookie::~CHttpCookie()
{
}

bool CHttpCookie::IsNull()
{
    return strName.empty();
}

bool CHttpCookie::IsPersistent()
{
    return ptExpires.is_special();
}

bool CHttpCookie::LoadFromResp(const string& strFromResp)
{
    CHttpUtil util;
    SetNull();
    // parse name and value
    size_t sp = strFromResp.find(';');
    {
        if (!util.ParseEncodedPair(strFromResp.substr(0, sp), strName, strValue))
        {
            return false;
        }
    }
    // parse attr
    while (sp != string::npos)
    {
        size_t start = sp + 1;
        while (strFromResp[start] == ' ')
        {
            start++;
        }

        sp = strFromResp.find(';', start);
        size_t len = (sp != string::npos ? sp - start : sp);
        string strAttr = strFromResp.substr(start, len);
        size_t eq = strAttr.find('=');
        if (eq != string::npos)
        {
            string strAttrKey = strAttr.substr(0, eq);
            string strAttrValue = strAttr.substr(eq + 1);
            boost::to_lower(strAttrKey);
            if (strAttrKey == "domain")
            {
                strDomain = strAttrValue;
            }
            else if (strAttrKey == "path")
            {
                if (!util.PercentDecode(strAttrValue, strPath))
                {
                    return false;
                }
            }
            else if (strAttrKey == "expires")
            {
                if (IsPersistent())
                {
                    ptExpires = util.ParseRFC1123DayTime(strAttrValue);
                }
            }
            else if (strAttrKey == "max-age")
            {
                char* endptr = NULL;
                long off = strtol(strAttrValue.c_str(), &endptr, 10);
                if (endptr != NULL)
                {
                    ptExpires = second_clock::universal_time() + seconds(off);
                }
            }
        }
        else
        {
            boost::to_lower(strAttr);
            if (strAttr == "secure")
            {
                fSecure = true;
            }
            else if (strAttr == "httponly")
            {
                fHttpOnly = true;
            }
        }
    }
    return true;
}

void CHttpCookie::Delete()
{
    ptExpires = from_time_t(time_t(1));
}

const string CHttpCookie::BuildSetCookie()
{
    CHttpUtil util;
    ostringstream oss;

    string strEncodedName;
    util.PercentEncode(strName, strEncodedName);
    oss << strEncodedName << "=";

    if (!strValue.empty())
    {
        string strEncodedValue;
        util.PercentEncode(strValue, strEncodedValue);
        oss << strEncodedValue;
    }

    if (!strDomain.empty())
    {
        oss << "; Domain=" << strDomain;
    }

    if (!strPath.empty())
    {
        string strEncodedPath;
        util.PercentEncode(strPath, strEncodedPath, "/");
        oss << "; Path=" << strEncodedPath;
    }

    if (!IsPersistent())
    {
        oss << "; Expires=" << util.FormatRFC1123DayTime(ptExpires);
    }

    if (fSecure)
    {
        oss << "; Secure";
    }

    if (fHttpOnly)
    {
        oss << "; HttpOnly";
    }

    return oss.str();
}

void CHttpCookie::SetNull()
{
    strName.clear();
    strValue.clear();
    strDomain.clear();
    strPath.clear();
    ptExpires = INVALID_PTIME;
    fSecure = false;
    fHttpOnly = false;
}

} // namespace xengine
