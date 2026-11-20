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
// The layout follows brew/rtl/v1/anacron_fpga/apb_uart.sv:
//   DATA    (0): read: received byte; write: byte to send
//   STATUS  (1): bit 0 RX data valid, bit 1 TX ready, bit 2 parity error,
//                bit 3 framing error, bit 4 overrun error, bit 5 CTS;
//                a write clears the error bits. The simulated port is always
//                ready to send and never reports errors.
//   CONFIG1 (2), CONFIG2 (3), DIVIDER (4): line settings; stored, but have no
//                effect in the simulator.
#define CON_DATA_REG_OFS     0x0
#define CON_STATUS_REG_OFS   0x1
#define CON_CONFIG1_REG_OFS  0x2
#define CON_CONFIG2_REG_OFS  0x3
#define CON_DIVIDER_REG_OFS  0x4

#define CON_STAT_RX_FULL     0x01
#define CON_STAT_TX_EMPTY    0x02
