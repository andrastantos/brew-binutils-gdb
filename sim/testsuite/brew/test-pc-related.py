from framework import *
from enum import Enum
from typing import Optional

def test_link(r_d, offset):
    init_regs(False)
    print_header()
    setup()
    print(f"        #>>>>>>>")
    print(f"        $r0 <- $pc")
    print(f"        {r_d:gas_u} <- $pc + {offset}")
    print(f"        {r_d:gas_u} <- {r_d:gas_u} - $r0")
    print(f"        #<<<<<<<")
    r_d.set_val(2+offset)
    test(reg[1:])
    print()

def test_tpc_get(r_d):
    init_regs(False)
    print_header()
    setup()
    print(f"        #>>>>>>>")
    print(f"        $r0 <- $pc")
    print(f"        {r_d:gas_u} <- $tpc")
    print(f"        {r_d:gas_u} <- {r_d:gas_u} - $r0")
    print(f"        #<<<<<<<")
    r_d.set_val(2)
    test(reg[1:])
    print()

def test_indirect_jump(r_d, use_tpc):
    init_regs(False)
    print_header()
    setup()
    print(f"        #>>>>>>>")
    print(f"        $r0 <- 0xc001c001")
    print(f"        {r_d:gas_u} <- 3f")
    print(f"        {'$tpc' if use_tpc else '$pc'} <- {r_d:gas_u}")
    print(f"        $r0 <- 0xdeadbeef")
    print(f"    3:")
    print(f"        {r_d:gas_u} <- $r0")
    print(f"        #<<<<<<<")
    reg[0].set_val(0xc001c001)
    r_d.set_val(0xc001c001)
    test()
    print()

def test_pc_load(r_b, offset):
    init_regs(False)
    print_header()
    if r_b is None and offset is not None:
        mem_ref = f"mem[{offset}]"
    elif r_b is not None and offset is None:
        mem_ref = f"mem[{r_b:gas_u}]"
    elif r_b is not None and offset is not None:
        mem_ref = f"mem[{r_b:gas_u},{offset}]"
    else:
        assert False, "Either r_b or offset (or both) must be specified"
    setup()
    print(f"        #>>>>>>>")
    print(f"        $r0 <- 3f")
    print(f"        {r_b:gas_u} <- .buffer")
    print(f"        {mem_ref} <- $r0")
    print(f"        $r0 <- 0xc001c001")
    print(f"        $pc <- {mem_ref}")
    print(f"        $r0 <- 0xdeadbeef")
    print(f"    3:")
    print(f"        {r_b:gas_u} <- $r0")
    print(f"        #<<<<<<<")
    reg[0].set_val(0xc001c001)
    r_b.set_val(0xc001c001)
    test()
    print()


start()
#test_link(reg[3], 4)
#test_tpc_get(reg[5])
#test_indirect_jump(reg[1], False)
#test_indirect_jump(reg[1], True)
test_pc_load(reg[10], None)
final()
