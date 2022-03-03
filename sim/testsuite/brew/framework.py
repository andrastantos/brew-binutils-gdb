import inspect
from enum import Enum
from typing import Optional
import struct

def as_float(integer):
    return struct.unpack('!f', struct.pack('!I', integer))[0]

def float_to_int(flt):
    return struct.unpack('!I', struct.pack('!f', flt))[0]

def as_sint32(integer):
    if integer == 0x80000000:
        return -integer
    if not integer & 0x80000000:
        return integer & 0x7fffffff
    return -((-integer) & 0x7fffffff)

def as_uint32(integer):
    return integer & 0xffffffff

def as_sint16(integer):
    if integer == 0x8000:
        return -integer
    if not integer & 0x8000:
        return integer & 0x7fff
    return -((-integer) & 0x7fff)

def as_uint16(integer):
    return integer & 0xffff

SYS_exit = 1
SYS_write = 5
STDOUT_FILENO = 1

def final():
    print(f"        $r3 <- $r3 ^ $r3")
    print(f"        $r4 <- {STDOUT_FILENO}")
    print(f"        $r5 <- .Lstr_pass")
    print(f"        $r6 <- (.Lstr_pass_end - 1)")
    print(f"        $r6 <- $r6 - $r5")
    print(f"        syscall")
    print(f"        .hword {SYS_write}")
    print(f"        $r4 <- 0")
    print(f"        syscall")
    print(f"        .hword {SYS_exit}")

def fail():
    print(f"        $r3 <- $r3 ^ $r3")
    print(f"        $r4 <- {STDOUT_FILENO}")
    print(f"        $r5 <- .Lstr_fail")
    print(f"        $r6 <- (.Lstr_fail_end - 1)")
    print(f"        $r6 <- $r6 - $r5")
    print(f"        syscall")
    print(f"        .hword {SYS_write}")
    print(f"        $r4 <- 0")
    print(f"        syscall")
    print(f"        .hword {SYS_exit}")

def start():
    print("# mach: brew")
    print(f".section .rodata")
    print(f"    .p2align 2")
    print(f"    .Lstr_pass:")
    print(f'        .string "pass\\n"')
    print(f"    .Lstr_pass_end:")
    print(f"    .Lstr_fail:")
    print(f'        .string "fail\\n"')
    print(f"    .Lstr_fail_end:")
    print(f"")
    print(f".text")
    print(f"    .global _start")
    print(f"    _start:")

def setup():
    for r in reg:
        print(f"        {r:gas} <- {r:val}")

def test():
    for r in reg:
        print(f"        {r:gas} <- {r:gas} + (-{r:val})")
        print(f"        if {r:gas} != 0 $pc <- 1f")
        r.set_val(0)
    print("        $pc <- 2f")
    print("    1:")
    fail()
    print("    2:")

def print_header():
    frame = inspect.currentframe().f_back
    args, _, _, values = inspect.getargvalues(frame)
    print(f"        ## {inspect.getframeinfo(frame)[2]}", end="")
    for i in args:
        print(f" {i}={values[i]}", end="")
    print()
    print("        ###################################################")

class RegOrImm(object):
    def __init__(self, val: int, idx: int, is_short: bool = False):
        self.idx = idx
        self.val = val
        self.is_short = is_short

    def __format__(self, fmt_spec: str) -> str:
        if self.is_reg():
            if fmt_spec == "" or fmt_spec == "gas" or fmt_spec == "gas_u":
                return f"$r{self.idx}"
            if fmt_spec == "gas_s":
                return f"$sr{self.idx}"
            if fmt_spec == "gas_f":
                return f"$fr{self.idx}"
            if fmt_spec == "val" or fmt_spec == "val_u":
                return f"({as_uint32(self.val)})"
            if fmt_spec == "val_s":
                return f"({as_sint32(self.val)})"
            if fmt_spec == "val_f":
                return f"{as_float(self.val)}"
            if fmt_spec == "size":
                return ""
            assert False, f"unrecognized format spec: {fmt_spec}"
        else:
            if self.is_short:
                if fmt_spec == "" or fmt_spec == "gas" or fmt_spec == "gas_u" or fmt_spec == "val" or fmt_spec == "val_u":
                    # short numbers are always signed...
                    return f"({as_sint16(self.val)})"
                if fmt_spec == "gas_s" or fmt_spec == "val_s":
                    return f"({as_sint16(self.val)})"
                if fmt_spec == "gas_f" or fmt_spec == "val_f":
                    assert False, f"float format is not supported for short immediates"
                if fmt_spec == "size":
                    return "short"
                assert False, f"unrecognized format spec: {fmt_spec}"
            else:
                if fmt_spec == "" or fmt_spec == "gas" or fmt_spec == "gas_u" or fmt_spec == "val" or fmt_spec == "val_u":
                    return f"({as_uint32(self.val)})"
                if fmt_spec == "gas_s" or fmt_spec == "val_s":
                    return f"({as_sint32(self.val)})"
                if fmt_spec == "gas_f" or fmt_spec == "val_f":
                    return f"{as_float(self.val)}"
                if fmt_spec == "size":
                    return ""
                assert False, f"unrecognized format spec: {fmt_spec}"

    def set_val(self, val):
        if self.is_short:
            self.val = val & 0xffff
        else:
            self.val = val & 0xffffffff


    def get_val(self):
        if self.is_short:
            return as_sint16(self.val)
        else:
            return self.val

    def is_reg(self) -> bool:
        return self.idx is not None

def imm(val):
    return RegOrImm(val, None)

def simm(val):
    return RegOrImm(val, None, True)

def fimm(val):
    return RegOrImm(float_to_int(val), None)

reg = list(RegOrImm(0, i) for i in range(0,15))
