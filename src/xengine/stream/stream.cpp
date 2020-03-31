// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "stream.h"

namespace xengine
{

///////////////////////////////
// CStream

CStream& CStream::Serialize(std::string& t, ObjectType&, SaveType&)
{
    *this << CVarInt(t.size());
    return Write((const char*)&t[0], t.size());
}

CStream& CStream::Serialize(std::string& t, ObjectType&, LoadType&)
{
    CVarInt var;
    *this >> var;
    t.resize(var.nValue);
    return Read((char*)&t[0], var.nValue);
}

CStream& CStream::Serialize(std::string& t, ObjectType&, std::size_t& serSize)
{
    CVarInt var(t.size());
    serSize += GetSerializeSize(var);
    serSize += t.size();
    return (*this);
}

///////////////////////////////
// CVarInt

void CVarInt::Serialize(CStream& s, SaveType&)
{
    if (nValue < 0xFD)
    {
        s.Write((const char*)&nValue, 1);
    }
    else if (nValue <= 0xFFFF)
    {
        uint64 v = (nValue << 8) | 0xFD;
        s.Write((const char*)&v, 1 + 2);
    }
    else if (nValue <= 0xFFFFFFFF)
    {
        uint64 v = (nValue << 8) | 0xFE;
        s.Write((const char*)&v, 1 + 4);
    }
    else
    {
        uint8 chSize = 0xFF;
        s.Write((const char*)&chSize, 1);
        s.Write((const char*)&nValue, 8);
    }
}

void CVarInt::Serialize(CStream& s, LoadType&)
{
    nValue = 0;
    s.Read((char*)&nValue, 1);

    if (nValue >= 0xFD)
    {
        s.Read((char*)&nValue, (2 << (nValue - 0xFD)));
    }
}

void CVarInt::Serialize(CStream& s, std::size_t& serSize)
{
    if (nValue < 0xFD)
    {
        serSize += 1;
    }
    else if (nValue <= 0xFFFF)
    {
        serSize += 1 + 2;
    }
    else if (nValue <= 0xFFFFFFFF)
    {
        serSize += 1 + 4;
    }
    else
    {
        serSize += 1 + 8;
    }
}

///////////////////////////////
// CBinary

void CBinary::Serialize(CStream& s, SaveType&)
{
    s.Write(pBuffer, nLength);
}

void CBinary::Serialize(CStream& s, LoadType&)
{
    s.Read(pBuffer, nLength);
}

void CBinary::Serialize(CStream& s, std::size_t& serSize)
{
    serSize += nLength;
}

} // namespace xengine
