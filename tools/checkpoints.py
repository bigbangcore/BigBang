#!/usr/bin/env python

# -*- coding: utf-8 -*-

# This tool refer to btcd checkpoint candidate tool
# https://github.com/bigbangcore/BigBang/issues/343

import json
import subprocess

# btcd confirmations is 2016, but we could reduce from 2016 to 1000 confirms,
# cause we cannot compared with BTC' Hash Rate in the main network


def Constant_CheckPointConfirmations():
    return 1000


def get_block_hash(height):
    json_str = str(subprocess.check_output(
        ['bigbang', 'getblockhash', str(height)]))

    block_hash_list = json.loads(json_str)
    if len(block_hash_list) > 1:
        return False, ''
    else:
        return True, block_hash_list[0]


def get_block(block_hash):
    json_str = subprocess.check_output(['bigbang', 'getblock', block_hash])
    return json.loads(json_str)


def get_primary_fork_height():
    return int(subprocess.check_output(['bigbang', 'getforkheight']))


def get_block_type(block_dict):
    return block_dict['type']


def get_block_height(block_dict):
    return block_dict['height']


def get_block_timestamp(block_dict):
    return block_dict['time']


def is_primary_block(block_type):
    if "primary-" in str(block_type):
        return True
    else:
        return False


def main():

    # Get (last height - confirms) from best primary chain
    end_height = get_primary_fork_height() - Constant_CheckPointConfirmations()

    # block at least have 1000 confirms
    height_list = [i for i in range(end_height)]
    # gensis block timestamp
    prev_block_timestamp = 1575043200

    print 'check point candidate list here: '

    for height in height_list:

        is_success, block_hash = get_block_hash(height)
        if not is_success:
            continue

        block_dict = get_block(block_hash)
        block_type = get_block_type(block_dict)
        if is_primary_block(block_type):
            block_height = get_block_height(block_dict)
            if block_height != height or block_height == 0:
                continue
            current_block_timestamp = get_block_timestamp(block_dict)
            if(current_block_timestamp > prev_block_timestamp):
                checkpoint = (block_height, block_hash)
                print checkpoint
                prev_block_timestamp = current_block_timestamp


if __name__ == '__main__':
    main()
