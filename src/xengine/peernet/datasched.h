// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_PEERNET_DATASCHED_H
#define XENGINE_PEERNET_DATASCHED_H

#include <list>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "type.h"

namespace xengine
{

template <typename CDataIdent>
class CDataScheduler;

template <typename CDataIdent>
class CDataPeer
{
    friend class CDataScheduler<CDataIdent>;

public:
    CDataPeer(const uint64 nNonceIn = 0)
      : nNonce(nNonceIn), fAssigned(false)
    {
    }

public:
    uint64 nNonce;

private:
    std::list<CDataIdent> listDataIdent;
    bool fAssigned;
};

template <typename CDataIdent>
class CDataFilter
{
public:
    virtual bool Ignored(const CDataIdent& ident) const
    {
        return false;
    }
};

class CDataKnown
{
public:
    bool AddPeer(uint64 nNonce)
    {
        return (setKnownPeer.insert(nNonce)).second;
    }
    void RemovePeer(uint64 nNonce)
    {
        setKnownPeer.erase(nNonce);
        if (nAssigned == nNonce)
        {
            nAssigned = 0;
        }
    }
    bool IsAssigned() const
    {
        return (nAssigned != 0);
    }
    bool Assign(uint64 nNonce)
    {
        if (nAssigned != 0 && nAssigned != nNonce)
        {
            return false;
        }
        nAssigned = nNonce;
        return true;
    }
    uint64 Unassign()
    {
        uint64 nNonce = nAssigned;
        nAssigned = 0;
        return nNonce;
    }
    uint64 GetAssignedPeer() const
    {
        return nAssigned;
    }

public:
    std::set<uint64> setKnownPeer;
    uint64 nAssigned;
};

template <typename CDataIdent>
class CDataScheduler
{
    typedef std::shared_ptr<CDataPeer<CDataIdent>> CDataPeerPtr;

public:
    CDataPeerPtr GetPeer(uint64 nNonce)
    {
        typename std::map<uint64, CDataPeerPtr>::iterator it = mapPeer.find(nNonce);
        return (it != mapPeer.end() ? (*it).second : nullptr);
    }
    void ActivatePeer(CDataPeerPtr spPeer)
    {
        mapPeer.insert(std::make_pair(spPeer->nNonce, spPeer));
    }
    void DeactivatePeer(uint64 nNonce)
    {
        typename std::map<uint64, CDataPeerPtr>::iterator it = mapPeer.find(nNonce);
        if (it != mapPeer.end())
        {
            CDataPeerPtr& spPeer = (*it).second;
            for (typename std::list<CDataIdent>::iterator li = spPeer->listDataIdent.begin();
                 li != spPeer->listDataIdent.end(); ++li)
            {
                mapData[(*li)].RemovePeer(nNonce);
            }
            mapPeer.erase(it);
        }
    }
    void AddKnownData(uint64 nNonce, const CDataIdent& ident)
    {
        typename std::map<uint64, CDataPeerPtr>::iterator it = mapPeer.find(nNonce);
        if (it != mapPeer.end())
        {
            CDataPeerPtr& spPeer = (*it).second;
            if (mapData[ident].AddPeer(nNonce))
            {
                spPeer->listDataIdent.push_back(ident);
            }
        }
    }
    void RemoveKnownData(const CDataIdent& ident)
    {
        typename std::map<CDataIdent, CDataKnown>::iterator it = mapData.find(ident);
        if (it != mapData.end())
        {
            CDataKnown& known = (*it).second;
            for (std::set<uint64>::iterator si = known.setKnownPeer.begin(); si != known.setKnownPeer.end(); ++si)
            {
                mapPeer[(*si)]->listDataIdent.remove(ident);
            }
            uint64 nAssigned = known.GetAssignedPeer();
            if (nAssigned != 0)
            {
                mapPeer[nAssigned]->fAssigned = false;
            }
            mapData.erase(it);
        }
    }
    void Schedule(typename std::vector<std::pair<uint64, CDataIdent>>& vAssigned,
                  const CDataFilter<CDataIdent>& filter = CDataFilter<CDataIdent>())
    {
        for (typename std::map<uint64, CDataPeerPtr>::iterator it = mapPeer.begin();
             it != mapPeer.end(); ++it)
        {
            uint64 nNonce = (*it).first;
            CDataPeerPtr& spPeer = (*it).second;
            if (!spPeer->fAssigned)
            {
                for (typename std::list<CDataIdent>::iterator li = spPeer->listDataIdent.begin();
                     li != spPeer->listDataIdent.end(); ++li)
                {
                    CDataIdent& ident = (*li);
                    if (!filter.Ignored(ident) && mapData[ident].Assign(nNonce))
                    {
                        spPeer->fAssigned = true;
                        vAssigned.push_back(std::make_pair(nNonce, ident));
                        break;
                    }
                }
            }
        }
    }
    uint64 GetAssignedPeer(const CDataIdent& ident)
    {
        typename std::map<CDataIdent, CDataKnown>::iterator it = mapData.find(ident);
        if (it != mapData.end())
        {
            return (*it).second.GetAssignedPeer();
        }
        return 0;
    }
    void UnassignPeer(const CDataIdent& ident)
    {
        uint64 nAssigned = mapData[ident].Unassign();
        if (nAssigned != 0)
        {
            mapPeer[nAssigned]->fAssigned = false;
        }
    }
    void Clear()
    {
        mapPeer.clear();
        mapData.clear();
    }
    std::size_t GetPeerCount()
    {
        return mapPeer.size();
    }
    void GetPeerNonce(std::vector<uint64>& vPeerNonce)
    {
        for (typename std::map<uint64, CDataPeerPtr>::iterator it = mapPeer.begin();
             it != mapPeer.end(); ++it)
        {
            vPeerNonce.push_back((*it).first);
        }
    }

public:
    std::map<uint64, CDataPeerPtr> mapPeer;
    std::map<CDataIdent, CDataKnown> mapData;
};

} // namespace xengine

#endif //XENGINE_PEERNET_DATASCHED_H
