/*
 * iostat: report CPU and I/O statistics
 * (C) 1999-2002 by Sebastien Godard <sebastien.godard@wanadoo.fr>
 */

#ifndef _IOSTAT_H
#define _IOSTAT_H

#include "common.h"

/* Maximum number of partitions (minimum is 4) */
#define MAX_PART	256

#define MAX_NAME_LEN 68

/* Files */
#define PARTITIONS	"/proc/partitions"

#define D_CPU_ONLY	1
#define D_DISK_ONLY	2
#define D_TIMESTAMP	4
#define D_EXTENDED	8
#define D_EXTENDED_ALL	16

#define DISPLAY_CPU_ONLY(m)	(((m) & D_CPU_ONLY) == D_CPU_ONLY)
#define DISPLAY_DISK_ONLY(m)	(((m) & D_DISK_ONLY) == D_DISK_ONLY)
#define DISPLAY_TIMESTAMP(m)	(((m) & D_TIMESTAMP) == D_TIMESTAMP)
#define DISPLAY_EXTENDED(m)	(((m) & D_EXTENDED) == D_EXTENDED)
#define DISPLAY_EXTENDED_ALL(m)	(((m) & D_EXTENDED_ALL) == D_EXTENDED_ALL)


struct comm_stats {
   unsigned long uptime				__attribute__ ((aligned (8)));
   unsigned long cpu_idle			__attribute__ ((aligned (8)));
   unsigned int  cpu_user			__attribute__ ((aligned (8)));
   unsigned int  cpu_nice			__attribute__ ((packed));
   unsigned int  cpu_system			__attribute__ ((packed));
   unsigned int  dk_drive_sum			__attribute__ ((packed));
};

#define COMM_STATS_SIZE	(sizeof(int)  * 4 + \
			 SIZEOF_LONG  * 2)


struct disk_hdr_stats {
   unsigned int  active				__attribute__ ((aligned (8)));
   unsigned int  major				__attribute__ ((packed));
   unsigned int  minor				__attribute__ ((packed));
            char name[MAX_NAME_LEN]		__attribute__ ((packed));
};

#define DISK_HDR_STATS_SIZE	(sizeof(int) * 3  \
				 sizeof(char) * MAX_NAME_LEN)


struct disk_stats {
   unsigned int  dk_drive			__attribute__ ((aligned (8)));
   unsigned int  dk_drive_rblk			__attribute__ ((packed));
   unsigned int  dk_drive_wblk			__attribute__ ((packed));
   /* Read I/O operations */
   unsigned int  rd_ios				__attribute__ ((packed));
   /* Operations merged */
   unsigned int  rd_merges			__attribute__ ((packed));
   /* Sectors read */
   unsigned int  rd_sectors			__attribute__ ((packed));
   /* Time of requests in queue */
   unsigned int  rd_ticks			__attribute__ ((packed));
   unsigned int  wr_ios				__attribute__ ((packed));
   unsigned int  wr_merges			__attribute__ ((packed));
   unsigned int  wr_sectors			__attribute__ ((packed));
   unsigned int  wr_ticks			__attribute__ ((packed));
   /* Average queue length */
   unsigned int  aveq				__attribute__ ((packed));
   /* Time of requests in queue */
   unsigned int  ticks				__attribute__ ((packed));
};

#define DISK_STATS_SIZE	(sizeof(int)  * 13)


#endif  /* _IOSTAT_H */
