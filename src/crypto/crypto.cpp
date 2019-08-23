// Copyright (c) 2019 The Bigbang developers
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

void CryptoSign(CCryptoKey& key, const void* md, size_t len, vector<uint8>& vchSig)
{
    vchSig.resize(64);
    crypto_sign_ed25519_detached(&vchSig[0], nullptr, (const uint8*)md, len, (uint8*)&key);
}

bool CryptoVerify(const uint256& pubkey, const void* md, size_t len, const vector<uint8>& vchSig)
{
    return (vchSig.size() == 64
            && !crypto_sign_ed25519_verify_detached(&vchSig[0], (const uint8*)md, len, (uint8*)&pubkey));
}

//     0        key1          keyn
// |________|________| ... |_______|
static vector<uint8> MultiSignPreApk(const set<uint256>& setPubKey)
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
static bool MultiSignApk(const set<uint256>& setPubKey, const set<uint256>& setPartKey, uint8* md32)
{
    vector<uint8> vecHash = MultiSignPreApk(setPubKey);

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
static CSC25519 MultiSignHash(const uint8* pX, const size_t lenX, const uint8* pApk, const size_t lenApk, const uint8* pM, const size_t lenM)
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
static CSC25519 MultiSignR(const CCryptoKey& key, const uint8* pM, const size_t lenM)
{
    uint8 hash[32];

    crypto_generichash_blake2b_state state;
    crypto_generichash_blake2b_init(&state, nullptr, 0, 32);
    uint256 keyHash = CryptoHash((uint8*)&key, 64);
    crypto_generichash_blake2b_update(&state, (uint8*)&keyHash, 32);
    crypto_generichash_blake2b_update(&state, pM, lenM);
    crypto_generichash_blake2b_final(&state, hash, 32);

    return CSC25519(hash);
}

static CSC25519 ClampPrivKey(const uint256& privkey)
{
    uint8 hash[64];
    crypto_hash_sha512(hash, privkey.begin(), privkey.size());
    hash[0] &= 248;
    hash[31] &= 127;
    hash[31] |= 64;

    return CSC25519(hash);
}

bool CryptoMultiSign(const set<uint256>& setPubKey, const CCryptoKey& privkey, const uint8* pX, const size_t lenX,
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
    if (!MultiSignApk(setPubKey, setPubKey, apk.begin()))
    {
        return false;
    }
    // H(X,apk,M)
    CSC25519 hash = MultiSignHash(pX, lenX, apk.begin(), apk.size(), pM, lenM);

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
        CSC25519 ri = MultiSignR(privkey, pM, lenM);
        // hi = H(Ai,A1,...,An)
        vector<uint8> vecHash = MultiSignPreApk(setPubKey);
        memcpy(&vecHash[0], itPub->begin(), itPub->size());
        CSC25519 hi = CSC25519(CryptoHash(&vecHash[0], vecHash.size()).begin());
        // si = H(privkey)
        CSC25519 si = ClampPrivKey(privkey.secret);
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

bool CryptoMultiVerify(const set<uint256>& setPubKey, const uint8* pX, const size_t lenX,
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
    if (!MultiSignApk(setPubKey, setPubKey, apk.begin()))
    {
        return false;
    }
    // H(X,apk,M)
    CSC25519 hash = MultiSignHash(pX, lenX, apk.begin(), apk.size(), pM, lenM);

    // A = hi*Ai + ... + aj*Aj
    CEdwards25519 A;
    vector<uint8> vecHash = MultiSignPreApk(setPubKey);
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

set<uint256> CryptoMultiPartKey(const set<uint256>& setPubKey, const vector<uint8>& vchSig)
{
    set<uint256> setPartKey;
    if (setPubKey.empty() || vchSig.empty())
    {
        return setPartKey;
    }

    int nIndexLen = (setPubKey.size() - 1) / 8 + 1;
    if (vchSig.size() != (nIndexLen + 64))
    {
        return setPartKey;
    }

    const uint8* pIndex = &vchSig[0];
    int i = 0;
    for (auto itPub = setPubKey.begin(); itPub != setPubKey.end(); ++itPub, ++i)
    {
        if (pIndex[i / 8] & (1 << (i % 8)))
        {
            setPartKey.insert(*itPub);
        }
    }

    return setPartKey;
}

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
