/*
 * sysstat: System performance tools for Linux
 * (C) 1999-2007 by Sebastien Godard (sysstat <at> wanadoo.fr)
 */

#ifndef _COMMON_H
#define _COMMON_H

#include <time.h>

#define FALSE	0
#define TRUE	1

#define MINIMUM(a,b)	((a) < (b) ? (a) : (b))

#define NR_CPUS		1024

/*
 * Size of /proc/interrupts line (at most NR_CPUS # of cpus)
 * 4 spaces for interrupt # field ; 11 spaces for each interrupt field.
 */
#define INTERRUPTS_LINE	(4 + 11 * NR_CPUS)

/* Keywords */
#define K_ISO	"ISO"
#define K_ALL	"ALL"
#define K_UTC	"UTC"

/* Files */
#define STAT		"/proc/stat"
#define UPTIME		"/proc/uptime"
#define PPARTITIONS	"/proc/partitions"
#define DISKSTATS	"/proc/diskstats"
#define INTERRUPTS	"/proc/interrupts"
#define MEMINFO		"/proc/meminfo"
#define SYSFS_BLOCK	"/sys/block"
#define SYSFS_DEVCPU	"/sys/devices/system/cpu"
#define NFSMOUNTSTATS	"/proc/self/mountstats"
#define S_STAT		"stat"
#define DEVMAP_DIR	"/dev/mapper"

#define MAX_FILE_LEN	256
#define MAX_PF_NAME	1024
#define DEVMAP_MAJOR	253

#define NR_DEV_PREALLOC		4
#define NR_DISK_PREALLOC	3
#define NR_NFS_PREALLOC		2

#define CNT_DEV		0
#define CNT_PART	1
#define CNT_ALL_DEV	0
#define CNT_USED_DEV	1

#define S_VALUE(m,n,p)	(((double) ((n) - (m))) / (p) * HZ)

/* new define to normalize to %; HZ is 1024 on IA64 and % should be normalized to 100 */
#define SP_VALUE(m,n,p)	(((double) ((n) - (m))) / (p) * 100)

/* Environment variables */
#define ENV_TIME_FMT	"S_TIME_FORMAT"
#define ENV_TIME_DEFTM	"S_TIME_DEF_TIME"

#define DIGITS		"0123456789"

#define UTSNAME_LEN	65
#define MAX_NAME_LEN	72

#define NR_DISKS	4

#define DISP_HDR	1

/* Number of ticks per second */
#define HZ		hz
extern unsigned int hz;

/* Number of bit shifts to convert pages to kB */
extern unsigned int kb_shift;

/* Memory data read from /proc/meminfo */
struct meminf {
   unsigned long frmkb;
   unsigned long bufkb;
   unsigned long camkb;
   unsigned long tlmkb;
   unsigned long frskb;
   unsigned long tlskb;
   unsigned long caskb;
};
   

/*
 * Under very special circumstances, STDOUT may become unavailable,
 * This is what we try to guess here
 */
#define TEST_STDOUT(_fd_)	do {					\
   				   if (write(_fd_, "", 0) == -1) {	\
				       perror("stdout");		\
				       exit(6);				\
				   }					\
				} while (0)

/* Functions */
extern char 	*device_name(char *);
extern void	get_HZ(void);
extern unsigned int	get_disk_io_nr(void);
extern unsigned long long 	get_interval(unsigned long long,
					     unsigned long long);
extern void	get_kb_shift(void);
extern time_t	get_localtime(struct tm *);
extern time_t	get_time(struct tm *);
extern int	get_cpu_nr(unsigned int);
extern int	get_sysfs_dev_nr(int);
extern int	get_diskstats_dev_nr(int, int);
extern int	get_ppartitions_dev_nr(int);
extern int	get_nfs_mount_nr(void);
extern int	get_win_height(void);
extern void	init_nls(void);
extern double	ll_s_value(unsigned long long, unsigned long long,
			   unsigned long long);
extern double	ll_sp_value(unsigned long long, unsigned long long,
			    unsigned long long);
extern int	print_gal_header(struct tm *, char *, char *, char *);
extern void	print_version(void);
extern int	readp_meminfo(struct meminf *);
extern void	readp_uptime(unsigned long long *);

#endif  /* _COMMON_H */
