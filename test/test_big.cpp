// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test_big.h"

#include "entry.h"

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

UtfWorldLine100Setup::UtfWorldLine100Setup()
  : UtfSetup("UtfWorldLine100Setup")
{
    std::cout << ("UtfWorldLine100Setup - specific fixture loaded!\n");
}

UtfWorldLine100Setup::~UtfWorldLine100Setup()
{
    std::cout << ("UtfWorldLine100Setup - specific fixture unloaded!\n");
}

void Shutdown()
{
    bigbang::CBbEntry::GetInstance().Stop();
}
