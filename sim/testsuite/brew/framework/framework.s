
.extern errno

# MACRO: exit
  .macro exit nr
  $r4 <- nr
  SYSCALLM SYS_exit
  .endm

# MACRO: pass
# Write 'pass' to stdout and quit
  .macro pass
  $r3 <- 0
  $r4 <- STDOUT_FILENO
  $r5 <- .Lstr_pass
  $r6 <- .Lstr_pass_end
  $r6 <- $r6 - $r5
  SYSCALLM SYS_write
  $r4 <- 0
  SYSCALLM SYS_exit
  .endm

# MACRO: fail
# Write 'fail' to stdout and quit
  .macro fail
  $r3 <- 0
  $r4 <- STDOUT_FILENO
  $r5 <- .Lstr_fail
  $r6 <- .Lstr_fail_end
  $r6 <- $r6 - $r5
  SYSCALLM SYS_write
  $r4 <- 1
  SYSCALLM SYS_exit
  .endm

# MACRO: start
# All assembler tests should start with a call to "start"
  .macro start
  .text
.global _start
_start:
  .endm

  .section .rodata
  .p2align 2
.Lstr_pass:
  .string "pass\n"
.Lstr_pass_end:
.Lstr_fail:
  .string "fail\n"
.Lstr_fail_end:
