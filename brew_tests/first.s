# ----------------------------------------------------------------------------------------
# Writes "Hello, World" to the console using only system calls. Runs on 64-bit Linux only.
# To assemble and run:
#
#     gcc -c hello.s && ld hello.o && ./a.out
#
# or
#
#     gcc -nostdlib hello.s && ./a.out
# ----------------------------------------------------------------------------------------

        .global _start

        .text
_start:
	#FENCE sdf
  #SII
  #MEM8[_start, $fr0] <- $r0
  #MEM8[$fr0] <- $r0
  #MEM8[$fr0, message+1] <- $r0
  $sr0 <- MEM8[$r1,message]
  $tpc <- MEM8[$r1]
  $r13 <- MEM16[$pc]
message:
        .ascii  "Hello, world\n"
