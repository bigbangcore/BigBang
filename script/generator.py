#!/usr/bin/env python
# -*-coding:utf-8-*-

import sys
sys.dont_write_bytecode = True

import os
import auto_options as options
import auto_protocol as protocol
import auto_rpc as rpc
from tool import *


def create_dir(dir):
    if not os.path.exists(dir):
        os.makedirs(dir)

def panic():
    print('[ERROR] arguments error!')
    print('usage: generator.py ["all"|"rpc"|"mode"] [output directory]')
    exit(1)


if len(sys.argv) < 3:
    panic()

# script dictionary
root = os.path.dirname(__file__)

# json files dictionary
json_root = os.path.abspath(os.path.join(root, 'template'))

# generating modules
modules = sys.argv[1]
if modules != 'all' and modules != 'rpc' and modules != 'mode':
    panic()

# cpp files dictionary
cpp_src_root = sys.argv[2]
if not os.path.isabs(cpp_src_root):
    cpp_src_root = os.path.join(os.getcwd(), cpp_src_root)

if modules == 'all' or modules == 'mode':
    create_dir(os.path.join(cpp_src_root, 'mode'))

if modules == 'all' or modules == 'rpc':
    create_dir(os.path.join(cpp_src_root, 'rpc'))

# json file name
json_files = {
    'options_json': 'options.json',
    'rpc_json': 'rpc.json',
    'mode_json': 'mode.json',
}

# cpp file name
mode_cpp_files = {
    'options_h': 'mode/auto_options.h',
    'config_h': 'mode/auto_config.h',
    'config_cpp': 'mode/auto_config.cpp',
}
rpc_cpp_files = {
    'protocol_h': 'rpc/auto_protocol.h',
    'protocol_cpp': 'rpc/auto_protocol.cpp',
}


def generate():
    # json files path
    for k, v in json_files.items():
        json_files[k] = os.path.join(json_root, v)

    # cpp files path
    cpp_files = {}
    for k, v in mode_cpp_files.items():
        if modules == 'all' or modules == 'mode':
            cpp_files[k] = os.path.join(cpp_src_root, v)
        else:
            cpp_files[k] = None

    for k, v in rpc_cpp_files.items():
        if modules == 'all' or modules == 'rpc':
            cpp_files[k] = os.path.join(cpp_src_root, v)
        else:
            cpp_files[k] = None

    options.generate_options(json_files['options_json'], cpp_files['options_h'])
    protocol.generate_protocol(json_files['rpc_json'], json_files['mode_json'], cpp_files['protocol_h'],
                               cpp_files['protocol_cpp'], cpp_files['config_h'], cpp_files['config_cpp'])
    # rpc.generate_rpc(json_files['mode_json'], json_files['rpc_json'], cpp_files['rpc_h'], cpp_files['rpc_cpp'])


if __name__ == '__main__':
    generate()
