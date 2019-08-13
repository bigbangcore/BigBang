// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CRYPTO_KEYSTORE_H
#define CRYPTO_KEYSTORE_H

#include <boost/thread/thread.hpp>
#include <map>

#include "key.h"

namespace bigbang
{
namespace crypto
{

class CKeyStore
{
public:
    CKeyStore();
    virtual ~CKeyStore();
    void Clear();
    bool IsEmpty() const
    {
        return mapKey.empty();
    }
    bool AddKey(const CKey& key);
    void RemoveKey(const CPubKey& pubkey);
    bool HaveKey(const CPubKey& pubkey) const;
    bool GetSecret(const CPubKey& pubkey, CCryptoKeyData& vchSecret) const;
    void GetKeys(std::set<CPubKey>& setPubKey) const;
    bool EncryptKey(const CPubKey& pubkey, const CCryptoString& strPassphrase,
                    const CCryptoString& strCurrentPassphrase);
    bool IsLocked(const CPubKey& pubkey) const;
    bool LockKey(const CPubKey& pubkey);
    bool UnlockKey(const CPubKey& pubkey, const CCryptoString& strPassphrase = "");
    bool Sign(const CPubKey& pubkey, const uint256& hash, std::vector<uint8>& vchSig) const;

protected:
    mutable boost::shared_mutex rwAccess;
    std::map<CPubKey, CKey> mapKey;
};

} // namespace crypto
} // namespace bigbang

#endif //CRYPTO_KEYSTORE_H
