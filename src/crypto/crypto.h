// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CRYPTO_CRYPTO_H
#define CRYPTO_CRYPTO_H

#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "uint256.h"

namespace bigbang
{
namespace crypto
{

// Runtime except
class CCryptoError : public std::runtime_error
{
public:
    explicit CCryptoError(const std::string& str)
      : std::runtime_error(str)
    {
    }
};

// Secure memory
void* CryptoAlloc(const std::size_t size);
void CryptoFree(void* ptr);

template <typename T>
T* CryptoAlloc()
{
    return static_cast<T*>(CryptoAlloc(sizeof(T)));
}

template <typename T>
class CCryptoAllocator : public std::allocator<T>
{
public:
    CCryptoAllocator() throw() {}
    CCryptoAllocator(const CCryptoAllocator& a) throw()
      : std::allocator<T>(a)
    {
    }
    template <typename U>
    CCryptoAllocator(const CCryptoAllocator<U>& a) throw()
      : std::allocator<T>(a)
    {
    }
    ~CCryptoAllocator() throw() {}
    template <typename _Other>
    struct rebind
    {
        typedef CCryptoAllocator<_Other> other;
    };

    T* allocate(std::size_t n, const void* hint = 0)
    {
        (void)hint;
        return static_cast<T*>(CryptoAlloc(sizeof(T) * n));
    }
    void deallocate(T* p, std::size_t n)
    {
        (void)n;
        CryptoFree(p);
    }
};

typedef std::basic_string<char, std::char_traits<char>, CCryptoAllocator<char>> CCryptoString;
typedef std::vector<unsigned char, CCryptoAllocator<unsigned char>> CCryptoKeyData;

// Heap memory lock
void CryptoMLock(void* const addr, const std::size_t len);
void CryptoMUnlock(void* const addr, const std::size_t len);

// Rand
uint32 CryptoGetRand32();
uint64 CryptoGetRand64();
void CryptoGetRand256(uint256& u);

// Hash
uint256 CryptoHash(const void* msg, std::size_t len);
uint256 CryptoHash(const uint256& h1, const uint256& h2);
uint256 CryptoPowHash(const void* msg, size_t len);

// SHA256
uint256 CryptoSHA256(const void* msg, size_t len);

// Sign & verify
struct CCryptoKey
{
    uint256 secret;
    uint256 pubkey;
};

uint256 CryptoMakeNewKey(CCryptoKey& key);
uint256 CryptoImportKey(CCryptoKey& key, const uint256& secret);
void CryptoSign(const CCryptoKey& key, const void* md, const std::size_t len, std::vector<uint8>& vchSig);
bool CryptoVerify(const uint256& pubkey, const void* md, const std::size_t len, const std::vector<uint8>& vchSig);

// assume:
//   1. 1 <= i <= j <= n
//   2. Pi is the i-th public key
//   3. ki is the i-th private key
//   4. Pi = si * B, si = Clamp(sha512(ki)[0, 32))
//      void Clamp(unsigned char k[32])
//      {
//          k[0] &= 248;
//          k[31] &= 127;
//          k[31] |= 64;
//      }
//   5. the sequence of (P1, ..., Pn) is according to the order of uint256
//
// signature:
//   1. vchSig = (index,Ri,Si...,Rj,Sj)
//   2. index is a bitmap of participation keys in (P1, ..., Pn) order.
//      For example: if n = 10, and cosigner participants are (P1, P3, P10), then index is [00000010,00000101]
//   3. (Ri,Si) is the same as standard ed25519 signature algorithm (reference libsodium crypto_sign_ed25519_detached)
//   4. Ri = ri * B, ri = sha512(sha512(ki)[32,64),M)
//   5. Si = ri + hash(Ri,Pi,M) * si,
//
// proof:
//   each (Ri,Si) proof is the same as standard ed25519 verify signature algorithm (reference libsodium crypto_sign_ed25519_verify_detached)
//   Si = Ri + hash(Ri,Pi,M) * Pi
bool CryptoMultiSign(const std::set<uint256>& setPubKey, const CCryptoKey& privkey, const uint8* pM, const std::size_t lenM, std::vector<uint8>& vchSig);
bool CryptoMultiVerify(const std::set<uint256>& setPubKey, const uint8* pM, const std::size_t lenM, const std::vector<uint8>& vchSig, std::set<uint256>& setPartKey);
// defect old version multi-sign algorithm, only for backward compatiblility.
bool CryptoMultiSignDefect(const std::set<uint256>& setPubKey, const CCryptoKey& privkey, const uint8* pX, const size_t lenX,
                           const uint8* pM, const size_t lenM, std::vector<uint8>& vchSig);
bool CryptoMultiVerifyDefect(const std::set<uint256>& setPubKey, const uint8* pX, const size_t lenX,
                             const uint8* pM, const size_t lenM, const std::vector<uint8>& vchSig, std::set<uint256>& setPartKey);

// Encrypt
struct CCryptoCipher
{
    CCryptoCipher()
      : nonce(0)
    {
        memset(encrypted, 0, 48);
    }

    uint8 encrypted[48];
    uint64 nonce;
};

void CryptoKeyFromPassphrase(int version, const CCryptoString& passphrase, const uint256& salt, CCryptoKeyData& key);
bool CryptoEncryptSecret(int version, const CCryptoString& passphrase, const CCryptoKey& key, CCryptoCipher& cipher);
bool CryptoDecryptSecret(int version, const CCryptoString& passphrase, const CCryptoCipher& cipher, CCryptoKey& key);

// aes encrypt
bool CryptoAesEncrypt(const uint256& secret_local, const uint256& pubkey_remote, const std::vector<uint8>& message, std::vector<uint8>& ciphertext);
bool CryptoAesDecrypt(const uint256& secret_local, const uint256& pubkey_remote, const std::vector<uint8>& ciphertext, std::vector<uint8>& message);

} // namespace crypto
} // namespace bigbang

#endif // CRYPTO_CRYPTO_H
