# Copyright (C) 2016-2022 Free Software Foundation, Inc.

# This file is part of GDB.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# An empty .SUFFIXES target clears the suffixes list, which will skip lookups
# of many implicit rules, speeding up the execution of make.
.SUFFIXES:

# Disable some implicit pattern rules.
%: %,v
%: RCS/%,v
%: RCS/%
%: s.%
%: SCCS/s.%
