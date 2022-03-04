from framework import *
from enum import Enum
from typing import Optional
from sys import stderr

def bswap(val):
    return \
        (val & 0x000000ff) << 24 | \
        (val & 0x0000ff00) << 8  | \
        (val & 0x00ff0000) >> 8  | \
        (val & 0xff000000) >> 24

def wswap(val):
    return \
        (val & 0x0000ffff) << 16 | \
        (val & 0xffff0000) >> 16

def bsi(val):
    if val & 0x80:
        return (val & 0xff) | 0xffffff00
    return val & 0xff

def wsi(val):
    if val & 0x8000:
        return (val & 0xffff) | 0xffff0000
    return val & 0xffff

def floor(val):
    from math import floor as mfloor
    return mfloor(val)

def rsqrt(val):
    from math import sqrt
    return 1.0 / sqrt(val)

def test_u(r_d, r_in, op:str, init_float: bool = False):
    init_regs(init_float)
    print_header()
    setup()
    left_side = op.replace("***", "gas").replace("#("," ").replace("#)"," ").format(r_in = r_in)
    print(f"        #>>>>>>>")
    print(f"        {r_d:gas} <- {left_side}")
    print(f"        #<<<<<<<")
    left_side = op.replace("***", "val").replace("#(","(").replace("#)",")").format(r_in = r_in)
    raw_result = eval(left_side)
    r_d.set_val(raw_result & 0xffffffff)
    test()
    print()

def test_s(r_d, r_in, op:str, init_float: bool = False):
    init_regs(init_float)
    print_header()
    setup()
    left_side = op.replace("***", "gas").replace("#("," ").replace("#)"," ").format(r_in = r_in)
    print(f"        #>>>>>>>")
    print(f"        {r_d:gas_s} <- {left_side}")
    print(f"        #<<<<<<<")
    left_side = op.replace("***", "val").replace("#(","(").replace("#)",")").format(r_in = r_in)
    raw_result = eval(left_side)
    r_d.set_val(raw_result & 0xffffffff)
    test()
    print()

def test_f(r_d, r_in, op:str, init_float: bool = True):
    init_regs(init_float)
    print_header()
    setup()
    left_side = op.replace("***", "gas").replace("#("," ").replace("#)"," ").format(r_in = r_in)
    print(f"        #>>>>>>>")
    print(f"        {r_d:gas_f} <- {left_side}")
    print(f"        #<<<<<<<")
    left_side = op.replace("***", "val").replace("#(","(").replace("#)",")").format(r_in = r_in)
    raw_result = float(eval(left_side))
    r_d.set_val(float_to_int(raw_result) & 0xffffffff)
    test()
    print()

start()
#test_u(reg[3], reg[4], "{r_in:***} + 1")
#test_u(reg[3], reg[4], "{r_in:***} - 1")
#test_u(reg[3], reg[4], "~{r_in:***}")
#test_s(reg[3], reg[4], "-{r_in:***_s}")
#test_u(reg[3], reg[4], "bswap#({r_in:***_u}#)")
#test_u(reg[3], reg[4], "wswap#({r_in:***_u}#)")
#test_s(reg[3], reg[4], "bsi#({r_in:***_u}#)")
#test_s(reg[3], reg[4], "wsi#({r_in:***_u}#)")
#test_s(reg[3], reg[4], "floor#({r_in:***_f}#)")
#test_f(reg[3], reg[4], "{r_in:***_f}", False)
#test_f(reg[2], reg[2], "1/{r_in:***_f}")
#test_f(reg[3], reg[4], "rsqrt#({r_in:***_f}#)")

for r_d in range(0,15):
    for r_a in range(0,15):
        test_u(reg[r_d], reg[r_a], "{r_in:***} + 1")
        test_u(reg[r_d], reg[r_a], "{r_in:***} - 1")
        test_u(reg[r_d], reg[r_a], "~{r_in:***}")
        test_s(reg[r_d], reg[r_a], "-{r_in:***_s}")
        test_u(reg[r_d], reg[r_a], "bswap#({r_in:***_u}#)")
        test_u(reg[r_d], reg[r_a], "wswap#({r_in:***_u}#)")
        test_s(reg[r_d], reg[r_a], "bsi#({r_in:***_u}#)")
        test_s(reg[r_d], reg[r_a], "wsi#({r_in:***_u}#)")
        test_s(reg[r_d], reg[r_a], "floor#({r_in:***_f}#)", True)
        test_f(reg[r_d], reg[r_a], "{r_in:***_s}", False)
        test_f(reg[r_d], reg[r_a], "1/{r_in:***_f}")

# Have to be careful not to try all the negative numbers
for r_d in range(0,15):
    for r_a in range(0,15,2):
        test_f(reg[r_d], reg[r_a], "rsqrt#({r_in:***_f}#)")
final()
