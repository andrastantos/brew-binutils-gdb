from framework import *
from enum import Enum
from typing import Optional

def op_prefix(in_a, in_b) -> str:
    if in_a.is_short or in_b.is_short:
        return "short"
    return ""

def test_uuu(r_d, in_a, in_b, op):
    for i in range(0,15):
        reg[i].set_val((-1)**i * (1 << i))
    print_header()
    setup()
    print(f"        #>>>>>>>")
    print(f"        {r_d:gas} <- {op_prefix(in_a, in_b)} {in_a:gas} {op} {in_b:gas}")
    print(f"        #<<<<<<<")
    raw_result = eval(f"({in_a.get_val()} {op} {in_b.get_val()})")
    r_d.set_val(raw_result & 0xffffffff)
    test()
    print()

def test_sss(r_d, in_a, in_b, op):
    for i in range(0,15):
        reg[i].set_val((-1)**i * (1 << i))
    print_header()
    setup()
    print(f"        #>>>>>>>")
    print(f"        {r_d:gas_s} <- {op_prefix(in_a, in_b)} {in_a:gas_s} {op} {in_b:gas_s}")
    print(f"        #<<<<<<<")
    raw_result = eval(f"({as_sint32(in_a.get_val())} {op} {as_sint32(in_b.get_val())})")
    r_d.set_val(raw_result & 0xffffffff)
    test()
    print()

def test_ssu(r_d, in_a, in_b, op):
    for i in range(0,15):
        reg[i].set_val((-1)**i * (1 << i))
    print_header()
    setup()
    print(f"        #>>>>>>>")
    print(f"        {r_d:gas_s} <- {op_prefix(in_a, in_b)} {in_a:gas_s} {op} {in_b:gas_u}")
    print(f"        #<<<<<<<")
    raw_result = eval(f"({as_sint32(in_a.get_val())} {op} {as_uint32(in_b.get_val())})")
    r_d.set_val(raw_result & 0xffffffff)
    test()
    print()

def test_fff(r_d, in_a, in_b, op):
    for i in range(0,15):
        reg[i].set_val(float_to_int(1.0 * i))
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
test_ssu(reg[3], reg[13], reg[2], ">>")
test_uuu(reg[3], reg[13], reg[2], ">>")

#for r_d in range(0,14):
#    for r_a in range(0,14):
#        for r_b in range(0,14):
#            test_rr(r_d,r_a,r_b,"+")
final()
