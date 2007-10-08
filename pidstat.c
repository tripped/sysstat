/*
 * pidstat: Report statistics for Linux tasks
 * (C) 2007 by Sebastien GODARD (sysstat <at> orange.fr)
 *
 ***************************************************************************
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published  by  the *
 * Free Software Foundation; either version 2 of the License, or (at  your *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it  will  be  useful,  but *
 * WITHOUT ANY WARRANTY; without the implied warranty  of  MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License *
 * for more details.                                                       *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA                   *
 ***************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include "pidstat.h"
#include "common.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

#define SCCSID "@(#)" __FILE__ " compiled " __DATE__ " " __TIME__
char *sccsid(void) { return (SCCSID); }

unsigned long long uptime[3] = {0, 0, 0};
unsigned long long uptime0[3] = {0, 0, 0};
struct pid_stats *st_pid_list[3] = {NULL, NULL, NULL};
unsigned int *pid_array = NULL;
struct pid_stats st_pid_null;
struct tm ps_tstamp[3];
char commstr[MAX_COMM_LEN];

unsigned int pid_nr = 0;	/* Nb of PID to display */
unsigned int pid_array_nr = 0;
int cpu_nr = 0;			/* Nb of processors on the machine */
unsigned long tlmkb;		/* Total memory in kB */
long interval = -1;
unsigned int pidflag = 0;	/* General flags */
unsigned int tskflag = 0;	/* TASK/CHILD stats */
unsigned int actflag = 0;	/* Activity flag */


/*
 ***************************************************************************
 * Print usage and exit
 ***************************************************************************
 */
void usage(char *progname)
{
   fprintf(stderr, _("Usage: %s [ options... ] [ <interval> [ <count> ] ]\n"
		   "Options are:\n"
		   "[ -C <comm> ] [ -d ] [ -I ] [ -r ] [ -t ] [ -u ] [ -V ]\n"
		   "[ -p { <pid> | SELF | ALL } ] [ -T { TASK | CHILD | ALL } ]\n"),
	   progname);
   exit(1);
}


/*
 ***************************************************************************
 * SIGALRM signal handler
 ***************************************************************************
 */
void alarm_handler(int sig)
{
   signal(SIGALRM, alarm_handler);
   alarm(interval);
}


/*
 ***************************************************************************
 * Initialize uptime variables
 ***************************************************************************
 */
void init_stats(void)
{
   memset(&st_pid_null, 0, PID_STATS_SIZE);
}


/*
 ***************************************************************************
 * Allocate structures for PIDs entered on command line
 ***************************************************************************
 */
void salloc_pid_array(unsigned int len)
{
   if ((pid_array = (unsigned int *) malloc(sizeof(int) * len)) == NULL) {
      perror("malloc");
      exit(4);
   }
   memset(pid_array, 0, sizeof(int) * len);
}


/*
 ***************************************************************************
 * Allocate structures for PIDs to read
 ***************************************************************************
 */
void salloc_pid(unsigned int len)
{
   int i;

   for (i = 0; i < 3; i++) {
      if ((st_pid_list[i] = (struct pid_stats *) malloc(PID_STATS_SIZE * len)) == NULL) {
	 perror("malloc");
	 exit(4);
      }
      memset(st_pid_list[i], 0, PID_STATS_SIZE * len);
   }
}


/*
 ***************************************************************************
 * Check flags and set default values
 ***************************************************************************
 */
void check_flags(void)
{
   unsigned int act = 0;

   /* Display CPU usage for active tasks by default */
   if (!actflag)
      actflag |= P_A_CPU;

   if (!DISPLAY_PID(pidflag))
      pidflag |= P_D_ACTIVE_PID + P_D_PID + P_D_ALL_PID;

   if (!tskflag)
      tskflag |= P_TASK;

   /* Check that requested activities are available */
   if (DISPLAY_TASK_STATS(tskflag))
      act |= P_A_CPU + P_A_MEM + P_A_IO;
   if (DISPLAY_CHILD_STATS(tskflag))
      act |= P_A_CPU + P_A_MEM;

   actflag &= act;
   
   if (!actflag) {
      fprintf(stderr, _("Requested activities not available\n"));
      exit(1);
   }
}


/*
 ***************************************************************************
 * Look for the PID in the list of PIDs entered on the command line, and
 * store it if necessary. Returns the position of the PID in the list.
 ***************************************************************************
 */
int update_pid_array(unsigned int *pid_array_nr, unsigned int pid)
{
   unsigned int i;

   for (i = 0; i < *pid_array_nr; i++) {
      if (pid_array[i] == pid)
	 break;
   }

   if (i == *pid_array_nr) {
      /* PID not found: Store it */
      (*pid_array_nr)++;
      pid_array[i] = pid;
   }

   return i;
}


/*
 ***************************************************************************
 * Read /proc/meminfo
 ***************************************************************************
 */
void read_proc_meminfo(void)
{
   struct meminf st_mem;

   if (readp_meminfo(&st_mem))
      tlmkb = 0;
   else
      tlmkb = st_mem.tlmkb;
}


/*
 ***************************************************************************
 * Read stats from /proc/stat file.
 * Used to get overall CPU utilization and possibly time interval.
 ***************************************************************************
 */
void read_proc_stat(int curr)
{
   FILE *fp;
   char line[8192];
   unsigned long long cc_user, cc_nice, cc_system, cc_idle;
   unsigned long long cc_iowait, cc_steal, cc_hardirq, cc_softirq;

   if ((fp = fopen(STAT, "r")) == NULL) {
      perror("fopen");
      exit(2);
   }

   /* Some fields are only present in 2.6 kernels */
   cc_iowait = cc_hardirq = cc_softirq = cc_steal = 0;

   while (fgets(line, 8192, fp) != NULL) {

      if (!strncmp(line, "cpu ", 4)) {
	 /*
	  * Read the number of jiffies spent in the different modes,
	  * and compute system uptime in jiffies (1/100ths of a second
	  * if HZ=100).
	  */
	 sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu %llu",
	        &cc_user, &cc_nice, &cc_system, &cc_idle, &cc_iowait,
		&cc_hardirq, &cc_softirq, &cc_steal);

	 /*
	  * Compute system uptime in jiffies.
	  * Uptime is multiplied by the number of processors.
	  */
	 uptime[curr] = cc_user + cc_nice + cc_system +
	                cc_idle + cc_iowait + cc_hardirq +
	                cc_softirq + cc_steal;
      }

      else if ((!strncmp(line, "cpu0", 4)) && !uptime0[curr]
	       && (cpu_nr > 1)) {
	 /*
	  * Read CPU line for proc#0 (if available).
	  * Done only if /proc/uptime was unavailable.
	  */
	 sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu %llu",
		&cc_user, &cc_nice, &cc_system, &cc_idle, &cc_iowait,
		&cc_hardirq, &cc_softirq, &cc_steal);
	
	 uptime0[curr] = cc_user + cc_nice + cc_system +
	                 cc_idle + cc_iowait + cc_hardirq +
	                 cc_softirq + cc_steal;
      }
   }
   fclose(fp);
}


/*
 ***************************************************************************
 * Read stats from /proc/<pid>/stat
 ***************************************************************************
 */
int read_proc_pid_stat(unsigned int pid, struct pid_stats *pst,
		       unsigned int *thread_nr, unsigned int tgid)
{
   FILE *fp;
   char filename[128], format[256], comm[MAX_COMM_LEN + 1];
   size_t len;

   if (tgid)
      sprintf(filename, TASK_STAT, tgid, pid);
   else
      sprintf(filename, PID_STAT, pid);
   
   if ((fp = fopen(filename, "r")) == NULL)
      /* No such process */
      return 1;

   sprintf(format, "%%*d (%%%ds %%*s %%*d %%*d %%*d %%*d %%*d %%*u %%lu %%lu"
	   " %%lu %%lu %%lu %%lu %%lu %%lu %%*d %%*d %%u %%*u %%*d %%lu %%lu"
	   " %%*u %%*u %%*u %%*u %%*u %%*u %%*u %%*u %%*u %%*u %%*u %%*u %%*u"
	   " %%*u %%u\\n", MAX_COMM_LEN);

   fscanf(fp, format, comm,
	  &(pst->minflt), &(pst->cminflt), &(pst->majflt), &(pst->cmajflt),
	  &(pst->utime),  &(pst->stime), &(pst->cutime), &(pst->cstime),
	  thread_nr, &(pst->vsz), &(pst->rss), &(pst->processor));

   fclose(fp);

   /* Convert to kB */
   pst->vsz >>= 10;
   pst->rss = PG_TO_KB(pst->rss);
   
   strncpy(pst->comm, comm, MAX_COMM_LEN);
   pst->comm[MAX_COMM_LEN - 1] = '\0';

   /* Remove trailing ')' */
   len = strlen(pst->comm);
   if (len && (pst->comm[len - 1] == ')'))
      pst->comm[len - 1] = '\0';

   pst->pid = pid;
   pst->tgid = tgid;
   return 0;
}


/*
 ***************************************************************************
 * Read stats from /proc/<pid>/io
 ***************************************************************************
 */
int read_proc_pid_io(unsigned int pid, struct pid_stats *pst,
		     unsigned int tgid)
{
   FILE *fp;
   char filename[128], line[256];

   if (tgid)
      sprintf(filename, TASK_IO, tgid, pid);
   else
      sprintf(filename, PID_IO, pid);

   if ((fp = fopen(filename, "r")) == NULL) {
      /* No such process... or file non existent! */
      pst->flags |= F_NO_PID_IO;
      return 0;
   }

   while (fgets(line, 256, fp) != NULL) {

      if (!strncmp(line, "read_bytes:", 11))
	 sscanf(line + 12, "%llu", &(pst->read_bytes));
      else if (!strncmp(line, "write_bytes:", 12))
	 sscanf(line + 13, "%llu", &(pst->write_bytes));
      else if (!strncmp(line, "cancelled_write_bytes:", 22))
	 sscanf(line + 23, "%llu", &(pst->cancelled_write_bytes));
   }

   fclose(fp);

   pst->pid = pid;
   pst->tgid = tgid;
   pst->flags &= ~F_NO_PID_IO;
   return 0;
}


/*
 ***************************************************************************
 * Read various stats for given PID
 ***************************************************************************
 */
int read_pid_stats(unsigned int pid, struct pid_stats *pst,
		   unsigned int *thread_nr, unsigned int tgid)
{
   if (read_proc_pid_stat(pid, pst, thread_nr, tgid))
      return 1;

   if (DISPLAY_IO(actflag))
      /* Assume that /proc/#/task/#/io exists! */
      return (read_proc_pid_io(pid, pst, tgid));

   return 0;
}


/*
 ***************************************************************************
 * Count nb of threads in /proc/#/task directory, including the leader one.
 ***************************************************************************
 */
unsigned int count_tid(unsigned int pid)
{
   struct pid_stats pst;
   unsigned int thread_nr;

   if (read_proc_pid_stat(pid, &pst, &thread_nr, 0) != 0)
      /* Task no longer exists */
      return 0;
   
   return thread_nr;
}


/*
 ***************************************************************************
 * Count nb of PIDs
 ***************************************************************************
 */
unsigned int count_pid(void)
{
   DIR *dir;
   struct dirent *drp;
   unsigned int pid = 0;

   /* Open /proc directory */
   if ((dir = opendir(PROC)) == NULL) {
      perror("opendir");
      exit(4);
   }

   /* Get directory entries */
   while ((drp = readdir(dir)) != NULL) {
      if (isdigit(drp->d_name[0])) {
	 /* There is at least the TGID */
	 pid++;
	 if (DISPLAY_TID(pidflag))
	    pid += count_tid(atoi(drp->d_name));
      }
   }

   /* Close /proc directory */
   closedir(dir);

   return pid;
}

/*
 ***************************************************************************
 * Count number of threads associated with the tasks entered on the command
 * line.
 ***************************************************************************
 */
unsigned int count_tid_in_list(void)
{
   unsigned int p, tid, pid = 0;
   
   for (p = 0; p < pid_array_nr; p++) {
      
      tid = count_tid(pid_array[p]);
      
      if (!tid)
	 /* PID no longer exists */
	 pid_array[p] = 0;
      else
	 /* <tid> TIDs + 1 TGID */
	 pid += tid + 1;
   }
   
   return pid;
}


/*
 ***************************************************************************
 * Allocate and init structures according to system state
 ***************************************************************************
 */
void pid_sys_init(void)
{
   /* Init stat common counters */
   init_stats();

   /* Count nb of proc */
   cpu_nr = get_cpu_nr(~0);

   if (DISPLAY_ALL_PID(pidflag)) {
      /* Count PIDs and allocate structures */
      pid_nr = count_pid() + NR_PID_PREALLOC;
      salloc_pid(pid_nr);
   }
   else if (DISPLAY_TID(pidflag)) {
      /* Count total number of threads associated with tasks in list */
      pid_nr = count_tid_in_list() + NR_PID_PREALLOC;
      salloc_pid(pid_nr);
   }
   else {
      pid_nr = pid_array_nr;
      salloc_pid(pid_nr);
   }
}


/*
 ***************************************************************************
 * Read stats for threads in /proc/#/task directory
 ***************************************************************************
 */
void read_task_stats(int curr, unsigned int pid, unsigned int *index)
{
   DIR *dir;
   struct dirent *drp;
   char filename[128];
   struct pid_stats *psti;
   unsigned int thr_nr;

   /* Open /proc/#/task directory */
   sprintf(filename, PROC_TASK, pid);
   if ((dir = opendir(filename)) == NULL)
      return;
   
   while (*index < pid_nr) {
      
      while ((drp = readdir(dir)) != NULL) {
	 if (isdigit(drp->d_name[0]))
	    break;
      }
      
      if (drp) {
	 psti = st_pid_list[curr] + (*index)++;
	 if (read_pid_stats(atoi(drp->d_name), psti, &thr_nr, pid))
	    /* Thread no longer exists */
	    psti->pid = 0;
      }
      else
	 break;
   }
}


/*
 ***************************************************************************
 * Read various stats
 ***************************************************************************
 */
void read_stats(int curr)
{
   DIR *dir;
   struct dirent *drp;
   unsigned int p = 0, q, pid, thr_nr;
   struct pid_stats *psti;

   /* Read CPU statistics */
   read_proc_stat(curr);

   if (DISPLAY_ALL_PID(pidflag)) {

      /* Open /proc directory */
      if ((dir = opendir(PROC)) == NULL) {
	 perror("opendir");
	 exit(4);
      }

      while (p < pid_nr) {

	 /* Get directory entries */
	 while ((drp = readdir(dir)) != NULL) {
	    if (isdigit(drp->d_name[0]))
	       break;
	 }
	 if (drp) {
	    psti = st_pid_list[curr] + p++;
	    pid = atoi(drp->d_name);
	    
	    if (read_pid_stats(pid, psti, &thr_nr, 0))
	       /* Process has terminated */
	       psti->pid = 0;
	    
	    else if (DISPLAY_TID(pidflag))
	       /* Read stats for threads in task subdirectory */
	       read_task_stats(curr, pid, &p);
	 }
	 else {
	    for (q = p; q < pid_nr; q++) {
	       psti = st_pid_list[curr] + q;
	       psti->pid = 0;
	    }
	    break;
	 }
      }

      /* Close /proc directory */
      closedir(dir);
   }

   else if (DISPLAY_PID(pidflag)) {
      unsigned int op;

      /* Read stats for each PID in the list */
      for (op = 0; op < pid_array_nr; op++) {

	 if (p >= pid_nr)
	    break;
	 psti = st_pid_list[curr] + p++;
	
	 if (pid_array[op]) {
	    /* PID should still exist. So read its stats */
	    if (read_pid_stats(pid_array[op], psti, &thr_nr, 0)) {
	       /* PID has terminated */
	       psti->pid = 0;
	       pid_array[op] = 0;
	    }
	    else if (DISPLAY_TID(pidflag))
	       read_task_stats(curr, pid_array[op], &p);
	 }
      }
   }
   /* else unknonw command */
}


/*
 ***************************************************************************
 * Get current PID to display
 * Return value: 0 if PID no longer exists
 * 		-1 if PID exists but should not be displayed
 * 		 1 if PID can be displayed
 * First, check that PID exists. *Then* check that it's an active process
 * and/or that the string is found in command name.
 ***************************************************************************
 */
int get_pid_to_display(int prev, int curr, int p, unsigned int activity,
		       unsigned int pflag,
		       struct pid_stats **psti, struct pid_stats **pstj)
{
   int q;

   *psti = st_pid_list[curr] + p;
   
   if (!(*psti)->pid)
      /* PID no longer exists */
      return 0;
   
   /* Don't display I/O stats for any PID if /proc/#/io file doesn't exist */
   if (DISPLAY_IO(activity) && NO_PID_IO((*psti)->flags))
      return -1;

   if (DISPLAY_ALL_PID(pidflag) || DISPLAY_TID(pidflag)) {

      /* Look for previous stats for same PID */
      q = p;
	
      do {
	 *pstj = st_pid_list[prev] + q;
	 if ((*pstj)->pid == (*psti)->pid)
	    break;
	 q++;
	 if (q >= pid_nr)
	    q = 0;
      }
      while (q != p);

      if ((*pstj)->pid != (*psti)->pid)
	 /* PID not found (no data previously read) */
	 *pstj = &st_pid_null;

      if (DISPLAY_ACTIVE_PID(pidflag)) {
	 /* Check that it's an "active" process */
	 if (DISPLAY_CPU(activity)) {
	    if (((*psti)->utime == (*pstj)->utime) &&
		((*psti)->stime == (*pstj)->stime)) {
	       if (DISPLAY_TASK_STATS(pflag))
		  return -1;	/* Inactive process */
	       else if (DISPLAY_CHILD_STATS(pflag)) {
		  if (((*psti)->cutime == (*pstj)->cutime) &&
		      ((*psti)->cstime == (*pstj)->cstime))
		     return -1;
	       }
	    }
	 }
	 else if (DISPLAY_MEM(activity)) {
	    if (((*psti)->minflt == (*pstj)->minflt) &&
		((*psti)->majflt == (*pstj)->majflt)) {
	       if (DISPLAY_TASK_STATS(pflag)) {
		  if (((*psti)->vsz == (*pstj)->vsz) &&
		      ((*psti)->rss == (*pstj)->rss))
		     return -1;
	       }
	       else if (DISPLAY_CHILD_STATS(pflag)) {
		  if (((*psti)->cminflt == (*pstj)->cminflt) &&
		      ((*psti)->cmajflt == (*pstj)->cmajflt))
		     return -1;
	       }
	    }
	 }
	 else if (DISPLAY_IO(activity)) {
	    if (((*psti)->read_bytes == (*pstj)->read_bytes) &&
		((*psti)->write_bytes == (*pstj)->write_bytes) &&
		((*psti)->cancelled_write_bytes ==
		 (*pstj)->cancelled_write_bytes))
	       return -1;
	 }
      }
   }
	
   else if (DISPLAY_PID(pidflag)) {

      *pstj = st_pid_list[prev] + p;
   }

   if (COMMAND_STRING(pidflag) && !(strstr((*psti)->comm, commstr)))
      return -1;	/* String not found in command name */

   return 1;
}


/*
 ***************************************************************************
 * Display timestamp, PID and TID
 ***************************************************************************
 */
void print_line_id(char *timestamp, struct pid_stats *pst)
{
   char format[32];
   
   printf("%-11s", timestamp);
   
   if (DISPLAY_TID(pidflag)) {
      if (pst->tgid)
	 /* This is a TID */
	 strcpy(format, "        -  %9u");
      else
	 /* This is a PID (TGID) */
	 strcpy(format, " %9u        - ");
   }
   else
      strcpy(format, " %9u");
   
   printf(format, pst->pid);
}

/*
 ***************************************************************************
 * Display statistics
 ***************************************************************************
 */
int write_stats_core(int prev, int curr, int dis, int disp_avg,
		     char *prev_string, char *curr_string)
{
   struct pid_stats *psti, *pstj;
   unsigned long long itv, g_itv;
   unsigned int p;
   int again = 0, rc;

   /* Test stdout */
   TEST_STDOUT(STDOUT_FILENO);

   /* g_itv is multiplied by the number of processors */
   g_itv = get_interval(uptime[prev], uptime[curr]);

   if (cpu_nr > 1)
      /* SMP machines */
      itv = get_interval(uptime0[prev], uptime0[curr]);
   else
      /* UP machines */
      itv = g_itv;

   if (DISPLAY_CPU(actflag)) {

      if (DISPLAY_TASK_STATS(tskflag)) {
	 if (dis) {
	    PRINT_ID_HDR(prev_string, pidflag);
	    printf("   %%user %%system    %%CPU   CPU  Command\n");
	 }
	 
	 for (p = 0; p < pid_nr; p++) {
	
	    if (get_pid_to_display(prev, curr, p, P_A_CPU, P_TASK,
				   &psti, &pstj) <= 0)
	       continue;
	
	    print_line_id(curr_string, psti);
	    printf(" %7.2f %7.2f %7.2f",
		   SP_VALUE(pstj->utime, psti->utime, itv),
		   SP_VALUE(pstj->stime, psti->stime, itv),
		   IRIX_MODE_OFF(pidflag) ?
		   SP_VALUE(pstj->utime + pstj->stime,
			    psti->utime + psti->stime, g_itv) :
		   SP_VALUE(pstj->utime + pstj->stime,
			    psti->utime + psti->stime, itv));
	    if (!disp_avg)
	       printf("   %3d", psti->processor);
	    else
	       printf("     -");
	    printf("  %s%s\n", (psti->tgid ? "|__" : ""), psti->comm);
	    again = 1;
	 }
      }
      if (DISPLAY_CHILD_STATS(tskflag)) {
	 if (dis) {
	    PRINT_ID_HDR(prev_string, pidflag);
	    printf("   user-ms system-ms  Command\n");
	 }

	 for (p = 0; p < pid_nr; p++) {
	
	    if ((rc = get_pid_to_display(prev, curr, p, P_A_CPU, P_CHILD,
					 &psti, &pstj)) == 0)
	       /* PID no longer exists */
	       continue;
	
	    /* This will be used to compute average */
	    if (!disp_avg)
	       psti->uc_asum_count = pstj->uc_asum_count + 1;
	
	    if (rc < 0)
	       /* PID should not be displayed */
	       continue;

	    print_line_id(curr_string, psti);
	    if (disp_avg) {
	       printf(" %9.0f %9.0f",
		      (double) ((psti->utime + psti->cutime) -
				(pstj->utime + pstj->cutime)) /
		      (HZ * psti->uc_asum_count) * 1000,
		      (double) ((psti->stime + psti->cstime) -
				(pstj->stime + pstj->cstime)) /
		      (HZ * psti->uc_asum_count) * 1000);
	    }
	    else {
	       printf(" %9.0f %9.0f",
		      (double) ((psti->utime + psti->cutime) -
				(pstj->utime + pstj->cutime)) / HZ * 1000,
		      (double) ((psti->stime + psti->cstime) -
				(pstj->stime + pstj->cstime)) / HZ * 1000);
	    }
	    printf("  %s%s\n", (psti->tgid ? "|__" : ""), psti->comm);
	    again = 1;
	 }
      }
   }

   if (DISPLAY_MEM(actflag)) {

      if (DISPLAY_TASK_STATS(tskflag)) {
	 if (dis) {
	    PRINT_ID_HDR(prev_string, pidflag);
	    printf("  minflt/s  majflt/s     VSZ    RSS   %%MEM  Command\n");
	 }
	 
	 for (p = 0; p < pid_nr; p++) {
	
	    if ((rc = get_pid_to_display(prev, curr, p, P_A_MEM, P_TASK,
					 &psti, &pstj)) == 0)
	       /* PID no longer exists */
	       continue;
	
	    /* This will be used to compute average */
	    if (!disp_avg) {
	       psti->total_vsz = pstj->total_vsz + psti->vsz;
	       psti->total_rss = pstj->total_rss + psti->rss;
	       psti->rt_asum_count = pstj->rt_asum_count + 1;
	    }
	
	    if (rc < 0)
	       /* PID should not be displayed */
	       continue;

	    print_line_id(curr_string, psti);
	    printf(" %9.2f %9.2f ",
		   S_VALUE(pstj->minflt, psti->minflt, itv),
		   S_VALUE(pstj->majflt, psti->majflt, itv));
	    if (disp_avg) {
	       printf("%7.0f %6.0f %6.2f",
		      (double) psti->total_vsz / psti->rt_asum_count,
		      (double) psti->total_rss / psti->rt_asum_count,
		      tlmkb ?
		      SP_VALUE(0, psti->total_rss / psti->rt_asum_count, tlmkb)
		      : 0.0);
	    }
	    else {
	       printf("%7lu %6lu %6.2f",
		      psti->vsz,
		      psti->rss,
		      tlmkb ? SP_VALUE(0, psti->rss, tlmkb) : 0.0);
	    }
	    printf("  %s%s\n", (psti->tgid ? "|__" : ""), psti->comm);
	    again = 1;
	 }
      }
      if (DISPLAY_CHILD_STATS(tskflag)) {
	 if (dis) {
	    PRINT_ID_HDR(prev_string, pidflag);
	    printf(" minflt-nr majflt-nr  Command\n");
	 }
	 
	 for (p = 0; p < pid_nr; p++) {
	
	    if ((rc = get_pid_to_display(prev, curr, p, P_A_MEM, P_CHILD,
					 &psti, &pstj)) == 0)
	       /* PID no longer exists */
	       continue;

	    /* This will be used to compute average */
	    if (!disp_avg)
	       psti->rc_asum_count = pstj->rc_asum_count + 1;
	
	    if (rc < 0)
	       /* PID should not be displayed */
	       continue;

	    print_line_id(curr_string, psti);
	    if (disp_avg) {
	       printf(" %9.0f %9.0f",
		      (double) ((psti->minflt + psti->cminflt) -
				(pstj->minflt + pstj->cminflt)) / psti->rc_asum_count,
		      (double) ((psti->majflt + psti->cmajflt) -
				(pstj->majflt + pstj->cmajflt)) / psti->rc_asum_count);
	    }
	    else {
	       printf(" %9lu %9lu",
		      (psti->minflt + psti->cminflt) - (pstj->minflt + pstj->cminflt),
		      (psti->majflt + psti->cmajflt) - (pstj->majflt + pstj->cmajflt));
	    }
	    printf("  %s%s\n", (psti->tgid ? "|__" : ""), psti->comm);
	    again = 1;
	 }
      }
   }

   if (DISPLAY_IO(actflag)) {
      if (dis) {
	 PRINT_ID_HDR(prev_string, pidflag);
	 printf("   kB_rd/s   kB_wr/s kB_ccwr/s  Command\n");
      }

      for (p = 0; p < pid_nr; p++) {
	
	 if (get_pid_to_display(prev, curr, p, P_A_IO, P_NULL,
				&psti, &pstj) <= 0)
	    continue;
	
	 print_line_id(curr_string, psti);
	 printf(" %9.2f %9.2f %9.2f",
		S_VALUE(pstj->read_bytes, psti->read_bytes, itv) / 1024,
		S_VALUE(pstj->write_bytes, psti->write_bytes, itv) / 1024,
		S_VALUE(pstj->cancelled_write_bytes,
			psti->cancelled_write_bytes, itv) / 1024);
	 printf("  %s%s\n", (psti->tgid ? "|__" : ""), psti->comm);
	 again = 1;
      }
   }

   if (DISPLAY_ALL_PID(pidflag))
      again = 1;

   return again;
}


/*
 ***************************************************************************
 * Print statistics average
 ***************************************************************************
 */
void write_stats_avg(int curr, int dis)
{
   char string[16];

   strcpy(string, _("Average:"));
   write_stats_core(2, curr, dis, TRUE, string, string);
}


/*
 ***************************************************************************
 * Print statistics
 ***************************************************************************
 */
int write_stats(int curr, int dis)
{
   char cur_time[2][16];

   /* Get previous timestamp */
   strftime(cur_time[!curr], 16, "%X", &(ps_tstamp[!curr]));

   /* Get current timestamp */
   strftime(cur_time[curr], 16, "%X", &(ps_tstamp[curr]));

   return (write_stats_core(!curr, curr, dis, FALSE,
			    cur_time[!curr], cur_time[curr]));
}


/*
 ***************************************************************************
 * Main loop: Read and dispalay PID stats
 ***************************************************************************
 */
void rw_pidstat_loop(int dis_hdr, long int count, unsigned long lines,
		     int rows)
{
   int curr = 1, dis = 1;
   int again;

   if (cpu_nr > 1) {
      /*
       * Read system uptime (only for SMP machines).
       * Init uptime0. So if /proc/uptime cannot fill it, this will be
       * done by /proc/stat.
       */
      uptime0[0] = 0;
      readp_uptime(&(uptime0[0]));
   }
   read_stats(0);

   if (DISPLAY_MEM(actflag))
      /* Get total memory */
      read_proc_meminfo();

   if (!interval) {
      /* Display since boot time */
      ps_tstamp[1] = ps_tstamp[0];
      memset(st_pid_list[1], 0, PID_STATS_SIZE * pid_nr);
      write_stats(0, DISP_HDR);
      exit(0);
   }

   /* Set a handler for SIGALRM */
   alarm_handler(0);

   /* Save the first stats collected. Will be used to compute the average */
   ps_tstamp[2] = ps_tstamp[0];
   uptime[2] = uptime[0];
   uptime0[2] = uptime0[0];
   memcpy(st_pid_list[2], st_pid_list[0], PID_STATS_SIZE * pid_nr);

   pause();

   do {
      /* Get time */
      get_localtime(&(ps_tstamp[curr]));

      if (cpu_nr > 1) {
	 /*
	  * Read system uptime (only for SMP machines).
	  * Init uptime0. So if /proc/uptime cannot fill it, this will be
	  * done by /proc/stat.
	  */
	 uptime0[curr] = 0;
	 readp_uptime(&(uptime0[curr]));
      }

      /* Read stats */
      read_stats(curr);

      if (!dis_hdr) {
	 dis = lines / rows;
	 if (dis)
	    lines %= rows;
	 lines++;
      }

      /* Print results */
      again = write_stats(curr, dis);
      fflush(stdout);

      if (!again)
	 return;

      if (count > 0)
	 count--;

      if (count) {
	 curr ^= 1;
	 pause();
      }
   }
   while (count);

   /* Write stats average */
   write_stats_avg(curr, dis_hdr);
}


/*
 ***************************************************************************
 * Main entry to the pidstat program
 ***************************************************************************
 */
int main(int argc, char **argv)
{
   int opt = 1, dis_hdr = -1;
   int i;
   long count = 0;
   unsigned int pid;
   struct utsname header;
   unsigned long lines = 0;
   int rows = 23;

#ifdef USE_NLS
   /* Init National Language Support */
   init_nls();
#endif

   /* Get HZ */
   get_HZ();
   
   /* Compute page shift in kB */
   get_kb_shift();

   /* Allocate structures for device list */
   if (argc > 1)
      salloc_pid_array((argc / 2) + 1);

   /* Process args... */
   while (opt < argc) {

      if (!strcmp(argv[opt], "-p")) {
	 pidflag |= P_D_PID;
	 if (argv[++opt]) {
	    if (!strcmp(argv[opt], K_ALL)) {
	       pidflag |= P_D_ALL_PID;
	       opt++;
	       continue;	/* Next option */
	    }
	    else if (!strcmp(argv[opt], K_SELF))
	       pid = getpid();
	    else {
	       if (strspn(argv[opt], DIGITS) != strlen(argv[opt]))
		  usage(argv[0]);
	       pid = atoi(argv[opt]);
	       if (pid < 1)
		  usage(argv[0]);
	    }
	    update_pid_array(&pid_array_nr, pid);
	    opt++;
	 }
	 else
	    usage(argv[0]);
      }

      else if (!strcmp(argv[opt], "-C")) {
	 if (argv[++opt]) {
	    strncpy(commstr, argv[opt++], MAX_COMM_LEN);
	    commstr[MAX_COMM_LEN - 1] = '\0';
	    pidflag |= P_F_COMMSTR;
	    if (!strlen(commstr))
	       usage(argv[0]);
	 }
	 else
	    usage(argv[0]);
      }

      else if (!strcmp(argv[opt], "-T")) {
	 if (argv[++opt]) {
	    if (tskflag)
	       dis_hdr++;
	    if (!strcmp(argv[opt], K_P_TASK))
	       tskflag |= P_TASK;
	    else if (!strcmp(argv[opt], K_P_CHILD))
	       tskflag |= P_CHILD;
	    else if (!strcmp(argv[opt], K_P_ALL)) {
	       tskflag |= P_TASK + P_CHILD;
	       dis_hdr++;
	    }
	    else
	       usage(argv[0]);
	    opt++;
	 }
	 else
	    usage(argv[0]);
      }

      else if (!strncmp(argv[opt], "-", 1)) {
	 for (i = 1; *(argv[opt] + i); i++) {

	    switch (*(argv[opt] + i)) {

	     case 'd':
	       actflag |= P_A_IO;	/* Display I/O usage */
	       dis_hdr++;
	       break;

	     case 'I':
	       pidflag |= P_F_IRIX_MODE; /* IRIX mode off */
	       break;
	
	     case 'r':
	       actflag |= P_A_MEM;	/* Display memory usage */
	       dis_hdr++;
	       break;
	       
	     case 't':
	       pidflag |= P_D_TID;	/* Display stats for threads */
	       break;

	     case 'u':
	       actflag |= P_A_CPU;	/* Display cpu usage */
	       dis_hdr++;
	       break;

	     case 'V':			/* Print version number and exit */
	       print_version();
	       break;
	
	     default:
	       usage(argv[0]);
	    }
	 }
	 opt++;
      }

      else if (interval < 0) {	/* Get interval */
	 if (strspn(argv[opt], DIGITS) != strlen(argv[opt]))
	    usage(argv[0]);
	 interval = atol(argv[opt++]);
	 if (interval < 0)
 	   usage(argv[0]);
	 count = -1;
      }

      else if (count <= 0) {	/* Get count value */
	 if ((strspn(argv[opt], DIGITS) != strlen(argv[opt])) ||
	     !interval)
	    usage(argv[0]);
	 count = atol(argv[opt++]);
	 if (count < 1)
	   usage(argv[0]);
      }
      else
	 usage(argv[0]);
   }

   if (interval < 0)
      /* Interval not set => display stats since boot time */
      interval = 0;

   /* Check flags and set default values */
   check_flags();

   /* Init structures */
   pid_sys_init();

   if (dis_hdr < 0)
      dis_hdr = 0;
   if (!dis_hdr) {
      if (pid_nr > 1)
	 dis_hdr = 1;
      else {
	 rows = get_win_height();
	 lines = rows;
      }
   }

   /* Get time */
   get_localtime(&(ps_tstamp[0]));

   /* Get system name, release number and hostname */
   uname(&header);
   print_gal_header(&(ps_tstamp[0]),
		    header.sysname, header.release, header.nodename);

   /* Main loop */
   rw_pidstat_loop(dis_hdr, count, lines, rows);

   return 0;
}