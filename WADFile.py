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
import mmap
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
	_WADResource = collections.namedtuple("WADResource", [ "name", "data", "size", "compressed", "compressed_size" ])

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
		compbits = 0
		compbitslen = 0

		n = 0
		while True:
			if compbitslen == 0:
				compbits = data[n]
				compbitslen = 8
				n = n + 1

			n = n + 1
			if compbits & 1 != 0:
				# decompress
				blen = (data[n] & 0xF)+1
				n = n + 1
				if blen == 1:
					break

			compbits = compbits >> 1
			compbitslen = compbitslen - 1

		return n

	@classmethod
	def decompress_data(cls, data):
		compbits = 0
		compbitslen = 0

		n = 0
		out = bytearray()
		while True:
			if compbitslen == 0:
				compbits = data[n]
				compbitslen = 8
				n += 1

			if compbits & 1 != 0:
				# decompress
				bpos = (data[n] << 4) | (data[n+1] >> 4)
				blen = data[n+1] & 0xF
				if blen == 0:
					break

				bpos = len(out) - bpos - 1
				for i in range(blen + 1):
					out.append(out[bpos+i])
				n += 2
			else:
				out.append(data[n])
				n += 1

			compbits = compbits >> 1
			compbitslen = compbitslen - 1

		return out

	@classmethod
	def create_from_file(cls, filename, endian, wadtype, decompress_sprites = False, decompress_other = False):
		wadfile = cls(endian)
		with open(filename, "rb") as f:
			mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_COPY)

			header = wadfile._WAD_HEADER.unpack(mm[0:])
			assert(header.magic == wadtype)

			is_sprite = True

			offset = header.directory_offset
			for fileno in range(header.number_of_files):
				size = wadfile._FILE_ENTRY.size
				fileinfo = wadfile._FILE_ENTRY.unpack(mm[offset:offset+size])
				offset += size

				is_demo = False
				is_stbar = False

				name = fileinfo.name.rstrip(b"\x00").decode("latin1")
				if len(name) == 0:
					name = "."
					compressed = False
				else:
					compressed = ord(name[0]) & 0x80 != 0
				size = fileinfo.size
				compressed_size = size
				if compressed:
					name = chr(ord(name[0]) & ~0x80) + name[1:]

				if name == "T_START":
					is_sprite = False
				if name == "STBAR":
					is_stbar = True
				if "DEMO" in name:
					is_demo = True

				data = mm[fileinfo.offset:]
				if compressed:
					if is_stbar:
						decompress = True
					elif is_sprite:
						decompress = decompress_sprites
					else:
						decompress = decompress_other

					if decompress:
						data = cls.decompress_data(data)
						compressed = False
					else:
						compressed_size = cls.compressed_length(data)
						data = data[:compressed_size]
				else:
					data = data[:size]

				resource = cls._WADResource(name = name, data = data, size = size, compressed = compressed, compressed_size = compressed_size)
				wadfile.add_resource(resource)

			mm.close()
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
			compressed_size = 0
			if "compressed" in resource_info:
				compressed = True
				compressed_size = resource_info["compressed_size"]
			resource = cls._WADResource(name = resource_info["name"], data = data, size = size, compressed = compressed, compressed_size = compressed_size)
			wadfile.add_resource(resource)
		return wadfile

	def write_to_directory(self, outdir):
		with contextlib.suppress(FileExistsError):
			os.makedirs(outdir)
		output_json_filename = outdir + "/content.json"
		output_json = [ ]

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
				else:
					template = resource.name.lower()
					
				filename = fns.generate(template, extension)
				resource_item["filename"] = filename
				resource_item["size"] = resource.size
				if resource.compressed:
					resource_item["compressed"] = resource.compressed
					resource_item["compressed_size"] = resource.compressed_size
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
