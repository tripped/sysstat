/*
 * sar: report system activity
 * (C) 1999-2002 by Sebastien GODARD <sebastien.godard@wanadoo.fr>
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
 * 675 Mass Ave, Cambridge, MA 02139, USA.                                 *
 ***************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/param.h>	/* for HZ */

#include "version.h"
#include "sa.h"
#include "common.h"
#include "sapath.h"


#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif


long interval = 0, count = 0;
unsigned int sar_actflag = 0;
unsigned int flags = 0;
unsigned int irq_bitmap[(NR_IRQS / 32) + 1];
unsigned int cpu_bitmap[(NR_CPUS / 32) + 1];
int kb_shift = 0;

struct stats_sum asum;
struct file_hdr file_hdr;
struct file_stats file_stats[DIM];
struct stats_one_cpu *st_cpu[DIM];
struct stats_serial *st_serial[DIM];
struct stats_irq_cpu *st_irq_cpu[DIM];
struct stats_net_dev *st_net_dev[DIM];
struct disk_stats *st_disk[DIM];

/* Array members of common types are always packed */
unsigned int interrupts[DIM][NR_IRQS];
/* Structures are aligned but also padded. Thus array members are packed */
struct pid_stats *pid_stats[DIM][MAX_PID_NR];

struct tm loc_time;
struct tstamp tm_start, tm_end;	/* Used for -s and -e options */
short dis_hdr = -1;
int pid_nr = 0;
char *args[MAX_ARGV_NR];


/*
 * Print usage and exit
 */
void usage(char *progname)
{
   fprintf(stderr, _("sysstat version %s\n"
		   "(C) S. Godard <sebastien.godard@wanadoo.fr>\n"
	           "Usage: %s [ options... ]\n"
	           "Options are:\n"
	           "[ -A ] [ -b ] [ -B ] [ -c ] [ -d ] [ -H ] [ -h ] [ -i <interval> ] [ -q ]\n"
		   "[ -r ] [ -R ] [ -t ] [ -u ] [ -v ] [ -V ] [ -w ] [ -W ] [ -y ]\n"
		   "[ -I { <irq> | SUM | PROC | ALL | XALL } ] [ -U { <cpu> | ALL } ]\n"
		   "[ -n { DEV | EDEV | SOCK | FULL } ]\n"
		   "[ -x { <pid> | SELF | SUM | ALL } ] [ -X { <pid> | SELF | ALL } ]\n"
	           "[ -o [ <filename> ] | -f [ <filename> ] ]\n"
		   "[ -s [ <hh:mm:ss> ] ] [ -e [ <hh:mm:ss> ] ]\n"
	           "[ <interval> [ <count> ] ]\n"),
	   VERSION, progname);
   exit(1);
}


/*
 * Init irqs array
 */
void init_irq_bitmap(unsigned int value)
{
   register int i;

   for (i = 0; i <= NR_IRQS >> 5; i++)
     irq_bitmap[i] = value;
}


/*
 * Init CPUs array
 */
void init_cpu_bitmap(unsigned int value)
{
   register int i;

   for (i = 0; i <= NR_CPUS >> 5; i++)
     cpu_bitmap[i] = value;
}


/*
 * Init stats_sum structure
 */
void init_stats_sum(void)
{
   memset(&asum, 0, STATS_SUM_SIZE);
}


/*
 * Init stats structures
 */
void init_stats(void)
{
   int i;

   for (i = 0; i < DIM; i++) {
      memset(&file_stats[i], 0, FILE_STATS_SIZE);
      memset(interrupts[i], 0, STATS_ONE_IRQ_SIZE);
   }
   init_stats_sum();
}


/*
 * Allocate memory for sadc args
 */
void salloc(int i, char *ltemp)
{
   if ((args[i] = (char *) malloc(strlen(ltemp) + 1)) == NULL) {
      perror("malloc");
      exit(4);
   }
   strcpy(args[i], ltemp);
}


/*
 * Allocate stats_one_cpu structures
 * (only on SMP machines)
 */
void salloc_cpu(int nr_cpu)
{
   int i;

   for (i = 0; i < DIM; i++) {
      if ((st_cpu[i] = (struct stats_one_cpu *) malloc(STATS_ONE_CPU_SIZE * nr_cpu)) == NULL) {
	 perror("malloc");
	 exit(4);
      }

      memset(st_cpu[i], 0, STATS_ONE_CPU_SIZE * nr_cpu);
   }
}


/*
 * Allocate stats_serial structures
 */
void salloc_serial(int nr_serial)
{
   int i;

   for (i = 0; i < DIM; i++) {
      if ((st_serial[i] = (struct stats_serial *) malloc(STATS_SERIAL_SIZE * nr_serial)) == NULL) {
	 perror("malloc");
	 exit(4);
      }

      memset(st_serial[i], 0, STATS_SERIAL_SIZE * nr_serial);
   }
}


/*
 * Allocate stats_irq_cpu structures
 */
void salloc_irqcpu(int nr_cpus, int nr_irqcpu)
{
   int i;

   for (i = 0; i < DIM; i++) {
      if ((st_irq_cpu[i] = (struct stats_irq_cpu *) malloc(STATS_IRQ_CPU_SIZE * nr_cpus * nr_irqcpu)) == NULL) {
	 perror("malloc");
	 exit(4);
      }

      memset(st_irq_cpu[i], 0, STATS_IRQ_CPU_SIZE * nr_cpus * nr_irqcpu);
   }
}


/*
 * Allocate stats_net_dev structures
 */
void salloc_net_dev(int nr_iface)
{
   int i;

   for (i = 0; i < DIM; i++) {
      if ((st_net_dev[i] = (struct stats_net_dev *) malloc(STATS_NET_DEV_SIZE * nr_iface)) == NULL) {
	 perror("malloc");
	 exit(4);
      }

      memset(st_net_dev[i], 0, STATS_NET_DEV_SIZE * nr_iface);
   }
}


/*
 * Allocate disk_stats structures
 */
void salloc_disk(int nr_disk)
{
   int i;

   for (i = 0; i < DIM; i++) {
      if ((st_disk[i] = (struct disk_stats *) malloc(DISK_STATS_SIZE * nr_disk)) == NULL) {
	 perror("malloc");
	 exit(4);
      }

      memset(st_disk[i], 0, DISK_STATS_SIZE * nr_disk);
   }
}


/*
 * Allocate pid_stats structures
 */
void salloc_pid(int nr_pid)
{
   int i, pid;

   for (i = 0; i < DIM; i++) {
      if ((pid_stats[i][0] = (struct pid_stats *) malloc(PID_STATS_SIZE * nr_pid)) == NULL) {
	 perror("malloc");
	 exit(4);
      }

      memset(pid_stats[i][0], 0, PID_STATS_SIZE * nr_pid);

      for (pid = 1; pid < nr_pid; pid++)
	 /* Structures are aligned but also padded. Thus array members are packed */
	 pid_stats[i][pid] = pid_stats[i][0] + pid;	/* Assume nr_pids <= MAX_PID_NR */
   }
}


void decode_time_stamp(char time_stamp[], struct tstamp *tse)
{
   time_stamp[2] = time_stamp[5] = '\0';
   tse->tm_sec  = atoi(&(time_stamp[6]));
   tse->tm_min  = atoi(&(time_stamp[3]));
   tse->tm_hour = atoi(time_stamp);
   tse->use = 1;
}


/*
 * Compare two time stamps
 */
int datecmp(struct tm *loc_time, struct tstamp *tse)
{
   if (loc_time->tm_hour == tse->tm_hour) {
      if (loc_time->tm_min == tse->tm_min)
	return (loc_time->tm_sec - tse->tm_sec);
      else
	return (loc_time->tm_min - tse->tm_min);
   }
   else
     return (loc_time->tm_hour - tse->tm_hour);
}


/*
 * Check if the user has the right to use -U option.
 * Note that he may use this option when reading stats from a file,
 * even if its machine is not an SMP one...
 * This routine is called only if we are *not* reading stats from a file.
 */
void check_smp_option(int proc_used)
{
   unsigned int cpu_max, j = 0;
   int i;

   if (proc_used == 0) {
      fprintf(stderr, _("Not an SMP machine...\n"));
      exit(1);
   }

   cpu_max = 1 << ((proc_used + 1) & 0x1f);

   for (i = (cpu_max >> 5) + 1; i <= NR_CPUS >> 5; i++)
      j |= cpu_bitmap[i];

   if (j || (cpu_bitmap[proc_used >> 5] >= cpu_max)) {
      fprintf(stderr, _("Not that many processors!\n"));
      exit(1);
   }
}


/*
 * Check the use of option -U.
 * Called only if reading stats sent by the data collector.
 */
void prep_smp_option(int proc_used)
{
   int i;

   if (GET_ONE_CPU(sar_actflag)) {
      if (WANT_ALL_PROC(flags))
	 for (i = proc_used + 1; i < ((NR_CPUS / 32) + 1) * 32; i++)
	    /*
	     * Reset every bit for proc > proc_used
	     * (only done when -U ALL entered on the command line)
	     */
	    cpu_bitmap[i >> 5] &= ~(1 << (i & 0x1f));
      check_smp_option(proc_used);
   }
}


int next_slice(int curr, int reset)
{
   unsigned long file_interval;
   static unsigned long last_uptime = 0;
   int min, max, pt1, pt2;

   if (!last_uptime || reset)
      last_uptime = file_stats[2].uptime;

   file_interval = ((file_stats[curr].uptime - last_uptime) / (file_hdr.sa_proc + 1)) / HZ;
   last_uptime = file_stats[curr].uptime;

   /*
    * A few notes about the "algorithm" used here to display selected entries
    * from the system activity file (option -f with -i flag):
    * Let 'Iu' be the interval value given by the user on the command line,
    *     'If' the interval between current and previous line in the system activity file,
    * and 'En' the nth entry (identified by its time stamp) of the file.
    * We choose In = [ En - If/2, En + If/2 [ if If is even,
    *        or In = [ En - If/2, En + If/2 ] if not.
    * En will be displayed if
    *       (Pn * Iu) or (P'n * Iu) belongs to In
    * with  Pn = En / Iu and P'n = En / Iu + 1
    */
   min = (((file_stats[curr].uptime - file_stats[2].uptime) / (file_hdr.sa_proc + 1)) / HZ) - (file_interval / 2);
   max = (((file_stats[curr].uptime - file_stats[2].uptime) / (file_hdr.sa_proc + 1)) / HZ) + (file_interval / 2) +
         (file_interval & 0x1);

   pt1 = (((( file_stats[curr].uptime - file_stats[2].uptime) / (file_hdr.sa_proc + 1)) / HZ) / interval)      * interval;
   pt2 = (((((file_stats[curr].uptime - file_stats[2].uptime) / (file_hdr.sa_proc + 1)) / HZ) / interval) + 1) * interval;

   return (((pt1 >= min) && (pt1 < max)) || ((pt2 >= min) && (pt2 < max)));
}


/*
 * Read data sent by the data collector
 */
int sa_read(void *buffer, int size)
{
   int n;

   while (size) {

      if ((n = read(STDIN_FILENO, buffer, size)) < 0) {
	 perror("read");
	 exit(2);
      }

      if (!n)
	 return 1;	/* EOF */

      size -= n;
      (char *) buffer += n;
   }

   return 0;
}


/*
 * Read data from a sa data file
 */
int sa_fread(int ifd, void *buffer, int size, int mode)
{
   int n;

   if ((n = read(ifd, buffer, size)) < 0) {
      fprintf(stderr, _("Error while reading system activity file: %s\n"), strerror(errno));
      exit(2);
   }

   if (!n && (mode == SOFT_SIZE))
      return 1;	/* EOF */

   if (n < size) {
      fprintf(stderr, _("End of system activity file unexpected\n"));
      exit(2);
   }

   return 0;
}


/*
 * Print report header
 */
void print_report_hdr(void)
{
   struct tm *ltm;

   if (PRINT_ORG_TIME(flags)) {
      /* Get local time */
      get_localtime(&loc_time);

      loc_time.tm_mday = file_hdr.sa_day;
      loc_time.tm_mon  = file_hdr.sa_month;
      loc_time.tm_year = file_hdr.sa_year;
      /*
       * Call mktime() to set DST flag.
       * Has anyone a better way to do it?
       */
      loc_time.tm_hour = loc_time.tm_min = loc_time.tm_sec = 0;
      mktime(&loc_time);
   }
   else {
      ltm = localtime(&file_hdr.sa_ust_time);
      loc_time = *ltm;
   }

   if (!USE_H_OPTION(flags) && !USE_DB_OPTION(flags))
      print_gal_header(&loc_time, file_hdr.sa_sysname, file_hdr.sa_release, file_hdr.sa_nodename);
}


/*
 * Check time and get interval value
 */
int prep_time(int use_tm_start, short curr, unsigned long *itv, unsigned long *itv0)
{
   if (use_tm_start && (datecmp(&loc_time, &tm_start) < 0))
      return 1;

   /* Interval value in jiffies */
   *itv0 = file_stats[curr].uptime - file_stats[!curr].uptime;
   *itv  = *itv0 / (file_hdr.sa_proc + 1);

   if (!(*itv)) {	/* Paranoia checking */
      *itv = 1;
      if (!(*itv0))
	 *itv0 = 1;
   }

   return 0;
}


void init_timestamp(short curr, char *cur_time, int len)
{
   struct tm *ltm;

   /*
    * Get boot time
    * NOTE: loc_time structure must have been init'ed before!
    */
   if (PRINT_ORG_TIME(flags)) {
      loc_time.tm_hour = file_stats[curr].hour;
      loc_time.tm_min  = file_stats[curr].minute;
      loc_time.tm_sec  = file_stats[curr].second;
   }
   else {
      if (USE_H_OPTION(flags) || USE_DB_OPTION(flags))
	 ltm = gmtime(&file_stats[curr].ust_time);
      else
	 ltm = localtime(&file_stats[curr].ust_time);
      loc_time = *ltm;
   }

   if (!cur_time)
      /* Stop if cur_time is NULL */
      return;

   if (USE_DB_OPTION(flags))
      strftime(cur_time, len, "%Y-%m-%d %H:%M:%S UTC", &loc_time);
   else if (!USE_H_OPTION(flags))
      strftime(cur_time, len, "%X  ", &loc_time);
}


/*
 * Network interfaces may now be registered (and unregistered) dynamically.
 * This is what we try to guess here.
 */
int check_iface_reg(struct stats_net_dev *st_net_dev[], short curr, short ref, int pos)
{
   struct stats_net_dev *st_net_dev_i, *st_net_dev_j;
   int index = 0;

   st_net_dev_i = st_net_dev[curr] + pos;

   while (index < file_hdr.sa_iface) {
      st_net_dev_j = st_net_dev[ref] + index;
      if (!strcmp(st_net_dev_i->interface, st_net_dev_j->interface)) {
	 /*
	  * Network interface found.
	  * If a counter has decreased, then we may assume that the
	  * corresponding interface was unregistered, then registered again.
	  */
	 if ((st_net_dev_i->rx_packets < st_net_dev_j->rx_packets) ||
	     (st_net_dev_i->tx_packets < st_net_dev_j->tx_packets) ||
	     (st_net_dev_i->rx_bytes < st_net_dev_j->rx_bytes) ||
	     (st_net_dev_i->tx_bytes < st_net_dev_j->tx_bytes) ||
	     (st_net_dev_i->rx_compressed < st_net_dev_j->rx_compressed) ||
	     (st_net_dev_i->tx_compressed < st_net_dev_j->tx_compressed) ||
	     (st_net_dev_i->multicast < st_net_dev_j->multicast) ||
	     (st_net_dev_i->rx_errors < st_net_dev_j->rx_errors) ||
	     (st_net_dev_i->tx_errors < st_net_dev_j->tx_errors) ||
	     (st_net_dev_i->collisions < st_net_dev_j->collisions) ||
	     (st_net_dev_i->rx_dropped < st_net_dev_j->rx_dropped) ||
	     (st_net_dev_i->tx_dropped < st_net_dev_j->tx_dropped) ||
	     (st_net_dev_i->tx_carrier_errors < st_net_dev_j->tx_carrier_errors) ||
	     (st_net_dev_i->rx_frame_errors < st_net_dev_j->rx_frame_errors) ||
	     (st_net_dev_i->rx_fifo_errors < st_net_dev_j->rx_fifo_errors) ||
	     (st_net_dev_i->tx_fifo_errors < st_net_dev_j->tx_fifo_errors)) {

	    memset(st_net_dev_j, 0, STATS_NET_DEV_SIZE);
	    strcpy(st_net_dev_j->interface, st_net_dev_i->interface);
	 }
	 return index;
      }
      index++;
   }

   /* Network interface not found: Look for the first free structure */
   for (index = 0; index < file_hdr.sa_iface; index++) {
      st_net_dev_j = st_net_dev[ref] + index;
      if (!strcmp(st_net_dev_j->interface, "?")) {
	 memset(st_net_dev_j, 0, STATS_NET_DEV_SIZE);
	 strcpy(st_net_dev_j->interface, st_net_dev_i->interface);
	 break;
      }
   }
   if (index >= file_hdr.sa_iface)
      /* No free structure: Default is structure of same rank */
      index = pos;

   st_net_dev_j = st_net_dev[ref] + index;
   /* Since the name is not the same, reset all the structure */
   memset(st_net_dev_j, 0, STATS_NET_DEV_SIZE);
   strcpy(st_net_dev_j->interface, st_net_dev_i->interface);

   return  index;
}


/*
 * Disks in /proc/stat may be registered dynamically.
 * This is what we try to guess here.
 */
int check_disk_reg(struct disk_stats *st_disk[], short curr, short ref, int pos)
{
   struct disk_stats *st_disk_i, *st_disk_j;
   int index = 0;

   st_disk_i = st_disk[curr] + pos;

   while (index < file_hdr.sa_nr_disk) {
      st_disk_j = st_disk[ref] + index;
      if ((st_disk_i->major == st_disk_j->major) && (st_disk_i->index == st_disk_j->index)) {
	 /*
	  * Disk found.
	  * If a counter has decreased, then we may assume that the
	  * corresponding device was unregistered, then registered again.
	  * NB: AFAIK, such a device cannot be unregistered with current Linux kernels.
	  */
	 if ((st_disk_i->dk_drive < st_disk_j->dk_drive) ||
	     (st_disk_i->dk_drive_rwblk < st_disk_j->dk_drive_rwblk)) {

	    memset(st_disk_j, 0, DISK_STATS_SIZE);
	    st_disk_j->major = st_disk_i->major;
	    st_disk_j->index = st_disk_i->index;
	 }
	 return index;
      }
      index++;
   }

   /* Disk not found: Look for the first free structure */
   for (index = 0; index < file_hdr.sa_nr_disk; index++) {
      st_disk_j = st_disk[ref] + index;
      if (!(st_disk_j->major + st_disk_j->index)) {
	 memset(st_disk_j, 0, DISK_STATS_SIZE);
	 st_disk_j->major = st_disk_i->major;
	 st_disk_j->index = st_disk_i->index;
	 break;
      }
   }
   if (index >= file_hdr.sa_nr_disk)
      /* No free structure found: Default is structure of same rank */
      index = pos;

   st_disk_j = st_disk[ref] + index;
   /* Since the device is not the same, reset all the structure */
   memset(st_disk_j, 0, DISK_STATS_SIZE);
   st_disk_j->major = st_disk_i->major;
   st_disk_j->index = st_disk_i->index;

   return index;
}


/*
 * Print statistics average
 */
void write_stats_avg(int curr, short dis, unsigned int act, int read_from_file)
{
   int i, j = 0, k;
   unsigned long itv, itv0;
   struct stats_irq_cpu *p, *q, *p0, *q0;
   struct stats_serial *st_serial_i, *st_serial_j;
   struct stats_net_dev *st_net_dev_i, *st_net_dev_j;
   struct stats_one_cpu *st_cpu_i, *st_cpu_j;
   struct disk_stats *st_disk_i, *st_disk_j;
   char stemp[16];

   /* Interval value in jiffies */
   itv0 = file_stats[curr].uptime - file_stats[2].uptime;
   itv  = itv0 / (file_hdr.sa_proc + 1);
   if (!itv) {	/* Should no longer happen with version 3.0 and higher... */
      /*
       * Aiee: null interval... This should only happen when reading stats
       * from a system activity file with a far too big interval value...
       */
      fprintf(stderr, _("Please give a smaller interval value\n"));
      exit(1);
   }

   if (GET_PROC(act)) {
      if (dis)
	 printf(_("\nAverage:       proc/s\n"));

      printf(_("Average:    %9.2f\n"),
	     S_VALUE(file_stats[2].processes, file_stats[curr].processes, itv));
   }

   if (GET_CTXSW(act)) {
      if (dis)
	 printf(_("\nAverage:      cswch/s\n"));

      printf(_("Average:    %9.2f\n"),
	     S_VALUE(file_stats[2].context_swtch, file_stats[curr].context_swtch, itv));
   }

   if (GET_CPU(act) || GET_ONE_CPU(act)) {
      if (dis)
	 printf(_("\nAverage:          CPU     %%user     %%nice   %%system     %%idle\n"));

      if (GET_CPU(act)) {
	
	 printf(_("Average:          all"));

	 printf("    %6.2f    %6.2f    %6.2f",
		SP_VALUE(file_stats[2].cpu_user,   file_stats[curr].cpu_user,   itv0),
		SP_VALUE(file_stats[2].cpu_nice,   file_stats[curr].cpu_nice,   itv0),
		SP_VALUE(file_stats[2].cpu_system, file_stats[curr].cpu_system, itv0));

	 if (file_stats[curr].cpu_idle < file_stats[2].cpu_idle)	/* Handle buggy RTC (or kernels?) */
	    printf("      %.2f\n", 0.0);
	 else
	    printf("    %6.2f\n",
		   SP_VALUE(file_stats[2].cpu_idle, file_stats[curr].cpu_idle, itv0));
      }

      if (GET_ONE_CPU(act)) {
	 for (i = 0; i <= file_hdr.sa_proc; i++) {
	    if (cpu_bitmap[i >> 5] & (1 << (i & 0x1f))) {

	       printf(_("Average:          %3d"), i);
	       st_cpu_i = st_cpu[curr] + i;
	       st_cpu_j = st_cpu[2]    + i;
	
	       printf("    %6.2f    %6.2f    %6.2f",
		      SP_VALUE(st_cpu_j->per_cpu_user,   st_cpu_i->per_cpu_user,   itv),
		      SP_VALUE(st_cpu_j->per_cpu_nice,   st_cpu_i->per_cpu_nice,   itv),
		      SP_VALUE(st_cpu_j->per_cpu_system, st_cpu_i->per_cpu_system, itv));

	       if (st_cpu_i->per_cpu_idle < st_cpu_j->per_cpu_idle)	/* Handle buggy RTC (or kernels?) */
		  printf("      %.2f\n", 0.0);
	       else
		  printf("    %6.2f\n",
			 SP_VALUE(st_cpu_j->per_cpu_idle, st_cpu_i->per_cpu_idle, itv));
	    }
	 }
      }
   }

   if (GET_IRQ(act) || GET_ONE_IRQ(act)) {
      if (dis)
	 printf(_("\nAverage:         INTR    intr/s\n"));

      if (GET_IRQ(act)) {
	 printf(_("Average:          sum"));

	 printf(" %9.2f\n",
		S_VALUE(file_stats[2].irq_sum, file_stats[curr].irq_sum, itv));
      }

      if (GET_ONE_IRQ(act)) {
	 for (i = 0; i < NR_IRQS; i++) {
	    if (irq_bitmap[i >> 5] & (1 << (i & 0x1f))) {

	       printf(_("Average:          %3d"), i);

	       printf(" %9.2f\n",
		      S_VALUE(interrupts[2][i], interrupts[curr][i], itv));
	    }
	 }
      }
   }

   if (GET_PAGE(act)) {
      if (dis)
	 printf(_("\nAverage:     pgpgin/s pgpgout/s  activepg  inadtypg  inaclnpg  inatarpg\n"));

      printf(_("Average:    %9.2f %9.2f %9.0f %9.0f %9.0f %9.0f\n"),
	     S_VALUE(file_stats[2].pgpgin,  file_stats[curr].pgpgin,  itv),
	     S_VALUE(file_stats[2].pgpgout, file_stats[curr].pgpgout, itv),
	     (double) asum.nr_active_pages         / asum.count,
	     (double) asum.nr_inactive_dirty_pages / asum.count,
	     (double) asum.nr_inactive_clean_pages / asum.count,
	     (double) asum.inactive_target         / asum.count);
   }

   if (GET_SWAP(act)) {
      if (dis)
	 printf(_("\nAverage:     pswpin/s pswpout/s\n"));

      printf(_("Average:    %9.2f %9.2f\n"),
	     S_VALUE(file_stats[2].pswpin,  file_stats[curr].pswpin,  itv),
	     S_VALUE(file_stats[2].pswpout, file_stats[curr].pswpout, itv));
   }

   if (GET_IO(act)) {
      if (dis)
	 printf(_("\nAverage:          tps      rtps      wtps   bread/s   bwrtn/s\n"));

      printf(_("Average:    %9.2f %9.2f %9.2f %9.2f %9.2f\n"),
	     S_VALUE(file_stats[2].dk_drive,      file_stats[curr].dk_drive,      itv),
	     S_VALUE(file_stats[2].dk_drive_rio,  file_stats[curr].dk_drive_rio,  itv),
	     S_VALUE(file_stats[2].dk_drive_wio,  file_stats[curr].dk_drive_wio,  itv),
	     S_VALUE(file_stats[2].dk_drive_rblk, file_stats[curr].dk_drive_rblk, itv),
	     S_VALUE(file_stats[2].dk_drive_wblk, file_stats[curr].dk_drive_wblk, itv));
   }

   if (GET_MEMORY(act)) {
      if (dis)
	 printf(_("\nAverage:      frmpg/s   shmpg/s   bufpg/s   campg/s\n"));

      printf(_("Average:    %9.2f %9.2f %9.2f %9.2f\n"),
	     ((double) PG(file_stats[curr].frmkb) - (double) PG(file_stats[2].frmkb)) / itv * HZ,
	     ((double) PG(file_stats[curr].shmkb) - (double) PG(file_stats[2].shmkb)) / itv * HZ,
	     ((double) PG(file_stats[curr].bufkb) - (double) PG(file_stats[2].bufkb)) / itv * HZ,
	     ((double) PG(file_stats[curr].camkb) - (double) PG(file_stats[2].camkb)) / itv * HZ);
   }

   if (GET_PID(act)) {
      if (dis)
	 printf(_("\nAverage:          PID  minflt/s  majflt/s     %%user   %%system   nswap/s\n"));

      for (i = 0; i < pid_nr; i++) {
	 if (!pid_stats[curr][i]->pid || !(pid_stats[curr][i]->flag & 0x01))
	    continue;
	 printf(_("Average:    %9ld"), pid_stats[curr][i]->pid);

	 printf(" %9.2f %9.2f    %6.2f    %6.2f %9.2f\n",
		S_VALUE(pid_stats[2][i]->minflt, pid_stats[curr][i]->minflt, itv),
		S_VALUE(pid_stats[2][i]->majflt, pid_stats[curr][i]->majflt, itv),
		S_VALUE(pid_stats[2][i]->utime,  pid_stats[curr][i]->utime,  itv),
		S_VALUE(pid_stats[2][i]->stime,  pid_stats[curr][i]->stime,  itv),
		((double) ((long) pid_stats[curr][i]->nswap - (long) pid_stats[2][i]->nswap)) / itv * HZ);
      }
   }

   if (GET_CPID(act)) {
      if (dis)
	 printf(_("\nAverage:         PPID cminflt/s cmajflt/s    %%cuser  %%csystem  cnswap/s\n"));

      for (i = 0; i < pid_nr; i++) {
	 if (!pid_stats[curr][i]->pid || !(pid_stats[curr][i]->flag & 0x02))
	    continue;
	 printf(_("Average:    %9ld"), pid_stats[curr][i]->pid);

	 printf(" %9.2f %9.2f    %6.2f    %6.2f %9.2f\n",
		S_VALUE(pid_stats[2][i]->cminflt, pid_stats[curr][i]->cminflt, itv),
		S_VALUE(pid_stats[2][i]->cmajflt, pid_stats[curr][i]->cmajflt, itv),
		S_VALUE(pid_stats[2][i]->cutime,  pid_stats[curr][i]->cutime,  itv),
		S_VALUE(pid_stats[2][i]->cstime,  pid_stats[curr][i]->cstime,  itv),
		((double) ((long) pid_stats[curr][i]->cnswap - (long) pid_stats[2][i]->cnswap)) / itv * HZ);
      }
   }

   /*
    * Average not available for A_SUM_PID activity if a process was
    * killed during sar execution (total number of major and minor faults
    * can be lower than the previous time).
    */
   if (GET_SUM_PID(act)) {
      if (dis)
	 printf(_("\nAverage:     minflt/s  majflt/s\n"));

      if (FLT_ARE_INC(flags))
	 printf(_("Average:          N/A       N/A\n"));
      else {
	 printf(_("Average:    %9.2f %9.2f\n"),
		S_VALUE(file_stats[2].minflt, file_stats[curr].minflt, itv),
		S_VALUE(file_stats[2].majflt, file_stats[curr].majflt, itv));
      }
   }

   if (GET_SERIAL(act)) {
      if (dis)
	 printf(_("\nAverage:          TTY   rcvin/s   xmtin/s\n"));

      for (i = 0; i < file_hdr.sa_serial; i++) {

	 st_serial_i = st_serial[curr] + i;
	 st_serial_j = st_serial[2]    + i;
	 if (st_serial_i->line == 0xff)
	    continue;

	 printf(_("Average:          "));
	 printf("%3d", st_serial_i->line);

	 if (st_serial_i->line == st_serial_j->line) {
	    printf(" %9.2f %9.2f\n",
		   S_VALUE(st_serial_j->rx, st_serial_i->rx, itv),
		   S_VALUE(st_serial_j->tx, st_serial_i->tx, itv));
	 }
	 else
	    printf("       N/A       N/A\n");
      }
   }

   if (GET_MEM_AMT(act)) {
      if (dis)
	 printf(_("\nAverage:    kbmemfree kbmemused  %%memused kbmemshrd kbbuffers  kbcached kbswpfree kbswpused  %%swpused\n"));

      printf(_("Average:    %9.0f %9.0f"),
	     (double) asum.frmkb / asum.count,
	     (double) file_stats[curr].tlmkb - ((double) asum.frmkb / asum.count));
      if (file_stats[curr].tlmkb)
	 printf("    %6.2f",
		SP_VALUE(asum.frmkb / asum.count, file_stats[curr].tlmkb, file_stats[curr].tlmkb));
      else
	 printf("      %.2f", 0.0);

      printf(" %9.0f %9.0f %9.0f %9.0f %9.0f",
	     (double) asum.shmkb / asum.count,
	     (double) asum.bufkb / asum.count,
	     (double) asum.camkb / asum.count,
	     (double) asum.frskb / asum.count,
	     ((double) asum.tlskb / asum.count) - ((double) asum.frskb / asum.count));
      if (asum.tlskb / asum.count)
	 printf("    %6.2f\n",
		SP_VALUE(asum.frskb / asum.count, asum.tlskb / asum.count, asum.tlskb / asum.count));
      else
	 printf("      %.2f\n", 0.0);
   }

   if (GET_IRQ_CPU(act)) {
      int offset;

      if (dis) {
	 /* Print header */
	 printf(_("\nAverage:     CPU"));
	 for (j = 0; j < file_hdr.sa_irqcpu; j++) {
	    p0 = st_irq_cpu[curr] + j;
	    if (p0->irq != ~0)	/* Nb of irq per proc may have varied... */
	       printf(_("  i%03d/s"), p0->irq);
	 }
	 printf("\n");
      }

      for (k = 0; k <= file_hdr.sa_proc; k++) {
	 printf(_("Average:     %3d"), k);

	 for (j = 0; j < file_hdr.sa_irqcpu; j++) {
	    p0 = st_irq_cpu[curr] + j;	/* irq field set only for proc #0 */
	    if (p0->irq != ~0) {
	       q0 = st_irq_cpu[2] + j;
	       offset = j;

	       if (p0->irq != q0->irq) {
		  if (j)
		     offset = j - 1;
		  q0 = st_irq_cpu[2] + offset;
		  if ((p0->irq != q0->irq) && (j + 1 < file_hdr.sa_irqcpu))
		     offset = j + 1;
		  q0 = st_irq_cpu[2] + offset;
	       }
	       if (p0->irq == q0->irq) {
		  p = st_irq_cpu[curr] + k * file_hdr.sa_irqcpu + j;
		  q = st_irq_cpu[2]    + k * file_hdr.sa_irqcpu + offset;
		  printf(" %7.2f",
			 S_VALUE(q->interrupt, p->interrupt, itv));
	       }
	       else
		  printf("     N/A");
	    }
	 }
	 printf("\n");
      }
   }

   if (GET_KTABLES(act)) {
      if (dis)
	 printf(_("\nAverage:    dentunusd   file-sz  %%file-sz  inode-sz  super-sz %%super-sz  dquot-sz %%dquot-sz  rtsig-sz %%rtsig-sz\n"));

      printf(_("Average:    %9.0f"), (double) asum.dentry_stat / asum.count);

      printf(" %9.0f", (double) asum.file_used / asum.count);
      if (file_stats[curr].file_max)
	 printf("    %6.2f",
		((double) ((asum.file_used / asum.count) * 100)) / file_stats[curr].file_max);
      else
	 printf("      %.2f", 0.0);

      printf(" %9.0f", (double) asum.inode_used / asum.count);

      printf(" %9.0f", (double) asum.super_used / asum.count);
      if (file_stats[curr].super_max)
	 printf("    %6.2f",
		((double) ((asum.super_used / asum.count) * 100)) / file_stats[curr].super_max);
      else
	 printf("      %.2f", 0.0);

      printf(" %9.0f", (double) asum.dquot_used / asum.count);
      if (file_stats[curr].dquot_max)
	 printf("    %6.2f",
		((double) ((asum.dquot_used / asum.count) * 100)) / file_stats[curr].dquot_max);
      else
	 printf("      %.2f", 0.0);

      printf(" %9.0f", (double) asum.rtsig_queued / asum.count);
      if (file_stats[curr].rtsig_max)
	 printf("    %6.2f\n",
		((double) ((asum.rtsig_queued / asum.count) * 100)) / file_stats[curr].rtsig_max);
      else
	 printf("      %.2f\n", 0.0);
   }

   if (GET_NET_DEV(act)) {
      if (dis)
	 printf(_("\nAverage:        IFACE   rxpck/s   txpck/s   rxbyt/s   txbyt/s   rxcmp/s   txcmp/s  rxmcst/s\n"));

      for (i = 0; i < file_hdr.sa_iface; i++) {

	 st_net_dev_i = st_net_dev[curr] + i;
	 if (!strcmp(st_net_dev_i->interface, "?"))
	    continue;
	 j = check_iface_reg(st_net_dev, curr, 2, i);
	 st_net_dev_j = st_net_dev[2] + j;

	 printf(_("Average:       %6s"), st_net_dev_i->interface);
	
	 printf(" %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f\n",
		S_VALUE(st_net_dev_j->rx_packets,    st_net_dev_i->rx_packets,    itv),
		S_VALUE(st_net_dev_j->tx_packets,    st_net_dev_i->tx_packets,    itv),
		S_VALUE(st_net_dev_j->rx_bytes,      st_net_dev_i->rx_bytes,      itv),
		S_VALUE(st_net_dev_j->tx_bytes,      st_net_dev_i->tx_bytes,      itv),
		S_VALUE(st_net_dev_j->rx_compressed, st_net_dev_i->rx_compressed, itv),
		S_VALUE(st_net_dev_j->tx_compressed, st_net_dev_i->tx_compressed, itv),
		S_VALUE(st_net_dev_j->multicast,     st_net_dev_i->multicast,     itv));
      }
   }

   if (GET_NET_EDEV(act)) {
      if (dis)
	 printf(_("\nAverage:        IFACE   rxerr/s   txerr/s    coll/s  rxdrop/s  txdrop/s  txcarr/s  rxfram/s  rxfifo/s  txfifo/s\n"));

      for (i = 0; i < file_hdr.sa_iface; i++) {

	 st_net_dev_i = st_net_dev[curr] + i;
	 if (!strcmp(st_net_dev_i->interface, "?"))
	    continue;
	 j = check_iface_reg(st_net_dev, curr, 2, i);
	 st_net_dev_j = st_net_dev[2] + j;

	 printf(_("Average:       %6s"), st_net_dev_i->interface);
	
	 printf(" %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f\n",
		S_VALUE(st_net_dev_j->rx_errors,         st_net_dev_i->rx_errors,         itv),
		S_VALUE(st_net_dev_j->tx_errors,         st_net_dev_i->tx_errors,         itv),
		S_VALUE(st_net_dev_j->collisions,        st_net_dev_i->collisions,        itv),
		S_VALUE(st_net_dev_j->rx_dropped,        st_net_dev_i->rx_dropped,        itv),
		S_VALUE(st_net_dev_j->tx_dropped,        st_net_dev_i->tx_dropped,        itv),
		S_VALUE(st_net_dev_j->tx_carrier_errors, st_net_dev_i->tx_carrier_errors, itv),
		S_VALUE(st_net_dev_j->rx_frame_errors,   st_net_dev_i->rx_frame_errors,   itv),
		S_VALUE(st_net_dev_j->rx_fifo_errors,    st_net_dev_i->rx_fifo_errors,    itv),
		S_VALUE(st_net_dev_j->tx_fifo_errors,    st_net_dev_i->tx_fifo_errors,    itv));
      }
   }

   if (GET_NET_SOCK(act)) {
      if (dis)
	 printf(_("\nAverage:       totsck    tcpsck    udpsck    rawsck   ip-frag\n"));

      printf(_("Average:    %9.0f %9.0f %9.0f %9.0f %9.0f\n"),
	     (double) asum.sock_inuse / asum.count,
	     (double) asum.tcp_inuse  / asum.count,
	     (double) asum.udp_inuse  / asum.count,
	     (double) asum.raw_inuse  / asum.count,
	     (double) asum.frag_inuse / asum.count);
   }

   if (GET_QUEUE(act)) {
      if (dis)
	 printf(_("\nAverage:      runq-sz  plist-sz   ldavg-1   ldavg-5\n"));

      printf(_("Average:    %9.0f %9.0f %9.2f %9.2f\n"),
	     (double) asum.nr_running / asum.count,
	     (double) asum.nr_threads / asum.count,
	     (double) asum.load_avg_1 / (asum.count * 100),
	     (double) asum.load_avg_5 / (asum.count * 100));
   }

   if (GET_DISK(act)) {
      if (dis)
	 printf(_("\nAverage:          DEV       tps    blks/s\n"));

      for (i = 0; i < file_hdr.sa_nr_disk; i++) {
	
	 st_disk_i = st_disk[curr] + i;
	 if (!(st_disk_i->major + st_disk_i->index))
	    continue;
	 j = check_disk_reg(st_disk, curr, 2, i);
	 st_disk_j = st_disk[2] + j;
	
	 sprintf(stemp, "dev%d-%d", st_disk_i->major, st_disk_i->index);
	 printf(_("Average:    %9.9s %9.2f %9.2f\n"), stemp,
		S_VALUE(st_disk_j->dk_drive,       st_disk_i->dk_drive,       itv),
		S_VALUE(st_disk_j->dk_drive_rwblk, st_disk_i->dk_drive_rwblk, itv));
      }
   }

   if (read_from_file)
      /* Reset counters only if we read stats from a system activity file */
      init_stats_sum();
}


/*
 * Print system statistics onto the screen
 */
int write_stats(short curr, short dis, unsigned int act, int read_from_file, long *cnt,
		int use_tm_start, int use_tm_end, int reset, int want_since_boot)
{
   int i, j = 0, k;
   char cur_time[2][14];
   unsigned long itv, itv0;
   struct stats_irq_cpu *p, *q, *p0, *q0;
   struct stats_serial *st_serial_i, *st_serial_j;
   struct stats_net_dev *st_net_dev_i, *st_net_dev_j;
   struct stats_one_cpu *st_cpu_i, *st_cpu_j;
   struct disk_stats *st_disk_i, *st_disk_j;
   char stemp[16];

   /* Check time (1) */
   if (read_from_file) {
      if (!next_slice(curr, reset))
	 /* Not close enough to desired interval */
	 return 0;
   }

   /* Get previous timestamp */
   init_timestamp(!curr, cur_time[!curr], 14);
   /* Get current timestamp */
   init_timestamp(curr, cur_time[curr], 14);
   /* Only the first 11 characters are printed */
   cur_time[curr][11] = cur_time[!curr][11] = '\0';

   /* Check time (2) */
   if (prep_time(use_tm_start, curr, &itv, &itv0))
      /* It's too soon... */
      return 0;

   (asum.count)++;	/* Nb of lines printed */

   /* Print number of processes created per second */
   if (GET_PROC(act)) {
      if (dis)
	 printf(_("\n%-11s    proc/s\n"), cur_time[!curr]);

      printf("%-11s %9.2f\n", cur_time[curr],
	     S_VALUE(file_stats[!curr].processes, file_stats[curr].processes, itv));
   }

   /* Print number of context switches per second */
   if (GET_CTXSW(act)) {
      if (dis)
	 printf(_("\n%-11s   cswch/s\n"), cur_time[!curr]);

      printf("%-11s %9.2f\n", cur_time[curr],
	     S_VALUE(file_stats[!curr].context_swtch, file_stats[curr].context_swtch, itv));
   }

   /* Print CPU usage */
   if (GET_CPU(act) || GET_ONE_CPU(act)) {
      if (dis)
	 printf(_("\n%-11s       CPU     %%user     %%nice   %%system     %%idle\n"), cur_time[!curr]);

      if (GET_CPU(act)) {
	 printf(_("%-11s       all"), cur_time[curr]);

	 printf("    %6.2f    %6.2f    %6.2f",
		SP_VALUE(file_stats[!curr].cpu_user,   file_stats[curr].cpu_user,   itv0),
		SP_VALUE(file_stats[!curr].cpu_nice,   file_stats[curr].cpu_nice,   itv0),
		SP_VALUE(file_stats[!curr].cpu_system, file_stats[curr].cpu_system, itv0));

	 if (file_stats[curr].cpu_idle < file_stats[!curr].cpu_idle)	/* Handle buggy RTC (or kernels?) */
	    printf("      %.2f\n", 0.0);
	 else
	    printf("    %6.2f\n",
		   SP_VALUE(file_stats[!curr].cpu_idle, file_stats[curr].cpu_idle, itv0));
      }

      if (GET_ONE_CPU(act)) {
	 for (i = 0; i <= file_hdr.sa_proc; i++) {
	    if (cpu_bitmap[i >> 5] & (1 << (i & 0x1f))) {

	       printf("%-11s       %3d", cur_time[curr], i);
	       st_cpu_i = st_cpu[curr]  + i;
	       st_cpu_j = st_cpu[!curr] + i;
	
	       printf("    %6.2f    %6.2f    %6.2f",
		      SP_VALUE(st_cpu_j->per_cpu_user,   st_cpu_i->per_cpu_user,   itv),
		      SP_VALUE(st_cpu_j->per_cpu_nice,   st_cpu_i->per_cpu_nice,   itv),
		      SP_VALUE(st_cpu_j->per_cpu_system, st_cpu_i->per_cpu_system, itv));

	       if (st_cpu_i->per_cpu_idle < st_cpu_j->per_cpu_idle)	/* Handle buggy RTC (or kernels?) */
		  printf("      %.2f\n", 0.0);
	       else
		  printf("    %6.2f\n",
			 SP_VALUE(st_cpu_j->per_cpu_idle, st_cpu_i->per_cpu_idle, itv));
	    }
	 }
      }
   }

   if (GET_IRQ(act) || GET_ONE_IRQ(act)) {
      if (dis)
	 printf(_("\n%-11s      INTR    intr/s\n"), cur_time[!curr]);

      /* Print number of interrupts per second */
      if (GET_IRQ(act)) {
	 printf(_("%-11s       sum"), cur_time[curr]);

	 printf(" %9.2f\n",
		S_VALUE(file_stats[!curr].irq_sum, file_stats[curr].irq_sum, itv));
      }

      if (GET_ONE_IRQ(act)) {
	 for (i = 0; i < NR_IRQS; i++) {
	    if (irq_bitmap[i >> 5] & (1 << (i & 0x1f))) {

	       printf("%-11s       %3d", cur_time[curr], i);

	       printf(" %9.2f\n",
		      S_VALUE(interrupts[!curr][i], interrupts[curr][i], itv));
	    }
	 }
      }
   }

   /* Print number of pages the system paged in and out */
   if (GET_PAGE(act)) {
      if (dis)
	 printf(_("\n%-11s  pgpgin/s pgpgout/s  activepg  inadtypg  inaclnpg  inatarpg\n"), cur_time[!curr]);

      printf("%-11s %9.2f %9.2f %9u %9u %9u %9lu\n", cur_time[curr],
	     S_VALUE(file_stats[!curr].pgpgin,  file_stats[curr].pgpgin,  itv),
	     S_VALUE(file_stats[!curr].pgpgout, file_stats[curr].pgpgout, itv),
	     file_stats[curr].nr_active_pages,
	     file_stats[curr].nr_inactive_dirty_pages,
	     file_stats[curr].nr_inactive_clean_pages,
	     file_stats[curr].inactive_target);

      /*
       * Will be used to compute the average.
       * Note: overflow unlikely to happen but not impossible...
       */
      asum.nr_active_pages         += file_stats[curr].nr_active_pages;
      asum.nr_inactive_dirty_pages += file_stats[curr].nr_inactive_dirty_pages;
      asum.nr_inactive_clean_pages += file_stats[curr].nr_inactive_clean_pages;
      asum.inactive_target         += file_stats[curr].inactive_target;
   }

   /* Print number of swap pages brought in and out */
   if (GET_SWAP(act)) {
      if (dis)
	 printf(_("\n%-11s  pswpin/s pswpout/s\n"), cur_time[!curr]);

      printf("%-11s %9.2f %9.2f\n", cur_time[curr],
	     S_VALUE(file_stats[!curr].pswpin,  file_stats[curr].pswpin,  itv),
	     S_VALUE(file_stats[!curr].pswpout, file_stats[curr].pswpout, itv));
   }

   /* Print I/O stats (no distinction made between disks) */
   if (GET_IO(act)) {
      if (dis)
	 printf(_("\n%-11s       tps      rtps      wtps   bread/s   bwrtn/s\n"), cur_time[!curr]);

      printf("%-11s %9.2f %9.2f %9.2f %9.2f %9.2f\n", cur_time[curr],
	     S_VALUE(file_stats[!curr].dk_drive,      file_stats[curr].dk_drive,      itv),
	     S_VALUE(file_stats[!curr].dk_drive_rio,  file_stats[curr].dk_drive_rio,  itv),
	     S_VALUE(file_stats[!curr].dk_drive_wio,  file_stats[curr].dk_drive_wio,  itv),
	     S_VALUE(file_stats[!curr].dk_drive_rblk, file_stats[curr].dk_drive_rblk, itv),
	     S_VALUE(file_stats[!curr].dk_drive_wblk, file_stats[curr].dk_drive_wblk, itv));
   }

   /* Print memory stats */
   if (GET_MEMORY(act)) {
      if (dis)
	 printf(_("\n%-11s   frmpg/s   shmpg/s   bufpg/s   campg/s\n"), cur_time[!curr]);

      printf("%-11s %9.2f %9.2f %9.2f %9.2f\n", cur_time[curr],
	     ((double) PG(file_stats[curr].frmkb) - (double) PG(file_stats[!curr].frmkb)) / itv * HZ,
	     ((double) PG(file_stats[curr].shmkb) - (double) PG(file_stats[!curr].shmkb)) / itv * HZ,
	     ((double) PG(file_stats[curr].bufkb) - (double) PG(file_stats[!curr].bufkb)) / itv * HZ,
	     ((double) PG(file_stats[curr].camkb) - (double) PG(file_stats[!curr].camkb)) / itv * HZ);
   }

   /* Print per-process statistics */
   if (GET_PID(act)) {
      if (dis)
	 printf(_("\n%-11s       PID  minflt/s  majflt/s     %%user   %%system   nswap/s   CPU\n"), cur_time[!curr]);

      for (i = 0; i < pid_nr; i++) {
	 if (!pid_stats[curr][i]->pid || !(pid_stats[curr][i]->flag & 0x01))
	    continue;

 	 printf("%-11s %9ld", cur_time[curr], pid_stats[curr][i]->pid);

	 printf(" %9.2f %9.2f    %6.2f    %6.2f %9.2f   %3d\n",
		S_VALUE(pid_stats[!curr][i]->minflt, pid_stats[curr][i]->minflt, itv),
		S_VALUE(pid_stats[!curr][i]->majflt, pid_stats[curr][i]->majflt, itv),
		S_VALUE(pid_stats[!curr][i]->utime,  pid_stats[curr][i]->utime,  itv),
		S_VALUE(pid_stats[!curr][i]->stime,  pid_stats[curr][i]->stime,  itv),
		((double) ((long) pid_stats[curr][i]->nswap - (long) pid_stats[!curr][i]->nswap)) / itv * HZ,
		pid_stats[curr][i]->processor);
      }
   }

   /* Print statistics about children of a given process */
   if (GET_CPID(act)) {
      if (dis)
	 printf(_("\n%-11s      PPID cminflt/s cmajflt/s    %%cuser  %%csystem  cnswap/s\n"), cur_time[!curr]);

      for (i = 0; i < pid_nr; i++) {
	 if (!pid_stats[curr][i]->pid || !(pid_stats[curr][i]->flag & 0x02))
	    continue;
	 printf("%-11s %9ld", cur_time[curr], pid_stats[curr][i]->pid);

	 printf(" %9.2f %9.2f    %6.2f    %6.2f %9.2f\n",
		S_VALUE(pid_stats[!curr][i]->cminflt, pid_stats[curr][i]->cminflt, itv),
		S_VALUE(pid_stats[!curr][i]->cmajflt, pid_stats[curr][i]->cmajflt, itv),
		S_VALUE(pid_stats[!curr][i]->cutime,  pid_stats[curr][i]->cutime,  itv),
		S_VALUE(pid_stats[!curr][i]->cstime,  pid_stats[curr][i]->cstime,  itv),
		((double) ((long) pid_stats[curr][i]->cnswap - (long) pid_stats[!curr][i]->cnswap)) / itv * HZ);
      }
   }

   /* Print number of system minor/major faults */
   if (GET_SUM_PID(act)) {
      if (dis)
	 printf(_("\n%-11s  minflt/s  majflt/s\n"), cur_time[!curr]);

      printf("%-11s", cur_time[curr]);

      /*
       * Results are meaningful only if no processes have terminated
       * since the last time.
       */
      if ((file_stats[curr].nr_processes - file_stats[!curr].nr_processes) !=
	  (file_stats[curr].processes    - file_stats[!curr].processes)) {
	 printf("      ????      ????\n");
	 /* A process was killed: cannot compute the average */
	 flags |= F_FLT_INC;
      }
      else {
	 printf(" %9.2f %9.2f\n",
		S_VALUE(file_stats[!curr].minflt, file_stats[curr].minflt, itv),
		S_VALUE(file_stats[!curr].majflt, file_stats[curr].majflt, itv));
      }
   }

   /* Print TTY statistics (serial lines) */
   if (GET_SERIAL(act)) {
      if (dis)
	 printf(_("\n%-11s       TTY   rcvin/s   xmtin/s\n"), cur_time[!curr]);

      for (i = 0; i < file_hdr.sa_serial; i++) {

	 st_serial_i = st_serial[curr]  + i;
	 st_serial_j = st_serial[!curr] + i;
	 if (st_serial_i->line == 0xff)
	    continue;
	
	 printf("%-11s       %3d", cur_time[curr], st_serial_i->line);

	 if ((st_serial_i->line == st_serial_j->line) || want_since_boot) {
	    printf(" %9.2f %9.2f\n",
		   S_VALUE(st_serial_j->rx, st_serial_i->rx, itv),
		   S_VALUE(st_serial_j->tx, st_serial_i->tx, itv));
	 }
	 else
	    printf("      ????      ????\n");
      }
   }

   /* Print amount and usage of memory */
   if (GET_MEM_AMT(act)) {
      if (dis)
	 printf(_("\n%-11s kbmemfree kbmemused  %%memused kbmemshrd kbbuffers  kbcached kbswpfree kbswpused  %%swpused\n"), cur_time[!curr]);

      printf("%-11s %9lu %9lu", cur_time[curr],
	     file_stats[curr].frmkb,
	     file_stats[curr].tlmkb - file_stats[curr].frmkb);
      if (file_stats[curr].tlmkb)
	 printf("    %6.2f",
		SP_VALUE(file_stats[curr].frmkb, file_stats[curr].tlmkb, file_stats[curr].tlmkb));
      else
	 printf("      %.2f", 0.0);

      printf(" %9lu %9lu %9lu %9lu %9lu",
	     file_stats[curr].shmkb,
	     file_stats[curr].bufkb,
	     file_stats[curr].camkb,
	     file_stats[curr].frskb,
	     file_stats[curr].tlskb - file_stats[curr].frskb);
      if (file_stats[curr].tlskb)
	 printf("    %6.2f\n",
		SP_VALUE(file_stats[curr].frskb, file_stats[curr].tlskb, file_stats[curr].tlskb));
      else
	 printf("      %.2f\n", 0.0);

      /*
       * Will be used to compute the average.
       * Note: overflow unlikely to happen but not impossible...
       * We assume that the total amount of memory installed can not vary
       * during the interval given on the command line, whereas the total amount
       * of swap space may.
       */
      asum.frmkb += file_stats[curr].frmkb;
      asum.shmkb += file_stats[curr].shmkb;
      asum.bufkb += file_stats[curr].bufkb;
      asum.camkb += file_stats[curr].camkb;
      asum.frskb += file_stats[curr].frskb;
      asum.tlskb += file_stats[curr].tlskb;
   }

   if (GET_IRQ_CPU(act)) {
      int offset;

      j = 0;
      /* Check if number of interrupts has changed */
      if (!dis && !want_since_boot) {
	 do {
	    p0 = st_irq_cpu[curr] + j;
	    if (p0->irq != ~0) {
	       q0 = st_irq_cpu[!curr] + j;
	       if (p0->irq != q0->irq)
		  j = -2;
	    }
	    j++;
	 }
	 while ((j > 0) && (j <= file_hdr.sa_irqcpu));
      }

      if (dis || (j < 0)) {
	 /* Print header */
	 printf(_("\n%-11s  CPU"), cur_time[!curr]);
	 for (j = 0; j < file_hdr.sa_irqcpu; j++) {
	    p0 = st_irq_cpu[curr] + j;
	    if (p0->irq != ~0)
	       printf(_("  i%03d/s"), p0->irq);
	 }
	 printf("\n");
      }

      for (k = 0; k <= file_hdr.sa_proc; k++) {
	 printf("%-11s  %3d", cur_time[curr], k);

	 for (j = 0; j < file_hdr.sa_irqcpu; j++) {
	    p0 = st_irq_cpu[curr] + j;	/* irq field set only for proc #0 */
	    if (p0->irq != ~0) {	/* A value of ~0 means it is a remaining interrupt
					 * which is no longer used, for example because the
					 * number of interrupts has decreased in /proc/interrupts
					 * or because we are appending data to an old sa file
					 * with more interrupts than are actually available now */
	       q0 = st_irq_cpu[!curr] + j;
	       offset = j;

	       /*
		* If we want stats for the time since system startup,
		* we have p0->irq != q0->irq, since q0 structure is
		* completely set to zero.
		*/
	       if ((p0->irq != q0->irq) && !want_since_boot) {
		  if (j)
		     offset = j - 1;
		  q0 = st_irq_cpu[!curr] + offset;
		  if ((p0->irq != q0->irq) && (j + 1 < file_hdr.sa_irqcpu))
		     offset = j + 1;
		  q0 = st_irq_cpu[!curr] + offset;
	       }
	       if ((p0->irq == q0->irq) || want_since_boot) {
		  p = st_irq_cpu[curr]  + k * file_hdr.sa_irqcpu + j;
		  q = st_irq_cpu[!curr] + k * file_hdr.sa_irqcpu + offset;
		  printf(" %7.2f",
			 S_VALUE(q->interrupt, p->interrupt, itv));
	       }
	       else
		  printf("     N/A");
	    }
	 }
	 printf("\n");
      }
   }

   /* Print values of some kernel tables */
   if (GET_KTABLES(act)) {
      if (dis)
	 printf(_("\n%-11s dentunusd   file-sz  %%file-sz  inode-sz  super-sz %%super-sz  dquot-sz %%dquot-sz  rtsig-sz %%rtsig-sz\n"),
		cur_time[!curr]);

      printf("%-11s %9u", cur_time[curr], file_stats[curr].dentry_stat);

      printf(" %9u", file_stats[curr].file_used);
      if (file_stats[curr].file_max)
	 printf("    %6.2f",
		((double) (file_stats[curr].file_used * 100)) / file_stats[curr].file_max);
      else
	 printf("      %.2f", 0.0);

      printf(" %9u", file_stats[curr].inode_used);

      printf(" %9u", file_stats[curr].super_used);
      if (file_stats[curr].super_max)
	 printf("    %6.2f",
		((double) (file_stats[curr].super_used * 100)) / file_stats[curr].super_max);
      else
	 printf("      %.2f", 0.0);

      printf(" %9u", file_stats[curr].dquot_used);
      if (file_stats[curr].dquot_max)
	 printf("    %6.2f",
		((double) (file_stats[curr].dquot_used * 100)) / file_stats[curr].dquot_max);
      else
	 printf("      %.2f", 0.0);

      printf(" %9u", file_stats[curr].rtsig_queued);
      if (file_stats[curr].rtsig_max)
	 printf("    %6.2f\n",
		((double) (file_stats[curr].rtsig_queued * 100)) / file_stats[curr].rtsig_max);
      else
	 printf("      %.2f\n", 0.0);

      /*
       * Will be used to compute the average.
       * Note: overflow unlikely to happen but not impossible...
       * We assume that *_max values can not vary during the interval.
       */
      asum.dentry_stat  += file_stats[curr].dentry_stat;
      asum.file_used    += file_stats[curr].file_used;
      asum.inode_used   += file_stats[curr].inode_used;
      asum.super_used   += file_stats[curr].super_used;
      asum.dquot_used   += file_stats[curr].dquot_used;
      asum.rtsig_queued += file_stats[curr].rtsig_queued;
   }

   /* Print network interface statistics */
   if (GET_NET_DEV(act)) {
      if (dis)
	 printf(_("\n%-11s     IFACE   rxpck/s   txpck/s   rxbyt/s   txbyt/s   rxcmp/s   txcmp/s  rxmcst/s\n"),
		cur_time[!curr]);

      for (i = 0; i < file_hdr.sa_iface; i++) {

	 st_net_dev_i = st_net_dev[curr] + i;
	 if (!strcmp(st_net_dev_i->interface, "?"))
	    continue;
	 j = check_iface_reg(st_net_dev, curr, !curr, i);
	 st_net_dev_j = st_net_dev[!curr] + j;
	
	 printf("%-11s    %6s", cur_time[curr], st_net_dev_i->interface);
	
	 printf(" %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f\n",
		S_VALUE(st_net_dev_j->rx_packets,    st_net_dev_i->rx_packets,    itv),
		S_VALUE(st_net_dev_j->tx_packets,    st_net_dev_i->tx_packets,    itv),
		S_VALUE(st_net_dev_j->rx_bytes,      st_net_dev_i->rx_bytes,      itv),
		S_VALUE(st_net_dev_j->tx_bytes,      st_net_dev_i->tx_bytes,      itv),
		S_VALUE(st_net_dev_j->rx_compressed, st_net_dev_i->rx_compressed, itv),
		S_VALUE(st_net_dev_j->tx_compressed, st_net_dev_i->tx_compressed, itv),
		S_VALUE(st_net_dev_j->multicast,     st_net_dev_i->multicast,     itv));
      }
   }

   /* Print network interface statistics (errors) */
   if (GET_NET_EDEV(act)) {
      if (dis)
	 printf(_("\n%-11s     IFACE   rxerr/s   txerr/s    coll/s  rxdrop/s  txdrop/s  txcarr/s  rxfram/s  rxfifo/s  txfifo/s\n"),
		cur_time[!curr]);

      for (i = 0; i < file_hdr.sa_iface; i++) {

	 st_net_dev_i = st_net_dev[curr] + i;
	 if (!strcmp(st_net_dev_i->interface, "?"))
	    continue;
	 j = check_iface_reg(st_net_dev, curr, !curr, i);
	 st_net_dev_j = st_net_dev[!curr] + j;
	
	 printf("%-11s    %6s", cur_time[curr], st_net_dev_i->interface);

	 printf(" %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f\n",
		S_VALUE(st_net_dev_j->rx_errors,         st_net_dev_i->rx_errors,         itv),
		S_VALUE(st_net_dev_j->tx_errors,         st_net_dev_i->tx_errors,         itv),
		S_VALUE(st_net_dev_j->collisions,        st_net_dev_i->collisions,        itv),
		S_VALUE(st_net_dev_j->rx_dropped,        st_net_dev_i->rx_dropped,        itv),
		S_VALUE(st_net_dev_j->tx_dropped,        st_net_dev_i->tx_dropped,        itv),
		S_VALUE(st_net_dev_j->tx_carrier_errors, st_net_dev_i->tx_carrier_errors, itv),
		S_VALUE(st_net_dev_j->rx_frame_errors,   st_net_dev_i->rx_frame_errors,   itv),
		S_VALUE(st_net_dev_j->rx_fifo_errors,    st_net_dev_i->rx_fifo_errors,    itv),
		S_VALUE(st_net_dev_j->tx_fifo_errors,    st_net_dev_i->tx_fifo_errors,    itv));
      }
   }

   /* Print number of sockets in use */
   if (GET_NET_SOCK(act)) {
      if (dis)
	 printf(_("\n%-11s    totsck    tcpsck    udpsck    rawsck   ip-frag\n"), cur_time[!curr]);

      printf("%-11s %9u %9u %9u %9u %9u\n", cur_time[curr],
	     file_stats[curr].sock_inuse,
	     file_stats[curr].tcp_inuse,
	     file_stats[curr].udp_inuse,
	     file_stats[curr].raw_inuse,
	     file_stats[curr].frag_inuse);

      /* Will be used to compute the average */
      asum.sock_inuse += file_stats[curr].sock_inuse;
      asum.tcp_inuse  += file_stats[curr].tcp_inuse;
      asum.udp_inuse  += file_stats[curr].udp_inuse;
      asum.raw_inuse  += file_stats[curr].raw_inuse;
      asum.frag_inuse += file_stats[curr].frag_inuse;
   }

   /* Print load averages and queue length */
   if (GET_QUEUE(act)) {
      if (dis)
	 printf(_("\n%-11s   runq-sz  plist-sz   ldavg-1   ldavg-5\n"), cur_time[!curr]);

      printf("%-11s %9u %9u %9.2f %9.2f\n", cur_time[curr],
	     file_stats[curr].nr_running,
	     file_stats[curr].nr_threads,
	     (double) file_stats[curr].load_avg_1 / 100,
	     (double) file_stats[curr].load_avg_5 / 100);

      /* Will be used to compute the average */
      asum.nr_running += file_stats[curr].nr_running;
      asum.nr_threads += file_stats[curr].nr_threads;
      asum.load_avg_1 += file_stats[curr].load_avg_1;
      asum.load_avg_5 += file_stats[curr].load_avg_5;
   }

   /* Print disk statistics */
   if (GET_DISK(act)) {
      if (dis)
	 printf(_("\n%-11s       DEV       tps    blks/s\n"), cur_time[!curr]);

      for (i = 0; i < file_hdr.sa_nr_disk; i++) {
	
	 st_disk_i = st_disk[curr]  + i;
	 if (!(st_disk_i->major + st_disk_i->index))
	    continue;
	 j = check_disk_reg(st_disk, curr, !curr, i);
	 st_disk_j = st_disk[!curr] + j;

	 sprintf(stemp, "dev%d-%d", st_disk_i->major, st_disk_i->index);
	 printf("%-11s %9.9s %9.2f %9.2f\n", cur_time[curr], stemp,
		S_VALUE(st_disk_j->dk_drive,       st_disk_i->dk_drive,       itv),
		S_VALUE(st_disk_j->dk_drive_rwblk, st_disk_i->dk_drive_rwblk, itv));
      }
   }

   /* Check time (3) */
   if (use_tm_end && (datecmp(&loc_time, &tm_end) >= 0)) {
      /* It's too late now... */
      *cnt = 0;
      return 0;
   }

   return 1;
}


/*
 * Print system statistics to be used by pattern processing commands
 */
void write_stats_for_ppc(short curr, unsigned int act, unsigned long dt,
			unsigned long itv, unsigned long itv0, char *cur_time)
{
   int i, j, k;
   struct stats_one_cpu *st_cpu_i, *st_cpu_j;
   struct stats_serial *st_serial_i, *st_serial_j;
   struct stats_irq_cpu *p, *q, *p0, *q0;
   struct stats_net_dev *st_net_dev_i, *st_net_dev_j;
   struct disk_stats *st_disk_i, *st_disk_j;

   if (GET_PROC(act))
      printf("%s\t%ld\t%s\t-\tproc/s\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].processes, file_stats[curr].processes, itv));

   /* Print number of context switches per second */
   if (GET_CTXSW(act))
      printf("%s\t%ld\t%s\t-\tcswch/s\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].context_swtch, file_stats[curr].context_swtch, itv));

   /* Print CPU usage */
   if (GET_CPU(act)) {
      printf("%s\t%ld\t%s\tall\t%%user\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     SP_VALUE(file_stats[!curr].cpu_user, file_stats[curr].cpu_user, itv0));
      printf("%s\t%ld\t%s\tall\t%%nice\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     SP_VALUE(file_stats[!curr].cpu_nice, file_stats[curr].cpu_nice, itv0));
      printf("%s\t%ld\t%s\tall\t%%system\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     SP_VALUE(file_stats[!curr].cpu_system, file_stats[curr].cpu_system, itv0));
      printf("%s\t%ld\t%s\tall\t%%idle\t", file_hdr.sa_nodename, dt, cur_time);
      if (file_stats[curr].cpu_idle < file_stats[!curr].cpu_idle)	/* Handle buggy RTC (or kernels?) */
	 printf("%.2f\n", 0.0);
      else
	 printf("%.2f\n",
		SP_VALUE(file_stats[!curr].cpu_idle, file_stats[curr].cpu_idle, itv0));
   }

   if (GET_ONE_CPU(act)) {

      for (i = 0; i <= file_hdr.sa_proc; i++) {
	 if (cpu_bitmap[i >> 5] & (1 << (i & 0x1f))) {

	    st_cpu_i = st_cpu[curr]  + i;
	    st_cpu_j = st_cpu[!curr] + i;
	
	    printf("%s\t%ld\t%s\tcpu%d\t%%user\t%.2f\n", file_hdr.sa_nodename, dt, cur_time, i,
		   SP_VALUE(st_cpu_j->per_cpu_user, st_cpu_i->per_cpu_user, itv));
	    printf("%s\t%ld\t%s\tcpu%d\t%%nice\t%.2f\n", file_hdr.sa_nodename, dt, cur_time, i,
		   SP_VALUE(st_cpu_j->per_cpu_nice, st_cpu_i->per_cpu_nice, itv));
	    printf("%s\t%ld\t%s\tcpu%d\t%%system\t%.2f\n", file_hdr.sa_nodename, dt, cur_time, i,
		   SP_VALUE(st_cpu_j->per_cpu_system, st_cpu_i->per_cpu_system, itv));

	    if (st_cpu_i->per_cpu_idle < st_cpu_j->per_cpu_idle)	/* Handle buggy RTC (or kernels?) */
	       printf("%s\t%ld\t%s\tcpu%d\t%%idle\t%.2f\n", file_hdr.sa_nodename, dt, cur_time, i, 0.0);
	    else
	       printf("%s\t%ld\t%s\tcpu%d\t%%idle\t%.2f\n", file_hdr.sa_nodename, dt, cur_time, i,
		      SP_VALUE(st_cpu_j->per_cpu_idle, st_cpu_i->per_cpu_idle, itv));
	 }
      }
   }

   /* Print number of interrupts per second */
   if (GET_IRQ(act))
      printf("%s\t%ld\t%s\tsum\tintr/s\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].irq_sum, file_stats[curr].irq_sum, itv));

   if (GET_ONE_IRQ(act)) {
      for (i = 0; i < NR_IRQS; i++) {
	 if (irq_bitmap[i >> 5] & (1 << (i & 0x1f))) {

	    printf("%s\t%ld\t%s\ti%03d\tintr/s\t%.2f\n", file_hdr.sa_nodename, dt, cur_time, i,
		   S_VALUE(interrupts[!curr][i], interrupts[curr][i], itv));
	 }
      }
   }

   /* Print number of pages the system paged in and out */
   if (GET_PAGE(act)) {
      printf("%s\t%ld\t%s\t-\tpgpgin/s\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].pgpgin, file_stats[curr].pgpgin, itv));
      printf("%s\t%ld\t%s\t-\tpgpgout/s\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].pgpgout, file_stats[curr].pgpgout, itv));
      printf("%s\t%ld\t%s\t-\tactivepg\t%u\n", file_hdr.sa_nodename, dt, cur_time,
	    file_stats[curr].nr_active_pages);
      printf("%s\t%ld\t%s\t-\tinadtypg\t%u\n", file_hdr.sa_nodename, dt, cur_time,
	    file_stats[curr].nr_inactive_dirty_pages);
      printf("%s\t%ld\t%s\t-\tinaclnpg\t%u\n", file_hdr.sa_nodename, dt, cur_time,
	    file_stats[curr].nr_inactive_clean_pages);
      printf("%s\t%ld\t%s\t-\tinatarpg\t%lu\n", file_hdr.sa_nodename, dt, cur_time,
	    file_stats[curr].inactive_target);
   }

   /* Print number of swap pages brought in and out */
   if (GET_SWAP(act)) {
      printf("%s\t%ld\t%s\t-\tpswpin/s\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].pswpin, file_stats[curr].pswpin, itv));
      printf("%s\t%ld\t%s\t-\tpgpgout/s\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].pswpout, file_stats[curr].pswpout, itv));
   }

   /* Print I/O stats (no distinction made between disks) */
   if (GET_IO(act)) {
      printf("%s\t%ld\t%s\t-\ttps\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].dk_drive, file_stats[curr].dk_drive, itv));
      printf("%s\t%ld\t%s\t-\trtps\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].dk_drive_rio, file_stats[curr].dk_drive_rio, itv));
      printf("%s\t%ld\t%s\t-\twtps\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].dk_drive_wio, file_stats[curr].dk_drive_wio, itv));
      printf("%s\t%ld\t%s\t-\tbread/s\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].dk_drive_rblk, file_stats[curr].dk_drive_rblk, itv));
      printf("%s\t%ld\t%s\t-\tbwrtn/s\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].dk_drive_wblk, file_stats[curr].dk_drive_wblk, itv));
   }

   /* Print memory stats */
   if (GET_MEMORY(act)) {
      printf("%s\t%ld\t%s\t-\tfrmpg/s\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     ((double) PG(file_stats[curr].frmkb) - (double) PG(file_stats[!curr].frmkb)) / itv * HZ);
      printf("%s\t%ld\t%s\t-\tshmpg/s\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     ((double) PG(file_stats[curr].shmkb) - (double) PG(file_stats[!curr].shmkb)) / itv * HZ);
      printf("%s\t%ld\t%s\t-\tbufpg/s\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     ((double) PG(file_stats[curr].bufkb) - (double) PG(file_stats[!curr].bufkb)) / itv * HZ);
      printf("%s\t%ld\t%s\t-\tcampg/s\t%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     ((double) PG(file_stats[curr].camkb) - (double) PG(file_stats[!curr].camkb)) / itv * HZ);
   }

   /* Print TTY statistics (serial lines) */
   if (GET_SERIAL(act)) {

      for (i = 0; i < file_hdr.sa_serial; i++) {

	 st_serial_i = st_serial[curr]  + i;
	 st_serial_j = st_serial[!curr] + i;
	 if (st_serial_i->line == 0xff)
	    continue;
	
	 if (st_serial_i->line == st_serial_j->line) {
	    printf("%s\t%ld\t%s\tttyS%d\trcvin/s\t%.2f\n",
		   file_hdr.sa_nodename, dt, cur_time, st_serial_i->line,
		   S_VALUE(st_serial_j->rx, st_serial_i->rx, itv));
	    printf("%s\t%ld\t%s\tttyS%d\txmtin/s\t%.2f\n",
		   file_hdr.sa_nodename, dt, cur_time, st_serial_i->line,
		   S_VALUE(st_serial_j->tx, st_serial_i->tx, itv));
	 }
      }
   }

   /* Print amount and usage of memory */
   if (GET_MEM_AMT(act)) {
      printf("%s\t%ld\t%s\t-\tkbmemfree\t%lu\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].frmkb);
      printf("%s\t%ld\t%s\t-\tkbmemused\t%lu\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].tlmkb - file_stats[curr].frmkb);
      printf("%s\t%ld\t%s\t-\t%%memused\t", file_hdr.sa_nodename, dt, cur_time);
      if (file_stats[curr].tlmkb)
	 printf("%.2f\n",
		SP_VALUE(file_stats[curr].frmkb, file_stats[curr].tlmkb, file_stats[curr].tlmkb));
      else
	 printf("%.2f\n", 0.0);

      printf("%s\t%ld\t%s\t-\tkbmemshrd\t%lu\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].shmkb);
      printf("%s\t%ld\t%s\t-\tkbbuffers\t%lu\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].bufkb);
      printf("%s\t%ld\t%s\t-\tkbcached\t%lu\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].camkb);
      printf("%s\t%ld\t%s\t-\tkbswpfree\t%lu\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].frskb);
      printf("%s\t%ld\t%s\t-\tkbswpused\t%lu\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].tlskb - file_stats[curr].frskb);
      printf("%s\t%ld\t%s\t-\t%%swpused\t", file_hdr.sa_nodename, dt, cur_time);
      if (file_stats[curr].tlskb)
	 printf("%.2f\n",
		SP_VALUE(file_stats[curr].frskb, file_stats[curr].tlskb, file_stats[curr].tlskb));
      else
	 printf("%.2f\n", 0.0);
   }

   if (GET_IRQ_CPU(act)) {
      int offset;

      for (k = 0; k <= file_hdr.sa_proc; k++) {

	 for (j = 0; j < file_hdr.sa_irqcpu; j++) {
	    p0 = st_irq_cpu[curr] + j;	/* irq field set only for proc #0 */
	    if (p0->irq != ~0) {	/* A value of ~0 means it is a remaining interrupt
					 * which is no longer used, for example because the
					 * number of interrupts has decreased in /proc/interrupts
					 * or because we are appending data to an old sa file
					 * with more interrupts than are actually available now */
	       q0 = st_irq_cpu[!curr] + j;
	       offset = j;

	       if (p0->irq != q0->irq) {
		  if (j)
		     offset = j - 1;
		  q0 = st_irq_cpu[!curr] + offset;
		  if ((p0->irq != q0->irq) && (j + 1 < file_hdr.sa_irqcpu))
		     offset = j + 1;
		  q0 = st_irq_cpu[!curr] + offset;
	       }
	       if (p0->irq == q0->irq) {
		  p = st_irq_cpu[curr]  + k * file_hdr.sa_irqcpu + j;
		  q = st_irq_cpu[!curr] + k * file_hdr.sa_irqcpu + offset;
		  printf("%s\t%ld\t%s\tcpu%d\ti%03d/s\t%.2f\n",
			 file_hdr.sa_nodename, dt, cur_time, k, p0->irq,
			 S_VALUE(q->interrupt, p->interrupt, itv));
	       }
	    }
	 }
      }
   }

   /* Print values of some kernel tables */
   if (GET_KTABLES(act)) {
      printf("%s\t%ld\t%s\t-\tdentunusd\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].dentry_stat);
      printf("%s\t%ld\t%s\t-\tfile-sz\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].file_used);
      printf("%s\t%ld\t%s\t-\t%%file-sz\t", file_hdr.sa_nodename, dt, cur_time);
      if (file_stats[curr].file_max)
	 printf("%.2f\n",
		((double) (file_stats[curr].file_used * 100)) / file_stats[curr].file_max);
      else
	 printf("%.2f\n", 0.0);

      printf("%s\t%ld\t%s\t-\tinode-sz\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].inode_used);

      printf("%s\t%ld\t%s\t-\tsuper-sz\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].super_used);
      printf("%s\t%ld\t%s\t-\t%%super-sz\t", file_hdr.sa_nodename, dt, cur_time);
      if (file_stats[curr].super_max)
	 printf("%.2f\n",
		((double) (file_stats[curr].super_used * 100)) / file_stats[curr].super_max);
      else
	 printf("%.2f\n", 0.0);

      printf("%s\t%ld\t%s\t-\tdquot-sz\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].dquot_used);
      printf("%s\t%ld\t%s\t-\t%%dquot-sz\t", file_hdr.sa_nodename, dt, cur_time);
      if (file_stats[curr].dquot_max)
	 printf("%.2f\n",
		((double) (file_stats[curr].dquot_used * 100)) / file_stats[curr].dquot_max);
      else
	 printf("%.2f\n", 0.0);

      printf("%s\t%ld\t%s\t-\trtsig-sz\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].rtsig_queued);
      printf("%s\t%ld\t%s\t-\t%%rtsig-sz\t", file_hdr.sa_nodename, dt, cur_time);
      if (file_stats[curr].rtsig_max)
	 printf("%.2f\n",
		((double) (file_stats[curr].rtsig_queued * 100)) / file_stats[curr].rtsig_max);
      else
	 printf("%.2f\n", 0.0);
   }

   /* Print network interface statistics */
   if (GET_NET_DEV(act)) {

      for (i = 0; i < file_hdr.sa_iface; i++) {

	 st_net_dev_i = st_net_dev[curr] + i;
	 if (!strcmp(st_net_dev_i->interface, "?"))
	    continue;
	 j = check_iface_reg(st_net_dev, curr, !curr, i);
	 st_net_dev_j = st_net_dev[!curr] + j;

	 printf("%s\t%ld\t%s\t%s\trxpck/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->rx_packets, st_net_dev_i->rx_packets, itv));
	 printf("%s\t%ld\t%s\t%s\ttxpck/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->tx_packets, st_net_dev_i->tx_packets, itv));
	 printf("%s\t%ld\t%s\t%s\trxbyt/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->rx_bytes, st_net_dev_i->rx_bytes, itv));
	 printf("%s\t%ld\t%s\t%s\ttxbyt/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->tx_bytes, st_net_dev_i->tx_bytes, itv));
	 printf("%s\t%ld\t%s\t%s\trxcmp/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->rx_compressed, st_net_dev_i->rx_compressed, itv));
	 printf("%s\t%ld\t%s\t%s\ttxcmp/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->tx_compressed, st_net_dev_i->tx_compressed, itv));
	 printf("%s\t%ld\t%s\t%s\trxmcst/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->multicast, st_net_dev_i->multicast, itv));
      }
   }

   /* Print network interface statistics (errors) */
   if (GET_NET_EDEV(act)) {

      for (i = 0; i < file_hdr.sa_iface; i++) {

	 st_net_dev_i = st_net_dev[curr] + i;
	 if (!strcmp(st_net_dev_i->interface, "?"))
	    continue;
	 j = check_iface_reg(st_net_dev, curr, !curr, i);
	 st_net_dev_j = st_net_dev[!curr] + j;
	
	 printf("%s\t%ld\t%s\t%s\trxerr/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->rx_errors, st_net_dev_i->rx_errors, itv));
	 printf("%s\t%ld\t%s\t%s\ttxerr/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->tx_errors, st_net_dev_i->tx_errors, itv));
	 printf("%s\t%ld\t%s\t%s\tcoll/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->collisions, st_net_dev_i->collisions, itv));
	 printf("%s\t%ld\t%s\t%s\trxdrop/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->rx_dropped, st_net_dev_i->rx_dropped, itv));
	 printf("%s\t%ld\t%s\t%s\ttxdrop/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->tx_dropped, st_net_dev_i->tx_dropped, itv));
	 printf("%s\t%ld\t%s\t%s\ttxcarr/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->tx_carrier_errors, st_net_dev_i->tx_carrier_errors, itv));
	 printf("%s\t%ld\t%s\t%s\trxfram/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->rx_frame_errors, st_net_dev_i->rx_frame_errors, itv));
	 printf("%s\t%ld\t%s\t%s\trxfifo/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->rx_fifo_errors, st_net_dev_i->rx_fifo_errors, itv));
	 printf("%s\t%ld\t%s\t%s\ttxfifo/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->tx_fifo_errors, st_net_dev_i->tx_fifo_errors, itv));
      }
   }

   /* Print number of sockets in use */
   if (GET_NET_SOCK(act)) {
      printf("%s\t%ld\t%s\t-\ttotsck\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].sock_inuse);
      printf("%s\t%ld\t%s\t-\ttcpsck\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].tcp_inuse);
      printf("%s\t%ld\t%s\t-\tudpsck\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].udp_inuse);
      printf("%s\t%ld\t%s\t-\trawsck\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].raw_inuse);
      printf("%s\t%ld\t%s\t-\tip-frag\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].frag_inuse);
   }

   /* Print load averages and queue length */
   if (GET_QUEUE(act)) {
      printf("%s\t%ld\t%s\t-\trunq-sz\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].nr_running);
      printf("%s\t%ld\t%s\t-\tplist-sz\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].nr_threads);
      printf("%s\t%ld\t%s\t-\tldavg-1\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time, (double) file_stats[curr].load_avg_1 / 100);
      printf("%s\t%ld\t%s\t-\tldavg-5\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time, (double) file_stats[curr].load_avg_5 / 100);
   }

   /* Print disk statistics */
   if (GET_DISK(act)) {

      for (i = 0; i < file_hdr.sa_nr_disk; i++) {

	 st_disk_i = st_disk[curr]  + i;
	 if (!(st_disk_i->major + st_disk_i->index))
	    continue;
	 j = check_disk_reg(st_disk, curr, !curr, i);
	 st_disk_j = st_disk[!curr] + j;
	
	 printf("%s\t%ld\t%s\tdev%d-%d\ttps\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_disk_i->major, st_disk_i->index,
		S_VALUE(st_disk_j->dk_drive, st_disk_i->dk_drive, itv));
	 printf("%s\t%ld\t%s\tdev%d-%d\tblks/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, st_disk_i->major, st_disk_i->index,
		S_VALUE(st_disk_j->dk_drive_rwblk, st_disk_i->dk_drive_rwblk, itv));
      }
   }
}


/*
 * Print system statistics to be used for loading into a database
 */
void write_stats_for_db(short curr, unsigned int act, unsigned long dt,
		       unsigned long itv, unsigned long itv0, char *cur_time)
{
   int i, j, k;
   struct stats_one_cpu *st_cpu_i, *st_cpu_j;
   struct stats_serial *st_serial_i, *st_serial_j;
   struct stats_irq_cpu *p, *q, *p0, *q0;
   struct stats_net_dev *st_net_dev_i, *st_net_dev_j;
   struct disk_stats *st_disk_i, *st_disk_j;

   /* Print number of processes created per second */
   if (GET_PROC(act))
      printf("%s;%ld;%s;%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].processes, file_stats[curr].processes, itv));

   /* Print number of context switches per second */
   if (GET_CTXSW(act))
      printf("%s;%ld;%s;%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].context_swtch, file_stats[curr].context_swtch, itv));

   /* Print CPU usage */
   if (GET_CPU(act)) {
      printf("%s;%ld;%s;-1;%.2f;%.2f;%.2f;", file_hdr.sa_nodename, dt, cur_time,
	     SP_VALUE(file_stats[!curr].cpu_user, file_stats[curr].cpu_user, itv0),
	     SP_VALUE(file_stats[!curr].cpu_nice, file_stats[curr].cpu_nice, itv0),
	     SP_VALUE(file_stats[!curr].cpu_system, file_stats[curr].cpu_system, itv0));
      if (file_stats[curr].cpu_idle < file_stats[!curr].cpu_idle)	/* Handle buggy RTC (or kernels?) */
	 printf("%.2f\n", 0.0);
      else
	 printf("%.2f\n",
		SP_VALUE(file_stats[!curr].cpu_idle, file_stats[curr].cpu_idle, itv0));
   }

   if (GET_ONE_CPU(act)) {

      for (i = 0; i <= file_hdr.sa_proc; i++) {
	 if (cpu_bitmap[i >> 5] & (1 << (i & 0x1f))) {

	    st_cpu_i = st_cpu[curr]  + i;
	    st_cpu_j = st_cpu[!curr] + i;
	
	    printf("%s;%ld;%s;%d;%.2f;%.2f;%.2f;", file_hdr.sa_nodename, dt, cur_time, i,
		   SP_VALUE(st_cpu_j->per_cpu_user, st_cpu_i->per_cpu_user, itv),
		   SP_VALUE(st_cpu_j->per_cpu_nice, st_cpu_i->per_cpu_nice, itv),
		   SP_VALUE(st_cpu_j->per_cpu_system, st_cpu_i->per_cpu_system, itv));

	    if (st_cpu_i->per_cpu_idle < st_cpu_j->per_cpu_idle)	/* Handle buggy RTC (or kernels?) */
	       printf("%.2f\n", 0.0);
	    else
	       printf("%.2f\n",
		      SP_VALUE(st_cpu_j->per_cpu_idle, st_cpu_i->per_cpu_idle, itv));
	 }
      }
   }

   /* Print number of interrupts per second */
   if (GET_IRQ(act))
      printf("%s;%ld;%s;-1;%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].irq_sum, file_stats[curr].irq_sum, itv));

   if (GET_ONE_IRQ(act)) {
      for (i = 0; i < NR_IRQS; i++) {
	 if (irq_bitmap[i >> 5] & (1 << (i & 0x1f))) {

	    printf("%s;%ld;%s;%d;%.2f\n", file_hdr.sa_nodename, dt, cur_time, i,
		   S_VALUE(interrupts[!curr][i], interrupts[curr][i], itv));
	 }
      }
   }

   /* Print number of pages the system paged in and out */
   if (GET_PAGE(act))
      printf("%s;%ld;%s;%.2f;%.2f;%u;%u;%u;%lu\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].pgpgin, file_stats[curr].pgpgin, itv),
	     S_VALUE(file_stats[!curr].pgpgout, file_stats[curr].pgpgout, itv),
	     file_stats[curr].nr_active_pages,
	     file_stats[curr].nr_inactive_dirty_pages,
	     file_stats[curr].nr_inactive_clean_pages,
	     file_stats[curr].inactive_target);

   /* Print number of swap pages brought in and out */
   if (GET_SWAP(act))
      printf("%s;%ld;%s;%.2f;%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].pswpin, file_stats[curr].pswpin, itv),
	     S_VALUE(file_stats[!curr].pswpout, file_stats[curr].pswpout, itv));

   /* Print I/O stats (no distinction made between disks) */
   if (GET_IO(act))
      printf("%s;%ld;%s;%.2f;%.2f;%.2f;%.2f;%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].dk_drive, file_stats[curr].dk_drive, itv),
	     S_VALUE(file_stats[!curr].dk_drive_rio, file_stats[curr].dk_drive_rio, itv),
	     S_VALUE(file_stats[!curr].dk_drive_wio, file_stats[curr].dk_drive_wio, itv),
	     S_VALUE(file_stats[!curr].dk_drive_rblk, file_stats[curr].dk_drive_rblk, itv),
	     S_VALUE(file_stats[!curr].dk_drive_wblk, file_stats[curr].dk_drive_wblk, itv));

   /* Print memory stats */
   if (GET_MEMORY(act))
      printf("%s;%ld;%s;%.2f;%.2f;%.2f;%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     ((double) PG(file_stats[curr].frmkb) - (double) PG(file_stats[!curr].frmkb)) / itv * HZ,
	     ((double) PG(file_stats[curr].shmkb) - (double) PG(file_stats[!curr].shmkb)) / itv * HZ,
	     ((double) PG(file_stats[curr].bufkb) - (double) PG(file_stats[!curr].bufkb)) / itv * HZ,
	     ((double) PG(file_stats[curr].camkb) - (double) PG(file_stats[!curr].camkb)) / itv * HZ);

   /* Print TTY statistics (serial lines) */
   if (GET_SERIAL(act)) {

      for (i = 0; i < file_hdr.sa_serial; i++) {

	 st_serial_i = st_serial[curr]  + i;
	 st_serial_j = st_serial[!curr] + i;
	 if (st_serial_i->line == 0xff)
	    continue;
	
	 if (st_serial_i->line == st_serial_j->line) {
	    printf("%s;%ld;%s;%d;%.2f;%.2f\n", file_hdr.sa_nodename, dt, cur_time, st_serial_i->line,
		   S_VALUE(st_serial_j->rx, st_serial_i->rx, itv),
		   S_VALUE(st_serial_j->tx, st_serial_i->tx, itv));
	 }
      }
   }

   /* Print amount and usage of memory */
   if (GET_MEM_AMT(act)) {
      printf("%s;%ld;%s;%lu;%lu;", file_hdr.sa_nodename, dt, cur_time,
	     file_stats[curr].frmkb, file_stats[curr].tlmkb - file_stats[curr].frmkb);
      if (file_stats[curr].tlmkb)
	 printf("%.2f",
		SP_VALUE(file_stats[curr].frmkb, file_stats[curr].tlmkb, file_stats[curr].tlmkb));
      else
	 printf("%.2f", 0.0);
      printf(";%lu;%lu;%lu;%lu;%lu;",
	     file_stats[curr].shmkb, file_stats[curr].bufkb, file_stats[curr].camkb,
	     file_stats[curr].frskb, file_stats[curr].tlskb - file_stats[curr].frskb);
      if (file_stats[curr].tlskb)
	 printf("%.2f\n",
		SP_VALUE(file_stats[curr].frskb, file_stats[curr].tlskb, file_stats[curr].tlskb));
      else
	 printf("%.2f\n", 0.0);
   }

   if (GET_IRQ_CPU(act)) {
      int offset;

      for (k = 0; k <= file_hdr.sa_proc; k++) {

	 for (j = 0; j < file_hdr.sa_irqcpu; j++) {
	    p0 = st_irq_cpu[curr] + j;	/* irq field set only for proc #0 */
	    if (p0->irq != ~0) {	/* A value of ~0 means it is a remaining interrupt
					 * which is no longer used, for example because the
					 * number of interrupts has decreased in /proc/interrupts
					 * or because we are appending data to an old sa file
					 * with more interrupts than are actually available now */
	       q0 = st_irq_cpu[!curr] + j;
	       offset = j;

	       if (p0->irq != q0->irq) {
		  if (j)
		     offset = j - 1;
		  q0 = st_irq_cpu[!curr] + offset;
		  if ((p0->irq != q0->irq) && (j + 1 < file_hdr.sa_irqcpu))
		     offset = j + 1;
		  q0 = st_irq_cpu[!curr] + offset;
	       }
	       if (p0->irq == q0->irq) {
		  p = st_irq_cpu[curr]  + k * file_hdr.sa_irqcpu + j;
		  q = st_irq_cpu[!curr] + k * file_hdr.sa_irqcpu + offset;
		  printf("%s;%ld;%s;%d;%d;%.2f\n", file_hdr.sa_nodename, dt, cur_time, k, p0->irq,
			 S_VALUE(q->interrupt, p->interrupt, itv));
	       }
	    }
	 }
      }
   }

   /* Print values of some kernel tables */
   if (GET_KTABLES(act)) {
      printf("%s;%ld;%s;%u;%u;", file_hdr.sa_nodename, dt, cur_time,
	     file_stats[curr].dentry_stat, file_stats[curr].file_used);
      if (file_stats[curr].file_max)
	 printf("%.2f",
		((double) (file_stats[curr].file_used * 100)) / file_stats[curr].file_max);
      else
	 printf("%.2f", 0.0);
      printf(";%u;%u;", file_stats[curr].inode_used, file_stats[curr].super_used);
      if (file_stats[curr].super_max)
	 printf("%.2f",
		((double) (file_stats[curr].super_used * 100)) / file_stats[curr].super_max);
      else
	 printf("%.2f", 0.0);
      printf(";%u;", file_stats[curr].dquot_used);
      if (file_stats[curr].dquot_max)
	 printf("%.2f",
		((double) (file_stats[curr].dquot_used * 100)) / file_stats[curr].dquot_max);
      else
	 printf("%.2f", 0.0);
      printf(";%u;", file_stats[curr].rtsig_queued);
      if (file_stats[curr].rtsig_max)
	 printf("%.2f\n",
		((double) (file_stats[curr].rtsig_queued * 100)) / file_stats[curr].rtsig_max);
      else
	 printf("%.2f\n", 0.0);
   }

   /* Print network interface statistics */
   if (GET_NET_DEV(act)) {

      for (i = 0; i < file_hdr.sa_iface; i++) {

	 st_net_dev_i = st_net_dev[curr] + i;
	 if (!strcmp(st_net_dev_i->interface, "?"))
	    continue;
	 j = check_iface_reg(st_net_dev, curr, !curr, i);
	 st_net_dev_j = st_net_dev[!curr] + j;

	 printf("%s;%ld;%s;%s;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f\n", file_hdr.sa_nodename, dt, cur_time,
		st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->rx_packets, st_net_dev_i->rx_packets, itv),
		S_VALUE(st_net_dev_j->tx_packets, st_net_dev_i->tx_packets, itv),
		S_VALUE(st_net_dev_j->rx_bytes, st_net_dev_i->rx_bytes, itv),
		S_VALUE(st_net_dev_j->tx_bytes, st_net_dev_i->tx_bytes, itv),
		S_VALUE(st_net_dev_j->rx_compressed, st_net_dev_i->rx_compressed, itv),
		S_VALUE(st_net_dev_j->tx_compressed, st_net_dev_i->tx_compressed, itv),
		S_VALUE(st_net_dev_j->multicast, st_net_dev_i->multicast, itv));
      }
   }

   /* Print network interface statistics (errors) */
   if (GET_NET_EDEV(act)) {

      for (i = 0; i < file_hdr.sa_iface; i++) {

	 st_net_dev_i = st_net_dev[curr] + i;
	 if (!strcmp(st_net_dev_i->interface, "?"))
	    continue;
	 j = check_iface_reg(st_net_dev, curr, !curr, i);
	 st_net_dev_j = st_net_dev[!curr] + j;

	 printf("%s;%ld;%s;%s;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f\n", file_hdr.sa_nodename, dt, cur_time,
		st_net_dev_i->interface,
		S_VALUE(st_net_dev_j->rx_errors, st_net_dev_i->rx_errors, itv),
		S_VALUE(st_net_dev_j->tx_errors, st_net_dev_i->tx_errors, itv),
		S_VALUE(st_net_dev_j->collisions, st_net_dev_i->collisions, itv),
		S_VALUE(st_net_dev_j->rx_dropped, st_net_dev_i->rx_dropped, itv),
		S_VALUE(st_net_dev_j->tx_dropped, st_net_dev_i->tx_dropped, itv),
		S_VALUE(st_net_dev_j->tx_carrier_errors, st_net_dev_i->tx_carrier_errors, itv),
		S_VALUE(st_net_dev_j->rx_frame_errors, st_net_dev_i->rx_frame_errors, itv),
		S_VALUE(st_net_dev_j->rx_fifo_errors, st_net_dev_i->rx_fifo_errors, itv),
		S_VALUE(st_net_dev_j->tx_fifo_errors, st_net_dev_i->tx_fifo_errors, itv));
      }
   }

   /* Print number of sockets in use */
   if (GET_NET_SOCK(act))
      printf("%s;%ld;%s;%u;%u;%u;%u;%u\n", file_hdr.sa_nodename, dt, cur_time,
	     file_stats[curr].sock_inuse, file_stats[curr].tcp_inuse, file_stats[curr].udp_inuse,
 	     file_stats[curr].raw_inuse, file_stats[curr].frag_inuse);

   /* Print load averages and queue length */
   if (GET_QUEUE(act))
      printf("%s;%ld;%s;%u;%u;%.2f;%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     file_stats[curr].nr_running, file_stats[curr].nr_threads,
	     (double) file_stats[curr].load_avg_1 / 100, (double) file_stats[curr].load_avg_5 / 100);

   /* Print disk statistics */
   if (GET_DISK(act)) {

      for (i = 0; i < file_hdr.sa_nr_disk; i++) {

	 st_disk_i = st_disk[curr]  + i;
	 if (!(st_disk_i->major + st_disk_i->index))
	    continue;
	 j = check_disk_reg(st_disk, curr, !curr, i);
	 st_disk_j = st_disk[!curr] + j;
	
	 printf("%s;%ld;%s;dev%d-%d;%.2f;%.2f\n", file_hdr.sa_nodename, dt, cur_time,
		st_disk_i->major, st_disk_i->index,
		S_VALUE(st_disk_j->dk_drive, st_disk_i->dk_drive, itv),
		S_VALUE(st_disk_j->dk_drive_rwblk, st_disk_i->dk_drive_rwblk, itv));
      }
   }
}


/*
 * Write system statistics for options -h/-H
 */
int write_parsable_stats(short curr, unsigned int act, int reset, long *cnt,
			 int use_tm_start, int use_tm_end)
{
   unsigned long dt, itv, itv0;
   struct tm *ltm;
   char cur_time[26];

   /* Check time (1) */
   if (!next_slice(curr, reset))
      /* Not close enough to desired interval */
      return 0;

   /* Get current timestamp */
   ltm = gmtime(&file_stats[curr].ust_time);
   loc_time = *ltm;
   if (USE_H_OPTION(flags))
      sprintf(cur_time, "%ld", file_stats[curr].ust_time);
   else
      /* USE_DB_OPTION */
      strftime(cur_time, 26, "%Y-%m-%d %H:%M:%S UTC", &loc_time);

   /* Check time (2) */
   if (prep_time(use_tm_start, curr, &itv, &itv0))
      /* It's too soon... */
      return 0;

   dt = itv / HZ;
   /* Correct rounding error for dt */
   if ((itv % HZ) >= (HZ / 2))
      dt++;

   if (USE_H_OPTION(flags))
      /* Write stats to be used by pattern processing commands */
      write_stats_for_ppc(curr, act, dt, itv, itv0, cur_time);
   else if (USE_DB_OPTION(flags))
      /* Write stats to be used for loading into a database */
      write_stats_for_db(curr, act, dt, itv, itv0, cur_time);

   /* Check time (3) */
   if (use_tm_end && (datecmp(&loc_time, &tm_end) >= 0)) {
      /* It's too late now... */
      *cnt = 0;
      return 0;
   }

   return 1;
}


/*
 * Print a Linux restart message (contents of a DUMMY record)
 */
void write_dummy(short curr)
{
   char cur_time[26];

   init_timestamp(curr, cur_time, 26);

   if (USE_H_OPTION(flags))
      printf("%s\t-1\t%ld\tLINUX-RESTART\n", file_hdr.sa_nodename, file_stats[curr].ust_time);
   else if (USE_DB_OPTION(flags))
      printf("%s;-1;%s;LINUX-RESTART\n", file_hdr.sa_nodename, cur_time);
   else
      printf(_("\n%-11s       LINUX RESTART\n"), cur_time);
}


/*
 * Allocate structures
 */
void allocate_structures(int stype)
{
   if (file_hdr.sa_proc > 0)
      salloc_cpu(file_hdr.sa_proc + 1);
   if ((stype == USE_SADC) && (GET_PID(sar_actflag) || GET_CPID(sar_actflag))) {
      pid_nr = file_hdr.sa_nr_pid;
      salloc_pid(pid_nr);
   }
   if (file_hdr.sa_serial)
      salloc_serial(file_hdr.sa_serial);
   if (file_hdr.sa_irqcpu)
      salloc_irqcpu(file_hdr.sa_proc + 1, file_hdr.sa_irqcpu);
   if (file_hdr.sa_iface)
      salloc_net_dev(file_hdr.sa_iface);
   if (file_hdr.sa_nr_disk)
      salloc_disk(file_hdr.sa_nr_disk);

   /* Print report header */
   print_report_hdr();
}


/*
 * Move structures data
 */
void copy_structures(int dest, int src, int stype)
{
   memcpy(&file_stats[dest], &file_stats[src], FILE_STATS_SIZE);
   if (file_hdr.sa_proc > 0)
      memcpy(st_cpu[dest], st_cpu[src], STATS_ONE_CPU_SIZE * (file_hdr.sa_proc + 1));
   if (GET_ONE_IRQ(file_hdr.sa_actflag))
      memcpy(interrupts[dest], interrupts[src], STATS_ONE_IRQ_SIZE);
   if ((stype == USE_SADC) && (pid_nr))
      memcpy(pid_stats[dest][0], pid_stats[src][0], PID_STATS_SIZE * pid_nr);
   if (file_hdr.sa_serial)
      memcpy(st_serial[dest], st_serial[src], STATS_SERIAL_SIZE * file_hdr.sa_serial);
   if (file_hdr.sa_irqcpu)
      memcpy(st_irq_cpu[dest], st_irq_cpu[src], STATS_IRQ_CPU_SIZE * (file_hdr.sa_proc + 1) * file_hdr.sa_irqcpu);
   if (file_hdr.sa_iface)
      memcpy(st_net_dev[dest], st_net_dev[src], STATS_NET_DEV_SIZE * file_hdr.sa_iface);
   if (file_hdr.sa_nr_disk)
      memcpy(st_disk[dest], st_disk[src], DISK_STATS_SIZE * file_hdr.sa_nr_disk);
}


/*
 * Read varying part of the statistics from a daily data file
 */
void read_extra_stats(short curr, int ifd)
{
   if (file_hdr.sa_proc > 0)
      sa_fread(ifd, st_cpu[curr], STATS_ONE_CPU_SIZE * (file_hdr.sa_proc + 1), HARD_SIZE);
   if (GET_ONE_IRQ(file_hdr.sa_actflag))
      sa_fread(ifd, interrupts[curr], STATS_ONE_IRQ_SIZE, HARD_SIZE);
   if (file_hdr.sa_serial)
      sa_fread(ifd, st_serial[curr], STATS_SERIAL_SIZE * file_hdr.sa_serial, HARD_SIZE);
   if (file_hdr.sa_irqcpu)
      sa_fread(ifd, st_irq_cpu[curr], STATS_IRQ_CPU_SIZE * (file_hdr.sa_proc + 1) * file_hdr.sa_irqcpu, HARD_SIZE);
   if (file_hdr.sa_iface)
      sa_fread(ifd, st_net_dev[curr], STATS_NET_DEV_SIZE * file_hdr.sa_iface, HARD_SIZE);
   if (file_hdr.sa_nr_disk)
      sa_fread(ifd, st_disk[curr], DISK_STATS_SIZE * file_hdr.sa_nr_disk, HARD_SIZE);
   /* PID stats cannot be saved in file. So we don't read them */
}


/*
 * Read a bunch of statistics sent by the data collector
 */
void read_stat_bunch(short curr)
{
   if (sa_read(&file_stats[curr], file_hdr.sa_st_size))
      exit(0);
   if ((file_hdr.sa_proc > 0) && sa_read(st_cpu[curr], STATS_ONE_CPU_SIZE * (file_hdr.sa_proc + 1)))
      exit(0);
   if (GET_ONE_IRQ(file_hdr.sa_actflag) && sa_read(interrupts[curr], STATS_ONE_IRQ_SIZE))
      exit(0);
   if (pid_nr && sa_read(pid_stats[curr][0], PID_STATS_SIZE * pid_nr))
      exit(0);
   if (file_hdr.sa_serial && sa_read(st_serial[curr], STATS_SERIAL_SIZE * file_hdr.sa_serial))
      exit(0);
   if (file_hdr.sa_irqcpu && sa_read(st_irq_cpu[curr], STATS_IRQ_CPU_SIZE * (file_hdr.sa_proc + 1) * file_hdr.sa_irqcpu))
      exit(0);
   if (file_hdr.sa_iface && sa_read(st_net_dev[curr], STATS_NET_DEV_SIZE * file_hdr.sa_iface))
      exit(0);
   if (file_hdr.sa_nr_disk && sa_read(st_disk[curr], DISK_STATS_SIZE * file_hdr.sa_nr_disk))
      exit(0);
}


/*
 * Read statistics from a system activity data file
 */
void read_stats_from_file(char from_file[])
{
   short curr = 1, dis = 1;
   unsigned char rtype;
   unsigned long lines;
   unsigned int act;
   int ifd, nb, davg, next;
   int rows = 23, eosaf = 1, reset = 0;
   long cnt = 1;
   off_t fpos, fps;

   if (!dis_hdr)
      /* Get window size */
      rows = get_win_height();

   /* Open sa data file */
   if ((ifd = open(from_file, O_RDONLY)) < 0) {
      fprintf(stderr, _("Cannot open %s: %s\n"), from_file, strerror(errno));
      exit(2);
   }

   /* Read sa data file header */
   nb = read(ifd, &file_hdr, FILE_HDR_SIZE);
   if ((nb != FILE_HDR_SIZE) || (file_hdr.sa_magic != SA_MAGIC)) {
      fprintf(stderr, _("Invalid system activity file\n"));
      exit(3);
   }

   if ((GET_SERIAL(sar_actflag) && (file_hdr.sa_serial > 1)) ||
       ((GET_NET_DEV(sar_actflag) || GET_NET_EDEV(sar_actflag)) && (file_hdr.sa_iface  > 1)) ||
       (GET_DISK(sar_actflag) && (file_hdr.sa_nr_disk > 1)))
      dis_hdr = 9;

   sar_actflag &= file_hdr.sa_actflag;
   if (!sar_actflag) {
      /*
       * We want stats that are not available,
       * maybe because this is an old version of the sa data file.
       * Note that if A_ONE_CPU or A_ONE_IRQ stats are available, stats
       * concerning _all_ the CPUS and/or IRQS are available.
       */
      fprintf(stderr, _("Requested activities not available in file\n"));
      close(ifd);
      exit(1);
   }

   /* Perform required allocations */
   allocate_structures(USE_SA_FILE);

   /* Read system statistics from file */
   do {
      /*
       * If this record is a DUMMY one, print it and (try to) get another one.
       * We must be sure that we have real stats in file_stats[2].
       */
      do {
	 if (sa_fread(ifd, &file_stats[0], file_hdr.sa_st_size, SOFT_SIZE))
	    /* End of sa data file */
	    return;

	 if (file_stats[0].record_type == R_DUMMY)
	    write_dummy(0);
	 else {
	    /* Ok: previous record was not a DUMMY one. So read now the extra fields. */
	    read_extra_stats(0, ifd);

	    init_timestamp(0, NULL, 0);
	 }
      }
      while ((file_stats[0].record_type == R_DUMMY) ||
	     (tm_start.use && (datecmp(&loc_time, &tm_start) < 0)) ||
	     (tm_end.use && (datecmp(&loc_time, &tm_end) >=0)));

      /* Save the first stats collected. Will be used to compute the average */
      copy_structures(2, 0, USE_SA_FILE);

      reset = 1;	/* Set flag to reset last_uptime variable */

      /* Save current file position */
      if ((fpos = lseek(ifd, 0, SEEK_CUR)) < 0) {
	 perror("lseek");
	 exit(2);
      }

      /* Reading stats between two possible Linux restarts */

      /* For each requested activity... */
      for (act = 1; act <= A_LAST; act <<= 1) {

	 if (sar_actflag & act) {

	    if ((fps = lseek(ifd, fpos, SEEK_SET)) < fpos) {
	       perror("lseek");
	       exit(2);
	    }

	    /* Restore the first stats collected. Used to compute the rate displayed on the first line */
	    copy_structures(!curr, 2, USE_SA_FILE);
	
	    lines = 0;
	    davg  = 0;
	    cnt   = count;

	    do {
	       /* Display count lines of stats */
	       eosaf = sa_fread(ifd, &file_stats[curr], file_hdr.sa_st_size, SOFT_SIZE);
	       rtype = file_stats[curr].record_type;
	
	       if (!eosaf && (rtype != R_DUMMY))
		  /* Read the extra fields since it's not a DUMMY record */
		  read_extra_stats(curr, ifd);

	       dis = !(lines++ % rows);

	       if (!eosaf && (rtype != R_DUMMY)) {

		  /* next is set to 1 when we were close enough to desired interval */
		  if (USE_H_OPTION(flags) || USE_DB_OPTION(flags))
		     next = write_parsable_stats(curr, act, reset, &cnt, tm_start.use, tm_end.use);
		  else
		     next = write_stats(curr, dis, act, USE_SA_FILE, &cnt,
							tm_start.use, tm_end.use, reset, ST_IMMEDIATE);
		  if (next && (cnt > 0))
		     cnt--;
		  if (next) {
		     davg++;
		     curr ^=1;
		  }
		  else
		     lines--;
		  reset = 0;
	       }
	    }
	    while (cnt && !eosaf && (rtype != R_DUMMY));

	    if (davg && !USE_H_OPTION(flags) && !USE_DB_OPTION(flags))
	       write_stats_avg(!curr, dis, act, USE_SA_FILE);
	    reset = 1;
	 }
      }

      if (!cnt) {
	 /* Go to next Linux restart, if possible */
	 do {
	    eosaf = sa_fread(ifd, &file_stats[curr], file_hdr.sa_st_size, SOFT_SIZE);
	    if (!eosaf && (file_stats[curr].record_type != R_DUMMY))
	       read_extra_stats(curr, ifd);
	 }
	 while (!eosaf && (file_stats[curr].record_type != R_DUMMY));
      }

      /* The last record we read was a DUMMY one: print it */
      if (!eosaf && (file_stats[curr].record_type == R_DUMMY))
	 write_dummy(curr);
   }
   while (!eosaf);

   close(ifd);
}


/*
 * Read statistics sent by sadc, the data collector.
 */
void read_stats(void)
{
   short curr = 1, dis = 1;
   unsigned long lines = 0;
   int rows = 23, more = 1;

   /* Read stats header */
   if (sa_read(&file_hdr, FILE_HDR_SIZE))
      exit(0);
   if (file_hdr.sa_magic != SA_MAGIC) {
      /* sar and sadc commands are not consistent */
      fprintf(stderr, _("Invalid data format\n"));
      exit(3);
   }

   /* Force '-U ALL' flag if -A option is used on SMP machines */
   if (USE_A_OPTION(flags) && file_hdr.sa_proc) {
      sar_actflag |= A_ONE_CPU;
      init_cpu_bitmap(~0);
      flags |= F_ALL_PROC;
   }

   /* Check that data corresponding to requested activities are sent by the data collector */
   if (GET_SERIAL(sar_actflag) && !file_hdr.sa_serial) {
      sar_actflag &= ~A_SERIAL;
      dis_hdr--;
   }
   if ((sar_actflag & (A_NET_DEV + A_NET_EDEV)) && !file_hdr.sa_iface) {
      if (GET_NET_DEV(sar_actflag) && GET_NET_EDEV(sar_actflag))
	 dis_hdr--;
      sar_actflag &= ~(A_NET_DEV + A_NET_EDEV);
      dis_hdr--;
   }
   if (GET_IRQ_CPU(sar_actflag) && !file_hdr.sa_irqcpu) {
      sar_actflag &= ~A_IRQ_CPU;
      dis_hdr--;
   }
   if (GET_DISK(sar_actflag) && !file_hdr.sa_nr_disk) {
      sar_actflag &= ~A_DISK;
      dis_hdr--;
   }
   if (!sar_actflag) {
      fprintf(stderr, _("Requested activities not available\n"));
      exit(1);
   }

   if ((GET_SERIAL(sar_actflag) && (file_hdr.sa_serial > 1)) ||
       ((GET_NET_DEV(sar_actflag) || GET_NET_EDEV(sar_actflag)) && (file_hdr.sa_iface  > 1)) ||
       (GET_DISK(sar_actflag) && (file_hdr.sa_nr_disk > 1)))
      dis_hdr = 9;

   if (!dis_hdr) {
      /* Get window size */
      rows = get_win_height();
      lines = rows;
   }

   /* Check use of -U option */
   prep_smp_option(file_hdr.sa_proc);

   /* No need to force sar_actflag to file_hdr.sa_actflag since we are not reading stats from a file */

   /* Perform required allocations */
   allocate_structures(USE_SADC);

   /* Read system statistics sent by the data collector */
   read_stat_bunch(0);

   if (!dis_hdr) {
      if (file_hdr.sa_proc > 0)
	 more = 2 + file_hdr.sa_proc;
      if (pid_nr)
	 more = pid_nr;
   }

   if (!interval) {
      /* Update structures corresponding to boot time */
      memset(&file_stats[1], 0, FILE_STATS_SIZE);
      file_stats[1].record_type = R_STATS;
      file_stats[1].hour        = file_stats[0].hour;
      file_stats[1].minute      = file_stats[0].minute;
      file_stats[1].second      = file_stats[0].second;
      file_stats[1].ust_time    = file_stats[0].ust_time;
      if (file_hdr.sa_proc > 0)
	 memset(st_cpu[1], 0, STATS_ONE_CPU_SIZE * (file_hdr.sa_proc + 1));
      memset(interrupts[1], 0, STATS_ONE_IRQ_SIZE);
      if (pid_nr)
	 memset (pid_stats[1][0], 0, PID_STATS_SIZE * pid_nr);
      if (file_hdr.sa_serial)
	 memset(st_serial[1], 0, STATS_SERIAL_SIZE * file_hdr.sa_serial);
      if (file_hdr.sa_irqcpu)
	 memset(st_irq_cpu[1], 0, STATS_IRQ_CPU_SIZE * (file_hdr.sa_proc + 1) * file_hdr.sa_irqcpu);
      if (file_hdr.sa_iface)
	 memset(st_net_dev[1], 0, STATS_NET_DEV_SIZE * file_hdr.sa_iface);
      if (file_hdr.sa_nr_disk)
	 memset(st_disk[1], 0, DISK_STATS_SIZE * file_hdr.sa_nr_disk);
	
      /* Display stats since boot time */
      write_stats(0, DISP_HDR, sar_actflag, USE_SADC, &count,
		  NO_TM_START, NO_TM_END, NO_RESET, ST_SINCE_BOOT);	/* First arg: !curr */
      exit(0);
   }

   /* Save the first stats collected. Will be used to compute the average */
   copy_structures(2, 0, USE_SADC);

   /* Main loop */
   do {

      /* Get stats */
      read_stat_bunch(curr);

      /* Print results */
      if (!dis_hdr) {
	 dis = lines / rows;
	 if (dis)
	    lines %= rows;
	 lines += more;
      }
      write_stats(curr, dis, sar_actflag, USE_SADC, &count, NO_TM_START, tm_end.use,
		  NO_RESET, ST_IMMEDIATE);
      fflush(stdout);	/* Don't buffer data if redirected to a pipe... */

      if (count > 0)
	 count--;
      if (count)
	 curr ^= 1;
   }
   while (count);

   /* Print statistics average */
   write_stats_avg(curr, dis_hdr, sar_actflag, USE_SADC);
}


/*
 * Main entry to the sar program
 */
int main(int argc, char **argv)
{
   int opt = 1, args_idx = 2;
   int i;
   int fd[2];
   unsigned long pid = 0;
   char from_file[MAX_FILE_LEN], to_file[MAX_FILE_LEN];
   char ltemp[20];
   char time_stamp[9];

   /* Compute page shift in kB */
   kb_shift = get_kb_shift();

   from_file[0] = to_file[0] = '\0';

#ifdef USE_NLS
   /* Init National Language Support */
   init_nls();
#endif

   tm_start.use = tm_end.use = 0;
   init_irq_bitmap(0);
   init_cpu_bitmap(0);
   init_stats();

   /* Process options */
   while (opt < argc) {

      if (!strcmp(argv[opt], "-I")) {
	 if (argv[++opt]) {
	    dis_hdr++;
	    if (!strcmp(argv[opt], K_SUM))
	       sar_actflag |= A_IRQ;
	    else if (!strcmp(argv[opt], K_PROC))
	       sar_actflag |= A_IRQ_CPU;
	    else {
	       sar_actflag |= A_ONE_IRQ;
	       if (!strcmp(argv[opt], K_ALL) || !strcmp(argv[opt], "-1")) {
		  dis_hdr = 9;
		  /* Set bit for the first 16 irq */
		  irq_bitmap[0] = 0x0000ffff;
	       }
	       else if (!strcmp(argv[opt], K_XALL) || !strcmp(argv[opt], "-2")) {
		  dis_hdr = 9;
		  /* Set every bit */
		  init_irq_bitmap(~0);
	       }
	       else {
		  /*
		   * Get irq number.
		   */
		  if (strspn(argv[opt], DIGITS) != strlen(argv[opt]))
		     usage(argv[0]);
		  i = atoi(argv[opt]);
		  if ((i < 0) || (i >= NR_IRQS))
		     usage(argv[0]);
		  irq_bitmap[i >> 5] |= 1 << (i & 0x1f);
	       }
	    }
	    opt++;
	 }
	 else
	    usage(argv[0]);
      }

      else if (!strcmp(argv[opt], "-U")) {
	 if (argv[++opt]) {
	    sar_actflag |= A_ONE_CPU;
	    dis_hdr++;
	    if (!strcmp(argv[opt], K_ALL) || !strcmp(argv[opt], "-1")) {
	       dis_hdr = 9;
	       /*
		* Set bit for every processor.
		* We still don't know if we are going to read stats
		* from a file or not...
		*/
	       init_cpu_bitmap(~0);
	       flags |= F_ALL_PROC;
	    }
	    else {
	       if (strspn(argv[opt], DIGITS) != strlen(argv[opt]))
		  usage(argv[0]);
	       i = atoi(argv[opt]);	/* Get cpu number */
	       /* Assume NR_CPUS <= 256... */
	       if ((i < 0) || (i >= NR_CPUS))
		  usage(argv[0]);
	       cpu_bitmap[i >> 5] |= 1 << (i & 0x1f);
	    }
	    opt++;
	 }
	 else
	    usage(argv[0]);
      }

      else if (!strcmp(argv[opt], "-o")) {
	 /* Save stats to a file */
	 if ((argv[++opt]) && strncmp(argv[opt], "-", 1) &&
	     (strspn(argv[opt], DIGITS) != strlen(argv[opt]))) {
	    strncpy(to_file, argv[opt++], MAX_FILE_LEN);
	    to_file[MAX_FILE_LEN - 1] = '\0';
	 }
	 else
	    strcpy(to_file, "-");
      }

      else if (!strcmp(argv[opt], "-f")) {
	 /* Read stats from a file */
	 if ((argv[++opt]) && strncmp(argv[opt], "-", 1) &&
	     (strspn(argv[opt], DIGITS) != strlen(argv[opt]))) {
	    strncpy(from_file, argv[opt++], MAX_FILE_LEN);
	    from_file[MAX_FILE_LEN - 1] = '\0';
	 }
	 else {
	    get_localtime(&loc_time);
	    snprintf(from_file, MAX_FILE_LEN, "%s/sa%02d", SA_DIR, loc_time.tm_mday);
	    from_file[MAX_FILE_LEN - 1] = '\0';
	 }
      }

      else if (!strcmp(argv[opt], "-s")) {
	 /* Get time start */
	 if ((argv[++opt]) && (strlen(argv[opt]) == 8))
	    strcpy(time_stamp, argv[opt++]);
	 else
	    strcpy(time_stamp, DEF_TMSTART);
	 decode_time_stamp(time_stamp, &tm_start);
      }

      else if (!strcmp(argv[opt], "-e")) {
	 /* Get time end */
	 if ((argv[++opt]) && (strlen(argv[opt]) == 8))
	    strcpy(time_stamp, argv[opt++]);
	 else
	    strcpy(time_stamp, DEF_TMEND);
	 decode_time_stamp(time_stamp, &tm_end);
      }

      else if (!strcmp(argv[opt], "-i")) {
	 if (!argv[++opt] || (strspn(argv[opt], DIGITS) != strlen(argv[opt])))
	    usage(argv[0]);
	 interval = atol(argv[opt++]);
	 if (interval < 1)
	   usage(argv[0]);
	 flags |= F_I_OPTION;
	 flags |= F_DEFAULT_COUNT;
      }

      else if (!strcmp(argv[opt], "-x") || !strcmp(argv[opt], "-X")) {
	 if (argv[++opt]) {
	    dis_hdr++;
	    if (!strcmp(argv[opt], K_SADC)) {
	       strcpy(ltemp, K_SELF);
	       opt++;
	    }
	    else if (!strcmp(argv[opt], K_SUM) && !strcmp(argv[opt - 1], "-x")) {
	       if (args_idx < MAX_ARGV_NR - 7) {
		  salloc(args_idx++, "-x");
		  salloc(args_idx++, K_SUM);
		  sar_actflag |= A_SUM_PID;
	       }
	       opt++;
	       continue;	/* Next option */
	    }
	    else if (!strcmp(argv[opt], K_ALL)) {
	       if (args_idx < MAX_ARGV_NR - 7) {
		  dis_hdr = 9;
		  salloc(args_idx++, argv[opt - 1]);	/* "-x" or "-X" */
		  salloc(args_idx++, K_ALL);
		  if (!strcmp(argv[opt - 1], "-x"))
		     sar_actflag |= A_PID;
		  else
		     sar_actflag |= A_CPID;
	       }
	       opt++;
	       continue;	/* Next option */
	    }
	    else {
	       if (!strcmp(argv[opt], K_SELF)) {
		  pid = getpid();
		  opt++;
	       }
	       else if (strspn(argv[opt], DIGITS) != strlen(argv[opt]))
		  usage(argv[0]);
	       else {
		  pid = atol(argv[opt++]);
		  if (pid < 1)
		     usage(argv[0]);
	       }
	       sprintf(ltemp, "%ld", pid);
	    }

	    if (args_idx < MAX_ARGV_NR - 7) {
	       if (!strcmp(argv[opt - 2], "-x"))
		  sar_actflag |= A_PID;
	       else
		  sar_actflag |= A_CPID;
	       salloc(args_idx++, argv[opt -2]);	/* "-x" or "-X" */
	       salloc(args_idx++, ltemp);
	    }
	 }
      }

      else if (!strcmp(argv[opt], "-n")) {
	 if (argv[++opt]) {
	    dis_hdr++;
	    if (!strcmp(argv[opt], K_DEV))
	       sar_actflag |= A_NET_DEV;
	    else if (!strcmp(argv[opt], K_EDEV))
	       sar_actflag |= A_NET_EDEV;
	    else if (!strcmp(argv[opt], K_SOCK))
	       sar_actflag |= A_NET_SOCK;
	    else if (!strcmp(argv[opt], K_FULL)) {
	       sar_actflag |= A_NET_DEV + A_NET_EDEV + A_NET_SOCK;
	       dis_hdr = 9;
	    }
	    opt++;
	 }
	 else
	    usage(argv[0]);
      }

      else if (!strncmp(argv[opt], "-", 1)) {
	 /* Other options not previously tested */
	 for (i = 1; *(argv[opt] + i); i++) {

	    switch (*(argv[opt] + i)) {

	     case 'A':
	       sar_actflag |= A_PROC + A_PAGE + A_IRQ + A_IO + A_CPU + A_CTXSW + A_SWAP +
		              A_MEMORY + A_SERIAL + A_MEM_AMT + A_IRQ_CPU + A_KTABLES +
		              A_NET_DEV + A_NET_EDEV + A_NET_SOCK + A_QUEUE + A_DISK;
	       flags |= F_A_OPTION;
	       break;
	     case 'B':
	       sar_actflag |= A_PAGE;
	       dis_hdr++;
	       break;
	     case 'b':
	       sar_actflag |= A_IO;
	       dis_hdr++;
	       break;
	     case 'c':
	       sar_actflag |= A_PROC;
	       dis_hdr++;
	       break;
	     case 'd':
	       sar_actflag |= A_DISK;
	       dis_hdr++;
	       break;
	     case 'H':
	       flags |= F_DB_OPTION;
	       break;
	     case 'h':
	       flags |= F_H_OPTION;
	       break;
	     case 'q':
	       sar_actflag |= A_QUEUE;
	       dis_hdr++;
	       break;
	     case 'r':
	       sar_actflag |= A_MEM_AMT;
	       dis_hdr++;
	       break;
	     case 'R':
	       sar_actflag |= A_MEMORY;
	       dis_hdr++;
	       break;
	     case 't':
	       flags |= F_ORG_TIME;
	       break;
	     case 'u':
	       sar_actflag |= A_CPU;
	       dis_hdr++;
	       break;
	     case 'v':
	       sar_actflag |= A_KTABLES;
	       dis_hdr++;
	       break;
	     case 'w':
	       sar_actflag |= A_CTXSW;
	       dis_hdr++;
	       break;
	     case 'W':
	       sar_actflag |= A_SWAP;
	       dis_hdr++;
	       break;
	     case 'y':
	       sar_actflag |= A_SERIAL;
	       dis_hdr++;
	       break;
	     case 'V':
	     default:
	       usage(argv[0]);
	    }
	 }
	 opt++;
      }

      else if (!interval) { 				/* Get interval */
	 if ((strspn(argv[opt], DIGITS) != strlen(argv[opt])) ||
	     WANT_BOOT_STATS(flags))
	    usage(argv[0]);
	 interval = atol(argv[opt++]);
	 if (!interval)
	    flags |= F_BOOT_STATS;
	 else if (interval < 0)
	   usage(argv[0]);
	 count = 1;	/* Default value for the count parameter is 1 */
	 flags |= F_DEFAULT_COUNT;
      }

      else {					/* Get count value */
	 if (strspn(argv[opt], DIGITS) != strlen(argv[opt]))
	    usage(argv[0]);
	 if (count && !USE_DEFAULT_COUNT(flags))
	    /* Count parameter already set */
	    usage(argv[0]);
	 count = atol(argv[opt++]);
	 if (count < 0)
	   usage(argv[0]);
	 else if (!count)
	    count = -1;	/* To generate a report continuously */
	 flags &= ~F_DEFAULT_COUNT;
      }
   }

   /* 'sar' is equivalent to 'sar -f' */
   if ((argc == 1) ||
       (!interval && !WANT_BOOT_STATS(flags) && !from_file[0] && !to_file[0])) {
      get_localtime(&loc_time);
      snprintf(from_file, MAX_FILE_LEN, "%s/sa%02d", SA_DIR, loc_time.tm_mday);
      from_file[MAX_FILE_LEN - 1] = '\0';
   }

   /*
    * Check option dependencies
    */
   /* You read from a file OR you write to it... */
   if (from_file[0] && to_file[0]) {
      fprintf(stderr, _("-f and -o options are mutually exclusive\n"));
      exit(1);
   }
   /* Use time start or options -i/-h only when reading stats from a file */
   if ((tm_start.use || (USE_I_OPTION(flags)) || USE_H_OPTION(flags) || USE_DB_OPTION(flags))
       && !from_file[0]) {
      fprintf(stderr, _("Not reading from a system activity file (use -f option)\n"));
      exit(1);
   }
   /* Don't print stats since boot time if -o or -f flags are used */
   if (WANT_BOOT_STATS(flags) && (from_file[0] || to_file[0]))
      usage(argv[0]);
   /*
    * Display all the contents of the daily data file if the count parameter
    * was not set on the command line.
    */
   if (USE_DEFAULT_COUNT(flags) && from_file[0])
      count = -1;

   /* -x and -X options ignored when writing to a file */
   if (to_file[0]) {
      sar_actflag &= ~A_PID;
      sar_actflag &= ~A_CPID;
      sar_actflag &= ~A_SUM_PID;
   }

   /* Default is CPU activity... */
   if (!sar_actflag) {
      /* Still OK even when reading stats from a file since A_CPU activity is always recorded */
      sar_actflag |= A_CPU;
      dis_hdr++;
   }

   /* ---Reading stats from file */
   if (from_file[0]) {
      if (!count)
	 count = -1;
      if (!interval)
	 interval = 1;

      /* If -A option is used, force A_ONE_CPU */
      if (USE_A_OPTION(flags)) {
	 sar_actflag |= A_ONE_CPU;
	 init_cpu_bitmap(~0);
	 flags |= F_ALL_PROC;
      }

      /* Read stats from file */
      read_stats_from_file(from_file);

      return 0;
   }

   /* ---Reading stats from sadc */

   /* Create anonymous pipe */
   if (pipe(fd) == -1) {
      perror("pipe");
      exit(4);
   }

   switch (fork()) {

    case -1:
      perror("fork");
      exit(4);
      break;

    case 0: /* Child */
      dup2(fd[1], STDOUT_FILENO);
      CLOSE_ALL(fd);

      /*
       * Prepare options for sadc
       */
      /* Program name */
      salloc(0, SADC);

      /* Interval value */
      if (!interval) {
	 if (WANT_BOOT_STATS(flags))
	    strcpy(ltemp, "1");
	 else
	    usage(argv[0]);
      }
      else
	 sprintf(ltemp, "%ld", interval);
      salloc(1, ltemp);

      /* Count number */
      if (count >= 0) {
	 sprintf(ltemp, "%ld", count + 1);
	 salloc(args_idx++, ltemp);
      }

      /* -I flag */
      if (GET_ONE_IRQ(sar_actflag))
	 salloc(args_idx++, "-I");

      /* Outfile arg */
      if (to_file[0])
	 salloc(args_idx++, to_file);

      /* Last arg is NULL */
      args[args_idx] = NULL;

      /* Call now the data collector */
      execv(SADC_PATH, args);
      execv(SADC_LOCAL_PATH, args);
      execvp(SADC, args);
      execv(SADC_ALT_PATH, args);
      /* Note: don't use execl/execlp since we don't have a fixed number of args to give to sadc */

      perror("exec");
      exit(4);
      break;

    default: /* Parent */
      dup2(fd[0], STDIN_FILENO);
      CLOSE_ALL(fd);

      /* Get now the statistics */
      read_stats();
      break;
   }
   return 0;
}
