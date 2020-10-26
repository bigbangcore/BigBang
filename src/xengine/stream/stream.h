// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_STREAM_STREAM_H
#define XENGINE_STREAM_STREAM_H

#include <boost/asio.hpp>
#include <boost/type_traits.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>

#include "../type.h"
#include "stream/circular.h"

namespace xengine
{

enum
{
    STREAM_SAVE,
    STREAM_LOAD
};

typedef const boost::true_type BasicType;
typedef const boost::false_type ObjectType;

typedef const boost::integral_constant<int, STREAM_SAVE> SaveType;
typedef const boost::integral_constant<int, STREAM_LOAD> LoadType;

// Base class of serializable stream
class CStream
{
public:
    CStream(std::streambuf* sb)
      : ios(sb) {}

    virtual std::size_t GetSize()
    {
        return 0;
    }

    std::iostream& GetIOStream()
    {
        return ios;
    }

    CStream& Write(const char* s, std::size_t n)
    {
        ios.write(s, n);
        return (*this);
    }

    CStream& Read(char* s, std::size_t n)
    {
        ios.read(s, n);
        if (ios.gcount() != n)
        {
            throw std::runtime_error((std::string("stream read error. To be reading ") + std::to_string(n) + " but " + std::to_string(ios.gcount())).c_str());
        }
        return (*this);
    }

    template <typename T, typename O>
    CStream& Serialize(T& t, O& opt)
    {
        return Serialize(t, boost::is_fundamental<T>(), opt);
    }

    template <typename T>
    CStream& operator<<(const T& t)
    {
        return Serialize(const_cast<T&>(t), boost::is_fundamental<T>(), SaveType());
    }

    template <typename T>
    CStream& operator>>(T& t)
    {
        return Serialize(t, boost::is_fundamental<T>(), LoadType());
    }

    template <typename T>
    std::size_t GetSerializeSize(const T& t)
    {
        std::size_t serSize = 0;
        Serialize(const_cast<T&>(t), boost::is_fundamental<T>(), serSize);
        return serSize;
    }

protected:
    template <typename T>
    CStream& Serialize(const T& t, BasicType&, SaveType&)
    {
        return Write((const char*)&t, sizeof(t));
    }

    template <typename T>
    CStream& Serialize(T& t, BasicType&, LoadType&)
    {
        return Read((char*)&t, sizeof(t));
    }

    template <typename T>
    CStream& Serialize(const T& t, BasicType&, std::size_t& serSize)
    {
        serSize += sizeof(t);
        return (*this);
    }

    template <typename T, typename O>
    CStream& Serialize(T& t, ObjectType&, O& opt)
    {
        t.Serialize(*this, opt);
        return (*this);
    }

    /* std::string */
    CStream& Serialize(std::string& t, ObjectType&, SaveType&);
    CStream& Serialize(std::string& t, ObjectType&, LoadType&);
    CStream& Serialize(std::string& t, ObjectType&, std::size_t& serSize);

    /* std::vector */
    template <typename T, typename A>
    CStream& Serialize(const std::vector<T, A>& t, ObjectType&, SaveType&);
    template <typename T, typename A>
    CStream& Serialize(std::vector<T, A>& t, ObjectType&, SaveType&);
    template <typename T, typename A>
    CStream& Serialize(std::vector<T, A>& t, ObjectType&, LoadType&);
    template <typename T, typename A>
    CStream& Serialize(const std::vector<T, A>& t, ObjectType&, std::size_t& serSize);
    template <typename T, typename A>
    CStream& Serialize(std::vector<T, A>& t, ObjectType&, std::size_t& serSize);

    /* std::map */
    template <typename K, typename V, typename C, typename A>
    CStream& Serialize(const std::map<K, V, C, A>& t, ObjectType&, SaveType&);
    template <typename K, typename V, typename C, typename A>
    CStream& Serialize(std::map<K, V, C, A>& t, ObjectType&, SaveType&);
    template <typename K, typename V, typename C, typename A>
    CStream& Serialize(std::map<K, V, C, A>& t, ObjectType&, LoadType&);
    template <typename K, typename V, typename C, typename A>
    CStream& Serialize(const std::map<K, V, C, A>& t, ObjectType&, std::size_t& serSize);
    template <typename K, typename V, typename C, typename A>
    CStream& Serialize(std::map<K, V, C, A>& t, ObjectType&, std::size_t& serSize);

    /* std::multimap */
    template <typename K, typename V, typename C, typename A>
    CStream& Serialize(const std::multimap<K, V, C, A>& t, ObjectType&, SaveType&);
    template <typename K, typename V, typename C, typename A>
    CStream& Serialize(std::multimap<K, V, C, A>& t, ObjectType&, SaveType&);
    template <typename K, typename V, typename C, typename A>
    CStream& Serialize(std::multimap<K, V, C, A>& t, ObjectType&, LoadType&);
    template <typename K, typename V, typename C, typename A>
    CStream& Serialize(const std::multimap<K, V, C, A>& t, ObjectType&, std::size_t& serSize);
    template <typename K, typename V, typename C, typename A>
    CStream& Serialize(std::multimap<K, V, C, A>& t, ObjectType&, std::size_t& serSize);

    /* std::pair */
    template <typename P1, typename P2, typename O>
    CStream& Serialize(std::pair<P1, P2>& t, ObjectType&, O& o);

protected:
    std::iostream ios;
};

// Autosize buffer stream
class CBufStream : public boost::asio::streambuf, public CStream
{
public:
    CBufStream()
      : CStream(this) {}

    void Clear()
    {
        ios.clear();
        consume(size());
    }

    char* GetData() const
    {
        return gptr();
    }

    std::size_t GetSize()
    {
        return size();
    }

    void HexToString(std::string& strHex)
    {
        const char hexc[17] = "0123456789abcdef";
        char strByte[4] = "00 ";
        strHex.clear();
        strHex.reserve(size() * 3);
        char* p = gptr();
        while (p != pptr())
        {
            int c = (int)((unsigned char*)p++)[0];
            strByte[0] = hexc[c >> 4];
            strByte[1] = hexc[c & 15];
            strHex.append(strByte);
        }
    }

    void Dump()
    {
        std::cout << "CStream Dump : " << size() << std::endl
                  << std::hex;
        char* p = gptr();
        int n = 0;
        std::cout.setf(std::ios_base::hex);
        while (p != pptr())
        {
            std::cout << std::setfill('0') << std::setw(2) << (int)((unsigned char*)p++)[0] << " ";
            if (++n == 32)
            {
                std::cout << std::endl;
                n = 0;
            }
        }
        std::cout.unsetf(std::ios::hex);
        if (n != 0)
        {
            std::cout << std::endl;
        }
    }

    friend CStream& operator<<(CStream& s, CBufStream& ssAppend)
    {
        return s.Write(ssAppend.gptr(), ssAppend.GetSize());
    }

    friend CStream& operator>>(CStream& s, CBufStream& ssSink)
    {
        std::size_t len = s.GetSize();
        ssSink.prepare(len);
        s.Read(ssSink.pptr(), len);
        ssSink.commit(len);
        return s;
    }
};

// Circular buffer stream
class CCircularStream : public circularbuf, public CStream
{
public:
    CCircularStream(std::size_t nMaxSize)
      : circularbuf(nMaxSize), CStream(this) {}

    void Clear()
    {
        ios.clear();
        consume(size());
    }

    std::size_t GetSize()
    {
        return size();
    }

    std::size_t GetBufFreeSpace() const
    {
        return freespace();
    }

    std::size_t GetWritePos() const
    {
        return putpos();
    }

    bool Seek(std::size_t nPos)
    {
        ios.clear();
        return (seekpos(nPos) >= 0);
    }

    void Rewind()
    {
        ios.clear();
        seekoff(0, std::ios_base::beg);
    }

    void Consume(std::size_t nSize)
    {
        consume(nSize);
    }

    void Dump()
    {
        std::cout << "CStream Dump : " << size() << std::endl
                  << std::hex;
        seekoff(0, std::ios_base::beg);

        int n = 0;
        std::cout.setf(std::ios_base::hex);
        while (in_avail() != 0)
        {
            int c = sbumpc();

            std::cout << std::setfill('0') << std::setw(2) << c << " ";
            if (++n == 32)
            {
                std::cout << std::endl;
                n = 0;
            }
        }
        std::cout.unsetf(std::ios::hex);
        if (n != 0)
        {
            std::cout << std::endl;
        }
    }
};

// File stream with compatible serialization mothed
class CFileStream : public std::filebuf, public CStream
{

public:
    CFileStream(const char* filename)
      : CStream(this)
    {
        open(filename, std::ios_base::in | std::ios_base::out
                           | std::ios_base::binary);
    }
    ~CFileStream()
    {
        close();
    }

    bool IsValid() const
    {
        return is_open();
    }

    bool IsEOF() const
    {
        return ios.eof();
    }

    std::size_t GetSize()
    {
        std::streampos cur = seekoff(0, std::ios_base::cur);
        std::size_t size = (std::size_t)(seekoff(0, std::ios_base::end) - cur);
        seekpos(cur);
        return size;
    }

    std::size_t GetCurPos()
    {
        return (std::size_t)seekoff(0, std::ios_base::cur);
    }

    void Seek(std::size_t pos)
    {
        seekpos((std::streampos)pos);
    }

    void SeekToBegin()
    {
        seekoff(0, std::ios_base::beg);
    }

    void SeekToEnd()
    {
        seekoff(0, std::ios_base::end);
    }

    void Sync()
    {
        sync();
    }
};

// R/W compact size
//  size <  253        -- 1 byte
//  size <= USHRT_MAX  -- 3 bytes  (253 + 2 bytes)
//  size <= UINT_MAX   -- 5 bytes  (254 + 4 bytes)
//  size >  UINT_MAX   -- 9 bytes  (255 + 8 bytes)
class CVarInt
{
    friend class CStream;

public:
    CVarInt()
      : nValue(0) {}
    CVarInt(uint64 ValueIn)
      : nValue(ValueIn) {}

protected:
    void Serialize(CStream& s, SaveType&);
    void Serialize(CStream& s, LoadType&);
    void Serialize(CStream& s, std::size_t& serSize);

public:
    uint64 nValue;
};

// Binary memory
class CBinary
{
    friend class CStream;

public:
    CBinary(char* pBufferIn, std::size_t nLengthIn)
      : pBuffer(pBufferIn), nLength(nLengthIn) {}
    friend bool operator==(const CBinary& a, const CBinary& b)
    {
        return (a.nLength == b.nLength
                && (a.nLength == 0 || std::memcmp(a.pBuffer, b.pBuffer, a.nLength) == 0));
    }
    friend bool operator!=(const CBinary& a, const CBinary& b)
    {
        return (!(a == b));
    }

protected:
    void Serialize(CStream& s, SaveType&);
    void Serialize(CStream& s, LoadType&);
    void Serialize(CStream& s, std::size_t& serSize);

protected:
    char* pBuffer;
    std::size_t nLength;
};

/* CStream vector serialize impl */
template <typename T, typename A>
inline CStream& CStream::Serialize(const std::vector<T, A>& t, ObjectType&, SaveType&)
{
    *this << CVarInt(t.size());
    if (t.size() > 0)
    {
        if (boost::is_fundamental<T>::value)
        {
            Write((char*)&t[0], sizeof(T) * t.size());
        }
        else
        {
            for (uint64 i = 0; i < t.size(); i++)
            {
                *this << t[i];
            }
        }
    }
    return (*this);
}

template <typename T, typename A>
inline CStream& CStream::Serialize(std::vector<T, A>& t, ObjectType&, SaveType&)
{
    *this << CVarInt(t.size());
    if (t.size() > 0)
    {
        if (boost::is_fundamental<T>::value)
        {
            Write((char*)&t[0], sizeof(T) * t.size());
        }
        else
        {
            for (uint64 i = 0; i < t.size(); i++)
            {
                *this << t[i];
            }
        }
    }
    return (*this);
}

template <typename T, typename A>
inline CStream& CStream::Serialize(std::vector<T, A>& t, ObjectType&, LoadType&)
{
    t.clear();

    CVarInt var;
    *this >> var;

    // prevent huge & bad length
    if (var.nValue > 0)
    {
        if (boost::is_fundamental<T>::value)
        {
            size_t length = sizeof(T) * var.nValue;
            if (GetSize() < length)
            {
                throw std::runtime_error((std::string("stream read size error. left size: ") + std::to_string(GetSize()) + " < read size: " + std::to_string(length)).c_str());
            }

            t.resize(var.nValue);
            Read((char*)&t[0], length);
        }
        else
        {
            std::list<T> l;
            for (uint64 i = 0; i < var.nValue; i++)
            {
                T data;
                *this >> data;
                l.push_back(std::move(data));
            }

            t.reserve(var.nValue);
            t.insert(t.end(), std::make_move_iterator(l.begin()), std::make_move_iterator(l.end()));
        }
    }

    return (*this);
}

template <typename T, typename A>
inline CStream& CStream::Serialize(const std::vector<T, A>& t, ObjectType&, std::size_t& serSize)
{
    CVarInt var(t.size());
    serSize += GetSerializeSize(var);
    if (t.size() > 0)
    {
        if (boost::is_fundamental<T>::value)
        {
            serSize += sizeof(T) * t.size();
        }
        else
        {
            for (uint64 i = 0; i < t.size(); i++)
            {
                serSize += GetSerializeSize(t[i]);
            }
        }
    }
    return (*this);
}

template <typename T, typename A>
inline CStream& CStream::Serialize(std::vector<T, A>& t, ObjectType&, std::size_t& serSize)
{
    CVarInt var(t.size());
    serSize += GetSerializeSize(var);
    if (t.size() > 0)
    {
        if (boost::is_fundamental<T>::value)
        {
            serSize += sizeof(T) * t.size();
        }
        else
        {
            for (uint64 i = 0; i < t.size(); i++)
            {
                serSize += GetSerializeSize(t[i]);
            }
        }
    }
    return (*this);
}

/* CStream map serialize impl */
template <typename K, typename V, typename C, typename A>
inline CStream& CStream::Serialize(const std::map<K, V, C, A>& t, ObjectType&, SaveType&)
{
    *this << CVarInt(t.size());
    for (typename std::map<K, V, C, A>::const_iterator it = t.begin(); it != t.end(); ++it)
    {
        *this << (*it);
    }
    return (*this);
}

template <typename K, typename V, typename C, typename A>
inline CStream& CStream::Serialize(std::map<K, V, C, A>& t, ObjectType&, SaveType&)
{
    *this << CVarInt(t.size());
    for (typename std::map<K, V, C, A>::iterator it = t.begin(); it != t.end(); ++it)
    {
        *this << (*it);
    }
    return (*this);
}

template <typename K, typename V, typename C, typename A>
inline CStream& CStream::Serialize(std::map<K, V, C, A>& t, ObjectType&, LoadType&)
{
    CVarInt var;
    *this >> var;
    for (uint64 i = 0; i < var.nValue; i++)
    {
        std::pair<K, V> item;
        *this >> item;
        t.insert(item);
    }
    return (*this);
}

template <typename K, typename V, typename C, typename A>
inline CStream& CStream::Serialize(const std::map<K, V, C, A>& t, ObjectType&, std::size_t& serSize)
{
    CVarInt var(t.size());
    serSize += GetSerializeSize(var);
    for (typename std::map<K, V, C, A>::const_iterator it = t.begin(); it != t.end(); ++it)
    {
        serSize += GetSerializeSize(*it);
    }
    return (*this);
}

template <typename K, typename V, typename C, typename A>
inline CStream& CStream::Serialize(std::map<K, V, C, A>& t, ObjectType&, std::size_t& serSize)
{
    CVarInt var(t.size());
    serSize += GetSerializeSize(var);
    for (typename std::map<K, V, C, A>::iterator it = t.begin(); it != t.end(); ++it)
    {
        serSize += GetSerializeSize(*it);
    }
    return (*this);
}

/* CStream multimap serialize impl */
template <typename K, typename V, typename C, typename A>
inline CStream& CStream::Serialize(const std::multimap<K, V, C, A>& t, ObjectType&, SaveType&)
{
    *this << CVarInt(t.size());
    for (typename std::multimap<K, V, C, A>::const_iterator it = t.begin(); it != t.end(); ++it)
    {
        *this << (*it);
    }
    return (*this);
}

template <typename K, typename V, typename C, typename A>
inline CStream& CStream::Serialize(std::multimap<K, V, C, A>& t, ObjectType&, SaveType&)
{
    *this << CVarInt(t.size());
    for (typename std::multimap<K, V, C, A>::iterator it = t.begin(); it != t.end(); ++it)
    {
        *this << (*it);
    }
    return (*this);
}

template <typename K, typename V, typename C, typename A>
inline CStream& CStream::Serialize(std::multimap<K, V, C, A>& t, ObjectType&, LoadType&)
{
    CVarInt var;
    *this >> var;
    for (uint64 i = 0; i < var.nValue; i++)
    {
        std::pair<K, V> item;
        *this >> item;
        t.insert(item);
    }
    return (*this);
}

template <typename K, typename V, typename C, typename A>
inline CStream& CStream::Serialize(const std::multimap<K, V, C, A>& t, ObjectType&, std::size_t& serSize)
{
    CVarInt var(t.size());
    serSize += GetSerializeSize(var);
    for (typename std::multimap<K, V, C, A>::const_iterator it = t.begin(); it != t.end(); ++it)
    {
        serSize += GetSerializeSize(*it);
    }
    return (*this);
}

template <typename K, typename V, typename C, typename A>
inline CStream& CStream::Serialize(std::multimap<K, V, C, A>& t, ObjectType&, std::size_t& serSize)
{
    CVarInt var(t.size());
    serSize += GetSerializeSize(var);
    for (typename std::multimap<K, V, C, A>::iterator it = t.begin(); it != t.end(); ++it)
    {
        serSize += GetSerializeSize(*it);
    }
    return (*this);
}

/* CStream pair serialize impl */
template <typename P1, typename P2, typename O>
inline CStream& CStream::Serialize(std::pair<P1, P2>& t, ObjectType&, O& o)
{
    return Serialize(t.first, o).Serialize(t.second, o);
}

template <typename T>
std::size_t GetSerializeSize(const T& obj)
{
    CBufStream ss;
    return ss.GetSerializeSize(obj);
}

} // namespace xengine

#endif //XENGINE_STREAM_STREAM_H
