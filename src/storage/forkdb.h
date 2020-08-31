// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STORAGE_FORKDB_H
#define STORAGE_FORKDB_H

#include <map>

#include "forkcontext.h"
#include "uint256.h"
#include "xengine.h"

namespace bigbang
{
namespace storage
{

class CForkIncreaseCoin
{
    friend class xengine::CStream;

    class CIncreaseCoin
    {
        friend class xengine::CStream;

    public:
        int64 nAmount;
        int64 nMint;
        uint256 hashTx;

    public:
        CIncreaseCoin()
        {
            nAmount = 0;
            nMint = 0;
            hashTx = 0;
        }
        CIncreaseCoin(int64 nAmountIn, int64 nMintIn, const uint256& hashTxIn)
        {
            nAmount = nAmountIn;
            nMint = nMintIn;
            hashTx = hashTxIn;
        }

    protected:
        template <typename O>
        void Serialize(xengine::CStream& s, O& opt)
        {
            s.Serialize(nAmount, opt);
            s.Serialize(nMint, opt);
            s.Serialize(hashTx, opt);
        }
    };

public:
    std::map<int, CIncreaseCoin> mapIncreaseCoin;

public:
    CForkIncreaseCoin()
    {
    }
    bool AddIncreaseCoin(int nHeight, int64 nAmount, int64 nMint, const uint256& hashTx)
    {
        auto it = mapIncreaseCoin.find(nHeight);
        if (it == mapIncreaseCoin.end())
        {
            mapIncreaseCoin.insert(std::make_pair(nHeight, CIncreaseCoin(nAmount, nMint, hashTx)));
        }
        else
        {
            it->second.nAmount = nAmount;
            it->second.nMint = nMint;
            it->second.hashTx = hashTx;
        }
        return true;
    }
    bool GetIncreaseCoin(int nHeight, int64& nAmount, int64& nMint, uint256& hashTx)
    {
        auto it = mapIncreaseCoin.find(nHeight);
        if (it == mapIncreaseCoin.end())
        {
            return false;
        }
        nAmount = it->second.nAmount;
        nMint = it->second.nMint;
        hashTx = it->second.hashTx;
        return true;
    }
    void RemoveIncreaseCoin(int nHeight)
    {
        mapIncreaseCoin.erase(nHeight);
    }
    bool IsNull()
    {
        return mapIncreaseCoin.empty();
    }
    void Clear()
    {
        mapIncreaseCoin.clear();
    }
    bool GetIncreaseCoin(int nHeightIn, int& nTakeEffectHeightOut, int64& nIncreaseCoinOut, int64& nBlockRewardOut)
    {
        bool fFindInc = false;
        for (auto it = mapIncreaseCoin.begin(); it != mapIncreaseCoin.end(); ++it)
        {
            if (nHeightIn < it->first)
            {
                break;
            }
            nTakeEffectHeightOut = it->first;
            nIncreaseCoinOut = it->second.nAmount;
            nBlockRewardOut = it->second.nMint;
            fFindInc = true;
        }
        return fFindInc;
    }

protected:
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt)
    {
        s.Serialize(mapIncreaseCoin, opt);
    }
};

class CForkDB : public xengine::CKVDB
{
public:
    CForkDB() {}
    bool Initialize(const boost::filesystem::path& pathData);
    void Deinitialize();
    bool AddNewForkContext(const CForkContext& ctxt);
    bool RemoveForkContext(const uint256& hashFork);
    bool RetrieveForkContext(const uint256& hashFork, CForkContext& ctxt);
    bool ListForkContext(std::vector<CForkContext>& vForkCtxt);
    bool UpdateFork(const uint256& hashFork, const uint256& hashLastBlock = uint256());
    bool RemoveFork(const uint256& hashFork);
    bool RetrieveFork(const uint256& hashFork, uint256& hashLastBlock);
    bool ListFork(std::vector<std::pair<uint256, uint256>>& vFork);
    bool AddForkIncreaseCoin(const uint256& hashFork, int nHeight, int64 nAmount, int64 nMint, const uint256& hashTx);
    bool RetrieveForkIncreaseCoin(const uint256& hashFork, int nHeight, int64& nAmount, int64& nMint, uint256& hashTx);
    bool RemoveForkIncreaseCoin(const uint256& hashFork, int nHeight);
    bool ListForkIncreaseCoin(const uint256& hashFork, CForkIncreaseCoin& incCoin);
    void Clear();

protected:
    bool LoadCtxtWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                        std::multimap<int, CForkContext>& mapCtxt);
    bool LoadForkWalker(xengine::CBufStream& ssKey, xengine::CBufStream& ssValue,
                        std::multimap<int, uint256>& mapJoint, std::map<uint256, uint256>& mapFork);
};

} // namespace storage
} // namespace bigbang

#endif //STORAGE_FORKDB_H
