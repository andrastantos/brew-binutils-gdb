# check load/store operators
# mach: brew

.include "framework/macros.s"
.include "framework/syscall.s"
.include "framework/fileno.s"
.include "framework/framework.s"

  # Create a set of initial values for all registers (these are all primes)
  .set NUM_1,  1
  .set NUM_2,  0x12345678
  .set NUM_3,  0
  .set NUM_4,  -7
  .set NUM_5,  9
  .set NUM_6,  11
  .set NUM_7,  13
  .set NUM_8,  15
  .set NUM_9,  17
  .set NUM_10, 131
  .set NUM_11, 37000
  .set NUM_12, 31
  .set NUM_13, 32
  .set NUM_14, 33

  # Create a set of initial values for all registers
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .set IN_R$r\R, NUM_\R
  .set IN_R$sr\R, NUM_\R
  .endr

  .macro setup_regs
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  .endm


####################################################
  start
  # Test loads from fixed address
  .irp D,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  setup_regs
  $r\D <- mem[data]
  TEST $r\D, 0x01234567
  setup_regs
  $r\D <- mem8[data]
  TEST $r\D, 0x67
  setup_regs
  $r\D <- mem8[data+1]
  TEST $r\D, 0x45
  setup_regs
  $r\D <- mem8[data+2]
  TEST $r\D, 0x23
  setup_regs
  $r\D <- mem8[data+3]
  TEST $r\D, 0x01
  setup_regs
  $r\D <- mem16[data]
  TEST $r\D, 0x4567
  setup_regs
  $r\D <- mem16[data+2]
  TEST $r\D, 0x0123
  setup_regs
  $sr\D <- mem8[data]
  TEST $r\D, 0x67
  setup_regs
  $sr\D <- mem8[data+1]
  TEST $r\D, 0x45
  setup_regs
  $sr\D <- mem8[data+2]
  TEST $r\D, 0x23
  setup_regs
  $sr\D <- mem8[data+3]
  TEST $r\D, 0x01
  setup_regs
  $sr\D <- mem16[data]
  TEST $r\D, 0x4567
  setup_regs
  $sr\D <- mem16[data+2]
  TEST $r\D, 0x0123

  setup_regs
  $r\D <- mem[data+4]
  TEST $r\D, 0x89abcdef
  setup_regs
  $r\D <- mem8[data+4]
  TEST $r\D, 0xef
  setup_regs
  $r\D <- mem8[data+5]
  TEST $r\D, 0xcd
  setup_regs
  $r\D <- mem8[data+6]
  TEST $r\D, 0xab
  setup_regs
  $r\D <- mem8[data+7]
  TEST $r\D, 0x89
  setup_regs
  $r\D <- mem16[data+4]
  TEST $r\D, 0xcdef
  setup_regs
  $r\D <- mem16[data+6]
  TEST $r\D, 0x89ab
  setup_regs
  $sr\D <- mem8[data+4]
  TEST $r\D, 0xffffffef
  setup_regs
  $sr\D <- mem8[data+5]
  TEST $r\D, 0xffffffcd
  setup_regs
  $sr\D <- mem8[data+6]
  TEST $r\D, 0xffffffab
  setup_regs
  $sr\D <- mem8[data+7]
  TEST $r\D, 0xffffff89
  setup_regs
  $sr\D <- mem16[data+4]
  TEST $r\D, 0xffffcdef
  setup_regs
  $sr\D <- mem16[data+6]
  TEST $r\D, 0xffff89ab
  .endr

  # Test loads from register and register-relative address
  .irp D,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp S,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  setup_regs
  $r\S <- data
  $r\D <- mem[$r\S]
  TEST $r\D, 0x01234567
  setup_regs
  $r\S <- data
  $r\D <- mem8[$r\S]
  TEST $r\D, 0x67
  setup_regs
  $r\S <- data
  $r\D <- mem8[$r\S,1]
  TEST $r\D, 0x45
  setup_regs
  # throw in some negative offsets
  $r\S <- data
  $r\S <- $r\S+4
  $r\D <- mem8[$r\S,(-2)]
  TEST $r\D, 0x23
  setup_regs
  $r\S <- data
  $r\D <- mem8[$r\S,3]
  TEST $r\D, 0x01
  setup_regs
  $r\S <- data
  $r\D <- mem16[$r\S]
  TEST $r\D, 0x4567
  setup_regs
  $r\S <- data
  $r\D <- mem16[$r\S,2]
  TEST $r\D, 0x0123
  setup_regs
  $r\S <- data
  $sr\D <- mem8[$r\S]
  TEST $r\D, 0x67
  setup_regs
  $r\S <- data
  $sr\D <- mem8[$r\S,1]
  TEST $r\D, 0x45
  setup_regs
  $r\S <- data
  $sr\D <- mem8[$r\S,2]
  TEST $r\D, 0x23
  setup_regs
  $r\S <- data
  $sr\D <- mem8[$r\S,3]
  TEST $r\D, 0x01
  setup_regs
  $r\S <- data
  $sr\D <- mem16[$r\S]
  TEST $r\D, 0x4567
  setup_regs
  $r\S <- data
  $sr\D <- mem16[$r\S,2]
  TEST $r\D, 0x0123

  setup_regs
  $r\S <- data
  $r\D <- mem[$r\S,4]
  TEST $r\D, 0x89abcdef
  setup_regs
  $r\S <- data
  $r\D <- mem8[$r\S,4]
  TEST $r\D, 0xef
  setup_regs
  $r\S <- data
  $r\D <- mem8[$r\S,5]
  TEST $r\D, 0xcd
  setup_regs
  $r\S <- data
  $r\D <- mem8[$r\S,6]
  TEST $r\D, 0xab
  setup_regs
  $r\S <- data
  $r\D <- mem8[$r\S,7]
  TEST $r\D, 0x89
  setup_regs
  $r\S <- data
  $r\D <- mem16[$r\S,4]
  TEST $r\D, 0xcdef
  setup_regs
  $r\S <- data
  $r\D <- mem16[$r\S,6]
  TEST $r\D, 0x89ab
  setup_regs
  $r\S <- data
  $sr\D <- mem8[$r\S,4]
  TEST $r\D, 0xffffffef
  setup_regs
  $r\S <- data
  $sr\D <- mem8[$r\S,5]
  TEST $r\D, 0xffffffcd
  setup_regs
  $r\S <- data
  $sr\D <- mem8[$r\S,6]
  TEST $r\D, 0xffffffab
  setup_regs
  $r\S <- data
  $sr\D <- mem8[$r\S,7]
  TEST $r\D, 0xffffff89
  setup_regs
  $r\S <- data
  $sr\D <- mem16[$r\S,4]
  TEST $r\D, 0xffffcdef
  setup_regs
  $r\S <- data
  $sr\D <- mem16[$r\S,6]
  TEST $r\D, 0xffff89ab
  .endr
  .endr

  # Test stores to fixed address
  .irp D,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r1 <- 0xdeadbeef
  mem[data] <- $r1
  setup_regs
  mem[data] <- $r\D
  $r1 <- 0xdeadbeef
  $r1 <- mem[data]
  TEST $r1, IN_R$r\D

  .irp OFS,0,1,2,3
  $r1 <- 0xdeadbeef
  mem[data] <- $r1
  setup_regs
  mem8[data+\OFS] <- $r\D
  $r1 <- 0xdeadbeef
  $r1 <- mem[data]
  TEST $r1, ((0xdeadbeef & ~(0xff << (8*\OFS))) | ((IN_R$r\D & 0xff) << (8*\OFS)))
  .endr

  .irp OFS,0,2
  $r1 <- 0xdeadbeef
  mem[data] <- $r1
  setup_regs
  mem16[data+\OFS] <- $r\D
  $r1 <- 0xdeadbeef
  $r1 <- mem[data]
  TEST $r1, ((0xdeadbeef & ~(0xffff << (8*\OFS))) | ((IN_R$r\D & 0xffff) << (8*\OFS)))
  .endr

  .endr

  # Test stores to register-relative and register address
  .irp D,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp S,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .if \D != \S
  $r1 <- 0xdeadbeef
  mem[data] <- $r1
  setup_regs
  $r\S <- data
  mem[$r\S] <- $r\D
  $r1 <- 0xdeadbeef
  $r1 <- mem[data]
  TEST $r1, IN_R$r\D

  .irp OFS,0,1,2,3
  $r1 <- 0xdeadbeef
  mem[data] <- $r1
  setup_regs
  $r\S <- data
  mem8[$r\S,\OFS] <- $r\D
  $r1 <- 0xdeadbeef
  $r1 <- mem[data]
  TEST $r1, ((0xdeadbeef & ~(0xff << (8*\OFS))) | ((IN_R$r\D & 0xff) << (8*\OFS)))
  .endr

  .irp OFS,0,2
  $r1 <- 0xdeadbeef
  mem[data] <- $r1
  setup_regs
  $r\S <- data
  mem16[$r\S,\OFS] <- $r\D
  $r1 <- 0xdeadbeef
  $r1 <- mem[data]
  TEST $r1, ((0xdeadbeef & ~(0xffff << (8*\OFS))) | ((IN_R$r\D & 0xffff) << (8*\OFS)))
  .endr
  
  .endif

  .endr
  .endr

  pass

  .data
data:
  .long 0x01234567
  .long 0x89abcdef
