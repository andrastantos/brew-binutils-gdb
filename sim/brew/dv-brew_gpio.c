/* GPIO model.

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

#include "defs.h"

//#define basename basename
#include "bfd.h"
//#undef basename

//#include "libiberty.h"
//#include "sim/sim.h"
#include "sim-main.h"
#include "sim-signal.h"
#include "hw-main.h"

#include "dv-brew_gpio.h"

typedef struct dv_brew_gpio
{
  uint32_t base_addr;
  bool terminate_on_write;
} dv_brew_gpio;

static void dv_brew_gpio_init(dv_brew_gpio *gpio)
{
  gpio->terminate_on_write = FALSE;
}

static void dv_brew_gpio_set_base_addr(dv_brew_gpio *gpio, uint32_t base_addr) {
  gpio->base_addr = base_addr;
}

static unsigned dv_brew_gpio_io_write_buffer(struct hw *me, const void *source, int space, address_word addr, unsigned nr_bytes)
{
  SIM_DESC sd = hw_system(me);
  dv_brew_gpio *gpio = (dv_brew_gpio *)hw_data(me);
  uint32_t ofs = addr - gpio->base_addr;

  SIM_ASSERT(nr_bytes == 1);
  switch (ofs) {
    case GPIO_DATA_REG_OFS:
      if (gpio->terminate_on_write)
        {
          sim_engine_halt(hw_system(me), hw_system_cpu(me), NULL, NULL_CIA, sim_exited, SIM_SIGNONE);
        }
      return 1;
    break;
    default:
      return 0;
    break;
  }
  return nr_bytes;
}

static unsigned dv_brew_gpio_io_read_buffer(struct hw *me, void *dest, int space, address_word addr, unsigned nr_bytes)
{
  SIM_DESC sd = hw_system(me);
  dv_brew_gpio *gpio = (dv_brew_gpio *)hw_data(me);
  uint32_t ofs = addr - gpio->base_addr;

  SIM_ASSERT(nr_bytes == 1);
  switch (ofs) {
    case GPIO_DATA_REG_OFS:
      // Bit 0 is set fo RTL sims, Bit 1 is set for SW sims
      *(char*)(dest) = 0x02;
      return 1;
    break;
    default:
      return 0;
    break;
  }
  hw_abort(me, "Should never get here");
}

static dv_brew_gpio *attach_gpio_regs(struct hw *me)
{
  int attach_space;
  unsigned attach_size;
  reg_property_spec reg;
  address_word attach_address;

  dv_brew_gpio *gpio = HW_ZALLOC(me, dv_brew_gpio);

  dv_brew_gpio_init(gpio);

  if (hw_args(me) != NULL && strcmp(hw_args(me), "terminate_on_write") == 0) {
    gpio->terminate_on_write = TRUE;
  }

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

  if (attach_size != 1)
    hw_abort(me, "GPO must occupy 1 bytes of memory space");

  // Attach ourselves to the address space
  hw_attach_address(
    hw_parent (me),
    0,
    attach_space,
    attach_address,
    attach_size,
    me
  );

  dv_brew_gpio_set_base_addr(gpio, attach_address);
  set_hw_data(me, gpio);
  return gpio;
}

static void brew_gpio_finish(struct hw *me)
{
  set_hw_io_read_buffer(me, dv_brew_gpio_io_read_buffer);
  set_hw_io_write_buffer(me, dv_brew_gpio_io_write_buffer);

  attach_gpio_regs(me);
}

const struct hw_descriptor dv_brew_gpio_descriptor[] =
{
  {"brew_gpio", brew_gpio_finish,},
  {NULL, NULL},
};
