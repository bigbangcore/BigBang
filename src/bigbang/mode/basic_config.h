// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_MODE_BASIC_CONFIG_H
#define BIGBANG_MODE_BASIC_CONFIG_H

#include <exception>
#include <string>

#include "mode/auto_options.h"
#include "xengine.h"

namespace bigbang
{
/**
 * Dynamic cast xengine::CConfig* to it's derived class.
 * T is a pointer type.
 * If occured error, throw runtime_error.
 */
template <typename T>
typename std::enable_if<std::is_pointer<T>::value, T>::type
CastConfigPtr(xengine::CConfig* ptr)
{
    if (!ptr)
        return nullptr;

    T p = dynamic_cast<T>(ptr);
    if (!p)
    {
        throw std::runtime_error(
            std::string("bad_cast: xengine::CConfig* to ") + xengine::TypeName(typeid(T)));
    }
    return p;
}

/**
 * basic config on business.
 */
class CBasicConfig : virtual public xengine::CConfig, virtual public CBasicConfigOption
{
public:
    CBasicConfig();
    virtual ~CBasicConfig();
    virtual bool PostLoad();
    virtual std::string ListConfig() const;
    virtual std::string Help() const;

public:
    unsigned int nMagicNum;
};

} // namespace bigbang

#endif // BIGBANG_MODE_BASIC_CONFIG_H
