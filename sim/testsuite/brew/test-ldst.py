from framework import *
from enum import Enum
from typing import Optional

def bsi(val):
    if val & 0x80:
        return (val & 0xff) | 0xffffff00
    return val & 0xff

def wsi(val):
    if val & 0x8000:
        return (val & 0xffff) | 0xffff0000
    return val & 0xffff

def test_ld_st(r_d, r_b, offset, size, sign_extend):
    init_regs(False)
    print_header()
    if r_b is None and offset is not None:
        mem_ref = f"mem{size}[{offset}+.buffer+128]"
    elif r_b is not None and offset is None:
        mem_ref = f"mem{size}[{r_b:gas_u}]"
    elif r_b is not None and offset is not None:
        mem_ref = f"mem{size}[{r_b:gas_u},{offset}]"
    else:
        assert False, "Either r_b or offset (or both) must be specified"
    print(f"        $r0 <- .buffer")
    print(f"        $r1 <- .buffer_end")
    print(f"        $r2 <- $r2 ^ $r2")
    print(f"1:")
    print(f"        mem[$r0] <- $r2")
    print(f"        $r0 <- short $r0 + 4")
    print(f"        if $r0 != $r1 $pc <- 1b")
    setup()
    print(f"        #>>>>>>>")
    if r_b is not None:
        print(f"        {r_b:gas_u} <- .buffer")
        print(f"        {r_b:gas_u} <- {r_b:gas_u} + 128")
    print(f"        {mem_ref} <- {r_d:gas_u}")
    print(f"        {r_d:gas_u} <- 0xdeadbeef")
    if sign_extend:
        print(f"        {r_d:gas_s} <- {mem_ref}")
    else:
        print(f"        {r_d:gas_u} <- {mem_ref}")
    if r_b is not None:
        print(f"        {r_b:gas_u} <- {r_d:gas_u}")
    print(f"        #<<<<<<<")
    if sign_extend:
        if size == 8:
            exp_val = bsi(r_d.get_val())
        elif size == 16:
            exp_val = wsi(r_d.get_val())
        else:
            exp_val = r_d.get_val()
    else:
        if size == 8:
            exp_val = r_d.get_val() & 0xff
        elif size == 16:
            exp_val = r_d.get_val() & 0xffff
        else:
            exp_val = r_d.get_val()
    r_d.set_val(exp_val)
    if r_b is not None:
        r_b.set_val(exp_val)
    test()
    print()




start()

test_ld_st(reg[1], reg[0], 4,    16, False)
#test_ld_st(reg[1], reg[0], 4,    16, True)

#for offset in (None, 4, -4, 100)
for r_d in range(0,15):
    for r_b in range(0,15):
        if r_d == r_b:
            continue
        test_ld_st(reg[r_d], reg[r_b], 4,    32, False)
        test_ld_st(reg[r_d], reg[r_b], None, 32, False)
        test_ld_st(reg[r_d], None,     4,    32, False)
        test_ld_st(reg[r_d], reg[r_b], 4,    16, False)
        test_ld_st(reg[r_d], reg[r_b], None, 16, False)
        test_ld_st(reg[r_d], None,     4,    16, False)
        test_ld_st(reg[r_d], reg[r_b], 4,     8, False)
        test_ld_st(reg[r_d], reg[r_b], None,  8, False)
        test_ld_st(reg[r_d], None,     4,     8, False)
        test_ld_st(reg[r_d], reg[r_b], 4,    32, True)
        test_ld_st(reg[r_d], reg[r_b], None, 32, True)
        test_ld_st(reg[r_d], None,     4,    32, True)
        test_ld_st(reg[r_d], reg[r_b], 4,    16, True)
        test_ld_st(reg[r_d], reg[r_b], None, 16, True)
        test_ld_st(reg[r_d], None,     4,    16, True)
        test_ld_st(reg[r_d], reg[r_b], 4,     8, True)
        test_ld_st(reg[r_d], reg[r_b], None,  8, True)
        test_ld_st(reg[r_d], None,     4,     8, True)


final()
