/*
 * sar, sadc, mpstat and iostat common routines.
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
#include <time.h>
#include <errno.h>
#include <unistd.h>	/* For STDOUT_FILENO, among others */
#include <dirent.h>
#include <sys/types.h>
#include <sys/ioctl.h>

/*
 * For PAGE_SIZE (which may be itself a call to getpagesize()).
 * PAGE_SHIFT no longer necessarily exists in <asm/page.h>. So
 * we use PAGE_SIZE to compute PAGE_SHIFT...
 */
#include <asm/page.h>

#include "common.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif


/*
 ***************************************************************************
 * Get current date or time
 ***************************************************************************
 */
time_t get_localtime(struct tm *loc_time)
{
   time_t timer;
   struct tm *ltm;

   time(&timer);
   ltm = localtime(&timer);

   *loc_time = *ltm;
   return timer;
}


/*
 ***************************************************************************
 * Find number of processors used on the machine
 * (0 means one proc, 1 means two proc, etc.)
 * As far as I know, there are two possibilities for this:
 * 1) Use /proc/stat or 2) Use /proc/cpuinfo
 * (I haven't heard of a better method to guess it...)
 ***************************************************************************
 */
int get_cpu_nr(int *cpu_nr, unsigned int max_nr_cpus)
{
   FILE *statfp;
   char line[16];
   int proc_nb, smp_box;

   *cpu_nr = -1;

   /* Open stat file */
   if ((statfp = fopen(STAT, "r")) == NULL) {
      fprintf(stderr, _("Cannot open %s: %s\n"), STAT, strerror(errno));
      exit(1);
   }

   while (fgets(line, 16, statfp) != NULL) {

      if (strncmp(line, "cpu ", 4) && !strncmp(line, "cpu", 3)) {
	 sscanf(line + 3, "%d", &proc_nb);
	 if (proc_nb > *cpu_nr)
	   *cpu_nr = proc_nb;
      }
   }

   /*
    * cpu_nr initial value: -1
    * If cpu_nr < 0 then there is only one proc.
    * If cpu_nr > 0 then this is an SMP machine.
    * If cpu_nr = 0 then there is only one proc but this is a Linux 2.2 SMP or
    * Linux 2.4 kernel.
    */
   smp_box = (*cpu_nr > 0);
   if (*cpu_nr < 0)
      *cpu_nr = 0;

   if (*cpu_nr >= max_nr_cpus) {
      fprintf(stderr, _("Cannot handle so many processors!\n"));
      exit(1);
   }

   /* Close file */
   fclose(statfp);

   return smp_box;
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

   sprintf(dfile, "%s/%s", SYSFS_BLOCK, dev_name);

   /* Open current device directory in /sys/block */
   if ((dir = opendir(dfile)) == NULL)
      return 0;

   /* Get current file entry */
   while ((drd = readdir(dir)) != NULL) {
      if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
	 continue;
      sprintf(line, "%s/%s/%s", dfile, drd->d_name, S_STAT);

      /* Try to guess if current entry is a directory containing a stat file */
      if (!access(line, R_OK))
	 /* Yep... */
	 part++;
   }

   /* Close directory */
   (void) closedir(dir);

   return part;
}


/*
 ***************************************************************************
 * Look for block devices present in /sys/ filesystem:
 * Check first that sysfs is mounted (done by trying to open /sys/block
 * directory), then find number of devices registered.
 ***************************************************************************
 */
int get_sysfs_dev_nr(int flags)
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
      sprintf(line, "%s/%s/%s", SYSFS_BLOCK, drd->d_name, S_STAT);

      /* Try to guess if current entry is a directory containing a stat file */
      if (!access(line, R_OK)) {
	 /* Yep... */
	 dev++;
	
	 if (DISPLAY_PARTITIONS(flags))
	    /* We also want the number of partitions for this device */
	    dev += get_dev_part_nr(drd->d_name);
      }
   }

    /* Close /sys/block directory */
   (void) closedir(dir);

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
   FILE *statfp;
   char line[8192];
   unsigned int dsk = 0;
   int pos;

   /* Open /proc/stat file */
   if ((statfp = fopen(STAT, "r")) == NULL) {
      fprintf(stderr, _("Cannot open %s: %s\n"), STAT, strerror(errno));
      exit(2);
   }

   while (fgets(line, 8192, statfp) != NULL) {

      if (!strncmp(line, "disk_io: ", 9)) {
	 for (pos = 9; pos < strlen(line) - 1; pos +=strcspn(line + pos, " ") + 1)
	    dsk++;
      }
   }

   /* Close /proc/stat file */
   fclose(statfp);

   return dsk;
}


/*
 ***************************************************************************
 * Print banner
 ***************************************************************************
 */
inline void print_gal_header(struct tm *loc_time, char *sysname, char *release, char *nodename)
{
   char cur_date[64];
   char *e;

   if (((e = getenv(TM_FMT_VAR)) != NULL) && !strcmp(e, K_ISO))
      strftime(cur_date, sizeof(cur_date), "%Y-%m-%d", loc_time);
   else
      strftime(cur_date, sizeof(cur_date), "%x", loc_time);

   printf("%s %s (%s) \t%s\n", sysname, release, nodename, cur_date);
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
 * Get window height (number of lines)
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


   if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) != -1) && (win.ws_row > 2))
      rows = win.ws_row - 2;

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
int get_kb_shift(void)
{
   int shift = 0;
   int size;

   size = PAGE_SIZE >> 10; /* Assume that a page has a minimum size of 1 kB */
   while (size > 1) {
      shift++;
      size >>= 1;
   }

   return shift;
}
