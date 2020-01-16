// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_DESTINATION_H
#define COMMON_DESTINATION_H

#include <vector>

#include "key.h"
#include "stream/datastream.h"
#include "stream/stream.h"
#include "template/templateid.h"
#include "uint256.h"

class CTemplate;

class CTransaction;
class CBlock;

class CDestination
{
    friend class xengine::CStream;

public:
    uint8 prefix;
    uint256 data;

    enum
    {
        PREFIX_NULL = 0x00,
        PREFIX_PUBKEY = 0x01,
        PREFIX_TEMPLATE = 0x02,
        PREFIX_MAX
    };
    enum
    {
        DESTINATION_SIZE = 33
    };

public:
    CDestination();
    CDestination(const bigbang::crypto::CPubKey& pubkey);
    CDestination(const CTemplateId& tid);
    virtual ~CDestination() = default;

    // null
    void SetNull();
    bool IsNull() const;

    // public key
    bool IsPubKey() const;
    bool SetPubKey(const std::string& str);
    CDestination& SetPubKey(const bigbang::crypto::CPubKey& pubkey);
    bool GetPubKey(bigbang::crypto::CPubKey& pubkey) const;
    const bigbang::crypto::CPubKey GetPubKey() const;

    // template id
    bool IsTemplate() const;
    bool SetTemplateId(const std::string& str);
    CDestination& SetTemplateId(const CTemplateId& tid);
    bool GetTemplateId(CTemplateId& tid) const;
    const CTemplateId GetTemplateId() const;

    bool VerifyTxSignature(const uint256& hash, const uint256& hashAnchor, const CDestination& destTo,
                           const std::vector<uint8>& vchSig, const int32 nHeight, bool& fCompleted) const;
    bool VerifyTxSignature(const uint256& hash, const uint256& hashAnchor, const CDestination& destTo,
                           const std::vector<uint8>& vchSig, const int32 nForkHeight, const uint256& fork, const int32 nHeight) const;

    bool VerifyBlockSignature(const uint256& hash, const std::vector<uint8>& vchSig) const;

    // format
    virtual bool ParseString(const std::string& str);
    virtual std::string ToString() const;

    void ToDataStream(xengine::CODataStream& os) const;
    void FromDataStream(xengine::CIDataStream& is);

protected:
    void Serialize(xengine::CStream& s, xengine::LoadType& opt)
    {
        s.Serialize(prefix, opt);
        s.Serialize(data, opt);
    }
    template <typename O>
    void Serialize(xengine::CStream& s, O& opt) const
    {
        s.Serialize(prefix, opt);
        s.Serialize(data, opt);
    }
};

inline bool operator==(const CDestination& a, const CDestination& b)
{
    return (a.prefix == b.prefix && a.data == b.data);
}

inline bool operator!=(const CDestination& a, const CDestination& b)
{
    return !(a == b);
}
inline bool operator<(const CDestination& a, const CDestination& b)
{
    return (a.prefix < b.prefix || (a.prefix == b.prefix && a.data < b.data));
}

#endif // COMMON_DESTINATION_H
