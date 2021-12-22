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
  #$sr0 <- MEM8[$r1,message]
  #$tpc <- MEM8[$r1]
  #$r13 <- MEM16[$pc]
  #if $r3[12] == 1 $pc <- 0
  #if $r3[12] == 0 $pc <- message
  #if $r3 >= $r1 $pc <- 0
  #if $r3 <= $r1 $pc <- 0
  #if $sr3 <= 0 $pc <- message
  $r5 <- $r3 ^ $r1
  $r5 <- $r3
  $r5 <- 123
  $pc <- 111
  $tpc <- 444
  $r1 <- 3 + $r2
  $r1 <- $r2 + 4
  $r1 <- 3 - $r2
  $r1 <- $r2 - 4
message:
        .ascii  "Hello, world\n"
