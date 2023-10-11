/* CONSOLE model.

   Copyright (C) 1996-2023 Free Software Foundation, Inc.
   Contributed by Andras Tantos.

   This file is part of simulators.

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

#pragma once

// The console device that simulates the Anachron serial port. It supports both sockets and stdin/out.
// As such, it doesn't handle baud rate settings, flow-control, stop bits, parity settings and the like.
// It is a heavily based on the m32r UART virtual peripheral.

// All registers are 8-bit wide and can only be accessed using 8-bit transactions.
#define GPIO_DATA_REG_OFS     0x0

