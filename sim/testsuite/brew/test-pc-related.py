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
        mem_ref = f"mem[{offset}+.buffer+128]"
    elif r_b is not None and offset is None:
        mem_ref = f"mem[{r_b:gas_u}]"
    elif r_b is not None and offset is not None:
        mem_ref = f"mem[{r_b:gas_u},{offset}]"
    else:
        assert False, "Either r_b or offset (or both) must be specified"
    setup()
    print(f"        #>>>>>>>")
    print(f"        $r0 <- 3f")
    if r_b is not None:
        print(f"        {r_b:gas_u} <- .buffer")
        print(f"        {r_b:gas_u} <- {r_b:gas_u} + 128")
    print(f"        {mem_ref} <- $r0")
    print(f"        $r0 <- 0xc001c001")
    print(f"        $pc <- {mem_ref}")
    print(f"        $r0 <- 0xdeadbeef")
    print(f"    3:")
    if r_b is not None:
        print(f"        {r_b:gas_u} <- $r0")
    print(f"        #<<<<<<<")
    reg[0].set_val(0xc001c001)
    if r_b is not None:
        r_b.set_val(0xc001c001)
    test()
    print()

def test_cbranch_rr(r_a, r_b, is_signed, is_float, cond):
    init_regs(is_float)
    print_header()
    setup()

    a_val = as_float(r_a.get_val()) if is_float else as_sint32(r_a.get_val()) if is_signed else as_uint32(r_a.get_val())
    b_val = as_float(r_b.get_val()) if is_float else as_sint32(r_b.get_val()) if is_signed else as_uint32(r_b.get_val())
    taken = eval(f"({a_val} {cond} {b_val})")

    print(f"        #>>>>>>>")
    print(f"        $r0 <- 0xc001c001")
    if is_float:
        print(f"        if {r_a:gas_f} {cond} {r_b:gas_f} $pc <- 3f")
    elif is_signed:
        print(f"        if {r_a:gas_s} {cond} {r_b:gas_s} $pc <- 3f")
    else:
        print(f"        if {r_a:gas_u} {cond} {r_b:gas_u} $pc <- 3f")
    print(f"        $r0 <- 0xdeadbeef")
    print(f"    3:")
    print(f"        #<<<<<<<")
    reg[0].set_val(0xc001c001 if taken else 0xdeadbeef)
    test()
    print()

def test_cbranch_rz(r_a, is_signed, is_float, cond):
    init_regs(is_float)
    print_header()
    setup()

    a_val = as_float(r_a.get_val()) if is_float else as_sint32(r_a.get_val()) if is_signed else as_uint32(r_a.get_val())
    taken = eval(f"({a_val} {cond} 0)")

    print(f"        #>>>>>>>")
    print(f"        $r0 <- 0xc001c001")
    if is_float:
        print(f"        if {r_a:gas_f} {cond} 0 $pc <- 3f")
    elif is_signed:
        print(f"        if {r_a:gas_s} {cond} 0 $pc <- 3f")
    else:
        print(f"        if {r_a:gas_u} {cond} 0 $pc <- 3f")
    print(f"        $r0 <- 0xdeadbeef")
    print(f"    3:")
    print(f"        #<<<<<<<")
    reg[0].set_val(0xc001c001 if taken else 0xdeadbeef)
    test()
    print()

def test_cbranch_bit(r_a, bit_idx, value):
    init_regs(False)
    print_header()
    setup()

    a_val = as_uint32(r_a.get_val())
    taken = ((a_val >> bit_idx) & 1) == value

    print(f"        #>>>>>>>")
    print(f"        $r0 <- 0xc001c001")
    print(f"        if {r_a:gas_u}[{bit_idx}] == {value} $pc <- 3f")
    print(f"        $r0 <- 0xdeadbeef")
    print(f"    3:")
    print(f"        #<<<<<<<")
    reg[0].set_val(0xc001c001 if taken else 0xdeadbeef)
    test()
    print()



start()
#test_link(reg[3], 4)
#test_tpc_get(reg[5])
#test_indirect_jump(reg[1], False)
#test_indirect_jump(reg[1], True)
#test_pc_load(reg[10], None)
#test_pc_load(reg[10], 4)
#test_pc_load(reg[10], -4)
#test_pc_load(None, 4)
#test_cbranch_rr(reg[1], reg[2], False, False, "==")
#test_cbranch_rr(reg[1], reg[2], False, False, "!=")
#test_cbranch_rr(reg[1], reg[2], False, False, "<")
#test_cbranch_rr(reg[1], reg[2], False, False, "<=")
#test_cbranch_rr(reg[1], reg[2], False, False, ">")
#test_cbranch_rr(reg[1], reg[2], False, False, ">=")
#
#test_cbranch_rr(reg[1], reg[2], False, True, "==")
#test_cbranch_rr(reg[1], reg[2], False, True, "!=")
#test_cbranch_rr(reg[1], reg[2], False, True, "<")
#test_cbranch_rr(reg[1], reg[2], False, True, "<=")
#test_cbranch_rr(reg[1], reg[2], False, True, ">")
#test_cbranch_rr(reg[1], reg[2], False, True, ">=")
#
#test_cbranch_rr(reg[1], reg[2], True, False, "==")
#test_cbranch_rr(reg[1], reg[2], True, False, "!=")
#test_cbranch_rr(reg[1], reg[2], True, False, "<")
#test_cbranch_rr(reg[1], reg[2], True, False, "<=")
#test_cbranch_rr(reg[1], reg[2], True, False, ">")
#test_cbranch_rr(reg[1], reg[2], True, False, ">=")
#
#test_cbranch_rz(reg[1], False, False, "==")
#test_cbranch_rz(reg[1], False, False, "!=")
#test_cbranch_rz(reg[1], False, False, "<")
#test_cbranch_rz(reg[1], False, False, "<=")
#test_cbranch_rz(reg[1], False, False, ">")
#test_cbranch_rz(reg[1], False, False, ">=")
#
#test_cbranch_rz(reg[1], False, True, "==")
#test_cbranch_rz(reg[1], False, True, "!=")
#test_cbranch_rz(reg[1], False, True, "<")
#test_cbranch_rz(reg[1], False, True, "<=")
#test_cbranch_rz(reg[1], False, True, ">")
#test_cbranch_rz(reg[1], False, True, ">=")
#
#test_cbranch_rz(reg[1], True, False, "==")
#test_cbranch_rz(reg[1], True, False, "!=")
#test_cbranch_rz(reg[1], True, False, "<")
#test_cbranch_rz(reg[1], True, False, "<=")
#test_cbranch_rz(reg[1], True, False, ">")
#test_cbranch_rz(reg[1], True, False, ">=")

#test_cbranch_bit(reg[3], 3, 0)
#test_cbranch_bit(reg[3], 3, 1)

for r_idx in range(1,15):
    for bit in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 15, 16, 30, 31):
        test_cbranch_bit(reg[r_idx], bit, 0)
        test_cbranch_bit(reg[r_idx], bit, 1)

for r_idx in range(1,15):
    test_cbranch_rz(reg[r_idx], False, False, "==")
    test_cbranch_rz(reg[r_idx], False, False, "!=")
    test_cbranch_rz(reg[r_idx], False, False, "<")
    test_cbranch_rz(reg[r_idx], False, False, "<=")
    test_cbranch_rz(reg[r_idx], False, False, ">")
    test_cbranch_rz(reg[r_idx], False, False, ">=")
    test_cbranch_rz(reg[r_idx], False, True, "==")
    test_cbranch_rz(reg[r_idx], False, True, "!=")
    test_cbranch_rz(reg[r_idx], False, True, "<")
    test_cbranch_rz(reg[r_idx], False, True, "<=")
    test_cbranch_rz(reg[r_idx], False, True, ">")
    test_cbranch_rz(reg[r_idx], False, True, ">=")
    test_cbranch_rz(reg[r_idx], True, False, "==")
    test_cbranch_rz(reg[r_idx], True, False, "!=")
    test_cbranch_rz(reg[r_idx], True, False, "<")
    test_cbranch_rz(reg[r_idx], True, False, "<=")
    test_cbranch_rz(reg[r_idx], True, False, ">")
    test_cbranch_rz(reg[r_idx], True, False, ">=")

for r_a in range(1,15):
    for r_b in range(1,15):
        test_cbranch_rr(reg[r_a], reg[r_b], False, False, "==")
        test_cbranch_rr(reg[r_a], reg[r_b], False, False, "!=")
        test_cbranch_rr(reg[r_a], reg[r_b], False, False, "<")
        test_cbranch_rr(reg[r_a], reg[r_b], False, False, "<=")
        test_cbranch_rr(reg[r_a], reg[r_b], False, False, ">")
        test_cbranch_rr(reg[r_a], reg[r_b], False, False, ">=")
        test_cbranch_rr(reg[r_a], reg[r_b], False, True, "==")
        test_cbranch_rr(reg[r_a], reg[r_b], False, True, "!=")
        test_cbranch_rr(reg[r_a], reg[r_b], False, True, "<")
        test_cbranch_rr(reg[r_a], reg[r_b], False, True, "<=")
        test_cbranch_rr(reg[r_a], reg[r_b], False, True, ">")
        test_cbranch_rr(reg[r_a], reg[r_b], False, True, ">=")
        test_cbranch_rr(reg[r_a], reg[r_b], True, False, "==")
        test_cbranch_rr(reg[r_a], reg[r_b], True, False, "!=")
        test_cbranch_rr(reg[r_a], reg[r_b], True, False, "<")
        test_cbranch_rr(reg[r_a], reg[r_b], True, False, "<=")
        test_cbranch_rr(reg[r_a], reg[r_b], True, False, ">")
        test_cbranch_rr(reg[r_a], reg[r_b], True, False, ">=")

for r_idx in range(1,15):
    for ofs in (None, 0,4,8,-12):
        test_tpc_get(reg[r_idx])
        test_indirect_jump(reg[r_idx], False)
        test_indirect_jump(reg[r_idx], True)
        test_pc_load(reg[r_idx], ofs)
        if ofs is not None:
            test_pc_load(None, ofs)

for r_idx in range(1,15):
    for ofs in (0,2,4,8,16,28):
        test_link(reg[r_idx], ofs)

final()
