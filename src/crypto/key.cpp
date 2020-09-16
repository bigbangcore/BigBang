// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"

#include "stream/datastream.h"
#include "util.h"

using namespace xengine;

namespace bigbang
{
namespace crypto
{

//////////////////////////////
// CPubKey
CPubKey::CPubKey()
{
}

CPubKey::CPubKey(const uint256& pubkey)
  : uint256(pubkey)
{
}

bool CPubKey::Verify(const uint256& hash, const std::vector<uint8>& vchSig) const
{
    return CryptoVerify(*this, &hash, sizeof(hash), vchSig);
}

//////////////////////////////
// CKey

CKey::CKey()
{
    nVersion = INIT;
    pCryptoKey = CryptoAlloc<CCryptoKey>();
    if (!pCryptoKey)
    {
        throw CCryptoError("CKey : Failed to alloc memory");
    }
    pCryptoKey->secret = 0;
    pCryptoKey->pubkey = 0;
}

CKey::CKey(const CKey& key)
{
    pCryptoKey = CryptoAlloc<CCryptoKey>();
    if (!pCryptoKey)
    {
        throw CCryptoError("CKey : Failed to alloc memory");
    }
    nVersion = key.nVersion;
    *pCryptoKey = *key.pCryptoKey;
    cipher = key.cipher;
}

CKey& CKey::operator=(const CKey& key)
{
    nVersion = key.nVersion;
    *pCryptoKey = *key.pCryptoKey;
    cipher = key.cipher;
    return *this;
}

CKey::~CKey()
{
    CryptoFree(pCryptoKey);
}

uint32 CKey::GetVersion() const
{
    return nVersion;
}

bool CKey::IsNull() const
{
    return (pCryptoKey->pubkey == 0);
}

bool CKey::IsLocked() const
{
    return (pCryptoKey->secret == 0);
}

bool CKey::IsPrivKey() const
{
    return nVersion == PRIVATE_KEY;
}

bool CKey::IsPubKey() const
{
    return nVersion == PUBLIC_KEY;
}

bool CKey::Renew()
{
    return (CryptoMakeNewKey(*pCryptoKey) != 0 && UpdateCipher());
}

void CKey::Load(const CPubKey& pubkeyIn, const uint32 nVersionIn, const CCryptoCipher& cipherIn)
{
    pCryptoKey->pubkey = pubkeyIn;
    pCryptoKey->secret = 0;
    nVersion = nVersionIn;
    cipher = cipherIn;
}

bool CKey::Load(const std::vector<unsigned char>& vchKey)
{
    if (vchKey.size() != 96)
    {
        return false;
    }

    CPubKey pubkey;
    int version;
    CCryptoCipher cipherNew;
    uint32 check;

    xengine::CIDataStream is(vchKey);
    try
    {
        is >> pubkey >> version;
        is.Pop(cipherNew.encrypted, 48);
        is >> cipherNew.nonce >> check;
    }
    catch (const std::exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }

    if (CryptoHash(&vchKey[0], vchKey.size() - 4).Get32() != check)
    {
        return false;
    }
    Load(pubkey, version, cipherNew);
    return true;
}

void CKey::Save(CPubKey& pubkeyRet, uint32& nVersionRet, CCryptoCipher& cipherRet) const
{
    pubkeyRet = pCryptoKey->pubkey;
    nVersionRet = nVersion;
    cipherRet = cipher;
}

void CKey::Save(std::vector<unsigned char>& vchKey) const
{
    vchKey.clear();
    vchKey.reserve(96);

    xengine::CODataStream os(vchKey);
    os << GetPubKey() << GetVersion();
    os.Push(GetCipher().encrypted, 48);
    os << GetCipher().nonce;

    uint256 hash = CryptoHash(&vchKey[0], vchKey.size());
    os << hash.Get32();
}

bool CKey::SetSecret(const CCryptoKeyData& vchSecret)
{
    if (vchSecret.size() != 32)
    {
        return false;
    }
    return (CryptoImportKey(*pCryptoKey, *((uint256*)&vchSecret[0])) != 0
            && UpdateCipher());
}

bool CKey::GetSecret(CCryptoKeyData& vchSecret) const
{
    if (!IsNull() && !IsLocked())
    {
        vchSecret.assign(pCryptoKey->secret.begin(), pCryptoKey->secret.end());
        return true;
    }
    return false;
}

const CPubKey CKey::GetPubKey() const
{
    return CPubKey(pCryptoKey->pubkey);
}

const CCryptoCipher& CKey::GetCipher() const
{
    return cipher;
}

bool CKey::Sign(const uint256& hash, std::vector<uint8>& vchSig) const
{
    if (IsNull())
    {
        StdError("CKey", "Sign: pubkey is null");
        return false;
    }
    if (IsLocked())
    {
        StdError("CKey", "Sign: pubkey is locked");
        return false;
    }
    CryptoSign(*pCryptoKey, &hash, sizeof(hash), vchSig);
    return true;
}

bool CKey::MultiSignDefect(const std::set<CPubKey>& setPubKey, const uint256& seed,
                           const uint256& hash, std::vector<uint8>& vchSig) const
{
    if (!IsNull() && !IsLocked())
    {
        CryptoMultiSignDefect(std::set<uint256>(setPubKey.begin(), setPubKey.end()), *pCryptoKey,
                              seed.begin(), seed.size(), hash.begin(), hash.size(), vchSig);
        return true;
    }
    return false;
}

bool CKey::MultiSign(const std::set<CPubKey>& setPubKey, const uint256& hash, std::vector<uint8>& vchSig) const
{
    if (!IsNull() && !IsLocked())
    {
        CryptoMultiSign(std::set<uint256>(setPubKey.begin(), setPubKey.end()), *pCryptoKey, hash.begin(), hash.size(), vchSig);
        return true;
    }
    return false;
}

bool CKey::AesEncrypt(const CPubKey& pubkeyRemote, const std::vector<uint8>& vMessage, std::vector<uint8>& vCiphertext) const
{
    if (!IsNull() && !IsLocked())
    {
        return CryptoAesEncrypt(pCryptoKey->secret, pubkeyRemote, vMessage, vCiphertext);
    }
    return false;
}

bool CKey::AesDecrypt(const CPubKey& pubkeyRemote, const std::vector<uint8>& vCiphertext, std::vector<uint8>& vMessage) const
{
    if (!IsNull() && !IsLocked())
    {
        return CryptoAesDecrypt(pCryptoKey->secret, pubkeyRemote, vCiphertext, vMessage);
    }
    return false;
}

bool CKey::Encrypt(const CCryptoString& strPassphrase,
                   const CCryptoString& strCurrentPassphrase)
{
    if (!IsLocked())
    {
        Lock();
    }
    if (Unlock(strCurrentPassphrase))
    {
        return UpdateCipher(1, strPassphrase);
    }
    return false;
}

void CKey::Lock()
{
    pCryptoKey->secret = 0;
}

bool CKey::Unlock(const CCryptoString& strPassphrase)
{
    try
    {
        return CryptoDecryptSecret(nVersion, strPassphrase, cipher, *pCryptoKey);
    }
    catch (std::exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
    }
    return false;
}

bool CKey::UpdateCipher(uint32 nVersionIn, const CCryptoString& strPassphrase)
{
    try
    {
        CCryptoCipher cipherNew;
        if (CryptoEncryptSecret(nVersionIn, strPassphrase, *pCryptoKey, cipherNew))
        {
            nVersion = nVersionIn;
            cipher = cipherNew;
            return true;
        }
    }
    catch (std::exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
    }
    return false;
}

} // namespace crypto
} // namespace bigbang
