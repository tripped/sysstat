/*
 * pidstat: Display per-process statistics
 * (C) 2007 by Sebastien GODARD (sysstat <at> wanadoo.fr)
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

unsigned long long uptime[3] = {0, 0, 0};
unsigned long long uptime0[3] = {0, 0, 0};
struct pid_stats *st_pid_list[3] = {NULL, NULL, NULL};
struct pid_stats st_pid_null;
struct tm ps_tstamp[3];
char commstr[MAX_COMM_LEN];

unsigned int pid_nr = 0;	/* Nb of PID to display */
int cpu_nr = 0;			/* Nb of processors on the machine */
unsigned long tlmkb;		/* Total memory in kB */
long interval = -1;


/*
 ***************************************************************************
 * Print usage and exit
 ***************************************************************************
 */
void usage(char *progname)
{
   fprintf(stderr, _("Usage: %s [ options... ] [ <interval> [ <count> ] ]\n"
		   "Options are:\n"
		   "[ -C <comm> ] [ -I ] [ -r ] [ -u ] [ -V ]\n"
		   "[ -p { <pid> | SELF | ALL } ]\n"),
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
 * Allocate structures for PIDs
 ***************************************************************************
 */
void salloc_pid(unsigned int len)
{
   int i;
   
   for (i = 0; i < 3; i++) {
      if (st_pid_list[i] != NULL)
	 /* Free list if previously allocated */
	 free(st_pid_list[i]);
   
      if ((st_pid_list[i] = (struct pid_stats *) malloc(PID_STATS_SIZE * len)) == NULL) {
	 perror("malloc");
	 exit(4);
      }
      memset(st_pid_list[i], 0, PID_STATS_SIZE * len);
   }
}


/*
 ***************************************************************************
 * Look for the PID in the device list and store it if necessary.
 * Returns the position of the PID in the list.
 * PIDs are those entered on the command line.
 ***************************************************************************
 */
int update_pid_list(unsigned int *pid_nr, unsigned long pid)
{
   unsigned int i;
   struct pid_stats *psti = st_pid_list[2];

   for (i = 0; i < *pid_nr; i++, psti++) {
      if (psti->pid == pid)
	 break;
   }

   if (i == *pid_nr) {
      /* PID not found: Store it */
      (*pid_nr)++;
      psti->pid = pid;
   }

   return i;
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
      if (isdigit(drp->d_name[0]))
	 pid++;
   }

   /* Close /proc directory */
   closedir(dir);
   
   return pid;
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
int read_proc_pid_stat(unsigned long pid, unsigned int flags,
		       struct pid_stats *pst)
{
   FILE *fp;
   char filename[64], format[256], comm[MAX_COMM_LEN + 1];
   size_t len;

   sprintf(filename, PID_STAT, pid);
   if ((fp = fopen(filename, "r")) == NULL)
      /* No such process */
      return 1;

   sprintf(format, "%%*d (%%%ds %%*s %%*d %%*d %%*d %%*d %%*d %%*u %%lu %%lu"
	   " %%lu %%lu %%lu %%lu %%lu %%lu %%*d %%*d %%*u %%*u %%*d %%*u %%*u"
	   " %%*u %%*u %%*u %%*u %%*u %%*u %%*u %%*u %%*u %%*u %%*u %%*u %%*u"
	   " %%*u %%u\\n", MAX_COMM_LEN);

   fscanf(fp, format, comm,
	  &(pst->minflt), &(pst->cminflt), &(pst->majflt), &(pst->cmajflt),
	  &(pst->utime),  &(pst->stime), &(pst->cutime), &(pst->cstime),
	  &(pst->processor));

   fclose(fp);
   
   strncpy(pst->comm, comm, MAX_COMM_LEN);
   pst->comm[MAX_COMM_LEN - 1] = '\0';
   
   /* Remove trailing ')' */
   len = strlen(pst->comm);
   if (len && (pst->comm[len - 1] == ')'))
      pst->comm[len - 1] = '\0';
   
   pst->pid = pid;
   return 0;
}


/*
 ***************************************************************************
 * Read stats from /proc/<pid>/status
 ***************************************************************************
 */
int read_proc_pid_status(unsigned long pid, unsigned int flags,
			 struct pid_stats *pst)
{
   FILE *fp;
   char filename[64], line[256];

   sprintf(filename, PID_STATUS, pid);
   if ((fp = fopen(filename, "r")) == NULL)
      /* No such process */
      return 1;

   while (fgets(line, 256, fp) != NULL) {

      if (!strncmp(line, "VmSize:", 7))
	 sscanf(line + 8, "%lu", &(pst->vsz));
      else if (!strncmp(line, "VmRSS:", 6))
	 sscanf(line + 7, "%lu", &(pst->rss));
   }

   fclose(fp);

   pst->pid = pid;
   return 0;
}


/*
 ***************************************************************************
 * Read various stats for given PID
 ***************************************************************************
 */
int read_pid_stats(unsigned long pid, unsigned int flags,
		   struct pid_stats *pst)
{
   if (read_proc_pid_stat(pid, flags, pst))
      return 1;

   if (DISPLAY_MEM(flags))
      return (read_proc_pid_status(pid, flags, pst));
   
   return 0;
}


/*
 ***************************************************************************
 * Allocate and init structures according to system state
 ***************************************************************************
 */
void pid_sys_init(int *flags)
{
   /* Init stat common counters */
   init_stats();
   
   /* Count nb of proc */
   cpu_nr = get_cpu_nr(~0);

   if (DISPLAY_ALL_PID(*flags)) {
      /* Count PIDs and allocate structures */
      pid_nr = count_pid() + NR_PID_PREALLOC;
      salloc_pid(pid_nr);
   }
}


/*
 ***************************************************************************
 * Read various stats
 ***************************************************************************
 */
void read_stats(int flags, int curr)
{
   DIR *dir;
   struct dirent *drp;
   unsigned int p, q;
   struct pid_stats *pst0, *psti;

   /* Read CPU statistics */
   read_proc_stat(curr);
      
   if (DISPLAY_ALL_PID(flags)) {

      /* Open /proc directory */
      if ((dir = opendir(PROC)) == NULL) {
	 perror("opendir");
	 exit(4);
      }

      for (p = 0; p < pid_nr; p++) {

	 /* Get directory entries */
	 while ((drp = readdir(dir)) != NULL) {
	    if (isdigit(drp->d_name[0]))
	       break;
	 }
	 if (drp) {
	    psti = st_pid_list[curr] + p;
	    if (read_pid_stats(atol(drp->d_name), flags, psti))
	       /* Process has terminated */
	       psti->pid = 0;
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

   else if (DISPLAY_PID(flags)) {
   
      /* Read stats for each PID in the list */
      for (p = 0; p < pid_nr; p++) {
	 
	 pst0 = st_pid_list[2] + p;
	 psti = st_pid_list[curr] + p;
	 
	 if (pst0->pid) {
	    /* PID should still exist. So read its stats */
	    if (read_pid_stats(pst0->pid, flags, psti))
	       /* PID has terminated */
	       pst0->pid = 0;
	 }
      }
   }
   /* else unknonw command */
}


/*
 ***************************************************************************
 * Get current PID to display
 * Return value: 0 if PID no longer exists
 * 		-1 if PID should not be displayed
 * 		 1 if PID can be displayed
 * First, check that PID exists. *Then* check that it's an active process
 * and/or that the string is found in command name.
 ***************************************************************************
 */
int get_pid_to_display(int prev, int curr, int flags, int p, int activity,
		       struct pid_stats **psti, struct pid_stats **pstj)
{
   int q;
   struct pid_stats *pst0;
   
   *psti = st_pid_list[curr] + p;
   
   if (DISPLAY_ALL_PID(flags)) {
     
      if (!(*psti)->pid)
	 return 0;	/* Next PID */

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
    
      if (DISPLAY_ACTIVE_PID(flags)) {
	 /* Check that it's an "active" process */
	 if (DISPLAY_CPU(activity)) {
	    if (((*psti)->utime == (*pstj)->utime) &&
		((*psti)->stime == (*pstj)->stime))
	       return -1;	/* Inactive process */
	 }
	 else if (DISPLAY_MEM(activity)) {
	    if (((*psti)->minflt == (*pstj)->minflt) &&
		((*psti)->majflt == (*pstj)->majflt) &&
		((*psti)->vsz == (*pstj)->vsz) &&
		((*psti)->rss == (*pstj)->rss))
	       return -1;
	 }
      }
   }
	 
   else if (DISPLAY_PID(flags)) {

      pst0 = st_pid_list[2] + p;
      if (!pst0->pid)
	 return 0;	/* Next PID */

      *pstj = st_pid_list[prev] + p;
   }
   
   if (COMMAND_STRING(flags) && !(strstr((*psti)->comm, commstr)))
      return -1;	/* String not found in command name */

   return 1;
}


/*
 ***************************************************************************
 * Display statistics
 ***************************************************************************
 */
int write_stats_core(int prev, int curr, int flags, int dis, int disp_avg,
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
   
   if (DISPLAY_CPU(flags)) {
      if (dis)
	 printf("\n%-11s       PID   %%user %%system    %%CPU   CPU  Command\n",
		prev_string);
      
      for (p = 0; p < pid_nr; p++) {
	 
	 if (get_pid_to_display(prev, curr, flags, p, P_D_CPU,
				&psti, &pstj) <= 0)
	    continue;
	 
	 printf("%-11s %9ld", curr_string, psti->pid);
	 printf(" %7.2f %7.2f %7.2f",
		SP_VALUE(pstj->utime, psti->utime, itv),
		SP_VALUE(pstj->stime, psti->stime, itv),
		IRIX_MODE_OFF(flags) ?
		SP_VALUE(pstj->utime + pstj->stime,
			 psti->utime + psti->stime, g_itv) :
		SP_VALUE(pstj->utime + pstj->stime,
			 psti->utime + psti->stime, itv));
	 if (!disp_avg)
	    printf("   %3d", psti->processor);
	 else
	    printf("     -");
	 printf("  %s\n", psti->comm);
	 again = 1;
      }
   }
   
   if (DISPLAY_MEM(flags)) {
      if (dis)
	 printf("\n%-11s       PID  minflt/s  majflt/s     VSZ    RSS   %%MEM  Command\n",
		prev_string);
      
      for (p = 0; p < pid_nr; p++) {
	 
	 if ((rc = get_pid_to_display(prev, curr, flags, p, P_D_MEM,
				      &psti, &pstj)) == 0)
	    /* PID no longer exists */
	    continue;
	 
	 /* This will be used to compute average */
	 if (!disp_avg) {
	    psti->total_vsz = pstj->total_vsz + psti->vsz;
	    psti->total_rss = pstj->total_rss + psti->rss;
	    psti->asum_count = pstj->asum_count + 1;
	 }
	 
	 if (rc < 0)
	    /* PID should not be displayed */
	    continue;

	 printf("%-11s %9ld", curr_string, psti->pid);
	 printf(" %9.2f %9.2f ",
		S_VALUE(pstj->minflt, psti->minflt, itv),
		S_VALUE(pstj->majflt, psti->majflt, itv));
	 if (disp_avg) {
	    printf("%7.0f %6.0f %6.2f",
		   (double) psti->total_vsz / psti->asum_count,
		   (double) psti->total_rss / psti->asum_count,
		   tlmkb ?
		   SP_VALUE(0, psti->total_rss / psti->asum_count, tlmkb)
		   : 0.0);
	 }
	 else {
	    printf("%7lu %6lu %6.2f",
		   psti->vsz,
		   psti->rss,
		   tlmkb ? SP_VALUE(0, psti->rss, tlmkb) : 0.0);
	 }
	 printf("  %s\n", psti->comm);
	 again = 1;
      }
   }
   
   if (DISPLAY_ALL_PID(flags))
      again = 1;
   
   return again;
}


/*
 ***************************************************************************
 * Print statistics average
 ***************************************************************************
 */
void write_stats_avg(int curr, int dis, int flags)
{
   char string[16];

   strcpy(string, _("Average:"));
   write_stats_core(2, curr, flags, dis, TRUE, string, string);
}


/*
 ***************************************************************************
 * Print statistics
 ***************************************************************************
 */
int write_stats(int curr, int dis, int flags)
{
   char cur_time[2][16];

   /* Get previous timestamp */
   strftime(cur_time[!curr], 16, "%X", &(ps_tstamp[!curr]));

   /* Get current timestamp */
   strftime(cur_time[curr], 16, "%X", &(ps_tstamp[curr]));

   return (write_stats_core(!curr, curr, flags, dis, FALSE,
			    cur_time[!curr], cur_time[curr]));
}


/*
 ***************************************************************************
 * Main loop: Read and dispalay PID stats
 ***************************************************************************
 */
void rw_pidstat_loop(int dis_hdr, int flags, long int count,
		     unsigned long lines, int rows)
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
   read_stats(flags, 0);

   if (DISPLAY_MEM(flags))
      /* Get total memory */
      read_proc_meminfo();
   
   if (!interval) {
      /* Display since boot time */
      ps_tstamp[1] = ps_tstamp[0];
      memset(st_pid_list[1], 0, PID_STATS_SIZE * pid_nr);
      write_stats(0, DISP_HDR, flags);
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
      read_stats(flags, curr);

      if (!dis_hdr) {
	 dis = lines / rows;
	 if (dis)
	    lines %= rows;
	 lines++;
      }

      /* Print results */
      again = write_stats(curr, dis, flags);
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
   write_stats_avg(curr, dis_hdr, flags);
}


/*
 ***************************************************************************
 * Main entry to the pidstat program
 ***************************************************************************
 */
int main(int argc, char **argv)
{
   int flags = 0;
   int opt = 1, dis_hdr = -1;
   int i;
   long count = 0;
   unsigned long pid;
   struct utsname header;
   unsigned long lines = 0;
   int rows = 23;

#ifdef USE_NLS
   /* Init National Language Support */
   init_nls();
#endif

   /* Get HZ */
   get_HZ();

   /* Allocate structures for device list */
   if (argc > 1)
      salloc_pid(argc - 1);

   /* Process args... */
   while (opt < argc) {

      if (!strcmp(argv[opt], "-p")) {
	 flags |= P_D_PID;
	 if (argv[++opt]) {
	    if (!strcmp(argv[opt], K_ALL)) {
	       flags |= P_D_ALL_PID;
	       opt++;
	       continue;	/* Next option */
	    }
	    else if (!strcmp(argv[opt], K_SELF))
	       pid = getpid();
	    else {
	       if (strspn(argv[opt], DIGITS) != strlen(argv[opt]))
		  usage(argv[0]);
	       pid = atol(argv[opt]);
	       if (pid < 1)
		  usage(argv[0]);
	    }
	    update_pid_list(&pid_nr, pid);
	    opt++;
	 }
	 else
	    usage(argv[0]);
      }
      
      else if (!strcmp(argv[opt], "-C")) {
	 if (argv[++opt]) {
	    strncpy(commstr, argv[opt++], MAX_COMM_LEN);
	    commstr[MAX_COMM_LEN - 1] = '\0';
	    flags |= P_F_COMMSTR;
	    if (!strlen(commstr))
	       usage(argv[0]);
	 }
	 else
	    usage(argv[0]);
      }
	    
      else if (!strncmp(argv[opt], "-", 1)) {
	 for (i = 1; *(argv[opt] + i); i++) {

	    switch (*(argv[opt] + i)) {

	     case 'I':
	       flags |= P_F_IRIX_MODE;	/* IRIX mode off */
	       break;
	       
	     case 'r':
	       flags |= P_D_MEM;	/* Display memory usage */
	       dis_hdr++;
	       break;

	     case 'u':
	       flags |= P_D_CPU;	/* Display cpu usage */
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

   /* Display CPU usage by default */
   if (!DISPLAY_CPU(flags) && !DISPLAY_MEM(flags))
      flags |= P_D_CPU;

   if (!DISPLAY_PID(flags))
      flags |= P_D_ACTIVE_PID + P_D_PID + P_D_ALL_PID;
      
   /* Init structures */
   pid_sys_init(&flags);

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
   rw_pidstat_loop(dis_hdr, flags, count, lines, rows);
   
   return 0;
}
