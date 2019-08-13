# -*-conding:utf-8-*-
import sys
sys.dont_write_bytecode = True

import math
import re
import os
from collections import OrderedDict

type_f = type

# copyright on .h file
copyright = \
    '''\
// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
'''

# attempt one tab equals how many blanks on terminal
tab_len = 8
# set max length of one line to show on terminal
# Temporarily not break line now (system terminal will be auto line break, macosx, ubuntu)
max_line_len = -1
# set max "format" length of one line to show on terminal
max_format_len = 40
# increasing step length, when format length is not enough
step_len = 20 


# esacpe \t to space
def tab_to_space(s):
    return s.replace('\t', ' ' * tab_len)


# various indent on showing to terminal
usage_indent = tab_to_space('\t')
desc_indent = tab_to_space('')
options_indent = tab_to_space('  ')
commands_indent = tab_to_space('  ')
argument_indent = tab_to_space(' ')
sub_argument_indent = tab_to_space('  ')
summary_indent = None
example_req_indent = tab_to_space('>> ')
example_resp_indent = tab_to_space('<< ')
error_indent = tab_to_space('* ')


def is_str(s):
    return isinstance(s, str) or isinstance(s, str)


# length of esacpe \t to space
def tab_to_space_len(s):
    return len(tab_to_space(s))


# compute step length
def step(s, length):
    if length < len(s):
        length = length + ((len(s) - length) / step_len + 1) * step_len
    return length


# compute how many tabs append string by max length
def space(s, indent=None):
    if indent == None:
        indent = ' ' * max_format_len
    else:
        indent = tab_to_space(indent)

    indent_len = step(s, len(indent))

    return ' ' * int(indent_len - len(s))


# split string to multiple line by indent and max_line_len
def split(s, indent=None, max_len=None):
    def first_char_index(s, pos=0):
        reg = re.compile(r'(\S)')
        match = reg.search(s, pos)
        if match:
            return match.span()[0]
        return len(s)

    if indent == None:
        indent = ' ' * max_format_len
    else:
        indent = tab_to_space(indent)

    if max_len == None:
        max_len = max_line_len
    if max_len == -1:
        max_len = len(indent) + len(s)

    line_len = step(indent, max_len)
    line_len = line_len - len(indent)

    lines = []
    begin, end = first_char_index(s), line_len
    while begin < len(s):
        # if s contains '\n', wrap there. Or wrap the next ' ' behind end
        enter = s.find('\n', begin, end)
        if enter > 0:
            end = enter + 1
        else:
            blank = s.find(' ', end - 1)
            comma = s.find(',', end - 1)
            colon = s.find(':', end - 1)
            min_end = min(list(filter(lambda x: x >= 0, [blank, comma, colon])) + [len(s) - 1])
            end = min_end + 1

        line = s[begin:end]
        if begin != 0:
            line = indent + line

        if enter > 0:
            lines.append(line)
        else:
            lines.append(line + '\n')

        begin = first_char_index(s, end)
        end = begin + line_len

    if len(lines) == 0:
        lines.append('\n')
    return lines


# escape for c++ string content
def escape(s):
    return s.replace('"', '\\"').replace('\n', '\\n').replace('\t', '\\t')


# write an { and return new indent
def brace_begin(w, indent=''):
    w.write(indent + '{\n')
    return indent + '\t'


# write an } and return new indent
def brace_end(w, indent=''):
    indent = indent[:-1]
    w.write(indent + '}\n')
    return indent


# write an empty line
def empty_line(w):
    w.write('\n')


# return param class name
def param_class_name(name):
    return 'C' + name + 'Param'


# return result class name
def result_class_name(name):
    return 'C' + name + 'Result'


# return config class name
def config_class_name(name):
    return 'C' + name + 'Config'


# join prefix
def join_prefix(*args):
    return '-'.join(args)


# check a value' type
def check_value_type(prefix, value, type):
    if not isinstance(value, type):
        raise Exception('[%s] type wrong. [%s] is needed, but actually is [%s]' % (prefix, type, type_f(value)))


# get a json value.
def get_json_value(prefix, json, key, type=None, default=None, required=True):
    value = None
    if not key in json:
        if default == None and required:
            raise Exception('[%s] no key in json. key: [%s] ' % (prefix, key))
        value = default
    else:
        value = json[key]

    if type and value != None:
        check_value_type(join_prefix(prefix, key), value, type)

    return value


# get string text or array text. If it's an array, join them.
def get_multiple_text(prefix, json, key):
    desc = get_json_value(prefix, json, key, default='')
    if isinstance(desc, list):
        desc = '\n'.join(desc)
    check_value_type(join_prefix(prefix, key), desc, str)
    return desc


# get "desc" field. If it's an array, join them.
def get_desc(prefix, json):
    return get_multiple_text(prefix, json, 'desc')


# string to "string", others to string type
def quote(o, type='string'):
    if type == 'string':
        return '"%s"' % o
    elif type == 'bool':
        return str(o).lower()
    else:
        return str(o)

# indent + prefix + "name + desc";
# or
# indent + prefix + "name + desc1"
#                      "desc2"
#                      "desc3";


def terminal_str_code(indent, prefix, name, split_list):
    if not split_list or len(split_list) == 0:
        return indent + prefix + quote(escape(name)) + ';\n'

    first = indent + prefix + quote(escape(name + split_list[0]))
    new_list = [first] + list(map(lambda x: quote(escape(x)), split_list[1:]))
    line_space = '\n' + indent + (' ' * len(prefix))
    return line_space.join(new_list) + ';\n'


# write 'Usage'
def write_usage(usage, w, indent):
    w.write(indent + 'oss << "\\nUsage:\\n";\n')
    usage_list = split(usage, usage_indent)
    usage_code = terminal_str_code(indent, 'oss << ', usage_indent, usage_list)
    w.write(usage_code)
    w.write(indent + 'oss << "\\n";\n')

# write 'Desc'


def write_desc(desc, w, indent):
    if desc:
        desc_list = split(desc, desc_indent)
        desc_code = terminal_str_code(indent, 'oss << ', desc_indent, desc_list)
        w.write(desc_code)
        w.write(indent + 'oss << "\\n";\n')

# write 'Arguments', 'Requist', 'Result' ...


def write_chapter(infos, w, indent):
    if len(infos) > 0:
        for info_fmt, info_list in infos:
            cpp_code = terminal_str_code(indent, 'oss << ', info_fmt, info_list)
            w.write(cpp_code)
        w.write(indent + 'oss << "\\n";\n')
    else:
        w.write(indent + 'oss << "\\tnone\\n\\n";\n')

def json_hook(data):
    if sys.version < '3' and isinstance(data, unicode):
        return data.encode('utf-8')
    if isinstance(data, list):
        return [ json_hook(item) for item in data ]
    if isinstance(data, dict):
        return OrderedDict([
            (json_hook(key), json_hook(value))
            for key, value in data.items()
        ])
        
    return data
