// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "delegatedchn.h"

#include "delegatecomm.h"
#include "logger.h"

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
    CDelegatedDataFilter(const std::set<uint256>& setAnchorIn)
      : setAnchor(setAnchorIn) {}
    bool Ignored(const CDelegatedDataIdent& ident) const
    {
        return (setAnchor.count(ident.hashAnchor) == 0);
    }

protected:
    std::set<uint256> setAnchor;
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
                                    const vector<pair<uint256, map<CDestination, size_t>>>& vEnrolledWeight,
                                    const map<CDestination, vector<unsigned char>>& mapDistributeData,
                                    const map<CDestination, vector<unsigned char>>& mapPublishData)
{
    if (nLastBlockHeight < nStartHeight)
    {
        Clear();
    }
    while (nLastBlockHeight > nStartHeight && !listBlockHash.empty())
    {
        const uint256& hash = listBlockHash.front();
        mapChainData.erase(hash);
        listBlockHash.pop_front();
        --nLastBlockHeight;
    }
    nLastBlockHeight = nStartHeight;

    for (size_t i = 0; i < vEnrolledWeight.size(); i++)
    {
        const uint256& hash = vEnrolledWeight[i].first;
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
        const uint256& hash = listBlockHash.front();
        CDelegatedChannelChainData& data = mapChainData[hash];
        data.mapDistributeData.insert(mapDistributeData.begin(), mapDistributeData.end());
        data.mapPublishData.insert(mapPublishData.begin(), mapPublishData.end());
    }

    while (listBlockHash.size() > CONSENSUS_DISTRIBUTE_INTERVAL + 1)
    {
        const uint256& hash = listBlockHash.back();
        mapChainData.erase(hash);
        listBlockHash.pop_back();
    }
}

uint64 CDelegatedChannelChain::GetDistributeBitmap(const uint256& hashAnchor)
{
    map<uint256, CDelegatedChannelChainData>::iterator it = mapChainData.find(hashAnchor);
    if (it != mapChainData.end())
    {
        return ((*it).second.GetBitmap((*it).second.mapDistributeData));
    }
    return 0;
}

uint64 CDelegatedChannelChain::GetPublishBitmap(const uint256& hashAnchor)
{
    if (listBlockHash.size() == CONSENSUS_DISTRIBUTE_INTERVAL + 1 && hashAnchor == listBlockHash.front())
    {
        return (mapChainData[listBlockHash.back()].GetBitmap(mapChainData[hashAnchor].mapPublishData));
    }
    return 0;
}

void CDelegatedChannelChain::GetDistribute(const uint256& hashAnchor, uint64 bmDistribute, set<CDestination>& setDestination)
{
    map<uint256, CDelegatedChannelChainData>::iterator it = mapChainData.find(hashAnchor);
    if (it != mapChainData.end())
    {
        CDelegatedChannelChainData& chain = (*it).second;
        chain.GetKnownData(bmDistribute, setDestination);
    }
}

void CDelegatedChannelChain::GetPublish(const uint256& hashAnchor, uint64 bmPublish, set<CDestination>& setDestination)
{
    if (listBlockHash.size() == CONSENSUS_DISTRIBUTE_INTERVAL + 1 && hashAnchor == listBlockHash.front())
    {
        CDelegatedChannelChainData& chain = mapChainData[listBlockHash.back()];
        chain.GetKnownData(bmPublish, setDestination);
    }
}

void CDelegatedChannelChain::AskForDistribute(const uint256& hashAnchor, uint64 bmDistribute, set<CDestination>& setDestination)
{
    map<uint256, CDelegatedChannelChainData>::iterator it = mapChainData.find(hashAnchor);
    if (it != mapChainData.end())
    {
        CDelegatedChannelChainData& chain = (*it).second;
        uint64 bitmap = chain.GetBitmap(chain.mapDistributeData);
        bitmap = (bitmap & bmDistribute) ^ bmDistribute;
        chain.GetKnownData(bitmap, setDestination);
    }
}

void CDelegatedChannelChain::AskForPublish(const uint256& hashAnchor, uint64 bmDistribute, set<CDestination>& setDestination)
{
    if (listBlockHash.size() == CONSENSUS_DISTRIBUTE_INTERVAL + 1 && hashAnchor == listBlockHash.front())
    {
        CDelegatedChannelChainData& chain = mapChainData[listBlockHash.back()];
        uint64 bitmap = chain.GetBitmap(mapChainData[hashAnchor].mapPublishData);
        bitmap = (bitmap & bmDistribute) ^ bmDistribute;
        chain.GetKnownData(bitmap, setDestination);
    }
}

bool CDelegatedChannelChain::GetDistributeData(const uint256& hashAnchor, const CDestination& dest,
                                               vector<unsigned char>& vchData)
{
    map<uint256, CDelegatedChannelChainData>::iterator it = mapChainData.find(hashAnchor);
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

bool CDelegatedChannelChain::GetPublishData(const uint256& hashAnchor, const CDestination& dest,
                                            vector<unsigned char>& vchData)
{
    map<uint256, CDelegatedChannelChainData>::iterator it = mapChainData.find(hashAnchor);
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

bool CDelegatedChannelChain::IsOutOfDistributeRange(const uint256& hashAnchor) const
{
    return (!mapChainData.count(hashAnchor));
}

bool CDelegatedChannelChain::IsOutOfPublishRange(const uint256& hashAnchor) const
{
    return (listBlockHash.empty() || listBlockHash.front() != hashAnchor);
}

bool CDelegatedChannelChain::InsertDistributeData(const uint256& hashAnchor, const CDestination& dest,
                                                  const vector<unsigned char>& vchData)
{
    map<uint256, CDelegatedChannelChainData>::iterator it = mapChainData.find(hashAnchor);
    if (it != mapChainData.end())
    {
        return ((*it).second.mapDistributeData.insert(make_pair(dest, vchData))).second;
    }
    return false;
}

bool CDelegatedChannelChain::InsertPublishData(const uint256& hashAnchor, const CDestination& dest,
                                               const vector<unsigned char>& vchData)
{
    if (listBlockHash.empty() || listBlockHash.front() != hashAnchor)
    {
        return false;
    }
    map<uint256, CDelegatedChannelChainData>::iterator it = mapChainData.find(hashAnchor);
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
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol\n");
        return false;
    }

    if (!GetObject("blockchain", pBlockChain))
    {
        Error("Failed to request blockchain\n");
        return false;
    }

    if (!GetObject("dispatcher", pDispatcher))
    {
        Error("Failed to request dispatcher\n");
        return false;
    }

    RegisterHandler<CPeerActiveMessage>(boost::bind(&CDelegatedChannel::HandleActive, this, _1));
    RegisterHandler<CPeerDeactiveMessage>(boost::bind(&CDelegatedChannel::HandleDeactive, this, _1));
    RegisterHandler<CPeerBulletinMessageInBound>(boost::bind(&CDelegatedChannel::HandleBulletin, this, _1));
    RegisterHandler<CPeerGetDelegatedMessageInBound>(boost::bind(&CDelegatedChannel::HandleGetDelegate, this, _1));
    RegisterHandler<CPeerDistributeMessageInBound>(boost::bind(&CDelegatedChannel::HandleDistribute, this, _1));
    RegisterHandler<CPeerPublishMessageInBound>(boost::bind(&CDelegatedChannel::HandlePublish, this, _1));

    return true;
}

void CDelegatedChannel::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    pBlockChain = nullptr;
    pDispatcher = nullptr;

    DeregisterHandler(CPeerActiveMessage::MessageType());
    DeregisterHandler(CPeerDeactiveMessage::MessageType());
    DeregisterHandler(CPeerBulletinMessageInBound::MessageType());
    DeregisterHandler(CPeerGetDelegatedMessageInBound::MessageType());
    DeregisterHandler(CPeerDistributeMessageInBound::MessageType());
    DeregisterHandler(CPeerPublishMessageInBound::MessageType());
}

bool CDelegatedChannel::HandleInvoke()
{
    {
        boost::unique_lock<boost::mutex> lock(mtxBulletin);
        nTimerBulletin = 0;
        fBulletin = false;
    }
    return network::IDelegatedChannelActor::HandleInvoke();
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

    network::IDelegatedChannelActor::HandleHalt();
    schedPeer.Clear();
    dataChain.Clear();
}

void CDelegatedChannel::HandleActive(const CPeerActiveMessage& activeMsg)
{
    uint64 nNonce = activeMsg.nNonce;
    if ((activeMsg.address.nService & network::NODE_DELEGATED))
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwPeer);
        schedPeer.ActivatePeer(std::shared_ptr<CDataPeer<CDelegatedDataIdent>>(new CDelegatedChannelPeer(nNonce)));
    }
}

void CDelegatedChannel::HandleDeactive(const CPeerDeactiveMessage& deactiveMsg)
{
    uint64 nNonce = deactiveMsg.nNonce;
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwPeer);
        schedPeer.DeactivatePeer(nNonce);
        DispatchGetDelegated();
    }
}

void CDelegatedChannel::HandleBulletin(const CPeerBulletinMessageInBound& bulletinMsg)
{
    uint64 nNonce = bulletinMsg.nNonce;
    const uint256& hashAnchor = bulletinMsg.hashAnchor;

    {
        boost::unique_lock<boost::shared_mutex> wlock(rwPeer);

        AddPeerKnownDistrubute(nNonce, hashAnchor, bulletinMsg.deletegatedBulletin.bmDistribute);
        AddPeerKnownPublish(nNonce, hashAnchor, bulletinMsg.deletegatedBulletin.bmPublish);

        for (size_t i = 0; i < bulletinMsg.deletegatedBulletin.vBitmap.size(); ++i)
        {
            AddPeerKnownDistrubute(nNonce, bulletinMsg.deletegatedBulletin.vBitmap[i].hashAnchor,
                                   bulletinMsg.deletegatedBulletin.vBitmap[i].bitmap);
        }
        DispatchGetDelegated();

        std::shared_ptr<CDelegatedChannelPeer> spPeer = GetPeer(nNonce);
        if (spPeer)
        {
            spPeer->Renew(hashAnchor, bulletinMsg.deletegatedBulletin);
        }
    }
}

void CDelegatedChannel::HandleGetDelegate(const CPeerGetDelegatedMessageInBound& getDelegatedMsg)
{
    uint64 nNonce = getDelegatedMsg.nNonce;
    const uint256& hashAnchor = getDelegatedMsg.hashAnchor;
    const CDestination& dest = getDelegatedMsg.delegatedGetData.destDelegate;

    if (getDelegatedMsg.delegatedGetData.nInvType == network::CInv::MSG_DISTRIBUTE)
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwPeer);

        auto spDistributeMsg = CPeerDistributeMessageOutBound::Create();
        spDistributeMsg->nNonce = nNonce;
        spDistributeMsg->hashAnchor = hashAnchor;
        spDistributeMsg->delegatedData.destDelegate = dest;
        dataChain.GetDistributeData(hashAnchor, dest, spDistributeMsg->delegatedData.vchData);
        PUBLISH_MESSAGE(spDistributeMsg);
    }
    else if (getDelegatedMsg.delegatedGetData.nInvType == network::CInv::MSG_PUBLISH)
    {
        boost::shared_lock<boost::shared_mutex> rlock(rwPeer);

        auto spPublishMsg = CPeerPublishMessageOutBound::Create();
        spPublishMsg->nNonce = nNonce;
        spPublishMsg->hashAnchor = hashAnchor;
        spPublishMsg->delegatedData.destDelegate = dest;
        dataChain.GetPublishData(hashAnchor, dest, spPublishMsg->delegatedData.vchData);
        PUBLISH_MESSAGE(spPublishMsg);
    }
}

void CDelegatedChannel::HandleDistribute(const CPeerDistributeMessageInBound& distributeMsg)
{
    uint64 nNonce = distributeMsg.nNonce;
    const uint256& hashAnchor = distributeMsg.hashAnchor;
    const CDestination& dest = distributeMsg.delegatedData.destDelegate;
    CDelegatedDataIdent ident(hashAnchor, network::CInv::MSG_DISTRIBUTE, dest);
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwPeer);

        if (nNonce == schedPeer.GetAssignedPeer(ident))
        {
            const vector<unsigned char>& vchData = distributeMsg.delegatedData.vchData;

            schedPeer.RemoveKnownData(ident);

            if (dataChain.IsOutOfDistributeRange(hashAnchor))
            {
                DispatchGetDelegated();
                return;
            }
            else if (vchData.empty())
            {
                return;
            }
            else if (pDispatcher->AddNewDistribute(hashAnchor, dest, vchData))
            {
                bool fAssigned = DispatchGetDelegated();
                if (dataChain.InsertDistributeData(hashAnchor, dest, vchData))
                {
                    BroadcastBulletin(!fAssigned);
                }
                return;
            }
        }

        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK);
    }
}

void CDelegatedChannel::HandlePublish(const CPeerPublishMessageInBound& publishMsg)
{
    uint64 nNonce = publishMsg.nNonce;
    const uint256& hashAnchor = publishMsg.hashAnchor;
    const CDestination& dest = publishMsg.delegatedData.destDelegate;
    CDelegatedDataIdent ident(hashAnchor, network::CInv::MSG_PUBLISH, dest);
    {
        boost::unique_lock<boost::shared_mutex> wlock(rwPeer);

        if (nNonce == schedPeer.GetAssignedPeer(ident))
        {
            const vector<unsigned char>& vchData = publishMsg.delegatedData.vchData;

            schedPeer.RemoveKnownData(ident);

            if (dataChain.IsOutOfPublishRange(hashAnchor))
            {
                DispatchGetDelegated();
                return;
            }
            else if (vchData.empty())
            {
                return;
            }
            else if (pDispatcher->AddNewPublish(hashAnchor, dest, vchData))
            {
                bool fAssigned = DispatchGetDelegated();
                if (dataChain.InsertPublishData(hashAnchor, dest, vchData))
                {
                    BroadcastBulletin(!fAssigned);
                }
                return;
            }
        }

        DispatchMisbehaveEvent(nNonce, CEndpointManager::DDOS_ATTACK);
    }
}

void CDelegatedChannel::PrimaryUpdate(int nStartHeight,
                                      const vector<pair<uint256, map<CDestination, size_t>>>& vEnrolledWeight,
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
        list<uint256>& listHash = dataChain.GetHashList();
        schedPeer.Schedule(vAssigned, CDelegatedDataFilter(set<uint256>(listHash.begin(), listHash.end())));
    }

    for (size_t i = 0; i < vAssigned.size(); i++)
    {
        uint64 nNonce = vAssigned[i].first;
        const CDelegatedDataIdent& ident = vAssigned[i].second;

        auto spGetDelegatedMsg = CPeerGetDelegatedMessageOutBound::Create();
        spGetDelegatedMsg->nNonce = nNonce;
        spGetDelegatedMsg->hashAnchor = ident.hashAnchor;
        spGetDelegatedMsg->delegatedGetData.nInvType = ident.nInvType;
        spGetDelegatedMsg->delegatedGetData.destDelegate = ident.destDelegated;
        PUBLISH_MESSAGE(spGetDelegatedMsg);
    }

    return (!vAssigned.empty());
}

void CDelegatedChannel::AddPeerKnownDistrubute(uint64 nNonce, const uint256& hashAnchor, uint64 bmDistrubute)
{
    set<CDestination> setDestination;
    dataChain.AskForDistribute(hashAnchor, bmDistrubute, setDestination);
    for (const CDestination& dest : setDestination)
    {
        schedPeer.AddKnownData(nNonce, CDelegatedDataIdent(hashAnchor, network::CInv::MSG_DISTRIBUTE, dest));
    }
}

void CDelegatedChannel::AddPeerKnownPublish(uint64 nNonce, const uint256& hashAnchor, uint64 bmPublish)
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
    // TODO
    /*auto spNetCloseMsg = CPeerNetCloseMessage::Create();
    spNetCloseMsg->nNonce = nNonce;
    spNetCloseMsg->closeReason = reason;
    PUBLISH_MESSAGE(spNetCloseMsg);*/
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

    list<uint256>& listHash = dataChain.GetHashList();

    if (!listHash.empty() && !vPeer.empty())
    {
        const uint256& hashAnchor = listHash.front();

        auto spBulletinMsg = CPeerBulletinMessageOutBound::Create();
        spBulletinMsg->nNonce = 0ULL;
        spBulletinMsg->hashAnchor = hashAnchor;
        spBulletinMsg->deletegatedBulletin.bmDistribute = dataChain.GetDistributeBitmap(hashAnchor);
        spBulletinMsg->deletegatedBulletin.bmPublish = dataChain.GetPublishBitmap(hashAnchor);

        for (list<uint256>::iterator it = ++listHash.begin(); it != listHash.end(); ++it)
        {
            const uint256& hash = (*it);
            uint64 bitmap = dataChain.GetDistributeBitmap(hash);
            if (bitmap != 0)
            {
                spBulletinMsg->deletegatedBulletin.AddBitmap(hash, bitmap);
            }
        }
        for (const uint64& nNonce : vPeer)
        {
            std::shared_ptr<CDelegatedChannelPeer> spPeer = GetPeer(nNonce);
            if (spPeer != nullptr && spPeer->HaveUnknown(hashAnchor, spBulletinMsg->deletegatedBulletin))
            {
                spPeer->Update(hashAnchor, spBulletinMsg->deletegatedBulletin);
                spBulletinMsg->nNonce = nNonce;
                PUBLISH_MESSAGE(spBulletinMsg);
            }
        }
    }
}

} // namespace bigbang
