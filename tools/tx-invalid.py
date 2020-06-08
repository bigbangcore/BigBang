#!/usr/bin/env python

# -*- coding: utf-8 -*-

import json
import subprocess
import sys

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

def get_transaction(tx_hash):
    json_str = subprocess.check_output(['bigbang', 'gettransaction', tx_hash])
    json_obj = json.loads(json_str)
    json_obj['transaction']

def get_primary_fork_height():
    return int(subprocess.check_output(['bigbang', 'getforkheight']))


def get_block_type(block_dict):
    return block_dict['type']


def get_block_height(block_dict):
    return block_dict['height']

def get_block_vtx(block_dict):
    return block_dict['tx']

def get_block_minttx(block_dict):
    return block_dict['txmint']

def is_primary_block(block_type):
    if "primary-" in str(block_type):
        return True
    else:
        return False


def main():

    # Get last height from best primary chain
    end_height = get_primary_fork_height()

    height_list = [i for i in range(int(sys.argv[1]),end_height)]

    for height in height_list:

        is_success, block_hash = get_block_hash(height)
        if not is_success:
            continue

        print "Current check height is: %d" % height
        
        block_dict = get_block(block_hash)
        block_type = get_block_type(block_dict)
        if is_primary_block(block_type):
            
            mint_tx  = get_block_minttx(block_dict) 
            try:
                get_transaction(mint_tx)
            except KeyError:
                print "mint tx index invalid: %s, height: %d" % (mint_tx, height)

            vtx_list = get_block_vtx(block_dict)
            for txid in vtx_list:
                try:
                    get_transaction(txid)
                except KeyError:
                    print "tx index invalid: %s, height: %d" % (txid, height)

if __name__ == '__main__':
    main()
