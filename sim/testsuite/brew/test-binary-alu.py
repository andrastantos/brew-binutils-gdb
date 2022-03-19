from framework import *
from enum import Enum
from typing import Optional

def op_prefix(in_a, in_b) -> str:
    if in_a.is_short or in_b.is_short:
        return "short"
    return ""

def test_uuu(r_d, in_a, in_b, op, is_upper = False):
    init_regs(False)
    upper_str = "upper" if is_upper else ""
    print_header()
    setup()
    print(f"        #>>>>>>>")
    print(f"        {r_d:gas} <- {upper_str} {op_prefix(in_a, in_b)} {in_a:gas} {op} {in_b:gas}")
    print(f"        #<<<<<<<")
    b_val = as_uint32(in_b.get_val())
    if op == ">>" or op == "<<":
        b_val = b_val & 31
    raw_result = eval(f"({as_uint32(in_a.get_val())} {op} {b_val})")
    if is_upper:
        raw_result = (raw_result & 0xffff_ffff_ffff_ffff) >> 32
    r_d.set_val(raw_result & 0xffffffff)
    test()
    print()

def test_sss(r_d, in_a, in_b, op, is_upper = False):
    init_regs(False)
    upper_str = "upper" if is_upper else ""
    print_header()
    setup()
    print(f"        #>>>>>>>")
    print(f"        {r_d:gas_s} <- {upper_str} {op_prefix(in_a, in_b)} {in_a:gas_s} {op} {in_b:gas_s}")
    print(f"        #<<<<<<<")
    raw_result = eval(f"({as_sint32(in_a.get_val())} {op} {as_sint32(in_b.get_val())})")
    if is_upper:
        raw_result = raw_result >> 32
    r_d.set_val(raw_result & 0xffffffff)
    test()
    print()

def test_ssu(r_d, in_a, in_b, op):
    init_regs(False)
    print_header()
    setup()
    print(f"        #>>>>>>>")
    print(f"        {r_d:gas_s} <- {op_prefix(in_a, in_b)} {in_a:gas_s} {op} {in_b:gas_u}")
    print(f"        #<<<<<<<")
    b_val = as_uint32(in_b.get_val())
    if op == ">>" or op == "<<":
        b_val = b_val & 31
    raw_result = eval(f"({as_sint32(in_a.get_val())} {op} {b_val})")
    r_d.set_val(raw_result & 0xffffffff)
    test()
    print()

def test_fff(r_d, in_a, in_b, op):
    init_regs(True)
    print_header()
    setup()
    print(f"        #>>>>>>>")
    print(f"        {r_d:gas_f} <- {in_a:gas_f} {op} {in_b:gas_f}")
    print(f"        #<<<<<<<")
    raw_result = eval(f"({as_float(in_a.get_val())} {op} {as_float(in_b.get_val())})")
    int_result = float_to_int(raw_result)
    print(f"        # float: {raw_result} int: {int_result}")
    r_d.set_val(int_result)
    test()
    print()

start()
#test_uuu(reg[3], reg[2], reg[1], "+")
#test_uuu(reg[3], reg[2], simm(0xffff), "+")
#test_sss(reg[3], reg[2], reg[1], "+")
#test_sss(reg[3], reg[2], simm(0xffff), "+")
#test_fff(reg[3], reg[2], fimm(1), "+")
#test_fff(reg[3], fimm(1), reg[2], "-")
#test_fff(reg[3], fimm(-1), reg[2], "-")
#test_fff(reg[3], reg[2], reg[14], "*")
#test_ssu(reg[3], reg[13], reg[2], ">>")
#test_uuu(reg[3], reg[13], reg[2], ">>")
test_sss(reg[3],reg[3],reg[3],"*", True)

for r_d in range(0,15,3):
    for r_a in range(0,15,3):
        for r_b in range(0,15,3):
            test_uuu(reg[r_d],reg[r_a],reg[r_b],"^")
            test_uuu(reg[r_d],reg[r_a],reg[r_b],"|")
            test_uuu(reg[r_d],reg[r_a],reg[r_b],"&")
            test_uuu(reg[r_d],reg[r_a],reg[r_b],"+")
            test_uuu(reg[r_d],reg[r_a],reg[r_b],"-")
            test_uuu(reg[r_d],reg[r_a],reg[r_b],"<<")
            test_uuu(reg[r_d],reg[r_a],reg[r_b],">>")
            test_ssu(reg[r_d],reg[r_a],reg[r_b],">>")
            test_uuu(reg[r_d],reg[r_a],reg[r_b],"*")
            test_uuu(reg[r_d],reg[r_a],reg[r_b],"*", True)
            test_sss(reg[r_d],reg[r_a],reg[r_b],"*", True)
            test_fff(reg[r_d],reg[r_a],reg[r_b],"+")
            test_fff(reg[r_d],reg[r_a],reg[r_b],"-")
            test_fff(reg[r_d],reg[r_a],reg[r_b],"*")
final()
