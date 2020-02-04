// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "delegatedchn.h"

#include "delegatecomm.h"

using namespace std;
using namespace xengine;
using boost::asio::ip::tcp;

#define BULLETIN_TIMEOUT (500)

namespace bigbang
{

//////////////////////////////
// CDelegatedDataFilter

class CDelegatedDataFilter : public xengine::CDataFilter<CDelegatedDataIdent>
{
public:
    CDelegatedDataFilter(const std::set<int>& setAnchorIn)
      : setAnchor(setAnchorIn) {}
    bool Ignored(const CDelegatedDataIdent& ident) const
    {
        return (setAnchor.count(ident.hashAnchor) == 0);
    }

protected:
    std::set<int> setAnchor;
};

//////////////////////////////
// CDelegatedChannelChain

void CDelegatedChannelChain::Clear()
{
    listBlockHash.clear();
    mapChainData.clear();
    nLastBlockHeight = -1;
}

void CDelegatedChannelChain::Update(int nStartHeight,
                                    const vector<pair<int, map<CDestination, size_t>>>& vEnrolledWeight,
                                    const map<CDestination, vector<unsigned char>>& mapDistributeData,
                                    const map<CDestination, vector<unsigned char>>& mapPublishData)
{
    if (nLastBlockHeight < nStartHeight)
    {
        Clear();
    }
    while (nLastBlockHeight > nStartHeight && !listBlockHash.empty())
    {
        const int& hash = listBlockHash.front();
        mapChainData.erase(hash);
        listBlockHash.pop_front();
        --nLastBlockHeight;
    }
    nLastBlockHeight = nStartHeight;

    for (size_t i = 0; i < vEnrolledWeight.size(); i++)
    {
        const int& hash = vEnrolledWeight[i].first;
        CDelegatedChannelChainData& data = mapChainData[hash];

        for (map<CDestination, size_t>::const_iterator it = vEnrolledWeight[i].second.begin();
             it != vEnrolledWeight[i].second.end(); ++it)
        {
            data.vEnrolled.push_back((*it).first);
        }
        listBlockHash.push_front(hash);
        ++nLastBlockHeight;
    }

    if (!listBlockHash.empty())
    {
        const int& hash = listBlockHash.front();
        CDelegatedChannelChainData& data = mapChainData[hash];
        data.mapDistributeData.insert(mapDistributeData.begin(), mapDistributeData.end());
        data.mapPublishData.insert(mapPublishData.begin(), mapPublishData.end());
    }

    while (listBlockHash.size() > CONSENSUS_DISTRIBUTE_INTERVAL + 1)
    {
        const int& hash = listBlockHash.back();
        mapChainData.erase(hash);
        listBlockHash.pop_back();
    }
}

uint64 CDelegatedChannelChain::GetDistributeBitmap(const int& hashAnchor)
{
    map<int, CDelegatedChannelChainData>::iterator it = mapChainData.find(hashAnchor);
    if (it != mapChainData.end())
    {
        return ((*it).second.GetBitmap((*it).second.mapDistributeData));
    }
    return 0;
}

uint64 CDelegatedChannelChain::GetPublishBitmap(const int& hashAnchor)
{
    if (listBlockHash.size() == CONSENSUS_DISTRIBUTE_INTERVAL + 1 && hashAnchor == listBlockHash.front())
    {
        return (mapChainData[listBlockHash.back()].GetBitmap(mapChainData[hashAnchor].mapPublishData));
    }
    return 0;
}

void CDelegatedChannelChain::GetDistribute(const int& hashAnchor, uint64 bmDistribute, set<CDestination>& setDestination)
{
    map<int, CDelegatedChannelChainData>::iterator it = mapChainData.find(hashAnchor);
    if (it != mapChainData.end())
    {
        CDelegatedChannelChainData& chain = (*it).second;
        chain.GetKnownData(bmDistribute, setDestination);
    }
}

void CDelegatedChannelChain::GetPublish(const int& hashAnchor, uint64 bmPublish, set<CDestination>& setDestination)
{
    if (listBlockHash.size() == CONSENSUS_DISTRIBUTE_INTERVAL + 1 && hashAnchor == listBlockHash.front())
    {
        CDelegatedChannelChainData& chain = mapChainData[listBlockHash.back()];
        chain.GetKnownData(bmPublish, setDestination);
    }
}

void CDelegatedChannelChain::AskForDistribute(const int& hashAnchor, uint64 bmDistribute, set<CDestination>& setDestination)
{
    map<int, CDelegatedChannelChainData>::iterator it = mapChainData.find(hashAnchor);
    if (it != mapChainData.end())
    {
        CDelegatedChannelChainData& chain = (*it).second;
        uint64 bitmap = chain.GetBitmap(chain.mapDistributeData);
        bitmap = (bitmap & bmDistribute) ^ bmDistribute;
        chain.GetKnownData(bitmap, setDestination);
    }
}

void CDelegatedChannelChain::AskForPublish(const int& hashAnchor, uint64 bmDistribute, set<CDestination>& setDestination)
{
    if (listBlockHash.size() == CONSENSUS_DISTRIBUTE_INTERVAL + 1 && hashAnchor == listBlockHash.front())
    {
        CDelegatedChannelChainData& chain = mapChainData[listBlockHash.back()];
        uint64 bitmap = chain.GetBitmap(mapChainData[hashAnchor].mapPublishData);
        bitmap = (bitmap & bmDistribute) ^ bmDistribute;
        chain.GetKnownData(bitmap, setDestination);
    }
}

bool CDelegatedChannelChain::GetDistributeData(const int& hashAnchor, const CDestination& dest,
                                               vector<unsigned char>& vchData)
{
    map<int, CDelegatedChannelChainData>::iterator it = mapChainData.find(hashAnchor);
    if (it != mapChainData.end())
    {
        map<CDestination, vector<unsigned char>>::iterator mi = (*it).second.mapDistributeData.find(dest);
        if (mi != (*it).second.mapDistributeData.end())
        {
            vchData = (*mi).second;
            return true;
        }
    }
    return false;
}

bool CDelegatedChannelChain::GetPublishData(const int& hashAnchor, const CDestination& dest,
                                            vector<unsigned char>& vchData)
{
    map<int, CDelegatedChannelChainData>::iterator it = mapChainData.find(hashAnchor);
    if (it != mapChainData.end())
    {
        map<CDestination, vector<unsigned char>>::iterator mi = (*it).second.mapPublishData.find(dest);
        if (mi != (*it).second.mapPublishData.end())
        {
            vchData = (*mi).second;
            return true;
        }
    }
    return false;
}

bool CDelegatedChannelChain::IsOutOfDistributeRange(const int& hashAnchor) const
{
    return (!mapChainData.count(hashAnchor));
}

bool CDelegatedChannelChain::IsOutOfPublishRange(const int& hashAnchor) const
{
    return (listBlockHash.empty() || listBlockHash.front() != hashAnchor);
}

bool CDelegatedChannelChain::InsertDistributeData(const int& hashAnchor, const CDestination& dest,
                                                  const vector<unsigned char>& vchData)
{
    map<int, CDelegatedChannelChainData>::iterator it = mapChainData.find(hashAnchor);
    if (it != mapChainData.end())
    {
        return ((*it).second.mapDistributeData.insert(make_pair(dest, vchData))).second;
    }
    return false;
}

bool CDelegatedChannelChain::InsertPublishData(const int& hashAnchor, const CDestination& dest,
                                               const vector<unsigned char>& vchData)
{
    if (listBlockHash.empty() || listBlockHash.front() != hashAnchor)
    {
        return false;
    }
    map<int, CDelegatedChannelChainData>::iterator it = mapChainData.find(hashAnchor);
    if (it != mapChainData.end())
    {
        return ((*it).second.mapPublishData.insert(make_pair(dest, vchData))).second;
    }
    return false;
}

//////////////////////////////
// CDelegatedChannel

CDelegatedChannel::CDelegatedChannel()
{
    pPeerNet = nullptr;
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pDispatcher = nullptr;
    fBulletin = false;
}

CDelegatedChannel::~CDelegatedChannel()
{
}

bool CDelegatedChannel::HandleInitialize()
{
    if (!GetObject("peernet", pPeerNet))
    {
        Error("Failed to request peer net");
        return false;
    }

    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol");
        return false;
    }

    if (!GetObject("blockchain", pBlockChain))
    {
        Error("Failed to request blockchain");
        return false;
    }

    if (!GetObject("dispatcher", pDispatcher))
    {
        Error("Failed to request dispatcher");
        return false;
    }
    return true;
}

void CDelegatedChannel::HandleDeinitialize()
{
    pPeerNet = nullptr;
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pDispatcher = nullptr;
}

bool CDelegatedChannel::HandleInvoke()
{
    {
        boost::unique_lock<boost::mutex> lock(mtxBulletin);
        nTimerBulletin = 0;
        fBulletin = false;
    }
    return network::IDelegatedChannel::HandleInvoke();
}

void CDelegatedChannel::HandleHalt()
{
    {
        boost::unique_lock<boost::mutex> lock(mtxBulletin);
        if (nTimerBulletin != 0)
        {
            CancelTimer(nTimerBulletin);
            nTimerBulletin = 0;
        }
        fBulletin = false;
    }

    network::IDelegatedChannel::HandleHalt();
    schedPeer.Clear();
    dataChain.Clear();
}

bool CDelegatedChannel::HandleEvent(network::CEventPeerActive& eventActive)
{
    uint64 nNonce = eventActive.nNonce;
    if ((eventActive.data.nService & network::NODE_DELEGATED))
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwPeer);
        schedPeer.ActivatePeer(std::shared_ptr<CDataPeer<CDelegatedDataIdent>>(new CDelegatedChannelPeer(nNonce)));
    }
    return true;
}

bool CDelegatedChannel::HandleEvent(network::CEventPeerDeactive& eventDeactive)
{
    uint64 nNonce = eventDeactive.nNonce;
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwPeer);
        schedPeer.DeactivatePeer(nNonce);
        DispatchGetDelegated();
    }
    return true;
}

bool CDelegatedChannel::HandleEvent(network::CEventPeerBulletin& eventBulletin)
{
    uint64 nNonce = eventBulletin.nNonce;
    const int& hashAnchor = eventBulletin.hashAnchor;

    {
        boost::unique_lock<boost::shared_mutex> wlock(rwPeer);

        AddPeerKnownDistrubute(nNonce, hashAnchor, eventBulletin.data.bmDistribute);
        AddPeerKnownPublish(nNonce, hashAnchor, eventBulletin.data.bmPublish);

        for (size_t i = 0; i < eventBulletin.data.vBitmap.size(); ++i)
        {
            AddPeerKnownDistrubute(nNonce, eventBulletin.data.vBitmap[i].hashAnchor,
                                   eventBulletin.data.vBitmap[i].bitmap);
        }
        DispatchGetDelegated();

        std::shared_ptr<CDelegatedChannelPeer> spPeer = GetPeer(nNonce);
        if (spPeer != nullptr)
        {
            spPeer->Renew(hashAnchor, eventBulletin.data);
        }
    }

    return true;
}

bool CDelegatedChannel::HandleEvent(network::CEventPeerGetDelegated& eventGetDelegated)
{
    uint64 nNonce = eventGetDelegated.nNonce;
    const int& hashAnchor = eventGetDelegated.hashAnchor;
    const CDestination& dest = eventGetDelegated.data.destDelegate;

    if (eventGetDelegated.data.nInvType == network::CInv::MSG_DISTRIBUTE)
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwPeer);

        network::CEventPeerDistribute eventDistribute(nNonce, hashAnchor);
        eventDistribute.data.destDelegate = dest;
        dataChain.GetDistributeData(hashAnchor, dest, eventDistribute.data.vchData);
        pPeerNet->DispatchEvent(&eventDistribute);
    }
    else if (eventGetDelegated.data.nInvType == network::CInv::MSG_PUBLISH)
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwPeer);

        network::CEventPeerPublish eventPublish(nNonce, hashAnchor);
        eventPublish.data.destDelegate = dest;
        dataChain.GetPublishData(hashAnchor, dest, eventPublish.data.vchData);
        pPeerNet->DispatchEvent(&eventPublish);
    }

    return true;
}

bool CDelegatedChannel::HandleEvent(network::CEventPeerDistribute& eventDistribute)
{
    uint64 nNonce = eventDistribute.nNonce;
    const int& hashAnchor = eventDistribute.hashAnchor;
    const CDestination& dest = eventDistribute.data.destDelegate;
    CDelegatedDataIdent ident(hashAnchor, network::CInv::MSG_DISTRIBUTE, dest);
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwPeer);

        if (nNonce == schedPeer.GetAssignedPeer(ident))
        {
            const vector<unsigned char>& vchData = eventDistribute.data.vchData;

            schedPeer.RemoveKnownData(ident);

            if (dataChain.IsOutOfDistributeRange(hashAnchor))
            {
                DispatchGetDelegated();
                return true;
            }
            else if (vchData.empty())
            {
                return true;
            }
            else if (pDispatcher->AddNewDistribute(hashAnchor, dest, vchData))
            {
                bool fAssigned = DispatchGetDelegated();
                if (dataChain.InsertDistributeData(hashAnchor, dest, vchData))
                {
                    BroadcastBulletin(!fAssigned);
                }
                return true;
            }
        }

        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK);
    }
    return true;
}

bool CDelegatedChannel::HandleEvent(network::CEventPeerPublish& eventPublish)
{
    uint64 nNonce = eventPublish.nNonce;
    const int& hashAnchor = eventPublish.hashAnchor;
    const CDestination& dest = eventPublish.data.destDelegate;
    CDelegatedDataIdent ident(hashAnchor, network::CInv::MSG_PUBLISH, dest);
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwPeer);

        if (nNonce == schedPeer.GetAssignedPeer(ident))
        {
            const vector<unsigned char>& vchData = eventPublish.data.vchData;

            schedPeer.RemoveKnownData(ident);

            if (dataChain.IsOutOfPublishRange(hashAnchor))
            {
                DispatchGetDelegated();
                return true;
            }
            else if (vchData.empty())
            {
                return true;
            }
            else if (pDispatcher->AddNewPublish(hashAnchor, dest, vchData))
            {
                bool fAssigned = DispatchGetDelegated();
                if (dataChain.InsertPublishData(hashAnchor, dest, vchData))
                {
                    BroadcastBulletin(!fAssigned);
                }
                return true;
            }
        }

        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK);
    }
    return true;
}

void CDelegatedChannel::PrimaryUpdate(int nStartHeight,
                                      const vector<pair<int, map<CDestination, size_t>>>& vEnrolledWeight,
                                      const map<CDestination, vector<unsigned char>>& mapDistributeData,
                                      const map<CDestination, vector<unsigned char>>& mapPublishData)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwPeer);

    dataChain.Update(nStartHeight, vEnrolledWeight, mapDistributeData, mapPublishData);

    if (!mapDistributeData.empty() || !mapPublishData.empty())
    {
        BroadcastBulletin(true);
    }
    DispatchGetDelegated();
}

void CDelegatedChannel::BroadcastBulletin(bool fForced)
{
    boost::unique_lock<boost::mutex> lock(mtxBulletin);
    if (fForced && nTimerBulletin != 0)
    {
        CancelTimer(nTimerBulletin);
        nTimerBulletin = 0;
        fBulletin = false;
    }
    if (nTimerBulletin == 0)
    {
        PushBulletin();
        nTimerBulletin = SetTimer(BULLETIN_TIMEOUT, boost::bind(&CDelegatedChannel::PushBulletinTimerFunc, this, _1));
    }
    else
    {
        fBulletin = true;
    }
}

bool CDelegatedChannel::DispatchGetDelegated()
{
    vector<pair<uint64, CDelegatedDataIdent>> vAssigned;
    {
        list<int>& listHash = dataChain.GetHashList();
        schedPeer.Schedule(vAssigned, CDelegatedDataFilter(set<int>(listHash.begin(), listHash.end())));
    }

    for (size_t i = 0; i < vAssigned.size(); i++)
    {
        uint64 nNonce = vAssigned[i].first;
        const CDelegatedDataIdent& ident = vAssigned[i].second;
        network::CEventPeerGetDelegated eventGetDelegated(nNonce, ident.hashAnchor);
        eventGetDelegated.data.nInvType = ident.nInvType;
        eventGetDelegated.data.destDelegate = ident.destDelegated;
        pPeerNet->DispatchEvent(&eventGetDelegated);
    }

    return (!vAssigned.empty());
}

void CDelegatedChannel::AddPeerKnownDistrubute(uint64 nNonce, const int& hashAnchor, uint64 bmDistrubute)
{
    set<CDestination> setDestination;
    dataChain.AskForDistribute(hashAnchor, bmDistrubute, setDestination);
    for (const CDestination& dest : setDestination)
    {
        schedPeer.AddKnownData(nNonce, CDelegatedDataIdent(hashAnchor, network::CInv::MSG_DISTRIBUTE, dest));
    }
}

void CDelegatedChannel::AddPeerKnownPublish(uint64 nNonce, const int& hashAnchor, uint64 bmPublish)
{
    set<CDestination> setDestination;
    dataChain.AskForPublish(hashAnchor, bmPublish, setDestination);
    for (const CDestination& dest : setDestination)
    {
        schedPeer.AddKnownData(nNonce, CDelegatedDataIdent(hashAnchor, network::CInv::MSG_PUBLISH, dest));
    }
}

void CDelegatedChannel::DispatchMisbehaveEvent(uint64 nNonce, CEndpointManager::CloseReason reason)
{
    /*
    CEventPeerNetClose eventClose(nNonce);
    eventClose.data = reason;
    pPeerNet->DispatchEvent(&eventClose);
*/
}

void CDelegatedChannel::PushBulletinTimerFunc(uint32 nTimerId)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwPeer, boost::defer_lock);
    boost::unique_lock<boost::mutex> lock(mtxBulletin, boost::defer_lock);
    boost::lock(wlock, lock);
    if (nTimerBulletin == nTimerId)
    {
        if (fBulletin)
        {
            PushBulletin();
            fBulletin = false;
            nTimerBulletin = SetTimer(BULLETIN_TIMEOUT, boost::bind(&CDelegatedChannel::PushBulletinTimerFunc, this, _1));
        }
        else
        {
            nTimerBulletin = 0;
        }
    }
}

void CDelegatedChannel::PushBulletin()
{
    vector<uint64> vPeer;
    schedPeer.GetPeerNonce(vPeer);

    list<int>& listHash = dataChain.GetHashList();

    if (!listHash.empty() && !vPeer.empty())
    {
        const int& hashAnchor = listHash.front();

        network::CEventPeerBulletin eventBulletin(0ULL, hashAnchor);
        eventBulletin.data.bmDistribute = dataChain.GetDistributeBitmap(hashAnchor);
        eventBulletin.data.bmPublish = dataChain.GetPublishBitmap(hashAnchor);
        for (list<int>::iterator it = ++listHash.begin(); it != listHash.end(); ++it)
        {
            const int& hash = (*it);
            uint64 bitmap = dataChain.GetDistributeBitmap(hash);
            if (bitmap != 0)
            {
                eventBulletin.data.AddBitmap(hash, bitmap);
            }
        }
        for (const uint64& nNonce : vPeer)
        {
            std::shared_ptr<CDelegatedChannelPeer> spPeer = GetPeer(nNonce);
            if (spPeer != nullptr && spPeer->HaveUnknown(hashAnchor, eventBulletin.data))
            {
                spPeer->Update(hashAnchor, eventBulletin.data);
                eventBulletin.nNonce = nNonce;
                pPeerNet->DispatchEvent(&eventBulletin);
            }
        }
    }
}

} // namespace bigbang
