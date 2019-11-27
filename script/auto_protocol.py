# -*-conding:utf-8-*-
import sys
sys.dont_write_bytecode = True

import json
import re
from collections import OrderedDict
from tool import *
from functools import cmp_to_key

rpc_json, mode_json, protocol_h, protocol_cpp, config_h, config_cpp = None, None, None, None, None, None

# help list and dict
commands = OrderedDict()
alone_classes = OrderedDict()
all_classes = {}
json_type_list = ('int', 'uint', 'double', 'bool', 'string', 'array', 'object')

RPC_DOUBLE_PRECISION = '6'

modes = OrderedDict()
param_class = OrderedDict()
result_class = OrderedDict()
config_class = OrderedDict()
introduction = OrderedDict()

# help function
def subclass_name(name, named=False):
    if named:
        return 'C' + name
    else:
        return 'C' + name.title()


def param_name(name, type):
    if type == 'int' or type == 'uint':
        return 'n' + name.title()
    elif type == 'double':
        return 'd' + name.title()
    elif type == 'bool':
        return 'f' + name.title()
    elif type == 'array':
        return 'vec' + name.title()
    elif type == 'string':
        return 'str' + name.title()
    else:
        return name


def is_pod(type):
    return type == 'int' or type == 'uint' or type == 'double' \
        or type == 'bool' or type == 'string'


def get_content(prefix, json, type):
    content = get_json_value(prefix, json, 'content', dict)
    if type == 'array' and len(content) != 1:
        raise Exception('[%s] content length must be 1' % prefix)
    return content


def get_type(prefix, json):
    type = get_json_value(prefix, json, 'type', str)
    reference = not type in json_type_list
    ref_cls = None
    if reference:
        if (not type in alone_classes):
            raise Exception('[%s] Unknown [%s]' % (prefix, type))
        ref_cls = alone_classes[type]
    return type, reference, ref_cls


def get_required(prefix, json):
    required = get_json_value(prefix, json, 'required', bool, default=True)
    return required


def get_default(prefix, json, type, required):
    default = get_json_value(prefix, json, 'default', required=False)
    if default != None and is_pod(type):
        return quote(default, type)
    elif type == 'array' and required:
        return 'RPCValid'
    return None


def get_opt(prefix, json):
    opt = get_json_value(prefix, json, 'opt', str, required=False)
    return opt


def get_condition(prefix, json, content):
    condstr = get_json_value(prefix, json, 'condition', str, required=False)
    condition = None
    if condstr:
        condition = re.findall('(\&\&|\|\||^)[\s]*([^=\s\&\|]*)=([^=\s\&\|]*)', condstr)
        for i in range(len(condition)):
            op, cond_key, cond_value = condition[i]

            # if cond_key is not in content, cond_cpp_name = None, and only used in help.
            cond_cpp_name = None
            if cond_key in content:
                cond_type = get_json_value(prefix, content[cond_key], 'type')
                if not is_pod(cond_type):
                    raise Exception('condition type can not be [%s]' % cond_type)
                cond_value = quote(cond_value, cond_type)
                cond_cpp_name = param_name(cond_key, cond_type)

            condition[i] = (op, cond_key, cond_value, cond_cpp_name)
    return condstr, condition


def if_condition_code(cls_name, condition):
    s = ''
    for v in condition:
        op, _, cond_value, cond_cpp_name = v
        if cond_cpp_name:
            if s != '' and op:
                op = ' ' + op + ' '
            else:
                op = ''
            s = s + op + cls_name + cond_cpp_name + ' == ' + cond_value
    
    if s == '':
        return None
    else:
        return 'if (' + s + ')\n'


def convert_cpp_type(type, sub=None):
    if type == 'int':
        return 'CRPCInt64'
    elif type == 'uint':
        return 'CRPCUint64'
    elif type == 'double':
        return 'CRPCDouble'
    elif type == 'bool':
        return 'CRPCBool'
    elif type == 'string':
        return 'CRPCString'
    elif type == 'array':
        return 'CRPCVector<%s>' % sub
    else:
        return sub


def convert_native_type(type, sub=None):
    if type == 'int':
        return 'int64'
    elif type == 'uint':
        return 'uint64'
    elif type == 'double':
        return 'double'
    elif type == 'bool':
        return 'bool'
    elif type == 'string':
        return 'std::string'
    else:
        return sub


def get_cpp_json(value, type):
    if type == 'int':
        return value + '.get_int64()'
    elif type == 'uint':
        return value + '.get_uint64()'
    elif type == 'double':
        return value + '.get_real()'
    elif type == 'bool':
        return value + '.get_bool()'
    elif type == 'string':
        return value + '.get_str()'
    elif type == 'array':
        return value + '.get_array()'
    else:
        return value + '.get_obj()'


def get_cpp_json_type(type):
    if type == 'int':
        return 'int_type'
    elif type == 'uint':
        return 'int_type'
    elif type == 'double':
        return 'real_type'
    elif type == 'bool':
        return 'bool_type'
    elif type == 'string':
        return 'str_type'
    elif type == 'array':
        return 'array_type'
    else:
        return 'obj_type'


def config_options_type(type):
    if type == 'int':
        return 'int64'
    elif type == 'uint':
        return 'uint64'
    elif type == 'bool':
        return 'bool'
    elif type == 'double':
        return 'double'
    else:
        return 'string'


def help_type(s, type):
    if type == 'double':
        return '$%s$' % s
    elif type == 'bool':
        return '*%s*' % s
    elif type == 'string':
        return '"%s"' % s
    elif type == 'object':
        return '{%s}' % s
    elif type == 'array':
        return '[%s]' % s
    else:
        return s


def help_placeholder(s, type, force=False):
    if type == 'int' or type == 'uint':
        return s if force else '0'
    elif type == 'double':
        return s if force else '0.0'
    elif type == 'bool':
        return s if force else 'true|false'
    elif type == 'string':
        return '"%s"' % s if force else '""'
    else:
        return s if force else ''


def is_nested_array(type):
    matched = re.match('^CRPCVector\<(CRPCVector\<.*\>)\>$', type)
    return matched.groups()[0] if matched else False


def decode_nested_array(type):
    sub_array = is_nested_array(type)
    return sub_array if sub_array else type


def pod_from_json(name, type, value, w, indent):
    w.write(indent + name + ' = ' + get_cpp_json(value, type) + ';\n')


def array_from_json(name, cpp_type, sub_type, value, w, indent, n=0):
    w.write(indent + 'for (auto& v : ' + value + ')\n')
    indent = brace_begin(w, indent)
    # nested array
    if is_nested_array(cpp_type):
        arr_type = decode_nested_array(cpp_type)
        arr_name = 'vec' + str(n)
        arr_val = 'arr' + str(n)

        w.write(indent + 'auto ' + arr_val + ' = ' + get_cpp_json('v', 'array') + ';\n')
        w.write(indent + arr_type + ' ' + arr_name + ';\n')
        array_from_json(arr_name, arr_type, sub_type, arr_val, w, indent, n + 1)
        w.write(indent + name + '.push_back(' + arr_name + ');\n')
    elif is_pod(sub_type):
        w.write(indent + name + '.push_back(' + get_cpp_json('v', sub_type) + ');\n')
    else:
        w.write(indent + name + '.push_back(' + cpp_type + '::value_type().FromJSON(v));\n')

    brace_end(w, indent)


def object_from_json(name, value, w, indent):
    w.write(indent + name + '.FromJSON(' + value + ');\n')


def pod_to_json(name, value, w, indent):
    w.write(indent + value + ' = Value(' + name + ');\n')


def array_to_json(name, cpp_type, sub_type, value, w, indent, n=0):
    w.write(indent + 'for (auto& v : ' + name + ')\n')
    indent = brace_begin(w, indent)
    if is_nested_array(cpp_type):
        arr_type = decode_nested_array(cpp_type)
        arr_name = 'vec' + str(n)
        arr_val = 'arr' + str(n)

        w.write(indent + 'auto& ' + arr_name + ' = v;\n')
        w.write(indent + 'Array ' + arr_val + ';\n')
        array_to_json(arr_name, arr_type, sub_type, arr_val, w, indent, n + 1)
        w.write(indent + value + '.push_back(' + arr_val + ');\n')
    elif is_pod(sub_type):
        w.write(indent + value + '.push_back(' + convert_native_type(sub_type) + '(v));\n')
    else:
        w.write(indent + value + '.push_back(v.ToJSON());\n')
    brace_end(w, indent)


def object_to_json(key, name, type, cpp_type, sub_type, value, w, indent):
    val_name = ''
    if is_pod(type):
        val_name = convert_native_type(type) + '(' + name + ')'
    elif type == 'array':
        val_name = name + 'Array'
        w.write(indent + 'Array ' + val_name + ';\n')
        array_to_json(name, cpp_type, sub_type, val_name, w, indent)
    else:
        val_name = name + '.ToJSON()'
    w.write(indent + value + '.push_back(Pair("' + key + '", ' + val_name + '));\n')


def find_subclass(p, subclass):
    if p.subclass_prefix in all_classes:
        return all_classes[p.subclass_prefix]
    return None


def call_check_json_type(value, key, type, w, indent):
    w.write(indent + 'CheckJSONType(' + value + ', ' + quote(type) + ', ' + quote(key) + ');\n')


def from_json_value(value, key, name, type, w, indent):
    call_check_json_type(value, key, type, w, indent)
    if type == 'object':
        w.write(indent + name + '.FromJSON(' + get_cpp_json(value, type) + ');\n')
    else:
        w.write(indent + name + ' = ' + get_cpp_json(value, type) + ';\n')


def call_check_is_valid(value, key, w, indent):
    w.write(indent + 'CheckIsValid(' + value + ', ' + quote(key) + ');\n')


# generate function
def check_json_type_fun(w):
    w.write('static void CheckJSONType(const Value& value, const string& type, const string& key)\n')
    indent = brace_begin(w)

    w.write(indent + 'bool b;\n')

    w.write(indent)
    for type in json_type_list:
        w.write('if (type == ' + quote(type) + ')\n')
        indent = brace_begin(w, indent)
        if type == 'uint':
            w.write(indent + 'b = ((value.type() == ' + get_cpp_json_type('uint')
                    + ') && (value.is_uint64() || ' + get_cpp_json('value', 'int') + ' >= 0));\n')
        elif type == 'double':
            w.write(indent + 'b = ((value.type() == ' + get_cpp_json_type('double')
                    + ') || (value.type() == ' + get_cpp_json_type('int') + '));\n')
        else:
            w.write(indent + 'b = (value.type() == ' + get_cpp_json_type(type) + ');\n')
        indent = brace_end(w, indent)
        w.write(indent + 'else ')

    empty_line(w)
    indent = brace_begin(w, indent)
    w.write(indent + 'b = false;\n')
    indent = brace_end(w, indent)

    w.write(indent + 'if (!b)\n')
    indent = brace_begin(w, indent)
    w.write(indent + 'throw CRPCException(RPC_INVALID_PARAMS, string("[") + key + "] type is not " + type);\n')
    indent = brace_end(w, indent)

    brace_end(w, indent)


def check_is_valid_fun(w):
    w.write('template <typename T>\n')
    w.write('static void CheckIsValid(const T& value, const string& key)\n')
    indent = brace_begin(w)

    w.write(indent + 'if (!value.IsValid())\n')
    indent = brace_begin(w, indent)
    w.write(indent + 'throw CRPCException(RPC_INVALID_PARAMS, string("required param [") + key + "] is not valid");\n')
    indent = brace_end(w, indent)

    brace_end(w, indent)


# functions of protocol_h and protocol_cpp
def constructor_h(name, params, w, indent):
    # default
    w.write(indent + name + '();\n')

    # all params
    if len(params) > 0:
        w.write(indent + name + '(')
        param_list = []
        for p in params:
            param_list.append('const ' + p.cpp_type + '& ' + p.cpp_name)

        w.write(', '.join(param_list))
        w.write(');\n')


def constructor_cpp(name, params, w, scope):
    # default
    w.write(scope + name + '() {}\n')

    # all params
    if len(params) > 0:
        w.write(scope + name + '(')
        param_list = []
        for p in params:
            param_list.append('const ' + p.cpp_type + '& ' + p.cpp_name)
        w.write(', '.join(param_list))
        w.write(')\n')

        if len(params) > 0:
            w.write('\t: ')
            init_list = []
            for p in params:
                init_list.append(p.cpp_name + '(' + p.cpp_name + ')')
            w.write(', '.join(init_list) + '\n')

        brace_begin(w)
        brace_end(w)

def destructor_h(name, w, indent):
    # default
    w.write(indent + "virtual ~" + name + '() = default;\n')


def FromJSON_h(virtual, name, w, indent):
    v_tag = 'virtual ' if virtual else ''
    w.write(indent + v_tag + name + '& FromJSON(const json_spirit::Value& v);\n')


def FromJSON_cpp(key, name, params, container, w, scope):
    def one_param(p, value, indent):
        if p.type == 'array':
            arr_val = p.cpp_name + 'Array'
            from_json_value(value, p.key, 'auto ' + arr_val, p.type, w, indent)
            array_from_json(p.cpp_name, p.cpp_type, p.sub_type, arr_val, w, indent)
        else:
            from_json_value(value, p.key, p.cpp_name, p.type, w, indent)

    # begin
    w.write(name + '& ' + scope + 'FromJSON(const Value& v)\n')
    indent = brace_begin(w)

    # object
    if container == 'object':
        call_check_json_type('v', key, 'object', w, indent)
        w.write(indent + 'auto obj = v.get_obj();\n')
        for p in params:
            condstr = if_condition_code('', p.condition) if p.condition else None
            if condstr:
                w.write(indent + condstr)
                indent = brace_begin(w, indent)

            val = 'val' + p.key.title()
            w.write(indent + 'auto ' + val + ' = find_value(obj, "' + p.key + '");\n')

            if not p.required:
                w.write(indent + 'if (!' + val + '.is_null())\n')
                indent = brace_begin(w, indent)

            one_param(p, val, indent)

            if not p.required:
                indent = brace_end(w, indent)

            if condstr:
                indent = brace_end(w, indent)
    else:
        one_param(params[0], 'v', indent)

    # bottom
    w.write(indent + 'return *this;\n')
    brace_end(w)


def ToJSON_h(virtual, w, indent):
    v_tag = 'virtual ' if virtual else ''
    w.write(indent + v_tag + 'json_spirit::Value ToJSON() const;\n')


def ToJSON_cpp(name, params, container, w, scope):

    # begin
    w.write('Value ' + scope + 'ToJSON() const\n')
    indent = brace_begin(w)

    if is_pod(container):
        p = params[0]
        if p.required:
            call_check_is_valid(p.cpp_name, p.cpp_name, w, indent)
        w.write(indent + 'Value val;\n')
        pod_to_json(p.cpp_name, 'val', w, indent)
        w.write(indent + 'return val;\n')
    # array
    elif container == 'array':
        w.write(indent + 'Array ret;\n')
        p = params[0]
        # push array
        array_to_json(p.cpp_name, p.cpp_type, p.sub_type, 'ret', w, indent)
        w.write(indent + 'return ret;\n')
    # object
    elif container == 'object':
        w.write(indent + 'Object ret;\n')
        for p in params:
            condstr = if_condition_code('', p.condition) if p.condition else None
            if condstr:
                w.write(indent + condstr)
                indent = brace_begin(w, indent)

            if p.required:
                call_check_is_valid(p.cpp_name, p.cpp_name, w, indent)
            else:
                w.write(indent + 'if (' + p.cpp_name + '.IsValid())\n')
                indent = brace_begin(w, indent)

            object_to_json(p.key, p.cpp_name, p.type, p.cpp_type, p.sub_type, 'ret', w, indent)

            if not p.required:
                indent = brace_end(w, indent)

            if condstr:
                indent = brace_end(w, indent)
        empty_line(w)
        w.write(indent + 'return ret;\n')
    # request, response reference
    else:
        p = params[0]
        if p.required:
            call_check_is_valid(p.cpp_name, p.cpp_name, w, indent)
        w.write(indent + 'return ' + p.cpp_name + '.ToJSON();\n')

    brace_end(w, indent)


def Method_h(virtual, w, indent):
    v_tag = 'virtual ' if virtual else ''
    w.write(indent + v_tag + 'std::string Method() const;\n')


def Method_cpp(method, w, scope):
    w.write('string ' + scope + 'Method() const\n')
    indent = brace_begin(w)
    w.write(indent + 'return "' + method + '";\n')
    brace_end(w)


def MakeSharedPtr_h(name, w, indent):
    w.write(indent + 'template <typename... Args>\n')
    w.write(indent + 'std::shared_ptr<' + name + '> Make' + name + 'Ptr(Args&&... args)\n')
    indent = brace_begin(w, indent)
    w.write(indent + 'return std::make_shared<' + name + '>(std::forward<Args>(args)...);\n')
    indent = brace_end(w, indent)


def IsValid_h(w, indent):
    w.write(indent + 'bool IsValid() const;\n')


def IsValid_cpp(name, params, w, scope):
    w.write('bool ' + scope + 'IsValid() const\n')
    indent = brace_begin(w)

    for p in params:
        if p.required:
            condstr = if_condition_code('', p.condition) if p.condition else None
            if condstr:
                w.write(indent + condstr)
                indent = brace_begin(w, indent)

            w.write(indent + 'if (!' + p.cpp_name + '.IsValid()) { return false; }\n')

            if condstr:
                indent = brace_end(w, indent)

    w.write(indent + 'return true;\n')
    brace_end(w)


def constructor_null_h(name, params, w, indent):
    w.write(indent + name + '(const CRPCType& type);\n')


def constructor_null_cpp(name, params, w, scope):
    # all params constructor
    if not params or len(params) == 0:
        return
    w.write(scope + name + '(const CRPCType& null)\n')
    if len(params) > 0:
        w.write('\t: ')
        init_list = []
        for p in params:
            init_list.append(p.cpp_name + '(null)')
        w.write(', '.join(init_list) + '\n')

    brace_begin(w)
    brace_end(w)


def CreateCRPCParam_h(w):
    w.write('// Create family class shared ptr of CRPCParam\n')
    w.write('CRPCParamPtr CreateCRPCParam(const std::string& cmd, const json_spirit::Value& valParam);\n')


def CreateCRPCParam_cpp(w):
    w.write('CRPCParamPtr CreateCRPCParam(const std::string& cmd, const json_spirit::Value& valParam)\n')
    indent = brace_begin(w)

    if len(param_class) == 0:
        w.write(
            indent + 'throw CRPCException(RPC_METHOD_NOT_FOUND, cmd + " not found!");\n')
    else:
        w.write(indent)
        for cmd, name in param_class.items():
            w.write('if (cmd == "' + cmd + '")\n')
            indent = brace_begin(w, indent)
            w.write(indent + 'auto ptr = Make' + name + 'Ptr();\n')
            w.write(indent + 'ptr->FromJSON(valParam);\n')
            w.write(indent + 'return ptr;\n')
            indent = brace_end(w, indent)
            w.write(indent + 'else ')

        empty_line(w)
        indent = brace_begin(w, indent)
        w.write(indent + 'throw CRPCException(RPC_METHOD_NOT_FOUND, cmd + " not found!");\n')
        indent = brace_end(w, indent)

    brace_end(w, indent)


def CreateCRPCResult_h(w):
    w.write('// Create family class shared ptr of CRPCResult\n')
    w.write('CRPCResultPtr CreateCRPCResult(const std::string& cmd, const json_spirit::Value& valResult);\n')


def CreateCRPCResult_cpp(w):
    w.write('CRPCResultPtr CreateCRPCResult(const std::string& cmd, const json_spirit::Value& valResult)\n')
    indent = brace_begin(w)

    if len(result_class) == 0:
        w.write(indent + 'return std::make_shared<CRPCCommonResult>();\n')
    else:
        w.write(indent)
        for cmd, name in result_class.items():
            if result_class:
                w.write('if (cmd == "' + cmd + '")\n')
                indent = brace_begin(w, indent)
                w.write(indent + 'auto ptr = Make' + name + 'Ptr();\n')
                w.write(indent + 'ptr->FromJSON(valResult);\n')
                w.write(indent + 'return ptr;\n')
                indent = brace_end(w, indent)
                w.write(indent + 'else ')

        empty_line(w)
        indent = brace_begin(w, indent)
        w.write(indent + 'auto ptr = std::make_shared<CRPCCommonResult>();\n')
        w.write(indent + 'ptr->FromJSON(valResult);\n')
        w.write(indent + 'return ptr;\n')
        indent = brace_end(w, indent)

    brace_end(w, indent)

def RPCCmdList_h(w):
    w.write('// All rpc command list\n')
    w.write('const std::vector<std::string>& RPCCmdList();\n')


def RPCCmdList_cpp(w):
    w.write('const std::vector<std::string>& RPCCmdList()\n')
    indent = brace_begin(w)

    w.write(indent + 'static std::vector<std::string> list = \n')
    w.write(indent + '{\n')
    for cmd in param_class:
        w.write(indent + '\t' + quote(cmd) + ',\n')
    w.write(indent + '};\n')

    w.write(indent + 'return list;\n')

    brace_end(w, indent)


# functions of config_h and config_cpp
def config_constructor_h(name, w, indent):
    # default constructor
    w.write(indent + name + '();\n')


def config_constructor_cpp(name, params, w, scope):
    w.write(scope + name + '()\n')
    indent = brace_begin(w)

    opt_params = []
    for p in params:
        if p.opt:
            opt_params.append(p)

    if len(opt_params) > 0:
        w.write(
            indent + 'boost::program_options::options_description desc("' + name + '");\n')
        empty_line(w)

        # put default value assignment to PostLoad
        for p in opt_params:
            w.write(indent + 'AddOpt<' + config_options_type(p.type) + '>(desc, "' + p.opt + '");\n')

        empty_line(w)
        w.write(indent + 'AddOptions(desc);\n')

    brace_end(w)


def PostLoad_h(w, indent):
    w.write(indent + 'virtual bool PostLoad();\n')


def PostLoad_cpp(name, params, sub_params, w, scope):
    w.write('bool ' + scope + 'PostLoad()\n')
    indent = brace_begin(w)

    # if help, return
    w.write(indent + 'if (fHelp)\n')
    indent = brace_begin(w, indent)
    w.write(indent + 'return true;\n')
    indent = brace_end(w, indent)

    # check vecCommand size
    empty_line(w)

    # <class obj>.<member>
    origin_param = ''
    if sub_params:
        origin_param = params[0].cpp_name + '.'
        params = sub_params

    # TODO: simple check superfluous arguments given, not exactly because of not considering options arguments
    w.write(indent + 'if (vecCommand.size() > ' + str(len(params)+1) + ')\n')
    indent = brace_begin(w, indent)
    w.write(indent + 'throw CRPCException(RPC_PARSE_ERROR, string("too arguments given."));\n')
    indent = brace_end(w, indent)

    # read data from vecCommand and vm
    w.write(indent + 'auto it = vecCommand.begin(); \n(void)it;\n')
    for p in params:
        param_name = origin_param + p.cpp_name

        # condition
        condstr = if_condition_code(origin_param, p.condition) if p.condition else None
        if condstr:
            w.write(indent + condstr)
            indent = brace_begin(w, indent)

        container_str = 'strOrigin' + p.key.title()
        if not is_pod(p.type):
            w.write(indent + 'string ' + container_str + ';\n')

        # opt argument first find from boost::program_option::variables_map, then find from vecCommand
        if p.opt:
            w.write(indent + 'if (vm.find("' + p.opt + '") != vm.end())\n')
            indent = brace_begin(w, indent)
            w.write(indent + 'auto value = vm["' + p.opt + '"];\n')
            if is_pod(p.type):
                w.write(indent + param_name + ' = value.as<' + config_options_type(p.type) + '>();\n')
            else:
                w.write(indent + container_str + ' = value.as<string>();\n')
            indent = brace_end(w, indent)
            w.write(indent + 'else\n')
            indent = brace_begin(w, indent)

        # find from vecCommand
        w.write(indent + 'if (next(it, 1) != vecCommand.end())\n')
        indent = brace_begin(w, indent)
        if is_pod(p.type):
            if p.type == 'string':
                w.write(indent + param_name + ' = *++it;\n')
            else:
                w.write(indent + 'istringstream iss(*++it);\n')
                if p.type == 'bool':
                    w.write(indent + 'iss >> boolalpha >> ' + param_name + ';\n')
                else:
                    w.write(indent + 'iss >> ' + param_name + ';\n')
                w.write(indent + 'if (!iss.eof() || iss.fail())\n')
                w.write(indent + '\tthrow CRPCException(RPC_PARSE_ERROR, "[' + p.key + '] type error, needs ' + p.type + '");\n')
        else:
            w.write(indent + container_str + ' = *++it;\n')
        indent = brace_end(w, indent)

        if p.default != None:
            w.write(indent + 'else\n')
            indent = brace_begin(w, indent)
            w.write(indent + param_name + ' = ' + p.default + ';\n')
            indent = brace_end(w, indent)
        # throw exception if required
        elif p.required:
            w.write(indent + 'else\n')
            indent = brace_begin(w, indent)
            w.write(indent + 'throw CRPCException(RPC_PARSE_ERROR, "[' + p.key + '] is required");\n')
            indent = brace_end(w, indent)

        # deal object and array
        if not is_pod(p.type):
            # read json string
            container_val = 'valOrigin' + p.key.title()
            w.write(indent + 'Value ' + container_val + ';\n')
            w.write(indent + 'if (!read_string(' + container_str + ', ' + container_val + '))\n')
            indent = brace_begin(w, indent)
            w.write(indent + 'throw CRPCException(RPC_PARSE_ERROR, "parse json error");\n')
            indent = brace_end(w, indent)

            # construct object
            if p.type == 'object':
                w.write(indent + param_name + '.FromJSON(' + container_val + ');\n')
            # construct array
            else:
                array_from_json(param_name, p.cpp_type, p.sub_type,
                                get_cpp_json(container_val, 'array'), w, indent)

        # opt end brace
        if p.opt:
            indent = brace_end(w, indent)

        # condition
        if condstr:
            indent = brace_end(w, indent)

    w.write(indent + 'return true;\n')
    brace_end(w)


def ListConfig_h(w, indent):
    w.write(indent + 'virtual std::string ListConfig() const;\n')
    pass


def ListConfig_cpp(name, params, sub_params, w, scope):
    w.write('string ' + scope + 'ListConfig() const\n')
    indent = brace_begin(w)
    w.write(indent + 'return "";\n')
    brace_end(w)


def Help_h(w, indent):
    w.write(indent + 'virtual std::string Help() const;\n')


def Help_cpp(config, w, scope):

    # one line content
    def one_line(container, content, indent, max_length=None):
        l = split(content, ' ' * len(indent), max_length)
        container.append((indent, l))

    # deal one command line argument
    def one_argument(container, format, type, desc, required, default, condstr, indent):
        if condstr:
            cond_statement = indent + '(if ' + condstr + ')\n'
            container.append((indent + cond_statement, None))

        arg_format = indent + format
        arg_format = arg_format + space(arg_format, summary_indent)

        tips = [type, 'required' if required else 'optional']
        if default != None and default != 'RPCNull':
            tips.append('default=' + default)
        desc = '(' + ', '.join(tips) + ') ' + desc
        desc_list = split(desc, summary_indent)
        container.append((arg_format, desc_list))

    # deal one json-rpc param
    def one_param(container, key, type, desc, required, indent, default=None,
                  condstr=None, has_dot=True, place_holder=None):
        if condstr:
            cond_statement = '(if ' + condstr + ')\n'
            container.append((indent + cond_statement, None))

        if type == 'array':
            return

        key_fmt = quote(key) + ': '
        if is_pod(type):
            if place_holder == None:
                key_fmt = key_fmt + help_placeholder(key, type)
            else:
                key_fmt = key_fmt + help_placeholder(place_holder, type, True)

            if has_dot:
                key_fmt = key_fmt + ','

        arg_format = indent + key_fmt
        arg_format = arg_format + space(arg_format, summary_indent)

        tips = [type, 'required' if required else 'optional']
        if default != None and default != 'RPCNull':
            tips.append('default=' + default)
        desc = '(' + ', '.join(tips) + ') ' + desc
        desc_list = split(desc, summary_indent)
        container.append((arg_format, desc_list))

    # nested arguments
    def sub_params(container, type, cpp_type, p, subclass, indent):
        next_indent = indent + sub_argument_indent
        if type == 'array':
            container.append((indent + '[\n', None))

            if is_nested_array(cpp_type):
                arr_type = decode_nested_array(cpp_type)
                sub_params(container, 'array', arr_type, p, subclass, next_indent)
            else:
                one_param(container, p.sub_key, p.sub_type, p.sub_desc, True, next_indent, has_dot=False)
                if not is_pod(p.sub_type):
                    sub_params(container, 'object', cpp_type, p, subclass, next_indent)

            container.append((indent + ']\n', None))

        elif not is_pod(type):
            container.append((indent + '{\n', None))
            for p in subclass.params:
                if p.type != 'array':
                    one_param(container, p.key, p.type, p.desc, p.required, next_indent,
                              p.default, p.condstr, p != subclass.params[-1])
                if not is_pod(p):
                    sub_params(container, p.type, p.cpp_type, p, find_subclass(p, subclass.subclass), next_indent)

            container.append((indent + '}\n', None))

    # reference class, <class obj>.<member>
    req_params = config.req_params
    if config.req_sub_params:
        req_params = config.req_sub_params
    resp_params = config.resp_params
    if config.resp_sub_params:
        resp_params = config.resp_sub_params

    # storage struct
    usage = config.cmd
    desc = config.desc
    arguments = []
    request = []
    response = []
    example = []
    error = []

    # parse command line arguments
    arg_count = 1
    condition = False
    for p in req_params:
        format = help_type(p.key, p.type)
        # usage
        if p.opt:
            if p.type == 'bool':
                true_opt, false_opt = '-' + p.opt, '-no' + p.opt
                format = true_opt + '|' + false_opt + format
            else:
                format = '-' + p.opt + '=' + format

        required_begin, required_end = ('<', '>') if p.required else ('(', ')')
        required_fmt = required_begin + format + required_end

        if p.condition and condition:
            usage = usage + '|' + required_fmt
        else:
            usage = usage + ' ' + required_fmt
        condition = True if p.condition else False

        # parse one argument
        one_argument(arguments, format, p.type, p.desc, p.required,
                     p.default, p.condstr, argument_indent)

        # index keep same when condition
        if not condition:
            arg_count = arg_count + 1

    # parse request
    if len(req_params) == 0:
        request.append((argument_indent + '"param" : {}\n', None))
    else:
        request.append((argument_indent + '"param" :\n', None))
        if config.req_type != 'array':
            request.append((argument_indent + '{\n', None))

        next_indent = argument_indent + sub_argument_indent
        for p in req_params:
            one_param(request, p.key, p.type, p.desc, p.required, next_indent,
                      p.default, p.condstr, p != req_params[-1])
            sub_params(request, p.type, p.cpp_type, p, find_subclass(p, config.req_subclass), next_indent)

        if config.req_type != 'array':
            request.append((argument_indent + '}\n', None))

    # parse response
    if resp_params:
        if is_pod(config.resp_type):
            p = resp_params[0]
            one_param(response, "result", p.type, p.desc, p.required, argument_indent,
                      p.default, p.condstr, False, p.key)
        else:
            response.append((argument_indent + '"result" :\n', None))
            if config.resp_type != 'array':
                response.append((argument_indent + '{\n', None))

            next_indent = argument_indent + sub_argument_indent
            for p in resp_params:
                one_param(response, p.key, p.type, p.desc, p.required, next_indent,
                          p.default, p.condstr, p != resp_params[-1])
                sub_params(response, p.type, p.cpp_type, p, find_subclass(p, config.resp_subclass), next_indent)

            if config.resp_type != 'array':
                response.append((argument_indent + '}\n', None))

    # parse example
    if config.example:
        if isinstance(config.example, str):
            one_line(example, config.example, example_req_indent)
        elif isinstance(config.example, list):
            for eg in config.example:
                enter = '' if eg == config.example[0] else '\n'
                if isinstance(eg, str):
                    one_line(example, eg, enter + example_req_indent)
                elif isinstance(eg, dict):
                    if 'request' in eg:
                        one_line(example, eg['request'], enter + example_req_indent)
                    if 'response' in eg:
                        one_line(example, eg['response'], example_resp_indent)

    # parse error
    if config.error:
        if isinstance(config.error, str):
            one_line(error, config.error, error_indent)
        elif isinstance(config.error, list):
            for e in config.error:
                one_line(error, e, error_indent)

    # function begin
    w.write('string ' + scope + 'Help() const\n')
    indent = brace_begin(w)

    w.write(indent + 'std::ostringstream oss;\n')

    # write usage
    write_usage(usage, w, indent)

    # description
    write_desc(desc, w, indent)

    # argument
    w.write(indent + 'oss << "Arguments:\\n";\n')
    write_chapter(arguments, w, indent)

    # request
    w.write(indent + 'oss << "Request:\\n";\n')
    write_chapter(request, w, indent)

    # response
    w.write(indent + 'oss << "Response:\\n";\n')
    write_chapter(response, w, indent)

    # example
    w.write(indent + 'oss << "Examples:\\n";\n')
    write_chapter(example, w, indent)

    # error
    w.write(indent + 'oss << "Errors:\\n";\n')
    write_chapter(error, w, indent)

    # return
    w.write(indent + 'return oss.str();\n')

    brace_end(w)


def CreateConfig_h(w):
    w.write('// Dynamic create combin config with rpc command\n')
    w.write('template <typename... T>\n')
    w.write('CBasicConfig* CreateRPCCommandConfig(const std::string& cmd)\n')
    indent = brace_begin(w)

    if len(config_class) == 0:
        w.write(indent + 'return new mode_impl::CCombinConfig<T...>();\n')
    else:
        w.write(indent)
        for cmd, name in config_class.items():
            w.write('if (cmd == "' + cmd + '")\n')
            indent = brace_begin(w, indent)
            w.write(indent + 'return new mode_impl::CCombinConfig<' +
                    name + ', T...>;\n')
            indent = brace_end(w, indent)
            w.write(indent + 'else ')

        empty_line(w)
        indent = brace_begin(w, indent)
        w.write(indent + 'return new mode_impl::CCombinConfig<T...>();\n')
        indent = brace_end(w, indent)

    brace_end(w, indent)


def RPCHelp_h(w):
    w.write('// Return help info by mode type and sub command\n')
    w.write('std::string RPCHelpInfo(EModeType type, const std::string& subCmd, const std::string& options = "");\n')


def RPCHelp_cpp(w):
    # write "Options"
    def write_options(w, indent):
        w.write(indent + 'oss << "Options:\\n";\n')
        w.write(indent + 'oss << options << "\\n";\n')

    # write 'commands'
    def write_introduction(cmd, introduction, w, indent):
        introduction_list = split(introduction)
        introduction_code = terminal_str_code(indent, 'oss << ', cmd + space(cmd), introduction_list)
        w.write(introduction_code)

    w.write('std::string RPCHelpInfo(EModeType type, const std::string& subCmd, const std::string& options)\n')
    indent = brace_begin(w)

    w.write(indent + 'std::ostringstream oss;\n')
    w.write(indent)
    for mode in modes:
        usage, desc = modes[mode]
        w.write('if (type == ' + mode + ')\n')
        indent = brace_begin(w, indent)
        if mode != 'EModeType::CONSOLE':
            write_usage(usage, w, indent)
            write_desc(desc, w, indent)
            write_options(w, indent)
        else:
            # sub command is empty, output help
            w.write(indent + 'if (subCmd.empty())\n')
            indent = brace_begin(w, indent)

            # options is not empty, output options. It's bigbang-cli -help
            w.write(indent + 'if (!options.empty())\n')
            indent = brace_begin(w, indent)
            write_usage(usage, w, indent)
            write_desc(desc, w, indent)
            write_options(w, indent)
            indent = brace_end(w, indent)

            # output command list
            w.write(indent + 'oss << "Commands:\\n";\n')
            for cmd, intro in introduction.items():
                write_introduction(commands_indent + cmd, intro, w, indent)

            indent = brace_end(w, indent)

            # output one command help. If subCmd == "all", show all command help
            for cmd, class_name in config_class.items():
                w.write(indent + 'if (subCmd == "all" || subCmd == "' + cmd + '")\n')
                indent = brace_begin(w, indent)
                w.write(indent + 'oss << ' + class_name + '().Help();\n')
                indent = brace_end(w, indent)

        indent = brace_end(w, indent)
        w.write(indent + 'else ')

    empty_line(w)
    indent = brace_begin(w, indent)
    indent = brace_end(w, indent)

    w.write(indent + 'return oss.str();\n')
    brace_end(w, indent)


# other
def parse_params(class_prefix, content, allow_empty):
    # function for nested array
    def nested_array(arr_prefix, arr_content):
        cpp_type, sub_key, sub_type, sub_desc, subclass, subclass_prefix = None, None, None, None, None, None

        sub_content = get_content(arr_prefix, arr_content, 'array')
        sub_key, sub_value = list(sub_content.items())[0]
        sub_prefix = join_prefix(arr_prefix, sub_key)

        # reference
        sub_type, sub_ref, sub_ref_cls = get_type(sub_prefix, sub_value)
        if sub_ref:
            subclass_prefix = sub_type
            sub_type = 'object'

        # desc
        sub_desc = get_desc(sub_prefix, sub_value)

        if sub_type == 'array':
            cpp_type, sub_key, sub_type, sub_desc, subclass, subclass_prefix = nested_array(sub_prefix, sub_value)
            cpp_type = convert_cpp_type('array', cpp_type)
        elif sub_ref:
            cpp_type = convert_cpp_type('array', sub_ref_cls.cls_name)
        elif sub_type == 'object':
            # sub_key may be ambiguous as class name, using key instead
            sub_cls_name = subclass_name(key)
            cpp_type = convert_cpp_type('array', sub_cls_name)
            sub_cls_content = get_content(sub_prefix, sub_value, sub_type)
            subclass_prefix = sub_prefix
            subclass = (subclass_prefix, sub_cls_name, sub_cls_content)
        else:
            cpp_type = convert_cpp_type('array', convert_native_type(sub_type))

        return cpp_type, sub_key, sub_type, sub_desc, subclass, subclass_prefix

    # check empty
    if not allow_empty and len(content) == 0:
        raise Exception('content is empty.')

    ret_params, ret_subclass = [], []

    for key, detail in content.items():
        prefix = join_prefix(class_prefix, key)

        check_value_type(prefix, detail, dict)

        cpp_type, sub_key, sub_type, sub_desc, subclass, subclass_prefix = None, None, None, None, None, None

        # type
        type, reference, ref_cls = get_type(prefix, detail)
        if reference:
            subclass_prefix = type
            type = 'object'

        # description
        desc = get_desc(prefix, detail)
        # required
        required = get_required(prefix, detail)
        # opt
        opt = get_opt(prefix, detail)
        # default
        default = get_default(prefix, detail, type, required)
        # condition
        condstr, condition = get_condition(prefix, detail, content)

        if is_pod(type):
            cpp_type = convert_cpp_type(type)
        # reference class
        elif reference:
            cpp_type = ref_cls.cls_name
        # subclass
        elif type == 'object':
            cpp_type = sub_cls_name = subclass_name(key)
            sub_cls_content = get_content(prefix, detail, type)
            subclass_prefix = prefix
            ret_subclass.append((subclass_prefix, sub_cls_name, sub_cls_content))
        # array vector<vector<...> >
        elif type == 'array':
            cpp_type, sub_key, sub_type, sub_desc, subclass, subclass_prefix = nested_array(prefix, detail)
            if subclass:
                ret_subclass.append(subclass)

        cpp_name = param_name(key, type)
        ret_params.append(Param(key, type, desc, required, default, opt, cpp_type, cpp_name,
                                condstr, condition, sub_key, sub_type, sub_desc, subclass_prefix))

    # sort params, optional behind required
    def cmp_params(l, r):
        if l.required and not r.required:
            return -1
        elif not l.required and r.required:
            return 1
        else:
            return 0

    ret_params.sort(key=cmp_to_key(cmp_params))

    return ret_params, ret_subclass


# struct
# one param
class Param:
    def __init__(self, key, type, desc, required, default, opt, cpp_type, cpp_name,
                 condstr, condition, sub_key, sub_type, sub_desc, subclass_prefix):
        # key in json
        self.key = key
        # int, bool, array...
        self.type = type
        # CRPCInt64, CRPCBool, CRPCVector<std::string>, CSubclass...
        self.cpp_type = cpp_type
        # variable param name. e.g. nIntparam, fBoolparam, strString, vecVector...
        self.cpp_name = cpp_name
        # value of 'desc' in json
        self.desc = desc
        # value of 'required' in json
        self.required = required
        # value of 'default' in json
        self.default = default
        # value of 'opt' in json
        self.opt = opt
        # (condition origin string)
        self.condstr = condstr
        # (key, value, op, cppname)
        self.condition = condition
        # When type == 'array', it's not None. Array element key likes self.key
        self.sub_key = sub_key
        # When type == 'array', it's not None. Array element type likes self.type
        self.sub_type = sub_type
        # When type == 'array', it's not None. Array element description likes self.desc
        self.sub_desc = sub_desc
        # When type == 'object' or cpp_type is CRPCVector< >, it's not None. Use this to search all_classes
        self.subclass_prefix = subclass_prefix

    def declare(self, w, indent):
        required_tag = '__required__ ' if self.required else '__optional__ '
        w.write(indent + required_tag + self.cpp_type + ' ' + self.cpp_name)
        if self.default != None:
            w.write(' = ' + self.default + ';\n')
        else:
            w.write(';\n')


# one class
class SubClass:
    def __init__(self, prefix, cls_name, content):
        # key path in json
        self.prefix = prefix
        # class name in cpp
        self.cls_name = cls_name
        # origin value of subclass in json
        self.content = content

    def parse(self):
        self.params, subclass_info = parse_params(self.prefix, self.content, False)

        self.subclass = {}
        for sub_prefix, sub_name, sub_content in subclass_info:
            subclass = SubClass(sub_prefix, sub_name, sub_content)
            all_classes[sub_prefix] = self.subclass[sub_prefix] = subclass

        for subclass in self.subclass.values():
            subclass.parse()

    def declare(self, w, indent):
        next_indent = indent + '\t'

        # class top
        w.write(indent + '// class ' + self.cls_name + '\n')
        w.write(indent + 'class ' + self.cls_name + '\n')
        brace_begin(w, indent)

        # sub class
        if len(self.subclass) > 0:
            w.write(indent + 'public:\n')
        for s in self.subclass.values():
            s.declare(w, next_indent)

        # params
        w.write(indent + 'public:\n')
        for p in self.params:
            p.declare(w, next_indent)

        # functions
        w.write(indent + 'public:\n')
        constructor_h(self.cls_name, self.params, w, next_indent)
        constructor_null_h(self.cls_name, self.params, w, next_indent)
        ToJSON_h(False, w, next_indent)
        FromJSON_h(False, self.cls_name, w, next_indent)
        IsValid_h(w, next_indent)

        # class bottom
        w.write(indent + '};\n')

    def implement(self, w, scope):
        cls_name = scope + self.cls_name
        next_scope = cls_name + '::'

        for s in self.subclass.values():
            s.implement(w, next_scope)

        empty_line(w)
        w.write('// ' + cls_name + '\n')
        # functions
        constructor_cpp(self.cls_name, self.params, w, next_scope)
        constructor_null_cpp(self.cls_name, self.params, w, next_scope)
        ToJSON_cpp(self.cls_name, self.params, 'object', w, next_scope)
        FromJSON_cpp(cls_name, cls_name, self.params, 'object', w, next_scope)
        IsValid_cpp(self.cls_name, self.params, w, next_scope)


# one request
class Request:
    def __init__(self, cmd, cls_name, desc, content):
        # key in json
        self.cmd = cmd
        # class name in cpp
        self.cls_name = cls_name
        # value of desc in json
        self.desc = desc
        # origin value of request in json
        self.content = content

    def parse(self):
        prefix = join_prefix(self.cmd, 'request')

        self.type, self.reference, self.ref_cls = get_type(prefix, self.content)
        if is_pod(self.type):
            raise Exception('request type should be object or array')

        name = get_json_value(prefix, self.content, 'name', str, 'data', required=False)
        parsed_content = None
        if self.type == 'object':
            parsed_content = get_content(prefix, self.content, self.type)
        else:
            parsed_content = {name: self.content}

        # parse params
        self.params, subclass_info = parse_params(prefix, parsed_content, True)

        self.subclass = {}
        for sub_prefix, sub_name, sub_content in subclass_info:
            subclass = SubClass(sub_prefix, sub_name, sub_content)
            all_classes[sub_prefix] = self.subclass[sub_prefix] = subclass

        # parse sub class
        for subclass in self.subclass.values():
            subclass.parse()


# one response
class Response:
    def __init__(self, cmd, cls_name, desc, content):
        # key in json
        self.cmd = cmd
        # class name in cpp
        self.cls_name = cls_name
        # value of desc in json
        self.desc = desc
        # origin value of response in json
        self.content = content

    def parse(self):
        prefix = join_prefix(self.cmd, 'response')

        self.type, self.reference, self.ref_cls = get_type(prefix, self.content)

        name = get_json_value(prefix, self.content, 'name', str)
        parsed_content = None
        if self.type == 'object':
            parsed_content = get_content(prefix, self.content, self.type)
        else:
            parsed_content = {name: self.content}

        self.params, subclass_info = parse_params(prefix, parsed_content, True)

        self.subclass = {}
        for sub_prefix, sub_name, sub_content in subclass_info:
            subclass = SubClass(sub_prefix, sub_name, sub_content)
            all_classes[sub_prefix] = self.subclass[sub_prefix] = subclass

        # parse sub class
        for subclass in self.subclass.values():
            subclass.parse()


# one config
class Config:
    def __init__(self, cmd, cls_name, desc, example, error):
        # key in json
        self.cmd = cmd
        # class name in cpp
        self.cls_name = cls_name
        # value of desc in json
        self.desc = desc
        # value of example in json
        self.example = example
        # value of error in json
        self.error = error

    def parse(self, req_type, req_params, req_subclass, resp_type=None, resp_params=None, resp_subclass=None):
        # req.type
        self.req_type = req_type
        # req.params
        self.req_params = req_params
        # req.subclass
        self.req_subclass = req_subclass
        # resp.type
        self.resp_type = resp_type
        # resp.params
        self.resp_params = resp_params
        # resp.subclass
        self.resp_subclass = resp_subclass

        # reference class params
        self.req_sub_params = None
        if self.req_type in alone_classes:
            self.req_sub_params = alone_classes[self.req_type].params
        self.resp_sub_params = None
        if self.resp_type in alone_classes:
            self.resp_sub_params = alone_classes[self.resp_type].params


# main function
def parse():
    # parse mode.json
    if mode_json:
        with open(mode_json, 'r') as r:
            content = json.loads(r.read(), object_pairs_hook=OrderedDict)
            content = json_hook(content)

            for mode, detail in content.items():
                usage = get_json_value(mode, detail, 'usage', str, '')
                desc = get_desc(mode, detail)
                modes[mode] = (usage, desc)
    
    # parse rpc.json
    if rpc_json:
        with open(rpc_json, 'r') as r:
            content = json.loads(r.read(), object_pairs_hook=OrderedDict)
            content = json_hook(content)
            check_value_type(rpc_json, content, dict)

        for cmd, detail in content.items():
            check_value_type(cmd, detail, dict)

            type = get_json_value(cmd, detail, 'type', type=str, default='command')
            name = get_json_value(cmd, detail, 'name', type=str, default=cmd.title())

            # stand-alone class
            if type == 'class':
                class_content = get_content(cmd, detail, 'object')
                alone_class = SubClass(cmd, subclass_name(name, True), class_content)
                all_classes[cmd] = alone_classes[cmd] = alone_class
            # rpc command
            elif type == 'command':
                desc = get_desc(cmd, detail)
                example = get_json_value(cmd, detail, 'example', required=False)
                error = get_json_value(cmd, detail, 'error', required=False)
                name = get_json_value(cmd, detail, 'name', str, default=cmd.title())

                # request
                request_content = get_json_value(cmd, detail, 'request', dict)
                request = Request(cmd, param_class_name(name), desc, request_content)

                param_class[cmd] = param_class_name(name)
                config_class[cmd] = config_class_name(name)

                # response
                response_content = get_json_value(cmd, detail, 'response', dict, required=False)
                response = Response(cmd, result_class_name(name), desc, response_content) if response_content else None

                if response:
                    result_class[cmd] = result_class_name(name)

                # config
                config = Config(cmd, config_class_name(name), desc, example, error)

                commands[cmd] = (request, response, config)

                # introduction
                intro = get_multiple_text(cmd, detail, 'introduction')
                if len(intro) == 0:
                    intro = get_desc(cmd, detail)
                introduction[cmd] = intro
            else:
                raise Exception('Unknown type [%s]' % type)

    # parse stand-alone class
    for subclass in alone_classes.values():
        subclass.parse()

    # parse request, response
    for cmd in commands:
        request, response, config = commands[cmd]

        request.parse()

        if response:
            response.parse()
            config.parse(request.type, request.params, request.subclass,
                         response.type, response.params, response.subclass)
        else:
            config.parse(request.type, request.params, request.subclass)


def generate_protocol_h():
    with open(protocol_h, 'w') as w:
        # copyright
        w.write(copyright)
        # file top
        w.write('''
#ifndef BIGBANG_RPC_AUTO_PROTOCOL_H
#define BIGBANG_RPC_AUTO_PROTOCOL_H

#include <cfloat>
#include <climits>

#include "json/json_spirit_utils.h"

#include "rpc/rpc_type.h"
#include "rpc/rpc_req.h"
#include "rpc/rpc_resp.h"

namespace bigbang
{
namespace rpc
{

#ifdef __required__
#pragma push_macro("__required__")
#define PUSHED_MACRO_REQUIRED
#undef __required__
#endif
#define __required__

#ifdef __optional__
#pragma push_macro("__optional__")
#define PUSHED_MACRO_OPTIONAL
#undef __optional__
#endif
#define __optional__

''')
        # alone_classes
        for subclass in alone_classes.values():
            w.write('class ' + subclass.cls_name + ';\n')

        for subclass in alone_classes.values():
            empty_line(w)
            subclass.declare(w, '')

        # commands
        for cmd in commands:
            request, response, config = commands[cmd]

            # comment
            w.write('''
/////////////////////////////////////////////////////
// ''' + cmd + '''
''')

            # request begin
            empty_line(w)
            w.write('// ' + request.cls_name + '\n')

            w.write('class ' + request.cls_name + ' : public CRPCParam\n')
            indent = brace_begin(w)

            # sub class
            if len(request.subclass) > 0:
                w.write('public:\n')
            for s in request.subclass.values():
                s.declare(w, indent)

            # params
            if len(request.params) > 0:
                w.write('public:\n')
            for p in request.params:
                p.declare(w, indent)

            # functions
            w.write('public:\n')
            constructor_h(request.cls_name, request.params, w, indent)
            destructor_h(request.cls_name, w, indent)
            ToJSON_h(True, w, indent)
            FromJSON_h(True, request.cls_name, w, indent)
            Method_h(True, w, indent)

            # request end
            w.write('};\n')
            MakeSharedPtr_h(request.cls_name, w, '')

            # response begin
            if response:
                empty_line(w)
                w.write('// ' + response.cls_name + '\n')

                w.write('class ' + response.cls_name +
                        ' : public CRPCResult\n')
                indent = brace_begin(w)

                # sub class
                if len(response.subclass) > 0:
                    w.write('public:\n')
                for s in response.subclass.values():
                    s.declare(w, indent)

                # params
                if len(response.params) > 0:
                    w.write('public:\n')
                for p in response.params:
                    p.declare(w, indent)

                # functions
                w.write('public:\n')
                constructor_h(response.cls_name, response.params, w, indent)
                destructor_h(response.cls_name, w, indent)
                ToJSON_h(True, w, indent)
                FromJSON_h(True, response.cls_name, w, indent)
                Method_h(True, w, indent)

                # response end
                w.write('};\n')
                MakeSharedPtr_h(response.cls_name, w, '')

        # function CreateCRPCParam
        empty_line(w)
        CreateCRPCParam_h(w)

        # function CreateCRPCResult
        empty_line(w)
        CreateCRPCResult_h(w)

        # function RPCCmdList
        empty_line(w)
        RPCCmdList_h(w)

        # help tips
        empty_line(w)
        w.write('// help tips used when error occured on client\n')
        w.write('static const std::string strClientHelpTips = "\\nRun \'COMMAND -help\' for more information on a command.\\n";\n')
        w.write('// help tips used when error occured on server\n')
        w.write('static const std::string strServerHelpTips = "\\nRun \'help COMMAND\' for more information on a command.\\n";\n')

        # file bottom
        w.write('''
#undef __required__
#undef __optional__

#ifdef PUSHED_MACRO_REQUIRED
#pragma pop_macro("__required__")
#endif

#ifdef PUSHED_MACRO_OPTIONAL
#pragma pop_macro("__optional__")
#endif

}  // namespace rpc
}  // namespace bigbang

#endif  // BIGBANG_RPC_AUTO_PROTOCOL_H
''')


def generate_protocol_cpp():
    with open(protocol_cpp, 'w') as w:
        # copyright
        w.write(copyright)
        # file top
        w.write('''\
#include "auto_protocol.h"

#include <sstream>
#include <cmath>

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_utils.h"

using namespace std;
using namespace json_spirit;

namespace bigbang
{
namespace rpc
{
''')
        # inner function
        check_json_type_fun(w)
        check_is_valid_fun(w)

        # alone_classes
        for subclass in alone_classes.values():
            subclass.implement(w, '')

        # commands
        for cmd in commands:
            request, response, config = commands[cmd]

            # comment
            w.write('''
/////////////////////////////////////////////////////
// ''' + cmd + '''
''')

            # request begin
            scope = request.cls_name + '::'

            # request sub class
            for s in request.subclass.values():
                s.implement(w, scope)

            # request functions
            w.write('\n// ' + request.cls_name + '\n')
            constructor_cpp(request.cls_name, request.params, w, scope)
            ToJSON_cpp(request.cls_name, request.params, request.type, w, scope)
            FromJSON_cpp(request.cmd, request.cls_name, request.params, request.type, w, scope)
            Method_cpp(request.cmd, w, scope)

            # response begin
            if response:
                scope = response.cls_name + '::'

                # request sub class
                for s in response.subclass.values():
                    s.implement(w, scope)

                # response functions
                w.write('\n// ' + response.cls_name + '\n')
                constructor_cpp(response.cls_name, response.params, w, scope)
                ToJSON_cpp(response.cls_name, response.params, response.type, w, scope)
                FromJSON_cpp(response.cmd, response.cls_name, response.params, response.type, w, scope)
                Method_cpp(response.cmd, w, scope)

        # function CreateCRPCParam
        empty_line(w)
        CreateCRPCParam_cpp(w)

        # function CreateCRPCResult
        empty_line(w)
        CreateCRPCResult_cpp(w)

        # function RPCCmdList
        empty_line(w)
        RPCCmdList_cpp(w)

        # file bottom
        w.write('''
}  // namespace rpc
}  // namespace bigbang

''')


def generate_config_h():
    with open(config_h, 'w') as w:
        # copyright
        w.write(copyright)
        # file top
        w.write('''
#ifndef BIGBANG_MODE_AUTO_CONFIG_H
#define BIGBANG_MODE_AUTO_CONFIG_H

#include "mode/mode_impl.h"
#include "mode/mode_type.h"
#include "rpc/auto_protocol.h"

namespace bigbang
{

''')
        # commands
        for cmd in commands:
            request, response, config = commands[cmd]

            # comment
            w.write('''
/////////////////////////////////////////////////////
// ''' + cmd + '''
''')

            # config begin
            empty_line(w)
            w.write('// ' + config.cls_name + '\n')

            w.write('class ' + config.cls_name + ' : virtual public CBasicConfig, public rpc::' + request.cls_name + '\n')
            indent = brace_begin(w)

            # functions
            w.write('public:\n')
            config_constructor_h(config.cls_name, w, indent)
            PostLoad_h(w, indent)
            ListConfig_h(w, indent)
            Help_h(w, indent)

            # config end
            w.write('};\n')

        # function Help
        empty_line(w)
        RPCHelp_h(w)

        # function CreateConfig
        empty_line(w)
        CreateConfig_h(w)

        # file bottom
        w.write('''
}  // namespace bigbang

#endif  // BIGBANG_MODE_AUTO_CONFIG_H
''')


def generate_config_cpp():
    with open(config_cpp, 'w') as w:
        # copyright
        w.write(copyright)
        # file top
        w.write('''\
#include "auto_config.h"

#include <sstream>

#include "json/json_spirit_reader_template.h"

using namespace std;
using namespace bigbang::rpc;
using namespace json_spirit;

namespace bigbang
{
''')
        # commands
        for cmd in commands:
            request, response, config = commands[cmd]

            # comment
            w.write('''
/////////////////////////////////////////////////////
// ''' + cmd + '''
''')

            # config begin
            scope = config.cls_name + '::'

            # config functions
            w.write('\n// ' + config.cls_name + '\n')
            config_constructor_cpp(config.cls_name, config.req_params, w, scope)
            PostLoad_cpp(config.cls_name, config.req_params, config.req_sub_params, w, scope)
            ListConfig_cpp(config.cls_name, config.req_params, config.req_sub_params, w, scope)
            Help_cpp(config, w, scope)

        # function Help
        empty_line(w)
        RPCHelp_cpp(w)
        
        # file bottom
        w.write('''
}  // namespace bigbang

''')

def generate_protocol(rpc_json_path, mode_json_path, protocol_h_path, protocol_cpp_path, config_h_path, config_cpp_path):
    global rpc_json, mode_json, protocol_h, protocol_cpp, config_h, config_cpp
    rpc_json = rpc_json_path
    mode_json = mode_json_path
    protocol_h = protocol_h_path
    protocol_cpp = protocol_cpp_path
    config_h = config_h_path
    config_cpp = config_cpp_path

    if not rpc_json or ((not protocol_h or not protocol_cpp) and (not config_h or not config_cpp)):
        return

    # parse json file
    parse()

    # generate auto_protocol
    if protocol_h and protocol_cpp:
        generate_protocol_h()
        generate_protocol_cpp()

    # generate auto_config
    if config_h and config_cpp:
        generate_config_h()
        generate_config_cpp()
