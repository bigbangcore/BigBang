// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_DELEGATEVOTESAVE_H
#define STORAGE_DELEGATEVOTESAVE_H

#include <boost/filesystem.hpp>

#include "delegate.h"
#include "transaction.h"
#include "xengine.h"

namespace bigbang
{
namespace storage
{

class CDelegateVoteSave
{
public:
    CDelegateVoteSave();
    ~CDelegateVoteSave();
    bool Initialize(const boost::filesystem::path& pathData);
    bool Remove();
    bool Save(const delegate::CDelegate& delegate);
    bool Load(delegate::CDelegate& delegate);

protected:
    boost::filesystem::path pathDelegateVoteFile;
};

} // namespace storage
} // namespace bigbang

#endif //STORAGE_DELEGATEVOTESAVE_H
