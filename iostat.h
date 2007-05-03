/*
 * iostat: report CPU and I/O statistics
 * (C) 1999-2007 by Sebastien Godard (sysstat <at> wanadoo.fr)
 */

#ifndef _IOSTAT_H
#define _IOSTAT_H

#include "common.h"

/* I_: iostat - D_: Display - F_: Flag */
#define I_D_CPU			0x00001
#define I_D_DISK		0x00002
#define I_D_TIMESTAMP		0x00004
#define I_D_EXTENDED		0x00008
#define I_D_PART_ALL		0x00010
#define I_D_KILOBYTES		0x00020
#define I_F_HAS_SYSFS		0x00040
#define I_F_OLD_KERNEL		0x00080
#define I_D_UNFILTERED		0x00100
#define I_D_MEGABYTES		0x00200
#define I_D_PARTITIONS		0x00400
#define I_F_HAS_DISKSTATS	0x00800
#define I_F_HAS_PPARTITIONS	0x01000
#define I_F_PLAIN_KERNEL24	0x02000
#define I_D_NFS			0x04000
#define I_F_HAS_NFS		0x08000
#define I_D_DEVMAP_NAME		0x10000
#define I_D_ISO			0x20000

#define DISPLAY_CPU(m)		(((m) & I_D_CPU) == I_D_CPU)
#define DISPLAY_DISK(m)		(((m) & I_D_DISK) == I_D_DISK)
#define DISPLAY_TIMESTAMP(m)	(((m) & I_D_TIMESTAMP) == I_D_TIMESTAMP)
#define DISPLAY_EXTENDED(m)	(((m) & I_D_EXTENDED) == I_D_EXTENDED)
#define DISPLAY_PART_ALL(m)	(((m) & I_D_PART_ALL) == I_D_PART_ALL)
#define DISPLAY_KILOBYTES(m)	(((m) & I_D_KILOBYTES) == I_D_KILOBYTES)
#define DISPLAY_MEGABYTES(m)	(((m) & I_D_MEGABYTES) == I_D_MEGABYTES)
#define HAS_SYSFS(m)		(((m) & I_F_HAS_SYSFS) == I_F_HAS_SYSFS)
#define HAS_OLD_KERNEL(m)	(((m) & I_F_OLD_KERNEL) == I_F_OLD_KERNEL)
#define DISPLAY_UNFILTERED(m)	(((m) & I_D_UNFILTERED) == I_D_UNFILTERED)
#define DISPLAY_PARTITIONS(m)	(((m) & I_D_PARTITIONS) == I_D_PARTITIONS)
#define HAS_DISKSTATS(m)	(((m) & I_F_HAS_DISKSTATS) == I_F_HAS_DISKSTATS)
#define HAS_PPARTITIONS(m)	(((m) & I_F_HAS_PPARTITIONS) == I_F_HAS_PPARTITIONS)
#define HAS_PLAIN_KERNEL24(m)	(((m) & I_F_PLAIN_KERNEL24) == I_F_PLAIN_KERNEL24)
#define DISPLAY_NFS(m)		(((m) & I_D_NFS) == I_D_NFS)
#define HAS_NFS(m)		(((m) & I_F_HAS_NFS) == I_F_HAS_NFS)
#define DISPLAY_DEVMAP_NAME(m)	(((m) & I_D_DEVMAP_NAME) == I_D_DEVMAP_NAME)
#define DISPLAY_ISO(m)		(((m) & I_D_ISO) == I_D_ISO)

#define DT_DEVICE	0
#define DT_PARTITION	1

/* Device name for old kernels */
#define K_HDISK	"hdisk"

struct comm_stats {
   unsigned long long uptime;
   unsigned long long uptime0;
   unsigned long long cpu_iowait;
   unsigned long long cpu_idle;
   unsigned long long cpu_user;
   unsigned long long cpu_nice;
   unsigned long long cpu_system;
   unsigned long long cpu_steal;
};

#define COMM_STATS_SIZE	(sizeof(struct comm_stats))

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
   /* # of sectors read */
   unsigned long long rd_sectors		__attribute__ ((aligned (8)));
   /* # of sectors written */
   unsigned long long wr_sectors		__attribute__ ((packed));
   /* # of read operations issued to the device */
   unsigned long rd_ios				__attribute__ ((packed));
   /* # of read requests merged */
   unsigned long rd_merges			__attribute__ ((packed));
   /* Time of read requests in queue */
   unsigned long rd_ticks			__attribute__ ((packed));
   /* # of write operations issued to the device */
   unsigned long wr_ios				__attribute__ ((packed));
   /* # of write requests merged */
   unsigned long wr_merges			__attribute__ ((packed));
   /* Time of write requests in queue */
   unsigned long wr_ticks			__attribute__ ((packed));
   /* # of I/Os in progress */
   unsigned long ios_pgr			__attribute__ ((packed));
   /* # of ticks total (for this device) for I/O */
   unsigned long tot_ticks			__attribute__ ((packed));
   /* # of ticks requests spent in queue */
   unsigned long rq_ticks			__attribute__ ((packed));
   /* # of I/O done since last reboot */
   unsigned long dk_drive			__attribute__ ((packed));
   /* # of blocks read */
   unsigned long dk_drive_rblk			__attribute__ ((packed));
   /* # of blocks written */
   unsigned long dk_drive_wblk			__attribute__ ((packed));
};

#define IO_STATS_SIZE	(sizeof(struct io_stats))

struct io_nfs_stats {
   unsigned long long rd_normal_bytes		__attribute__ ((aligned (8)));
   unsigned long long wr_normal_bytes		__attribute__ ((packed));
   unsigned long long rd_direct_bytes		__attribute__ ((packed));
   unsigned long long wr_direct_bytes		__attribute__ ((packed));
   unsigned long long rd_server_bytes		__attribute__ ((packed));
   unsigned long long wr_server_bytes		__attribute__ ((packed));
};

#define IO_NFS_STATS_SIZE	(sizeof(struct io_nfs_stats))

struct io_hdr_stats {
   unsigned int active				__attribute__ ((aligned (4)));
   unsigned int used				__attribute__ ((packed));
   char name[MAX_NAME_LEN];
};

#define IO_HDR_STATS_SIZE	(sizeof(struct io_hdr_stats))

/* List of devices entered on the command line */
struct io_dlist {
   /* Indicate whether its partitions are to be displayed or not */
   int disp_part				__attribute__ ((aligned (4)));
   /* Device name */
   char dev_name[MAX_NAME_LEN];
};

#define IO_DLIST_SIZE	(sizeof(struct io_dlist))

#endif  /* _IOSTAT_H */
