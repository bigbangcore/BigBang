// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef JSONRPC_RPC_RPC_TYPE_H
#define JSONRPC_RPC_RPC_TYPE_H

#include <iostream>
#include <string>

#include "type.h"

namespace bigbang
{
namespace rpc
{
class CRPCType
{
public:
    bool IsValid() const
    {
        return fValid;
    }

public:
    CRPCType() = default;
    CRPCType(const CRPCType& o) = default;
    CRPCType(CRPCType&& o) = default;
    CRPCType& operator=(const CRPCType& o) = default;
    CRPCType& operator=(CRPCType&& o) = default;

    explicit CRPCType(bool b)
      : fValid(b) {}

public:
    inline bool operator==(const CRPCType& o)
    {
        return fValid == o.IsValid();
    }

protected:
    bool fValid = false;
};

// int64
class CRPCInt64 : public CRPCType
{
    friend std::ostream& operator<<(std::ostream& os, const CRPCInt64& obj);
    friend std::istream& operator>>(std::istream& is, CRPCInt64& obj);

public:
    CRPCInt64() = default;
    CRPCInt64(const CRPCInt64&) = default;
    CRPCInt64(CRPCInt64&& o) = default;
    CRPCInt64& operator=(const CRPCInt64& o) = default;
    CRPCInt64& operator=(CRPCInt64&& o) = default;

public:
    CRPCInt64(const CRPCType& o)
      : CRPCType(o.IsValid()) {}
    CRPCInt64(const int64 i)
      : i(i), CRPCType(true) {}

public:
    inline operator int64() const
    {
        return i;
    }
    inline CRPCInt64& operator=(const CRPCType&& o)
    {
        fValid = o.IsValid();
        return *this;
    }

protected:
    int64 i = 0;
};
inline std::ostream& operator<<(std::ostream& os, const CRPCInt64& obj)
{
    os << obj.i;
    return os;
}
inline std::istream& operator>>(std::istream& is, CRPCInt64& obj)
{
    is >> obj.i;
    obj.fValid = true;
    return is;
}

// uint64
class CRPCUint64 : public CRPCType
{
    friend std::ostream& operator<<(std::ostream& os, const CRPCUint64& obj);
    friend std::istream& operator>>(std::istream& is, CRPCUint64& obj);

public:
    CRPCUint64() = default;
    CRPCUint64(const CRPCUint64&) = default;
    CRPCUint64(CRPCUint64&& o) = default;
    CRPCUint64& operator=(const CRPCUint64& o) = default;
    CRPCUint64& operator=(CRPCUint64&& o) = default;

public:
    CRPCUint64(const CRPCType& o)
      : CRPCType(o.IsValid()) {}
    CRPCUint64(uint64 u)
      : u(u), CRPCType(true) {}

public:
    inline operator uint64() const
    {
        return u;
    }
    CRPCUint64& operator=(const CRPCType&& o)
    {
        fValid = o.IsValid();
        return *this;
    }

protected:
    uint64 u = 0;
};
inline std::ostream& operator<<(std::ostream& os, const CRPCUint64& obj)
{
    os << obj.u;
    return os;
}
inline std::istream& operator>>(std::istream& is, CRPCUint64& obj)
{
    is >> obj.u;
    obj.fValid = true;
    return is;
}

// double
class CRPCDouble : public CRPCType
{
    friend std::ostream& operator<<(std::ostream& os, const CRPCDouble& obj);
    friend std::istream& operator>>(std::istream& is, CRPCDouble& obj);

public:
    CRPCDouble() = default;
    CRPCDouble(const CRPCDouble&) = default;
    CRPCDouble(CRPCDouble&& o) = default;
    CRPCDouble& operator=(const CRPCDouble& o) = default;
    CRPCDouble& operator=(CRPCDouble&& o) = default;

public:
    CRPCDouble(const CRPCType& o)
      : CRPCType(o.IsValid()) {}
    CRPCDouble(double d)
      : d(d), CRPCType(true) {}

public:
    inline operator double() const
    {
        return d;
    }
    CRPCDouble& operator=(const CRPCType&& o)
    {
        fValid = o.IsValid();
        return *this;
    }

protected:
    double d = 0;
};
inline std::ostream& operator<<(std::ostream& os, const CRPCDouble& obj)
{
    os << obj.d;
    return os;
}
inline std::istream& operator>>(std::istream& is, CRPCDouble& obj)
{
    is >> obj.d;
    obj.fValid = true;
    return is;
}

// bool
class CRPCBool : public CRPCType
{
    friend std::ostream& operator<<(std::ostream& os, const CRPCBool& obj);
    friend std::istream& operator>>(std::istream& is, CRPCBool& obj);

public:
    CRPCBool() = default;
    CRPCBool(const CRPCBool&) = default;
    CRPCBool(CRPCBool&& o) = default;
    CRPCBool& operator=(const CRPCBool& o) = default;
    CRPCBool& operator=(CRPCBool&& o) = default;

public:
    CRPCBool(const CRPCType& o)
      : CRPCType(o.IsValid()) {}
    CRPCBool(bool b)
      : b(b), CRPCType(true) {}

public:
    inline operator bool() const
    {
        return b;
    }
    CRPCBool& operator=(const CRPCType&& o)
    {
        fValid = o.IsValid();
        return *this;
    }

protected:
    bool b = false;
};
inline std::ostream& operator<<(std::ostream& os, const CRPCBool& obj)
{
    os << obj.b;
    return os;
}
inline std::istream& operator>>(std::istream& is, CRPCBool& obj)
{
    is >> obj.b;
    obj.fValid = true;
    return is;
}

// string
class CRPCString : public CRPCType
{
    friend std::ostream& operator<<(std::ostream& os, const CRPCString& obj);
    friend std::istream& operator>>(std::istream& is, CRPCString& obj);

public:
    CRPCString() = default;
    CRPCString(const CRPCString&) = default;
    CRPCString(CRPCString&& o) = default;
    CRPCString& operator=(const CRPCString& o) = default;
    CRPCString& operator=(CRPCString&& o) = default;

public:
    CRPCString(const CRPCType& o)
      : CRPCType(o.IsValid()) {}
    CRPCString(const std::string& s)
      : s(s), CRPCType(true) {}
    CRPCString(const char* s)
      : s(s), CRPCType(true) {}

public:
    inline operator const std::string&() const
    {
        return s;
    }
    CRPCString& operator=(const CRPCType&& o)
    {
        fValid = o.IsValid();
        return *this;
    }
    CRPCString& operator=(const std::string&& str)
    {
        s = str;
        fValid = true;
        return *this;
    }
    CRPCString& operator=(const char* str)
    {
        s = str;
        fValid = true;
        return *this;
    }
    inline const char* c_str() const
    {
        return s.c_str();
    }
    bool empty() const
    {
        return s.empty();
    }
    std::size_t size() const
    {
        return s.size();
    }

protected:
    std::string s;
};
inline std::ostream& operator<<(std::ostream& os, const CRPCString& obj)
{
    os << obj.s;
    return os;
}
inline std::istream& operator>>(std::istream& is, CRPCString& obj)
{
    is >> obj.s;
    obj.fValid = true;
    return is;
}
inline bool operator==(const char* lhs, const CRPCString& rhs)
{
    return std::string(lhs) == std::string(rhs);
}
inline bool operator==(const CRPCString& lhs, const char* rhs)
{
    return std::string(lhs) == std::string(rhs);
}
inline bool operator==(const CRPCString& lhs, const CRPCString& rhs)
{
    return std::string(lhs) == std::string(rhs);
}
inline bool operator!=(const char* lhs, const CRPCString& rhs)
{
    return !(lhs == rhs);
}
inline bool operator!=(const CRPCString& lhs, const char* rhs)
{
    return !(lhs == rhs);
}
inline bool operator!=(const CRPCString& lhs, const CRPCString& rhs)
{
    return !(lhs == rhs);
}
inline std::string operator+(const char* lhs, const CRPCString& rhs)
{
    return std::string(lhs) + std::string(rhs);
}
inline std::string operator+(const CRPCString& lhs, const char* rhs)
{
    return std::string(lhs) + std::string(rhs);
}
inline std::string operator+(const CRPCString& lhs, const CRPCString& rhs)
{
    return std::string(lhs) + std::string(rhs);
}

// vector
template <typename T>
class CRPCVector : public CRPCType
{
public:
    CRPCVector() = default;
    CRPCVector(const CRPCVector<T>&) = default;
    CRPCVector(CRPCVector<T>&& o) = default;
    CRPCVector& operator=(const CRPCVector<T>& o) = default;
    CRPCVector& operator=(CRPCVector<T>&& o) = default;

public:
    CRPCVector(const CRPCType& o)
      : CRPCType(o) {}
    CRPCVector(const std::vector<T>& o)
      : o(o), CRPCType(true) {}

public:
    typedef T value_type;

    inline operator const std::vector<T>&() const
    {
        return o;
    }
    CRPCVector& operator=(const CRPCType&& o)
    {
        fValid = false;
        return *this;
    }
    CRPCVector& operator=(std::vector<T>&& vec)
    {
        o = vec;
        fValid = true;
        return *this;
    }
    CRPCVector& operator=(const std::vector<T>& vec)
    {
        o = vec;
        fValid = true;
        return *this;
    }
    void push_back(T&& value)
    {
        o.push_back(value);
        fValid = true;
    }
    void push_back(const T& value)
    {
        o.push_back(value);
        fValid = true;
    }

    inline typename std::vector<T>::const_iterator begin() const
    {
        return o.begin();
    }
    inline typename std::vector<T>::const_iterator end() const
    {
        return o.end();
    }
    inline typename std::vector<T>::size_type size() const
    {
        return o.size();
    }

protected:
    std::vector<T> o;
};

const CRPCType RPCNull;
const CRPCType RPCValid(true);

} // namespace rpc

} // namespace bigbang

#endif // JSONRPC_RPC_RPC_TYPE_H
