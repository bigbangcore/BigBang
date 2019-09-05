// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_LOGGER_H
#define XENGINE_LOGGER_H

#include <boost/noncopyable.hpp>

namespace xengine
{
class CLogger : public boost::noncopyable
{
public:
    // getInstance thread-safe in C++11
    static CLogger*& getInstance();
    void Init(const boost::filesystem::path& pathData, bool debug, bool daemon);

private:
};
} // namespace xengine

#endif