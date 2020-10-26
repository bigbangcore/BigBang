// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto.h"

#include <iostream>
#include <sodium.h>

#include "curve25519/curve25519.h"
#include "util.h"
#ifdef __cplusplus
extern "C"
{
#include "pow_hash/hash-ops.h"
}
#endif

using namespace std;

namespace bigbang
{
namespace crypto
{

//////////////////////////////
// CCryptoSodiumInitializer()
class CCryptoSodiumInitializer
{
public:
    CCryptoSodiumInitializer()
    {
        if (sodium_init() < 0)
        {
            throw CCryptoError("CCryptoSodiumInitializer : Failed to initialize libsodium");
        }
    }
    ~CCryptoSodiumInitializer()
    {
    }
};

static CCryptoSodiumInitializer _CCryptoSodiumInitializer;

//////////////////////////////
// Secure memory
void* CryptoAlloc(const size_t size)
{
    return sodium_malloc(size);
}

void CryptoFree(void* ptr)
{
    sodium_free(ptr);
}

//////////////////////////////
// Heap memory lock

void CryptoMLock(void* const addr, const size_t len)
{
    if (sodium_mlock(addr, len) < 0)
    {
        throw CCryptoError("CryptoMLock : Failed to mlock");
    }
}

void CryptoMUnlock(void* const addr, const size_t len)
{
    if (sodium_munlock(addr, len) < 0)
    {
        throw CCryptoError("CryptoMUnlock : Failed to munlock");
    }
}

//////////////////////////////
// Rand

uint32 CryptoGetRand32()
{
    return randombytes_random();
}

uint64 CryptoGetRand64()
{
    return (randombytes_random() | (((uint64)randombytes_random()) << 32));
}

void CryptoGetRand256(uint256& u)
{
    randombytes_buf(&u, 32);
}

//////////////////////////////
// Hash

uint256 CryptoHash(const void* msg, size_t len)
{
    uint256 hash;
    crypto_generichash_blake2b((uint8*)&hash, sizeof(hash), (const uint8*)msg, len, nullptr, 0);
    return hash;
}

uint256 CryptoHash(const uint256& h1, const uint256& h2)
{
    uint256 hash;
    crypto_generichash_blake2b_state state;
    crypto_generichash_blake2b_init(&state, nullptr, 0, sizeof(hash));
    crypto_generichash_blake2b_update(&state, h1.begin(), sizeof(h1));
    crypto_generichash_blake2b_update(&state, h2.begin(), sizeof(h2));
    crypto_generichash_blake2b_final(&state, hash.begin(), sizeof(hash));
    return hash;
}

uint256 CryptoPowHash(const void* msg, size_t len)
{
    uint256 hash;
    cn_slow_hash(msg, len, (char*)hash.begin(), 2, 0, 0);
    return hash;
}

//////////////////////////////
// Sign & verify

uint256 CryptoMakeNewKey(CCryptoKey& key)
{
    uint256 pk;
    while (crypto_sign_ed25519_keypair((uint8*)&pk, (uint8*)&key) == 0)
    {
        int count = 0;
        const uint8* p = key.secret.begin();
        for (int i = 1; i < 31; ++i)
        {
            if (p[i] != 0x00 && p[i] != 0xFF)
            {
                if (++count >= 4)
                {
                    return pk;
                }
            }
        }
    }
    return uint64(0);
}

uint256 CryptoImportKey(CCryptoKey& key, const uint256& secret)
{
    uint256 pk;
    crypto_sign_ed25519_seed_keypair((uint8*)&pk, (uint8*)&key, (uint8*)&secret);
    return pk;
}

void CryptoSign(const CCryptoKey& key, const void* md, const size_t len, vector<uint8>& vchSig)
{
    vchSig.resize(64);
    crypto_sign_ed25519_detached(&vchSig[0], nullptr, (const uint8*)md, len, (const uint8*)&key);
}

bool CryptoVerify(const uint256& pubkey, const void* md, const size_t len, const vector<uint8>& vchSig)
{
    return (vchSig.size() == 64
            && !crypto_sign_ed25519_verify_detached(&vchSig[0], (const uint8*)md, len, (const uint8*)&pubkey));
}

// return the nIndex key is signed in multiple signature
static bool IsSigned(const uint8* pIndex, const size_t nLen, const size_t nIndex)
{
    return (nLen * 8 > nIndex) && pIndex[nLen - 1 - nIndex / 8] & (1 << (nIndex % 8));
}

// set the nIndex key signed in multiple signature
static bool SetSigned(uint8* pIndex, const size_t nLen, const size_t nIndex)
{
    if (nLen * 8 <= nIndex)
    {
        return false;
    }
    pIndex[nLen - 1 - nIndex / 8] |= (1 << (nIndex % 8));
    return true;
}

bool CryptoMultiSign(const set<uint256>& setPubKey, const CCryptoKey& privkey, const uint8* pM, const size_t lenM, vector<uint8>& vchSig)
{
    if (setPubKey.empty())
    {
        xengine::StdTrace("multisign", "key set is empty");
        return false;
    }

    // index
    set<uint256>::const_iterator itPub = setPubKey.find(privkey.pubkey);
    if (itPub == setPubKey.end())
    {
        xengine::StdTrace("multisign", "no key %s in set", privkey.pubkey.ToString().c_str());
        return false;
    }
    size_t nIndex = distance(setPubKey.begin(), itPub);

    // unpack index of  (index,S,Ri,...,Rj)
    size_t nIndexLen = (setPubKey.size() - 1) / 8 + 1;
    if (vchSig.empty())
    {
        vchSig.resize(nIndexLen);
    }
    else if (vchSig.size() < nIndexLen + 64)
    {
        xengine::StdTrace("multisign", "vchSig size %lu is too short, need %lu minimum", vchSig.size(), nIndexLen + 64);
        return false;
    }
    uint8* pIndex = &vchSig[0];

    // already signed
    if (IsSigned(pIndex, nIndexLen, nIndex))
    {
        xengine::StdTrace("multisign", "key %s is already signed", privkey.pubkey.ToString().c_str());
        return true;
    }

    // position of RS, (index,Ri,Si,...,R,S,...,Rj,Sj)
    size_t nPosRS = nIndexLen;
    for (size_t i = 0; i < nIndex; ++i)
    {
        if (IsSigned(pIndex, nIndexLen, i))
        {
            nPosRS += 64;
        }
    }
    if (nPosRS > vchSig.size())
    {
        xengine::StdTrace("multisign", "index %lu key is signed, but not exist R", nIndex);
        return false;
    }

    // sign
    vector<uint8> vchRS;
    CryptoSign(privkey, pM, lenM, vchRS);

    // record
    if (!SetSigned(pIndex, nIndexLen, nIndex))
    {
        xengine::StdTrace("multisign", "set %lu index signed error", nIndex);
        return false;
    }
    vchSig.insert(vchSig.begin() + nPosRS, vchRS.begin(), vchRS.end());

    return true;
}

bool CryptoMultiVerify(const set<uint256>& setPubKey, const uint8* pM, const size_t lenM, const vector<uint8>& vchSig, set<uint256>& setPartKey)
{
    if (setPubKey.empty())
    {
        xengine::StdTrace("multiverify", "key set is empty");
        return false;
    }

    // unpack (index,Ri,Si,...,Rj,Sj)
    int nIndexLen = (setPubKey.size() - 1) / 8 + 1;
    if (vchSig.size() < (nIndexLen + 64))
    {
        xengine::StdTrace("multiverify", "vchSig size %lu is too short, need %lu minimum", vchSig.size(), nIndexLen + 64);
        return false;
    }
    const uint8* pIndex = &vchSig[0];

    size_t nPosRS = nIndexLen;
    size_t i = 0;
    for (auto itPub = setPubKey.begin(); itPub != setPubKey.end(); ++itPub, ++i)
    {
        if (IsSigned(pIndex, nIndexLen, i))
        {
            const uint256& pk = *itPub;
            if (nPosRS + 64 > vchSig.size())
            {
                xengine::StdTrace("multiverify", "index %lu key is signed, but not exist R", i);
                return false;
            }

            vector<uint8> vchRS(vchSig.begin() + nPosRS, vchSig.begin() + nPosRS + 64);
            if (!CryptoVerify(pk, pM, lenM, vchRS))
            {
                xengine::StdTrace("multiverify", "verify index %lu key sign failed", i);
                return false;
            }

            setPartKey.insert(pk);
            nPosRS += 64;
        }
    }

    if (nPosRS != vchSig.size())
    {
        xengine::StdTrace("multiverify", "vchSig size %lu is too long, need %lu", vchSig.size(), nPosRS);
        return false;
    }

    return true;
}

/******** defect old version multi-sign begin *********/
//     0        key1          keyn
// |________|________| ... |_______|
static vector<uint8> MultiSignPreApkDefect(const set<uint256>& setPubKey)
{
    vector<uint8> vecHash;
    vecHash.resize((setPubKey.size() + 1) * uint256::size());

    int i = uint256::size();
    for (const uint256& key : setPubKey)
    {
        memcpy(&vecHash[i], key.begin(), key.size());
        i += key.size();
    }

    return vecHash;
}

// H(Ai,A1,...,An)*Ai + ... + H(Aj,A1,...,An)*Aj
// setPubKey = [A1 ... An], setPartKey = [Ai ... Aj], 1 <= i <= j <= n
static bool MultiSignApkDefect(const set<uint256>& setPubKey, const set<uint256>& setPartKey, uint8* md32)
{
    vector<uint8> vecHash = MultiSignPreApkDefect(setPubKey);

    CEdwards25519 apk;
    for (const uint256& key : setPartKey)
    {
        memcpy(&vecHash[0], key.begin(), key.size());
        CSC25519 hash = CSC25519(CryptoHash(&vecHash[0], vecHash.size()).begin());

        CEdwards25519 point;
        if (!point.Unpack(key.begin()))
        {
            return false;
        }
        apk += point.ScalarMult(hash);
    }

    apk.Pack(md32);
    return true;
}

// hash = H(X,apk,M)
static CSC25519 MultiSignHashDefect(const uint8* pX, const size_t lenX, const uint8* pApk, const size_t lenApk, const uint8* pM, const size_t lenM)
{
    uint8 hash[32];

    crypto_generichash_blake2b_state state;
    crypto_generichash_blake2b_init(&state, nullptr, 0, 32);
    crypto_generichash_blake2b_update(&state, pX, lenX);
    crypto_generichash_blake2b_update(&state, pApk, lenApk);
    crypto_generichash_blake2b_update(&state, pM, lenM);
    crypto_generichash_blake2b_final(&state, hash, 32);

    return CSC25519(hash);
}

// r = H(H(sk,pk),M)
static CSC25519 MultiSignRDefect(const CCryptoKey& key, const uint8* pM, const size_t lenM)
{
    uint8 hash[32];

    crypto_generichash_blake2b_state state;
    crypto_generichash_blake2b_init(&state, NULL, 0, 32);
    uint256 keyHash = CryptoHash((uint8*)&key, 64);
    crypto_generichash_blake2b_update(&state, (uint8*)&keyHash, 32);
    crypto_generichash_blake2b_update(&state, pM, lenM);
    crypto_generichash_blake2b_final(&state, hash, 32);

    return CSC25519(hash);
}

static CSC25519 ClampPrivKeyDefect(const uint256& privkey)
{
    uint8 hash[64];
    crypto_hash_sha512(hash, privkey.begin(), privkey.size());
    hash[0] &= 248;
    hash[31] &= 127;
    hash[31] |= 64;

    return CSC25519(hash);
}

bool CryptoMultiSignDefect(const set<uint256>& setPubKey, const CCryptoKey& privkey, const uint8* pX, const size_t lenX,
                           const uint8* pM, const size_t lenM, vector<uint8>& vchSig)
{
    if (setPubKey.empty())
    {
        return false;
    }

    // unpack (index,R,S)
    CSC25519 S;
    CEdwards25519 R;
    int nIndexLen = (setPubKey.size() - 1) / 8 + 1;
    if (vchSig.empty())
    {
        vchSig.resize(nIndexLen + 64);
    }
    else
    {
        if (vchSig.size() != nIndexLen + 64)
        {
            return false;
        }
        if (!R.Unpack(&vchSig[nIndexLen]))
        {
            return false;
        }
        S.Unpack(&vchSig[nIndexLen + 32]);
    }
    uint8* pIndex = &vchSig[0];

    // apk
    uint256 apk;
    if (!MultiSignApkDefect(setPubKey, setPubKey, apk.begin()))
    {
        return false;
    }
    // H(X,apk,M)
    CSC25519 hash = MultiSignHashDefect(pX, lenX, apk.begin(), apk.size(), pM, lenM);

    // sign
    set<uint256>::const_iterator itPub = setPubKey.find(privkey.pubkey);
    if (itPub == setPubKey.end())
    {
        return false;
    }
    size_t index = distance(setPubKey.begin(), itPub);

    if (!(pIndex[index / 8] & (1 << (index % 8))))
    {
        // ri = H(H(si,pi),M)
        CSC25519 ri = MultiSignRDefect(privkey, pM, lenM);
        // hi = H(Ai,A1,...,An)
        vector<uint8> vecHash = MultiSignPreApkDefect(setPubKey);
        memcpy(&vecHash[0], itPub->begin(), itPub->size());
        CSC25519 hi = CSC25519(CryptoHash(&vecHash[0], vecHash.size()).begin());
        // si = H(privkey)
        CSC25519 si = ClampPrivKeyDefect(privkey.secret);
        // Si = ri + H(X,apk,M) * hi * si
        CSC25519 Si = ri + hash * hi * si;
        // S += Si
        S += Si;
        // R += Ri
        CEdwards25519 Ri;
        Ri.Generate(ri);
        R += Ri;

        pIndex[index / 8] |= (1 << (index % 8));
    }

    R.Pack(&vchSig[nIndexLen]);
    S.Pack(&vchSig[nIndexLen + 32]);

    return true;
}

bool CryptoMultiVerifyDefect(const set<uint256>& setPubKey, const uint8* pX, const size_t lenX,
                             const uint8* pM, const size_t lenM, const vector<uint8>& vchSig, set<uint256>& setPartKey)
{
    if (setPubKey.empty())
    {
        return false;
    }

    if (vchSig.empty())
    {
        return true;
    }

    // unpack (index,R,S)
    CSC25519 S;
    CEdwards25519 R;
    int nIndexLen = (setPubKey.size() - 1) / 8 + 1;
    if (vchSig.size() != (nIndexLen + 64))
    {
        return false;
    }
    if (!R.Unpack(&vchSig[nIndexLen]))
    {
        return false;
    }
    S.Unpack(&vchSig[nIndexLen + 32]);
    const uint8* pIndex = &vchSig[0];

    // apk
    uint256 apk;
    if (!MultiSignApkDefect(setPubKey, setPubKey, apk.begin()))
    {
        return false;
    }
    // H(X,apk,M)
    CSC25519 hash = MultiSignHashDefect(pX, lenX, apk.begin(), apk.size(), pM, lenM);

    // A = hi*Ai + ... + aj*Aj
    CEdwards25519 A;
    vector<uint8> vecHash = MultiSignPreApkDefect(setPubKey);
    setPartKey.clear();
    int i = 0;

    for (auto itPub = setPubKey.begin(); itPub != setPubKey.end(); ++itPub, ++i)
    {
        if (pIndex[i / 8] & (1 << (i % 8)))
        {
            // hi = H(Ai,A1,...,An)
            memcpy(&vecHash[0], itPub->begin(), itPub->size());
            CSC25519 hi = CSC25519(CryptoHash(&vecHash[0], vecHash.size()).begin());
            // hi * Ai
            CEdwards25519 Ai;
            if (!Ai.Unpack(itPub->begin()))
            {
                return false;
            }
            A += Ai.ScalarMult(hi);

            setPartKey.insert(*itPub);
        }
    }

    // SB = R + H(X,apk,M) * (hi*Ai + ... + aj*Aj)
    CEdwards25519 SB;
    SB.Generate(S);

    return SB == R + A.ScalarMult(hash);
}

/******** defect old version multi-sign end *********/

// Encrypt
void CryptoKeyFromPassphrase(int version, const CCryptoString& passphrase, const uint256& salt, CCryptoKeyData& key)
{
    key.resize(32);
    if (version == 0)
    {
        key.assign(salt.begin(), salt.end());
    }
    else if (version == 1)
    {
        if (crypto_pwhash_scryptsalsa208sha256(&key[0], 32, passphrase.c_str(), passphrase.size(), (const uint8*)&salt,
                                               crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_INTERACTIVE,
                                               crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_INTERACTIVE)
            != 0)
        {
            throw CCryptoError("CryptoKeyFromPassphrase : Failed to create key, key version = 1");
        }
    }
    else
    {
        throw CCryptoError("CryptoKeyFromPassphrase : Failed to create key, unknown key version");
    }
}

bool CryptoEncryptSecret(int version, const CCryptoString& passphrase, const CCryptoKey& key, CCryptoCipher& cipher)
{
    CCryptoKeyData ek;
    CryptoKeyFromPassphrase(version, passphrase, key.pubkey, ek);
    randombytes_buf(&cipher.nonce, sizeof(cipher.nonce));

    return (crypto_aead_chacha20poly1305_encrypt(cipher.encrypted, nullptr,
                                                 (const uint8*)&key.secret, 32,
                                                 (const uint8*)&key.pubkey, 32,
                                                 nullptr, (uint8*)&cipher.nonce, &ek[0])
            == 0);
}

bool CryptoDecryptSecret(int version, const CCryptoString& passphrase, const CCryptoCipher& cipher, CCryptoKey& key)
{
    CCryptoKeyData ek;
    CryptoKeyFromPassphrase(version, passphrase, key.pubkey, ek);
    return (crypto_aead_chacha20poly1305_decrypt((uint8*)&key.secret, nullptr, nullptr,
                                                 cipher.encrypted, 48,
                                                 (const uint8*)&key.pubkey, 32,
                                                 (const uint8*)&cipher.nonce, &ek[0])
            == 0);
}

} // namespace crypto
} // namespace bigbang
