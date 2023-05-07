/* Perform tilde expansion on paths for GDB and gdbserver.

   Copyright (C) 2017-2022 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef COMMON_GDB_TILDE_EXPAND_H
#define COMMON_GDB_TILDE_EXPAND_H

/* Perform tilde expansion on DIR, and return the full path.  */
extern std::string gdb_tilde_expand (const char *dir);

#endif /* COMMON_GDB_TILDE_EXPAND_H */
