/* Simulator control device.

   Copyright (C) 2026 Free Software Foundation, Inc.

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

#include "defs.h"

#include "sim-main.h"
#include "hw-main.h"

#include "dv-brew_simctl.h"

// See dv-brew_simctl.h for the register layout. Instantiate it with
//   /core/brew_simctl@<addr>/reg <addr> 4

typedef struct dv_brew_simctl
{
  uint32_t base_addr;
} dv_brew_simctl;

static unsigned dv_brew_simctl_io_write_buffer(struct hw *me, const void *source, int space, address_word addr, unsigned nr_bytes)
{
  dv_brew_simctl *simctl = (dv_brew_simctl *)hw_data(me);
  const uint8_t *bytes = (const uint8_t *)source;
  uint32_t ofs = addr - simctl->base_addr;
  uint32_t value = 0;
  unsigned i;

  // Anything but an access at offset 0 is a bus error (the core reports it)
  if (ofs != SIMCTL_EXIT_REG_OFS || (nr_bytes != 1 && nr_bytes != 2 && nr_bytes != 4))
    return 0;
  for (i = 0; i < nr_bytes; ++i)
    value |= (uint32_t)bytes[i] << (8 * i);
  sim_engine_halt(hw_system(me), hw_system_cpu(me), NULL, NULL_CIA, sim_exited, (int)value);
  return nr_bytes;
}

static unsigned dv_brew_simctl_io_read_buffer(struct hw *me, void *dest, int space, address_word addr, unsigned nr_bytes)
{
  dv_brew_simctl *simctl = (dv_brew_simctl *)hw_data(me);
  uint8_t *bytes = (uint8_t *)dest;
  uint32_t ofs = addr - simctl->base_addr;
  unsigned i;

  if (ofs != SIMCTL_EXIT_REG_OFS || (nr_bytes != 1 && nr_bytes != 2 && nr_bytes != 4))
    return 0;
  for (i = 0; i < nr_bytes; ++i)
    bytes[i] = (uint8_t)(SIMCTL_ID >> (8 * i));
  return nr_bytes;
}

static void brew_simctl_finish(struct hw *me)
{
  int attach_space;
  unsigned attach_size;
  reg_property_spec reg;
  address_word attach_address;
  dv_brew_simctl *simctl = HW_ZALLOC(me, dv_brew_simctl);

  set_hw_data(me, simctl);
  set_hw_io_read_buffer(me, dv_brew_simctl_io_read_buffer);
  set_hw_io_write_buffer(me, dv_brew_simctl_io_write_buffer);

  if (hw_find_property(me, "reg") == NULL)
    hw_abort(me, "Missing \"reg\" property");
  if (!hw_find_reg_array_property(me, "reg", 0, &reg))
    hw_abort(me, "\"reg\" property must contain one addr/size entry");

  hw_unit_address_to_attach_address(hw_parent(me), &reg.address, &attach_space, &attach_address, me);
  hw_unit_size_to_attach_size(hw_parent(me), &reg.size, &attach_size, me);
  if (attach_size != SIMCTL_SIZE)
    hw_abort(me, "Simulator control must occupy %d bytes of memory space", SIMCTL_SIZE);

  hw_attach_address(hw_parent(me), 0, attach_space, attach_address, attach_size, me);
  simctl->base_addr = attach_address;
}

const struct hw_descriptor dv_brew_simctl_descriptor[] =
{
  {"brew_simctl", brew_simctl_finish,},
  {NULL, NULL},
};
