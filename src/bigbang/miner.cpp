// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_utils.h"
#include "json/json_spirit_writer_template.h"

#include "miner.h"
#include "rpc/rpc.h"

using namespace std;
using namespace xengine;
using namespace json_spirit;
using boost::asio::ip::tcp;
using namespace bigbang::rpc;

extern void Shutdown();

namespace bigbang
{

///////////////////////////////
// CMiner

CMiner::CMiner(const vector<string>& vArgsIn)
  : IIOModule("miner"),
    thrFetcher("fetcher", boost::bind(&CMiner::LaunchFetcher, this)),
    thrMiner("miner", boost::bind(&CMiner::LaunchMiner, this))
{
    nNonceGetWork = 1;
    nNonceSubmitWork = 2;
    nMinerStatus = -1;
    pHttpGet = nullptr;
    if (vArgsIn.size() >= 3)
    {
        strAddrSpent = vArgsIn[1];
        strMintKey = vArgsIn[2];
    }
}

CMiner::~CMiner()
{
}

bool CMiner::HandleInitialize()
{
    if (!GetObject("httpget", pHttpGet))
    {
        cerr << "Failed to request httpget\n";
        return false;
    }
    return true;
}

void CMiner::HandleDeinitialize()
{
    pHttpGet = nullptr;
}

bool CMiner::HandleInvoke()
{
    if (strAddrSpent.empty())
    {
        cerr << "Invalid spent address\n";
        return false;
    }
    if (strMintKey.empty())
    {
        cerr << "Invalid mint key\n";
        return false;
    }
    if (!ThreadDelayStart(thrFetcher))
    {
        return false;
    }
    if (!ThreadDelayStart(thrMiner))
    {
        return false;
    }
    nMinerStatus = MINER_RUN;
    return IIOModule::HandleInvoke();
}

void CMiner::HandleHalt()
{
    IIOModule::HandleHalt();

    {
        boost::unique_lock<boost::mutex> lock(mutex);
        nMinerStatus = MINER_EXIT;
    }
    condFetcher.notify_all();
    condMiner.notify_all();

    if (thrFetcher.IsRunning())
    {
        CancelRPC();
        thrFetcher.Interrupt();
    }
    thrFetcher.Interrupt();
    ThreadExit(thrFetcher);

    if (thrMiner.IsRunning())
    {
        thrMiner.Interrupt();
    }
    thrMiner.Interrupt();
    ThreadExit(thrMiner);
}

const CRPCClientConfig* CMiner::Config()
{
    return dynamic_cast<const CRPCClientConfig*>(IBase::Config());
}

bool CMiner::HandleEvent(CEventHttpGetRsp& event)
{
    try
    {
        CHttpRsp& rsp = event.data;

        if (rsp.nStatusCode < 0)
        {

            const char* strErr[] = { "", "connect failed", "invalid nonce", "activate failed",
                                     "disconnected", "no response", "resolve failed",
                                     "internal failure", "aborted" };
            throw runtime_error(rsp.nStatusCode >= HTTPGET_ABORTED ? strErr[-rsp.nStatusCode] : "unknown error");
        }
        if (rsp.nStatusCode == 401)
        {
            throw runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
        }
        else if (rsp.nStatusCode > 400 && rsp.nStatusCode != 404 && rsp.nStatusCode != 500)
        {
            ostringstream oss;
            oss << "server returned HTTP error " << rsp.nStatusCode;
            throw runtime_error(oss.str());
        }
        else if (rsp.strContent.empty())
        {
            throw runtime_error("no response from server");
        }

        // Parse reply
        if (event.nNonce == nNonceGetWork)
        {
            auto spResp = DeserializeCRPCResp<CGetWorkResult>(rsp.strContent);
            if (spResp->IsError())
            {
                // Error
                cerr << spResp->spError->Serialize(true) << endl;
                cerr << strHelpTips << endl;
            }
            else if (spResp->IsSuccessful())
            {
                auto spResult = CastResultPtr<CGetWorkResult>(spResp->spResult);

                int nRet = 0;
                if (!spResult->fResult.IsValid())
                {
                    nRet = -1;
                    cout << "get work fResult invalid." << endl;
                }
                else if (!spResult->work.IsValid())
                {
                    nRet = -2;
                    cout << "get work work invalid." << endl;
                }
                else if (!spResult->fResult)
                {
                    nRet = -3;
                    cout << "get work fResult is false." << endl;
                }
                else
                {
                    cout << "get work success." << endl;
                }

                if (nRet != 0)
                {
                    {
                        boost::unique_lock<boost::mutex> lock(mutex);

                        workCurrent.SetNull();
                        nMinerStatus = MINER_HOLD;
                    }
                    condMiner.notify_all();
                }
                else
                {
                    {
                        boost::unique_lock<boost::mutex> lock(mutex);

                        workCurrent.hashPrev.SetHex(spResult->work.strPrevblockhash);
                        workCurrent.nPrevBlockHeight = spResult->work.nPrevblockheight;
                        workCurrent.nPrevTime = spResult->work.nPrevblocktime;
                        workCurrent.nAlgo = spResult->work.nAlgo;
                        workCurrent.nBits = spResult->work.nBits;
                        workCurrent.vchWorkData = ParseHexString(spResult->work.strData);

                        nMinerStatus = MINER_RESET;
                    }
                    condMiner.notify_all();
                }
            }
            else
            {
                cerr << "server error: neither error nor result. resp: " << spResp->Serialize(true) << endl;
            }
        }
        else if (event.nNonce == nNonceSubmitWork)
        {
            auto spResp = DeserializeCRPCResp<CSubmitWorkResult>(rsp.strContent);
            if (spResp->IsError())
            {
                // Error
                cerr << spResp->spError->Serialize(true) << endl;
                cerr << strHelpTips << endl;
            }
            else if (spResp->IsSuccessful())
            {
                auto spResult = CastResultPtr<CSubmitWorkResult>(spResp->spResult);
                if (spResult->strHash.IsValid())
                {
                    cout << "Submited new block : " << spResult->strHash << "\n";
                }
            }
            else
            {
                cerr << "server error: neither error nor result. resp: " << spResp->Serialize(true) << endl;
            }
        }
        return true;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
    }
    catch (...)
    {
        StdError(__PRETTY_FUNCTION__, "unknown");
    }
    return true;
}

bool CMiner::SendRequest(uint64 nNonce, const string& content)
{
    CEventHttpGet eventHttpGet(nNonce);
    CHttpReqData& httpReqData = eventHttpGet.data;
    httpReqData.strIOModule = GetOwnKey();
    httpReqData.nTimeout = Config()->nRPCConnectTimeout;
    ;
    if (Config()->fRPCSSLEnable)
    {
        httpReqData.strProtocol = "https";
        httpReqData.fVerifyPeer = true;
        httpReqData.strPathCA = Config()->strRPCCAFile;
        httpReqData.strPathCert = Config()->strRPCCertFile;
        httpReqData.strPathPK = Config()->strRPCPKFile;
    }
    else
    {
        httpReqData.strProtocol = "http";
    }

    CNetHost host(Config()->strRPCConnect, Config()->nRPCPort);
    httpReqData.mapHeader["host"] = host.ToString();
    httpReqData.mapHeader["url"] = string("/");
    httpReqData.mapHeader["method"] = "POST";
    httpReqData.mapHeader["accept"] = "application/json";
    httpReqData.mapHeader["content-type"] = "application/json";
    httpReqData.mapHeader["user-agent"] = string("bigbang-json-rpc/");
    httpReqData.mapHeader["connection"] = "Keep-Alive";
    if (!Config()->strRPCPass.empty() || !Config()->strRPCUser.empty())
    {
        string strAuth;
        CHttpUtil().Base64Encode(Config()->strRPCUser + ":" + Config()->strRPCPass, strAuth);
        httpReqData.mapHeader["authorization"] = string("Basic ") + strAuth;
    }

    httpReqData.strContent = content + "\n";

    return pHttpGet->DispatchEvent(&eventHttpGet);
}

bool CMiner::GetWork()
{
    try
    {
        //    nNonceGetWork += 2;
        auto spParam = MakeCGetWorkParamPtr();
        spParam->strSpent = strAddrSpent;
        spParam->strPrivkey = strMintKey;
        spParam->strPrev = workCurrent.hashPrev.GetHex();
        CRPCReqPtr spReq = MakeCRPCReqPtr(nNonceGetWork, spParam);
        return SendRequest(nNonceGetWork, spReq->Serialize());
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
    }
    return false;
}

bool CMiner::SubmitWork(const vector<unsigned char>& vchWorkData)
{
    Array params;
    try
    {
        //    nNonceSubmitWork += 2;
        auto spParam = MakeCSubmitWorkParamPtr();
        spParam->strData = ToHexString(vchWorkData);
        spParam->strSpent = strAddrSpent;
        spParam->strPrivkey = strMintKey;

        CRPCReqPtr spReq = MakeCRPCReqPtr(nNonceSubmitWork, spParam);
        return SendRequest(nNonceSubmitWork, spReq->Serialize());
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
    }
    return false;
}

void CMiner::CancelRPC()
{
    if (pHttpGet)
    {
        CEventHttpAbort eventAbort(0);
        CHttpAbort& httpAbort = eventAbort.data;
        httpAbort.strIOModule = GetOwnKey();
        httpAbort.vNonce.push_back(nNonceGetWork);
        httpAbort.vNonce.push_back(nNonceSubmitWork);
        pHttpGet->DispatchEvent(&eventAbort);
    }
}

uint256 CMiner::GetHashTarget(const CMinerWork& work, int64 nTime)
{
    int64 nPrevTime = work.nPrevTime;
    int nBits = work.nBits;

    if (nTime - nPrevTime < BLOCK_TARGET_SPACING)
    {
        return (nBits + 1);
    }

    nBits -= (nTime - nPrevTime - BLOCK_TARGET_SPACING) / PROOF_OF_WORK_DECAY_STEP;
    if (nBits < 16)
    {
        nBits = 16;
    }
    return ((~uint256(uint64(0)) >> nBits));
}

void CMiner::LaunchFetcher()
{
    while (nMinerStatus != MINER_EXIT)
    {
        if (!GetWork())
        {
            cerr << "Failed to getwork\n";
        }
        boost::system_time const timeout = boost::get_system_time() + boost::posix_time::seconds(30);
        {
            boost::unique_lock<boost::mutex> lock(mutex);
            while (nMinerStatus != MINER_EXIT)
            {
                if (!condFetcher.timed_wait(lock, timeout) || nMinerStatus == MINER_HOLD)
                {
                    break;
                }
            }
        }
    }
}

void CMiner::LaunchMiner()
{
    while (nMinerStatus != MINER_EXIT)
    {
        CMinerWork work;
        {
            boost::unique_lock<boost::mutex> lock(mutex);
            while (nMinerStatus == MINER_HOLD)
            {
                condMiner.wait(lock);
            }
            if (nMinerStatus == MINER_EXIT)
            {
                break;
            }
            if (workCurrent.IsNull() || workCurrent.nAlgo != CM_CRYPTONIGHT)
            {
                nMinerStatus = MINER_HOLD;
                continue;
            }
            work = workCurrent;
            nMinerStatus = MINER_RUN;
        }

        uint32& nTime = *((uint32*)&work.vchWorkData[4]);
        uint256& nNonce = *((uint256*)&work.vchWorkData[work.vchWorkData.size() - 32]);

        if (work.nAlgo == CM_CRYPTONIGHT)
        {
            cout << "Get cryptonight work,prev block hash : " << work.hashPrev.GetHex() << "\n";
            uint256 hashTarget = GetHashTarget(work, nTime);
            while (nMinerStatus == MINER_RUN)
            {
                int64 t = GetTime();
                if (t > nTime)
                {
                    nTime = t;
                    hashTarget = GetHashTarget(work, t);
                }
                for (int i = 0; i < 100 * 1024; i++, nNonce += 256)
                {
                    uint256 hash = crypto::CryptoPowHash(&work.vchWorkData[0], work.vchWorkData.size());
                    if (hash <= hashTarget)
                    {
                        cout << "Proof-of-work found\n hash : " << hash.GetHex() << "\ntarget : " << hashTarget.GetHex() << "\n";
                        if (!SubmitWork(work.vchWorkData))
                        {
                            cerr << "Failed to submit work\n";
                        }
                        boost::unique_lock<boost::mutex> lock(mutex);
                        if (nMinerStatus == MINER_RUN)
                        {
                            nMinerStatus = MINER_HOLD;
                        }
                        break;
                    }
                }
            }
            condFetcher.notify_all();
        }
    }
}

} // namespace bigbang
