/*
 * sar/sadc: report system activity
 * (C) 1999-2001 by Sebastien Godard <sebastien.godard@wanadoo.fr>
 */

#ifndef _SA_H
#define _SA_H

#include <asm/page.h>	/* for PAGE_SHIFT */
#include "common.h"

/* Define activities */
#define A_PROC		0x000001
#define A_CTXSW		0x000002
#define A_CPU		0x000004
#define A_IRQ		0x000008
#define A_PAGE		0x000010
#define A_SWAP		0x000020
#define A_IO		0x000040
#define A_ONE_CPU	0x000080
#define A_ONE_IRQ	0x000100
#define A_MEMORY	0x000200
#define A_PID		0x000400
#define A_CPID		0x000800
#define A_SUM_PID	0x001000
#define A_SERIAL	0x002000
#define A_MEM_AMT	0x004000
#define A_IRQ_CPU	0x008000
#define A_KTABLES	0x010000
#define A_NET_DEV	0x020000
#define A_NET_EDEV	0x040000
#define A_NET_SOCK	0x080000
#define A_QUEUE		0x100000
#define A_DISK		0x200000

#define A_LAST		0x200000

#define GET_PROC(m)	(((m) & A_PROC) == A_PROC)
#define GET_CTXSW(m)	(((m) & A_CTXSW) == A_CTXSW)
#define GET_CPU(m)	(((m) & A_CPU) == A_CPU)
#define GET_IRQ(m)	(((m) & A_IRQ) == A_IRQ)
#define GET_PAGE(m)	(((m) & A_PAGE) == A_PAGE)
#define GET_SWAP(m)	(((m) & A_SWAP) == A_SWAP)
#define GET_IO(m)	(((m) & A_IO) == A_IO)
#define GET_ONE_CPU(m)	(((m) & A_ONE_CPU) == A_ONE_CPU)
#define GET_ONE_IRQ(m)	(((m) & A_ONE_IRQ) == A_ONE_IRQ)
#define GET_MEMORY(m)	(((m) & A_MEMORY) == A_MEMORY)
#define GET_PID(m)	(((m) & A_PID) == A_PID)
#define GET_CPID(m)	(((m) & A_CPID) == A_CPID)
#define GET_SUM_PID(m)	(((m) & A_SUM_PID) == A_SUM_PID)
#define GET_CPID(m)	(((m) & A_CPID) == A_CPID)
#define GET_ALL_PID(m)	(((m) & A_ALL_PID) == A_ALL_PID)
#define GET_SERIAL(m)	(((m) & A_SERIAL) == A_SERIAL)
#define GET_MEM_AMT(m)	(((m) & A_MEM_AMT) == A_MEM_AMT)
#define GET_IRQ_CPU(m)	(((m) & A_IRQ_CPU) == A_IRQ_CPU)
#define GET_KTABLES(m)	(((m) & A_KTABLES) == A_KTABLES)
#define GET_NET_DEV(m)	(((m) & A_NET_DEV) == A_NET_DEV)
#define GET_NET_EDEV(m)	(((m) & A_NET_EDEV) == A_NET_EDEV)
#define GET_NET_SOCK(m)	(((m) & A_NET_SOCK) == A_NET_SOCK)
#define GET_QUEUE(m)	(((m) & A_QUEUE) == A_QUEUE)
#define GET_DISK(m)	(((m) & A_DISK) == A_DISK)


/*
 * KB -> number of pages.
 * A page is 4 Kb or 8 Kb according to the machine architecture.
 */
#define PG(k)	((k) >> (PAGE_SHIFT - 10))

/* Keywords */
#define K_XALL	"XALL"
#define K_SUM	"SUM"
#define K_SELF	"SELF"
#define K_SADC	"SADC"
#define K_PROC	"PROC"
#define K_DEV	"DEV"
#define K_EDEV	"EDEV"
#define K_SOCK	"SOCK"
#define K_FULL	"FULL"

/* Define flags */
#define F_ALL_PROC      0x001
#define F_SA_ROTAT      0x002
#define F_FLT_INC	0x004
#define F_A_OPTION	0x008
/* 0x10 flag used */
#define F_H_OPTION	0x020
#define F_ORG_TIME	0x040
#define F_DEFAULT_COUNT	0x080
#define F_I_OPTION	0x100

#define WANT_ALL_PROC(m)	(((m) & F_ALL_PROC) == F_ALL_PROC)
#define WANT_SA_ROTAT(m)	(((m) & F_SA_ROTAT) == F_SA_ROTAT)
#define FLT_ARE_INC(m)		(((m) & F_FLT_INC) == F_FLT_INC)
#define USE_A_OPTION(m)		(((m) & F_A_OPTION) == F_A_OPTION)
#define USE_H_OPTION(m)		(((m) & F_H_OPTION) == F_H_OPTION)
#define PRINT_ORG_TIME(m)	(((m) & F_ORG_TIME) == F_ORG_TIME)
#define USE_DEFAULT_COUNT(m)	(((m) & F_DEFAULT_COUNT) == F_DEFAULT_COUNT)
#define USE_I_OPTION(m)		(((m) & F_I_OPTION) == F_I_OPTION)

/* Files */
#define PROC		"/proc"
#define PSTAT		"stat"
#define MEMINFO		"/proc/meminfo"
#define PID_STAT	"/proc/%ld/stat"
#define SERIAL		"/proc/tty/driver/serial"
#define FDENTRY_STATE	"/proc/sys/fs/dentry-state"
#define FFILE_NR	"/proc/sys/fs/file-nr"
#define FINODE_MAX	"/proc/sys/fs/inode-max"
#define FINODE_STATE	"/proc/sys/fs/inode-state"
#define FDQUOT_NR	"/proc/sys/fs/dquot-nr"
#define FDQUOT_MAX	"/proc/sys/fs/dquot-max"
#define FSUPER_NR	"/proc/sys/fs/super-nr"
#define FSUPER_MAX	"/proc/sys/fs/super-max"
#define FRTSIG_NR	"/proc/sys/kernel/rtsig-nr"
#define FRTSIG_MAX	"/proc/sys/kernel/rtsig-max"
#define NET_DEV		"/proc/net/dev"
#define NET_SOCKSTAT	"/proc/net/sockstat"
#define SADC		"sadc"
#define SADC_PATH	"/usr/lib/sysstat/sadc"
#define SADC_LOCAL_PATH	"/usr/local/lib/sysstat/sadc"
#define LOADAVG		"/proc/loadavg"

#define NR_CPUS		64
#define NR_IRQS		224
#define MAX_IRQS_DIV_8	32	/* Assume NR_IRQS <= 256 */

#define NR_IFACE_PREALLOC	2
#define NR_SERIAL_PREALLOC	2
#define NR_IRQPROC_PREALLOC	3
#define NR_DISK_PREALLOC	3

/* Maximum number of processes that can be monitored simultaneously */
#define MAX_PID_NR		256
/*
 * Maximum number of args that can be passed to sadc:
 * sadc -x <pid> [-x <pid> ...] -X <pid> [-X <pid> ...] -I <interval> <count> <outfile> NULL
 */
#define MAX_ARGV_NR	(64 * 2) + 6

#define USE_SADC	0
#define USE_SA_FILE	1
#define ST_IMMEDIATE	0
#define ST_SINCE_BOOT	1
#define NO_TM_START	0
#define NO_TM_END	0
#define NO_RESET	0

/* Record type */
#define R_STATS		1
#define R_DUMMY		2

#define SOFT_SIZE	0
#define HARD_SIZE	1

/*
 * System activity daily file magic number
 * (will vary when file format changes)
 */
#define SA_MAGIC	0x215d

/*
 * Attributes such as 'aligned' and 'packed' have been defined for every
 * members of the following structures, so that:
 * 1) structures have a fixed size whether on 32 or 64-bit systems,
 * 2) we don't have variable gap between members.
 */
/* System activity data file header */
struct file_hdr {
   /* Activity flag */
   unsigned int	 sa_actflag			__attribute__ ((aligned (8)));
   /* System activity data file magic number */
   short    int  sa_magic			__attribute__ ((packed));
   /* file_stats structure size */
   short    int  sa_st_size			__attribute__ ((packed));
   /* Number of processes to monitor ( {-x | -X } ALL) */
   unsigned int  sa_nr_pid			__attribute__ ((packed));
   /* Number of interrupts per procesor: 2 means two interrupts */
   unsigned int  sa_irqcpu			__attribute__ ((packed));
   /* Time stamp in seconds since the epoch */
   unsigned long sa_ust_time			__attribute__ ((aligned (8)));
   /* Number of disks */
   unsigned int  sa_nr_disk			__attribute__ ((aligned (8)));
   /*
    * Current day, month and year.
    * No need to save DST (daylight saving time) flag, since it is not taken
    * into account by the strftime() function used to print the timestamp.
    */
   unsigned char sa_day				__attribute__ ((packed));
   unsigned char sa_month			__attribute__ ((packed));
   unsigned char sa_year			__attribute__ ((packed));
   /* Number of processors: 1 means two proc */
   unsigned char sa_proc 			__attribute__ ((packed));
   /* Number of serial lines: 2 means two lines (ttyS00 and ttyS01) */
   unsigned char sa_serial 			__attribute__ ((packed));
   /* Number of network devices (interfaces): 2 means two lines */
   unsigned char sa_iface 			__attribute__ ((packed));
   /* Operating system name */
   char          sa_sysname[UTSNAME_LEN]	__attribute__ ((packed));
   /* Machine hostname */
   char          sa_nodename[UTSNAME_LEN]	__attribute__ ((packed));
   /* Operating system release number */
   char          sa_release[UTSNAME_LEN]	__attribute__ ((packed));
};

/* Note that sizeof(file_hdr) may be greater than FILE_HDR_SIZE */
#define FILE_HDR_SIZE	(SIZEOF_LONG + \
			 sizeof(int)   * 4 + \
                         sizeof(short) * 2 + \
                         sizeof(char)  * (6 + 3 * UTSNAME_LEN))

struct file_stats {
   /* Record type: R_STATS or R_DUMMY */
   unsigned char record_type			__attribute__ ((aligned (8)));
   /* Time stamp: hour, minute and second */
   unsigned char hour		/* (0-23) */	__attribute__ ((packed));
   unsigned char minute		/* (0-59) */	__attribute__ ((packed));
   unsigned char second		/* (0-59) */	__attribute__ ((packed));
   /* Nb of processes (set only when using '-x SUM') */
   unsigned int  nr_processes			__attribute__ ((packed));
   /* Time stamp (number of seconds since the epoch) */
   unsigned long ust_time			__attribute__ ((aligned (8)));
   /* Stats... */
   unsigned long uptime				__attribute__ ((aligned (8)));
   unsigned long processes			__attribute__ ((aligned (8)));
   unsigned int  context_swtch			__attribute__ ((aligned (8)));
   unsigned int  cpu_user			__attribute__ ((packed));
   unsigned int  cpu_nice			__attribute__ ((packed));
   unsigned int  cpu_system			__attribute__ ((packed));
   unsigned long cpu_idle			__attribute__ ((aligned (8)));
   unsigned int  irq_sum			__attribute__ ((aligned (8)));
   unsigned int  pgpgin				__attribute__ ((packed));
   unsigned int  pgpgout			__attribute__ ((packed));
   unsigned int  pswpin				__attribute__ ((packed));
   unsigned int  pswpout			__attribute__ ((packed));
   unsigned int  dk_drive			__attribute__ ((packed));
   unsigned int  dk_drive_rio			__attribute__ ((packed));
   unsigned int  dk_drive_wio			__attribute__ ((packed));
   unsigned int  dk_drive_rblk			__attribute__ ((packed));
   unsigned int  dk_drive_wblk			__attribute__ ((packed));
   /* Memory stats in Kb */
   unsigned long frmkb				__attribute__ ((aligned (8)));
   unsigned long shmkb				__attribute__ ((aligned (8)));
   unsigned long bufkb				__attribute__ ((aligned (8)));
   unsigned long camkb				__attribute__ ((aligned (8)));
   unsigned long tlmkb				__attribute__ ((aligned (8)));
   unsigned long frskb				__attribute__ ((aligned (8)));
   unsigned long tlskb				__attribute__ ((aligned (8)));
   /* minflt and majflt set only when using '-x SUM' */
   unsigned long minflt				__attribute__ ((aligned (8)));
   unsigned long majflt				__attribute__ ((aligned (8)));
   unsigned int  dentry_stat			__attribute__ ((aligned (8)));
   unsigned int  file_used			__attribute__ ((packed));
   unsigned int  file_max			__attribute__ ((packed));
   unsigned int  inode_used			__attribute__ ((packed));
   unsigned int  super_used			__attribute__ ((packed));
   unsigned int  super_max			__attribute__ ((packed));
   unsigned int  dquot_used			__attribute__ ((packed));
   unsigned int  dquot_max			__attribute__ ((packed));
   unsigned int  rtsig_queued			__attribute__ ((packed));
   unsigned int  rtsig_max			__attribute__ ((packed));
   unsigned int  sock_inuse			__attribute__ ((packed));
   unsigned int  tcp_inuse			__attribute__ ((packed));
   unsigned int  udp_inuse			__attribute__ ((packed));
   unsigned int  raw_inuse			__attribute__ ((packed));
   unsigned int  frag_inuse			__attribute__ ((packed));
   unsigned int  nr_active_pages		__attribute__ ((packed));
   unsigned int  nr_inactive_dirty_pages	__attribute__ ((packed));
   unsigned int  nr_inactive_clean_pages	__attribute__ ((packed));
   /* This field of type 'long' _must_ be aligned(8) even if the attribute
    * here wasn't used. Else, FILE_STATS_SIZE below would be wrong! */
   unsigned long inactive_target		__attribute__ ((aligned (8)));
   unsigned int  load_avg_1			__attribute__ ((aligned (8)));
   unsigned int  load_avg_5			__attribute__ ((packed));
   unsigned int  nr_running			__attribute__ ((packed));
   unsigned int  nr_threads			__attribute__ ((packed));
};

#define FILE_STATS_SIZE	(sizeof(int)  * 37 + \
			 sizeof(char) *  4 + \
			 SIZEOF_LONG  * 14)

struct stats_one_cpu {
   unsigned long per_cpu_idle			__attribute__ ((aligned (8)));
   unsigned int  per_cpu_user			__attribute__ ((aligned (8)));
   unsigned int  per_cpu_nice			__attribute__ ((packed));
   unsigned int  per_cpu_system			__attribute__ ((packed));
   unsigned char pad[4]				__attribute__ ((packed));
/* IMPORTANT NOTE:
 * Structure must be a multiple of 8 bytes, since we use an array of structures.
 * Each structure is *aligned*, and we want the structures to be packed together.
 */
};

#define STATS_ONE_CPU_SIZE	(sizeof(int)  * 3 + \
				 sizeof(char) * 4 + \
				 SIZEOF_LONG)

struct pid_stats {
   /* If pid is null, the process has been killed */
   unsigned long pid				__attribute__ ((aligned (8)));
   unsigned long minflt				__attribute__ ((aligned (8)));
   unsigned long majflt				__attribute__ ((aligned (8)));
   unsigned long utime				__attribute__ ((aligned (8)));
   unsigned long stime				__attribute__ ((aligned (8)));
   unsigned long nswap				__attribute__ ((aligned (8)));
   unsigned long cminflt			__attribute__ ((aligned (8)));
   unsigned long cmajflt			__attribute__ ((aligned (8)));
   unsigned long cutime				__attribute__ ((aligned (8)));
   unsigned long cstime				__attribute__ ((aligned (8)));
   unsigned long cnswap				__attribute__ ((aligned (8)));
   unsigned int  processor			__attribute__ ((aligned (8)));
   unsigned char flag				__attribute__ ((packed));
   unsigned char pad[3]				__attribute__ ((packed));
   /* See IMPORTANT NOTE above */
};

#define PID_STATS_SIZE	(sizeof(int) + \
			 sizeof(char) * 4 + \
			 SIZEOF_LONG  * 11)

struct stats_serial {
   unsigned int  rx				__attribute__ ((aligned (8)));
   unsigned int  tx				__attribute__ ((packed));
   unsigned char line				__attribute__ ((packed));
   unsigned char pad[7]				__attribute__ ((packed));
   /* See IMPORTANT NOTE above */
};

#define STATS_SERIAL_SIZE	(sizeof(int)  * 2 + \
				 sizeof(char) * 8)

/* See linux source file linux/include/linux/netdevice.h */
struct stats_net_dev {
   unsigned long rx_packets			__attribute__ ((aligned (8)));
   unsigned long tx_packets			__attribute__ ((aligned (8)));
   unsigned long rx_bytes			__attribute__ ((aligned (8)));
   unsigned long tx_bytes			__attribute__ ((aligned (8)));
   unsigned long rx_compressed			__attribute__ ((aligned (8)));
   unsigned long tx_compressed			__attribute__ ((aligned (8)));
   unsigned long multicast			__attribute__ ((aligned (8)));
   unsigned long collisions			__attribute__ ((aligned (8)));
   unsigned long rx_errors			__attribute__ ((aligned (8)));
   unsigned long tx_errors			__attribute__ ((aligned (8)));
   unsigned long rx_dropped			__attribute__ ((aligned (8)));
   unsigned long tx_dropped			__attribute__ ((aligned (8)));
   unsigned long rx_fifo_errors			__attribute__ ((aligned (8)));
   unsigned long tx_fifo_errors			__attribute__ ((aligned (8)));
   unsigned long rx_frame_errors		__attribute__ ((aligned (8)));
   unsigned long tx_carrier_errors		__attribute__ ((aligned (8)));
   unsigned char interface[7]			__attribute__ ((aligned (8)));
   unsigned char pad				__attribute__ ((packed));
   /* See IMPORTANT NOTE above */
};

#define STATS_NET_DEV_SIZE	(SIZEOF_LONG  * 16 + \
				 sizeof(char) * 8)

struct stats_irq_cpu {
   unsigned int interrupt			__attribute__ ((aligned (8)));
   unsigned int irq				__attribute__ ((packed));
   /* See IMPORTANT NOTE above */
};

#define STATS_IRQ_CPU_SIZE	(sizeof(int) * 2)
#define STATS_ONE_IRQ_SIZE	(sizeof(int) * NR_IRQS)


struct disk_stats {
   unsigned int major				__attribute__ ((aligned (8)));
   unsigned int index				__attribute__ ((packed));
   unsigned int dk_drive			__attribute__ ((packed));
   unsigned int dk_drive_rwblk			__attribute__ ((packed));
   /* See IMPORTANT NOTE above */
};

#define DISK_STATS_SIZE		(sizeof(int) * 4)


struct stats_sum {
   unsigned long count				__attribute__ ((aligned (8)));
   unsigned long frmkb				__attribute__ ((packed));
   unsigned long shmkb				__attribute__ ((packed));
   unsigned long bufkb				__attribute__ ((packed));
   unsigned long camkb				__attribute__ ((packed));
   unsigned long frskb				__attribute__ ((packed));
   unsigned long tlskb				__attribute__ ((packed));
   unsigned long dentry_stat			__attribute__ ((packed));
   unsigned long file_used			__attribute__ ((packed));
   unsigned long inode_used			__attribute__ ((packed));
   unsigned long super_used			__attribute__ ((packed));
   unsigned long dquot_used			__attribute__ ((packed));
   unsigned long rtsig_queued			__attribute__ ((packed));
   unsigned long sock_inuse			__attribute__ ((packed));
   unsigned long tcp_inuse			__attribute__ ((packed));
   unsigned long udp_inuse			__attribute__ ((packed));
   unsigned long raw_inuse			__attribute__ ((packed));
   unsigned long frag_inuse			__attribute__ ((packed));
   unsigned long nr_active_pages		__attribute__ ((packed));
   unsigned long nr_inactive_dirty_pages	__attribute__ ((packed));
   unsigned long nr_inactive_clean_pages	__attribute__ ((packed));
   unsigned long inactive_target		__attribute__ ((packed));
   unsigned long nr_running			__attribute__ ((packed));
   unsigned long nr_threads			__attribute__ ((packed));
   unsigned long load_avg_1			__attribute__ ((packed));
   unsigned long load_avg_5			__attribute__ ((packed));
};

#define STATS_SUM_SIZE	(sizeof(long) * 26)

struct tstamp {
   int tm_sec;
   int tm_min;
   int tm_hour;
   int use;
};

/* Time must have the format HH:MM:SS with HH in 24-hour format */
#define DEF_TMSTART	"08:00:00"
#define DEF_TMEND	"18:00:00"

#define CLOSE_ALL(_fd_)		close(_fd_[0]); \
				close(_fd_[1])

#endif  /* _SA_H */
