// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "recovery.h"

#include <boost/filesystem.hpp>

#include "block.h"
#include "purger.h"
#include "timeseries.h"

using namespace boost::filesystem;

namespace bigbang
{

class CRecoveryWalker : public storage::CTSWalker<CBlockEx>
{
public:
    CRecoveryWalker(IDispatcher* pDispatcherIn, const size_t nSizeIn)
      : pDispatcher(pDispatcherIn), nSize(nSizeIn), nNextSize(nSizeIn / 100), nWalkedFileSize(0) {}
    bool Walk(const CBlockEx& t, uint32 nFile, uint32 nOffset) override
    {
        if (!t.IsGenesis())
        {
            Errno err = pDispatcher->AddNewBlock(t);
            if (err == OK)
            {
                xengine::StdTrace("Recovery", "Recovery block [%s]", t.GetHash().ToString().c_str());
            }
            else if (err != ERR_ALREADY_HAVE)
            {
                printf("...... block: %s, file: %u, offset: %u\n", t.GetHash().ToString().c_str(), nFile, nOffset);
                xengine::StdError("Recovery", "Recovery block [%s] error: %s", t.GetHash().ToString().c_str(), ErrorString(err));
                return false;
            }
        }

        if (nWalkedFileSize + nOffset > nNextSize)
        {
            xengine::StdLog("CRecovery", "....................... Recovered %d%% ..................", nNextSize / (nSize / 100));
            nNextSize += (nSize / 100);
        }

        return true;
    }

protected:
    IDispatcher* pDispatcher;
    const size_t nSize;
    size_t nNextSize;
    size_t nWalkedFileSize;
};

CRecovery::CRecovery()
  : pDispatcher(nullptr)
{
}

CRecovery::~CRecovery()
{
}

bool CRecovery::HandleInitialize()
{
    if (!GetObject("dispatcher", pDispatcher))
    {
        Error("Failed to request dispatcher");
        return false;
    }

    if (!StorageConfig()->strRecoveryDir.empty())
    {
        Warn("Clear old database except wallet address");

        storage::CPurger purger;
        if (!purger(Config()->pathData))
        {
            Error("Failed to reset DB");
            return false;
        }
        Warn("Clear completed");
    }

    return true;
}

void CRecovery::HandleDeinitialize()
{
    pDispatcher = nullptr;
}

bool CRecovery::HandleInvoke()
{
    if (!StorageConfig()->strRecoveryDir.empty())
    {
        Log("Recovery [%s] begin", StorageConfig()->strRecoveryDir.c_str());

        path blockDir(StorageConfig()->strRecoveryDir);
        if (!exists(blockDir))
        {
            Error("Recovery dir [%s] not exist", StorageConfig()->strRecoveryDir.c_str());
            return false;
        }

        storage::CTimeSeriesCached tsBlock;
        if (!tsBlock.Initialize(blockDir, "block"))
        {
            Error("Recovery initialze fail");
            return false;
        }

        size_t nSize = tsBlock.GetSize();
        CRecoveryWalker walker(pDispatcher, nSize);
        uint32 nLastFile;
        uint32 nLastPos;
        if (!tsBlock.WalkThrough(walker, nLastFile, nLastPos, false))
        {
            Error("Recovery walkthrough fail");
            return false;
        }
        xengine::StdLog("CRecovery", "....................... Recovered success .......................");

        Log("Recovery [%s] end", StorageConfig()->strRecoveryDir.c_str());
    }
    return true;
}

} // namespace bigbang
