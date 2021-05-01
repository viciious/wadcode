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

import os
import re
import json
import collections
import contextlib
import sys
from NamedStruct import NamedStruct

class Filenames():
	def __init__(self):
		self._names = set()

	def generate(self, template, extension = ""):
		for i in range(1000):
			if i == 0:
				name = "%s%s" % (template, extension)
			else:
				name = "%s_%03d%s" % (template, i, extension)
			if name not in self._names:
				self._names.add(name)
				return name

class WADFile():
	_WADResource = collections.namedtuple("WADResource", [ "name", "data", "size", "compressed" ])

	def __init__(self, struct_extra = "<"):
		self._WAD_HEADER = NamedStruct((
			("4s", "magic"),
			("l", "number_of_files"),
			("l", "directory_offset"),
		), struct_extra)

		self._FILE_ENTRY = NamedStruct((
			("l", "offset"),
			("l", "size"),
			("8s", "name"),
		), struct_extra)

		self._resources = [ ]
		self._resources_by_name = collections.defaultdict(list)

	def add_resource(self, resource):
		self._resources.append(resource)
		self._resources_by_name[resource.name].append(resource)

	@classmethod
	def compressed_length(cls, data):
		getidbyte = 0
		idbyte = 0

		n = 0
		while True:
			if getidbyte == 0:
				if n == len(data):
					break
				idbyte = data[n]
				n = n + 1
			getidbyte = (getidbyte + 1) & 7

			if n == len(data):
				break
			n = n + 1

			if idbyte & 1 != 0:
				# decompress
				if n == len(data):
					break
				blen = (data[n] & 0xf)+1
				n = n + 1
				if blen == 1:
					break
			idbyte = idbyte >> 1

		return n

	@classmethod
	def create_from_file(cls, filename, endian, wadtype):
		wadfile = cls(endian)
		with open(filename, "rb") as f:
			header = wadfile._WAD_HEADER.unpack_from_file(f)
			assert(header.magic == wadtype)

			f.seek(header.directory_offset)
			for fileno in range(header.number_of_files):
				fileinfo = wadfile._FILE_ENTRY.unpack_from_file(f)
				name = fileinfo.name.rstrip(b"\x00").decode("latin1")
				compressed = ord(name[0]) & 0x80 != 0
				cur_pos = f.tell()
				f.seek(fileinfo.offset)
				size = fileinfo.size
				data = f.read(fileinfo.size)
				if compressed:
					data = data[:cls.compressed_length(data)]
				f.seek(cur_pos)
				resource = cls._WADResource(name = name, data = data, size = size, compressed = compressed)
				wadfile.add_resource(resource)
		return wadfile

	@classmethod
	def create_from_directory(cls, dirname, endian):
		wadfile = cls(endian)
		content_json = dirname + "/content.json"
		with open(content_json) as f:
			content = json.load(f)

		for resource_info in content:
			size = 0
			if resource_info.get("virtual") is True:
				data = b""
			else:
				size = resource_info["size"]
				with open(dirname + "/files/" + resource_info["filename"], "rb") as f:
					data = f.read()

			compressed = False
			if "compressed" in resource_info:
				compressed = True
			resource = cls._WADResource(name = resource_info["name"], data = data, size = size, compressed = compressed)
			wadfile.add_resource(resource)
		return wadfile

	def write_to_directory(self, outdir):
		with contextlib.suppress(FileExistsError):
			os.makedirs(outdir)
		output_json_filename = outdir + "/content.json"
		output_json = [ ]

		lvl_regex = re.compile("E\dM\d")
		fns = Filenames()
		section = None
		prev_resource = None
		for resource in self._resources:
			resource_item = {
				"name":		resource.name,
			}
			if len(resource.data) == 0:
				resource_item["virtual"] = True
				section = resource.name
			else:
				extension = ""
				encoder = None
				if resource.name == ".":
					template = "." + prev_resource.name.lower()
				elif ord(resource.name[0]) & 0x80:
					resource_item["compressed"] = True
					resource_item["name"] = chr(ord(resource.name[0]) & ~0x80) + resource.name[1:]
					template = resource_item["name"].lower()
				else:
					template = resource.name.lower()
					
				filename = fns.generate(template, extension)
				resource_item["filename"] = filename
				resource_item["size"] = resource.size
				write_data = resource.data

				full_outname = "%s/files/%s" % (outdir, filename)
				with contextlib.suppress(FileExistsError):
					os.makedirs(os.path.dirname(full_outname))
				with open(full_outname, "wb") as outfile:
					outfile.write(write_data)

			output_json.append(resource_item)
			prev_resource = resource

		with open(output_json_filename, "w") as f:
			json.dump(output_json, fp = f, indent = 4, sort_keys = True)

	def write(self, wad_filename, wadtype):
		with open(wad_filename, "wb") as f:
			header = self._WAD_HEADER.pack({
				"magic":			wadtype,
				"number_of_files":	len(self._resources),
				"directory_offset":	0,
			})
			f.write(header)

			for resource in self._resources:
				data_len = len(resource.data)
				if data_len > 0:
					f.write(resource.data)
					# pad to a multiple of 4
					padded_len = (data_len + 3) & ~3
					f.write(b'\x00' * (padded_len - data_len))

			data_offset = self._WAD_HEADER.size
			for resource in self._resources:
				name = resource.name
				if resource.compressed:
					name = chr(ord(name[0]) | 0x80) + name[1:]
				name = name.encode("latin1")

				file_entry = self._FILE_ENTRY.pack({
					"offset":	data_offset,
					"size":		resource.size,
					"name":		name,
				})
				f.write(file_entry)

				data_len = len(resource.data)
				if data_len > 0:
					# pad to a multiple of 4
					data_offset += (data_len + 3) & ~3 

		with open(wad_filename, "r+b") as f:
			header = self._WAD_HEADER.pack({
				"magic":			wadtype,
				"number_of_files":	len(self._resources),
				"directory_offset":	data_offset,
			})
			f.write(header)
