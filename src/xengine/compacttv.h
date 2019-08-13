// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_COMPACTTV_H
#define XENGINE_COMPACTTV_H

#include <boost/type_traits.hpp>
#include <boost/variant.hpp>
#include <exception>
#include <map>
#include <string>
#include <vector>

namespace xengine
{

class CCompactTagValue
{
public:
    typedef boost::variant<boost::int64_t, std::vector<unsigned char>> CCompactTVType;
    enum
    {
        COMPACT_TYPE_FALSE = (0 << 5),
        COMPACT_TYPE_TRUE = (1 << 5),
        COMPACT_TYPE_INT8 = (2 << 5),
        COMPACT_TYPE_INT16 = (3 << 5),
        COMPACT_TYPE_INT32 = (4 << 5),
        COMPACT_TYPE_INT64 = (5 << 5),
        COMPACT_TYPE_INT256 = (6 << 5),
        COMPACT_TYPE_BINARY = (7 << 5),
    };

    CCompactTagValue() {}
    void Push(int nTag, const std::vector<unsigned char>& vch, const boost::false_type&)
    {
        if (vch.size() > 255)
        {
            throw std::range_error("out of range");
        }
        mapTagValue.insert(std::make_pair(nTag, std::make_pair(COMPACT_TYPE_BINARY, vch)));
    }
    void Push(int nTag, const std::string& str, const boost::false_type&)
    {
        Push(nTag, std::vector<unsigned char>(str.begin(), str.end()));
    }
    void Push(int nTag, bool f, const boost::true_type&)
    {
        int type = (f ? COMPACT_TYPE_TRUE : COMPACT_TYPE_FALSE);
        mapTagValue.insert(std::make_pair(nTag, std::make_pair(type, (boost::int64_t)0)));
    }
    template <typename T>
    void Push(int nTag, const T& t, const boost::false_type&)
    {
        if (sizeof(T) != 32)
        {
            throw std::range_error("unknown type");
        }
        std::vector<unsigned char> vch((const unsigned char*)&t, (const unsigned char*)(&t + 1));
        mapTagValue.insert(std::make_pair(nTag, std::make_pair(COMPACT_TYPE_INT256, vch)));
    }
    template <typename T>
    void Push(int nTag, const T& t, const boost::true_type&)
    {
        int type = 0;
        switch (sizeof(T))
        {
        case 1:
            type = COMPACT_TYPE_INT8;
            break;
        case 2:
            type = COMPACT_TYPE_INT16;
            break;
        case 4:
            type = COMPACT_TYPE_INT32;
            break;
        case 8:
            type = COMPACT_TYPE_INT64;
            break;
        default:
            throw std::range_error("unknown type");
            break;
        }
        mapTagValue.insert(std::make_pair(nTag, std::make_pair(type, (boost::int64_t)t)));
    }
    template <typename T>
    void Push(int nTag, const T& t)
    {
        if (nTag > 31)
        {
            throw std::range_error("invalid tag");
        }
        if (mapTagValue.count(nTag))
        {
            throw std::range_error("duplicated tag");
        }
        Push(nTag, t, boost::is_fundamental<T>());
    }
    bool Get(int nTag, std::vector<unsigned char>& vch)
    {
        std::map<int, std::pair<int, CCompactTVType>>::iterator it = mapTagValue.find(nTag);
        if (it == mapTagValue.end())
        {
            return false;
        }
        if ((*it).second.first != COMPACT_TYPE_BINARY)
        {
            throw std::range_error("type error");
        }
        vch = boost::get<std::vector<unsigned char>>((*it).second.second);
        return true;
    }
    bool Get(int nTag, std::string& str)
    {
        std::map<int, std::pair<int, CCompactTVType>>::iterator it = mapTagValue.find(nTag);
        if (it == mapTagValue.end())
        {
            return false;
        }
        if ((*it).second.first != COMPACT_TYPE_BINARY)
        {
            throw std::range_error("type error");
        }
        std::vector<unsigned char>& vch = boost::get<std::vector<unsigned char>>((*it).second.second);
        str = std::string(vch.begin(), vch.end());
        return true;
    }
    bool Get(int nTag, bool& f)
    {
        std::map<int, std::pair<int, CCompactTVType>>::iterator it = mapTagValue.find(nTag);
        if (it == mapTagValue.end())
        {
            return false;
        }
        if ((*it).second.first != COMPACT_TYPE_TRUE && (*it).second.first != COMPACT_TYPE_FALSE)
        {
            throw std::range_error("type error");
        }
        f = ((*it).second.first == COMPACT_TYPE_TRUE);
        return true;
    }
    template <typename T>
    bool Get(int nTag, T& t)
    {
        std::map<int, std::pair<int, CCompactTVType>>::iterator it = mapTagValue.find(nTag);
        if (it == mapTagValue.end())
        {
            return false;
        }
        int type = (*it).second.first;
        if ((type == COMPACT_TYPE_INT256 && sizeof(T) != 32)
            || (type != COMPACT_TYPE_INT256 && sizeof(T) > 8))
        {
            throw std::range_error("type error");
        }
        if (type == COMPACT_TYPE_INT256)
        {
            std::vector<unsigned char>& vch = boost::get<std::vector<unsigned char>>((*it).second.second);
            t = *(T*)(&vch[0]);
        }
        else
        {
            t = *(T*)&(boost::get<boost::int64_t>((*it).second.second));
        }
        return true;
    }
    void Encode(std::vector<unsigned char>& vchEncode)
    {
        for (std::map<int, std::pair<int, CCompactTVType>>::iterator it = mapTagValue.begin();
             it != mapTagValue.end(); ++it)
        {
            int type = (*it).second.first;
            vchEncode.push_back((unsigned char)((*it).first | type));
            if (type == COMPACT_TYPE_BINARY)
            {
                std::vector<unsigned char>& vch = boost::get<std::vector<unsigned char>>((*it).second.second);
                vchEncode.push_back((unsigned char)vch.size());
                vchEncode.insert(vchEncode.end(), vch.begin(), vch.end());
            }
            else if (type == COMPACT_TYPE_INT256)
            {
                std::vector<unsigned char>& vch = boost::get<std::vector<unsigned char>>((*it).second.second);
                vchEncode.insert(vchEncode.end(), vch.begin(), vch.end());
            }
            else
            {
                boost::int64_t value = boost::get<boost::int64_t>((*it).second.second);
                int len = 0;
                switch (type)
                {
                case COMPACT_TYPE_INT8:
                    len = 1;
                    break;
                case COMPACT_TYPE_INT16:
                    len = 2;
                    break;
                case COMPACT_TYPE_INT32:
                    len = 4;
                    break;
                case COMPACT_TYPE_INT64:
                    len = 8;
                    break;
                default:
                    break;
                }
                vchEncode.insert(vchEncode.end(), (unsigned char*)&value, ((unsigned char*)&value) + len);
            }
        }
    }
    void Decode(const std::vector<unsigned char>& vchEncoded)
    {
        mapTagValue.clear();
        std::size_t offset = 0;
        while (offset != vchEncoded.size())
        {
            unsigned char c = vchEncoded[offset++];
            int tag = (c & 0x1f);
            int type = (c & 0xe0);
            if (type == COMPACT_TYPE_BINARY)
            {
                std::size_t size = vchEncoded[offset++];
                std::vector<unsigned char> vch(&vchEncoded[offset], &vchEncoded[offset + size]);
                if (!mapTagValue.insert(std::make_pair(tag, std::make_pair(COMPACT_TYPE_BINARY, vch))).second)
                {
                    throw std::range_error("duplicated tag");
                }
                offset += size;
            }
            else if (type == COMPACT_TYPE_INT256)
            {
                std::vector<unsigned char> vch(&vchEncoded[offset], &vchEncoded[offset + 32]);
                if (!mapTagValue.insert(std::make_pair(tag, std::make_pair(COMPACT_TYPE_INT256, vch))).second)
                {
                    throw std::range_error("duplicated tag");
                }
                offset += 32;
            }
            else
            {
                boost::int64_t value = 0;
                switch (type)
                {
                case COMPACT_TYPE_INT8:
                    value = *(unsigned char*)(&vchEncoded[offset]);
                    offset += 1;
                    break;
                case COMPACT_TYPE_INT16:
                    value = *(unsigned short*)(&vchEncoded[offset]);
                    offset += 2;
                    break;
                case COMPACT_TYPE_INT32:
                    value = *(unsigned int*)(&vchEncoded[offset]);
                    offset += 4;
                    break;
                case COMPACT_TYPE_INT64:
                    value = *(boost::int64_t*)(&vchEncoded[offset]);
                    offset += 8;
                    break;
                default:
                    break;
                }
                if (!mapTagValue.insert(std::make_pair(tag, std::make_pair(type, (boost::int64_t)value))).second)
                {
                    throw std::range_error("duplicated tag");
                }
            }
        }
    }

protected:
    std::map<int, std::pair<int, CCompactTVType>> mapTagValue;
};

} // namespace xengine

#endif //XENGINE_COMPACTTV_H
