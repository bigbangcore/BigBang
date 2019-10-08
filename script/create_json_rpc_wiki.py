#!/usr/bin/env python
#-*-conding:utf-8-*-

import sys
sys.dont_write_bytecode = True

import commands
import os
import re

# root = os.path.dirname(__file__)
bigbang_cli = 'bigbang-cli'

simple_help = bigbang_cli + ' -help'
detail_help = bigbang_cli + ' all -help'

def create_wiki(filename = None):
    # output value
    output_format, output_index, output_commands = '', '', ''

    # get summary help info
    return_code, simple = commands.getstatusoutput(simple_help)
    if return_code != 0:
        raise Exception('Execute fail: [%s]' % simple_help)
    # get detail help info
    return_code, detail = commands.getstatusoutput(detail_help)
    if return_code != 0:
        raise Exception('Execute fail: [%s]' % detail_help)

    # split simple help to cmd_format and command summary
    match = re.match(r'(.*?)\n*Commands:\n(.*)', simple, re.S)
    if not match:
        raise Exception('parse summary fail')
    output_format, command_summary = match.groups()

    # compose output_format
    output_format = '### command line format:\n```' + output_format + '\n```\n'

    # split command summary to command and introduction
    command_intro = []
    command_lines = command_summary.split('\n')
    for line in command_lines:
        match = re.search(r'\s*(\S+)\s*(.*)', line)
        # empty line
        if not match:
            continue

        cmd, intro = match.groups()
        command_intro.append((cmd, intro))
    
    # compose output_index
    output_index = '## commands\n'
    for cmd, intro in command_intro:
        if cmd == 'help':
            output_index = output_index + '### System\n'
        elif cmd == 'getpeercount':
            output_index = output_index + '### Network\n'
        elif cmd == 'getforkcount':
            output_index = output_index + '### WorldLine & TxPool\n'
        elif cmd == 'listkey':
            output_index = output_index + '### Wallet\n'
        elif cmd == 'verifymessage':
            output_index = output_index + '### Util\n'
        elif cmd == 'getwork':
            output_index = output_index + '### Mint\n'
        
        output_index = output_index + ' - [%s](#%s): %s' % (cmd, cmd, intro) + '\n'
    output_index = output_index + '---\n'

    # split detail to command list
    command_list = []
    begin = detail.find('Usage:')
    while begin >= 0:
        end = detail.find('Usage:', begin + 1)
        if end >= 0:
            command_list.append(detail[begin:end])
        else:
            command_list.append(detail[begin:])
        begin = end
    
    for index in range(len(command_list)):
        c = command_list[index]

        # Usage
        c = re.sub(r'Usage:(.*?)\n*(?=Arguments:)', '**Usage:**\n```\g<1>\n```\n', c, flags=re.S)

        # Arguments
        c = re.sub(r'Arguments:(.*?)\n*(?=Request:)', '**Arguments:**\n```\g<1>\n```\n', c, flags=re.S)

        # Request
        c = re.sub(r'Request:(.*?)\n*(?=Response:)', '**Request:**\n```\g<1>\n```\n', c, flags=re.S)

        # Response
        c = re.sub(r'Response:(.*?)\n*(?=Examples:)', '**Response:**\n```\g<1>\n```\n', c, flags=re.S)

        # Examples
        c = re.sub(r'Examples:(.*?)\n*(?=Errors:)', '**Examples:**\n```\g<1>\n```\n', c, flags=re.S)

        # Errors
        c = re.sub(r'Errors:(.*?)\n*$', '**Errors:**\n```\g<1>\n```\n', c, flags=re.S)

        # add tail
        c = c + '##### [Back to top](#commands)\n---\n'

        # add command head
        cmd, _ = command_intro[index]
        command_list[index] = '### %s\n' % cmd + c
    output_commands = ''.join(command_list)

    print output_format
    print output_index
    print output_commands
    

if __name__ == '__main__':
    create_wiki()
