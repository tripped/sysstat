/*
 * sadf: system activity data formatter
 * (C) 1999-2004 by Sebastien GODARD (sysstat <at> wanadoo.fr)
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
#include <time.h>
#include <errno.h>
#include <sys/param.h>	/* for HZ */

#include "version.h"
#include "sa.h"
#include "common.h"


#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif


long interval = 0, count = 0;
unsigned int sadf_actflag = 0;
unsigned int flags = 0;
unsigned char irq_bitmap[(NR_IRQS / 8) + 1];
unsigned char cpu_bitmap[(NR_CPUS / 8) + 1];
int kb_shift = 0;

struct file_hdr file_hdr;
struct file_stats file_stats[DIM];
struct stats_one_cpu *st_cpu[DIM];
struct stats_serial *st_serial[DIM];
struct stats_irq_cpu *st_irq_cpu[DIM];
struct stats_net_dev *st_net_dev[DIM];
struct disk_stats *st_disk[DIM];

/* Array members of common types are always packed */
unsigned int interrupts[DIM][NR_IRQS];

struct tm loc_time;
/* Contain the date specified by -s and -e options */
struct tstamp tm_start, tm_end;
char *args[MAX_ARGV_NR];


/*
 ***************************************************************************
 * Print usage and exit
 ***************************************************************************
 */
void usage(char *progname)
{
   fprintf(stderr, _("sysstat version %s\n"
		   "(C) Sebastien Godard\n"
	           "Usage: %s [ options... ] [ <interval> [ <count> ] ] [ <datafile> ]\n"
	           "Options are:\n"
	           "[ -d ] [ -p ] [ -t ]\n"
		   "[ -P { <cpu> | ALL } ] [ -s [ <hh:mm:ss> ] ] [ -e [ <hh:mm:ss> ] ]\n"
		   "[ -- <sar_options...> ]\n"),
	   VERSION, progname);
   exit(1);
}


/*
 ***************************************************************************
 * Set timestamp string
 ***************************************************************************
*/
void init_timestamp(short curr, char *cur_time, int len)
{
   struct tm *ltm;

   /* NOTE: loc_time structure must have been init'ed before! */
   if (PRINT_ORG_TIME(flags) && USE_DB_OPTION(flags))
      /* -d -t */
      ltm = localtime(&file_stats[curr].ust_time);
   else
      /* '-p' or '-p -t' or '-d' */
      ltm = gmtime(&file_stats[curr].ust_time);

   loc_time = *ltm;
   /*
    * NB: Option -t is ignored when option -h is used, since option -h
    * displays its timestamp as a long integer. This is type 'time_t',
    * which is the number of seconds since 1970 _always_ expressed in UTC.
    */
	
   if (!cur_time)
      /* Stop if cur_time is NULL */
      return;

   /* Set cur_time date value */
   if (USE_DB_OPTION(flags)) {
      if (PRINT_ORG_TIME(flags))
	 strftime(cur_time, len, "%Y-%m-%d %H:%M:%S", &loc_time);
      else
	 strftime(cur_time, len, "%Y-%m-%d %H:%M:%S UTC", &loc_time);
   }
}


/*
 ***************************************************************************
 * Print system statistics to be used by pattern processing commands
 ***************************************************************************
 */
void write_stats_for_ppc(short curr, unsigned int act, unsigned long dt,
			 unsigned long long itv, unsigned long long g_itv,
			 char *cur_time)
{
   int i, j, k;
   struct stats_one_cpu *st_cpu_i, *st_cpu_j;
   struct stats_serial *st_serial_i, *st_serial_j;
   struct stats_irq_cpu *p, *q, *p0, *q0;
   struct stats_net_dev *st_net_dev_i, *st_net_dev_j;
   struct disk_stats *st_disk_i, *st_disk_j;

   if (GET_PROC(act))
      printf("%s\t%ld\t%s\t-\tproc/s\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].processes, file_stats[curr].processes, itv));

   /* Print number of context switches per second */
   if (GET_CTXSW(act))
      printf("%s\t%ld\t%s\t-\tcswch/s\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     ll_s_value(file_stats[!curr].context_swtch, file_stats[curr].context_swtch, itv));

   /* Print CPU usage */
   if (GET_CPU(act) &&
       (!WANT_PER_PROC(flags) ||
	(WANT_PER_PROC(flags) && WANT_ALL_PROC(flags)))) {
      printf("%s\t%ld\t%s\tall\t%%user\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     ll_sp_value(file_stats[!curr].cpu_user, file_stats[curr].cpu_user, g_itv));
      printf("%s\t%ld\t%s\tall\t%%nice\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     ll_sp_value(file_stats[!curr].cpu_nice, file_stats[curr].cpu_nice, g_itv));
      printf("%s\t%ld\t%s\tall\t%%system\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     ll_sp_value(file_stats[!curr].cpu_system, file_stats[curr].cpu_system, g_itv));
      printf("%s\t%ld\t%s\tall\t%%iowait\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     ll_sp_value(file_stats[!curr].cpu_iowait, file_stats[curr].cpu_iowait, g_itv));
      printf("%s\t%ld\t%s\tall\t%%idle\t",
	     file_hdr.sa_nodename, dt, cur_time);
      if (file_stats[curr].cpu_idle < file_stats[!curr].cpu_idle)
	 printf("%.2f\n", 0.0);	/* Handle buggy RTC (or kernels?) */
      else
	 printf("%.2f\n",
		ll_sp_value(file_stats[!curr].cpu_idle, file_stats[curr].cpu_idle, g_itv));
   }

   if (GET_CPU(act) && WANT_PER_PROC(flags) && file_hdr.sa_proc) {
      unsigned long long pc_itv;

      for (i = 0; i <= file_hdr.sa_proc; i++) {
	 if (cpu_bitmap[i >> 3] & (1 << (i & 0x07))) {

	    st_cpu_i = st_cpu[curr]  + i;
	    st_cpu_j = st_cpu[!curr] + i;

	    /* Recalculate itv for current proc */
	    pc_itv = get_per_cpu_interval(st_cpu_i, st_cpu_j);
	
	    printf("%s\t%ld\t%s\tcpu%d\t%%user\t%.2f\n",
		   file_hdr.sa_nodename, dt, cur_time, i,
		   ll_sp_value(st_cpu_j->per_cpu_user, st_cpu_i->per_cpu_user, pc_itv));
	    printf("%s\t%ld\t%s\tcpu%d\t%%nice\t%.2f\n",
		   file_hdr.sa_nodename, dt, cur_time, i,
		   ll_sp_value(st_cpu_j->per_cpu_nice, st_cpu_i->per_cpu_nice, pc_itv));
	    printf("%s\t%ld\t%s\tcpu%d\t%%system\t%.2f\n",
		   file_hdr.sa_nodename, dt, cur_time, i,
		   ll_sp_value(st_cpu_j->per_cpu_system, st_cpu_i->per_cpu_system, pc_itv));
	    printf("%s\t%ld\t%s\tcpu%d\t%%iowait\t%.2f\n",
		   file_hdr.sa_nodename, dt, cur_time, i,
		   ll_sp_value(st_cpu_j->per_cpu_iowait, st_cpu_i->per_cpu_iowait, pc_itv));

	    if (st_cpu_i->per_cpu_idle < st_cpu_j->per_cpu_idle)
	       printf("%s\t%ld\t%s\tcpu%d\t%%idle\t%.2f\n",
		      file_hdr.sa_nodename, dt, cur_time, i, 0.0);
	    else
	       printf("%s\t%ld\t%s\tcpu%d\t%%idle\t%.2f\n",
		      file_hdr.sa_nodename, dt, cur_time, i,
		      ll_sp_value(st_cpu_j->per_cpu_idle, st_cpu_i->per_cpu_idle, pc_itv));
	 }
      }
   }

   /* Print number of interrupts per second */
   if (GET_IRQ(act) &&
       (!WANT_PER_PROC(flags) ||
	(WANT_PER_PROC(flags) && WANT_ALL_PROC(flags)))) {
      printf("%s\t%ld\t%s\tsum\tintr/s\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     ll_s_value(file_stats[!curr].irq_sum, file_stats[curr].irq_sum, itv));
   }

   if (GET_ONE_IRQ(act)) {
      for (i = 0; i < NR_IRQS; i++) {
	 if (irq_bitmap[i >> 3] & (1 << (i & 0x07))) {

	    printf("%s\t%ld\t%s\ti%03d\tintr/s\t%.2f\n",
		   file_hdr.sa_nodename, dt, cur_time, i,
		   S_VALUE(interrupts[!curr][i], interrupts[curr][i], itv));
	 }
      }
   }

   /* Print paging statistics */
   if (GET_PAGE(act)) {
      printf("%s\t%ld\t%s\t-\tpgpgin/s\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].pgpgin, file_stats[curr].pgpgin, itv));
      printf("%s\t%ld\t%s\t-\tpgpgout/s\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].pgpgout, file_stats[curr].pgpgout, itv));
      printf("%s\t%ld\t%s\t-\tfault/s\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].pgfault, file_stats[curr].pgfault, itv));
      printf("%s\t%ld\t%s\t-\tmajflt/s\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].pgmajfault, file_stats[curr].pgmajfault, itv));
   }

   /* Print number of swap pages brought in and out */
   if (GET_SWAP(act)) {
      printf("%s\t%ld\t%s\t-\tpswpin/s\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].pswpin, file_stats[curr].pswpin, itv));
      printf("%s\t%ld\t%s\t-\tpswpout/s\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].pswpout, file_stats[curr].pswpout, itv));
   }

   /* Print I/O stats (no distinction made between disks) */
   if (GET_IO(act)) {
      printf("%s\t%ld\t%s\t-\ttps\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].dk_drive, file_stats[curr].dk_drive, itv));
      printf("%s\t%ld\t%s\t-\trtps\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].dk_drive_rio, file_stats[curr].dk_drive_rio, itv));
      printf("%s\t%ld\t%s\t-\twtps\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].dk_drive_wio, file_stats[curr].dk_drive_wio, itv));
      printf("%s\t%ld\t%s\t-\tbread/s\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].dk_drive_rblk, file_stats[curr].dk_drive_rblk, itv));
      printf("%s\t%ld\t%s\t-\tbwrtn/s\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].dk_drive_wblk, file_stats[curr].dk_drive_wblk, itv));
   }

   /* Print memory stats */
   if (GET_MEMORY(act)) {
      printf("%s\t%ld\t%s\t-\tfrmpg/s\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     ((double) PG(file_stats[curr].frmkb) - (double) PG(file_stats[!curr].frmkb))
	     / itv * HZ);
      printf("%s\t%ld\t%s\t-\tbufpg/s\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     ((double) PG(file_stats[curr].bufkb) - (double) PG(file_stats[!curr].bufkb))
	     / itv * HZ);
      printf("%s\t%ld\t%s\t-\tcampg/s\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     ((double) PG(file_stats[curr].camkb) - (double) PG(file_stats[!curr].camkb))
	     / itv * HZ);
   }

   /* Print TTY statistics (serial lines) */
   if (GET_SERIAL(act)) {

      for (i = 0; i < file_hdr.sa_serial; i++) {

	 st_serial_i = st_serial[curr]  + i;
	 st_serial_j = st_serial[!curr] + i;
	 if (st_serial_i->line == ~0)
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
	     file_hdr.sa_nodename, dt, cur_time,
	     file_stats[curr].tlmkb - file_stats[curr].frmkb);
      printf("%s\t%ld\t%s\t-\t%%memused\t",
	     file_hdr.sa_nodename, dt, cur_time);
      if (file_stats[curr].tlmkb)
	 printf("%.2f\n",
		SP_VALUE(file_stats[curr].frmkb, file_stats[curr].tlmkb, file_stats[curr].tlmkb));
      else
	 printf("%.2f\n", 0.0);

      printf("%s\t%ld\t%s\t-\tkbbuffers\t%lu\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].bufkb);
      printf("%s\t%ld\t%s\t-\tkbcached\t%lu\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].camkb);
      printf("%s\t%ld\t%s\t-\tkbswpfree\t%lu\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].frskb);
      printf("%s\t%ld\t%s\t-\tkbswpused\t%lu\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     file_stats[curr].tlskb - file_stats[curr].frskb);
      printf("%s\t%ld\t%s\t-\t%%swpused\t",
	     file_hdr.sa_nodename, dt, cur_time);
      if (file_stats[curr].tlskb)
	 printf("%.2f\n",
		SP_VALUE(file_stats[curr].frskb, file_stats[curr].tlskb, file_stats[curr].tlskb));
      else
	 printf("%.2f\n", 0.0);
      printf("%s\t%ld\t%s\t-\tkbswpcad\t%lu\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].caskb);
   }

   if (GET_IRQ(act) && WANT_PER_PROC(flags) && file_hdr.sa_irqcpu) {
      int offset;

      for (k = 0; k <= file_hdr.sa_proc; k++) {
	 if (cpu_bitmap[k >> 3] & (1 << (k & 0x07))) {

	    for (j = 0; j < file_hdr.sa_irqcpu; j++) {
	       p0 = st_irq_cpu[curr] + j;	/* irq field set only for proc #0 */
	       /*
		* A value of ~0 means it is a remaining interrupt
		* which is no longer used, for example because the
		* number of interrupts has decreased in /proc/interrupts
		* or because we are appending data to an old sa file
		* with more interrupts than are actually available now.
		*/
	       if (p0->irq != ~0) {
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
   }

   /* Print values of some kernel tables */
   if (GET_KTABLES(act)) {
      printf("%s\t%ld\t%s\t-\tdentunusd\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].dentry_stat);
      printf("%s\t%ld\t%s\t-\tfile-sz\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].file_used);
      printf("%s\t%ld\t%s\t-\tinode-sz\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].inode_used);

      printf("%s\t%ld\t%s\t-\tsuper-sz\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].super_used);
      printf("%s\t%ld\t%s\t-\t%%super-sz\t",
	     file_hdr.sa_nodename, dt, cur_time);
      if (file_stats[curr].super_max)
	 printf("%.2f\n",
		((double) (file_stats[curr].super_used * 100)) / file_stats[curr].super_max);
      else
	 printf("%.2f\n", 0.0);

      printf("%s\t%ld\t%s\t-\tdquot-sz\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].dquot_used);
      printf("%s\t%ld\t%s\t-\t%%dquot-sz\t",
	     file_hdr.sa_nodename, dt, cur_time);
      if (file_stats[curr].dquot_max)
	 printf("%.2f\n",
		((double) (file_stats[curr].dquot_used * 100)) / file_stats[curr].dquot_max);
      else
	 printf("%.2f\n", 0.0);

      printf("%s\t%ld\t%s\t-\trtsig-sz\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].rtsig_queued);
      printf("%s\t%ld\t%s\t-\t%%rtsig-sz\t",
	     file_hdr.sa_nodename, dt, cur_time);
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
	 j = check_iface_reg(&file_hdr, st_net_dev, curr, !curr, i);
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
	 j = check_iface_reg(&file_hdr, st_net_dev, curr, !curr, i);
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
      printf("%s\t%ld\t%s\t-\trunq-sz\t%lu\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].nr_running);
      printf("%s\t%ld\t%s\t-\tplist-sz\t%u\n",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].nr_threads);
      printf("%s\t%ld\t%s\t-\tldavg-1\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     (double) file_stats[curr].load_avg_1 / 100);
      printf("%s\t%ld\t%s\t-\tldavg-5\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     (double) file_stats[curr].load_avg_5 / 100);
      printf("%s\t%ld\t%s\t-\tldavg-15\t%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     (double) file_stats[curr].load_avg_15 / 100);
   }

   /* Print disk statistics */
   if (GET_DISK(act)) {
      char *name;
      double tput, util, await, svctm, arqsz;

      for (i = 0; i < file_hdr.sa_nr_disk; i++) {

	 st_disk_i = st_disk[curr]  + i;
	 if (!(st_disk_i->major + st_disk_i->minor))
	    continue;

	 tput = ((double) st_disk_i->nr_ios) * HZ / itv;
	 util = ((double) st_disk_i->tot_ticks) / itv * HZ;
	 svctm = tput ? util / tput : 0.0;
	 await = st_disk_i->nr_ios ?
	    (st_disk_i->rd_ticks + st_disk_i->wr_ticks) / ((double) st_disk_i->nr_ios) : 0.0;
	 arqsz  = st_disk_i->nr_ios ?
	    (st_disk_i->rd_sect + st_disk_i->wr_sect) / ((double) st_disk_i->nr_ios) : 0.0;
	
	 j = check_disk_reg(&file_hdr, st_disk, curr, !curr, i);
	 st_disk_j = st_disk[!curr] + j;
	 name = get_devname(st_disk_i->major, st_disk_i->minor, flags);
	
	 printf("%s\t%ld\t%s\t%s\ttps\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, name,
		S_VALUE(st_disk_j->nr_ios, st_disk_i->nr_ios, itv));
	 printf("%s\t%ld\t%s\t%s\trd_sec/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, name,
		ll_s_value(st_disk_j->rd_sect, st_disk_i->rd_sect, itv));
	 printf("%s\t%ld\t%s\t%s\twr_sec/s\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, name,
		ll_s_value(st_disk_j->wr_sect, st_disk_i->wr_sect, itv));
	 printf("%s\t%ld\t%s\t%s\tavgrq-sz\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, name,
		arqsz);
	 printf("%s\t%ld\t%s\t%s\tavgqu-sz\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, name,
		((double) st_disk_i->rq_ticks) / itv * HZ / 1000.0);
	 printf("%s\t%ld\t%s\t%s\tawait\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, name,
		await);
	 printf("%s\t%ld\t%s\t%s\tsvctm\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, name,
		svctm);
	 printf("%s\t%ld\t%s\t%s\t%%util\t%.2f\n",
		file_hdr.sa_nodename, dt, cur_time, name,
		util / 10.0);
      }
   }
}


/*
 ***************************************************************************
 * Print system statistics to be used for loading into a database
 ***************************************************************************
 */
void write_stats_for_db(short curr, unsigned int act, unsigned long dt,
			unsigned long long itv, unsigned long long g_itv,
			char *cur_time)
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
	     ll_s_value(file_stats[!curr].context_swtch, file_stats[curr].context_swtch, itv));

   /* Print CPU usage */
   if (GET_CPU(act) &&
       (!WANT_PER_PROC(flags) ||
	(WANT_PER_PROC(flags) && WANT_ALL_PROC(flags)))) {
      printf("%s;%ld;%s;-1;%.2f;%.2f;%.2f;%.2f;",
	     file_hdr.sa_nodename, dt, cur_time,
	     ll_sp_value(file_stats[!curr].cpu_user, file_stats[curr].cpu_user, g_itv),
	     ll_sp_value(file_stats[!curr].cpu_nice, file_stats[curr].cpu_nice, g_itv),
	     ll_sp_value(file_stats[!curr].cpu_system, file_stats[curr].cpu_system, g_itv),
	     ll_sp_value(file_stats[!curr].cpu_iowait, file_stats[curr].cpu_iowait, g_itv));
      if (file_stats[curr].cpu_idle < file_stats[!curr].cpu_idle)
	 printf("%.2f\n", 0.0);	/* Handle buggy RTC (or kernels?) */
      else
	 printf("%.2f\n",
		ll_sp_value(file_stats[!curr].cpu_idle, file_stats[curr].cpu_idle, g_itv));
   }

   if (GET_CPU(act) && WANT_PER_PROC(flags) && file_hdr.sa_proc) {
      unsigned long long pc_itv;

      for (i = 0; i <= file_hdr.sa_proc; i++) {
	 if (cpu_bitmap[i >> 3] & (1 << (i & 0x07))) {

	    st_cpu_i = st_cpu[curr]  + i;
	    st_cpu_j = st_cpu[!curr] + i;

	    /* Recalculate itv for current proc */
	    pc_itv = get_per_cpu_interval(st_cpu_i, st_cpu_j);
	
	    printf("%s;%ld;%s;%d;%.2f;%.2f;%.2f;%.2f;",
		   file_hdr.sa_nodename, dt, cur_time, i,
		   ll_sp_value(st_cpu_j->per_cpu_user, st_cpu_i->per_cpu_user, pc_itv),
		   ll_sp_value(st_cpu_j->per_cpu_nice, st_cpu_i->per_cpu_nice, pc_itv),
		   ll_sp_value(st_cpu_j->per_cpu_system, st_cpu_i->per_cpu_system, pc_itv),
		   ll_sp_value(st_cpu_j->per_cpu_iowait, st_cpu_i->per_cpu_iowait, pc_itv));

	    if (st_cpu_i->per_cpu_idle < st_cpu_j->per_cpu_idle)
	       printf("%.2f\n", 0.0);
	    else
	       printf("%.2f\n",
		      ll_sp_value(st_cpu_j->per_cpu_idle, st_cpu_i->per_cpu_idle, pc_itv));
	 }
      }
   }

   /* Print number of interrupts per second */
   if (GET_IRQ(act) &&
       (!WANT_PER_PROC(flags) ||
	(WANT_PER_PROC(flags) && WANT_ALL_PROC(flags)))) {
      printf("%s;%ld;%s;-1;%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     ll_s_value(file_stats[!curr].irq_sum, file_stats[curr].irq_sum, itv));
   }

   if (GET_ONE_IRQ(act)) {
      for (i = 0; i < NR_IRQS; i++) {
	 if (irq_bitmap[i >> 3] & (1 << (i & 0x07))) {

	    printf("%s;%ld;%s;%d;%.2f\n",
		   file_hdr.sa_nodename, dt, cur_time, i,
		   S_VALUE(interrupts[!curr][i], interrupts[curr][i], itv));
	 }
      }
   }

   /* Print paging statistics */
   if (GET_PAGE(act))
      printf("%s;%ld;%s;%.2f;%.2f;%.2f;%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].pgpgin, file_stats[curr].pgpgin, itv),
	     S_VALUE(file_stats[!curr].pgpgout, file_stats[curr].pgpgout, itv),
	     S_VALUE(file_stats[!curr].pgfault, file_stats[curr].pgfault, itv),
	     S_VALUE(file_stats[!curr].pgmajfault, file_stats[curr].pgmajfault, itv));

   /* Print number of swap pages brought in and out */
   if (GET_SWAP(act))
      printf("%s;%ld;%s;%.2f;%.2f\n", file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].pswpin, file_stats[curr].pswpin, itv),
	     S_VALUE(file_stats[!curr].pswpout, file_stats[curr].pswpout, itv));

   /* Print I/O stats (no distinction made between disks) */
   if (GET_IO(act))
      printf("%s;%ld;%s;%.2f;%.2f;%.2f;%.2f;%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     S_VALUE(file_stats[!curr].dk_drive, file_stats[curr].dk_drive, itv),
	     S_VALUE(file_stats[!curr].dk_drive_rio, file_stats[curr].dk_drive_rio, itv),
	     S_VALUE(file_stats[!curr].dk_drive_wio, file_stats[curr].dk_drive_wio, itv),
	     S_VALUE(file_stats[!curr].dk_drive_rblk, file_stats[curr].dk_drive_rblk, itv),
	     S_VALUE(file_stats[!curr].dk_drive_wblk, file_stats[curr].dk_drive_wblk, itv));

   /* Print memory stats */
   if (GET_MEMORY(act))
      printf("%s;%ld;%s;%.2f;%.2f;%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     ((double) PG(file_stats[curr].frmkb) - (double) PG(file_stats[!curr].frmkb))
	     / itv * HZ,
	     ((double) PG(file_stats[curr].bufkb) - (double) PG(file_stats[!curr].bufkb))
	     / itv * HZ,
	     ((double) PG(file_stats[curr].camkb) - (double) PG(file_stats[!curr].camkb))
	     / itv * HZ);

   /* Print TTY statistics (serial lines) */
   if (GET_SERIAL(act)) {

      for (i = 0; i < file_hdr.sa_serial; i++) {

	 st_serial_i = st_serial[curr]  + i;
	 st_serial_j = st_serial[!curr] + i;
	 if (st_serial_i->line == ~0)
	    continue;
	
	 if (st_serial_i->line == st_serial_j->line) {
	    printf("%s;%ld;%s;%d;%.2f;%.2f\n",
		   file_hdr.sa_nodename, dt, cur_time, st_serial_i->line,
		   S_VALUE(st_serial_j->rx, st_serial_i->rx, itv),
		   S_VALUE(st_serial_j->tx, st_serial_i->tx, itv));
	 }
      }
   }

   /* Print amount and usage of memory */
   if (GET_MEM_AMT(act)) {
      printf("%s;%ld;%s;%lu;%lu;",
	     file_hdr.sa_nodename, dt, cur_time, file_stats[curr].frmkb,
	     file_stats[curr].tlmkb - file_stats[curr].frmkb);
      if (file_stats[curr].tlmkb)
	 printf("%.2f",
		SP_VALUE(file_stats[curr].frmkb, file_stats[curr].tlmkb, file_stats[curr].tlmkb));
      else
	 printf("%.2f", 0.0);
      printf(";%lu;%lu;%lu;%lu;",
	     file_stats[curr].bufkb, file_stats[curr].camkb,
	     file_stats[curr].frskb,
	     file_stats[curr].tlskb - file_stats[curr].frskb);
      if (file_stats[curr].tlskb)
	 printf("%.2f",
		SP_VALUE(file_stats[curr].frskb, file_stats[curr].tlskb, file_stats[curr].tlskb));
      else
	 printf("%.2f", 0.0);
      printf(";%lu\n", file_stats[curr].caskb);
   }

   if (GET_IRQ(act) && WANT_PER_PROC(flags) && file_hdr.sa_irqcpu) {
      int offset;

      for (k = 0; k <= file_hdr.sa_proc; k++) {
	 if (cpu_bitmap[k >> 3] & (1 << (k & 0x07))) {

	    for (j = 0; j < file_hdr.sa_irqcpu; j++) {
	       p0 = st_irq_cpu[curr] + j;	/* irq field set only for proc #0 */
	       /*
		* A value of ~0 means it is a remaining interrupt
		* which is no longer used, for example because the
		* number of interrupts has decreased in /proc/interrupts
		* or because we are appending data to an old sa file
		* with more interrupts than are actually available now.
		*/
	       if (p0->irq != ~0) {
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
		     printf("%s;%ld;%s;%d;%d;%.2f\n",
			    file_hdr.sa_nodename, dt, cur_time, k, p0->irq,
			    S_VALUE(q->interrupt, p->interrupt, itv));
		  }
	       }
	    }
	 }
      }
   }

   /* Print values of some kernel tables */
   if (GET_KTABLES(act)) {
      printf("%s;%ld;%s;%u;%u;%u;%u;", file_hdr.sa_nodename, dt, cur_time,
	     file_stats[curr].dentry_stat, file_stats[curr].file_used,
	     file_stats[curr].inode_used, file_stats[curr].super_used);
      if (file_stats[curr].super_max)
	 printf("%.2f",
		((double) (file_stats[curr].super_used * 100))
		/ file_stats[curr].super_max);
      else
	 printf("%.2f", 0.0);
      printf(";%u;", file_stats[curr].dquot_used);
      if (file_stats[curr].dquot_max)
	 printf("%.2f",
		((double) (file_stats[curr].dquot_used * 100))
		/ file_stats[curr].dquot_max);
      else
	 printf("%.2f", 0.0);
      printf(";%u;", file_stats[curr].rtsig_queued);
      if (file_stats[curr].rtsig_max)
	 printf("%.2f\n",
		((double) (file_stats[curr].rtsig_queued * 100))
		/ file_stats[curr].rtsig_max);
      else
	 printf("%.2f\n", 0.0);
   }

   /* Print network interface statistics */
   if (GET_NET_DEV(act)) {

      for (i = 0; i < file_hdr.sa_iface; i++) {

	 st_net_dev_i = st_net_dev[curr] + i;
	 if (!strcmp(st_net_dev_i->interface, "?"))
	    continue;
	 j = check_iface_reg(&file_hdr, st_net_dev, curr, !curr, i);
	 st_net_dev_j = st_net_dev[!curr] + j;

	 printf("%s;%ld;%s;%s;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f\n",
		file_hdr.sa_nodename, dt, cur_time,
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
	 j = check_iface_reg(&file_hdr, st_net_dev, curr, !curr, i);
	 st_net_dev_j = st_net_dev[!curr] + j;

	 printf("%s;%ld;%s;%s;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f\n",
		file_hdr.sa_nodename, dt, cur_time,
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
      printf("%s;%ld;%s;%u;%u;%u;%u;%u\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     file_stats[curr].sock_inuse, file_stats[curr].tcp_inuse,
	     file_stats[curr].udp_inuse, file_stats[curr].raw_inuse,
	     file_stats[curr].frag_inuse);

   /* Print load averages and queue length */
   if (GET_QUEUE(act))
      printf("%s;%ld;%s;%lu;%u;%.2f;%.2f;%.2f\n",
	     file_hdr.sa_nodename, dt, cur_time,
	     file_stats[curr].nr_running, file_stats[curr].nr_threads,
	     (double) file_stats[curr].load_avg_1  / 100,
	     (double) file_stats[curr].load_avg_5  / 100,
	     (double) file_stats[curr].load_avg_15 / 100);

   /* Print disk statistics */
   if (GET_DISK(act)) {
      double tput, util, await, svctm, arqsz;

      for (i = 0; i < file_hdr.sa_nr_disk; i++) {

	 st_disk_i = st_disk[curr]  + i;
	 if (!(st_disk_i->major + st_disk_i->minor))
	    continue;

	 tput = ((double) st_disk_i->nr_ios) * HZ / itv;
	 util = ((double) st_disk_i->tot_ticks) / itv * HZ;
	 svctm = tput ? util / tput : 0.0;
	 await = st_disk_i->nr_ios ?
	    (st_disk_i->rd_ticks + st_disk_i->wr_ticks) / ((double) st_disk_i->nr_ios) : 0.0;
	 arqsz  = st_disk_i->nr_ios ?
	    (st_disk_i->rd_sect + st_disk_i->wr_sect) / ((double) st_disk_i->nr_ios) : 0.0;
	
	 j = check_disk_reg(&file_hdr, st_disk, curr, !curr, i);
	 st_disk_j = st_disk[!curr] + j;
	
	 printf("%s;%ld;%s;%s;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f\n",
		file_hdr.sa_nodename, dt, cur_time,
		get_devname(st_disk_i->major, st_disk_i->minor, flags),
		S_VALUE(st_disk_j->nr_ios, st_disk_i->nr_ios, itv),
		ll_s_value(st_disk_j->rd_sect, st_disk_i->rd_sect, itv),
		ll_s_value(st_disk_j->wr_sect, st_disk_i->wr_sect, itv),
		arqsz,
		((double) st_disk_i->rq_ticks) / itv * HZ / 1000.0,
		await,
		svctm,
		util / 10.0);
      }
   }
}


/*
 ***************************************************************************
 * Write system statistics
 ***************************************************************************
 */
int write_parsable_stats(short curr, unsigned int act, int reset, long *cnt,
			 int use_tm_start, int use_tm_end)
{
   unsigned long long dt, itv, g_itv;
   char cur_time[26];

   /* Check time (1) */
   if (!next_slice(file_stats[2].uptime, file_stats[curr].uptime, &file_hdr,
		   reset, interval))
      /* Not close enough to desired interval */
      return 0;

   /* Get current timestamp */
   init_timestamp(curr, cur_time, 26);
   if (USE_PPC_OPTION(flags))
      sprintf(cur_time, "%ld", file_stats[curr].ust_time);

   /* Check time */
   if (prep_time(&file_stats[curr], &file_stats[!curr], &file_hdr, &loc_time,
		 &tm_start, use_tm_start, &itv, &g_itv))
      /* It's too soon... */
      return 0;
   if (use_tm_end && (datecmp(&loc_time, &tm_end) > 0)) {
      /* It's too late... */
      *cnt = 0;
      return 0;
   }

   dt = itv / HZ;
   /* Correct rounding error for dt */
   if ((itv % HZ) >= (HZ / 2))
      dt++;

   if (USE_PPC_OPTION(flags))
      /* Write stats to be used by pattern processing commands */
      write_stats_for_ppc(curr, act, dt, itv, g_itv, cur_time);
   else if (USE_DB_OPTION(flags))
      /* Write stats to be used for loading into a database */
      write_stats_for_db(curr, act, dt, itv, g_itv, cur_time);

   return 1;
}


/*
 ***************************************************************************
 * Print a Linux restart message (contents of a DUMMY record)
 ***************************************************************************
 */
void write_dummy(short curr, int use_tm_start, int use_tm_end)
{
   char cur_time[26];

   init_timestamp(curr, cur_time, 26);

   /* The RESTART message must be in the interval specified by -s/-e options */
   if ((use_tm_start && (datecmp(&loc_time, &tm_start) < 0)) ||
       (use_tm_end && (datecmp(&loc_time, &tm_end) > 0)))
      return;

   if (USE_PPC_OPTION(flags))
      printf("%s\t-1\t%ld\tLINUX-RESTART\n",
	     file_hdr.sa_nodename, file_stats[curr].ust_time);
   else if (USE_DB_OPTION(flags))
      printf("%s;-1;%s;LINUX-RESTART\n",
	     file_hdr.sa_nodename, cur_time);
}


/*
 ***************************************************************************
 * Allocate structures
 ***************************************************************************
 */
void allocate_structures(int stype)
{
   if (file_hdr.sa_proc > 0)
      salloc_cpu_array(st_cpu, file_hdr.sa_proc + 1);
   if (file_hdr.sa_serial)
      salloc_serial_array(st_serial, file_hdr.sa_serial);
   if (file_hdr.sa_irqcpu)
      salloc_irqcpu_array(st_irq_cpu, file_hdr.sa_proc + 1,
			  file_hdr.sa_irqcpu);
   if (file_hdr.sa_iface)
      salloc_net_dev_array(st_net_dev, file_hdr.sa_iface);
   if (file_hdr.sa_nr_disk)
      salloc_disk_array(st_disk, file_hdr.sa_nr_disk);

   /* Print report header */
   print_report_hdr(flags, &loc_time, &file_hdr);
}


/*
 ***************************************************************************
 * Move structures data
 ***************************************************************************
 */
void copy_structures(int dest, int src)
{
   memcpy(&file_stats[dest], &file_stats[src], FILE_STATS_SIZE);
   if (file_hdr.sa_proc > 0)
      memcpy(st_cpu[dest], st_cpu[src],
	     STATS_ONE_CPU_SIZE * (file_hdr.sa_proc + 1));
   if (GET_ONE_IRQ(file_hdr.sa_actflag))
      memcpy(interrupts[dest], interrupts[src],
	     STATS_ONE_IRQ_SIZE);
   if (file_hdr.sa_serial)
      memcpy(st_serial[dest], st_serial[src],
	     STATS_SERIAL_SIZE * file_hdr.sa_serial);
   if (file_hdr.sa_irqcpu)
      memcpy(st_irq_cpu[dest], st_irq_cpu[src],
	     STATS_IRQ_CPU_SIZE * (file_hdr.sa_proc + 1) * file_hdr.sa_irqcpu);
   if (file_hdr.sa_iface)
      memcpy(st_net_dev[dest], st_net_dev[src],
	     STATS_NET_DEV_SIZE * file_hdr.sa_iface);
   if (file_hdr.sa_nr_disk)
      memcpy(st_disk[dest], st_disk[src],
	     DISK_STATS_SIZE * file_hdr.sa_nr_disk);
}


/*
 ***************************************************************************
 * Read varying part of the statistics from a daily data file
 ***************************************************************************
 */
void read_extra_stats(short curr, int ifd)
{
   if (file_hdr.sa_proc > 0)
      sa_fread(ifd, st_cpu[curr],
	       STATS_ONE_CPU_SIZE * (file_hdr.sa_proc + 1), HARD_SIZE);
   if (GET_ONE_IRQ(file_hdr.sa_actflag))
      sa_fread(ifd, interrupts[curr],
	       STATS_ONE_IRQ_SIZE, HARD_SIZE);
   if (file_hdr.sa_serial)
      sa_fread(ifd, st_serial[curr],
	       STATS_SERIAL_SIZE * file_hdr.sa_serial, HARD_SIZE);
   if (file_hdr.sa_irqcpu)
      sa_fread(ifd, st_irq_cpu[curr],
	       STATS_IRQ_CPU_SIZE * (file_hdr.sa_proc + 1) * file_hdr.sa_irqcpu, HARD_SIZE);
   if (file_hdr.sa_iface)
      sa_fread(ifd, st_net_dev[curr],
	       STATS_NET_DEV_SIZE * file_hdr.sa_iface, HARD_SIZE);
   if (file_hdr.sa_nr_disk)
      sa_fread(ifd, st_disk[curr],
	       DISK_STATS_SIZE * file_hdr.sa_nr_disk, HARD_SIZE);
   /* PID stats cannot be saved in file. So we don't read them */
}


/*
 ***************************************************************************
 * Read stats for current activity from file
 ***************************************************************************
 */
void read_curr_act_stats(int ifd, off_t fpos, short *curr, long *cnt, int *eosaf,
			 unsigned int act, int *reset)
{
   unsigned long lines;
   unsigned char rtype;
   int davg, next;
   off_t fps;

   if ((fps = lseek(ifd, fpos, SEEK_SET)) < fpos) {
      perror("lseek");
      exit(2);
   }

   /*
    * Restore the first stats collected.
    * Used to compute the rate displayed on the first line.
    */
   copy_structures(!(*curr), 2);
	
   lines = 0;
   davg  = 0;
   *cnt  = count;

   do {
      /* Display count lines of stats */
      *eosaf = sa_fread(ifd, &file_stats[*curr],
			file_hdr.sa_st_size, SOFT_SIZE);
      rtype = file_stats[*curr].record_type;
	
      if (!(*eosaf) && (rtype != R_DUMMY))
	 /* Read the extra fields since it's not a DUMMY record */
	 read_extra_stats(*curr, ifd);

      if (!(*eosaf) && (rtype != R_DUMMY)) {

	 /* next is set to 1 when we were close enough to desired interval */
	 next = write_parsable_stats(*curr, act, *reset, cnt,
				     tm_start.use, tm_end.use);
	 if (next && ((*cnt) > 0))
	    (*cnt)--;
	 if (next) {
	    davg++;
	    *curr ^=1;
	 }
	 else
	    lines--;
	 *reset = 0;
      }
   }
   while ((*cnt) && !(*eosaf) && (rtype != R_DUMMY));

   *reset = TRUE;
}


/*
 ***************************************************************************
 * Read statistics from a system activity data file
 ***************************************************************************
 */
void read_stats_from_file(char dfile[])
{
   short curr = 1;
   unsigned int act;
   int ifd;
   int eosaf = TRUE, reset = FALSE;
   long cnt = 1;
   off_t fpos;

   /* Prepare file for reading */
   prep_file_for_reading(&ifd, dfile, &file_hdr, &sadf_actflag, flags);

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
	    write_dummy(0, tm_start.use, tm_end.use);
	 else {
	    /*
	     * Ok: previous record was not a DUMMY one.
	     * So read now the extra fields.
	     */
	    read_extra_stats(0, ifd);

	    init_timestamp(0, NULL, 0);
	 }
      }
      while ((file_stats[0].record_type == R_DUMMY) ||
	     (tm_start.use && (datecmp(&loc_time, &tm_start) < 0)) ||
	     (tm_end.use && (datecmp(&loc_time, &tm_end) >=0)));

      /* Save the first stats collected. Will be used to compute the average */
      copy_structures(2, 0);

      reset = TRUE;	/* Set flag to reset last_uptime variable */

      /* Save current file position */
      if ((fpos = lseek(ifd, 0, SEEK_CUR)) < 0) {
	 perror("lseek");
	 exit(2);
      }

      /* Read and write stats located between two possible Linux restarts */

      /* For each requested activity... */
      for (act = 1; act <= A_LAST; act <<= 1) {

	 if (sadf_actflag & act) {
	    if ((act == A_IRQ) && WANT_PER_PROC(flags) && WANT_ALL_PROC(flags)) {
	       /* Distinguish -I SUM activity from IRQs per processor activity */
	       flags &= ~F_PER_PROC;
	       read_curr_act_stats(ifd, fpos, &curr, &cnt, &eosaf, act, &reset);
	       flags |= F_PER_PROC;
	       flags &= ~F_ALL_PROC;
	       read_curr_act_stats(ifd, fpos, &curr, &cnt, &eosaf, act, &reset);
	       flags |= F_ALL_PROC;
	    }
	    else
	       read_curr_act_stats(ifd, fpos, &curr, &cnt, &eosaf, act, &reset);
	 }
      }

      if (!cnt) {
	 /* Go to next Linux restart, if possible */
	 do {
	    eosaf = sa_fread(ifd, &file_stats[curr],
			     file_hdr.sa_st_size, SOFT_SIZE);
	    if (!eosaf && (file_stats[curr].record_type != R_DUMMY))
	       read_extra_stats(curr, ifd);
	 }
	 while (!eosaf && (file_stats[curr].record_type != R_DUMMY));
      }

      /* The last record we read was a DUMMY one: print it */
      if (!eosaf && (file_stats[curr].record_type == R_DUMMY))
	 write_dummy(curr, tm_start.use, tm_end.use);
   }
   while (!eosaf);

   close(ifd);
}


/*
 ***************************************************************************
 * Main entry to the sadf program
 ***************************************************************************
 */
int main(int argc, char **argv)
{
   int opt = 1, sar_options = 0;
   int i;
   char dfile[MAX_FILE_LEN];
   short dum;

   /* Compute page shift in kB */
   kb_shift = get_kb_shift();

   dfile[0] = '\0';

#ifdef USE_NLS
   /* Init National Language Support */
   init_nls();
#endif

   tm_start.use = tm_end.use = FALSE;
   init_bitmap(irq_bitmap, 0, NR_IRQS);
   init_bitmap(cpu_bitmap, 0, NR_CPUS);
   init_stats(file_stats, interrupts);

   /* Process options */
   while (opt < argc) {

      if (!strcmp(argv[opt], "-I")) {
	 if (argv[++opt] && sar_options) {
	    if (parse_sar_I_opt(argv, &opt, &sadf_actflag, &dum,
				irq_bitmap))
	       usage(argv[0]);
	 }
	 else
	    usage(argv[0]);
      }

      else if (!strcmp(argv[opt], "-P")) {
	 if (parse_sa_P_opt(argv, &opt, &flags, &dum, cpu_bitmap))
	    usage(argv[0]);
      }

      else if (!strcmp(argv[opt], "-s")) {
	 /* Get time start */
	 if (parse_timestamp(argv, &opt, &tm_start, DEF_TMSTART))
	    usage(argv[0]);
      }

      else if (!strcmp(argv[opt], "-e")) {
	 /* Get time end */
	 if (parse_timestamp(argv, &opt, &tm_end, DEF_TMEND))
	    usage(argv[0]);
      }

      else if (!strcmp(argv[opt], "--")) {
	 sar_options = 1;
	 opt++;
      }

      else if (!strcmp(argv[opt], "-n")) {
	 if (argv[++opt] && sar_options) {
	    /* Parse sar's option -n */
	    if (parse_sar_n_opt(argv, &opt, &sadf_actflag, &dum))
	       usage(argv[0]);
	 }
	 else
	    usage(argv[0]);
      }

      else if (!strncmp(argv[opt], "-", 1)) {
	 /* Other options not previously tested */
	 if (sar_options) {
	    if (parse_sar_opt(argv, opt, &sadf_actflag, &flags, &dum, C_SADF))
	       usage(argv[0]);
	 }
	 else {

	    for (i = 1; *(argv[opt] + i); i++) {

	       switch (*(argv[opt] + i)) {
	
		case 'd':
		  flags |= F_DB_OPTION;
		  break;
		case 'p':
		  flags |= F_PPC_OPTION;
	       break;
		case 't':
		  flags |= F_ORG_TIME;
	       break;
		case 'V':
		default:
		  usage(argv[0]);
	       }
	    }
	 }
	 opt++;
      }
	
      /* Get data file name */
      else if (strspn(argv[opt], DIGITS) != strlen(argv[opt])) {
	 if (!dfile[0]) {
	    if (!strcmp(argv[opt], "-")) {
	       /* File name set to '-' */
	       get_localtime(&loc_time);
	       snprintf(dfile, MAX_FILE_LEN,
			"%s/sa%02d", SA_DIR, loc_time.tm_mday);
	       dfile[MAX_FILE_LEN - 1] = '\0';
	       flags |= F_SA_ROTAT;
	       opt++;
	    }
	    else if (!strncmp(argv[opt], "-", 1))
	       /* Bad option */
	       usage(argv[0]);
	    else {
	       /* Write data to file */
	       strncpy(dfile, argv[opt++], MAX_FILE_LEN);
	       dfile[MAX_FILE_LEN - 1] = '\0';
	    }
	 }
	 else
	    /* File already specified */
	    usage(argv[0]);
      }

      else if (!interval) { 				/* Get interval */
	 if (strspn(argv[opt], DIGITS) != strlen(argv[opt]))
	    usage(argv[0]);
	 interval = atol(argv[opt++]);
	 if (interval <= 0)
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

   /* sadf reads current daily data file by default */
   if (!dfile[0]) {
      get_localtime(&loc_time);
      snprintf(dfile, MAX_FILE_LEN,
	       "%s/sa%02d", SA_DIR, loc_time.tm_mday);
      dfile[MAX_FILE_LEN - 1] = '\0';
   }

   /*
    * Display all the contents of the daily data file if the count parameter
    * was not set on the command line.
    */
   if (USE_DEFAULT_COUNT(flags))
      count = -1;

   /* Default is CPU activity and PPC display */
   if (!sadf_actflag)
      sadf_actflag |= A_CPU;
   if (!USE_DB_OPTION(flags) && !USE_PPC_OPTION(flags))
      flags |= F_PPC_OPTION;

   if (!count)
      count = -1;
   if (!interval)
      interval = 1;

   /* If -A option is used, force '-P ALL' */
   if (USE_A_OPTION(flags)) {
      init_bitmap(cpu_bitmap, ~0, NR_CPUS);
      flags |= F_ALL_PROC + F_PER_PROC;
   }

   /* Read stats from file */
   read_stats_from_file(dfile);

   return 0;
}
