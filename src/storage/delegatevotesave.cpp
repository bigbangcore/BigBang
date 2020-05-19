// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "delegatevotesave.h"

using namespace std;
using namespace boost::filesystem;
using namespace xengine;

namespace bigbang
{
namespace storage
{

//////////////////////////////
// CDelegateVoteSave

CDelegateVoteSave::CDelegateVoteSave()
{
}

CDelegateVoteSave::~CDelegateVoteSave()
{
}

bool CDelegateVoteSave::Initialize(const path& pathData)
{
    path pathTxPool = pathData / "delegatevote";

    if (!exists(pathTxPool))
    {
        create_directories(pathTxPool);
    }

    if (!is_directory(pathTxPool))
    {
        return false;
    }

    pathDelegateVoteFile = pathTxPool / "votedata.dat";

    if (exists(pathDelegateVoteFile) && !is_regular_file(pathDelegateVoteFile))
    {
        return false;
    }

    return true;
}

bool CDelegateVoteSave::Remove()
{
    if (is_regular_file(pathDelegateVoteFile))
    {
        return remove(pathDelegateVoteFile);
    }
    return true;
}

bool CDelegateVoteSave::Save(const delegate::CDelegate& delegate)
{
    FILE* fp = fopen(pathDelegateVoteFile.c_str(), "w");
    if (fp == nullptr)
    {
        return false;
    }
    fclose(fp);

    if (!is_regular_file(pathDelegateVoteFile))
    {
        return false;
    }

    try
    {
        CFileStream fs(pathDelegateVoteFile.c_str());
        fs << delegate;
    }
    catch (std::exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }

    return true;
}

bool CDelegateVoteSave::Load(delegate::CDelegate& delegate)
{
    delegate.Clear();

    if (!is_regular_file(pathDelegateVoteFile))
    {
        return true;
    }

    try
    {
        CFileStream fs(pathDelegateVoteFile.c_str());
        fs >> delegate;
    }
    catch (std::exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }

    remove(pathDelegateVoteFile);

    return true;
}

} // namespace storage
} // namespace bigbang
