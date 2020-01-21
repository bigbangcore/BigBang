// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_PURGER_H
#define STORAGE_PURGER_H

#include <boost/filesystem.hpp>

namespace bigbang
{
namespace storage
{

class CPurger
{
public:
    bool ResetDB(const boost::filesystem::path& pathDataLocation) const;
    bool RemoveBlockFile(const boost::filesystem::path& pathDataLocation) const;
    bool operator()(const boost::filesystem::path& pathDataLocation) const
    {
        return (ResetDB(pathDataLocation) && RemoveBlockFile(pathDataLocation));
    }
};

} // namespace storage
} // namespace bigbang

#endif //STORAGE_PURGER_H
