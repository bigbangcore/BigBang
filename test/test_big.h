// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TEST_BIG_COREWALLET_H
#define TEST_BIG_COREWALLET_H

#include <iostream>
#include <string>

struct BasicUtfSetup
{
    explicit BasicUtfSetup(const std::string& strMsg = "BasicUtfSetup");
    ~BasicUtfSetup();
};

struct UtfSetup : public BasicUtfSetup
{
    explicit UtfSetup(const std::string& strMsg = "UtfSetup");
    ~UtfSetup();
};

struct UtfWorldLine100Setup : public UtfSetup
{
    UtfWorldLine100Setup();
    ~UtfWorldLine100Setup();
};

void Shutdown();

#endif
