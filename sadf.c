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
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/param.h>	/* for HZ */

#include "version.h"
#include "sadf.h"
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
unsigned int sadf_flags = 0;
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
	           "[ -d ] [ -H ] [ -p ] [ -t ]\n"
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
 * cons() -
 *   encapsulate a pair of ints or pair of char * into a static Cons and
 *   return a pointer to it.
 *
 * given:   t - type of Cons {iv, sv}
 *	    arg1 - unsigned long int (if iv), char * (if sv) to become
 *		   element 'a'
 *	    arg2 - unsigned long int (if iv), char * (if sv) to become
 *		   element 'b'
 *
 * does:    load a static Cons with values using the t parameter to
 *	    guide pulling values from the arglist
 *
 * return:  the address of it's static Cons.  If you need to keep
 *	    the contents of this Cons, copy it somewhere before calling
 *	    cons() against to avoid overwrite.
 *	    ie. don't do this:  f( cons( iv, i, j ), cons( iv, a, b ) );
 ***************************************************************************
 */

static Cons *cons(tcons t, ...)
{
   va_list ap;
   static Cons c;

   c.t = t;

   va_start(ap, t);
   if (t == iv) {
      c.a.i = va_arg(ap, unsigned long int);
      c.b.i = va_arg(ap, unsigned long int);
   }
   else {
      c.a.s = va_arg(ap, char *);
      c.b.s = va_arg(ap, char *);
   }
   va_end(ap);
   return(&c);
}


/*
 ***************************************************************************
 * render():
 *
 * given:    isdb - flag, true if db printing, false if ppc printing
 *	     pre  - prefix string for output entries
 *	     rflags - PT_.... rendering flags
 *	     pptxt - printf-format text required for ppc output (may be null)
 *	     dbtxt - printf-format text required for db output (may be null)
 *	     mid - pptxt/dbtxt format args as a Cons.
 *	     luval - %lu printable arg (PT_USEINT must be set)
 *	     dval  - %.2f printable arg (used unless PT_USEINT is set)
 *
 * does:     print [pre<sep>]([dbtxt,arg,arg<sep>]|[pptxt,arg,arg<sep>]) \
 *                     (luval|dval)(<sep>|\n)
 *
 * return:   void.
 ***************************************************************************
 */

static void render(int isdb, char *pre, int rflags, const char *pptxt,
		   const char *dbtxt, Cons *mid, unsigned long int luval,
		   double dval)
{
   static int newline = 1;
   const char *txt[]  = {pptxt, dbtxt};
   char *sep;

   /* Start a new line? */
   if (newline)
      printf("%s%s", pre, seps[isdb]);

   /* Terminate this one ? ppc always gets a newline */
   newline = ((rflags & PT_NEWLIN) || !isdb);

   if (txt[isdb]) {		/* pp/dbtxt? */

      if (mid) {		/* Got format args? */
	 switch(mid->t) {
	  case iv:
	    printf(txt[isdb], mid->a.i, mid->b.i);
	    break;
	  case sv:
	    printf(txt[isdb], mid->a.s, mid->b.s);
	    break;
	 }
      }
      else {
	 printf(txt[isdb]);	/* No args */
      }
      printf("%s", seps[isdb]);	/* Only if something actually got printed */
   }

   sep = (newline) ? "\n" : seps[isdb]; /* How does this rendering end? */

   if (rflags & PT_USEINT) {
      printf("%lu%s", luval, sep);
   }
   else {
      printf("%.2f%s", dval, sep);
   }
}


/*
 ***************************************************************************
 * write_mech_stats() -
 * Replace the old write_stats_for_ppc() and write_stats_for_db(),
 * making it easier for them to remain in sync and print the same data.
 ***************************************************************************
 */

void write_mech_stats(int isdb, short curr, unsigned int act,
		      unsigned long dt, unsigned long long itv,
		      unsigned long long g_itv, char *cur_time)
{
   struct file_stats
      *fsi = &file_stats[curr],
      *fsj = &file_stats[!curr];

   char pre[80];	/* Text at beginning of each line */
   int wantproc = !WANT_PER_PROC(flags)
      || (WANT_PER_PROC(flags) && WANT_ALL_PROC(flags));


   /*
    * This substring appears on every output line, preformat it here
    */
   snprintf(pre, 80, "%s%s%ld%s%s",
	    file_hdr.sa_nodename, seps[isdb], dt, seps[isdb], cur_time);


   if (GET_PROC(act)) {
      /* The first one as an example */
      render(isdb,		/* db/ppc flag */
	     pre,		/* the preformatted line leader */
	     PT_NEWLIN,		/* is this the end of a db line? */
	     "-\tproc/s",	/* ppc text */
	     NULL,		/* db text */
	     NULL,		/* db/ppc text format args (Cons *) */
	     NOVAL,		/* %lu value (unused unless PT_USEINT) */
	     /* and %.2f value, used unless PT_USEINT */
	     S_VALUE(fsj->processes, fsi->processes, itv));
   }

   if (GET_CTXSW(act)) {
      render(isdb, pre, PT_NEWLIN,
	     "-\tcswch/s", NULL, NULL,
	     NOVAL,
	     ll_sp_value(fsj->context_swtch, fsi->context_swtch, itv));
   }


   if (GET_CPU(act) && wantproc) {
      render(isdb, pre,
	     PT_NOFLAG,		/* that's zero but you know what it means */
	     "all\t%%user",	/* all ppctext is used as format, thus '%%' */
	     "-1",		/* look! dbtext */
	     NULL,		/* no args */
	     NOVAL,		/* another 0, named for readability */
	     ll_sp_value(fsj->cpu_user, fsi->cpu_user, g_itv));

      render(isdb, pre, PT_NOFLAG,
	     "all\t%%nice", NULL, NULL,
	     NOVAL,
	     ll_sp_value(fsj->cpu_nice, fsi->cpu_nice, g_itv));

      render(isdb, pre, PT_NOFLAG,
	     "all\t%%system", NULL, NULL,
	     NOVAL,
	     ll_sp_value(fsj->cpu_system, fsi->cpu_system, g_itv));

      render(isdb, pre, PT_NOFLAG,
	     "all\t%%iowait", NULL, NULL,
	     NOVAL,
	     ll_sp_value(fsj->cpu_iowait, fsi->cpu_iowait, g_itv));

      render(isdb, pre, PT_NEWLIN,
	     "all\t%%idle", NULL, NULL,
	     NOVAL,
	     (fsi->cpu_idle < fsj->cpu_idle)
	     ? 0.0
	     : ll_sp_value(fsj->cpu_idle, fsi->cpu_idle, g_itv));
   }


   if (GET_CPU(act) && WANT_PER_PROC(flags) && file_hdr.sa_proc) {
      int i;
      struct stats_one_cpu
	 *t = st_cpu[curr],
         *s = st_cpu[!curr];

      for (i = 0; i <= file_hdr.sa_proc; i++, t++, s++) {
	 if (cpu_bitmap[i >> 3] & (1 << (i & 0x07))) {

	    /* Recalculate itv for current proc */
	    itv = get_per_cpu_interval(t, s);

	    render(isdb, pre, PT_NOFLAG,
		   "cpu%d\t%%user",	/* ppc text with formatting */
		   "%d",		/* db text with format char */
		   cons(iv, i, NOVAL),	/* how we pass format args */
		   NOVAL,
		   ll_sp_value(s->per_cpu_user, t->per_cpu_user, itv));

	    render(isdb, pre, PT_NOFLAG,
		   "cpu%d\t%%nice", NULL, cons(iv, i, NOVAL),
		   NOVAL,
		   ll_sp_value(s->per_cpu_nice, t->per_cpu_nice, itv));

	    render(isdb, pre, PT_NOFLAG,
		   "cpu%d\t%%system", NULL, cons(iv, i, NOVAL),
		   NOVAL,
		   ll_sp_value(s->per_cpu_system, t->per_cpu_system, itv));

	    render(isdb, pre, PT_NOFLAG,
		   "cpu%d\t%%iowait", NULL, cons(iv, i, NOVAL),
		   NOVAL,
		   ll_sp_value(s->per_cpu_iowait, t->per_cpu_iowait, itv));

	    render(isdb, pre, PT_NEWLIN,
		   "cpu%d\t%%idle", NULL, cons(iv, i, NOVAL),
		   NOVAL,
		   (t->per_cpu_idle < s->per_cpu_idle)
		   ? 0.0
		   : ll_sp_value(s->per_cpu_idle, t->per_cpu_idle, itv));
	 }
      }
   }

   if (GET_IRQ(act) && wantproc) {
      /* Print number of interrupts per second */
      render(isdb, pre, PT_NEWLIN,
	     "sum\tintr/s", "-1", NULL,
	     NOVAL, ll_sp_value(fsj->irq_sum, fsi->irq_sum, itv));
   }

   if (GET_ONE_IRQ(act)) {
      int i;

      for (i = 0; i < NR_IRQS; i++) {
	 if (irq_bitmap[i >> 3] & (1 << (i & 0x07))) {
	    render(isdb, pre, PT_NEWLIN,
		   "i%03d\tintr/s", "%d", cons(iv, i, NOVAL),
		   NOVAL,
		   S_VALUE(interrupts[!curr][i], interrupts[curr][i], itv));
	 }
      }
   }

   /* print paging stats */
   if (GET_PAGE(act)) {
      render(isdb, pre, PT_NOFLAG,
	     "-\tpgpgin/s", NULL, NULL,
	     NOVAL,
	     S_VALUE(fsj->pgpgin, fsi->pgpgin, itv));

      render(isdb, pre, PT_NOFLAG,
	     "-\tpgpgout/s", NULL, NULL,
	     NOVAL,
	     S_VALUE(fsj->pgpgout, fsi->pgpgout, itv));

      render(isdb, pre, PT_NOFLAG,
	     "-\tfault/s", NULL, NULL,
	     NOVAL,
	     S_VALUE(fsj->pgfault, fsi->pgfault, itv));

      render(isdb, pre, PT_NEWLIN,
	     "-\tmajflt/s", NULL, NULL,
	     NOVAL,
	     S_VALUE(fsj->pgmajfault, fsi->pgmajfault, itv));
   }

   /* Print number of swap pages brought in and out */
   if (GET_SWAP(act)) {
      render(isdb, pre, PT_NOFLAG,
	     "-\tpswpin/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->pswpin, fsi->pswpin, itv));
      render(isdb, pre, PT_NEWLIN,
	     "-\tpswpout/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->pswpout, fsi->pswpout, itv));
   }

   /* Print I/O stats (no distinction made between disks) */
   if (GET_IO(act)) {
      render(isdb, pre, PT_NOFLAG,
	     "-\ttps", NULL, NULL,
	     NOVAL, S_VALUE(fsj->dk_drive, fsi->dk_drive, itv));

      render(isdb, pre, PT_NOFLAG,
	     "-\trtps", NULL, NULL,
	     NOVAL, S_VALUE(fsj->dk_drive_rio, fsi->dk_drive_rio, itv));

      render(isdb, pre, PT_NOFLAG,
	     "-\twtps", NULL, NULL,
	     NOVAL, S_VALUE(fsj->dk_drive_wio, fsi->dk_drive_wio, itv));

      render(isdb, pre, PT_NOFLAG,
	     "-\tbread/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->dk_drive_rblk, fsi->dk_drive_rblk, itv));

      render(isdb, pre, PT_NEWLIN,
	     "-\tbwrtn/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->dk_drive_wblk, fsi->dk_drive_wblk, itv));
   }

   /* Print memory stats */
   if (GET_MEMORY(act)) {
      render(isdb, pre, PT_NOFLAG,
	     "-\tfrmpg/s", NULL, NULL,
	     NOVAL,
	     S_VALUE((double) PG(fsj->frmkb),
		      (double) PG(fsi->frmkb), itv));

      render(isdb, pre, PT_NOFLAG,
	     "-\tbufpg/s", NULL, NULL,
	     NOVAL, S_VALUE((double) PG(fsj->bufkb),
			     (double) PG(fsi->bufkb), itv));

      render(isdb, pre, PT_NEWLIN,
	     "-\tcampg/s", NULL, NULL,
	     NOVAL, S_VALUE((double) PG(fsj->camkb),
			     (double) PG(fsi->camkb), itv));
   }

   /* Print TTY statistics (serial lines) */
   if (GET_SERIAL(act)) {
      int i;
      struct stats_serial
	 *sti = st_serial[curr],
         *stj = st_serial[!curr];

      for (i = 0; i++ < file_hdr.sa_serial; sti++, stj++) {

	 if (sti->line == ~0)
	    continue;
	
	 if (sti->line == stj->line) {
	    render(isdb, pre, PT_NOFLAG,
		   "ttyS%d\trcvin/s", "%d", cons(iv, sti->line, NOVAL),
		   NOVAL, S_VALUE(stj->rx, sti->rx, itv));

	    render(isdb, pre, PT_NEWLIN,
		   "ttyS%d\txmtin/s", "%d", cons(iv, sti->line, NOVAL),
		   NOVAL, S_VALUE(stj->tx, sti->tx, itv));
	 }
      }
   }

   /* Print amount and usage of memory */
   if (GET_MEM_AMT(act)) {
      render(isdb, pre, PT_USEINT,
	     "-\tkbmemfree", NULL, NULL, fsi->frmkb, DNOVAL);

      render(isdb, pre, PT_USEINT,
	     "-\tkbmemused", NULL, NULL, fsi->tlmkb - fsi->frmkb, DNOVAL);

      render(isdb, pre, PT_NOFLAG,
	     "-\t%%memused", NULL, NULL, NOVAL,
	     fsi->tlmkb
	     ? SP_VALUE(fsi->frmkb, fsi->tlmkb, fsi->tlmkb)
	     : 0.0);

      render(isdb, pre, PT_USEINT,
	     "-\tkbbuffers", NULL, NULL, fsi->bufkb, DNOVAL);

      render(isdb, pre, PT_USEINT,
	     "-\tkbcached", NULL, NULL, fsi->camkb, DNOVAL);

      render(isdb, pre, PT_USEINT,
	     "-\tkbswpfree", NULL, NULL, fsi->frskb, DNOVAL);

      render(isdb, pre, PT_USEINT,
	     "-\tkbswpused", NULL, NULL, fsi->tlskb - fsi->frskb, DNOVAL);

      render(isdb, pre, PT_NOFLAG,
	     "-\t%%swpused", NULL, NULL, NOVAL,
	     fsi->tlskb
	     ? SP_VALUE(fsi->frskb, fsi->tlskb, fsi->tlskb)
	     : 0.0);

      render(isdb, pre, PT_USEINT | PT_NEWLIN,
	     "-\tkbswpcad", NULL, NULL, fsi->caskb, DNOVAL);
   }

   if (GET_IRQ(act) && WANT_PER_PROC(flags) && file_hdr.sa_irqcpu) {
      int j, k, offset;
      struct stats_irq_cpu *p, *q, *p0, *q0;

      for (k = 0; k <= file_hdr.sa_proc; k++) {
	 if (!(cpu_bitmap[k >> 3] & (1 << (k & 0x07))))
	    /* These are not the droids you are looking for */
	    continue;

	 p0 = st_irq_cpu[curr];
	 for (j = 0; j < file_hdr.sa_irqcpu; p0++, j++) {
	    /* irq field set only for proc #0 */

	    /*
	     * A value of ~0 means it is a remaining interrupt
	     * which is no longer used, for example because the
	     * number of interrupts has decreased in /proc/interrupts
	     * or because we are appending data to an old sa file
	     * with more interrupts than are actually available now.
	     */
	    if (p0->irq == ~0)
	       continue;
	
	    q0 = st_irq_cpu[!curr] + j;
	    offset = j;
	
	    if (p0->irq != q0->irq) {
	       if (j)
		  offset = j - 1;
	       q0 = st_irq_cpu[!curr] + offset;
	
	       if (p0->irq != q0->irq
		   && (j + 1) < file_hdr.sa_irqcpu)
		  offset = j + 1;
	       q0 = st_irq_cpu[!curr] + offset;
	    }
	
	    if (p0->irq != q0->irq)
	       continue;
	
	    p = st_irq_cpu[curr]  + k * file_hdr.sa_irqcpu + j;
	    q = st_irq_cpu[!curr] + k * file_hdr.sa_irqcpu + offset;
	    render(isdb, pre, PT_NEWLIN,
		   "cpu%d\ti%03d/s", "%d;%d", cons(iv, k, p0->irq),
		   NOVAL, S_VALUE(q->interrupt, p->interrupt, itv));
	 }
      }
   }


   /* Print values of some kernel tables */
   if (GET_KTABLES(act)) {
      render(isdb, pre, PT_USEINT,
	     "-\tdentunusd", NULL, NULL,
	     fsi->dentry_stat, DNOVAL);

      render(isdb, pre, PT_USEINT,
	     "-\tfile-sz", NULL, NULL,
	     fsi->file_used, DNOVAL);

      render(isdb, pre, PT_USEINT,
	     "-\tinode-sz", NULL, NULL,
	     fsi->inode_used, DNOVAL);

      render(isdb, pre, PT_USEINT,
	     "-\tsuper-sz", NULL, NULL,
	     fsi->super_used, DNOVAL);

      render(isdb, pre, PT_NOFLAG,
	     "-\t%%super-sz", NULL, NULL,
	     NOVAL,
	     fsi->super_max
	     ? ((double) (fsi->super_used * 100)) / fsi->super_max
	     : 0.0);
			
      render(isdb, pre, PT_USEINT,
	     "-\tdquot-sz", NULL, NULL,
	     fsi->dquot_used, DNOVAL);

      render(isdb, pre, PT_NOFLAG,
	     "-\t%%dquot-sz", NULL, NULL,
	     NOVAL,
	     fsi->dquot_max
	     ? ((double) (fsi->dquot_used * 100)) / fsi->dquot_max
	     : 0.0);

      render(isdb, pre, PT_USEINT,
	     "-\trtsig-sz", NULL, NULL,
	     fsi->rtsig_queued, DNOVAL);

      render(isdb, pre, PT_NEWLIN,
	     "-\t%%rtsig-sz", NULL, NULL,
	     NOVAL,
	     fsi->rtsig_max
	     ? ((double) (fsi->rtsig_queued * 100)) / fsi->rtsig_max
	     : 0.0);
   }

   /* Print network interface statistics */
   if (GET_NET_DEV(act)) {
      int i, j;
      struct stats_net_dev
	 *sni = st_net_dev[curr],
         *snj;
      char *ifc;

      for (i = 0; i < file_hdr.sa_iface; i++, ++sni) {

	 if (!strcmp((ifc = sni->interface), "?"))
	    continue;

	 j = check_iface_reg(&file_hdr, st_net_dev, curr, !curr, i);
	 snj = st_net_dev[!curr] + j;

	 render(isdb, pre, PT_NOFLAG,
		"%s\trxpck/s", "%s",
		cons(sv, ifc, NULL), /* What if the format args are strings? */
		NOVAL, S_VALUE(snj->rx_packets, sni->rx_packets, itv));
		
	 render(isdb, pre, PT_NOFLAG,
		"%s\ttxpck/s", NULL, cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->tx_packets, sni->tx_packets, itv));

	 render(isdb, pre, PT_NOFLAG,
		"%s\trxbyt/s", NULL, cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->rx_bytes, sni->rx_bytes, itv));

	 render(isdb, pre, PT_NOFLAG,
		"%s\ttxbyt/s", NULL, cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->tx_bytes, sni->tx_bytes, itv));

	 render(isdb, pre, PT_NOFLAG,
		"%s\trxcmp/s", NULL, cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->rx_compressed, sni->rx_compressed, itv));

	 render(isdb, pre, PT_NOFLAG,
		"%s\ttxcmp/s", NULL, cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->tx_compressed, sni->tx_compressed, itv));

	 render(isdb, pre, PT_NEWLIN,
		"%s\trxmcst/s", NULL, cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->multicast, sni->multicast, itv));
      }
   }

   /* Print network interface statistics (errors) */
   if (GET_NET_EDEV(act)) {
      int i, j;
      struct stats_net_dev
	 *sni = st_net_dev[curr],
         *snj;
      char *ifc;

      for (i = 0; i < file_hdr.sa_iface; i++, ++sni) {

	 if (!strcmp((ifc = sni->interface), "?"))
	    continue;

	 j = check_iface_reg(&file_hdr, st_net_dev, curr, !curr, i);
	 snj = st_net_dev[!curr] + j;
	
	 render(isdb, pre, PT_NOFLAG,
		"%s\trxerr/s", "%s", cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->rx_errors, sni->rx_errors, itv));

	 render(isdb, pre, PT_NOFLAG,
		"%s\ttxerr/s", NULL, cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->tx_errors,
			       sni->tx_errors, itv));

	 render(isdb, pre, PT_NOFLAG,
		"%s\tcoll/s", NULL, cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->collisions, sni->collisions, itv));

	 render(isdb, pre, PT_NOFLAG,
		"%s\trxdrop/s", NULL, cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->rx_dropped, sni->rx_dropped, itv));

	 render(isdb, pre, PT_NOFLAG,
		"%s\ttxdrop/s", NULL, cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->tx_dropped, sni->tx_dropped, itv));

	 render(isdb, pre, PT_NOFLAG,
		"%s\ttxcarr/s", NULL, cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->tx_carrier_errors,
			       sni->tx_carrier_errors, itv));

	 render(isdb, pre, PT_NOFLAG,
		"%s\trxfram/s", NULL, cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->rx_frame_errors,
			       sni->rx_frame_errors, itv));

	 render(isdb, pre, PT_NOFLAG,
		"%s\trxfifo/s", NULL, cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->rx_fifo_errors,
			       sni->rx_fifo_errors, itv));

	 render(isdb, pre, PT_NEWLIN,
		"%s\ttxfifo/s", NULL, cons(sv, ifc, NULL),
		NOVAL, S_VALUE(snj->tx_fifo_errors,
			       sni->tx_fifo_errors, itv));
      }
   }

   /* Print number of sockets in use */
   if (GET_NET_SOCK(act)) {
      render(isdb, pre, PT_USEINT,
	     "-\ttotsck", NULL, NULL, fsi->sock_inuse, DNOVAL);

      render(isdb, pre, PT_USEINT,
	     "-\ttcpsck", NULL, NULL, fsi->tcp_inuse, DNOVAL);

      render(isdb, pre, PT_USEINT,
	     "-\tudpsck",  NULL, NULL, fsi->udp_inuse, DNOVAL);

      render(isdb, pre, PT_USEINT,
	     "-\trawsck", NULL, NULL, fsi->raw_inuse, DNOVAL);

      render(isdb, pre, PT_USEINT | PT_NEWLIN,
	     "-\tip-frag", NULL, NULL, fsi->frag_inuse, DNOVAL);
   }


   /* Print load averages and queue length */
   if (GET_QUEUE(act)) {
      render(isdb, pre, PT_USEINT,
	     "-\trunq-sz", NULL, NULL, fsi->nr_running, DNOVAL);
	
      render(isdb, pre, PT_USEINT,
	     "-\tplist-sz", NULL, NULL, fsi->nr_threads, DNOVAL);

      render(isdb, pre, PT_NOFLAG,
	     "-\tldavg-1", NULL, NULL,
	     NOVAL, (double) fsi->load_avg_1 / 100);

      render(isdb, pre, PT_NOFLAG,
	     "-\tldavg-5", NULL, NULL,
	     NOVAL, (double) fsi->load_avg_5 / 100);

      render(isdb, pre, PT_NEWLIN,
	     "-\tldavg-15", NULL, NULL,
	     NOVAL, (double) fsi->load_avg_15 / 100);
   }

   /* Print disk statistics */
   if (GET_DISK(act)) {
      int i, j;
      char *name;
      double tput, util, await, svctm, arqsz;
      struct disk_stats
	 *sdi = st_disk[curr],
         *sdj;

      for (i = 0; i < file_hdr.sa_nr_disk; i++, ++sdi) {

	 if (!(sdi->major + sdi->minor))
	    continue;

	 tput = ((double) sdi->nr_ios) * HZ / itv;
	 util = ((double) sdi->tot_ticks) / itv * HZ;
	 svctm = tput ? util / tput : 0.0;
	 await = sdi->nr_ios ?
	    (sdi->rd_ticks + sdi->wr_ticks) / ((double) sdi->nr_ios) : 0.0;
	 arqsz  = sdi->nr_ios ?
	    (sdi->rd_sect + sdi->wr_sect) / ((double) sdi->nr_ios) : 0.0;

	 j = check_disk_reg(&file_hdr, st_disk, curr, !curr, i);
	 sdj = st_disk[!curr] + j;
	 name = get_devname(sdi->major, sdi->minor, flags);
	
	 render(isdb, pre, PT_NOFLAG,
		"%s\ttps", "%s",
		cons(sv, name, NULL),
		NOVAL, S_VALUE(sdj->nr_ios, sdi->nr_ios, itv));

	 render(isdb, pre, PT_NOFLAG,
		"%s\trd_sec/s", NULL,
		cons(sv, name, NULL),
		NOVAL, ll_s_value(sdj->rd_sect, sdi->rd_sect, itv));

	 render(isdb, pre, PT_NOFLAG,
		"%s\twr_sec/s", NULL,
		cons(sv, name, NULL),
		NOVAL,	ll_s_value(sdj->wr_sect, sdi->wr_sect, itv));
	
	 render(isdb, pre, PT_NOFLAG,
		"%s\tavgrq-sz", NULL,
		cons(sv, name, NULL),
		NOVAL, arqsz);
	
	 render(isdb, pre, PT_NOFLAG,
		"%s\tavgqu-sz", NULL,
		cons(sv, name, NULL),
		NOVAL, ((double) sdi->rq_ticks) / itv * HZ / 1000.0);

	 render(isdb, pre, PT_NOFLAG,
		"%s\tawait", NULL,
		cons(sv, name, NULL),
		NOVAL, await);

	 render(isdb, pre, PT_NOFLAG,
		"%s\tsvctm", NULL,
		cons(sv, name, NULL),
		NOVAL, svctm);

	 render(isdb, pre, PT_NEWLIN,
		"%s\t%%util", NULL,
		cons(sv, name, NULL),
		NOVAL, util / 10.0);
      }
   }

   /* Print NFS client stats */
   if (GET_NET_NFS(act)) {
      render(isdb, pre, PT_NOFLAG,
	     "-\tcall/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfs_rpccnt, fsi->nfs_rpccnt, itv));
      render(isdb, pre, PT_NOFLAG,
	     "-\tretrans/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfs_rpcretrans, fsi->nfs_rpcretrans, itv));
      render(isdb, pre, PT_NOFLAG,
	     "-\tread/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfs_readcnt, fsi->nfs_readcnt, itv));
      render(isdb, pre, PT_NOFLAG,
	     "-\twrite/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfs_writecnt, fsi->nfs_writecnt, itv));
      render(isdb, pre, PT_NOFLAG,
	     "-\taccess/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfs_accesscnt, fsi->nfs_accesscnt, itv));
      render(isdb, pre, PT_NEWLIN,
	     "-\tgetatt/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfs_getattcnt, fsi->nfs_getattcnt, itv));
   }

   /* Print NFS server stats */
   if (GET_NET_NFSD(act)) {
      render(isdb, pre, PT_NOFLAG,
	     "-\tscall/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfsd_rpccnt, fsi->nfsd_rpccnt, itv));
      render(isdb, pre, PT_NOFLAG,
	     "-\tbadcall/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfsd_rpcbad, fsi->nfsd_rpcbad, itv));
      render(isdb, pre, PT_NOFLAG,
	     "-\tpacket/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfsd_netcnt, fsi->nfsd_netcnt, itv));
      render(isdb, pre, PT_NOFLAG,
	     "-\tudp/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfsd_netudpcnt, fsi->nfsd_netudpcnt, itv));
      render(isdb, pre, PT_NOFLAG,
	     "-\ttcp/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfsd_nettcpcnt, fsi->nfsd_nettcpcnt, itv));
      render(isdb, pre, PT_NOFLAG,
	     "-\thit/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfsd_rchits, fsi->nfsd_rchits, itv));
      render(isdb, pre, PT_NOFLAG,
	     "-\tmiss/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfsd_rcmisses, fsi->nfsd_rcmisses, itv));
      render(isdb, pre, PT_NOFLAG,
	     "-\tsread/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfsd_readcnt, fsi->nfsd_readcnt, itv));
      render(isdb, pre, PT_NOFLAG,
	     "-\tswrite/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfsd_writecnt, fsi->nfsd_writecnt, itv));
      render(isdb, pre, PT_NOFLAG,
	     "-\tsaccess/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfsd_accesscnt, fsi->nfsd_accesscnt, itv));
      render(isdb, pre, PT_NEWLIN,
	     "-\tsgetatt/s", NULL, NULL,
	     NOVAL, S_VALUE(fsj->nfsd_getattcnt, fsi->nfsd_getattcnt, itv));
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

   write_mech_stats(USE_DB_OPTION(flags), curr, act, dt, itv, g_itv, cur_time);

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
 * Display data file header
 ***************************************************************************
 */
void display_file_header(char *dfile, struct file_hdr *file_hdr)
{
   printf("File: %s (%#x)\n", dfile, file_hdr->sa_magic);

   print_gal_header(localtime(&(file_hdr->sa_ust_time)), file_hdr->sa_sysname, file_hdr->sa_release,
		    file_hdr->sa_nodename);

   printf("Activity flag: %#x\n", file_hdr->sa_actflag);
   printf("#CPU:    %u\n", file_hdr->sa_proc + 1);
   printf("#IrqCPU: %u\n", file_hdr->sa_irqcpu);
   printf("#Disks:  %u\n", file_hdr->sa_nr_disk);
   printf("#Serial: %u\n", file_hdr->sa_serial);
   printf("#Ifaces: %u\n", file_hdr->sa_iface);
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

   if (USE_H_OPTION(sadf_flags)) {
      /* Display data file header */
      display_file_header(dfile, &file_hdr);
      return;
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
		case 'H':
		  sadf_flags |= F_H_OPTION;
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
