#ifndef NEWLIB_STAT_H__
#define NEWLIB_STAT_H__

struct newlib_timespec
  {
    uint32_t tv_sec;  /* seconds */
    uint32_t tv_nsec; /* and nanoseconds */
  };

struct newlib_stat
  {
    int16_t st_dev;     /* Device.  */
    uint16_t __pad1;
    uint32_t __st_ino;  /* 32bit file serial number.	*/
    uint32_t st_mode;			/* File mode.  */
    uint16_t st_nlink;			/* Link count.  */
    uint16_t st_uid;		/* User ID of the file's owner.	*/
    uint16_t st_gid;		/* Group ID of the file's group.*/
    uint16_t st_rdev;		/* Device number, if device.  */
    //uint16_t __pad2;
    uint64_t st_size;			/* Size of file, in bytes.  */
    uint32_t st_blksize;	/* Optimal block size for I/O.  */
    uint64_t st_blocks;		/* Number 512-byte blocks allocated. */
    struct newlib_timespec st_atim;		/* Time of last access.  */
    struct newlib_timespec st_mtim;		/* Time of last modification.  */
    struct newlib_timespec st_ctim;		/* Time of last status change.  */
  };

#endif // NEWLIB_STAT_H__
