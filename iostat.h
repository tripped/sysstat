/*
 * iostat: report CPU and I/O statistics
 * (C) 1999,2000 by Sebastien Godard <sebastien.godard@wanadoo.fr>
 */

#ifndef _IOSTAT_H
#define _IOSTAT_H

#include "common.h"


#define MIN(a, b) 	(((a) < (b)) ? (a) : (b))

#define D_CPU_ONLY	1
#define D_DISK_ONLY	2
#define D_TIMESTAMP	4

#define DISPLAY_CPU_ONLY(m)	(((m) & D_CPU_ONLY) == D_CPU_ONLY)
#define DISPLAY_DISK_ONLY(m)	(((m) & D_DISK_ONLY) == D_DISK_ONLY)
#define DISPLAY_TIMESTAMP(m)	(((m) & D_TIMESTAMP) == D_TIMESTAMP)


/*
 * iostat file magic number:
 * will vary when file format changes
 */
#define IO_MAGIC	0x025b


/*
 * iostat file header
 */
struct file_hdr {
   /* System activity data file magic number */
   short int     io_magic			__attribute__ ((aligned (8)));
   /* Current day, month and year */
   unsigned char io_day				__attribute__ ((packed));
   unsigned char io_month			__attribute__ ((packed));
   unsigned char io_year			__attribute__ ((packed));
   /* Operating system name */
   char io_sysname[UTSNAME_LEN]			__attribute__ ((packed));
   /* Machine hostname */
   char io_nodename[UTSNAME_LEN]		__attribute__ ((packed));
   /* Operating system release number */
   char io_release[UTSNAME_LEN]			__attribute__ ((packed));
};

#define FILE_HDR_SIZE	(sizeof(int) + \
			 sizeof(char) * (3 + 3 * UTSNAME_LEN))

struct file_stats {
   /* Time stamp: hour, minute and second */
   unsigned char hour		/* (0-23) */	__attribute__ ((aligned (8)));
   unsigned char minute		/* (0-59) */	__attribute__ ((packed));
   unsigned char second		/* (0-59) */	__attribute__ ((packed));
   /* (Padding) */
   unsigned char pad[5]				__attribute__ ((packed));
   /* Stats... */
   unsigned long uptime				__attribute__ ((aligned (8)));
   unsigned int  cpu_user			__attribute__ ((aligned (8)));
   unsigned int  cpu_nice			__attribute__ ((packed));
   unsigned int  cpu_system			__attribute__ ((packed));
   unsigned long cpu_idle			__attribute__ ((aligned (8)));
   unsigned int  dk_drive_sum			__attribute__ ((aligned (8)));
   unsigned int  dk_drive[NR_DISKS]		__attribute__ ((packed));
   unsigned int  dk_drive_rblk[NR_DISKS]	__attribute__ ((packed));
   unsigned int  dk_drive_wblk[NR_DISKS]	__attribute__ ((packed));
};

#define FILE_STATS_SIZE	(sizeof(int) * (4 + 3 * NR_DISKS) + \
			 sizeof(char) * 8 + \
			 SIZEOF_LONG * 2)

#endif  /* _IOSTAT_H */
