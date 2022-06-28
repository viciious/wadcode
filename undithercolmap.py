#!/usr/bin/env python3
import struct
import sys
import os

global black_color

def read_palette(filename):
    global black_color

    colors = []
    minc = 255

    with open(filename, "rb") as f:
        for i in range(256):
            color = struct.unpack("BBB", f.read(3))
            r, g, b = color[0], color[1], color[2]
            if i > 0:
                if r == g and g == b and r < minc:
                    minc = r
            colors.append((r,g,b))

    black_color = minc
    return colors

def read_colormap(filename):
    with open(filename, "rb") as f:
        return list(f.read())

def output_colormap(colormap):
    i = 0
    while i < len(colormap):
        c = colormap[i]
        if c == 0:
            c = black_color
        os.write(sys.stdout.fileno(), struct.pack("B", c))
        i += 2

palette = read_palette(sys.argv[1])
colormap = read_colormap(sys.argv[2])
output_colormap(colormap)
