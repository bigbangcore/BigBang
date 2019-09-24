// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "datastat.h"

#include "event.h"

using namespace std;
using namespace xengine;

namespace bigbang
{

//////////////////////////////
// CStatBlockMakerFork

CStatBlockMakerFork::CStatBlockMakerFork()
  : nStatPOWBlockCount(0), nStatDPOSBlockCount(0), nStatTxCount(0)
{
    vStatTable.resize(STAT_MAX_ITEM_COUNT);
    for (uint32 i = 0; i < STAT_MAX_ITEM_COUNT; i++)
    {
        vStatTable[i].nTimeValue = i;
    }
}

CStatBlockMakerFork::~CStatBlockMakerFork()
{
    vStatTable.clear();
}

void CStatBlockMakerFork::AddStatData(bool fPOW, uint64 nTxCountIn)
{
    if (fPOW)
    {
        nStatPOWBlockCount++;
    }
    else
    {
        nStatDPOSBlockCount++;
    }
    nStatTxCount += nTxCountIn;
}

void CStatBlockMakerFork::TimerStatData(uint32 nTimeValue)
{
    if (nTimeValue < STAT_MAX_ITEM_COUNT)
    {
        CStatItemBlockMaker& data = vStatTable[nTimeValue];

        data.nPOWBlockCount = nStatPOWBlockCount;
        data.nDPOSBlockCount = nStatDPOSBlockCount;
        data.nBlockTPS = (nStatPOWBlockCount + nStatDPOSBlockCount) * 100 / 60;
        data.nTxTPS = nStatTxCount * 100 / 60;
    }
    nStatPOWBlockCount = 0;
    nStatDPOSBlockCount = 0;
    nStatTxCount = 0;
}

bool CStatBlockMakerFork::GetStatData(uint32 nBeginTime, uint32 nGetCount, std::vector<CStatItemBlockMaker>& vOut)
{
    if (nBeginTime >= STAT_MAX_ITEM_COUNT || nGetCount == 0 || nGetCount > STAT_MAX_ITEM_COUNT)
    {
        return false;
    }
    uint32 nGetPos = nBeginTime;
    for (uint32 i = 0; i < nGetCount; i++)
    {
        vOut.push_back(vStatTable[nGetPos]);
        nGetPos = (nGetPos + 1) % STAT_MAX_ITEM_COUNT;
    }
    return true;
}

bool CStatBlockMakerFork::CumulativeStatData(uint32 nBeginTime, uint32 nGetCount, std::vector<CStatItemBlockMaker>& vOut)
{
    if (nBeginTime >= STAT_MAX_ITEM_COUNT || nGetCount == 0 || nGetCount > STAT_MAX_ITEM_COUNT || nGetCount != vOut.size())
    {
        return false;
    }
    uint32 nGetPos = nBeginTime;
    for (uint32 i = 0; i < nGetCount; i++)
    {
        CStatItemBlockMaker& out = vOut[i];
        CStatItemBlockMaker& get = vStatTable[nGetPos];
        nGetPos = (nGetPos + 1) % STAT_MAX_ITEM_COUNT;

        out.nPOWBlockCount += get.nPOWBlockCount;
        out.nDPOSBlockCount += get.nDPOSBlockCount;
        out.nBlockTPS += get.nBlockTPS;
        out.nTxTPS += get.nTxTPS;
    }
    return true;
}

//////////////////////////////
// CStatP2pSynFork

CStatP2pSynFork::CStatP2pSynFork()
  : nStatRecvBlockCount(0), nStatRecvTxCount(0), nStatSendBlockCount(0),
    nStatSendTxCount(0), nStatSynRecvTxCount(0), nStatSynSendTxCount(0)
{
    vStatTable.resize(STAT_MAX_ITEM_COUNT);
    for (uint32 i = 0; i < STAT_MAX_ITEM_COUNT; i++)
    {
        vStatTable[i].nTimeValue = i;
    }
}

CStatP2pSynFork::~CStatP2pSynFork()
{
    vStatTable.clear();
}

void CStatP2pSynFork::AddRecvStatData(uint64 nBlockCountIn, uint64 nTxCountIn)
{
    nStatRecvBlockCount += nBlockCountIn;
    nStatRecvTxCount += nTxCountIn;
}

void CStatP2pSynFork::AddSendStatData(uint64 nBlockCountIn, uint64 nTxCountIn)
{
    nStatSendBlockCount += nBlockCountIn;
    nStatSendTxCount += nTxCountIn;
}

void CStatP2pSynFork::AddTxSynStatData(bool fRecv)
{
    if (fRecv)
    {
        nStatSynRecvTxCount++;
    }
    else
    {
        nStatSynSendTxCount++;
    }
}

void CStatP2pSynFork::TimerStatData(uint32 nTimeValue)
{
    if (nTimeValue < STAT_MAX_ITEM_COUNT)
    {
        CStatItemP2pSyn& data = vStatTable[nTimeValue];

        data.nRecvBlockCount = nStatRecvBlockCount;
        data.nRecvTxTPS = nStatRecvTxCount * 100 / 60;
        data.nSendBlockCount = nStatSendBlockCount;
        data.nSendTxTPS = nStatSendTxCount * 100 / 60;
        data.nSynRecvTxTPS = nStatSynRecvTxCount * 100 / 60;
        data.nSynSendTxTPS = nStatSynSendTxCount * 100 / 60;
    }
    nStatRecvBlockCount = 0;
    nStatRecvTxCount = 0;
    nStatSendBlockCount = 0;
    nStatSendTxCount = 0;
    nStatSynRecvTxCount = 0;
    nStatSynSendTxCount = 0;
}

bool CStatP2pSynFork::GetStatData(uint32 nBeginTime, uint32 nGetCount, std::vector<CStatItemP2pSyn>& vOut)
{
    if (nBeginTime >= STAT_MAX_ITEM_COUNT || nGetCount == 0 || nGetCount > STAT_MAX_ITEM_COUNT)
    {
        return false;
    }
    uint32 nGetPos = nBeginTime;
    for (uint32 i = 0; i < nGetCount; i++)
    {
        vOut.push_back(vStatTable[nGetPos]);
        nGetPos = (nGetPos + 1) % STAT_MAX_ITEM_COUNT;
    }
    return true;
}

bool CStatP2pSynFork::CumulativeStatData(uint32 nBeginTime, uint32 nGetCount, std::vector<CStatItemP2pSyn>& vOut)
{
    if (nBeginTime >= STAT_MAX_ITEM_COUNT || nGetCount == 0 || nGetCount > STAT_MAX_ITEM_COUNT || nGetCount != vOut.size())
    {
        return false;
    }
    uint32 nGetPos = nBeginTime;
    for (uint32 i = 0; i < nGetCount; i++)
    {
        CStatItemP2pSyn& out = vOut[i];
        CStatItemP2pSyn& get = vStatTable[nGetPos];
        nGetPos = (nGetPos + 1) % STAT_MAX_ITEM_COUNT;

        out.nRecvBlockCount += get.nRecvBlockCount;
        out.nRecvTxTPS += get.nRecvTxTPS;
        out.nSendBlockCount += get.nSendBlockCount;
        out.nSendTxTPS += get.nSendTxTPS;
        out.nSynRecvTxTPS += get.nSynRecvTxTPS;
        out.nSynSendTxTPS += get.nSynSendTxTPS;
    }
    return true;
}

//////////////////////////////
// CDataStat

CDataStat::CDataStat()
  : fRunFlag(false), thrStatTimer("stattimer", boost::bind(&CDataStat::StatTimerProc, this)), fStatWork(false)
{
    pCoreProtocol = nullptr;
}

CDataStat::~CDataStat()
{
}

const CRPCServerConfig* CDataStat::RPCServerConfig()
{
    return dynamic_cast<const CRPCServerConfig*>(IBase::Config());
}

bool CDataStat::HandleInitialize()
{
    if (!GetObject("coreprotocol", pCoreProtocol))
    {
        Error("Failed to request coreprotocol\n");
        return false;
    }
    if (!Initialize())
    {
        Error("Data stat initialize fail.\n");
        return false;
    }

    fStatWork = RPCServerConfig() ? RPCServerConfig()->fStatDataEnable : false;

    return true;
}

void CDataStat::HandleDeinitialize()
{
    pCoreProtocol = nullptr;
    Clear();
}

bool CDataStat::HandleInvoke()
{
    if (RPCServerConfig())
    {
        if (RPCServerConfig()->fStatDataEnable)
        {
            fRunFlag = true;
            if (!ThreadDelayStart(thrStatTimer))
            {
                return false;
            }
        }
    }

    return IIOModule::HandleInvoke();
}

void CDataStat::HandleHalt()
{
    IIOModule::HandleHalt();

    fRunFlag = false;
    condStat.notify_all();
    if (thrStatTimer.IsRunning())
    {
        thrStatTimer.Interrupt();
    }
    thrStatTimer.Exit();
}

bool CDataStat::Initialize()
{
    if (!mapStatBlockMaker.insert(make_pair(pCoreProtocol->GetGenesisBlockHash(), CStatBlockMakerFork())).second)
    {
        return false;
    }
    if (!mapStatP2pSyn.insert(make_pair(pCoreProtocol->GetGenesisBlockHash(), CStatP2pSynFork())).second)
    {
        return false;
    }
    return true;
}

void CDataStat::Clear()
{
    boost::unique_lock<boost::mutex> lock(mutex);
    mapStatBlockMaker.clear();
    mapStatP2pSyn.clear();
}

void CDataStat::StatTimerProc()
{
    boost::system_time timeout = boost::get_system_time();
    int64 nPrevStatTime = GetTime();
    while (fRunFlag)
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        timeout += boost::posix_time::seconds(1);
        condStat.timed_wait(lock, timeout);
        if (!fRunFlag)
        {
            break;
        }

        int64 nCurTime = GetTime();
        if (nCurTime % 60 == 0 || nCurTime - nPrevStatTime >= 60)
        {
            nPrevStatTime = nCurTime;

            if (fStatWork)
            {
                uint32 nTimeValue = ((nCurTime - 60) % 86400) / 60;

                BlockMakerTimerStat(nTimeValue);
                P2pSynTimerStat(nTimeValue);
            }
        }
    }
}

void CDataStat::BlockMakerTimerStat(uint32 nTimeValue)
{
    map<uint256, CStatBlockMakerFork>::iterator it = mapStatBlockMaker.begin();
    for (; it != mapStatBlockMaker.end(); ++it)
    {
        (*it).second.TimerStatData(nTimeValue);
    }
}

void CDataStat::P2pSynTimerStat(uint32 nTimeValue)
{
    map<uint256, CStatP2pSynFork>::iterator it = mapStatP2pSyn.begin();
    for (; it != mapStatP2pSyn.end(); ++it)
    {
        (*it).second.TimerStatData(nTimeValue);
    }
}

bool CDataStat::AddBlockMakerStatData(const uint256& hashFork, bool fPOW, uint64 nTxCountIn)
{
    if (fStatWork)
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        map<uint256, CStatBlockMakerFork>::iterator it = mapStatBlockMaker.find(hashFork);
        if (it == mapStatBlockMaker.end())
        {
            it = mapStatBlockMaker.insert(make_pair(hashFork, CStatBlockMakerFork())).first;
            if (it == mapStatBlockMaker.end())
            {
                return false;
            }
        }
        (*it).second.AddStatData(fPOW, nTxCountIn);
    }
    return true;
}

bool CDataStat::AddP2pSynRecvStatData(const uint256& hashFork, uint64 nBlockCountIn, uint64 nTxCountIn)
{
    if (fStatWork)
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        map<uint256, CStatP2pSynFork>::iterator it = mapStatP2pSyn.find(hashFork);
        if (it == mapStatP2pSyn.end())
        {
            it = mapStatP2pSyn.insert(make_pair(hashFork, CStatP2pSynFork())).first;
            if (it == mapStatP2pSyn.end())
            {
                return false;
            }
        }
        (*it).second.AddRecvStatData(nBlockCountIn, nTxCountIn);
    }
    return true;
}

bool CDataStat::AddP2pSynSendStatData(const uint256& hashFork, uint64 nBlockCountIn, uint64 nTxCountIn)
{
    if (fStatWork)
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        map<uint256, CStatP2pSynFork>::iterator it = mapStatP2pSyn.find(hashFork);
        if (it == mapStatP2pSyn.end())
        {
            it = mapStatP2pSyn.insert(make_pair(hashFork, CStatP2pSynFork())).first;
            if (it == mapStatP2pSyn.end())
            {
                return false;
            }
        }
        (*it).second.AddSendStatData(nBlockCountIn, nTxCountIn);
    }
    return true;
}

bool CDataStat::AddP2pSynTxSynStatData(const uint256& hashFork, bool fRecv)
{
    if (fStatWork)
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        map<uint256, CStatP2pSynFork>::iterator it = mapStatP2pSyn.find(hashFork);
        if (it == mapStatP2pSyn.end())
        {
            it = mapStatP2pSyn.insert(make_pair(hashFork, CStatP2pSynFork())).first;
            if (it == mapStatP2pSyn.end())
            {
                return false;
            }
        }
        (*it).second.AddTxSynStatData(fRecv);
    }
    return true;
}

bool CDataStat::GetBlockMakerStatData(const uint256& hashFork, uint32 nBeginTime, uint32 nGetCount, std::vector<CStatItemBlockMaker>& vStatData)
{
    if (nBeginTime >= STAT_MAX_ITEM_COUNT || nGetCount == 0 || nGetCount > STAT_MAX_ITEM_COUNT)
    {
        DebugLog("STAT", (string("GetBlockMakerStatData fail: nBeginTime: ")
                          + to_string(nBeginTime) + string(", nGetCount: ")
                          + to_string(nGetCount) + string(".\n"))
                             .c_str());
        return false;
    }
    if (fStatWork)
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        vStatData.clear();
        if (hashFork == 0)
        {
            bool fGetFirst = false;
            map<uint256, CStatBlockMakerFork>::iterator it = mapStatBlockMaker.begin();
            for (; it != mapStatBlockMaker.end(); ++it)
            {
                if (!fGetFirst)
                {
                    fGetFirst = true;
                    if (!(*it).second.GetStatData(nBeginTime, nGetCount, vStatData))
                    {
                        DebugLog("STAT", "GetBlockMakerStatData: GetStatData fail.");
                        return false;
                    }
                }
                else
                {
                    if (!(*it).second.CumulativeStatData(nBeginTime, nGetCount, vStatData))
                    {
                        DebugLog("STAT", "GetBlockMakerStatData: CumulativeStatData fail.");
                        return false;
                    }
                }
            }
            return true;
        }
        else
        {
            map<uint256, CStatBlockMakerFork>::iterator it = mapStatBlockMaker.find(hashFork);
            if (it != mapStatBlockMaker.end())
            {
                (*it).second.GetStatData(nBeginTime, nGetCount, vStatData);
                return true;
            }
            else
            {
                DebugLog("STAT",
                         (string("GetBlockMakerStatData: find fork fail, fork hash: ") + hashFork.ToString()).c_str());
                return false;
            }
        }
    }
    return false;
}

bool CDataStat::GetP2pSynStatData(const uint256& hashFork, uint32 nBeginTime, uint32 nGetCount, std::vector<CStatItemP2pSyn>& vStatData)
{
    if (nBeginTime >= STAT_MAX_ITEM_COUNT || nGetCount == 0 || nGetCount > STAT_MAX_ITEM_COUNT)
    {
        DebugLog("STAT",
                 (string("GetP2pSynStatData fail: nBeginTime: ") + to_string(nBeginTime) + string(", nGetCount: ") + to_string(nGetCount) + string(".\n")).c_str());
        return false;
    }
    if (fStatWork)
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        vStatData.clear();
        if (hashFork == 0)
        {
            bool fGetFirst = false;
            map<uint256, CStatP2pSynFork>::iterator it = mapStatP2pSyn.begin();
            for (; it != mapStatP2pSyn.end(); ++it)
            {
                if (!fGetFirst)
                {
                    fGetFirst = true;
                    if (!(*it).second.GetStatData(nBeginTime, nGetCount, vStatData))
                    {
                        DebugLog("STAT", "GetP2pSynStatData: GetStatData fail.");
                        return false;
                    }
                }
                else
                {
                    if (!(*it).second.CumulativeStatData(nBeginTime, nGetCount, vStatData))
                    {
                        DebugLog("STAT", "GetP2pSynStatData: CumulativeStatData fail.");
                        return false;
                    }
                }
            }
            return true;
        }
        else
        {
            map<uint256, CStatP2pSynFork>::iterator it = mapStatP2pSyn.find(hashFork);
            if (it != mapStatP2pSyn.end())
            {
                (*it).second.GetStatData(nBeginTime, nGetCount, vStatData);
                return true;
            }
            else
            {
                DebugLog("STAT",
                         (string("GetP2pSynStatData: find fork fail, fork hash: ") + hashFork.ToString()).c_str());
                return false;
            }
        }
    }
    return false;
}

} // namespace bigbang
