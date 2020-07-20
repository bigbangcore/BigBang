// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_RECOVERY_H
#define BIGBANG_RECOVERY_H

#include "base.h"

namespace bigbang
{

class CRecovery : public IRecovery
{
public:
    CRecovery();
    ~CRecovery();

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;

protected:
    IDispatcher* pDispatcher;
};

} // namespace bigbang
#endif // BIGBANG_RECOVERY_H
