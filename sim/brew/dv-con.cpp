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

extern "C" {
#include "defs.h"

#define basename basename
#include "bfd.h"
#undef basename

//#include "libiberty.h"
//#include "sim/sim.h"
#include "sim-main.h"
#include "hw-main.h"

#include "dv-sockser.h"
#include "dv-con.h"
}

struct dv_console
{
  dv_console() {
    this->ctrl_reg1 = 0;
    this->ctrl_reg2 = 0;
    this->divider_reg = 0;
    this->status_reg = 0;
  }
  void set_base_addr(uint32_t base_addr) {
    this->base_addr = base_addr;
  }
  static unsigned io_read_buffer(struct hw *me, void *dest, int space, address_word addr, unsigned nr_bytes);
  static unsigned io_write_buffer(struct hw *me, const void *source, int space, address_word addr, unsigned nr_bytes);
  static bool handle_input(struct hw *me);

  uint32_t base_addr;
  // Allow reading and writing of registers that we don't actually simulate
  uint8_t ctrl_reg1;
  uint8_t ctrl_reg2;
  uint8_t divider_reg;
  char con_input;
  uint8_t status_reg;
};

unsigned dv_console::io_write_buffer(struct hw *me, const void *source, int space, address_word addr, unsigned nr_bytes)
{
  SIM_DESC sd = hw_system(me);
  dv_console *console = (dv_console *)hw_data(me);
  int status = dv_sockser_status(sd);

  SIM_ASSERT(nr_bytes == 1);
  uint32_t ofs = addr - console->base_addr;
  switch (ofs) {
    case CON_DATA_REG_OFS:
      if (status & DV_SOCKSER_DISCONNECTED) {
        sim_io_write_stdout(sd, (const char *)source, nr_bytes);
        sim_io_flush_stdout(sd);
      } else {
        int ret = dv_sockser_write_buffer(sd, (const unsigned char *)source, nr_bytes);
        // If any error occurs, simply say we couldn't write anything
        if (ret < 0) nr_bytes = 0;
      }
    break;
    case CON_CONFIG1_REG_OFS:
      console->ctrl_reg1 = *(uint8_t*)(source);
    break;
    case CON_CONFIG2_REG_OFS:
      console->ctrl_reg2 = *(uint8_t*)(source);
    break;
    case CON_DIVIDER_REG_OFS:
      console->divider_reg = *(uint8_t*)(source);
    break;
    default:
      return 0;
    break;
  }
  return nr_bytes;
}

bool dv_console::handle_input(struct hw *me)
{
  SIM_DESC sd = hw_system(me);
  dv_console *console = (dv_console *)hw_data(me);
  int status = dv_sockser_status(sd);

  // If we have a pending character already, simply return
  if ((console->status_reg & CON_STAT_RX_FULL) != 0) return true;

  if (status & DV_SOCKSER_DISCONNECTED) {
    int ret = sim_io_poll_read(sd, 0/*STDIN*/, &console->con_input, 1);
    if (ret < 0) return false;
    // We have gotten a character: set the status bit and return
    console->status_reg |= CON_STAT_RX_FULL;
    return true;
  } else {
    int ret = dv_sockser_read(sd);
    if (ret < 0) return false;
    console->con_input = (char)ret;
    console->status_reg |= CON_STAT_RX_FULL;
    return true;
  }
}

unsigned dv_console::io_read_buffer(struct hw *me, void *dest, int space, address_word addr, unsigned nr_bytes)
{
  SIM_DESC sd = hw_system(me);
  dv_console *console = (dv_console *)hw_data(me);
  int status = dv_sockser_status(sd);

  SIM_ASSERT(nr_bytes == 1);
  uint32_t ofs = addr - console->base_addr;
  switch (ofs) {
    case CON_DATA_REG_OFS:
      if (dv_console::handle_input(me)) {
        // We have a character in the buffer -> send it and clear the status
        *(char*)(dest) = console->con_input;
        console->status_reg &= ~CON_STAT_RX_FULL;
        return 1;
      }
    break;
    case CON_STATUS_REG_OFS:
      // check for new input (it sets the requisite status bits too if not already set)
      dv_console::handle_input(me);
      *(uint8_t*)(dest) = console->status_reg;
      return 1;
    case CON_CONFIG1_REG_OFS:
      *(uint8_t*)(dest) = console->ctrl_reg1;
      return 1;
    break;
    case CON_CONFIG2_REG_OFS:
      *(uint8_t*)(dest) = console->ctrl_reg2;
      return 1;
    break;
    case CON_DIVIDER_REG_OFS:
      *(uint8_t*)(dest) = console->divider_reg;
      return 1;
    break;
    default:
      return 0;
    break;
  }
  hw_abort(me, "Should never get here");
}

static dv_console *attach_console_regs(struct hw *me)
{
  // TODO: this should be a unique-pointer!
  dv_console *console = new dv_console();

  address_word attach_address;
  int attach_space;
  unsigned attach_size;
  reg_property_spec reg;

  if (hw_find_property(me, "reg") == NULL)
    hw_abort(me, "Missing \"reg\" property");

  if (!hw_find_reg_array_property(me, "reg", 0, &reg))
    hw_abort(me, "\"reg\" property must contain three addr/size entries");

  // Convert bus-based address to attach address
  // :::NOTE: I'm guessing this is mostly an offset to the bus base-address
  hw_unit_address_to_attach_address(
    hw_parent(me),
    &reg.address,
    &attach_space,
    &attach_address,
    me
  );

  // Convert bus-based size to attach size
  // :::NOTE: I'm guessing it's a no-op, mostly
  hw_unit_size_to_attach_size(hw_parent(me), &reg.size, &attach_size, me);

  if (attach_size != 5)
    hw_abort(me, "Console must occupy 5 bytes of memory space");

  // Attach ourselves to the address space
  hw_attach_address(
    hw_parent (me),
    0,
    attach_space,
    attach_address,
    attach_size,
    me
  );

  console->set_base_addr(attach_address);
  set_hw_data(me, console);
  return console;
}

static void brew_con_finish(struct hw *me)
{
  // todo: should we use HW_ZALLOC and placement new?
  //uart = HW_ZALLOC (me, struct m32r_uart);

  set_hw_io_read_buffer(me, dv_console::io_read_buffer);
  set_hw_io_write_buffer(me, dv_console::io_write_buffer);

  attach_console_regs(me);
}

const struct hw_descriptor dv_brew_con_descriptor[] =
{
  {"brew_con", brew_con_finish,},
  {NULL, NULL},
};
