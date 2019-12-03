// Copyright (c) 2019 The Bigbang developers
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
    return static_cast<T*>(CryptoAlloc(16 * 1024));
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

// Sign & verify
struct CCryptoKey
{
    uint256 secret;
    uint256 pubkey;
};

uint256 CryptoMakeNewKey(CCryptoKey& key);
uint256 CryptoImportKey(CCryptoKey& key, const uint256& secret);
void CryptoSign(CCryptoKey& key, const void* md, std::size_t len, std::vector<uint8>& vchSig);
bool CryptoVerify(const uint256& pubkey, const void* md, std::size_t len, const std::vector<uint8>& vchSig);

// assume 1 <= i <= j <= n
// vchSig = (index,R,S)
//   index is a bitmap of participation keys in order.
//   R = Ri + ... + Rj = (ri + ... + rj) * B
//   S = Si + ... + Sj
//     Si = ri + H(X,apk,M) * hi * si
//       X is a fixed value in this signature by signers and verifiers
//       M is message
//       ri = H(H(si,pi),M)
//       apk = H(A1,A1,...,An)*A1 + ... + H(An,A1,...,An)*An
//       hi = H(Ai,A1,...,An)
//       si is hash of serect of Ai
// SB = R + H(X,apk,M) * (hi*Ai + ... + hj*Aj)
bool CryptoMultiSign(const std::set<uint256>& setPubKey, const CCryptoKey& privkey, const uint8* pX, const size_t lenX,
                     const uint8* pM, const size_t lenM, std::vector<uint8>& vchSig);
bool CryptoMultiVerify(const std::set<uint256>& setPubKey, const uint8* pX, const size_t lenX,
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

} // namespace crypto
} // namespace bigbang

#endif // CRYPTO_CRYPTO_H
