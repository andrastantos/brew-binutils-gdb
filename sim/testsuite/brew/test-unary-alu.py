from framework import *
from enum import Enum
from typing import Optional
from sys import stderr
import re

# Op templates are used both for the assembly and (through eval) to compute the expected
# result, so the helpers below are named after the assembly mnemonics:
#   {r_in:***}    the operand; '***_u', '***_s', '***_f' select how its value is interpreted
#                 for the expected result (unsigned, signed, float). '***_f' also makes the
#                 operand register fp32-typed for the operation.
#   #( and #)     parentheses that only appear in the eval'd expression

def bse(val):
    if val & 0x80:
        return (val & 0xff) | 0xffffff00
    return val & 0xff

def wse(val):
    if val & 0x8000:
        return (val & 0xffff) | 0xffff0000
    return val & 0xffff

# Type conversions ($rD <- int32 $rA, $rD <- fp32 $rA)
def int32(val):
    # Round to nearest (the default rounding mode). All test inputs are integral anyway.
    return round(val)

def fp32(val):
    return float(val)

def rsqrt(val):
    from math import sqrt
    return 1.0 / sqrt(val)

def asm_side(op: str, r_in) -> str:
    return re.sub(r"\*\*\*(_[usf])?", "gas", op).replace("#(", " ").replace("#)", " ").format(r_in = r_in)

def val_side(op: str, r_in) -> str:
    return op.replace("***", "val").replace("#(", "(").replace("#)", ")").format(r_in = r_in)

def emit_op(r_d, r_in, op: str):
    in_float = "***_f" in op
    print(f"        #>>>>>>>")
    if in_float:
        set_type("fp32", r_in)
    print(f"        {r_d:gas} <- {asm_side(op, r_in)}")
    # The result can be fp32 (from the operand type or a conversion); go back to int32 for checking
    reset_types(r_d, r_in)
    print(f"        #<<<<<<<")

def test_u(r_d, r_in, op:str, init_float: bool = False):
    init_regs(init_float)
    print_header()
    setup()
    emit_op(r_d, r_in, op)
    raw_result = eval(val_side(op, r_in))
    r_d.set_val(raw_result & 0xffffffff)
    test()
    print()

def test_s(r_d, r_in, op:str, init_float: bool = False):
    init_regs(init_float)
    print_header()
    setup()
    emit_op(r_d, r_in, op)
    raw_result = eval(val_side(op, r_in))
    r_d.set_val(raw_result & 0xffffffff)
    test()
    print()

def test_f(r_d, r_in, op:str, init_float: bool = True):
    init_regs(init_float)
    print_header()
    setup()
    emit_op(r_d, r_in, op)
    raw_result = float(eval(val_side(op, r_in)))
    r_d.set_val(float_to_int(raw_result) & 0xffffffff)
    test()
    print()

start()
#test_u(reg[3], reg[4], "{r_in:***} + 1")
#test_u(reg[3], reg[4], "{r_in:***} - 1")
#test_u(reg[3], reg[4], "~{r_in:***}")
#test_s(reg[3], reg[4], "-{r_in:***_s}")
#test_s(reg[3], reg[4], "bse#({r_in:***_u}#)")
#test_s(reg[3], reg[4], "wse#({r_in:***_u}#)")
#test_s(reg[3], reg[4], "int32#({r_in:***_f}#)", True)
#test_f(reg[3], reg[4], "fp32#({r_in:***_s}#)", False)
#test_f(reg[2], reg[2], "1/{r_in:***_f}")
#test_f(reg[3], reg[4], "rsqrt#({r_in:***_f}#)")

# NOTE: the old bswap/wswap tests were removed: those instructions are not part of the ISA any more.
for r_d in range(0,15):
    for r_a in range(0,15):
        test_u(reg[r_d], reg[r_a], "{r_in:***} + 1")
        test_u(reg[r_d], reg[r_a], "{r_in:***} - 1")
        test_u(reg[r_d], reg[r_a], "~{r_in:***}")
        test_s(reg[r_d], reg[r_a], "-{r_in:***_s}")
        test_s(reg[r_d], reg[r_a], "bse#({r_in:***_u}#)")
        test_s(reg[r_d], reg[r_a], "wse#({r_in:***_u}#)")
        test_s(reg[r_d], reg[r_a], "int32#({r_in:***_f}#)", True)
        test_f(reg[r_d], reg[r_a], "fp32#({r_in:***_s}#)", False)
        test_f(reg[r_d], reg[r_a], "1/{r_in:***_f}")

# Have to be careful not to try all the negative numbers
for r_d in range(0,15):
    for r_a in range(0,15,2):
        test_f(reg[r_d], reg[r_a], "rsqrt#({r_in:***_f}#)")
final()
