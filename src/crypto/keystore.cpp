// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "keystore.h"

using namespace std;

namespace bigbang
{
namespace crypto
{

//////////////////////////////
// CKeyStore

CKeyStore::CKeyStore()
{
}

CKeyStore::~CKeyStore()
{
}

void CKeyStore::Clear()
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
    mapKey.clear();
}

bool CKeyStore::AddKey(const CKey& key)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
    if (!key.IsNull())
    {
        pair<map<CPubKey, CKey>::iterator, bool> ret;
        ret = mapKey.insert(make_pair(key.GetPubKey(), key));
        if (ret.second)
        {
            (*(ret.first)).second.Lock();
            return true;
        }
    }
    return false;
}

void CKeyStore::RemoveKey(const CPubKey& pubkey)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
    mapKey.erase(pubkey);
}

bool CKeyStore::HaveKey(const CPubKey& pubkey) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    return (mapKey.find(pubkey) != mapKey.end());
}

bool CKeyStore::GetSecret(const CPubKey& pubkey, CCryptoKeyData& vchSecret) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    map<CPubKey, CKey>::const_iterator it = mapKey.find(pubkey);
    if (it != mapKey.end())
    {
        return (*it).second.GetSecret(vchSecret);
    }
    return false;
}

void CKeyStore::GetKeys(set<CPubKey>& setPubKey) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    for (map<CPubKey, CKey>::const_iterator it = mapKey.begin(); it != mapKey.end(); ++it)
    {
        setPubKey.insert((*it).first);
    }
}

bool CKeyStore::EncryptKey(const CPubKey& pubkey, const CCryptoString& strPassphrase,
                           const CCryptoString& strCurrentPassphrase)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
    map<CPubKey, CKey>::iterator it = mapKey.find(pubkey);
    if (it != mapKey.end())
    {
        return (*it).second.Encrypt(strPassphrase, strCurrentPassphrase);
    }
    return false;
}

bool CKeyStore::IsLocked(const CPubKey& pubkey) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    map<CPubKey, CKey>::const_iterator it = mapKey.find(pubkey);
    if (it != mapKey.end())
    {
        return (*it).second.IsLocked();
    }
    return false;
}

bool CKeyStore::LockKey(const CPubKey& pubkey)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
    map<CPubKey, CKey>::iterator it = mapKey.find(pubkey);
    if (it != mapKey.end())
    {
        (*it).second.Lock();
        return true;
    }
    return false;
}

bool CKeyStore::UnlockKey(const CPubKey& pubkey, const CCryptoString& strPassphrase)
{
    boost::unique_lock<boost::shared_mutex> wlock(rwAccess);
    map<CPubKey, CKey>::iterator it = mapKey.find(pubkey);
    if (it != mapKey.end())
    {
        return (*it).second.Unlock(strPassphrase);
    }
    return false;
}

bool CKeyStore::Sign(const CPubKey& pubkey, const uint256& hash, std::vector<uint8>& vchSig) const
{
    boost::shared_lock<boost::shared_mutex> rlock(rwAccess);
    map<CPubKey, CKey>::const_iterator it = mapKey.find(pubkey);
    if (it != mapKey.end())
    {
        return (*it).second.Sign(hash, vchSig);
    }
    return false;
}

} // namespace crypto
} // namespace bigbang
