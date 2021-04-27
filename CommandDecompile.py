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

class CommandDecompile(BaseCommand):
	def __init__(self, cmd, args):
		BaseCommand.__init__(self, cmd, args)

		endian = "<"
		if args.big_endian:
			endian = ">"
		wadtype = b"IWAD"
		if args.pwad:
			wadtype = b"PWAD"

		wadfile = WADFile.create_from_file(args.infile, endian, wadtype)
		wadfile.write_to_directory(args.outdir)
