/*
 * mpstat: per-processor statistics
 * (C) 2000-2007 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <errno.h>
#include <ctype.h>
#include <sys/utsname.h>

#include "mpstat.h"
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
struct mp_stats *st_mp_cpu[3];
/* NOTE: Use array of _char_ for bitmaps to avoid endianness problems...*/
unsigned char *cpu_bitmap;	/* Bit 0: Global; Bit 1: 1st proc; etc. */
struct tm mp_tstamp[3];
long interval = -1, count = 0;

/* Nb of processors on the machine */
int cpu_nr = 0;

/*
 ***************************************************************************
 * Print usage and exit
 ***************************************************************************
 */
void usage(char *progname)
{
   fprintf(stderr, _("Usage: %s [ options... ] [ <interval> [ <count> ] ]\n"
		   "Options are:\n"
		   "[ -P { <cpu> | ALL } ] [ -V ]\n"),
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
 * Allocate mp_stats structures and cpu bitmap
 ***************************************************************************
 */
void salloc_mp_cpu(int nr_cpus)
{
   int i;

   for (i = 0; i < 3; i++) {
      if ((st_mp_cpu[i] = (struct mp_stats *) malloc(MP_STATS_SIZE * nr_cpus)) == NULL) {
	 perror("malloc");
	 exit(4);
      }

      memset(st_mp_cpu[i], 0, MP_STATS_SIZE * nr_cpus);
   }

   if ((cpu_bitmap = (unsigned char *) malloc((nr_cpus >> 3) + 1)) == NULL) {
      perror("malloc");
      exit(4);
   }

   memset(cpu_bitmap, 0, (nr_cpus >> 3) + 1);
}


/*
 ***************************************************************************
 * Recalculate interval based on each CPU's tick count
 ***************************************************************************
 */
unsigned long long mget_per_cpu_interval(struct mp_stats *st_mp_cpu_i,
					 struct mp_stats *st_mp_cpu_j)
{
   return ((st_mp_cpu_i->cpu_user + st_mp_cpu_i->cpu_nice +
	    st_mp_cpu_i->cpu_system + st_mp_cpu_i->cpu_iowait +
	    st_mp_cpu_i->cpu_hardirq + st_mp_cpu_i->cpu_softirq +
	    st_mp_cpu_i->cpu_steal + st_mp_cpu_i->cpu_idle) -
	   (st_mp_cpu_j->cpu_user + st_mp_cpu_j->cpu_nice +
	    st_mp_cpu_j->cpu_system + st_mp_cpu_j->cpu_iowait +
	    st_mp_cpu_j->cpu_hardirq + st_mp_cpu_j->cpu_softirq +
	    st_mp_cpu_j->cpu_steal + st_mp_cpu_j->cpu_idle));
}


/*
 ***************************************************************************
 * Core function used to display statistics
 ***************************************************************************
 */
void write_stats_core(int prev, int curr, int dis,
		      char *prev_string, char *curr_string)
{
   struct mp_stats
      *smci = st_mp_cpu[curr] + 1,
      *smcj = st_mp_cpu[prev] + 1;
   unsigned long long itv, pc_itv;
   int cpu;

   /* Test stdout */
   TEST_STDOUT(STDOUT_FILENO);

   /* Compute time interval */
   itv = get_interval(uptime[prev], uptime[curr]);

   /* Print stats */
   if (dis)
      printf("\n%-11s  CPU   %%user   %%nice    %%sys %%iowait    %%irq   "
	     "%%soft  %%steal   %%idle    intr/s\n",
	     prev_string);

   /* Check if we want global stats among all proc */
   if (*cpu_bitmap & 1) {

      printf("%-11s  all", curr_string);

      printf("  %6.2f  %6.2f  %6.2f  %6.2f  %6.2f  %6.2f  %6.2f  %6.2f",
	     ll_sp_value(st_mp_cpu[prev]->cpu_user,    st_mp_cpu[curr]->cpu_user,    itv),
 	     ll_sp_value(st_mp_cpu[prev]->cpu_nice,    st_mp_cpu[curr]->cpu_nice,    itv),
	     ll_sp_value(st_mp_cpu[prev]->cpu_system,  st_mp_cpu[curr]->cpu_system,  itv),
	     ll_sp_value(st_mp_cpu[prev]->cpu_iowait,  st_mp_cpu[curr]->cpu_iowait,  itv),
	     ll_sp_value(st_mp_cpu[prev]->cpu_hardirq, st_mp_cpu[curr]->cpu_hardirq, itv),
	     ll_sp_value(st_mp_cpu[prev]->cpu_softirq, st_mp_cpu[curr]->cpu_softirq, itv),
	     ll_sp_value(st_mp_cpu[prev]->cpu_steal,   st_mp_cpu[curr]->cpu_steal  , itv),
	     (st_mp_cpu[curr]->cpu_idle < st_mp_cpu[prev]->cpu_idle) ?
	     0.0 :	/* Handle buggy kernels */
	     ll_sp_value(st_mp_cpu[prev]->cpu_idle, st_mp_cpu[curr]->cpu_idle, itv));
   }

   /* Reduce interval value to one processor */
   if (cpu_nr > 1)
      itv = get_interval(uptime0[prev], uptime0[curr]);

   if (*cpu_bitmap & 1) {
       printf(" %9.2f\n",
	  ll_s_value(st_mp_cpu[prev]->irq, st_mp_cpu[curr]->irq, itv));
   }

   for (cpu = 1; cpu <= cpu_nr; cpu++, smci++, smcj++) {

      /* Check if we want stats about this proc */
      if (!(*(cpu_bitmap + (cpu >> 3)) & (1 << (cpu & 0x07))))
	 continue;

      printf("%-11s %4d", curr_string, cpu - 1);

      /* Recalculate itv for current proc */
      pc_itv = mget_per_cpu_interval(smci, smcj);

      if (!pc_itv)
	 /* Current CPU is offline */
	 printf("    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00      0.00\n");
      else {
	 printf("  %6.2f  %6.2f  %6.2f  %6.2f  %6.2f  %6.2f  %6.2f  %6.2f %9.2f\n",
		ll_sp_value(smcj->cpu_user,    smci->cpu_user,    pc_itv),
		ll_sp_value(smcj->cpu_nice,    smci->cpu_nice,    pc_itv),
		ll_sp_value(smcj->cpu_system,  smci->cpu_system,  pc_itv),
		ll_sp_value(smcj->cpu_iowait,  smci->cpu_iowait,  pc_itv),
		ll_sp_value(smcj->cpu_hardirq, smci->cpu_hardirq, pc_itv),
		ll_sp_value(smcj->cpu_softirq, smci->cpu_softirq, pc_itv),
		ll_sp_value(smcj->cpu_steal,   smci->cpu_steal,   pc_itv),
		(smci->cpu_idle < smcj->cpu_idle) ?
		0.0 :
		ll_sp_value(smcj->cpu_idle, smci->cpu_idle, pc_itv),
		ll_s_value(smcj->irq, smci->irq, itv));
      }
   }
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
   write_stats_core(2, curr, dis, string, string);
}


/*
 ***************************************************************************
 * Print statistics
 ***************************************************************************
 */
void write_stats(int curr, int dis)
{
   char cur_time[2][16];

   /* Get previous timestamp */
   strftime(cur_time[!curr], 16, "%X", &(mp_tstamp[!curr]));

   /* Get current timestamp */
   strftime(cur_time[curr], 16, "%X", &(mp_tstamp[curr]));

   write_stats_core(!curr, curr, dis, cur_time[!curr], cur_time[curr]);
}


/*
 ***************************************************************************
 * Read stats from /proc/stat
 ***************************************************************************
 */
void read_proc_stat(int curr)
{
   FILE *fp;
   struct mp_stats *st_mp_cpu_i;
   static char line[80];
   unsigned long long cc_user, cc_nice, cc_system, cc_hardirq, cc_softirq;
   unsigned long long cc_idle, cc_iowait, cc_steal;
   int proc_nb;

   if ((fp = fopen(STAT, "r")) == NULL) {
      fprintf(stderr, _("Cannot open %s: %s\n"), STAT, strerror(errno));
      exit(2);
   }

   while (fgets(line, 80, fp) != NULL) {

      if (!strncmp(line, "cpu ", 4)) {
	 /*
	  * Read the number of jiffies spent in the different modes
	  * among all proc. CPU usage is not reduced to one
	  * processor to avoid rounding problems.
	  */
	 st_mp_cpu[curr]->cpu_iowait = 0;	/* For pre 2.5 kernels */
	 cc_hardirq = cc_softirq = cc_steal = 0;
	 /* CPU counters became unsigned long long with kernel 2.6.5 */
	 sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu %llu",
		&(st_mp_cpu[curr]->cpu_user),
		&(st_mp_cpu[curr]->cpu_nice),
		&(st_mp_cpu[curr]->cpu_system),
		&(st_mp_cpu[curr]->cpu_idle),
		&(st_mp_cpu[curr]->cpu_iowait),
		&(st_mp_cpu[curr]->cpu_hardirq),
		&(st_mp_cpu[curr]->cpu_softirq),
		&(st_mp_cpu[curr]->cpu_steal));

	 /*
	  * Compute the uptime of the system in jiffies (1/100ths of a second
	  * if HZ=100).
	  * Machine uptime is multiplied by the number of processors here.
	  */
	 uptime[curr] = st_mp_cpu[curr]->cpu_user +
	                st_mp_cpu[curr]->cpu_nice +
	                st_mp_cpu[curr]->cpu_system +
	                st_mp_cpu[curr]->cpu_idle +
	                st_mp_cpu[curr]->cpu_iowait +
	    		st_mp_cpu[curr]->cpu_hardirq +
	    		st_mp_cpu[curr]->cpu_softirq +
	    		st_mp_cpu[curr]->cpu_steal;
      }

      else if (!strncmp(line, "cpu", 3)) {
	 /*
	  * Read the number of jiffies spent in the different modes
	  * (user, nice, etc.) for current proc.
	  * This is done only on SMP machines.
	  */
	 cc_iowait = cc_hardirq = cc_softirq = cc_steal = 0;
	 sscanf(line + 3, "%d %llu %llu %llu %llu %llu %llu %llu %llu",
		&proc_nb,
		&cc_user, &cc_nice, &cc_system, &cc_idle, &cc_iowait,
		&cc_hardirq, &cc_softirq, &cc_steal);

	 if (proc_nb < cpu_nr) {
	    st_mp_cpu_i = st_mp_cpu[curr] + proc_nb + 1;
	    st_mp_cpu_i->cpu_user    = cc_user;
	    st_mp_cpu_i->cpu_nice    = cc_nice;
	    st_mp_cpu_i->cpu_system  = cc_system;
	    st_mp_cpu_i->cpu_idle    = cc_idle;
	    st_mp_cpu_i->cpu_iowait  = cc_iowait;
	    st_mp_cpu_i->cpu_hardirq = cc_hardirq;
	    st_mp_cpu_i->cpu_softirq = cc_softirq;
	    st_mp_cpu_i->cpu_steal   = cc_steal;
	 }
	 /* else additional CPUs have been dynamically registered in /proc/stat */
	
	 if (!proc_nb && !uptime0[curr])
	    /*
	     * Compute uptime reduced for one proc using proc#0.
	     * Done only if /proc/uptime was unavailable.
	     */
	    uptime0[curr] = cc_user + cc_nice + cc_system + cc_idle +
	                    cc_iowait + cc_hardirq + cc_softirq + cc_steal;
      }

      else if (!strncmp(line, "intr ", 5))
	 /*
	  * Read total number of interrupts received since system boot.
	  * Interrupts counter became unsigned long long with kernel 2.6.5.
	  */
	 sscanf(line + 5, "%llu", &(st_mp_cpu[curr]->irq));
   }

   fclose(fp);
}


/*
 ***************************************************************************
 * Read stats from /proc/interrupts
 ***************************************************************************
 */
void read_interrupts_stat(int curr)
{
   FILE *fp;
   struct mp_stats *st_mp_cpu_i;
   static char *line = NULL;
   unsigned long irq = 0;
   unsigned int cpu;
   char *cp, *next;

   for (cpu = 0; cpu < cpu_nr; cpu++) {
      st_mp_cpu_i = st_mp_cpu[curr] + cpu + 1;
      st_mp_cpu_i->irq = 0;
   }

   if ((fp = fopen(INTERRUPTS, "r")) != NULL) {

      if (!line) {
	 if ((line = (char *) malloc(INTERRUPTS_LINE + 11 * cpu_nr)) == NULL) {
	    perror("malloc");
	    exit(4);
	 }
      }

      while (fgets(line, INTERRUPTS_LINE + 11 * cpu_nr, fp) != NULL) {

	 if (isdigit(line[2])) {
	
	    /* Skip over "<irq>:" */
	    if ((cp = strchr(line, ':')) == NULL)
	       continue;
	    cp++;
	
	    for (cpu = 0; cpu < cpu_nr; cpu++) {
	       st_mp_cpu_i = st_mp_cpu[curr] + cpu + 1;
	       irq = strtol(cp, &next, 10);
	       st_mp_cpu_i->irq += irq;
	       cp = next;
	    }
	 }
      }

      fclose(fp);
   }
}


/*
 ***************************************************************************
 * Main loop: read stats from the relevant sources,
 * and display them.
 ***************************************************************************
 */
void rw_mpstat_loop(int dis_hdr, unsigned long lines, int rows)
{
   struct mp_stats *st_mp_cpu_i, *st_mp_cpu_j;
   int cpu;
   int curr = 1, dis = 1;

   /* Read stats */
   if (cpu_nr > 1) {
      /*
       * Init uptime0. So if /proc/uptime cannot fill it,
       * this will be done by /proc/stat.
       */
      uptime0[0] = 0;
      readp_uptime(&(uptime0[0]));
   }
   read_proc_stat(0);
   read_interrupts_stat(0);

   if (!interval) {
      /* Display since boot time */
      mp_tstamp[1] = mp_tstamp[0];
      memset(st_mp_cpu[1], 0, MP_STATS_SIZE * (cpu_nr + 1));
      write_stats(0, DISP_HDR);
      exit(0);
   }

   /* Set a handler for SIGALRM */
   alarm_handler(0);

   /* Save the first stats collected. Will be used to compute the average */
   mp_tstamp[2] = mp_tstamp[0];
   uptime[2] = uptime[0];
   uptime0[2] = uptime0[0];
   memcpy(st_mp_cpu[2], st_mp_cpu[0], MP_STATS_SIZE * (cpu_nr + 1));

   pause();

   do {
      /*
       * Resetting the structure not needed since every fields will be set.
       * Exceptions are per-CPU structures: some of them may not be filled
       * if corresponding processor is disabled (offline).
       */
      for (cpu = 1; cpu <= cpu_nr; cpu++) {
	 st_mp_cpu_i = st_mp_cpu[curr] + cpu;
	 st_mp_cpu_j = st_mp_cpu[!curr] + cpu;
	 *st_mp_cpu_i = *st_mp_cpu_j;
      }

      /* Get time */
      get_localtime(&(mp_tstamp[curr]));

      /* Read stats */
      if (cpu_nr > 1) {
	 uptime0[curr] = 0;
	 readp_uptime(&(uptime0[curr]));
      }
      read_proc_stat(curr);
      read_interrupts_stat(curr);

      /* Write stats */
      if (!dis_hdr) {
	 dis = lines / rows;
	 if (dis)
	    lines %= rows;
	 lines++;
      }
      write_stats(curr, dis);

      /* Flush data */
      fflush(stdout);

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
 * Main entry to the program
 ***************************************************************************
 */
int main(int argc, char **argv)
{
   int opt = 0, i;
   struct utsname header;
   int dis_hdr = -1, opt_used = 0;
   unsigned long lines = 0;
   int rows = 23;

#ifdef USE_NLS
   /* Init National Language Support */
   init_nls();
#endif

   /* Get HZ */
   get_HZ();

   /* How many processors on this machine ? */
   cpu_nr = get_cpu_nr(~0);

   /*
    * cpu_nr: a value of 2 means there are 2 processors (0 and 1).
    * In this case, we have to allocate 3 structures: global, proc0 and proc1.
    */
   salloc_mp_cpu(cpu_nr + 1);

   while (++opt < argc) {

      if (!strcmp(argv[opt], "-V"))
	 print_version();

      else if (!strcmp(argv[opt], "-P")) {
	 /* '-P ALL' can be used on UP machines */
	 if (argv[++opt]) {
	    opt_used = 1;
	    dis_hdr++;
	    if (!strcmp(argv[opt], K_ALL)) {
	       if (cpu_nr)
		  dis_hdr = 9;
	       /*
		* Set bit for every processor.
		* Also indicate to display stats for CPU 'all'.
		*/
	       memset(cpu_bitmap, 0xff, ((cpu_nr + 1) >> 3) + 1);
	    }
	    else {
	       if (strspn(argv[opt], DIGITS) != strlen(argv[opt]))
		  usage(argv[0]);
	       i = atoi(argv[opt]);	/* Get cpu number */
	       if (i >= cpu_nr) {
		  fprintf(stderr, _("Not that many processors!\n"));
		  exit(1);
	       }
	       i++;
	       *(cpu_bitmap + (i >> 3)) |= 1 << (i & 0x07);
	    }
	 }
	 else
	    usage(argv[0]);
      }

      else if (interval < 0) {		/* Get interval */
	 if (strspn(argv[opt], DIGITS) != strlen(argv[opt]))
	    usage(argv[0]);
	 interval = atol(argv[opt]);
	 if (interval < 0)
	    usage(argv[0]);
	 count = -1;
      }

      else if (count <= 0) {		/* Get count value */
	 if ((strspn(argv[opt], DIGITS) != strlen(argv[opt])) ||
	     !interval)
	    usage(argv[0]);
	 count = atol(argv[opt]);
	 if (count < 1)
	   usage(argv[0]);
      }

      else
	 usage(argv[0]);
   }

   if (!opt_used)
      /* Option -P not used: set bit 0 (global stats among all proc) */
      *cpu_bitmap = 1;
   if (dis_hdr < 0)
      dis_hdr = 0;
   if (!dis_hdr) {
      /* Get window size */
      rows = get_win_height();
      lines = rows;
   }
   if (interval < 0)
      /* Interval not set => display stats since boot time */
      interval = 0;

   /* Get time */
   get_localtime(&(mp_tstamp[0]));

   /* Get system name, release number and hostname */
   uname(&header);
   print_gal_header(&(mp_tstamp[0]), header.sysname, header.release,
		    header.nodename);

   /* Main loop */
   rw_mpstat_loop(dis_hdr, lines, rows);

   return 0;
}
