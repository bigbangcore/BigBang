// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "entry.h"

#if !defined(WIN32) && !defined(__APPLE__)
#include <malloc.h>
#endif
#include <map>
#include <string>

#include "blockchain.h"
#include "blockmaker.h"
#include "checkrepair.h"
#include "consensus.h"
#include "core.h"
#include "datastat.h"
#include "dbpclient.h"
#include "dbpserver.h"
#include "dbpservice.h"
#include "defs.h"
#include "delegatedchn.h"
#include "dispatcher.h"
#include "forkmanager.h"
#include "miner.h"
#include "netchn.h"
#include "network.h"
#include "purger.h"
#include "recovery.h"
#include "rpcclient.h"
#include "rpcmod.h"
#include "service.h"
#include "txpool.h"
#include "version.h"
#include "wallet.h"

#ifdef WIN32
#ifdef _MSC_VER
#pragma warning(disable : 4786)
#pragma warning(disable : 4804)
#pragma warning(disable : 4805)
#pragma warning(disable : 4717)
#endif
#include "shlobj.h"
#include "shlwapi.h"
#include "windows.h"
#endif

#define MINIMUM_HARD_DISK_AVAILABLE 104857600

using namespace std;
using namespace xengine;
using namespace boost::filesystem;

const char* GetGitVersion();

namespace bigbang
{

//////////////////////////////
// CBbEntry

CBbEntry& CBbEntry::GetInstance()
{
    static CBbEntry entry;
    return entry;
}

CBbEntry::CBbEntry()
{
}

CBbEntry::~CBbEntry()
{
    Exit();
}

bool CBbEntry::Initialize(int argc, char* argv[])
{
    if (!config.Load(argc, argv, GetDefaultDataDir(), "bigbang.conf") || !config.PostLoad())
    {
        cerr << "Failed to load/parse arguments and config file\n";
        return false;
    }

    // help
    if (config.GetConfig()->fHelp)
    {
        cout << config.Help() << endl;
        return false;
    }

    // version
    if (config.GetConfig()->fVersion)
    {
        cout << "Bigbang version is v" << VERSION_STR << ", git commit id is " << GetGitVersion() << endl;
        return false;
    }

    // purge
    if (config.GetConfig()->fPurge)
    {
        PurgeStorage();
        return false;
    }

    // list config if in debug mode
    if (config.GetConfig()->fDebug)
    {
        config.ListConfig();
    }

    // check log size
    if (config.GetConfig()->nLogFileSize < 1 || config.GetConfig()->nLogFileSize > 2048)
    {
        cerr << "Log file size beyond range(range: 1 ~ 2048), value: " << config.GetConfig()->nLogFileSize << "\n";
        return false;
    }
    if (config.GetConfig()->nLogHistorySize < 2 || config.GetConfig()->nLogHistorySize > 0x7FFFFFFF)
    {
        cerr << "Log history size beyond range(range: 2 ~ 2147483647), value: " << config.GetConfig()->nLogHistorySize << "\n";
        return false;
    }

    // path
    path& pathData = config.GetConfig()->pathData;
    if (!exists(pathData))
    {
        if (!create_directories(pathData))
        {
            cerr << "Failed create directory : " << pathData << "\n";
            return false;
        }
    }

    if (!is_directory(pathData))
    {
        cerr << "Failed to access data directory : " << pathData << "\n";
        return false;
    }

    // hard disk
    if (space(pathData).available < MINIMUM_HARD_DISK_AVAILABLE)
    {
        cerr << "Warning: hard disk available < 100M\n";
        return false;
    }

    // daemon
    if (config.GetConfig()->fDaemon && (config.GetModeType() == EModeType::SERVER || config.GetModeType() == EModeType::MINER))
    {
        if (!RunInBackground(pathData))
        {
            return false;
        }
        cout << "bigbang server starting\n";
    }

    // log
    if ((config.GetModeType() == EModeType::SERVER || config.GetModeType() == EModeType::MINER)
        && log.SetLogFilePath((pathData / "bigbang.log").string())
        && !InitLog(pathData, config.GetConfig()->fDebug, config.GetConfig()->fDaemon, config.GetConfig()->nLogFileSize, config.GetConfig()->nLogHistorySize))
    {
        cerr << "Failed to open log file : " << (pathData / "bigbang.log") << "\n";
        return false;
    }

    // check and repair data
    if (config.GetModeType() == EModeType::SERVER
        && (config.GetConfig()->fCheckRepair || config.GetConfig()->fOnlyCheck))
    {
        CCheckRepairData check(pathData.string(), config.GetConfig()->fTestNet, config.GetConfig()->fOnlyCheck);
        if (!check.CheckRepairData())
        {
            if (config.GetConfig()->fOnlyCheck)
            {
                StdError("Bigbang", "Check data fail.");
            }
            else
            {
                StdError("Bigbang", "Check and repair data fail.");
            }
            return false;
        }
        if (config.GetConfig()->fOnlyCheck)
        {
            StdLog("Bigbang", "Check data complete.");
            return false;
        }
        StdLog("Bigbang", "Check and repair data complete.");
    }

#if !defined(WIN32) && !defined(__APPLE__)
    StdLog("Bigbang", "malloc_trim: %d.", malloc_trim(0));
#endif

    // docker
    if (!docker.Initialize(config.GetConfig(), &log))
    {
        cerr << "Failed to initialize docker\n";
        return false;
    }
    StdLog("BigbangStartup", "Initialize: bigbang version is v%s, git commit id: %s", VERSION_STR.c_str(), GetGitVersion());

    // hard fork version
    if (config.GetConfig()->fTestNet)
    {
        HEIGHT_HASH_MULTI_SIGNER = HEIGHT_HASH_MULTI_SIGNER_TESTNET;
        HEIGHT_HASH_TX_DATA = HEIGHT_HASH_TX_DATA_TESTNET;
    }
    else
    {
        HEIGHT_HASH_MULTI_SIGNER = HEIGHT_HASH_MULTI_SIGNER_MAINNET;
        HEIGHT_HASH_TX_DATA = HEIGHT_HASH_TX_DATA_MAINNET;
    }

    // modules
    return InitializeModules(config.GetModeType());
}

bool CBbEntry::AttachModule(IBase* pBase)
{
    if (!pBase || !docker.Attach(pBase))
    {
        delete pBase;
        return false;
    }

    return true;
}

bool CBbEntry::InitializeModules(const EModeType& mode)
{
    const std::vector<EModuleType>& modules = CMode::GetModules(mode);

    for (auto& m : modules)
    {
        switch (m)
        {
        case EModuleType::LOCK:
        {
            if (!TryLockFile((config.GetConfig()->pathData / ".lock").string()))
            {
                cerr << "Cannot obtain a lock on data directory " << config.GetConfig()->pathData << "\n"
                     << "Bigbang is probably already running.\n";
                return false;
            }
            break;
        }
        case EModuleType::BLOCKMAKER:
        {
            if (!AttachModule(new CBlockMaker()))
            {
                return false;
            }
            break;
        }
        case EModuleType::COREPROTOCOL:
        {
            if (!AttachModule(config.GetConfig()->fTestNet ? new CTestNetCoreProtocol : new CCoreProtocol()))
            {
                return false;
            }
            break;
        }
        case EModuleType::DISPATCHER:
        {
            if (!AttachModule(new CDispatcher()))
            {
                return false;
            }
            break;
        }
        case EModuleType::HTTPGET:
        {
            if (!AttachModule(new CHttpGet()))
            {
                return false;
            }
            break;
        }
        case EModuleType::HTTPSERVER:
        {
            if (!AttachModule(new CHttpServer()))
            {
                return false;
            }
            break;
        }
        case EModuleType::MINER:
        {
            if (!AttachModule(new CMiner(config.GetConfig()->vecCommand)))
            {
                return false;
            }
            break;
        }
        case EModuleType::NETCHANNEL:
        {
            if (!AttachModule(new CNetChannel()))
            {
                return false;
            }
            break;
        }
        case EModuleType::DELEGATEDCHANNEL:
        {
            if (!AttachModule(new CDelegatedChannel()))
            {
                return false;
            }
            break;
        }
        case EModuleType::NETWORK:
        {
            if (!AttachModule(new CNetwork()))
            {
                return false;
            }
            break;
        }
        case EModuleType::RPCCLIENT:
        {
            if (!AttachModule(new CRPCClient(config.GetConfig()->vecCommand.empty())))
            {
                return false;
            }
            break;
        }
        case EModuleType::RPCMODE:
        {
            auto pBase = docker.GetObject("httpserver");
            if (!pBase)
            {
                return false;
            }
            dynamic_cast<CHttpServer*>(pBase)->AddNewHost(GetRPCHostConfig());

            if (!AttachModule(new CRPCMod()))
            {
                return false;
            }
            break;
        }
        case EModuleType::SERVICE:
        {
            if (!AttachModule(new CService()))
            {
                return false;
            }
            break;
        }
        case EModuleType::TXPOOL:
        {
            if (!AttachModule(new CTxPool()))
            {
                return false;
            }
            break;
        }
        case EModuleType::WALLET:
        {
            IWallet* pWallet;
            if (config.GetConfig()->fWallet)
            {
                pWallet = new CWallet();
            }
            else
            {
                pWallet = new CDummyWallet();
            }

            if (!AttachModule(pWallet))
            {
                return false;
            }
            break;
        }
        case EModuleType::BLOCKCHAIN:
        {
            if (!AttachModule(new CBlockChain()))
            {
                return false;
            }
            break;
        }
        case EModuleType::FORKMANAGER:
        {
            if (!AttachModule(new CForkManager()))
            {
                return false;
            }
            break;
        }
        case EModuleType::CONSENSUS:
        {
            if (!AttachModule(new CConsensus()))
            {
                return false;
            }
            break;
        }
        case EModuleType::DATASTAT:
        {
            if (!AttachModule(new CDataStat()))
            {
                return false;
            }
            break;
        }
        case EModuleType::RECOVERY:
        {
            if (!AttachModule(new CRecovery()))
            {
                return false;
            }
            break;
        }
        case EModuleType::DBPSERVER:
        {
            if (!AttachModule(new CDbpServer()))
            {
                return false;
            }
            break;
        }
        case EModuleType::DBPCLIENT:
        {
            if (!AttachModule(new CBbDbpClient()))
            {
                return false;
            }
            break;
        }
        case EModuleType::DBPSERVICE:
        {
            auto pBase = docker.GetObject("dbpserver");
            if (!pBase)
            {
                return false;
            }
            dynamic_cast<CDbpServer*>(pBase)->AddNewHost(GetDbpHostConfig());

            auto pClientBase = docker.GetObject("dbpclient");
            if (!pClientBase)
            {
                return false;
            }
            dynamic_cast<CBbDbpClient*>(pClientBase)->AddNewClient(GetDbpClientConfig());

            if (!AttachModule(new CDbpService()))
            {
                return false;
            }
            break;
        }
        default:
            cerr << "Unknown module:%d" << CMode::IntValue(m) << endl;
            break;
        }
    }

    return true;
}

CHttpHostConfig CBbEntry::GetRPCHostConfig()
{
    const CRPCServerConfig* pConfig = CastConfigPtr<CRPCServerConfig*>(config.GetConfig());
    CIOSSLOption sslRPC(pConfig->fRPCSSLEnable, pConfig->fRPCSSLVerify,
                        pConfig->strRPCCAFile, pConfig->strRPCCertFile,
                        pConfig->strRPCPKFile, pConfig->strRPCCiphers);

    map<string, string> mapUsrRPC;
    if (!pConfig->strRPCUser.empty())
    {
        mapUsrRPC[pConfig->strRPCUser] = pConfig->strRPCPass;
    }

    return CHttpHostConfig(pConfig->epRPC, pConfig->nRPCMaxConnections, sslRPC, mapUsrRPC,
                           pConfig->vRPCAllowIP, "rpcmod");
}

CDbpHostConfig CBbEntry::GetDbpHostConfig()
{

    const CBbDbpServerConfig* pConfig = CastConfigPtr<CBbDbpServerConfig*>(config.GetConfig());
    CIOSSLOption sslDbp(pConfig->fDbpSSLEnable, pConfig->fDbpSSLVerify,
                        pConfig->strDbpCAFile, pConfig->strDbpCertFile,
                        pConfig->strDbpPKFile, pConfig->strDbpCiphers);

    map<string, string> mapUsrDbp;
    if (!pConfig->strDbpUser.empty())
    {
        mapUsrDbp[pConfig->strDbpUser] = pConfig->strDbpPass;
    }
    return CDbpHostConfig(pConfig->epDbp, pConfig->nDbpMaxConnections, pConfig->nDbpSessionTimeout,
                          sslDbp, mapUsrDbp, pConfig->vDbpAllowIP, "dbpservice");
}

CDbpClientConfig CBbEntry::GetDbpClientConfig()
{
    const CBbDbpClientConfig* pConfig = CastConfigPtr<CBbDbpClientConfig*>(config.GetConfig());
    CIOSSLOption sslDbp(pConfig->fDbpSSLEnable, pConfig->fDbpSSLVerify,
                        pConfig->strDbpCAFile, pConfig->strDbpCertFile,
                        pConfig->strDbpPKFile, pConfig->strDbpCiphers);

    return CDbpClientConfig(pConfig->epParentHost, pConfig->strSupportForks, sslDbp, "dbpservice");
}

void CBbEntry::PurgeStorage()
{
    path& pathData = config.GetConfig()->pathData;

    if (!TryLockFile((pathData / ".lock").string()))
    {
        cerr << "Cannot obtain a lock on data directory " << pathData << "\n"
             << "Bigbang is probably already running.\n";
        return;
    }

    storage::CPurger purger;
    if (purger(pathData))
    {
        cout << "Reset database and removed blockfiles\n";
    }
    else
    {
        cout << "Failed to purge storage\n";
    }
}

bool CBbEntry::Run()
{
    if (!docker.Run())
    {
        return false;
    }

    return CEntry::Run();
}

void CBbEntry::Exit()
{
    docker.Exit();

    if (config.GetConfig()->fDaemon && config.GetConfig()->vecCommand.empty() && !config.GetConfig()->fHelp)
    {
        ExitBackground(config.GetConfig()->pathData);
    }
}

path CBbEntry::GetDefaultDataDir()
{
    // Windows: C:\Documents and Settings\username\Local Settings\Application Data\Bigbang
    // Mac: ~/Library/Application Support/Bigbang
    // Unix: ~/.bigbang

#ifdef WIN32
    // Windows
    char pszPath[MAX_PATH] = "";
    if (SHGetSpecialFolderPathA(nullptr, pszPath, CSIDL_LOCAL_APPDATA, true))
    {
        return path(pszPath) / "Bigbang";
    }
    return path("C:\\Bigbang");
#else
    path pathRet;
    char* pszHome = getenv("HOME");
    if (pszHome == nullptr || strlen(pszHome) == 0)
    {
        pathRet = path("/");
    }
    else
    {
        pathRet = path(pszHome);
    }
#ifdef __APPLE__
    // Mac
    pathRet /= "Library/Application Support";
    create_directory(pathRet);
    return pathRet / "Bigbang";
#else
    // Unix
    return pathRet / ".bigbang";
#endif
#endif
}

bool CBbEntry::SetupEnvironment()
{
#ifdef _MSC_VER
    // Turn off microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, 0));
#endif
#if _MSC_VER >= 1400
    // Disable confusing "helpful" text message on abort, ctrl-c
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifndef WIN32
    umask(077);
#endif
    return true;
}

bool CBbEntry::RunInBackground(const path& pathData)
{
#ifndef WIN32
    // Daemonize
    ioService.notify_fork(boost::asio::io_service::fork_prepare);

    pid_t pid = fork();
    if (pid < 0)
    {
        cerr << "Error: fork() returned " << pid << " errno " << errno << "\n";
        return false;
    }
    if (pid > 0)
    {
        FILE* file = fopen((pathData / "bigbang.pid").string().c_str(), "w");
        if (file)
        {
            fprintf(file, "%d\n", pid);
            fclose(file);
        }
        exit(0);
    }

    pid_t sid = setsid();
    if (sid < 0)
    {
        cerr << "Error: setsid) returned " << sid << " errno " << errno << "\n";
        return false;
    }
    ioService.notify_fork(boost::asio::io_service::fork_child);
    return true;

#else
    HWND hwnd = GetForegroundWindow();
    cout << "daemon running, window will run in background" << endl;
    system("pause");
    ShowWindow(hwnd, SW_HIDE);
    return true;
#endif
    return false;
}

void CBbEntry::ExitBackground(const path& pathData)
{
#ifndef WIN32
    boost::filesystem::remove(pathData / "bigbang.pid");
#endif
}

} // namespace bigbang
