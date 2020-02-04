// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test_big.h"

BasicUtfSetup::BasicUtfSetup(const std::string& strMsg)
{
    std::cout << strMsg << (" - basic fixture loaded!\n");
}

BasicUtfSetup::~BasicUtfSetup()
{
    std::cout << ("basic fixture unloaded!\n");
}

UtfSetup::UtfSetup(const std::string& strMsg)
  : BasicUtfSetup(strMsg)
{
    std::cout << strMsg << (" - UTF fixture loaded!\n");
}

UtfSetup::~UtfSetup()
{
    std::cout << ("UTF fixture unloaded!\n");
}

UtfBlockchain100Setup::UtfBlockchain100Setup()
  : UtfSetup("UtfBlockchain100Setup")
{
    std::cout << ("UtfBlockchain100Setup - specific fixture loaded!\n");
}

UtfBlockchain100Setup::~UtfBlockchain100Setup()
{
    std::cout << ("UtfBlockchain100Setup - specific fixture unloaded!\n");
}
