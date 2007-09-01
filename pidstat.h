/*
 * pidstat: Display per-process statistics.
 * (C) 2007 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _PIDSTAT_H
#define _PIDSTAT_H


#define K_SELF		"SELF"

#define K_P_TASK	"TASK"
#define K_P_CHILD	"CHILD"
#define K_P_ALL		"ALL"

#define NR_PID_PREALLOC	10

#define MAX_COMM_LEN	16

/* Activities */
#define P_A_CPU		0x01
#define P_A_MEM		0x02
#define P_A_IO		0x04

#define DISPLAY_CPU(m)		(((m) & P_A_CPU) == P_A_CPU)
#define DISPLAY_MEM(m)		(((m) & P_A_MEM) == P_A_MEM)
#define DISPLAY_IO(m)		(((m) & P_A_IO) == P_A_IO)

/* TASK/CHILD */
#define P_NULL		0x00
#define P_TASK		0x01
#define P_CHILD		0x02

#define DISPLAY_TASK_STATS(m)	(((m) & P_TASK) == P_TASK)
#define DISPLAY_CHILD_STATS(m)	(((m) & P_CHILD) == P_CHILD)

#define P_D_PID		0x01
#define P_D_ALL_PID	0x02
#define P_F_IRIX_MODE	0x04
#define P_F_COMMSTR	0x08
#define P_D_ACTIVE_PID	0x10

#define DISPLAY_PID(m)		(((m) & P_D_PID) == P_D_PID)
#define DISPLAY_ALL_PID(m)	(((m) & P_D_ALL_PID) == P_D_ALL_PID)
#define IRIX_MODE_OFF(m)	(((m) & P_F_IRIX_MODE) == P_F_IRIX_MODE)
#define COMMAND_STRING(m)	(((m) & P_F_COMMSTR) == P_F_COMMSTR)
#define DISPLAY_ACTIVE_PID(m)	(((m) & P_D_ACTIVE_PID) == P_D_ACTIVE_PID)


#define PROC		"/proc"
#define PID_STAT	"/proc/%lu/stat"
#define PID_STATUS	"/proc/%lu/status"
#define PID_IO		"/proc/%lu/io"

struct pid_stats {
   unsigned long long read_bytes		__attribute__ ((aligned (8)));
   unsigned long long write_bytes		__attribute__ ((packed));
   unsigned long long cancelled_write_bytes	__attribute__ ((packed));
   unsigned long long total_vsz			__attribute__ ((packed));
   unsigned long long total_rss			__attribute__ ((packed));
   /* If pid is null, the process has been killed */
   unsigned long      pid			__attribute__ ((packed));
   unsigned long      minflt			__attribute__ ((packed));
   unsigned long      cminflt			__attribute__ ((packed));
   unsigned long      majflt			__attribute__ ((packed));
   unsigned long      cmajflt			__attribute__ ((packed));
   unsigned long      utime			__attribute__ ((packed));
   unsigned long      cutime			__attribute__ ((packed));
   unsigned long      stime			__attribute__ ((packed));
   unsigned long      cstime			__attribute__ ((packed));
   unsigned long      vsz			__attribute__ ((packed));
   unsigned long      rss			__attribute__ ((packed));
   unsigned int       rt_asum_count		__attribute__ ((packed));
   unsigned int       rc_asum_count		__attribute__ ((packed));
   unsigned int       uc_asum_count		__attribute__ ((packed));
   unsigned int       processor			__attribute__ ((packed));
   char               comm[MAX_COMM_LEN];
};

#define PID_STATS_SIZE	(sizeof(struct pid_stats))

#endif  /* _PIDSTAT_H */
