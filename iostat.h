/*
 * iostat: report CPU and I/O statistics
 * (C) 1999-2003 by Sebastien Godard <sebastien.godard@wanadoo.fr>
 */

#ifndef _IOSTAT_H
#define _IOSTAT_H

#include "common.h"

#define MAX_NAME_LEN	72

#define D_CPU_ONLY	0x01
#define D_DISK_ONLY	0x02
#define D_TIMESTAMP	0x04
#define D_EXTENDED	0x08
#define D_PART_ALL	0x10
#define D_KILOBYTES	0x20
#define F_HAS_SYSFS	0x40
#define F_OLD_KERNEL	0x80
/* 0x100000:0x800000 -> reserved (cf. common.h) */

#define DISPLAY_CPU_ONLY(m)	(((m) & D_CPU_ONLY) == D_CPU_ONLY)
#define DISPLAY_DISK_ONLY(m)	(((m) & D_DISK_ONLY) == D_DISK_ONLY)
#define DISPLAY_TIMESTAMP(m)	(((m) & D_TIMESTAMP) == D_TIMESTAMP)
#define DISPLAY_EXTENDED(m)	(((m) & D_EXTENDED) == D_EXTENDED)
#define DISPLAY_PART_ALL(m)	(((m) & D_PART_ALL) == D_PART_ALL)
#define DISPLAY_KILOBYTES(m)	(((m) & D_KILOBYTES) == D_KILOBYTES)
#define HAS_SYSFS(m)		(((m) & F_HAS_SYSFS) == F_HAS_SYSFS)
#define HAS_OLD_KERNEL(m)	(((m) & F_OLD_KERNEL) == F_OLD_KERNEL)

#define DT_DEVICE	0
#define DT_PARTITION	1


struct comm_stats {
   unsigned long uptime;
   unsigned long cpu_iowait;
   unsigned long cpu_idle;
   unsigned int  cpu_user;
   unsigned int  cpu_nice;
   unsigned int  cpu_system;
};


/*
 * Structures for I/O stats.
 * The number of structures allocated corresponds to the number of devices
 * present in the system, plus a preallocation number to handle those
 * that can be registered dynamically.
 * The number of devices is found by using /sys filesystem (if mounted),
 * or the number of "disk_io:" entries in /proc/stat (2.4 kernels),
 * else the default value is 4 (for old kernels, which maintained stats
 * for the first four devices in /proc/stat).
 * For each io_stats structure allocated corresponds a io_hdr_stats structure.
 * A io_stats structure is considered as unused or "free" (containing no stats
 * for a particular device) if the 'major' field of the io_hdr_stats
 * structure is set to 0.
 */
struct io_stats {
   /* # of read operations issued to the device */
   unsigned int  rd_ios				__attribute__ ((aligned (8)));
   /* # of read requests merged */
   unsigned int  rd_merges			__attribute__ ((packed));
   /* # of sectors read */
   unsigned int  rd_sectors			__attribute__ ((packed));
   /* Time of read requests in queue */
   unsigned int  rd_ticks			__attribute__ ((packed));
   /* # of write operations issued to the device */
   unsigned int  wr_ios				__attribute__ ((packed));
   /* # of write requests merged */
   unsigned int  wr_merges			__attribute__ ((packed));
   /* # of sectors written */
   unsigned int  wr_sectors			__attribute__ ((packed));
   /* Time of write requests in queue */
   unsigned int  wr_ticks			__attribute__ ((packed));
   /* # of I/Os in progress */
   unsigned int  ios_pgr			__attribute__ ((packed));
   /* # of ticks total (for this device) for I/O */
   unsigned int  tot_ticks			__attribute__ ((packed));
   /* # of ticks requests spent in queue */
   unsigned int  rq_ticks			__attribute__ ((packed));
   /* # of I/O done since last reboot */
   unsigned int  dk_drive			__attribute__ ((packed));
   /* # of blocks read */
   unsigned int  dk_drive_rblk			__attribute__ ((packed));
   /* # of blocks written */
   unsigned int  dk_drive_wblk			__attribute__ ((packed));
};


struct io_hdr_stats {
   unsigned int  active				__attribute__ ((aligned (8)));
   unsigned int  major				__attribute__ ((packed));
   unsigned int  index				__attribute__ ((packed));
            char name[MAX_NAME_LEN]		__attribute__ ((packed));
};


/* List of devices entered on the command line */
struct io_dlist {
   /* Indicate whether its partitions are to be displayed or not */
   int  disp_part				__attribute__ ((aligned (8)));
   /* Device name */
   char dev_name[MAX_NAME_LEN]			__attribute__ ((packed));
};


#endif  /* _IOSTAT_H */
