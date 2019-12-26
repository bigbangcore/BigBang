#!/usr/bin/env python

# -*- coding: utf-8 -*-

# This tool refer to btcd checkpoint candidate tool
# https://github.com/bigbangcore/BigBang/issues/343

import subprocess
import json

# btcd confirmations is 2016, but we could reduce from 2016 to 1000 confirms,
# cause we cannot compared with BTC' Hash Rate in the main network


def Constant_CheckPointConfirmations():
    return 1000


end_height = int(subprocess.check_output(['bigbang', 'getforkheight']))

height_list = [i for i in range(
    end_height - Constant_CheckPointConfirmations())]
# gensis block timestamp
prev_block_timestamp = 1575043200

print 'check point candidate list here: '

for height in height_list:
    json_str = str(subprocess.check_output(
        ['bigbang', 'getblockhash', str(height)]))

    block_hash_list = json.loads(json_str)
    if len(block_hash_list) > 1:
        continue

    block_hash = block_hash_list[0]

    json_str = subprocess.check_output(['bigbang', 'getblock', block_hash])
    block_dict = json.loads(json_str)
    block_type = block_dict['type']
    if "primary-" in str(block_type):
        block_height = block_dict['height']
        if block_height != height:
            continue
        if block_height == 0:
            continue
        current_block_timestamp = block_dict['time']
        if(current_block_timestamp > prev_block_timestamp):
            checkpoint = (block_height, block_hash)
            print checkpoint
            prev_block_timestamp = current_block_timestamp
