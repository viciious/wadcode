#	wadcode - WAD compiler/decompiler for WAD resource files
#	Copyright (C) 2019-2019 Johannes Bauer
#
#	This file is part of wadcode.
#
#	wadcode is free software; you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation; this program is ONLY licensed under
#	version 3 of the License, later versions are explicitly excluded.
#
#	wadcode is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
#	Johannes Bauer <JohannesBauer@gmx.de>

from BaseCommand import BaseCommand
from WADFile import WADFile
import os
import json
import shutil
import contextlib

class CommandCombine(BaseCommand):
	def __init__(self, cmd, args):
		BaseCommand.__init__(self, cmd, args)

		wad32x = WADFile.create_from_file(args.wad32x, ">", b"IWAD", False)
		wadjag = WADFile.create_from_file(args.wadjag, ">", b"IWAD", True)
		outwad = WADFile(">")

		resource = WADFile._WADResource(name = "S_START", data = b"", size = 0, compressed = False, compressed_size = 0)
		outwad.add_resource(resource)

		copy = True
		for resource in wadjag._resources:
			if len(resource.data) == 0:
				if resource.name == "T_START":
					resource = WADFile._WADResource(name = "S_END", data = b"", size = 0, compressed = False, compressed_size = 0)
					outwad.add_resource(resource)
					copy = False
					continue
				if resource.name == "DS_END":
					outwad.add_resource(resource)
					copy = False
					continue
				if resource.name == "DS_START":
					outwad.add_resource(resource)
					copy = True
					continue

			if not copy:
				continue

			outwad.add_resource(resource)

		copy = False
		for resource in wad32x._resources:
			if len(resource.data) == 0:
				if resource.name == "T_START":
					outwad.add_resource(resource)
					copy = True
					continue
				if copy:
					outwad.add_resource(resource)
				continue

			if not copy:
				continue

			outwad.add_resource(resource)

		outwad.write(args.wadout, b"IWAD")
