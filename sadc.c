/*
 * sadc: system activity data collector
 * (C) 1999-2003 by Sebastien GODARD <sebastien.godard@wanadoo.fr>
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
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/utsname.h>

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

/* Nb of processors on the machine. A value of 1 means two processors */
int cpu_nr = -1;
int serial_used = 0, irqcpu_used = 0, iface_used = 0, disk_used = 0;
unsigned int sadc_actflag;
long interval = 0, count = 0;
int kb_shift = 0;

struct file_hdr file_hdr;
struct file_stats file_stats;
struct stats_one_cpu *st_cpu;
struct stats_serial *st_serial;
struct stats_net_dev *st_net_dev;
struct stats_irq_cpu *st_irq_cpu;
struct disk_stats *st_disk;
struct pid_stats *pid_stats[MAX_PID_NR];

unsigned long all_pids[MAX_PID_NR];
unsigned char f_pids[MAX_PID_NR];
unsigned int interrupts[NR_IRQS];
unsigned int u_tmp[NR_DISKS - 1];
int pid_nr = 0, apid_nr = 0;


/*
 ***************************************************************************
 * Print usage and exit
 ***************************************************************************
 */
void usage(char *progname)
{
   /*
    * Don't show options like -x ALL or -X SELF.
    * They should only be used with sar.
    */
   fprintf(stderr, _("sysstat version %s\n"
		   "(C) Sebastien Godard\n"
	           "Usage: %s [ options... ] [ <interval> [ <count> ] ] [ <outfile> ]\n"
		   "Options are:\n"
		   "[ -x <pid> ] [ -X <pid> ] [ -I ] [ -V ]\n"),
	   VERSION, progname);
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
 * Allocate stats_one_cpu structures
 * (only on SMP machines)
 ***************************************************************************
 */
void salloc_cpu(int nr_cpus)
{
   if ((st_cpu = (struct stats_one_cpu *) malloc(STATS_ONE_CPU_SIZE * nr_cpus)) == NULL) {
      perror("malloc");
      exit(4);
   }

   memset(st_cpu, 0, STATS_ONE_CPU_SIZE * nr_cpus);
}


/*
 ***************************************************************************
 * Allocate stats_serial structures
 ***************************************************************************
 */
void salloc_serial(int nr_serial)
{
   if ((st_serial = (struct stats_serial *) malloc(STATS_SERIAL_SIZE * nr_serial)) == NULL) {
      perror("malloc");
      exit(4);
   }

   memset(st_serial, 0, STATS_SERIAL_SIZE * nr_serial);
}


/*
 ***************************************************************************
 * Allocate stats_irq_cpu structures
 ***************************************************************************
 */
void salloc_irqcpu(int nr_cpus, int nr_irqcpu)
{
   /*
    * st_irq_cpu->irq:       IRQ#-A
    * st_irq_cpu->interrupt: number of IRQ#-A for proc 0
    * st_irq_cpu->irq:       IRQ#-B
    * st_irq_cpu->interrupt: number of IRQ#-B for proc 0
    * ...
    * st_irq_cpu->irq:       (undef'd)
    * st_irq_cpu->interrupt: number of IRQ#-A for proc 1
    * st_irq_cpu->irq:       (undef'd)
    * st_irq_cpu->interrupt: number of IRQ#-B for proc 1
    * ...
    */

   if ((st_irq_cpu = (struct stats_irq_cpu *) malloc(STATS_IRQ_CPU_SIZE * nr_cpus * nr_irqcpu)) == NULL) {
      perror("malloc");
      exit(4);
   }

   memset(st_irq_cpu, 0, STATS_IRQ_CPU_SIZE * nr_cpus * nr_irqcpu);
}


/*
 ***************************************************************************
 * Allocate stats_net_dev structures
 ***************************************************************************
 */
void salloc_net_dev(int nr_iface)
{
   if ((st_net_dev = (struct stats_net_dev *) malloc(STATS_NET_DEV_SIZE * nr_iface)) == NULL) {
      perror("malloc");
      exit(4);
   }

   memset(st_net_dev, 0, STATS_NET_DEV_SIZE * nr_iface);
}


/*
 ***************************************************************************
 * Allocate disk_stats structures
 ***************************************************************************
 */
void salloc_disk(int nr_disks)
{
   if ((st_disk = (struct disk_stats *) malloc(DISK_STATS_SIZE * nr_disks)) == NULL) {
      perror("malloc");
      exit(4);
   }

   memset(st_disk, 0, DISK_STATS_SIZE * nr_disks);
}


/*
 ***************************************************************************
 * Allocate pid_stats structures
 ***************************************************************************
 */
void salloc_pid(int pid_nr)
{
   int pid;

   if ((pid_stats[0] = (struct pid_stats *) malloc(PID_STATS_SIZE * pid_nr)) == NULL) {
      perror("malloc");
      exit(4);
   }

   memset(pid_stats[0], 0, PID_STATS_SIZE * pid_nr);

   for (pid = 1; pid < pid_nr; pid++)
      /* Structures are aligned but also padded. Thus array members are packed */
      pid_stats[pid] = pid_stats[0] + pid;	/* Assume pid_nr <= MAX_PID_NR */
}


void p_write_error(void)
{
    fprintf(stderr, _("Cannot write data to system activity file: %s\n"), strerror(errno));
    exit(2);
}


/*
 ***************************************************************************
 * Set PID flag value (bit 0 set: -x, bit 1 set: -X)
 ***************************************************************************
 */
void set_pflag(int child, unsigned long pid)
{
   int i = 0, flag;

   if (!pid) {
      if (child)
	 flag = 0x02;
      else
	 flag = 0x01;

      for (i = 0; i < MAX_PID_NR; i++)
	 f_pids[i] |= flag;
   }
   else {
      if (child)
	 flag = 0x02;
      else
	 flag = 0x01;

      while ((i < apid_nr) && (all_pids[i] != pid))
	 i++;

      /* PID not found: insert it if possible */
      if ((i == apid_nr) && (apid_nr < MAX_PID_NR))
	 all_pids[apid_nr++] = pid;

      f_pids[i] |= flag;
   }
}


/*
 ***************************************************************************
 * Count number of processes to display
 ***************************************************************************
 */
int count_pids(void)
{
   int i = 0, n = 0;

   while (i < apid_nr) {
      if (f_pids[i]) {
	 n++;
	 i++;
      }
      else {
	 /* It's an unused entry: remove it */
	 if (i < apid_nr - 1) {
	    all_pids[i] = all_pids[apid_nr - 1];
	    f_pids[i]  |= f_pids[apid_nr - 1];
	 }
	 apid_nr--;
      }
   }

   /* Allocate structures now */
   salloc_pid(n);

   return n;
}



/*
 ***************************************************************************
 * Look for all the PIDs
 ***************************************************************************
 */
void get_pid_list(void)
{
   int i;
   DIR *dir;
   struct dirent *drp;

   apid_nr = 0;

   /* Open /proc directory */
   if ((dir = opendir(PROC)) == NULL) {
      perror("opendir");
      exit(4);
   }

   /* Get directory entries */
   while ((drp = readdir(dir)) != NULL) {
      if (isdigit(drp->d_name[0]) && (apid_nr < MAX_PID_NR))
	 all_pids[apid_nr++] = atol(drp->d_name);
   }

   /* Close /proc directory */
   (void) closedir(dir);

   /* Init PID flag */
   for (i = 0; i < MAX_PID_NR; i++)
      f_pids[i] = 0;
}


/*
 ***************************************************************************
 * Find number of serial lines that support tx/rx accounting
 ***************************************************************************
 */
void get_serial_lines(int *serial_used)
{
   FILE *serfp;
   char line[256];
   int sl = 0;

#ifdef SMP_RACE
   /*
    * Ignore serial lines if SMP_RACE flag is defined.
    * This is because there is an SMP race in some 2.2.x kernels that
    * may be triggered when reading the /proc/tty/driver/serial file.
    */
   *serial_used = 0;
   return;

#else
   /* Open serial file */
   if ((serfp = fopen(SERIAL, "r")) == NULL) {
      *serial_used = 0;	/* No SERIAL file */
      return;
   }

   while (fgets(line, 256, serfp) != NULL) {
      /*
       * tx/rx statistics are always present,
       * except when serial line is unknown.
       */
      if (strstr(line, "tx:") != NULL)
	 sl++;
   }

   /* Close serial file */
   fclose(serfp);

   *serial_used = sl + NR_SERIAL_PREALLOC;
#endif
}


/*
 ***************************************************************************
 * Find number of interfaces (network devices) that are in /proc/net/dev file
 * (see linux source file linux/net/core/dev.c)
 ***************************************************************************
 */
void get_net_dev(int *iface_used)
{
   FILE *devfp;
   char line[128];
   int dev = 0;

   /* Open network device file */
   if ((devfp = fopen(NET_DEV, "r")) == NULL) {
      *iface_used = 0;	/* No network device file */
      return;
   }

   while (fgets(line, 128, devfp) != NULL) {
      if (line[6] == ':')
	 dev++;
   }

   /* Close network device file */
   fclose(devfp);

   *iface_used = dev + NR_IFACE_PREALLOC;
}


/*
 ***************************************************************************
 * Find number of interrupts available per processor.
 * Called on SMP machines only.
 ***************************************************************************
 */
void get_irqcpu_nb(int *irqcpu_used, unsigned int max_nr_irqcpu)
{
   FILE *irqfp;
   char line[16];
   int irq = 0;

   /* Open interrupts file */
   if ((irqfp = fopen(INTERRUPTS, "r")) == NULL) {
      *irqcpu_used = 0;	/* No INTERRUPTS file */
      return;
   }

   while ((fgets(line, 16, irqfp) != NULL) && (irq < max_nr_irqcpu)) {
      if (isdigit(line[2]))
	 irq++;
   }

   /* Close interrupts file */
   fclose(irqfp);

   *irqcpu_used = irq + NR_IRQPROC_PREALLOC;
}


/*
 ***************************************************************************
 * Get total number of minor and major faults made by the system
 ***************************************************************************
 */
void read_sysfaults(void)
{
   DIR *dir;
   struct dirent *drp;
   FILE *pidfp;
   unsigned long minflt, majflt;
   static char filename[24];

   file_stats.nr_processes = 0;
   file_stats.minflt       = 0;
   file_stats.majflt       = 0;

   /* Open /proc directory */
   if ((dir = opendir(PROC)) == NULL) {
      perror("opendir");
      exit(4);
   }

   /* Get directory entries */
   while ((drp = readdir(dir)) != NULL) {
      if (isdigit(drp->d_name[0])) {
	 sprintf(filename, "%s/%s/%s", PROC, drp->d_name, PSTAT);
	 if ((pidfp = fopen(filename, "r")) != NULL) {
	    fscanf(pidfp, "%*d %*s %*s %*d %*d %*d %*d %*d %*u %lu %*u %lu %*u %*u %*u %*u %*u %*d %*d %*u %*u %*d %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u\n",
		   &minflt, &majflt);

	    /* Close /proc/<pid>/stat file */
	    fclose(pidfp);

	    file_stats.nr_processes++;
	    file_stats.minflt += minflt;
	    file_stats.majflt += majflt;
	 }
      }
   }

   /* Close /proc directory */
   (void) closedir(dir);
}


/*
 ***************************************************************************
 * Fill system activity file header, then print it
 ***************************************************************************
 */
void setup_file_hdr(int ofd, size_t *file_stats_size)
{
   int nb;
   struct tm loc_time;
   struct utsname header;

   /* First reset the structure */
   memset(&file_hdr, 0, sizeof(file_hdr));

   /* Then get current date */
   file_hdr.sa_ust_time = get_localtime(&loc_time);

   /* Ok, now fill the header */
   file_hdr.sa_actflag = sadc_actflag;
   file_hdr.sa_magic   = SA_MAGIC;
   file_hdr.sa_st_size = FILE_STATS_SIZE;
   file_hdr.sa_day     = loc_time.tm_mday;
   file_hdr.sa_month   = loc_time.tm_mon;
   file_hdr.sa_year    = loc_time.tm_year;
   file_hdr.sa_proc    = cpu_nr;
   file_hdr.sa_nr_pid  = pid_nr;
   file_hdr.sa_serial  = serial_used;
   file_hdr.sa_irqcpu  = irqcpu_used;
   file_hdr.sa_iface   = iface_used;
   file_hdr.sa_nr_disk = disk_used;

   *file_stats_size = FILE_STATS_SIZE;

   /* Get system name, release number and hostname */
   uname(&header);
   strncpy(file_hdr.sa_sysname, header.sysname, UTSNAME_LEN);
   file_hdr.sa_sysname[UTSNAME_LEN - 1] = '\0';
   strncpy(file_hdr.sa_nodename, header.nodename, UTSNAME_LEN);
   file_hdr.sa_nodename[UTSNAME_LEN - 1] = '\0';
   strncpy(file_hdr.sa_release, header.release, UTSNAME_LEN);
   file_hdr.sa_release[UTSNAME_LEN - 1] = '\0';

   /* Write file header */
   if ((nb = write(ofd, &file_hdr, FILE_HDR_SIZE)) != FILE_HDR_SIZE) {
      fprintf(stderr, _("Cannot write system activity file header: %s\n"), strerror(errno));
      exit(2);
   }
}


/*
 ***************************************************************************
 * sadc called with interval and count parameters not set:
 * write a dummy record notifying a system restart.
 * This should typically be called this way at boot time,
 * before the cron daemon is started to avoid conflict with sa1/sa2 scripts.
 ***************************************************************************
 */
void write_dummy_record(int ofd, size_t file_stats_size)
{
   int nb;
   struct tm loc_time;

   /* Reset the structure (not compulsory, but a bit cleaner */
   memset(&file_stats, 0, sizeof(file_stats));

   file_stats.record_type = R_DUMMY;

   /* Save time */
   file_stats.ust_time = get_localtime(&loc_time);

   file_stats.hour   = loc_time.tm_hour;
   file_stats.minute = loc_time.tm_min;
   file_stats.second = loc_time.tm_sec;

   /* Write record now */
   if ((nb = write(ofd, &file_stats, file_stats_size)) != file_stats_size)
      p_write_error();
}


/*
 ***************************************************************************
 * Write stats.
 * NB: sadc provides all the stats, including:
 * -> CPU utilization per processor (on SMP machines only)
 * -> IRQ per processor (on SMP machines only)
 * -> number of each IRQ (if -I option passed to sadc)
 ***************************************************************************
 */
void write_stats(int ofd, size_t file_stats_size)
{
   int nb;

   if ((nb = write(ofd, &file_stats, file_stats_size)) != file_stats_size)
      p_write_error();
   if (cpu_nr > 0) {
      if ((nb = write(ofd, st_cpu, STATS_ONE_CPU_SIZE * (cpu_nr + 1))) != (STATS_ONE_CPU_SIZE * (cpu_nr + 1)))
	 p_write_error();
   }
   if (GET_ONE_IRQ(sadc_actflag)) {
      if ((nb = write(ofd, interrupts, STATS_ONE_IRQ_SIZE)) != STATS_ONE_IRQ_SIZE)
	 p_write_error();
   }
   if (pid_nr) {
      /* Structures are packed together! */
      if ((nb = write(ofd, pid_stats[0], PID_STATS_SIZE * pid_nr)) != (PID_STATS_SIZE * pid_nr))
	 p_write_error();
   }
   if (serial_used) {
      if ((nb = write(ofd, st_serial, STATS_SERIAL_SIZE * serial_used)) != (STATS_SERIAL_SIZE * serial_used))
	 p_write_error();
   }
   if (irqcpu_used) {
      if ((nb = write(ofd, st_irq_cpu, STATS_IRQ_CPU_SIZE * (cpu_nr + 1) * irqcpu_used))
	  != (STATS_IRQ_CPU_SIZE * (cpu_nr + 1) * irqcpu_used))
	 p_write_error();
   }
   if (iface_used) {
      if ((nb = write(ofd, st_net_dev, STATS_NET_DEV_SIZE * iface_used)) != (STATS_NET_DEV_SIZE * iface_used))
	 p_write_error();
   }
   if (disk_used) {
      if ((nb = write(ofd, st_disk, DISK_STATS_SIZE * disk_used)) != (DISK_STATS_SIZE * disk_used))
	 p_write_error();
   }
}


/*
 ***************************************************************************
 * Create a system activity daily data file
 ***************************************************************************
 */
void create_sa_file(int *ofd, char *ofile, size_t *file_stats_size)
{
   if ((*ofd = open(ofile, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
      fprintf(stderr, _("Cannot open %s: %s\n"), ofile, strerror(errno));
      exit(2);
   }

   /* Write file header */
   setup_file_hdr(*ofd, file_stats_size);
}


/*
 ***************************************************************************
 * Get file descriptor for output
 ***************************************************************************
 */
void open_ofile(int *ofd, char ofile[], size_t *file_stats_size)
{
   ssize_t size;

   if (ofile[0]) {
      /* Does file exist? */
      if (access(ofile, F_OK) < 0)
	 /* NO: create it */
	 create_sa_file(ofd, ofile, file_stats_size);
      else {
	 /* YES: append data to it if possible */
	 if ((*ofd = open(ofile, O_APPEND | O_RDWR)) < 0) {
	    fprintf(stderr, _("Cannot open %s: %s\n"), ofile, strerror(errno));
	    exit(2);
	 }
	 /* Read file header */
	 size = read(*ofd, &file_hdr, FILE_HDR_SIZE);
	 if (!size) {
	    /* This is an empty file: create it again */
	    create_sa_file(ofd, ofile, file_stats_size);
	    return;
	 }
	 if ((size != FILE_HDR_SIZE) || (file_hdr.sa_magic != SA_MAGIC)) {
	    fprintf(stderr, _("Invalid system activity file\n"));
	    exit(3);
	 }
	 /*
	  * Ok: it's a true system activity file.
	  * File activity flag prevails over that of the user
	  * in particular for the A_ONE_IRQ activity...
	  * Same thing with file file_stats size.
	  */
	 sadc_actflag     = file_hdr.sa_actflag;
	 *file_stats_size = file_hdr.sa_st_size;

	 if (file_hdr.sa_proc != cpu_nr) {
	    fprintf(stderr, _("Cannot append data to that file\n"));
	    exit(1);
	 }
	
	 /*
	  * Force characteristics (nb of serial lines, network interfaces...)
	  * to that of the file.
	  */
	 if (file_hdr.sa_serial != serial_used) {
	    if (serial_used)
	       free(st_serial);
	    serial_used = file_hdr.sa_serial;
	    salloc_serial(serial_used);
	 }
	 if (file_hdr.sa_iface != iface_used) {
	    if (iface_used)
	       free(st_net_dev);
	    iface_used = file_hdr.sa_iface;
	    salloc_net_dev(iface_used);
	 }
	 if (file_hdr.sa_irqcpu != irqcpu_used) {
	    if (irqcpu_used)
	       free(st_irq_cpu);
	    irqcpu_used = file_hdr.sa_irqcpu;
	    salloc_irqcpu(cpu_nr + 1, irqcpu_used);
	 }
	 if (file_hdr.sa_nr_disk != disk_used) {
	    if (disk_used)
	       free(st_disk);
	    disk_used = file_hdr.sa_nr_disk;
	    salloc_disk(disk_used);
	 }
      }
   }
   /* Duplicate stdout file descriptor */
   else {
      if ((*ofd = dup(1)) < 0) {
	 perror("dup");
	 exit(4);
      }
      /* Write file header */
      setup_file_hdr(*ofd, file_stats_size);
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/stat
 * (see linux source file linux/fs/proc/proc_misc.c)
 ***************************************************************************
 */
void read_proc_stat(void)
{
   FILE *statfp;
   struct stats_one_cpu *st_cpu_i;
   struct disk_stats *st_disk_i;
   static char line[2560];
   unsigned int cc_user, cc_nice, cc_system;
   unsigned long cc_idle, cc_iowait;
   unsigned int v_tmp[5], v_major, v_index;
   int proc_nb, i, pos;

   /* Open stat file */
   if ((statfp = fopen(STAT, "r")) == NULL) {
      fprintf(stderr, _("Cannot open %s: %s\n"), STAT, strerror(errno));
      exit(2);
   }

   while (fgets(line, 2560, statfp) != NULL) {

      if (!strncmp(line, "cpu ", 4)) {
	 /*
	  * Read the number of jiffies spent in user, nice, system, idle and
	  * iowait mode among all proc. CPU usage is not reduced to one
	  * processor to avoid rounding problems.
	  */
	 file_stats.cpu_iowait = 0;	/* in case it isn't 2.5 */
	 sscanf(line + 5, "%u %u %u %lu %lu",
		&(file_stats.cpu_user),   &(file_stats.cpu_nice),
		&(file_stats.cpu_system), &(file_stats.cpu_idle),
		&(file_stats.cpu_iowait));

	 /*
	  * Compute the uptime of the system in jiffies (1/100ths of a second
	  * if HZ=100).
	  * Machine uptime is multiplied by the number of processors here.
	  * Note that overflow is not so far away: ULONG_MAX is 4294967295 on
	  * 32 bit systems. Overflow happens when machine uptime is:
	  * 497 days on a monoprocessor machine,
	  * 248 days on a bi processor,
	  * 124 days on a quad processor...
	  */
	 file_stats.uptime = file_stats.cpu_user   + file_stats.cpu_nice +
	                     file_stats.cpu_system + file_stats.cpu_idle +
	                     file_stats.cpu_iowait;
      }

      else if (!strncmp(line, "cpu", 3)) {
	 if (cpu_nr > 0) {
	    /*
	     * Read the number of jiffies spent in user, nice, system, idle
	     * and iowait mode for current proc.
	     * This is done only on SMP machines.
	     * Warning: st_cpu_i struct is _not_ allocated even if the kernel
	     * has SMP support enabled.
	     */
	    cc_iowait = 0;	/* in case it isn't 2.5 */
	    sscanf(line + 3, "%d %u %u %u %lu %lu",
		   &proc_nb,
		   &cc_user, &cc_nice, &cc_system, &cc_idle, &cc_iowait);
	    if (proc_nb <= cpu_nr) {
	       st_cpu_i = st_cpu + proc_nb;
	       st_cpu_i->per_cpu_user   = cc_user;
	       st_cpu_i->per_cpu_nice   = cc_nice;
	       st_cpu_i->per_cpu_system = cc_system;
	       st_cpu_i->per_cpu_idle   = cc_idle;
	       st_cpu_i->per_cpu_iowait = cc_iowait;
	    }
	    /* else:
	     * Additional CPUs have been dynamically registered in /proc/stat.
	     * sar won't crash, but the CPU stats might be false...
	     */
	 }
      }

      else if (!strncmp(line, "disk ", 5)) {
	 /* Read number of I/O done since the last reboot */
	 sscanf(line + 5, "%u %u %u %u",
		&(file_stats.dk_drive), &u_tmp[0], &u_tmp[1], &u_tmp[2]);
	 file_stats.dk_drive += u_tmp[0] + u_tmp[1] + u_tmp[2];
      }

      else if (!strncmp(line, "disk_rio ", 9)) {
	 /* Read number of read I/O */
	 sscanf(line + 9, "%u %u %u %u",
		&(file_stats.dk_drive_rio), &u_tmp[0], &u_tmp[1], &u_tmp[2]);
	 file_stats.dk_drive_rio += u_tmp[0] + u_tmp[1] + u_tmp[2];
      }

      else if (!strncmp(line, "disk_wio ", 9)) {
	 /* Read number of write I/O */
	 sscanf(line + 9, "%u %u %u %u",
		&(file_stats.dk_drive_wio), &u_tmp[0], &u_tmp[1], &u_tmp[2]);
	 file_stats.dk_drive_wio += u_tmp[0] + u_tmp[1] + u_tmp[2];
      }

      else if (!strncmp(line, "disk_rblk ", 10)) {
	 /* Read number of blocks read from disk */
	 sscanf(line + 10, "%u %u %u %u",
		&(file_stats.dk_drive_rblk), &u_tmp[0], &u_tmp[1], &u_tmp[2]);
	 file_stats.dk_drive_rblk += u_tmp[0] + u_tmp[1] + u_tmp[2];
      }

      else if (!strncmp(line, "disk_wblk ", 10)) {
	 /* Read number of blocks written to disk */
	 sscanf(line + 10, "%u %u %u %u",
		&(file_stats.dk_drive_wblk), &u_tmp[0], &u_tmp[1], &u_tmp[2]);
	 file_stats.dk_drive_wblk += u_tmp[0] + u_tmp[1] + u_tmp[2];
      }

      else if (!strncmp(line, "page ", 5))
	 /* Read number of pages the system paged in and out */
	 sscanf(line + 5, "%u %u",
		&(file_stats.pgpgin), &(file_stats.pgpgout));

      else if (!strncmp(line, "swap ", 5))
	 /* Read number of swap pages brought in and out */
	 sscanf(line + 5, "%u %u",
		&(file_stats.pswpin), &(file_stats.pswpout));

      else if (!strncmp(line, "intr ", 5)) {
	 /* Read total number of interrupts received since system boot */
	 sscanf(line + 5, "%u", &(file_stats.irq_sum));
	 pos = strcspn(line + 5, " ") + 5;

	 /* Read number of each interrupts received since system boot */
	 for (i = 0; i < NR_IRQS; i++) {
	    sscanf(line + pos, " %u", &interrupts[i]);
	    pos += strcspn(line + pos + 1, " ") + 1;
	 }
      }

      else if (!strncmp(line, "disk_io: ", 9)) {
	 int dsk = 0;
	
	 file_stats.dk_drive = 0;
	 file_stats.dk_drive_rio  = file_stats.dk_drive_wio  = 0;
	 file_stats.dk_drive_rblk = file_stats.dk_drive_wblk = 0;
	 pos = 9;
	
	 /* Read disks I/O statistics (for 2.4 kernels) */
	 while (pos < strlen(line) - 1) {	/* Beware: a CR is already included in the line */
	    sscanf(line + pos, "(%u,%u):(%u,%u,%u,%u,%u) ",
		   &v_major, &v_index,
		   &v_tmp[0], &v_tmp[1], &v_tmp[2], &v_tmp[3], &v_tmp[4]);
	    file_stats.dk_drive += v_tmp[0];
	    file_stats.dk_drive_rio  += v_tmp[1];
	    file_stats.dk_drive_rblk += v_tmp[2];
	    file_stats.dk_drive_wio  += v_tmp[3];
	    file_stats.dk_drive_wblk += v_tmp[4];
	    if (dsk < disk_used) {
	       st_disk_i = st_disk + dsk;
	       st_disk_i->major = v_major;
	       st_disk_i->index = v_index;
	       st_disk_i->dk_drive = v_tmp[0];
	       st_disk_i->dk_drive_rwblk = v_tmp[2] + v_tmp[4];
	       dsk++;
	    }
	    pos += strcspn(line + pos, " ") + 1;
	 }

	 while (dsk < disk_used) {
	    /*
	     * Nb of disks has changed, or appending data to an old file
	     * with more disks than are actually available now.
	     */
	    st_disk_i = st_disk + dsk++;
	    st_disk_i->major = st_disk_i->index = 0;
	 }
      }

      else if (!strncmp(line, "ctxt ", 5))
	 /* Read number of context switches */
	 sscanf(line + 5, "%u", &(file_stats.context_swtch));

      else if (!strncmp(line, "processes ", 10))
	 /* Read number of processes created since system boot */
	 sscanf(line + 10, "%lu", &(file_stats.processes));
   }

   /* Close stat file */
   fclose(statfp);
}


/*
 ***************************************************************************
 * Read stats from /proc/loadavg
 * (see linux source file linux/fs/proc/proc_misc.c)
 ***************************************************************************
 */
void read_proc_loadavg(void)
{
   FILE *loadfp;
   int load_tmp[3];

   /* Open loadavg file */
   if ((loadfp = fopen(LOADAVG, "r")) == NULL) {
      file_stats.nr_running = 0;
      file_stats.nr_threads = 0;
      file_stats.load_avg_1 = file_stats.load_avg_5
			    = file_stats.load_avg_15
			    = 0;
   }
   else {
      /* Read load averages and queue length */
      fscanf(loadfp, "%d.%d %d.%d %d.%d %d/%d %*d\n",
	     &(load_tmp[0]), &(file_stats.load_avg_1),
	     &(load_tmp[1]), &(file_stats.load_avg_5),
	     &(load_tmp[2]), &(file_stats.load_avg_15),
	     &(file_stats.nr_running),
	     &(file_stats.nr_threads));
      fclose(loadfp);

      file_stats.load_avg_1  += load_tmp[0] * 100;
      file_stats.load_avg_5  += load_tmp[1] * 100;
      file_stats.load_avg_15 += load_tmp[2] * 100;
      if (file_stats.nr_running)
	 /* Do not take current process into account */
	 file_stats.nr_running--;
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/meminfo
 * (see linux source file linux/fs/proc/array.c and linux/fs/proc/proc_misc.c)
 ***************************************************************************
 */
void read_proc_meminfo(void)
{
   FILE *memfp;
   static char line[64];
   unsigned int mtemp;
   unsigned long lmtemp;

   /* Open meminfo file */
   if ((memfp = fopen(MEMINFO, "r")) == NULL) {
      file_stats.tlmkb = 0;
      file_stats.frmkb = 0;
      file_stats.shmkb = 0;
      file_stats.bufkb = 0;
      file_stats.camkb = 0;
      file_stats.tlskb = 0;
      file_stats.frskb = 0;
      file_stats.nr_active_pages         = 0;
      file_stats.nr_inactive_dirty_pages = 0;
      file_stats.nr_inactive_clean_pages = 0;
      file_stats.inactive_target         = 0;
      return;
   }

   while (fgets(line, 64, memfp) != NULL) {

      if (!strncmp(line, "MemTotal:", 9))
	 /* Read the total amount of memory in kB */
	 sscanf(line + 10, "%8lu", &(file_stats.tlmkb));

      else if (!strncmp(line, "MemFree:", 8))
	 /* Read the amount of free memory in kB */
	 sscanf(line + 10, "%8lu", &(file_stats.frmkb));

      else if (!strncmp(line, "MemShared:", 10))
	 /* Read the amount of shared memory in kB */
	 sscanf(line + 10, "%8lu", &(file_stats.shmkb));

      else if (!strncmp(line, "Buffers:", 8))
	 /* Read the amount of buffered memory in kB */
	 sscanf(line + 10, "%8lu", &(file_stats.bufkb));

      else if (!strncmp(line, "Cached:", 7))
	 /* Read the amount of cached memory in kB */
	 sscanf(line + 10, "%8lu", &(file_stats.camkb));

      else if (!strncmp(line, "Active:", 7)) {
	 /* Read the amount of active memory in kB */
	 sscanf(line + 10, "%8u", &mtemp);
	 file_stats.nr_active_pages = PG(mtemp);
      }

      else if (!strncmp(line, "Inact_dirty:", 12)) {
	 /* Read the amount of inactive dirty memory in kB */
	 sscanf(line + 13, "%8u", &mtemp);
	 file_stats.nr_inactive_dirty_pages = PG(mtemp);
      }

      else if (!strncmp(line, "Inact_clean:", 12)) {
	 /* Read the amount of inactive clean memory in kB */
	 sscanf(line + 13, "%8u", &mtemp);
	 file_stats.nr_inactive_clean_pages = PG(mtemp);
      }

      else if (!strncmp(line, "Inact_target:", 13)) {
	 /* Read the amount of inactive target memory in kB */
	 sscanf(line + 13, "%8lu", &lmtemp);
	 file_stats.inactive_target = PG(lmtemp);
      }

      else if (!strncmp(line, "SwapTotal:", 10))
	 /* Read the total amount of swap memory in kB */
	 sscanf(line + 10, "%8lu", &(file_stats.tlskb));

      else if (!strncmp(line, "SwapFree:", 9))
	 /* Read the amount of free swap memory in kB */
	 sscanf(line + 10, "%8lu", &(file_stats.frskb));
   }

   /* Close meminfo file */
   fclose(memfp);
}


/*
 ***************************************************************************
 * Read stats from /proc/<pid>/stat
 * (see linux source file linux/fs/proc/array.c)
 ***************************************************************************
 */
void read_pid_stat(void)
{
   FILE *pidfp;
   int pid;
   static char filename[24];

   for (pid = 0; pid < pid_nr; pid++) {

      if (!all_pids[pid])
	 continue;

      /* Open <pid>/stat file */
      sprintf(filename, PID_STAT, all_pids[pid]);
      if ((pidfp = fopen(filename, "r")) == NULL) {
	 /* No such process */
	 all_pids[pid] = 0;
	 pid_stats[pid]->pid = 0;
	 continue;
      }

      fscanf(pidfp, "%*d %*s %*s %*d %*d %*d %*d %*d %*u %lu %lu %lu %lu %lu %lu %lu %lu %*d %*d %*u %*u %*d %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %lu %lu %*u %u\n",
	     &(pid_stats[pid]->minflt), &(pid_stats[pid]->cminflt),
	     &(pid_stats[pid]->majflt), &(pid_stats[pid]->cmajflt),
	     &(pid_stats[pid]->utime),  &(pid_stats[pid]->stime),
	     &(pid_stats[pid]->cutime), &(pid_stats[pid]->cstime),
	     &(pid_stats[pid]->nswap),  &(pid_stats[pid]->cnswap),
	     &(pid_stats[pid]->processor));

      pid_stats[pid]->pid  = all_pids[pid];
      pid_stats[pid]->flag = f_pids[pid];

      /* Close <pid>/stat file */
      fclose(pidfp);
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/tty/driver/serial
 * (see linux source file linux/drivers/char/serial.c)
 ***************************************************************************
 */
void read_serial_stat(void)
{
   FILE *serfp;
   struct stats_serial *st_serial_i;
   static char line[256];
   int sl = 0;
   int tty;
   char *p;


#ifndef SMP_RACE
      /* Open serial file */
      if ((serfp = fopen(SERIAL, "r")) != NULL) {

	 while ((fgets(line, 256, serfp) != NULL) && (sl < serial_used)) {

	    if ((p = strstr(line, "tx:")) != NULL) {
	       /*
		* Read the number of chrs transmitted and received by
		* current serial line.
		*/
	       sscanf(line, "%d", &tty);
	       st_serial_i = st_serial + sl;
	       sscanf(p + 3, "%d", &(st_serial_i->tx));
	       sscanf(strstr(line, "rx:") + 3, "%d", &(st_serial_i->rx));

	       st_serial_i->line = tty;
	       sl++;
	    }
	 }

	 /* Close serial file */
	 fclose(serfp);
      }
#endif

   while (sl < serial_used) {
      /*
       * Nb of serial lines has changed, or appending data to an old file
       * with more serial lines than are actually available now.
       */
      st_serial_i = st_serial + sl++;
      st_serial_i->line = 0xff;
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/interrupts
 ***************************************************************************
 */
void read_interrupts_stat(void)
{
   FILE *irqfp;
   static char line[1024];	/* Should depend on the nb of processors */
   int irq = 0, cpu;
   struct stats_irq_cpu *p;

   /* Open interrupts file */
   if ((irqfp = fopen(INTERRUPTS, "r")) != NULL) {

      while ((fgets(line, 1024, irqfp) != NULL) && (irq < irqcpu_used)) {

	 if (isdigit(line[2])) {
	
	    p = st_irq_cpu + irq;
	    sscanf(line, "%3d", &(p->irq));
	
	    for (cpu = 0; cpu <= cpu_nr; cpu++) {
	       p = st_irq_cpu + cpu * irqcpu_used + irq;
	       /*
		* No need to set (st_irq_cpu + cpu * irqcpu_used)->irq:
		* same as st_irq_cpu->irq.
		*/
	       sscanf(line + 4 + 11 * cpu, " %10u", &(p->interrupt));
	    }
	    irq++;
	 }
      }

      /* Close serial file */
      fclose(irqfp);
   }

   while (irq < irqcpu_used) {
      /*
       * Nb of interrupts per processor has changed, or appending data to an
       * old file with more interrupts than are actually available now.
       */
      p = st_irq_cpu + irq;
      p->irq = ~0;	/* This value means this is a dummy interrupt */
      irq++;
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/sys/fs/...
 * (see linux source file linux/kernel/sysctl.c)
 * Some files may not exist, depending on the kernel configuration.
 ***************************************************************************
 */
void read_ktables_stat(void)
{
   FILE *ktfp;
   int parm;

   /* Open /proc/sys/fs/dentry-state file */
   if ((ktfp = fopen(FDENTRY_STATE, "r")) == NULL)
      file_stats.dentry_stat = 0;
   else {
      fscanf(ktfp, "%*d %u %*d %*d %*d %*d\n",
	     &(file_stats.dentry_stat));
      fclose(ktfp);
   }

   /* Open /proc/sys/fs/file-nr file */
   if ((ktfp = fopen(FFILE_NR, "r")) == NULL) {
      file_stats.file_used = 0;
      file_stats.file_max  = 0;
   }
   else {
      fscanf(ktfp, "%*d %u %u\n",
	     &(file_stats.file_used), &(file_stats.file_max));
      fclose(ktfp);
   }

   /* Open /proc/sys/fs/inode-state file */
   if ((ktfp = fopen(FINODE_STATE, "r")) == NULL)
      file_stats.inode_used = 0;
   else {
      fscanf(ktfp, "%u %u %*d %*d %*d %*d %*d\n",
	     &(file_stats.inode_used), &parm);
      fclose(ktfp);
      file_stats.inode_used -= parm;
   }

   /* Open /proc/sys/fs/super-max file */
   if ((ktfp = fopen(FSUPER_MAX, "r")) == NULL) {
      file_stats.super_max  = 0;
      file_stats.super_used = 0;
   }
   else {
      fscanf(ktfp, "%u\n",
	     &(file_stats.super_max));
      fclose(ktfp);

      /* Open /proc/sys/fs/super-nr file */
      if ((ktfp = fopen(FSUPER_NR, "r")) == NULL)
	 file_stats.super_used = 0;
      else {
	 fscanf(ktfp, "%u\n",
		&(file_stats.super_used));
	 fclose(ktfp);
      }
   }

   /* Open /proc/sys/fs/dquot-max file */
   if ((ktfp = fopen(FDQUOT_MAX, "r")) == NULL) {
      file_stats.dquot_max  = 0;
      file_stats.dquot_used = 0;
   }
   else {
      fscanf(ktfp, "%u\n",
	     &(file_stats.dquot_max));
      fclose(ktfp);

      /* Open /proc/sys/fs/dquot_nr file */
      if ((ktfp = fopen(FDQUOT_NR, "r")) == NULL)
	 file_stats.dquot_used = 0;
      else {
	 fscanf(ktfp, "%u %*u\n",
		&(file_stats.dquot_used));
	 fclose(ktfp);
      }
   }

   /* Open /proc/sys/kernel/rtsig-max file */
   if ((ktfp = fopen(FRTSIG_MAX, "r")) == NULL) {
      file_stats.rtsig_max    = 0;
      file_stats.rtsig_queued = 0;
   }
   else {
      fscanf(ktfp, "%u\n",
	     &(file_stats.rtsig_max));
      fclose(ktfp);

      /* Open /proc/sys/kernel/rtsig-nr file */
      if ((ktfp = fopen(FRTSIG_NR, "r")) == NULL)
	 file_stats.rtsig_queued = 0;
      else {
	 fscanf(ktfp, "%u\n",
		&(file_stats.rtsig_queued));
	 fclose(ktfp);
      }
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/net/dev
 * (see linux source file linux/net/core/dev.c and linux/include/linux/netdevice.h)
 ***************************************************************************
 */
void read_net_dev_stat(void)
{
   FILE *devfp;
   struct stats_net_dev *st_net_dev_i;
   static char line[256];
   int dev = 0;

   /* Open network device file */
   if ((devfp = fopen(NET_DEV, "r")) != NULL) {

      while ((fgets(line, 256, devfp) != NULL) && (dev < iface_used)) {

	 if (line[6] == ':') {
	    st_net_dev_i = st_net_dev + dev;
	    strncpy(st_net_dev_i->interface, line, 6);
	    st_net_dev_i->interface[6] = '\0';
	    sscanf(line + 7, "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
		   &(st_net_dev_i->rx_bytes),
		   &(st_net_dev_i->rx_packets),
		   &(st_net_dev_i->rx_errors),
		   &(st_net_dev_i->rx_dropped),
		   &(st_net_dev_i->rx_fifo_errors),
		   &(st_net_dev_i->rx_frame_errors),
		   &(st_net_dev_i->rx_compressed),
		   &(st_net_dev_i->multicast),
		   &(st_net_dev_i->tx_bytes),
		   &(st_net_dev_i->tx_packets),
		   &(st_net_dev_i->tx_errors),
		   &(st_net_dev_i->tx_dropped),
		   &(st_net_dev_i->tx_fifo_errors),
		   &(st_net_dev_i->collisions),
		   &(st_net_dev_i->tx_carrier_errors),
		   &(st_net_dev_i->tx_compressed));
	    dev++;
	 }
      }

      /* Close serial file */
      fclose(devfp);
   }

   if (dev < iface_used) {
      /* Reset unused structures */
      memset(st_net_dev + dev, 0, STATS_NET_DEV_SIZE * (iface_used - dev));

      while (dev < iface_used) {
	 /*
	  * Nb of network interfaces has changed, or appending data to an
	  * old file with more interfaces than are actually available now.
	  */
	 st_net_dev_i = st_net_dev + dev++;
	 strcpy(st_net_dev_i->interface, "?");
      }
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/net/sockstat
 * (see afinet_get_info() function in linux source file linux/net/ipv4/proc.c
 *  and socket_get_info() function in linux source file linux/net/socket.c)
 ***************************************************************************
 */
void read_net_sock_stat(void)
{
   FILE *sockfp;
   static char line[96];

   file_stats.sock_inuse = 0;
   file_stats.tcp_inuse  = 0;
   file_stats.udp_inuse  = 0;
   file_stats.raw_inuse  = 0;
   file_stats.frag_inuse = 0;

   /* Open /proc/net/sockstat file */
   if ((sockfp = fopen(NET_SOCKSTAT, "r")) != NULL) {

      while (fgets(line, 96, sockfp) != NULL) {
	
	 if (!strncmp(line, "sockets:", 8))
	    /* Sockets */
	    sscanf(line + 14, "%d", &(file_stats.sock_inuse));
	 else if (!strncmp(line, "TCP:", 4))
	    /* TCP sockets */
	    sscanf(line + 11, "%d", &(file_stats.tcp_inuse));
	 else if (!strncmp(line, "UDP:", 4))
	    /* UDP sockets */
	    sscanf(line + 11, "%d", &(file_stats.udp_inuse));
	 else if (!strncmp(line, "RAW:", 4))
	    /* RAW sockets */
	    sscanf(line + 11, "%d", &(file_stats.raw_inuse));
	 else if (!strncmp(line, "FRAG:", 5))
	    /* FRAGments */
	    sscanf(line + 12, "%d", &(file_stats.frag_inuse));
      }
   }

   /* Close socket file */
   fclose(sockfp);
}


/*
 ***************************************************************************
 * Main entry to the program
 ***************************************************************************
 */
int main(int argc, char **argv)
{
   int opt = 0;
   unsigned long pid;
   char ofile[MAX_FILE_LEN];
   char new_ofile[MAX_FILE_LEN];
   unsigned int flags = 0;
   struct tm loc_time;
   int ofd;
   /*
    * This variable contains:
    * - FILE_STATS_SIZE defined in sa.h if creating a new daily data file or
    *   using STDOUT,
    * - the size of the file_stats structure defined in the header of the
    *   file if appending data to an existing daily data file.
    */
   size_t file_stats_size = FILE_STATS_SIZE;

   /* Compute page shift in kB */
   kb_shift = get_kb_shift();

   ofile[0] = new_ofile[0] = '\0';

#ifdef USE_NLS
   /* Init National Language Support */
   init_nls();
#endif

   if (argc == 1) {
      /* sadc called with no args */
      get_localtime(&loc_time);
      snprintf(ofile, MAX_FILE_LEN, "%s/sa%02d", SA_DIR, loc_time.tm_mday);
      ofile[MAX_FILE_LEN - 1] = '\0';
   }

   /* Init activity flag */
   sadc_actflag = A_PROC + A_PAGE + A_IRQ + A_IO + A_CPU + A_CTXSW + A_SWAP +
                  A_MEMORY + A_MEM_AMT + A_KTABLES + A_NET_SOCK + A_QUEUE;

   /* Init stat structure */
   memset(&file_stats, 0, FILE_STATS_SIZE);

   /* How many processors on this machine ? */
   get_cpu_nr(&cpu_nr, NR_CPUS);
   if (cpu_nr > 0)
      salloc_cpu(cpu_nr + 1);

   /* Get serial lines that support accounting */
   get_serial_lines(&serial_used);
   if (serial_used) {
      sadc_actflag |= A_SERIAL;
      salloc_serial(serial_used);
   }
   /* Get number of interrupts available per processor */
   if (cpu_nr > 0) {
      get_irqcpu_nb(&irqcpu_used, NR_IRQS);
      if (irqcpu_used)
	 salloc_irqcpu(cpu_nr + 1, irqcpu_used);
   }
   else
      /* IRQ per processor are not provided by sadc on UP machines */
      irqcpu_used = 0;

   /* Get number of network devices (interfaces) */
   get_net_dev(&iface_used);
   if (iface_used) {
      sadc_actflag |= A_NET_DEV + A_NET_EDEV;
      salloc_net_dev(iface_used);
   }
   /* Get number of disk_io entries in /proc/stat */
   if ((disk_used = get_disk_io_nr()) > 0) {
      sadc_actflag |= A_DISK;
      disk_used += NR_DISK_PREALLOC;
      salloc_disk(disk_used);
   }

   while (++opt < argc) {

      if (!strcmp(argv[opt], "-I"))
	 sadc_actflag |= A_ONE_IRQ;

      else if (!strcmp(argv[opt], "-V"))
	 usage(argv[0]);

      else if (!strcmp(argv[opt], "-x") || !strcmp(argv[opt], "-X")) {
	 if (!strcmp(argv[++opt], K_SUM)) {
	    sadc_actflag |= A_SUM_PID;
	    continue;	/* Next option */
	 }
	 if (!GET_PID(sadc_actflag))
	    /* Get PID list */
	    get_pid_list();
	 sadc_actflag |= A_PID;
	 if (!strcmp(argv[opt], K_ALL)) {
	    set_pflag(strcmp(argv[opt - 1], "-x"), 0);
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

	 set_pflag(strcmp(argv[opt - 1], "-x"), pid);
      }

      else if (strspn(argv[opt], DIGITS) != strlen(argv[opt])) {
	 if (!ofile[0]) {
	    if (!strcmp(argv[opt], "-")) {
	       /* File name set to '-' */
	       get_localtime(&loc_time);
	       snprintf(ofile, MAX_FILE_LEN,
			"%s/sa%02d", SA_DIR, loc_time.tm_mday);
	       ofile[MAX_FILE_LEN - 1] = '\0';
	       flags |= F_SA_ROTAT;
	    }
	    else if (!strncmp(argv[opt], "-", 1))
	       /* Bad option */
	       usage(argv[0]);
	    else {
	       /* Write data to file */
	       strncpy(ofile, argv[opt], MAX_FILE_LEN);
	       ofile[MAX_FILE_LEN - 1] = '\0';
	    }
	 }
	 else
	    /* Outfile already specified */
	    usage(argv[0]);
      }

      else if (!interval) {		/* Get interval */
	 interval = atol(argv[opt]);
	 if (interval < 1)
	   usage(argv[0]);
	 count = -1;
      }

      else if (count <= 0) {		/* Get count value */
	 count = atol(argv[opt]);
	 if (count < 1)
	   usage(argv[0]);
      }

      else
	 usage(argv[0]);
   }

   /* -x and -X options ignored when writing to a file */
   if (ofile[0]) {
      pid_nr = 0;
      sadc_actflag &= ~A_PID;
      sadc_actflag &= ~A_SUM_PID;
   }

   if (GET_PID(sadc_actflag))
      /* Count number of processes to display */
      pid_nr = count_pids();

   /* Open output file and write header */
   open_ofile(&ofd, ofile, &file_stats_size);

   if (!interval) {
      /* Interval (and count) not set: write a dummy record and exit */
      write_dummy_record(ofd, file_stats_size);
      close(ofd);
      exit(0);
   }

   /* Set a handler for SIGALRM */
   alarm_handler(0);

   /* Set record type. Done only once. */
   file_stats.record_type = R_STATS;

   /* Main loop */
   do {

      /*
       * Resetting the structure is not needed since all the fields
       * (that will be written) will be filled. If one of them is not
       * filled, this is because its value cannot be read from /proc
       * with current kernel, which doesn't matter since the structure
       * has been set to zero at the beginning (see above).
       */

      /* Save time */
      file_stats.ust_time = get_localtime(&loc_time);

      file_stats.hour   = loc_time.tm_hour;
      file_stats.minute = loc_time.tm_min;
      file_stats.second = loc_time.tm_sec;

      /* Read stats */
      read_proc_stat();
      read_proc_meminfo();
      read_proc_loadavg();
      read_ktables_stat();
      read_net_sock_stat();
      if (pid_nr)
 	 read_pid_stat();
      if (GET_SUM_PID(sadc_actflag))
	 read_sysfaults();
      if (serial_used)
	 read_serial_stat();
      if (irqcpu_used)
	 read_interrupts_stat();
      if (iface_used)
	 read_net_dev_stat();

      /* Write stats */
      write_stats(ofd, file_stats_size);

      if DO_SA_ROTAT(flags) {
	 /*
	  * Stats are written at the end of previous file *and* at the
	  * beginning of the new one.
	  */
	 flags &= ~F_DO_SA_ROTAT;
	 if (fdatasync(ofd) < 0) {	/* Flush previous file */
	    perror("fdatasync");
	    exit(4);
	 }
	 close(ofd);
	 strcpy(ofile, new_ofile);
	 open_ofile(&ofd, ofile, &file_stats_size);	/* Open and init new file */
	 write_stats(ofd, file_stats_size);		/* Write stats again */
      }

      /* Flush data */
      fflush(stdout);
      if (ofile[0] && (fdatasync(ofd) < 0)) {
	 perror("fdatasync");
	 exit(4);
      }

      if (count > 0)
	 count--;

      if (count)
	 pause();

      /* Rotate activity file if necessary */
      if (WANT_SA_ROTAT(flags)) {
	 /* The user specified '-' as the filename to use */
	 get_localtime(&loc_time);
	 snprintf(new_ofile, MAX_FILE_LEN,
		  "%s/sa%02d", SA_DIR, loc_time.tm_mday);
	 new_ofile[MAX_FILE_LEN - 1] = '\0';

	 if (strcmp(ofile, new_ofile))
	    flags |= F_DO_SA_ROTAT;
      }
   }
   while (count);

   /* Close output file */
   close(ofd);

   return 0;
}
