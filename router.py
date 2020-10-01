#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 BIFIT Mitigator Team <info@mitigator.ru>

import argparse
import ipaddress
import struct
import sys


RESULTS = {'pass': 0, 'drop': 1, 'back': 2, 'limit': 3}
PADDING = '0000'


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--def-result', '-d', default='pass', metavar='DEF',
            help=f'default result ({"|".join(RESULTS.keys())})')
    parser.add_argument('--route', '-r', action='append',
            help='route as 192.0.2.0/24=01:02:03:04:05:06')
    parser.add_argument('--comment', '-c', action='store_true',
            help='add comments')
    return parser.parse_args()


def pack_route(route):
    ip_mask, next_hop = route.split('=')
    ip_mask = ipaddress.IPv4Network(ip_mask, strict=False)
    network = ip_mask.network_address
    mask = ip_mask.netmask
    next_hop = bytes([int(octet, 16) for octet in next_hop.split(':')])
    return (network.packed, mask.packed, next_hop)


if __name__ == '__main__':
    options = parse_args()
    result = RESULTS[options.def_result]
    result = struct.pack('<I', result)
    count = len(options.route)
    count_bytes = struct.pack('<I', count)
    routes = map(pack_route, options.route)

    comments = f' # {options.def_result}' if options.comment else ''
    print(result.hex(), comments)

    comments = f' # {count}' if options.comment else ''
    print(count_bytes.hex(), comments)

    for packed, source in zip(routes, options.route):
        data = map(lambda x: x.hex(), packed)
        comments = f' # {source}' if options.comment else ''
        print(*data, PADDING, comments)
