# RPC protocol generator

## scope
RPC command protocol detailed definition.  
Includes command line format, options and arguments, JSON-RPC request and response field, errors and examples.  
JSON-RPC version is [2.0](https://www.jsonrpc.org/specification#request_object).  
RPC protocol document [see](https://gitlab.lomocoin.com//BigBang/BigBangCoreWallet/wiki/JSON-RPC)

## difference with JSON-RPC 2.0
1. Response object is required.
2. In request object, **params** can't be omitted.
3. [Bat request](#bat-request) only support HTTP request, command line doesn't support now.
4. If **type** set `array` in configuration, it's exactly express there may be multiple same type elements in it. Specially in request object, don't allow `"params": [1, "2", true]`, use `object` instead.

## files
- **script/rpc.json**: RPC protocol file.
- **srcipt/rpc_generator.py**: rpc generator script.
- **srcipt/rpc_protocol_generator.py**: protocol generator script.
- **src/mode/auto_rpc.h, auto_rpc.cpp**: rpc cpp file.
- **src/mode/auto_protocol.h, auto_protocol.cpp**: detail protocol cpp file.

## usage
1. configure **script/rpc.json**
2. run **srcipt/generator.py**(all generator entry)
3. check **src/mode/auto_rpc.h**.
```c++
// contain all protocols
template <typename... T>
CBasicConfig* CreateConfig(const std::string& cmd)
{
	if (cmd == "help")
	{
		return new mode_impl::CCombinConfig<CHelpConfig, T...>;
	}
	else if (cmd == "stop")
	{
		return new mode_impl::CCombinConfig<CStopConfig, T...>;
	}
    ...
}
...
```
4. build project

### **script/rpc.json**
#### Overview
```json
"encryptkey": {
    "type": "command",
    "name": "EncryptKey",
    "introduction": "Encrypts the key.",
    "desc": [
        "Encrypts the key assoiciated with <passphrase>.  ",
        "For encrypted key, changes the passphrase for [oldpassphrase] to <passphrase>"
    ],
    "request": {
        "type": "object",
        "content": {
            "pubkey": {
                "type": "string",
                "desc": "public key"
            },
            "passphrase": {
                "type": "string",
                "desc": "passphrase of key"
            },
            "oldpassphrase": {
                "type": "string",
                "desc": "old passphrase of key",
                "required": false
            }
        }
    },
    "response": {
        "type": "string",
        "name": "result",
        "desc": "encrypt key result"
    },
    "example": [
        {
            "request": "bigbang-cli encryptkey 2e05c9ee45fdf58f7b007458298042fc3d3ad416a2f9977ace16d14164a3e882 123",
            "response": "Encrypt key successfully: 2e05c9ee45fdf58f7b007458298042fc3d3ad416a2f9977ace16d14164a3e882"
        },
        {
            "request": "{\"id\":64,\"method\":\"encryptkey\",\"jsonrpc\":\"2.0\",\"params\":{\"pubkey\":\"2e05c9ee45fdf58f7b007458298042fc3d3ad416a2f9977ace16d14164a3e882\",\"passphrase\":\"123\"}}",
            "response": "{\"id\":64,\"jsonrpc\":\"2.0\",\"result\":\"Encrypt key successfully: 2e05c9ee45fdf58f7b007458298042fc3d3ad416a2f9977ace16d14164a3e882\"}"
        },
        "bigbang-cli encryptkey 2e05c9ee45fdf58f7b007458298042fc3d3ad416a2f9977ace16d14164a3e882 456 123"
    ],
    "error": [
        "{\"code\":-4,\"message\":\"Unknown key\"}",
        "{\"code\":-406,\"message\":\"The passphrase entered was incorrect.\"}"
    ]
},
```
#### Top level
  + Two type struct can be defined in top level. `class` and `command`.
  + `class` is a cpp class for help organize cpp code. It's **key** is used reference by `command`
  + `command` is a particular RPC command. It's **key** is JSON-RPC **Method**
  + fields
    - **type** in `command` or `class`, required. Value is `class` or `command`.
    ```json
    // this is a class
    "templatepubkey": {
        "type": "class",

    // this is a command
    "encryptkey": {
        "type": "command",
    ```
    - **name**, in `class` and `commond`, optional. means classname in `class`, and part of classname in `command`. If no **name**, use **key** instead.
    ```json
    // class name is TemplatePubKey
    "templatepubkey": {
        "name": "TemplatePubKey",

    // generator at most 3 classes by EncryptKey. CEncryptKeyParam, CEncryptKeyResult, CEncryptKeyConfig
    // format: 'C' + name + 'Param'|'Result'|'Config'
    "encryptkey": {
        "name": "EncryptKey",
    ```
    - **introduction**, optional. Showing on summary help. If not be specified, use **desc** instead.
    ```json
    "introduction": "Get transaction pool info",
    ```
    - **desc** in `command`, optional. Showing on help information. [See](#desc)
    - **request** in `command`, required. Define JSON-RPC request field.
    - **response** in `command`, required. Define JSON-RPC response field. (In this project must define response, although in JSON-RPC standard response is optional.)
    - **example** in `command`, optional. Showing on help information. Support *string* or particular *object*.
    ```json
    "example": [
        // use object. Key is "request" or "response", means this example is a request or response
        {
            "request": "bigbang-cli addnewtemplate mint '{\"mint\": \"e8e3770e774d5ad84a8ea65ed08cc7c5c30b42e045623604d5c5c6be95afb4f9\", \"spent\": \"1z6taz5dyrv2xa11pc92y0ggbrf2wf36gbtk8wjprb96qe3kqwfm3ayc1\"}'",
            "response": "20g0b87qxcd52ceh9zmpzx0hy46pjfzdnqbkh8f4tqs4y0r6sxyzyny25"
        },
        // use string. Means this example is a request
        "bigbang-cli addnewtemplate delegate '{\"delegate\":\"2e05c9ee45fdf58f7b007458298042fc3d3ad416a2f9977ace16d14164a3e882\",\"owner\":\"1gbma6s21t4bcwymqz6h1dn1t7qy45019b1t00ywfyqymbvp90mqc1wmq\"}'",
    ],
    ```
    - **error** in `commmand`, optional. Showing on help information. Format is similar to **desc**
    ```json
    // use string
    "error": "{\"code\":-6,\"message\":\"Invalid parameters,failed to make template\"}"
    // use string array
    "error": [
        "{\"code\":-6,\"message\":\"Invalid parameters,failed to make template\"}",
        "{\"code\":-401,\"message\":\"Failed to add template\"}",
    ]
    ```

#### Request
  + overview
  ```json
  "request": {
    "type": "object",
    "content": {
        "txid": {
            "type": "string",
            "desc": "transaction hash"
        },
        "serialized": {
            "type": "bool",
            "desc": [
                "If serialized=0, returns an Object with information about <txid>.",
                "If serialized is non-zero, returns a string that is",
                "serialized, hex-encoded data for <txid>."
            ],
            "opt": "s",
            "default": false,
            "required": false
        }
    }
  ```
  + fields
    - **type**, required. Means [JSON-RPC params](https://www.jsonrpc.org/specification#request_object) type. Optional value is `object` or `array`, or a class key.
    - **content**, required. [see](#content)

#### Response
  + overview
  ```json
  // one
  "response": {
      "type": "object",
      "name": "location",
      "content": {
          "fork": {
              "type": "string",
              "desc": "fork hash"
          },
          "height": {
              "type": "int",
              "desc": "block height"
          }
      }
  },
  // another
  "response": {
      "type": "int",
      "name": "count",
      "desc": "fork count"
  },
  ```
  + fields
    - **type**, required. Means [JSON-RPC result](https://www.jsonrpc.org/specification#response_object). Optional value [see](#type).
    - **name**, required. Showing on help.
    - **desc**, optional. Showing on help. [See](#desc)
    - **content**, optional. [See](#content)

#### type
Basic type values are `int`, `uint`, `bool`, `double`, `string`, `object`, `array`.  
If type value is other string, it means a reference of class:
```json
    "templaterequest": {
        "type": "class",
        ...
    }
    // reference of class "templaterequest"
    "request": {
        "type": "templaterequest"
    },
```
##### rules
1. `double` type revecie `double`, `int`, `uint` in it's precision range
2. Data range is judged by the business level. RPC only is responsible for basic type range.

#### desc
Support *string* or *string array*, and *string array* will be joined by `\n`.
```json
"desc": [
    "Encrypts the key assoiciated with <passphrase>.  ",
    "For encrypted key, changes the passphrase for [oldpassphrase] to <passphrase>"
],
"desc": "Returns the number of forks.",
```
#### content
If [**type**](#type) value is `array` or `object`, needs to set **content** to describe it's elements.
```json
"list": {
    "type": "array",
    "desc": "transaction pool list",
    "required": false,
    "condition": "detail=true",

    // because "list"-"type" = "array", so needs "content"
    "content": {
        "pool": {
            "type": "object",
            "desc": "pool struct",

            // because "pool"-"type" = "object", so needs "content"
            "content": {
                "hex": {
                    "type": "string",
                    "desc": "tx pool hex"
                },
                "size": {
                    "type": "uint",
                    "desc": "tx pool size"
                }
            }
        }
    }
}
```
##### rules
1. [**content**](#content) is a json object. It's element key is [**param**](#param) name, value is [**param**](#param) attributes.
2. If [**type**](#content) is `array`, [**content**](#content) element number must be 1.
3. [**content**](#content) could nest [**content**](#content), it depends on param's [**type**](#type).
4. Generator supports nested array, but strongly suggest consider if is nessesary. Because in cpp code, it will appear `vector<vector<vector<...>>`
5. Not support use `object` to mock `map<>` in cpp, use `object` in `array` instead.
6. If no params, [request content](#request) should be `{}`. Only used in [Request](#request)

#### param
One param is a object, key is name and value is attributes.
  - overview
  ```json
  // one
  "serialized": {
      "type": "bool",
      "desc": [
          "If serialized=0, returns an Object with information about <txid>.",
          "If serialized is non-zero, returns a string that is",
          "serialized, hex-encoded data for <txid>."
      ],
      "opt": "s",
      "default": false,
      "required": false
  }
  // another
  "type": {
      "type": "string",
      "desc": "template type"
  },
  "delegate": {
      "type": "object",
      "desc": "delegate template struct",
      "condition": "type=delegate",
      "content": {
          "delegate": {
              "type": "string",
              "desc": "delegate public key"
          },
          "owner": {
              "type": "string",
              "desc": "owner address"
          }
      }
  },
  ```
  - fields:
    + **type**, [see](#type)
    + **desc**, [see](#desc)
    + **opt**, optional. A string used for command line option.  
        In overview, "serialized"-"opt" = "s" and "type" = "bool", means in command line:
        ```shell
        bigbang-cli COMMAND -s
        ```
        If "type" != "bool", means:
        ```shell
        bigbang-cli COMMAND -s=value
        ```
        **NOTICE: Don't repeat with other options**
    + **default**, optional. Used for command line, if not specify this param, use **default** value to send JSON-RPC.
        ```json
        ...
        "type": "bool",
        "default": false,
        ...
        ...
        "type": "int",
        "default": 0,
        ...
        ```
    + **required**, optional, default=true. Used for JSON-RPC, if **required** param not be specify value or **default**, error occured.
    + **condition**, optional. String format: `key=value`, used in business code, means `if key=value, this param is valid`.  
        `key` must be the same level with this param.
        ```json
        "type": {
            "type": "string",
            "desc": "template type"
        },
        "delegate": {
            "type": "object",
            "desc": "delegate template struct",

            // this means if "type" = "delegate", "delegate" param is valid
            "condition": "type=delegate",
            "content": {
                "delegate": {
                    "type": "string",
                    "desc": "delegate public key"
                },
                "owner": {
                    "type": "string",
                    "desc": "owner address"
                }
            }
        },
        ```
        **Specially, if `key` not is the same level with this param, it's only show in help**
        ```json
        "response": {
            "type": "object",
            "name": "data",
            "content": {
                "result": {
                    "type": "bool",
                    "desc": "result",
                    "required": false,

                    // there is no "prev" in same level, this only show in help
                    "condition": "prev=matched or block is not generated by POW"
                },
        ```
##### rules
1. put `required=false` params behind `required=true` params, or generator do this.
2. Suggest if one param has **opt**, params behind it also have **opt** to avoid ambiguous.

### bat request
  - request example
  ```shell
  curl -d '[{"id":13,"method":"unlockkey","jsonrpc":"2.0","params":{"pubkey":"d716e72ce58e649a57d54751a7707e325b522497da3a69ae8301a2cbec391c07","passphrase":"1234"}},{"id":14,"method":"unlockkey","jsonrpc":"2.0","params":{"pubkey":"2e05c9ee45fdf58f7b007458298042fc3d3ad416a2f9977ace16d14164a3e882","passphrase":"1234"}}]' http://localhost:9902
  ```
  - resonse example
  ```shell
  [{"id":13,"jsonrpc":"2.0","result":"Unlock key successfully: d716e72ce58e649a57d54751a7707e325b522497da3a69ae8301a2cbec391c07"},{"id":14,"jsonrpc":"2.0","error":{"code":-409,"message":"Key is already unlocked"}}]
  ```

# mode configuration
In script/template/mode.json, there define help info of modes.
```json
{
    "EModeType::MODE_SERVER": {                      // EModeType in src/mode/mode_type.h
        "usage": "bigbang-server (OPTIONS)", // usage info
        "desc": "Run bigbang server"         // desc info
    },
    ...
}
```
