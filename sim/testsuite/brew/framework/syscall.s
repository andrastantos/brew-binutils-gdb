/*
 * syscall.s -- syscall interface for brew
 *
 * Copyright (c) 2022 Andras Tantos
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 */


/* Every syscall follows the same, generic layout:
 * Input:
 * $r3        -- pointer to errno
 * $r4        -- first argument (if any)
 * $r5        -- second argument (if any)
 * $r6        -- third argument (if any)
 * $r7        -- fourth argument (if any)
 *            -- all other arguments (if any) are on the stack.
 *
 * Output:
 * $r4        -- first return value (if any)
 * $r5        -- second return value (if any)
 * $r6        -- third return value (if any)
 * $r7        -- fourth return value (if any)
 * errno      -- updated as(if) needed
 *
 * In other words, this is normal function calling conventions,
 * with the addition of $r3 pointing to errno on entry.
 */

  .set SYS_exit,      1
  .set SYS_open,      2
  .set SYS_close,     3
  .set SYS_read,      4
  .set SYS_write,     5
  .set SYS_lseek,     6
  .set SYS_unlink,    7
  .set SYS_getpid,    8
  .set SYS_kill,      9
  .set SYS_fstat,     10

  .global  errno

  .macro   CREATE_SYSCALL FN_NAME SYSCALL_NO
  .global  FN_NAME
  .type    FN_NAME,@function
  .text
FN_NAME:
  $r3 <- errno
  syscall
  .hword  SYSCALL_NO
  RET
.Lend:
  .size    FN_NAME,.Lend-FN_NAME
  .endm

  #CREATE_SYSCALL _exit SYS_exit
  #CREATE_SYSCALL _open SYS_open
  #CREATE_SYSCALL _close SYS_close
  #CREATE_SYSCALL _read SYS_read
  #CREATE_SYSCALL _write SYS_write
  #CREATE_SYSCALL _lseek SYS_lseek
  #CREATE_SYSCALL _unlink SYS_unlink
  #CREATE_SYSCALL _getpid SYS_getpid
  #CREATE_SYSCALL _kill SYS_kill
  #CREATE_SYSCALL _fstat SYS_fstat

  .data
errno:
  .long 0
