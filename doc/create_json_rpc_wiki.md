# Create JSON-RPC wiki
## Introduction
`script/create_json_rpc_wiki.py` is a semi-automatic stand-alone script to generate two parts of [JSON-RPC wiki](https://gitlab.lomocoin.com//BigBang/BigBangCoreWallet/wiki/JSON-RPC).  
one part is [command line format](https://gitlab.lomocoin.com//BigBang/BigBangCoreWallet/wiki/JSON-RPC#command-line-format),
and another is [commands](https://gitlab.lomocoin.com//BigBang/BigBangCoreWallet/wiki/JSON-RPC#commands)

## Usage
 - If modified `script/template/rpc.json` or rpc part of `script/template/options.json`, needs stand-alone run the script `script/create_json_rpc_wiki.py`
 - The script depends on executable file `multivers-cli` in project root. So you'd better run `INSTALL.sh` first.
 - The script will output result to screen. Or use `script/create_json_rpc_wiki.py > filename` to output to file.
 - The result contains two parts.
   + Copy between `## command line format:` and `## commands` to [command line format](https://gitlab.lomocoin.com//BigBang/BigBangCoreWallet/wiki/JSON-RPC#command-line-format). **Don't cover [command param format rule](https://gitlab.lomocoin.com//BigBang/BigBangCoreWallet/wiki/JSON-RPC#command-param-format-rule)**
   + Copy between `## commands` and end of result to [commands](https://gitlab.lomocoin.com//BigBang/BigBangCoreWallet/wiki/JSON-RPC#commands)
