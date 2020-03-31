// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_DATASTAT_H
#define BIGBANG_DATASTAT_H

#include <string>
#include <vector>

#include "base.h"
#include "xengine.h"

#define STAT_MAX_ITEM_COUNT 1440

namespace bigbang
{

//////////////////////////////////
// CStatBlockMakerFork

class CStatBlockMakerFork
{
public:
    CStatBlockMakerFork();
    ~CStatBlockMakerFork();

    void AddStatData(bool fPOW, uint64 nTxCountIn);
    void TimerStatData(uint32 nTimeValue);
    bool GetStatData(uint32 nBeginTime, uint32 nGetCount, std::vector<CStatItemBlockMaker>& vOut);
    bool CumulativeStatData(uint32 nBeginTime, uint32 nGetCount, std::vector<CStatItemBlockMaker>& vOut);

protected:
    uint64 nStatPOWBlockCount;
    uint64 nStatDPOSBlockCount;
    uint64 nStatTxCount;
    std::vector<CStatItemBlockMaker> vStatTable;
};

//////////////////////////////////
// CStatP2pSynFork

class CStatP2pSynFork
{
public:
    CStatP2pSynFork();
    ~CStatP2pSynFork();

    void AddRecvStatData(uint64 nBlockCountIn, uint64 nTxCountIn);
    void AddSendStatData(uint64 nBlockCountIn, uint64 nTxCountIn);
    void AddTxSynStatData(bool fRecv);
    void TimerStatData(uint32 nTimeValue);
    bool GetStatData(uint32 nBeginTime, uint32 nGetCount, std::vector<CStatItemP2pSyn>& vOut);
    bool CumulativeStatData(uint32 nBeginTime, uint32 nGetCount, std::vector<CStatItemP2pSyn>& vOut);

protected:
    uint64 nStatRecvBlockCount;
    uint64 nStatRecvTxCount;
    uint64 nStatSendBlockCount;
    uint64 nStatSendTxCount;
    uint64 nStatSynRecvTxCount;
    uint64 nStatSynSendTxCount;
    std::vector<CStatItemP2pSyn> vStatTable;
};

//////////////////////////////
// CDataStat

class CDataStat : public IDataStat
{
public:
    CDataStat();
    ~CDataStat();

    bool AddBlockMakerStatData(const uint256& hashFork, bool fPOW, uint64 nTxCountIn) override;

    bool AddP2pSynRecvStatData(const uint256& hashFork, uint64 nBlockCountIn, uint64 nTxCountIn) override;
    bool AddP2pSynSendStatData(const uint256& hashFork, uint64 nBlockCountIn, uint64 nTxCountIn) override;
    bool AddP2pSynTxSynStatData(const uint256& hashFork, bool fRecv) override;

    bool GetBlockMakerStatData(const uint256& hashFork, uint32 nBeginTime, uint32 nGetCount, std::vector<CStatItemBlockMaker>& vStatData) override;
    bool GetP2pSynStatData(const uint256& hashFork, uint32 nBeginTime, uint32 nGetCount, std::vector<CStatItemP2pSyn>& vStatData) override;

protected:
    const CRPCServerConfig* RPCServerConfig();

    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;

    bool Initialize();
    void Clear();
    void StatTimerProc();
    void BlockMakerTimerStat(uint32 nTimeValue);
    void P2pSynTimerStat(uint32 nTimeValue);

protected:
    ICoreProtocol* pCoreProtocol;

    xengine::CThread thrStatTimer;
    bool fRunFlag;
    bool fStatWork;

    boost::mutex mutex;
    boost::condition_variable condStat;

    std::map<uint256, CStatBlockMakerFork> mapStatBlockMaker;
    std::map<uint256, CStatP2pSynFork> mapStatP2pSyn;
};

} // namespace bigbang

#endif //BIGBANG_DATASTAT_H
