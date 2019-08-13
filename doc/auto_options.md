# command line options generator

## scope
The options configuration of command line. e.g. *-rpcport*, *-dbhost* ...

## files
- **script/options.json**: configuration file.
- **srcipt/options_generator.py**: generator script.
- **src/mode/auto_options.h**: target cpp file.

## usage
1. configure **script/options.json**
2. run **srcipt/options_generator.py** or **srcipt/generator.py**(all generator entry)
3. check **src/mode/auto_options.h**.
```c++
// class CNetworkConfig in src/mode/network_config.h
class CNetworkConfig : virtual public CBasicConfig,
                         virtual public CNetworkConfigOption

// class CNetworkConfigOption in src/mode/auto_options.h
class CNetworkConfigOption
```
4. build project

### **script/options.json**
```json
// one option class
"CRPCServerConfigOption": [   // Class name, content is array
    // first option
    {
        "access": "public",             // (optional, default="public"). The access modifier of c++ class
        "name": "nRPCMaxConnections",   // (required) parameter name
        "type": "unsigned int",         // (required) parameter type
        "opt": "rpcmaxconnections",     // (required) option of command-line
        "default": "DEFAULT_RPC_MAX_CONNECTIONS",           // (optional) default value of parameter
        "format": "-rpcmaxconnections=<num>",               // (required) prefix formatting in help
        "desc": "Set max connections to <num> (default: 5)" // (optional) description in help
    },
    // second option
    {
        "name": "vRPCAllowIP",
        "type": "vector<string>",
        "opt": "rpcallowip",
        "format": "-rpcallowip=<ip>",
        "desc": "Allow JSON-RPC connections from specified <ip> address"
    }
],
// another option class
CRPCBasicConfigOption": [           // Class name, content is array
  {
      "access": "protected",          // (optional, default="public"). The access modifier of c++ class
      "name": "nRPCPortInt",          // (required) parameter name
      "type": "int",                  // (required) parameter type
      "opt": "rpcport",               // (required) option of command-line
      "default": 0,                   // (optional) default value of parameter
      "format": "-rpcport=port",      // (required) prefix formatting in help
      "desc": "Listen for JSON-RPC"   // (optional) description in help
  },
  ...     // next option
]
```

### **src/mode/auto_options.h**
```cpp
class CRPCServerConfigOption
{
public:
	unsigned int nRPCMaxConnections;
	vector<string> vRPCAllowIP;
protected:
protected:
    // called by CRPCServerConfig.Help()
	string HelpImpl() const
	{
        ostringstream oss;
        // corresponding to "format"        "desc" in json
		oss << "  -rpcmaxconnections=<num>                      Set max connections to <num> (default: 5)\n";
		oss << "  -rpcallowip=<ip>                              Allow JSON-RPC connections from specified <ip> address\n";
		return oss.str();
	}
    // called by CRPCServerConfig.CRPCServerConfig()
	void AddOptionsImpl(boost::program_options::options_description& desc)
	{
		AddOpt(desc, "rpcmaxconnections", nRPCMaxConnections, (unsigned int)(DEFAULT_RPC_MAX_CONNECTIONS));
		AddOpt(desc, "rpcallowip", vRPCAllowIP);
	}
    // called by CRPCServerConfig.ListConfig()
	string ListConfigImpl() const
	{
		ostringstream oss;
		oss << "-rpcmaxconnections: " << nRPCMaxConnections << "\n";
		oss << "-rpcallowip: ";
		for (auto& s: vRPCAllowIP)
		{
			oss << s << " ";
		}
		oss << "\n";
		return oss.str();
	}
};
```

### **src/mode/rpc_config.h**
```cpp
// virtual public inherited CRPCServerConfigOption
class CRPCServerConfig : virtual public CRPCBasicConfig, 
                           virtual public CRPCServerConfigOption
{
public:
    CRPCServerConfig();
    virtual ~CRPCServerConfig();
    virtual bool PostLoad();
    virtual std::string ListConfig() const;
    virtual std::string Help() const;

public:
    boost::asio::ip::tcp::endpoint epRPC;
};
```

### **src/mode/rpc_config.cpp**
```cpp
// in construct, call CRPCServerConfigOption::AddOptionsImpl()
CRPCServerConfig::CRPCServerConfig()
{
    po::options_description desc("RPCServer");

    CRPCServerConfigOption::AddOptionsImpl(desc);

    AddOptions(desc);
}

// in ListConfig, call CRPCServerConfigOption::ListConfigImpl()
std::string CRPCServerConfig::ListConfig() const
{
    std::ostringstream oss;
    oss << CRPCServerConfigOption::ListConfigImpl();
    oss << "epRPC: " << epRPC << "\n";
    return CRPCBasicConfig::ListConfig() + oss.str();
}

// in Help, call CRPCServerConfigOption::HelpImpl()
std::string CRPCServerConfig::Help() const
{
    return CRPCBasicConfig::Help() + CRPCServerConfigOption::HelpImpl();
}
```
