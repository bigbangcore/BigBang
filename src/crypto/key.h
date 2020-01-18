// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CRYPTO_KEY_H
#define CRYPTO_KEY_H

#include "crypto.h"

namespace bigbang
{
namespace crypto
{

class CPubKey : public uint256
{
public:
    CPubKey();
    CPubKey(const uint256& pubkey);
    bool Verify(const uint256& hash, const std::vector<uint8>& vchSig) const;
};

class CKey
{
public:
    enum
    {
        INIT,
        PRIVATE_KEY,
        PUBLIC_KEY,
    };

public:
    CKey();
    CKey(const CKey& key);
    CKey& operator=(const CKey& key);
    ~CKey();
    uint32 GetVersion() const;
    bool IsNull() const;
    bool IsLocked() const;
    bool IsPrivKey() const;
    bool IsPubKey() const;
    bool Renew();
    void Load(const CPubKey& pubkeyIn, const uint32 nVersionIn, const CCryptoCipher& cipherIn);
    bool Load(const std::vector<unsigned char>& vchKey);
    void Save(CPubKey& pubkeyRet, uint32& nVersionRet, CCryptoCipher& cipherRet) const;
    void Save(std::vector<unsigned char>& vchKey) const;
    bool SetSecret(const CCryptoKeyData& vchSecret);
    bool GetSecret(CCryptoKeyData& vchSecret) const;
    const CPubKey GetPubKey() const;
    const CCryptoCipher& GetCipher() const;
    bool Sign(const uint256& hash, std::vector<uint8>& vchSig) const;
    bool MultiSignDefect(const std::set<CPubKey>& setPubKey, const uint256& seed,
                         const uint256& hash, std::vector<uint8>& vchSig) const;
    bool MultiSign(const std::set<CPubKey>& setPubKey, const uint256& hash, std::vector<uint8>& vchSig) const;
    bool Encrypt(const CCryptoString& strPassphrase,
                 const CCryptoString& strCurrentPassphrase = "");
    void Lock();
    bool Unlock(const CCryptoString& strPassphrase = "");

protected:
    bool UpdateCipher(uint32 nVersionIn = INIT, const CCryptoString& strPassphrase = "");

protected:
    uint32 nVersion;
    CCryptoKey* pCryptoKey;
    CCryptoCipher cipher;
};

} // namespace crypto
} // namespace bigbang

#endif //CRYPTO_KEY_H
