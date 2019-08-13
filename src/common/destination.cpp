// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "destination.h"

#include "block.h"
#include "key.h"
#include "template/exchange.h"
#include "template/mint.h"
#include "template/template.h"
#include "transaction.h"
#include "util.h"

using namespace std;
using namespace xengine;
using namespace bigbang::crypto;

//////////////////////////////
// CDestination

CDestination::CDestination()
{
    SetNull();
}

CDestination::CDestination(const CPubKey& pubkey)
{
    SetPubKey(pubkey);
}

CDestination::CDestination(const CTemplateId& tid)
{
    SetTemplateId(tid);
}

void CDestination::SetNull()
{
    prefix = PREFIX_NULL;
    data = 0;
}

bool CDestination::IsNull() const
{
    return (prefix == PREFIX_NULL);
}

bool CDestination::IsPubKey() const
{
    return (prefix == PREFIX_PUBKEY);
}

bool CDestination::SetPubKey(const std::string& str)
{
    if (ParseString(str))
    {
        if (IsNull() || !IsPubKey())
        {
            return false;
        }
    }
    else
    {
        if (data.SetHex(str) != str.size())
        {
            return false;
        }
        prefix = PREFIX_PUBKEY;
    }

    return true;
}

CDestination& CDestination::SetPubKey(const CPubKey& pubkey)
{
    prefix = PREFIX_PUBKEY;
    data = pubkey;
    return *this;
}

bool CDestination::GetPubKey(CPubKey& pubkey) const
{
    if (prefix == PREFIX_PUBKEY)
    {
        pubkey = CPubKey(data);
        return true;
    }
    return false;
}

const CPubKey CDestination::GetPubKey() const
{
    return (prefix == PREFIX_PUBKEY) ? CPubKey(data) : CPubKey(uint64(0));
}

bool CDestination::IsTemplate() const
{
    return (prefix == PREFIX_TEMPLATE);
}

bool CDestination::SetTemplateId(const std::string& str)
{
    if (ParseString(str))
    {
        if (IsNull() || !IsTemplate())
        {
            return false;
        }
    }
    else
    {
        if (data.SetHex(str) != str.size())
        {
            return false;
        }
        prefix = PREFIX_TEMPLATE;
    }

    return true;
}

CDestination& CDestination::SetTemplateId(const CTemplateId& tid)
{
    prefix = PREFIX_TEMPLATE;
    data = tid;
    return *this;
}

bool CDestination::GetTemplateId(CTemplateId& tid) const
{
    if (prefix == PREFIX_TEMPLATE)
    {
        tid = CTemplateId(data);
        return true;
    }
    return false;
}

const CTemplateId CDestination::GetTemplateId() const
{
    return (prefix == PREFIX_TEMPLATE) ? CTemplateId(data) : CTemplateId(uint64(0));
}

bool CDestination::VerifyTxSignature(const uint256& hash, const uint256& hashAnchor, const CDestination& destTo,
                                     const std::vector<uint8>& vchSig, bool& fCompleted) const
{
    if (IsPubKey())
    {
        fCompleted = GetPubKey().Verify(hash, vchSig);
        return fCompleted;
    }
    else if (IsTemplate())
    {
        return CTemplate::VerifyTxSignature(GetTemplateId(), hash, hashAnchor, destTo, vchSig, fCompleted);
    }
    return false;
}

bool CDestination::VerifyTxSignature(const uint256& hash, const uint256& hashAnchor, const CDestination& destTo,
                                     const std::vector<uint8>& vchSig, int nForkHeight, const uint256& fork) const
{
    if (IsTemplate() && GetTemplateId().GetType() == TEMPLATE_EXCHANGE)
    {
        std::shared_ptr<CTemplateExchange> sp = std::make_shared<CTemplateExchange>(vchSig);
        return sp->VerifySignature(hash, vchSig, nForkHeight, fork);
    }
    else
    {
        bool fCompleted = false;
        return VerifyTxSignature(hash, hashAnchor, destTo, vchSig, fCompleted) && fCompleted;
    }
}

bool CDestination::VerifyBlockSignature(const uint256& hash, const vector<uint8>& vchSig) const
{
    if (IsPubKey())
    {
        return GetPubKey().Verify(hash, vchSig);
    }
    else if (IsTemplate())
    {
        return CTemplateMint::VerifyBlockSignature(GetTemplateId(), hash, vchSig);
    }
    return false;
}

bool CDestination::ParseString(const string& str)
{
    if (str.size() != DESTINATION_SIZE * 2 - 1 || str[0] < '0' || str[0] >= '0' + PREFIX_MAX)
    {
        return false;
    }
    prefix = str[0] - '0';
    return ParseHexString(&str[1], data.begin(), uint256::size()) == uint256::size();
}

string CDestination::ToString() const
{
    return (string(1, '0' + prefix) + ToHexString(data.begin(), data.size()));
}

void CDestination::ToDataStream(xengine::CODataStream& os) const
{
    os << prefix << data;
}

void CDestination::FromDataStream(xengine::CIDataStream& is)
{
    is >> prefix >> data;
}
