#!/usr/bin/env python

import time
import requests
import json
from collections import OrderedDict
import os
import sys

rpcurl = 'http://127.0.0.1:9902'

genesis_privkey = '9df809804369829983150491d1086b99f6493356f91ccc080e661a76a976a4ee'
genesis_addr = '1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm'
dpos_privkey = '9f1e445c2a8e74fabbb7c53e31323b2316112990078cbd8d27b2cd7100a1648d'
dpos_pubkey = 'fe8455584d820639d140dad74d2644d742616ae2433e61c0423ec350c2226b78'
password = '123'


# RPC HTTP request
def call(body):
    req = requests.post(rpcurl, json=body)

    print('DEBUG: request: {}'.format(body))
    print('DEBUG: response: {}'.format(req.content))

    resp = json.loads(req.content.decode('utf-8'))
    return resp.get('result'), resp.get('error')


# RPC: makekeypair
def makekeypair():
    result, error = call({
        'id': 0,
        'jsonrpc': '1.0',
        'method': 'makekeypair',
        'params': {}
    })

    if result:
        pubkey = result.get('pubkey')
        # print('makekeypair success, pubkey: {}'.format(pubkey))
        return pubkey
    else:
        raise Exception('makekeypair error: {}'.format(error))


# RPC: getnewkey
def getnewkey():
    result, error = call({
        'id': 0,
        'jsonrpc': '1.0',
        'method': 'getnewkey',
        'params': {
            'passphrase': password
        }
    })

    if result:
        pubkey = result
        # print('getnewkey success, pubkey: {}'.format(pubkey))
        return pubkey
    else:
        raise Exception('getnewkey error: {}'.format(error))


# RPC: getpubkeyaddress
def getpubkeyaddress(pubkey):
    result, error = call({
        'id': 0,
        'jsonrpc': '1.0',
        'method': 'getpubkeyaddress',
        'params': {
            "pubkey": pubkey
        }
    })

    if result:
        address = result
        # print('getpubkeyaddress success, address: {}'.format(address))
        return address
    else:
        raise Exception('getpubkeyaddress error: {}'.format(error))


# RPC: importprivkey
def importprivkey(privkey):
    result, error = call({
        'id': 0,
        'jsonrpc': '1.0',
        'method': 'importprivkey',
        'params': {
            "privkey": privkey,
            'passphrase': password
        }
    })

    if result:
        pubkey = result
        # print('importprivkey success, pubkey: {}'.format(pubkey))
        return pubkey
    else:
        raise Exception('importprivkey error: {}'.format(error))


# RPC: getbalance
def getbalance(addr, forkid=None):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'getbalance',
        'params': {
            'address': addr,
            'fork': forkid
        }
    })

    if result and len(result) == 1:
        avail = result[0].get('avail')
        # print('getbalance success, avail: {}'.format(avail))
        return avail
    else:
        raise Exception('getbalance error: {}'.format(error))


# RPC: unlockkey
def unlockkey(key):
    call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'unlockkey',
        'params': {
            'pubkey': key,
            'passphrase': password
        }
    })


# RPC: sendfrom
def sendfrom(from_addr, to, amount, fork=None, type=0, data=None):
    unlockkey(from_addr)

    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'sendfrom',
        'params': {
            'from': from_addr,
            'to': to,
            'amount': amount,
            'fork': fork,
            'type': type,
            'data': data
        }
    })

    if result:
        txid = result
        # print('sendfrom success, txid: {}'.format(txid))
        return txid
    else:
        raise Exception('sendfrom error: {}'.format(error))


# RPC: makeorigin
def makeorigin(prev, owner, amount, name, symbol, reward, halvecycle, forktype=None, defi=None):
    unlockkey(owner)

    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'makeorigin',
        'params': {
            'prev': prev,
            'owner': owner,
            'amount': amount,
            'name': name,
            'symbol': symbol,
            'reward': reward,
            'halvecycle': halvecycle,
            'forktype': forktype,
            'defi': defi,
        }
    })

    if result:
        forkid = result.get('hash')
        data = result.get('hex')
        # print('makeorigin success, forkid: {}, data: {}'.format(forkid, data))
        return forkid, data
    else:
        print(error)
        raise Exception('makeorgin error: {}'.format(error))


# RPC: addnewtemplate fork
def addforktemplate(redeem, forkid):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'addnewtemplate',
        'params': {
            'type': 'fork',
            'fork': {
                'redeem': redeem,
                'fork': forkid,
            }
        }
    })

    if result:
        addr = result
        # print('addforktemplate success, template address: {}'.format(addr))
        return addr
    else:
        raise Exception('addforktemplate error: {}'.format(error))


# RPC: addnewtemplate delegate
def adddelegatetemplate(delegate, owner):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'addnewtemplate',
        'params': {
            'type': 'delegate',
            'delegate': {
                'delegate': delegate,
                'owner': owner,
            }
        }
    })

    if result:
        addr = result
        # print('adddelegatetemplate success, template address: {}'.format(addr))
        return addr
    else:
        raise Exception('adddelegatetemplate error: {}'.format(error))


# RPC: getforkheight
def getforkheight(forkid=None):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'getforkheight',
        'params': {
            'fork': forkid,
        }
    })

    if result:
        height = result
        # print('getforkheight success, height: {}'.format(height))
        return height
    else:
        return None


# RPC: getblockhash
def getblockhash(height, forkid=None):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'getblockhash',
        'params': {
            'height': height,
            'fork': forkid,
        }
    })

    if result:
        block_hash = result
        # print('getblockhash success, block hash: {}'.format(block_hash))
        return block_hash
    else:
        return None


# RPC: getblock
def getblock(blockid):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'getblock',
        'params': {
            'block': blockid,
        }
    })

    if result:
        block = result
        # print('getblock success, block: {}'.format(block))
        return block
    else:
        raise Exception('getblock error: {}'.format(error))


# RPC: gettransaction
def gettransaction(txid):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'gettransaction',
        'params': {
            'txid': txid,
        }
    })

    if result:
        tx = result['transaction']
        # print('gettransaction success, tx: {}'.format(tx))
        return tx
    else:
        raise Exception('gettransaction error: {}'.format(error))


# RPC: getgenealogy
def getgenealogy(forkid):
    result, _ = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'getgenealogy',
        'params': {
            'fork': forkid,
        }
    })

    if result:
        return True
    else:
        return False


# create dpos node
def dpos():
    # import genesis key
    genesis_pubkey = importprivkey(genesis_privkey)

    # check genesis addr balance
    addr = getpubkeyaddress(genesis_pubkey)
    if genesis_addr != getpubkeyaddress(genesis_pubkey):
        raise Exception(
            'genesis addr [{}] is not equal {}'.format(addr, genesis_addr))

    genesis_balance = getbalance(genesis_addr)
    if genesis_balance <= 0:
        raise Exception('No genesis balance: {}'.format(genesis_balance))

    # create delegate
    delegate_addr = adddelegatetemplate(dpos_pubkey, genesis_addr)
    sendfrom(genesis_addr, delegate_addr, 280000000)
    print('Create dpos node success')


# create fork
def create_fork(prev, amount, name, symbol, defi):
    prev = getblockhash(0)[0]
    forkid, data = makeorigin(
        prev, genesis_addr, amount, name, symbol, 0, 0, 'defi', defi)

    fork_addr = addforktemplate(genesis_addr, forkid)
    sendfrom(genesis_addr, fork_addr, 100000, None, 0, data)
    print('Create fork success, forkid: {}'.format(forkid))
    return forkid


if __name__ == "__main__":
    # json path
    if len(sys.argv) < 2:
        raise Exception('No json file')

    path = os.path.join(os.getcwd(), sys.argv[1])

    input = {}
    output = []
    # load json
    with open(path, 'r') as r:
        content = json.loads(r.read())
        input = content["input"]
        output = content["output"]

    # compute balance by stake and relation
    addrset = {}
    for addr, stake in input['stake'].items():
        addrset[addr] = {
            'stake': stake,
            'upper': None,
            'lower': []
        }

    for lower_addr, upper_addr in input['relation'].items():
        if lower_addr not in addrset:
            addrset[lower_addr] = {
                'stake': 0,
                'upper': None,
                'lower': []
            }

        if upper_addr not in addrset:
            addrset[upper_addr] = {
                'stake': 0,
                'upper': None,
                'lower': []
            }

        addrset[lower_addr]['upper'] = upper_addr
        addrset[upper_addr]['lower'].append(lower_addr)

    for addr, obj in addrset.items():
        obj['stake'] = obj['stake'] - \
            (0.01 if obj['upper'] else 0) + 0.02 * len(obj['lower'])

    # import priv key
    pubkey_addrset = {}
    for privkey in input['privkey']:
        if privkey == genesis_privkey:
            pubkey_addrset[genesis_addr] = True
        else:
            pubkey = importprivkey(privkey)
            pubkey_addr = getpubkeyaddress(pubkey)
            pubkey_addrset[pubkey_addr] = True

    # check address in 'stake', 'relation' of input and output
    for addr in addrset:
        if (addr != genesis_addr) and (addr not in pubkey_addrset):
            raise Exception(
                'no privkey of address in "stake" or "relation". address:', addr)

    for result in output:
        for r in result['reward']:
            if (r != genesis_addr) and (r not in pubkey_addrset):
                raise Exception(
                    'no privkey of address in "reward". address:', r)

    # delegate dpos
    dpos()

    # create fork
    if 'makeorigin' not in input:
        raise Exception('Can not create fork, no "makeorigin" in input')

    fork = input['makeorigin']
    forkid = create_fork(getblockhash(0), fork['amount'],
                         fork['name'], fork['symbol'], fork['defi'])

    # wait fork
    while True:
        print("Waitting fork...")
        if getgenealogy(forkid):
            break
        time.sleep(10)

    time.sleep(10)
    # send token to addrset
    for addr, obj in addrset.items():
        if addr != genesis_addr:
            sendfrom(genesis_addr, addr, obj['stake'], forkid)

    # send relation tx
    for addr, obj in addrset.items():
        for lower_addr in obj['lower']:
            sendfrom(addr, lower_addr, 0.01, forkid, 2)

    # mint height & reward cycle
    mint_height = 2 if ('mintheight' not in fork['defi']) or (
        fork['defi'] == -1) else fork['defi']['mintheight']
    reward_cycle = fork['defi']['rewardcycle']

    # check reward
    for result in output:
        height = result['height']
        reward = result['reward']
        print('Checking height [{}]...'.format(height))

        if ((height - mint_height) % reward_cycle != 0):
            print('ERROR: height [{}] is not a reward height'.format(height))
            continue

        # query reward tx beginning with height
        h = height
        while True:
            blockids = None
            # loop to query the block of height
            while True:
                print('Waiting block of height: {}...'.format(h))
                blockids = getblockhash(h, forkid)
                if blockids:
                    break
                print('fork height is {}'.format(getforkheight(forkid)))
                time.sleep(60)
            print('blockids of height: {}, blockids: {}'.format(height, blockids))

            error = False
            # loop to get every block of blockid array
            for hash in blockids:
                block = getblock(hash)

                # if block is vacant, continue to get next height block
                if block['type'] == 'vacant':
                    h = h + 1
                    break

                # if no tx in block, print error
                if len(block['tx']) == 0:
                    print('ERROR: no reward tx in height: {}'.format(height))

                # loop to check every tx in block
                for txid in block['tx']:
                    tx = gettransaction(txid)

                    if len(reward) == 0:
                        break

                    # miss reward tx in height
                    if tx['type'] != 'defi-reward':
                        print('ERROR: miss reward tx in height: {}, left reward: {}'.format(
                            height, reward))
                        reward, error = {}, True
                        break

                    anchor = int(tx['anchor'][:8], 16)
                    # previous sectin reward tx
                    if anchor < height - 1:
                        continue
                    # next sectin reward tx
                    elif anchor > height - 1:
                        print('ERROR: no reward tx in height: {}, left reward: {}'.format(
                            height, reward))
                        reward, error = {}, True
                        break

                    # check the reward of address correct or not
                    if tx['sendto'] in reward:
                        if reward[tx['sendto']] != tx['amount'] + tx['txfee']:
                            print('ERROR: addr reward error in height, addr: {}, height: {} should be: {}, actrual: {}'.format(
                                tx['sendto'], height, reward[tx['sendto']], tx['amount'] + tx['txfee']))
                            error = True
                        del(reward[tx['sendto']])

                if len(reward) == 0:
                    break

            if error:
                print('Checking failed height: {}'.format(height))
                break
            elif len(reward) == 0:
                print('Checking success height: {}'.format(height))
                break
