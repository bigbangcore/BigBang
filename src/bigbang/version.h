// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_VERSION_H
#define BIGBANG_VERSION_H

#include <boost/algorithm/string/replace.hpp>
#include <iostream>
#include <sstream>
#include <string>

#define VERSION_NAME "Bigbang"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_REVISION 0

std::string FormatVersion(int nVersion);

static const int VERSION = 10000 * VERSION_MAJOR
                           + 100 * VERSION_MINOR
                           + 1 * VERSION_REVISION;
static const std::string VERSION_STR = FormatVersion(VERSION);

static const int PROTO_VERSION = 100;
static const int MIN_PROTO_VERSION = 100;

inline std::string FormatVersion(const int nMajor, const int nMinor, const int nRevision)
{
    std::ostringstream ss;
    ss << nMajor << "." << nMinor << "." << nRevision;
    return ss.str();
}

inline std::string FormatVersion(int nVersion)
{
    return FormatVersion(nVersion / 10000, (nVersion / 100) % 100, nVersion % 100);
}

inline std::string FormatSubVersion()
{
    std::ostringstream ss;
    ss << "/" << VERSION_NAME << ":" << FormatVersion(VERSION);
    ss << "/Protocol:" << FormatVersion(PROTO_VERSION) << "/";
    return ss.str();
}

inline void ResolveVersion(const int nVersion, int& nMajor, int& nMinor, int& nRevision)
{
    nMajor = nVersion / 10000;
    nMinor = (nVersion / 100) % 100;
    nRevision = nVersion % 100;
}

inline bool ResolveVersion(const std::string& strVersion, int& nMajor, int& nMinor, int& nRevision)
{
    if (strVersion.find(".") != std::string::npos)
    {
        std::string strStream = boost::replace_all_copy(strVersion, ".", " ");
        std::istringstream iss(strStream);
        iss >> nMajor >> nMinor >> nRevision;
        return iss.eof() && !iss.fail();
    }
    else
    {
        std::istringstream iss(strVersion);
        int nVersion;
        iss >> nVersion;
        if (!iss.eof() || iss.fail())
        {
            return false;
        }

        ResolveVersion(nVersion, nMajor, nMinor, nRevision);
        return true;
    }
}

#endif //BIGBANG_VERSION_H
