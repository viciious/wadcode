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

import enum
import os
import re
import json
import collections
import contextlib
import sys
import mmap
import math
import hashlib
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
	class _WADResource:
		def __init__(self, **kwargs):
			self.name = kwargs.pop("name", "")
			self.data = kwargs.pop("data", b"")
			self.compressed = kwargs.pop("compressed", False)
			self.group = kwargs.pop("group", None)
			self.sha1 = kwargs.pop("sha1", "")
			self.remap_to = kwargs.pop("remap_to", None)
			self.remap_offset = kwargs.pop("remap_offset", 0)
			self.remap_len = kwargs.pop("remap_len", 0)
			self.filename = kwargs.pop("filename", "")
			self.padding = kwargs.pop("padding", 4)

		def padded_size(self):
			size = 0
			if len(self.data) > 0:
				size = len(self.data)
				if len(self.data) & (self.padding-1):
					# padding must be a power of 2
					size += self.padding - len(self.data) & (self.padding-1)
			return size

	class _WADLump:
		def __init__(self, **kwargs):
			self.name = kwargs.pop("name", "")
			self.data = kwargs.pop("data", b"")
			self.size = kwargs.pop("size", 0)
			self.offset = kwargs.pop("offset", 0)
			self.pad = kwargs.pop("pad", 0)

	_maplumps = ["things", "linedefs", "sidedefs", "vertexes", "segs", "ssectors", "nodes", "sectors", "reject", "blockmap"]

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
		self._resources_by_sha1 = collections.defaultdict(list)

	def add_resource(self, resource):
		self._resources.append(resource)
		if resource.sha1 != "":
			self._resources_by_sha1[resource.sha1].append(resource)
			if len(resource.data) > 0 and len(self._resources_by_sha1[resource.sha1]) > 1:
				resource.remap_len = len(resource.data)
				resource.remap_to = self._resources_by_sha1[resource.sha1][0]
				resource.data = b""
				print("%s is a duplicate of %s" % (resource.filename, self._resources_by_sha1[resource.sha1][0].filename))
			elif len(resource.data) > 4:
				for i, resource2 in enumerate(self._resources):
					if resource2 != resource and resource.data in resource2.data:
						resource.remap_to = resource2
						resource.remap_len = len(resource.data)
						resource.remap_offset = resource2.data.find(resource.data)
						resource.data = b""
						print("found %s in %s" % (resource.filename, resource2.filename))
						break
					elif resource2 != resource and len(resource2.data) > 4 and resource2.data in resource.data:
						resource2.remap_to = resource
						resource2.remap_len = len(resource2.data)
						resource2.remap_offset = resource.data.find(resource2.data)
						resource2.data = b""
						print("found %s in %s" % (resource2.filename, resource.filename))
						break

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

			is_sprite = False

			offset = header.directory_offset
			for fileno in range(header.number_of_files):
				size = wadfile._FILE_ENTRY.size
				fileinfo = wadfile._FILE_ENTRY.unpack(mm[offset:offset+size])
				offset += size

				is_stbar = False

				name = fileinfo.name.rstrip(b"\x00").decode("latin1")
				if len(name) == 0:
					compressed = False
				else:
					compressed = ord(name[0]) & 0x80 != 0
				size = fileinfo.size
				compressed_size = size
				if compressed:
					name = chr(ord(name[0]) & ~0x80) + name[1:]

				if name == "S_START":
					is_sprite = True
				elif name == "S_END":
					is_sprite = False
				if name == "STBAR":
					is_stbar = True

				data = mm[fileinfo.offset:]
				if compressed:
					if is_stbar:
						decompress = True
					elif is_sprite:
						decompress = decompress_sprites
					else:
						decompress = decompress_other

					try:
						if decompress:
							data = cls.decompress_data(data)
							compressed = False
						else:
							compressed_size = cls.compressed_length(data)
							data = data[:compressed_size]
					except:
						print ("Error decompressing file %s" % name)
						raise
				else:
					data = data[:size]

				resource = cls._WADResource(name = name, data = data, compressed = compressed, group = "")
				wadfile.add_resource(resource)

			mm.close()
		return wadfile

	@classmethod
	def create_from_directory(cls, dirname, endian, tag = None):
		wadfile = cls(endian)
		content_json = dirname + "/content.json"
		with open(content_json) as f:
			content = json.load(f)

		curtag = None
		is_sprite = False

		for resource_info in content:
			fn = ""
			if resource_info.get("virtual") is True:
				data = b""
			else:
				fn = resource_info["filename"]
				sha1 = hashlib.sha1()
				with open(dirname + "/files/" + fn, "rb") as f:
					data = f.read()

			sha1 = hashlib.sha1()
			sha1.update(data)

			name = ""
			if "name" in resource_info:
				name = str(resource_info["name"])

			if "group" in resource_info:
				group = resource_info["group"]
			else:
				group = None

			if "tag" in resource_info:
				curtag = resource_info["tag"]
			else:
				if not name.lower() in cls._maplumps:
					curtag = None

			if curtag:
				if curtag[0] == "!":
					if tag and curtag[1:] == tag:
						continue
				else:
					if not tag or curtag != tag:
						continue

			if name == "S_START":
				is_sprite = True
			elif name == "S_END":
				is_sprite = False

			padding = 4
			if is_sprite:
				padding = 2

			compressed = "compressed" in resource_info
			resource = cls._WADResource(name = name, data = data, compressed = compressed, group = group, sha1 = sha1.hexdigest(), filename = fn, padding = padding)
			wadfile.add_resource(resource)

		return wadfile

	def write_to_directory(self, outdir):
		with contextlib.suppress(FileExistsError):
			os.makedirs(outdir)
		output_json_filename = outdir + "/content.json"
		output_json = [ ]

		fns = Filenames()
		prev_resource = None
		in_sounds = False
		in_textures = False
		in_flats = False
		in_sprites = False
		for resource in self._resources:
			resource_item = {
				"name":		resource.name,
			}

			if len(resource.data) == 0:
				resource_item["virtual"] = True
				virt_name = resource.name.upper()
				if virt_name == "DS_START":
					in_sounds = True
					in_textures = in_flats = in_sprites = False
				elif virt_name == "DS_END":
					in_sounds = False
				elif virt_name == "T_START":
					in_textures = True
					in_sounds = in_flats = in_sprites = False
				elif virt_name == "T_END":
					in_textures = False
				elif virt_name == "F_START":
					in_flats = True
					in_sounds = in_textures = in_sprites = False
				elif virt_name == "F_END":
					in_flats = False
				elif virt_name == "S_START":
					in_sprites = True
					in_sounds = in_textures = in_flats = False
				elif virt_name == "S_END":
					in_sprites = False
			else:
				extension = ""

				if resource.name == "" or resource.name == ".":
					del resource_item["name"]
					template = "." + prev_resource.name.lower()
				else:
					template = resource.name.lower()
					if in_sounds:
						template = "s_" + template + ".wav"
					elif in_textures:
						template = "t_" + template
					elif in_flats:
						template = "f_" + template
					elif in_sprites:
						template = "p_" + template

				filename = fns.generate(template, extension)
				if ".." in filename:
					continue

				resource_item["filename"] = filename
				if resource.compressed:
					resource_item["compressed"] = resource.compressed
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

	@classmethod
	def resource_lump(cls, resource, data_offset):
		if resource.name == "":
			name = "."
		else:
			name = resource.name
			if resource.compressed:
				name = chr(ord(name[0]) | 0x80) + name[1:]
		name = name.encode("latin1")

		size = len(resource.data)
		if size > 0 and resource.compressed:
			try:
				size = len(cls.decompress_data(resource.data))
			except:
				print ("Error decompressing file %s" % name)
				raise

		data_len = len(resource.data)
		pad = resource.padded_size() - data_len

		return cls._WADLump(offset = data_offset, name = name, data = resource.data, size = size, pad = pad)

	def genlumps(self, ssf, base_offset):
		directory_offset = self._WAD_HEADER.size

		_group_cache = {}
		def group_size(group):
			if group in _group_cache:
				return _group_cache[group]

			size = 0
			for resource in self._resources:
				if resource.group != group:
					continue
				size += resource.padded_size()
			_group_cache[group] = size
			return size

		# assign groups to map lumps
		last_group = None
		for resource in self._resources:
			group = resource.group
			if group is not None:
				last_group = group
				continue
			if resource.name.lower() in self.__class__._maplumps:
				resource.group = last_group

		# assign groups to sprites
		s_start = [i for i, x in enumerate(self._resources) if x.name == "S_START"][0]
		s_end = [i for i, x in enumerate(self._resources) if x.name == "S_END"][0]

		i = 0
		while i < s_end-1-(s_start+1):
			res1 = self._resources[s_start+1+i]
			if res1.group:
				continue
			res1.group = res1.name
			self._resources[s_start+1+i+1].group = res1.name
			i += 2

		t_start = [i for i, x in enumerate(self._resources) if x.name == "T_START"][0]
		t_end = [i for i, x in enumerate(self._resources) if x.name == "T_END"][0]

		f_start = [i for i, x in enumerate(self._resources) if x.name == "F_START"][0]
		f_end = [i for i, x in enumerate(self._resources) if x.name == "F_END"][0]

		lumps = [None] * len(self._resources)
		lumps_sha1 = {}

		def add_resource_lump(num, resource, data_offset):
			#print(data_offset, resource.name)

			if resource.remap_to != None:
				print("remapping %s to %s" % (resource.filename, resource.remap_to.filename))

				lump2 = lumps_sha1[resource.remap_to.sha1]
				offset = lump2.offset
				size = lump2.size

				offset = offset + resource.remap_offset
				size = resource.remap_len

				lump = self.__class__.resource_lump(resource, offset)
				lump.size = size
				lump.pad = 0
			else:
				lump = self.__class__.resource_lump(resource, data_offset)
				if resource.sha1 not in lumps_sha1:
					lumps_sha1[resource.sha1] = lump
			lumps[num] = lump

			if ssf and resource.name == "PAGE7":
				page7_offset = 0x380000 - base_offset - data_offset
				if page7_offset < 0:
					print("PAGE7 overrun: %d bytes" % page7_offset)
					sys.exit(1)
				#lump.pad = page7_offset

			data_offset += len(lump.data) + lump.pad
			return lump, data_offset

		def map_resources(data_offset):
			mapped_count = 0
			lump = None
			first_lump = None
			first_unmapped = 0

			for i, resource in enumerate(self._resources):
				if lumps[i]:
					first_unmapped = i + 1
					continue

				if lumps[t_start] and lumps[t_end] is None and (i < t_start or i > t_end):
					continue
				if lumps[s_start] and lumps[s_end] is None and (i < s_start or i > s_end):
					continue
				if lumps[f_start] and lumps[f_end] is None and (i < f_start or i > f_end):
					continue

				size = resource.padded_size()
				page = math.floor((base_offset + data_offset) / 0x80000)
				if ssf:
					group = resource.group
					if group:
						size = group_size(resource.group)

					if size >= 0x80000:
						print("Lump %s is too large: %s" % (resource.name, size))
						sys.exit(1)

					# do not allow lumps or groups to cross over into the next page block
					if page >= 5:
						end_page = math.floor((base_offset + data_offset + size) / 0x80000)
						if page != end_page:
							# do not shuffle sprite lumps
							if first_unmapped >= s_start and first_unmapped < s_end:
								break
							continue

					if group:
						for j, resource2 in enumerate(self._resources[first_unmapped:]):
							if resource2.group != group:
								continue
							lump, data_offset = add_resource_lump(first_unmapped+j, resource2, data_offset)
							if first_lump is None:
								first_lump = lump
							mapped_count = mapped_count + 1
						continue

				# do not shuffle sprite lumps
				if first_unmapped >= s_start and first_unmapped < s_end:
					if i >= s_end:
						break

				# do not shuffle texture lumps
				if first_unmapped >= t_start and first_unmapped < t_end:
					if i >= t_end:
						break

				# do not shuffle flat lumps
				if first_unmapped >= f_start and first_unmapped < f_end:
					if i >= f_end:
						break

				lump, data_offset = add_resource_lump(i, resource, data_offset)
				if first_lump is None:
					first_lump = lump
				mapped_count = mapped_count + 1

			return data_offset, mapped_count, lump, first_lump

		total_mapped = 0
		data_offset = directory_offset + (len(self._resources) * self._FILE_ENTRY.size)

		while True:
			start_offset = data_offset
			data_offset, mapped_count, last_lump, first_lump = map_resources(data_offset)

			total_mapped += mapped_count
			if total_mapped == len(self._resources):
				break
			if not ssf:
				break

			next_page = math.floor((base_offset + data_offset + 0x7FFFF) / 0x80000)
			pad = 0x80000 * next_page - base_offset - data_offset
			print("Padding page %d with %d bytes" % (next_page-1, pad))
			print("First lump is %s at %X" % (first_lump.name, base_offset+start_offset))
			print("Last lump is %s " % last_lump.name)
			last_lump.pad += pad
			data_offset += pad

		if not ssf:
			return lumps
		return lumps
		#return sorted(lumps, key=lambda d: d.offset)

	def write(self, wad_filename, wadtype, ssf, base_offset):
		with open(wad_filename, "wb") as f:
			header = self._WAD_HEADER.pack({
				"magic":			wadtype,
				"number_of_files":	len(self._resources),
				"directory_offset":	self._WAD_HEADER.size,
			})
			f.write(header)

			lumps = self.genlumps(ssf, base_offset)

			for lump in lumps:
				file_entry = self._FILE_ENTRY.pack({
					"offset":	lump.offset,
					"size":		lump.size,
					"name":		lump.name,
				})
				f.write(file_entry)

			for lump in lumps:
				if len(lump.data) > 0:
					f.write(lump.data)
				if lump.pad > 0:
					f.write(b'\x00' * lump.pad)
