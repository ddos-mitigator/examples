#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 BIFIT Mitigator Team <info@mitigator.ru>

import struct
import sys

size = 4

if len(sys.argv) != 4:
    print(f'Usage:   {sys.argv[0]} offset mask     pattern', file=sys.stderr)
    print(f'Example: {sys.argv[0]} 22     FF00FF00 12003400', file=sys.stderr)
    sys.exit(1)

offset = int(sys.argv[1])
mask = bytes.fromhex(sys.argv[2])
pattern = bytes.fromhex(sys.argv[3])

if len(mask) != len(pattern) != size:
    print(f'Error: mask or pattern size is not {size}', file=sys.stderr)
    sys.exit(1)

data = struct.pack('<I', offset) + mask + pattern
print(data.hex())
