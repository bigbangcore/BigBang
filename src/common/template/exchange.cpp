// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "exchange.h"

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../../bigbang/address.h"
#include "rpc/auto_protocol.h"
#include "stream/datastream.h"
#include "template.h"
#include "transaction.h"
#include "util.h"

using namespace std;
using namespace xengine;
using namespace bigbang::crypto;

//////////////////////////////
// CTemplateExchange

CTemplateExchange::CTemplateExchange(
    const CDestination& destSpend_m_,
    const CDestination& destSpend_s_,
    int height_m_,
    int height_s_,
    const uint256& fork_m_,
    const uint256& fork_s_)
  : CTemplate(TEMPLATE_EXCHANGE),
    destSpend_m(destSpend_m_),
    destSpend_s(destSpend_s_),
    height_m(height_m_),
    height_s(height_s_),
    fork_m(fork_m_),
    fork_s(fork_s_)
{
    vchData.clear();
    CODataStream os(vchData);
    os << destSpend_m.prefix << destSpend_m.data << destSpend_s.prefix << destSpend_s.data << height_m << height_s << fork_m << fork_s;
    nId = CTemplateId(nType, bigbang::crypto::CryptoHash(vchData.data(), vchData.size()));
}

CTemplateExchange::CTemplateExchange(const std::vector<unsigned char>& vchDataIn)
  : CTemplate(TEMPLATE_EXCHANGE)
{
    xengine::CIDataStream is(vchDataIn);
    is >> destSpend_m.prefix >> destSpend_m.data >> destSpend_s.prefix >> destSpend_s.data >> height_m >> height_s >> fork_m >> fork_s;
    vchData.assign(vchDataIn.begin(), vchDataIn.begin() + 138);
    nId = CTemplateId(nType, bigbang::crypto::CryptoHash(vchData.data(), vchData.size()));
}

CTemplateExchange::CTemplateExchange(const CDestination& destSpend_m_,
                                     const CDestination& destSpend_s_,
                                     const uint256& fork_m_,
                                     const uint256& fork_s_,
                                     const int height_m_,
                                     const int height_s_)
  : CTemplate(TEMPLATE_EXCHANGE), destSpend_m(destSpend_m_), destSpend_s(destSpend_s_), fork_m(fork_m_), fork_s(fork_s_), height_m(height_m_), height_s(height_s_)
{
}

CTemplateExchange* CTemplateExchange::clone() const
{
    return new CTemplateExchange(*this);
}

void CTemplateExchange::GetTemplateData(bigbang::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.exchange.strFork_M = fork_m.ToString();
    obj.exchange.strFork_S = fork_s.ToString();

    obj.exchange.nHeight_M = height_m;
    obj.exchange.nHeight_S = height_s;

    obj.exchange.strSpend_M = bigbang::CAddress(destSpend_m).ToString();
    obj.exchange.strSpend_S = bigbang::CAddress(destSpend_s).ToString();
}

const CTemplateExchangePtr CTemplateExchange::CreateTemplatePtr(CTemplateExchange* ptr)
{
    return boost::dynamic_pointer_cast<CTemplateExchange>(CTemplate::CreateTemplatePtr(ptr));
}

bool CTemplateExchange::ValidateParam() const
{
    if (!destSpend_m.IsPubKey() || !destSpend_s.IsPubKey())
    {
        return false;
    }

    if (!fork_m || !fork_s)
    {
        return false;
    }

    if (height_m == 0 || height_s == 0 || height_m == height_s)
    {
        return false;
    }

    return true;
}

bool CTemplateExchange::SetTemplateData(const vector<uint8>& vchDataIn)
{
    CIDataStream is(vchDataIn);
    try
    {
        is >> destSpend_m.prefix >> destSpend_m.data >> destSpend_s.prefix >> destSpend_s.data >> height_m >> height_s >> fork_m >> fork_s;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

bool CTemplateExchange::SetTemplateData(const bigbang::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    if (obj.strType != GetTypeName(TEMPLATE_EXCHANGE))
    {
        return false;
    }

    const string& addr_m = obj.exchange.strAddr_M;
    const string& addr_s = obj.exchange.strAddr_S;
    bigbang::CAddress address_m(addr_m);
    bigbang::CAddress address_s(addr_s);
    if (address_m.IsNull() || address_s.IsNull())
    {
        return false;
    }

    destSpend_m = address_m;
    destSpend_s = address_s;

    height_m = obj.exchange.nHeight_M;
    height_s = obj.exchange.nHeight_S;

    if (fork_m.SetHex(obj.exchange.strFork_M) != obj.exchange.strFork_M.size())
    {
        return false;
    }

    if (fork_s.SetHex(obj.exchange.strFork_S) != obj.exchange.strFork_S.size())
    {
        return false;
    }

    return true;
}

void CTemplateExchange::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << destSpend_m.prefix << destSpend_m.data << destSpend_s.prefix << destSpend_s.data << height_m << height_s << fork_m << fork_s;
}

bool CTemplateExchange::VerifyTxSignature(const uint256& hash, const uint256& hashAnchor, const CDestination& destTo,
                                          const vector<uint8>& vchSig, const int32 nHeight, bool& fCompleted) const
{
    return true;
}

bool CTemplateExchange::GetSignDestination(const CTransaction& tx, const std::vector<uint8>& vchSig,
                                           std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const
{
    vector<unsigned char> vsm;
    vector<unsigned char> vss;
    uint256 hashFork;
    int height;
    xengine::CIDataStream ds(vchSig);
    try
    {
        ds >> vsm >> vss >> hashFork >> height;
    }
    catch(const std::exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }

    setSubDest.clear();
    if (!destSpend_m.IsPubKey() || !destSpend_s.IsPubKey())
    {
        return false;
    }

    if (hashFork == fork_m)
    {
        if (height > height_m)
        {
            setSubDest.insert(destSpend_m);
        }
        else
        {
            setSubDest.insert(destSpend_s);
        }
    }

    if (hashFork == fork_s)
    {
        if (height > height_s)
        {
            setSubDest.insert(destSpend_s);
        }
        else
        {
            setSubDest.insert(destSpend_m);
        }
    }
    return true;
}

bool CTemplateExchange::BuildTxSignature(const uint256& hash,
                                         const uint256& hashAnchor,
                                         const CDestination& destTo,
                                         const vector<uint8>& vchPreSig,
                                         vector<uint8>& vchSig) const
{

    vector<unsigned char> vsm;
    vector<unsigned char> vss;
    uint256 hashFork;
    int height;
    xengine::CIDataStream ds(vchSig);
    try
    {
        ds >> vsm >> vss >> hashFork >> height;
    }
    catch(const std::exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }

    vchSig = vchData;
    std::vector<uint8_t> temp;
    xengine::CODataStream ds_temp(temp);
    ds_temp << vsm << vss << vchPreSig;
    vchSig.insert(vchSig.end(), temp.begin(), temp.end());
    return true;
}

bool CTemplateExchange::VerifySignature(const uint256& hash, const std::vector<uint8>& vchSig, int height, const uint256& fork)
{
    std::vector<unsigned char> temp;
    temp.assign(vchSig.begin() + 138, vchSig.end());
    CIDataStream is(temp);
    std::vector<unsigned char> sign_m;
    std::vector<unsigned char> sign_s;
    std::vector<unsigned char> vchSig_;
    try
    {
        is >> sign_m >> sign_s >> vchSig_;
    }
    catch(const std::exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }

    if (fork == fork_m)
    {
        if (height > height_m)
        {
            return destSpend_m.GetPubKey().Verify(hash, vchSig_);
        }
        else
        {
            uint256 hash_(GetTemplateId());
            if (destSpend_m.GetPubKey().Verify(hash_, sign_m) && destSpend_s.GetPubKey().Verify(hash_, sign_s))
            {
                return destSpend_s.GetPubKey().Verify(hash, vchSig_);
            }
        }
    }
    else if (fork == fork_s)
    {
        if (height > height_s)
        {
            return destSpend_s.GetPubKey().Verify(hash, vchSig_);
        }
        else
        {
            uint256 hash_(GetTemplateId());
            if (destSpend_m.GetPubKey().Verify(hash_, sign_m) && destSpend_s.GetPubKey().Verify(hash_, sign_s))
            {
                return destSpend_m.GetPubKey().Verify(hash, vchSig_);
            }
        }
    }
    return false;
}