// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config.h"

#include <boost/algorithm/string/trim.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

#include "util.h"

using namespace std;

namespace po = boost::program_options;
namespace fs = boost::filesystem;

namespace xengine
{

//////////////////////////////
// CConfig

CConfig::CConfig()
  : defaultDesc("Options"), ignoreCmd(0)
{
}

CConfig::~CConfig()
{
}

bool CConfig::Load(int argc, char* argv[], const fs::path& pathDefault, const string& strConfile)
{
    const int defaultCmdStyle = po::command_line_style::allow_long
                                | po::command_line_style::long_allow_adjacent
                                | po::command_line_style::allow_long_disguise;
    fs::path pathConfile;
    string strRoot, strConfig;
    try
    {
        vector<string> vecIgnoreCmd;

        defaultDesc.add_options()("cmd", po::value<vector<string>>(&vecCommand))("help", po::value<bool>(&fHelp)->default_value(false))("daemon", po::value<bool>(&fDaemon)->default_value(false))("debug", po::value<bool>(&fDebug)->default_value(false))("datadir", po::value<string>(&strRoot)->default_value(pathDefault.string()))("conf", po::value<string>(&strConfig)->default_value(strConfile))("ignore", po::value<vector<string>>(&vecIgnoreCmd));

        po::positional_options_description defaultPosDesc;
        defaultPosDesc.add("cmd", -1).add("ignore", ignoreCmd);

        auto parser = po::command_line_parser(argc, argv).options(defaultDesc).style(defaultCmdStyle).extra_parser(CConfig::ExtraParser).positional(defaultPosDesc);
        po::store(parser.run(), vm);

        po::notify(vm);
        pathRoot = strRoot;
        pathConfile = strConfig;
        if (!STD_DEBUG)
            STD_DEBUG = fDebug;

        if (fHelp)
        {
            return true;
        }

        if (!pathConfile.is_complete())
        {
            pathConfile = pathRoot / pathConfile;
        }

        vector<string> confToken;
        if (TokenizeConfile(pathConfile.string().c_str(), confToken))
        {
            po::store(po::command_line_parser(confToken).options(defaultDesc).style(defaultCmdStyle).extra_parser(CConfig::ExtraParser).run(), vm);
            po::notify(vm);
        }
    }
    catch (exception& e)
    {
        cerr << e.what() << endl;
        return false;
    }
    return true;
}

bool CConfig::PostLoad()
{
    pathData = pathRoot;
    return true;
}

string CConfig::ListConfig() const
{
    ostringstream oss;
    oss << "debug : " << (fDebug ? "Y" : "N") << "\n"
        << "data path : " << pathData << "\n";
    return oss.str();
}

string CConfig::Help() const
{
    return string()
           + "  -help                                 Get more information\n"
           + "  -daemon                               Run server in background\n"
           + "  -debug                                Run in debug mode\n"
           + "  -datadir=<path>                       Root directory of resources\n"
           + "  -conf=<file>                          Configuration file name\n";
}

pair<string, string> CConfig::ExtraParser(const string& s)
{
    if (s[0] == '-' && !isdigit(s[1]))
    {
        bool fRev = (s.substr(1, 2) == "no");
        size_t eq = s.find('=');
        if (eq == string::npos)
        {
            if (fRev)
            {
                return make_pair(s.substr(3), string("0"));
            }
            else
            {
                return make_pair(s.substr(1), string("1"));
            }
        }
        else if (fRev)
        {
            int v = atoi(s.substr(eq + 1).c_str());
            return make_pair(s.substr(3, eq - 3), string(v != 0 ? "0" : "1"));
        }
        else
        {
            return make_pair(s.substr(1, eq - 1), s.substr(eq + 1));
        }
    }
    return make_pair(string(), string());
}

void CConfig::AddOptions(boost::program_options::options_description& desc)
{
    defaultDesc.add(desc);
}

bool CConfig::TokenizeConfile(const char* pzConfile, vector<string>& tokens)
{
    ifstream ifs(pzConfile);
    if (!ifs)
    {
        return false;
    }
    string line;
    while (!getline(ifs, line).eof() || !line.empty())
    {
        string s = line.substr(0, line.find('#'));
        boost::trim(s);
        if (!s.empty())
        {
            tokens.push_back(string("-") + s);
        }
        line.clear();
    }

    return true;
}

void CConfig::SetIgnoreCmd(int number)
{
    ignoreCmd = number;
}

vector<string> CConfig::ParseCmd(int argc, char* argv[])
{
    vector<string> vecCmd;

    po::options_description desc;
    desc.add_options()("cmd", po::value<vector<string>>(&vecCmd));

    po::positional_options_description optDesc;
    optDesc.add("cmd", -1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).allow_unregistered().positional(optDesc).run(), vm);
    po::notify(vm);
    return vecCmd;
}

} // namespace xengine
