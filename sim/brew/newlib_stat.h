#ifndef NEWLIB_STAT_H__
#define NEWLIB_STAT_H__

// newlib's (brew-none-elf) struct stat and struct utimbuf, as byte offsets
// and sizes; see brew/abi/abi-layout.c, which prints the real layout.
// time_t is 64 bits but only 4-byte aligned, so a struct timespec is
// 12 bytes. The simulator writes the fields one by one in the target's
// byte order (sim_write_le) instead of relying on the host's layout.

#define NEWLIB_STAT_SIZE         72
#define NEWLIB_STAT_ST_DEV        0  // dev_t, 2 bytes
#define NEWLIB_STAT_ST_INO        2  // ino_t, 2 bytes
#define NEWLIB_STAT_ST_MODE       4  // mode_t, 4 bytes
#define NEWLIB_STAT_ST_NLINK      8  // nlink_t, 2 bytes
#define NEWLIB_STAT_ST_UID       10  // uid_t, 2 bytes
#define NEWLIB_STAT_ST_GID       12  // gid_t, 2 bytes
#define NEWLIB_STAT_ST_RDEV      14  // dev_t, 2 bytes
#define NEWLIB_STAT_ST_SIZE      16  // off_t, 4 bytes (signed)
#define NEWLIB_STAT_ST_ATIM      20  // struct timespec
#define NEWLIB_STAT_ST_MTIM      32  // struct timespec
#define NEWLIB_STAT_ST_CTIM      44  // struct timespec
#define NEWLIB_STAT_ST_BLKSIZE   56  // blksize_t, 4 bytes
#define NEWLIB_STAT_ST_BLOCKS    60  // blkcnt_t, 4 bytes
                                     // 64: long st_spare4[2]

#define NEWLIB_TIMESPEC_TV_SEC    0  // time_t, 8 bytes
#define NEWLIB_TIMESPEC_TV_NSEC   8  // long, 4 bytes

#define NEWLIB_UTIMBUF_ACTIME     0  // time_t, 8 bytes
#define NEWLIB_UTIMBUF_MODTIME    8  // time_t, 8 bytes

#endif // NEWLIB_STAT_H__
