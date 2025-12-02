#!/usr/bin/python3

import sys
import os
from PIL import Image

#	Copyright (C) 2023 Victor Luchits
#
#	This file is free software; you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation; this program is ONLY licensed under
#	version 3 of the License, later versions are explicitly excluded.
#
#	This file is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program.  If not, see <https://www.gnu.org/licenses/>.

im1 = Image.open(sys.argv[1])
data1 = bytearray(im1.tobytes())

im2 = Image.open(sys.argv[2])
data2 = bytearray(im2.tobytes())

pal2pal = [0] * 256
for i in range(256):
    pal2pal[i] = {}

for i, p in enumerate(data1):
    p2 = data2[i]
    if p2 in pal2pal[p]:
        pal2pal[p][p2] += 1
    else:
        pal2pal[p][p2] = 1

mapping = ""

print(pal2pal)

for i, p in enumerate(pal2pal):
    if len(p) == 0:
        continue

    best = i
    bestv = 0
    for k in p:
        v = p[k]
        if v > bestv:
            best = k
            bestv = v

    if i != best:
        if mapping != "":
            mapping += ","
        mapping += '"%d:%d=%d:%d"' % (i, i, best, best)

print(mapping)
