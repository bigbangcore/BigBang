// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMMON_TEMPLATE_TEMPLATEID_H
#define COMMON_TEMPLATE_TEMPLATEID_H

#include "uint256.h"

/**
 * hash of template data + template type
 */
class CTemplateId : public uint256
{
public:
    CTemplateId();
    CTemplateId(const uint256& data);
    CTemplateId(const uint16 type, const uint256& hash);

    uint16 GetType() const;
    CTemplateId& operator=(uint64 b);
};

#endif //COMMON_TEMPLATE_TEMPLATEID_H
