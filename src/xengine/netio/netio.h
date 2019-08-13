// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_NETIO_NETIO_H
#define XENGINE_NETIO_NETIO_H

#include "event/eventproc.h"
namespace xengine
{

class IIOProc : public IBase
{
public:
    IIOProc(const std::string& ownKeyIn)
      : IBase(ownKeyIn) {}
    virtual bool DispatchEvent(CEvent* pEvent) = 0;
};

class IIOModule : public CEventProc
{
public:
    IIOModule(const std::string& ownKeyIn)
      : CEventProc(ownKeyIn) {}
};

} // namespace xengine

#endif //XENGINE_NETIO_NETIO_H
