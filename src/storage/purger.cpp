// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "purger.h"

#include "blockdb.h"
#include "txpooldata.h"
#include "walletdb.h"

using namespace std;
using namespace boost::filesystem;
using namespace xengine;

namespace bigbang
{
namespace storage
{

//////////////////////////////
// CPurger

bool CPurger::ResetDB(const boost::filesystem::path& pathDataLocation) const
{
    {
        CBlockDB dbBlock;
        if (dbBlock.Initialize(pathDataLocation))
        {
            if (!dbBlock.RemoveAll())
            {
                return false;
            }
            dbBlock.Deinitialize();
        }
    }

    {
        CWalletDB dbWallet;
        if (dbWallet.Initialize(pathDataLocation / "wallet"))
        {
            if (!dbWallet.ClearTx())
            {
                return false;
            }
            dbWallet.Deinitialize();
        }
    }

    {
        CTxPoolData datTxPool;
        if (datTxPool.Initialize(pathDataLocation))
        {
            if (!datTxPool.Remove())
            {
                return false;
            }
        }
    }

    return true;
}

bool CPurger::RemoveBlockFile(const path& pathDataLocation) const
{
    try
    {
        path pathBlock = pathDataLocation / "block";
        if (exists(pathBlock))
        {
            remove_all(pathBlock);
        }
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

} // namespace storage
} // namespace bigbang
