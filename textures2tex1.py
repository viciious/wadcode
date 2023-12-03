#!/usr/bin/env python3

import sys
import re
import json

wall = re.compile(r"\s*\"([^\"]+?)\"\s*,\s*(\d+)\s*,\s*(\d+)")
patch = re.compile(r"\s*Patch\s+\"([^\"]+?)\"\s*,\s*(\d+)\s*,\s*(\d+).*")

def parse_textures(fin):
    r = re.split(re.compile(r"WallTexture\s*"), fin.read())
    wt = []

    for wl in r:
        wl = wl.strip()
        if len(wl) == 0:
            continue

        state = 0
        for l in wl.split("\n"):
            if len(l) == 0:
                continue

            if state == 0:
                m = re.match(wall, l)
                w = {
                    "name": m.group(1),
                    "width": int(m.group(2)),
                    "height": int(m.group(3)),
                    "patches": []
                }
                state += 1 # expecting a '{'
                continue

            if state == 1:
                if l != "{":
                    print("Syntax error: encountered %s while expecting a {" % l)
                    sys.exit(1)
                state += 1 # expecting a patch or a '}'
                continue

            if state == 2:
                if l == "}":
                    wt.append(w)
                    state = 0 # expecting a new wall
                    continue

                m = re.match(patch, l)
                if m is None:
                    print("Syntax error: encountered %s while expecting Patch" % l)
                    sys.exit(1)

                p = {
                    "patch": m.group(1),
                    "originx": int(m.group(2)),
                    "originy": int(m.group(3)),
                }

                if p["originx"] == 0: del p["originx"]
                if p["originy"] == 0: del p["originy"]
                w["patches"].append(p)
                continue

    wt = sorted(wt, key=lambda x: x["name"])

    for w in wt:
        if len(w["patches"]) == 1 and w["patches"][0]["patch"] == w["name"]:
            del w["patches"]
    return wt

with open(sys.argv[1], "rt") as fin:
    with open(sys.argv[2], "wt") as fout:
        arr = parse_textures(fin)
        json.dump(arr, fp = fout, indent = 4, sort_keys = True)
        fout.write("\n")
