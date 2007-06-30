/*
 * sar, sadc, sadf, mpstat and iostat common routines.
 * (C) 1999-2007 by Sebastien GODARD (sysstat <at> wanadoo.fr)
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
#include <time.h>
#include <errno.h>
#include <unistd.h>	/* For STDOUT_FILENO, among others */
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "version.h"
#include "common.h"
#include "ioconf.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

/* Number of ticks per second */
unsigned int hz;
/* Number of bit shifts to convert pages to kB */
unsigned int kb_shift;

/*
 ***************************************************************************
 * Print sysstat version number and exit
 ***************************************************************************
 */
void print_version(void)
{
   fprintf(stderr, _("sysstat version %s\n"), VERSION);
   fprintf(stderr, "(C) Sebastien Godard (sysstat <at> wanadoo.fr)\n");
   exit(1);
}


/*
 ***************************************************************************
 * Get date and time and take into account <ENV_TIME_DEFTM> variable
 ***************************************************************************
 */
time_t get_time(struct tm *rectime)
{
   time_t timer;
   struct tm *ltm;
   static int utc = 0;
   char *e;

   if (!utc) {
      /* Read environment variable value once */
      if ((e = getenv(ENV_TIME_DEFTM)) != NULL)
	 utc = !strcmp(e, K_UTC);
      utc++;
   }

   time(&timer);
   if (utc ==  2)
      ltm = gmtime(&timer);
   else
      ltm = localtime(&timer);

   *rectime = *ltm;
   return timer;
}


/*
 ***************************************************************************
 * Get local date and time
 ***************************************************************************
 */
time_t get_localtime(struct tm *rectime)
{
   time_t timer;
   struct tm *ltm;

   time(&timer);
   ltm = localtime(&timer);

   *rectime = *ltm;
   return timer;
}


/*
 ***************************************************************************
 * Count number of processors in /sys
 ***************************************************************************
 */
int get_sys_cpu_nr(void)
{
   DIR *dir;
   struct dirent *drd;
   struct stat buf;
   char line[MAX_PF_NAME];
   int proc_nr = 0;

   /* Open relevant /sys directory */
   if ((dir = opendir(SYSFS_DEVCPU)) == NULL)
      return 0;

   /* Get current file entry */
   while ((drd = readdir(dir)) != NULL) {

      if (!strncmp(drd->d_name, "cpu", 3)) {
	 snprintf(line, MAX_PF_NAME, "%s/%s", SYSFS_DEVCPU, drd->d_name);
	 line[MAX_PF_NAME - 1] = '\0';
	 if (stat(line, &buf) < 0)
	    continue;
	 if (S_ISDIR(buf.st_mode))
	    proc_nr++;
      }
   }

   /* Close directory */
   closedir(dir);

   return proc_nr;
}


/*
 ***************************************************************************
 * Count number of processors in /proc/stat
 ***************************************************************************
 */
int get_proc_cpu_nr(void)
{
   FILE *fp;
   char line[16];
   int num_proc, proc_nr = -1;

   if ((fp = fopen(STAT, "r")) == NULL) {
      fprintf(stderr, _("Cannot open %s: %s\n"), STAT, strerror(errno));
      exit(1);
   }

   while (fgets(line, 16, fp) != NULL) {

      if (strncmp(line, "cpu ", 4) && !strncmp(line, "cpu", 3)) {
	 sscanf(line + 3, "%d", &num_proc);
	 if (num_proc > proc_nr)
	    proc_nr = num_proc;
      }
   }

   fclose(fp);

   return (proc_nr + 1);
}


/*
 ***************************************************************************
 * Return the number of processors used on the machine:
 * 0: one proc and non SMP kernel
 * 1: one proc and SMP kernel (NB: on SMP machines where all the CPU's but
 *    one have been disabled, we get the total number of proc since we use
 *    /sys to count them).
 * 2: two proc...
 * Try to use /sys for that, or /proc/stat if /sys doesn't exist.
 ***************************************************************************
 */
int get_cpu_nr(unsigned int max_nr_cpus)
{
   int cpu_nr;

   if ((cpu_nr = get_sys_cpu_nr()) == 0)
      /* /sys may be not mounted. Use /proc/stat instead */
      cpu_nr = get_proc_cpu_nr();

   if (cpu_nr > max_nr_cpus) {
      fprintf(stderr, _("Cannot handle so many processors!\n"));
      exit(1);
   }

   return cpu_nr;
}


/*
 ***************************************************************************
 * Look for partitions of a given block device in /sys filesystem
 ***************************************************************************
 */
int get_dev_part_nr(char *dev_name)
{
   DIR *dir;
   struct dirent *drd;
   char dfile[MAX_PF_NAME], line[MAX_PF_NAME];
   int part = 0;

   snprintf(dfile, MAX_PF_NAME, "%s/%s", SYSFS_BLOCK, dev_name);
   dfile[MAX_PF_NAME - 1] = '\0';

   /* Open current device directory in /sys/block */
   if ((dir = opendir(dfile)) == NULL)
      return 0;

   /* Get current file entry */
   while ((drd = readdir(dir)) != NULL) {
      if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
	 continue;
      snprintf(line, MAX_PF_NAME, "%s/%s/%s", dfile, drd->d_name, S_STAT);
      line[MAX_PF_NAME - 1] = '\0';

      /* Try to guess if current entry is a directory containing a stat file */
      if (!access(line, R_OK))
	 /* Yep... */
	 part++;
   }

   /* Close directory */
   closedir(dir);

   return part;
}


/*
 ***************************************************************************
 * Look for block devices present in /sys/ filesystem:
 * Check first that sysfs is mounted (done by trying to open /sys/block
 * directory), then find number of devices registered.
 ***************************************************************************
 */
int get_sysfs_dev_nr(int display_partitions)
{
   DIR *dir;
   struct dirent *drd;
   char line[MAX_PF_NAME];
   int dev = 0;

   /* Open /sys/block directory */
   if ((dir = opendir(SYSFS_BLOCK)) == NULL)
      /* sysfs not mounted, or perhaps this is an old kernel */
      return 0;

   /* Get current file entry in /sys/block directory */
   while ((drd = readdir(dir)) != NULL) {
      if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
	 continue;
      snprintf(line, MAX_PF_NAME, "%s/%s/%s", SYSFS_BLOCK, drd->d_name, S_STAT);
      line[MAX_PF_NAME - 1] = '\0';

      /* Try to guess if current entry is a directory containing a stat file */
      if (!access(line, R_OK)) {
	 /* Yep... */
	 dev++;
	
	 if (display_partitions)
	    /* We also want the number of partitions for this device */
	    dev += get_dev_part_nr(drd->d_name);
      }
   }

    /* Close /sys/block directory */
   closedir(dir);

   return dev;
}


/*
 ***************************************************************************
 * Find number of devices and partitions available in /proc/diskstats.
 * Args: count_part : set to TRUE if devices _and_ partitions are to be
 *		counted.
 *       only_used_dev : when counting devices, set to TRUE if only devices
 *		with non zero stats are to be counted.
 ***************************************************************************
 */
int get_diskstats_dev_nr(int count_part, int only_used_dev)
{
   FILE *fp;
   char line[256];
   int dev = 0, i;
   unsigned long rd_ios, wr_ios;

   if ((fp = fopen(DISKSTATS, "r")) == NULL)
      /* File non-existent */
      return 0;

   /*
    * Counting devices and partitions is simply a matter of counting
    * the number of lines...
    */
   while (fgets(line, 256, fp) != NULL) {
      if (!count_part) {
	 i = sscanf(line, "%*d %*d %*s %lu %*u %*u %*u %lu",
		    &rd_ios, &wr_ios);
	 if (i == 1)
	    /* It was a partition and not a device */
	    continue;
	 if (only_used_dev && !rd_ios && !wr_ios)
	    /* Unused device */
	    continue;
      }
      dev++;
   }

   fclose(fp);

   return dev;
}


/*
 ***************************************************************************
 * Find number of devices and partitions that have statistics in
 * /proc/partitions.
 ***************************************************************************
 */
int get_ppartitions_dev_nr(int count_part)
{
   FILE *fp;
   char line[256];
   int dev = 0;
   unsigned int major, minor, tmp;

   if ((fp = fopen(PPARTITIONS, "r")) == NULL)
      /* File non-existent */
      return 0;

   while (fgets(line, 256, fp) != NULL) {
      if (sscanf(line, "%u %u %*u %*s %u", &major, &minor, &tmp) == 3) {
	 /*
	  * We have just read a line from /proc/partitions containing stats
	  * for a device or a partition (i.e. this is not a fake line:
	  * header, blank line,... or a line without stats!)
	  */
	 if (!count_part && !ioc_iswhole(major, minor))
	    /* This was a partition, and we don't want to count them */
	    continue;
	 dev++;
      }
   }

   fclose(fp);

   return dev;
}


/*
 ***************************************************************************
 * Find number of disk entries that are registered on the
 * "disk_io:" line in /proc/stat.
 ***************************************************************************
 */
unsigned int get_disk_io_nr(void)
{
   FILE *fp;
   char line[8192];
   unsigned int dsk = 0;
   int pos;

   if ((fp = fopen(STAT, "r")) == NULL) {
      fprintf(stderr, _("Cannot open %s: %s\n"), STAT, strerror(errno));
      exit(2);
   }

   while (fgets(line, 8192, fp) != NULL) {

      if (!strncmp(line, "disk_io: ", 9)) {
	 for (pos = 9; pos < strlen(line) - 1; pos +=strcspn(line + pos, " ") + 1)
	    dsk++;
      }
   }

   fclose(fp);

   return dsk;
}


/*
 ***************************************************************************
 * Find number of NFS-mounted points that are registered in
 * /proc/self/mountstats.
 ***************************************************************************
 */
int get_nfs_mount_nr(void)
{
   FILE *fp;
   char line[8192];
   char type_name[10];
   unsigned int nfs = 0;

   if ((fp = fopen(NFSMOUNTSTATS, "r")) == NULL)
      /* File non-existent */
      return 0;

   while (fgets(line, 8192, fp) != NULL) {

      if ((strstr(line, "mounted")) && (strstr(line, "on")) && (strstr(line, "with")) &&
	  (strstr(line, "fstype"))) {
	
	 sscanf(strstr(line, "fstype") + 6, "%10s", type_name);
	 if ((!strncmp(type_name, "nfs", 3)) && (strncmp(type_name, "nfsd", 4))) {
	    nfs ++;
	 }
      }
   }

   fclose(fp);

   return nfs;
}


/*
 ***************************************************************************
 * Print banner
 * Return code: 1 if S_TIME_FORMAT is set to ISO, or 0 otherwise.
 ***************************************************************************
 */
int print_gal_header(struct tm *rectime, char *sysname, char *release, char *nodename)
{
   char cur_date[64];
   char *e;
   int rc = 0;

   if (((e = getenv(ENV_TIME_FMT)) != NULL) && !strcmp(e, K_ISO)) {
      strftime(cur_date, sizeof(cur_date), "%Y-%m-%d", rectime);
      rc = 1;
   }
   else
      strftime(cur_date, sizeof(cur_date), "%x", rectime);

   printf("%s %s (%s) \t%s\n", sysname, release, nodename, cur_date);
   
   return rc;
}


#ifdef USE_NLS
/*
 ***************************************************************************
 * Init National Language Support
 ***************************************************************************
 */
void init_nls(void)
{
   setlocale(LC_MESSAGES, "");
   setlocale(LC_CTYPE, "");
   setlocale(LC_TIME, "");
   setlocale(LC_NUMERIC, "");

   bindtextdomain(PACKAGE, LOCALEDIR);
   textdomain(PACKAGE);
}
#endif


/*
 ***************************************************************************
 * Get nb of rows and columns of current window
 ***************************************************************************
 */
int get_win_height(void)
{
   struct winsize win;
   /*
    * This default value will be used whenever STDOUT
    * is redirected to a pipe or a file
    */
   int rows = 3600 * 24;

   if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) != -1) {
      if (win.ws_row > 2)
	 rows = win.ws_row - 2;
   }
   return rows;
}


/*
 ***************************************************************************
 * Remove /dev from path name
 ***************************************************************************
 */
char *device_name(char *name)
{
   if (!strncmp(name, "/dev/", 5))
      return name + 5;

   return name;
}


/*
 ***************************************************************************
 * Get page shift in kB
 ***************************************************************************
 */
void get_kb_shift(void)
{
   int shift = 0;
   long size;

   /* One can also use getpagesize() to get the size of a page */
   if ((size = sysconf(_SC_PAGESIZE)) == -1)
      perror("sysconf");

   size >>= 10;	/* Assume that a page has a minimum size of 1 kB */

   while (size > 1) {
      shift++;
      size >>= 1;
   }

   kb_shift = (unsigned int) shift;
}


/*
 ***************************************************************************
 * Get number of clock ticks per second
 ***************************************************************************
 */
void get_HZ(void)
{
   long ticks;

   if ((ticks = sysconf(_SC_CLK_TCK)) == -1)
      perror("sysconf");

   hz = (unsigned int) ticks;
}


/*
 ***************************************************************************
 * Handle overflow conditions properly for counters which are read as
 * unsigned long long, but which can be unsigned long long or
 * unsigned long only depending on the kernel version used.
 * @value1 and @value2 being two values successively read for this
 * counter, if @value2 < @value1 and @value1 <= 0xffffffff, then we can
 * assume that the counter's type was unsigned long and has overflown, and
 * so the difference @value2 - @value1 must be casted to this type.
 * NOTE: These functions should no longer be necessary to handle a particular
 * stat counter when we can assume that everybody is using a recent kernel
 * (defining this counter as unsigned long long).
 ***************************************************************************
 */
double ll_sp_value(unsigned long long value1, unsigned long long value2,
		   unsigned long long itv)
{
   if ((value2 < value1) && (value1 <= 0xffffffff))
      /* Counter's type was unsigned long and has overflown */
      return ((double) ((value2 - value1) & 0xffffffff)) / itv * 100;
   else
      return SP_VALUE(value1, value2, itv);
}

double ll_s_value(unsigned long long value1, unsigned long long value2,
		  unsigned long long itv)
{
   if ((value2 < value1) && (value1 <= 0xffffffff))
      /* Counter's type was unsigned long and has overflown */
      return ((double) ((value2 - value1) & 0xffffffff)) / itv * HZ;
   else
      return S_VALUE(value1, value2, itv);
}


/*
 ***************************************************************************
 * Compute time interval.
 * The interval should always be smaller than 0xffffffff (ULONG_MAX on
 * 32-bit architectures), except perhaps if it is the interval since
 * system startup (we want stats since boot time).
 * Interval is and'ed with mask 0xffffffff to handle overflow conditions
 * that may happen since uptime values are unsigned long long but are
 * calculated as a sum of values that _may_ be unsigned long only...
 ***************************************************************************
 */
unsigned long long get_interval(unsigned long long prev_uptime,
				unsigned long long curr_uptime)
{
   unsigned long long itv;
   
   if (!prev_uptime)
      itv = curr_uptime;
   else
      itv = (curr_uptime - prev_uptime) & 0xffffffff;
   if (!itv)	/* Paranoia checking */
      itv = 1;

   return itv;
}


/*
 ***************************************************************************
 * Read machine uptime, independently of the number of processors
 ***************************************************************************
 */
void readp_uptime(unsigned long long *uptime)
{
   FILE *fp;
   char line[128];
   unsigned long up_sec, up_cent;

   if ((fp = fopen(UPTIME, "r")) == NULL)
      return;

   if (fgets(line, 128, fp) == NULL)
      return;

   sscanf(line, "%lu.%lu", &up_sec, &up_cent);
   
   fclose(fp);
   
   *uptime = up_sec * HZ + up_cent * HZ / 100;
}


/*
 ***************************************************************************
 * Read stats from /proc/meminfo
 ***************************************************************************
 */
int readp_meminfo(struct meminf *st_mem)
{
   FILE *fp;
   static char line[128];

   if ((fp = fopen(MEMINFO, "r")) == NULL)
      return 1;

   while (fgets(line, 128, fp) != NULL) {

      if (!strncmp(line, "MemTotal:", 9))
	 /* Read the total amount of memory in kB */
	 sscanf(line + 9, "%lu", &(st_mem->tlmkb));
      else if (!strncmp(line, "MemFree:", 8))
	 /* Read the amount of free memory in kB */
	 sscanf(line + 8, "%lu", &(st_mem->frmkb));

      else if (!strncmp(line, "Buffers:", 8))
	 /* Read the amount of buffered memory in kB */
	 sscanf(line + 8, "%lu", &(st_mem->bufkb));

      else if (!strncmp(line, "Cached:", 7))
	 /* Read the amount of cached memory in kB */
	 sscanf(line + 7, "%lu", &(st_mem->camkb));

      else if (!strncmp(line, "SwapCached:", 11))
	 /* Read the amount of cached swap in kB */
	 sscanf(line + 11, "%lu", &(st_mem->caskb));

      else if (!strncmp(line, "SwapTotal:", 10))
	 /* Read the total amount of swap memory in kB */
	 sscanf(line + 10, "%lu", &(st_mem->tlskb));

      else if (!strncmp(line, "SwapFree:", 9))
	 /* Read the amount of free swap memory in kB */
	 sscanf(line + 9, "%lu", &(st_mem->frskb));
   }

   fclose(fp);
   
   return 0;
}

