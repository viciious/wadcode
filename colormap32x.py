#!/usr/bin/env python3
# Copyright (C) 2001 Colin Phipps <cphipps@doomworld.com>
# Copyright (C) 2008, 2013 Simon Howard
# Parts copyright (C) 1999 by id Software (http://www.idsoftware.com/)
#
# SPDX-License-Identifier: GPL-2.0+
#
# Takes PLAYPAL as input (filename is the only parameter)
# Produces a light graduated COLORMAP on stdout
# O(n^2)
#
# This was originally a Perl script by Colin Phipps; it was converted
# to Python and now is a more generic tool for generating all kinds of
# COLORMAP effects.
#

import os
import sys
import struct
import random
import math

# Parameters affecting colormap generation:

# "Darkness" is this color, which is usually black, but can be
# overridden (RGB 0-255):
dark_color = (0, 0, 0)

# Color to tint the colormap (RGB 0.0-1.0):
tint_color = (255, 255, 255)

# Fractional balance between tint and normal color. 0 is no tint applied,
# 1.0 is full tint.
tint_frac = 0

# Fudge factor to adjust brightness when calculating 'tinted' version
# of colors. Larger values are brighter but may cause color clipping.
# A value of 0.33 is a straight-average of the RGB channels. Maximum
# sensible value is 1.0, though it can be overdriven for fancy
# brightness effects.
tint_bright = 0.5

# default remapping table: remaps the color to itself
remap = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255]

# baron to hell knight
# "16:24=49:57", "25:40=128:143", "41:44=148:151", "45:47=236:238", "56:61=59:64", "63:77=65:79", "78:79=5:6"
# remap = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 49, 50, 51, 52, 53, 54, 55, 56, 57, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 148, 149, 150, 151, 236, 237, 238, 48, 49, 50, 51, 52, 53, 54, 55, 59, 60, 61, 62, 63, 64, 62, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 5, 6, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 239, 249, 250, 251, 252, 253, 254, 255]

dither = False

def read_palette(filename):
    """Read palette from file and return a list of tuples containing
       RGB values."""
    f = open(filename, "rb")

    colors = []

    for i in range(256):
        data = f.read(3)

        color = struct.unpack("BBB", data)

        r = color[0]
        g = color[1]
        b = color[2]

        colors.append((r, g, b))

    return colors


# Return closest palette entry to the given RGB triple

def search_palette(palette, target, preserve_lum = True, ignore=-1, ignore2=-1):
    """Search the given palette and find the nearest matching
       color to the given color, returning an index into the
       palette of the color that best matches."""

    def dist(c1, c2):
        r = c1[0] - c2[0]
        g = c1[1] - c2[1]
        b = c1[2] - c2[2]
        return math.sqrt(r*r + g*g + b*b)

    def color_linearize(c):
        if c <= 0.04045:
            return c / 12.92
        return math.pow((( c + 0.055)/1.055), 2.4)

    def rgb_y(c):
        r = color_linearize(c[0] / 255.0)
        g = color_linearize(c[1] / 255.0)
        b = color_linearize(c[2] / 255.0)
        return 0.2126 * r + 0.7152 * g + 0.0722 * b

    def y_lstar(y):
        if y <= (216.0/24389):
            return y * (24389.9/27)
        return math.pow(y,(1.0/3)) * 116 - 16

    # https://www.compuphase.com/cmetric.htm
    def rgb_distance(c1, c2):
        #return dist(c1, c2)
        rmean = ( c1[0] + c2[0] ) / 2.0
        r = c1[0] - c2[0]
        g = c1[1] - c2[1]
        b = c1[2] - c2[2]
        return math.sqrt((((512.0+rmean)*r*r) / 256.0) + 4.0*g*g + (((767-rmean)*b*b)/256.0))

    def rgb_lstar(c):
        return y_lstar(rgb_y(c)) / 100.0
    
    best_diff = None
    best_index = None
    best_diff2 = None
    best_index2 = None

    for i in range(len(palette)):
        if i == ignore or i <= ignore2:
            continue

        color = palette[i]

        diff = rgb_distance(target, color)

        if best_index is None or diff < best_diff:
            best_diff2 = best_diff
            best_index2 = best_index
            best_diff = diff
            best_index = i

    if preserve_lum and best_index2 and best_diff2 < 32:
        y = rgb_lstar(target)
        if y > 0.10:
            y1 = rgb_lstar(palette[best_index])
            y2 = rgb_lstar(palette[best_index2])

            if abs(y2 - y) < abs(y1 - y):
                return best_index2
    return best_index


def generate_colormap(colors, palette, preserve_lum = True):
    """Given a list of colors, translate these into indexes into
       the given palette, finding the nearest color where an exact
       match cannot be found."""
    result = []

    for color in colors:
        index = search_palette(palette, color, preserve_lum)
        result.append(index)

    for i, r in enumerate(remap):
        result[i] = result[r]

    return result


def swap_colormap(colormap):
    result = []
    for i in range(128, len(colormap), 256):
        result += colormap[i:i+128]
        result += colormap[i-128:i]
    return result


def generate_dithered_colormap(colormap, dithered):
    global dither

    result = []
    if dither:
        for c in colormap:
            result.append(dithered[c][3])
            result.append(dithered[c][4])
    else:
        for c in colormap:
            result.append(dithered[c][3])
            result.append(dithered[c][3])

    return result


def tint_colors(colors, tint, bright=0.5):
    """Given a list of colors, tint them a particular color."""

    result = []
    for c in colors:
        # I've experimented with different methods of calculating
        # intensity, but this seems to work the best. This is basically
        # doing an average of the full channels, but a straight
        # average causes the picture to get darker - eg. (0,0,255)
        # maps to (87,87,87). So we have a controllable brightness
        # factor that allows the brightness to be adjusted.
        intensity = min((c[0] + c[1] + c[2]) * bright, 255) / 255.0
        result.append(
            (tint[0] * intensity, tint[1] * intensity, tint[2] * intensity)
        )

    return result


def blend_colors(colors1, colors2, factor=0.5):
    """Blend the two given lists of colors, with 'factor' controlling
       the mix between the two. factor=0 is exactly colors1, while
       factor=1 is exactly colors2. Returns a list of blended colors."""
    result = []

    for index, c1 in enumerate(colors1):
        c2 = colors2[index]
        result.append(
            (
                c2[0] * factor + c1[0] * (1 - factor),
                c2[1] * factor + c1[1] * (1 - factor),
                c2[2] * factor + c1[2] * (1 - factor),
            )
        )

    return result


def invert_colors(colors):
    """Given a list of colors, translate them to inverted monochrome."""
    result = []

    for color in colors:
        # Formula comes from ITU-R recommendation BT.709
        gray = round(color[0] * 0.2126 + color[1] * 0.7152 + color[2] * 0.0722)
        inverse = 255 - gray

        result.append((inverse, inverse, inverse))

    return result


def solid_color_list(color):
    """Generate a 256-entry palette where all entries are the
       same color."""
    return [color] * 256


def output_colormap(colormap):
    """Output the given palette to stdout."""
    for c in colormap:
        x = struct.pack("B", c)
        os.write(sys.stdout.fileno(), x)


def print_palette(colors):
    for y in range(16):
        for x in range(16):
            color = colors[y * 16 + x]

            print("#%02x%02x%02x" % color)

        print()

def dithered_palette(palette):
    global dither

    dithered = []
    for i, c in enumerate(palette):
        # round to 32X palette
        r, g, b = c[0], c[1], c[2]
        if (r & 7) == 7 and (g & 7) == 7 and (b & 7) == 7:
            r = (r + 1) & ~7
            if r > 255:
                r = 255
            g = (g + 1) & ~7
            if g > 255:
                g = 255
            b = (b + 1) & ~7
            if b > 255:
                b = 255
        else:
            r = r & ~7
            g = g & ~7
            b = b & ~7

        c1 = c2 = i
        grey = ((0.3 * r) + (0.59 * g) + (0.11 * b))
        #if random.randint(0, 255) < grey * 0.9:
        if dither:
            if grey > 64:
                c1 = search_palette(palette, (r, g, b), i)
                if not c1:
                    c1 = c2 = i
                elif max(abs(palette[c1][0] - r), abs(palette[c1][1] - g), abs(palette[c1][2] - b)) < 5:
                    c1 = c2 = i
                else:
                    c2 = search_palette(palette, (r, g, b), c1)
                    if not c2:
                        c2 = i

                    if c1 and min(abs(palette[c1][0] - r), abs(palette[c1][1] - g), abs(palette[c1][2] - b)) > 8:
                        c1 = i
                    if c2 and min(abs(palette[c2][0] - r), abs(palette[c2][1] - g), abs(palette[c2][2] - b)) > 8:
                        c2 = i

                if random.randint(0, 100) > 50:
                    t = c1
                    c1 = c2
                    c2 = t
        dithered.append((r, g, b, c1, c2))
    return dithered

def parse_color_code(s):
    """Parse a color code in HTML color code format, into an RGB
       3-tuple value."""
    if not s.startswith("#") or len(s) != 7:
        raise Exception("Not in HTML color code form: %s" % s)
    return (int(s[1:3], 16), int(s[3:5], 16), int(s[5:7], 16))


def set_parameter(name, value):
    """Set configuration value, from command line parameters."""
    global dark_color, tint_color, tint_frac, tint_bright, dither

    if name == "dark_color":
        dark_color = parse_color_code(value)
    elif name == "tint_color":
        tint_color = parse_color_code(value)
    elif name == "tint_pct":
        tint_frac = int(value) / 100.0
    elif name == "tint_bright":
        tint_bright = float(value)
    elif name == "dither":
        dither = True
    else:
        raise Exception("Unknown parameter: '%s'" % name)


# Parse command line.

playpal_filename = None

for arg in sys.argv[1:]:
    if arg.startswith("--") and "=" in arg:
        key, val = arg[2:].split("=", 2)
        set_parameter(key, val)
    else:
        playpal_filename = arg

if playpal_filename is None:
    print("Usage: %s playpal.lmp > output-file.lmp" % sys.argv[0])
    sys.exit(1)

palette = read_palette(playpal_filename)
colors = palette
dithered = dithered_palette(palette)

# Apply tint, if enabled.
# The tint is intentionally applied *before* the darkening effect is
# applied. This allows us to darken to a different color than the tint
# color, if so desired.
if tint_frac > 0:
    colors = blend_colors(
        palette, tint_colors(colors, tint_color, tint_bright), tint_frac
    )

# Generate colormaps for different darkness levels, by blending between
# the default colors and a palette where every entry is the "dark" color.
dark = solid_color_list(dark_color)

close_range = 0

for i in range(32):
    darken_factor = i - close_range
    if darken_factor < 0:
        darken_factor = 1
    else:
        darken_factor = darken_factor / 32.0
        darken_factor = 1 - darken_factor
        if darken_factor < 0:
            darken_factor = 0

    darkened_colors = blend_colors(dark, colors, darken_factor)
    output_colormap(generate_dithered_colormap(swap_colormap(generate_colormap(darkened_colors, palette, False)), dithered))

#tinted_colors = blend_colors(
#    palette, tint_colors(colors, (0, 0, 255), tint_bright), tint_frac
#)
#output_colormap(generate_colormap(tinted_colors, palette))

# Inverse color map for invulnerability effect.
inverse_colors = invert_colors(palette)
output_colormap(generate_dithered_colormap(swap_colormap(generate_colormap(inverse_colors, palette, False)), dithered))

# Last colormap is all black, and is actually unused in Vanilla Doom
# (it was mistakenly included by the dcolors.c utility). It's
# strictly unneeded, though some utilities (SLADE) do not detect a
# lump as a COLORMAP unless it is the right length.
#output_colormap(generate_colormap(dark, palette))
