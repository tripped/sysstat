/*
 * iostat: report CPU and I/O statistics
 * (C) 1998-2003 by Sebastien GODARD <sebastien.godard@wanadoo.fr>
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
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/param.h>	/* for HZ */

#include "version.h"
#include "iostat.h"
#include "common.h"


#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif


struct comm_stats  comm_stats[2];
struct io_stats *st_iodev[2];
struct io_hdr_stats *st_hdr_iodev;
struct io_dlist *st_dev_list;

struct tm loc_time;

/* Nb of devices (and possibly partitions if -p option was used) found */
int iodev_nr = 0;

/* Nb of devices entered on the command line */
int dlist_idx = 0;

long int interval = 0;
unsigned char timestamp[64];

/*
 * Nb of processors on the machine.
 * A value of 1 means two procs...
 */
int cpu_nr = -1;


/*
 ***************************************************************************
 * Print usage and exit
 ***************************************************************************
 */
void usage(char *progname)
{
   fprintf(stderr, _("sysstat version %s\n"
		   "(C) Sebastien Godard\n"
	           "Usage: %s [ options... ] [ <interval> [ <count> ] ]\n"
		   "Options are:\n"
		   "[ -c | -d ] [ -k ] [ -t ] [ -V ] [ -x ]\n"
		   "[ <device> [ ... ] ] [ -p { ALL | <device> } ]\n"),
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
 * Initialize stats common structures
 ***************************************************************************
 */
void init_stats(void)
{
   memset(&comm_stats[0], 0, sizeof(struct comm_stats));
   memset(&comm_stats[1], 0, sizeof(struct comm_stats));
}


/*
 ***************************************************************************
 * Set every disk_io entry to inactive state
 ***************************************************************************
 */
void set_entries_inactive(int iodev_nr)
{
   int i;
   struct io_hdr_stats *st_hdr_iodev_i;

   for (i = 0; i < iodev_nr; i++) {
      st_hdr_iodev_i = st_hdr_iodev + i;
      st_hdr_iodev_i->active = 0;
   }
}


/*
 ***************************************************************************
 * Set structures's state to free for inactive entries
 ***************************************************************************
 */
void free_inactive_entries(int iodev_nr)
{
   int i;
   struct io_hdr_stats *st_hdr_iodev_i;

   for (i = 0; i < iodev_nr; i++) {
      st_hdr_iodev_i = st_hdr_iodev + i;
      if (!st_hdr_iodev_i->active)
	 st_hdr_iodev_i->major = 0;
   }
}


/*
 ***************************************************************************
 * Allocate and init I/O devices structures
 ***************************************************************************
 */
void salloc_device(int iodev_nr)
{
   int i;
   struct io_hdr_stats *st_hdr_iodev_i;

   for (i = 0; i < 2; i++) {
      if ((st_iodev[i] = (struct io_stats *) malloc(sizeof(struct io_stats) * iodev_nr)) == NULL) {
	 perror("malloc");
	 exit(4);
      }
      memset(st_iodev[i], 0, sizeof(struct io_stats) * iodev_nr);
   }

   if ((st_hdr_iodev = (struct io_hdr_stats *) malloc(sizeof(struct io_hdr_stats) * iodev_nr)) == NULL) {
      perror("malloc");
      exit(4);
   }
   memset(st_hdr_iodev, 0, sizeof(struct io_hdr_stats) * iodev_nr);

   if (iodev_nr == 4) {
      /*
       * We may have an old kernel with the stats for the first four disks
       * in /proc/stat: reset the devices name. Anyway, if we are wrong,
       * the names will be read from disk_io entry in /proc/stat or from
       * /sys.
       */
      for (i = 0; i < 4; i++) {
	 st_hdr_iodev_i = st_hdr_iodev + i;
	 sprintf(st_hdr_iodev_i->name, "hdisk%d", i);
      }
   }
}


/*
 ***************************************************************************
 * Allocate structures for devices entered on the command line
 ***************************************************************************
 */
void salloc_dev_list(int list_len)
{
   if ((st_dev_list = (struct io_dlist *) malloc(sizeof(struct io_dlist) * list_len)) == NULL) {
      perror("malloc");
      exit(4);
   }
   memset(st_dev_list, 0, sizeof(struct io_dlist) * list_len);
}


/*
 ***************************************************************************
 * Look for the device in the device list
 * and store it if necessary.
 ***************************************************************************
 */
int update_dev_list(int *dlist_idx, char *device_name)
{
   int i;
   struct io_dlist *st_dev_list_i;

   st_dev_list_i = st_dev_list;

   for (i = 0; i < *dlist_idx; i++) {
      st_dev_list_i = st_dev_list + i;
      if (!strcmp(st_dev_list_i->dev_name, device_name))
	 break;
   }

   if (i == *dlist_idx) {
      (*dlist_idx)++;
      st_dev_list_i = st_dev_list + i;
      strncpy(st_dev_list_i->dev_name, device_name, MAX_NAME_LEN - 1);
   }

   return i;
}


/*
 ***************************************************************************
 * Allocate and init structures, according to system state
 ***************************************************************************
 */
void io_sys_init(int *flags)
{
   /* Init stat common counters */
   init_stats();

   /* How many processors on this machine ? */
   get_cpu_nr(&cpu_nr, ~0);

   /* Get number of block devices (and partitions) in sysfs */
   if ((iodev_nr = get_sysfs_dev_nr(*flags)) > 0) {
      *flags |= F_HAS_SYSFS;
      iodev_nr += NR_DEV_PREALLOC;
   }
   /* Get number of "disk_io:" entries in /proc/stat */
   else if ((iodev_nr = get_disk_io_nr()) > 0)
      iodev_nr += NR_DISK_PREALLOC;
   else {
      /* Assume we have an old kernel: stats for 4 disks are in /proc/stat */
      iodev_nr = 4;
      *flags |= F_OLD_KERNEL;
   }
   /* Allocate structures for number of disks found */
   salloc_device(iodev_nr);
}


/*
 ***************************************************************************
 * Read stats from /proc/stat file...
 * (see linux source file linux/fs/proc/array.c)
 * Useful at least for CPU utilization.
 * May be useful to get disk stats if /sys not available.
 ***************************************************************************
 */
void read_stat(int curr, int flags)
{
   FILE *statfp;
   char line[1024];
   int pos, i;
   unsigned int v_tmp[3], v_major, v_index;
   struct io_stats *st_iodev_tmp[4], *st_iodev_i;
   struct io_hdr_stats *st_hdr_iodev_i;


   /*
    * Prepare pointers on the 4 disk structures in case we have a
    * /proc/stat file with "disk_rblk", etc. entries.
    */
   for (i = 0; i < 4; i++)
      st_iodev_tmp[i] = st_iodev[curr] + i;

   /* Open stat file */
   if ((statfp = fopen(STAT, "r")) == NULL) {
      perror("fopen");
      exit(2);
   }

   while (fgets(line, 1024, statfp) != NULL) {

      if (!strncmp(line, "cpu ", 4)) {
	 /*
	  * Read the number of jiffies spent in user, nice, system, idle
	  * and iowait mode and compute system uptime in jiffies (1/100ths
	  * of a second if HZ=100).
	  * Only in 2.5 is the iowait field present (representing # of jiffies
	  * spent waiting for I/O to complete). This was previously counted as
	  * idle time.
	  */
	 comm_stats[curr].cpu_iowait = 0;	/* For non 2.5 machines */
	 sscanf(line + 5, "%u %u %u %lu %lu",
	        &(comm_stats[curr].cpu_user), &(comm_stats[curr].cpu_nice),
		&(comm_stats[curr].cpu_system), &(comm_stats[curr].cpu_idle),
		&(comm_stats[curr].cpu_iowait));

	 /*
	  * Compute system uptime in jiffies.
	  * Uptime is multiplied by the number of processors.
	  */
	 comm_stats[curr].uptime = comm_stats[curr].cpu_user   +
	                           comm_stats[curr].cpu_nice +
	                           comm_stats[curr].cpu_system +
	                           comm_stats[curr].cpu_idle +
	                           comm_stats[curr].cpu_iowait;
      }

      else if (DISPLAY_EXTENDED(flags) || HAS_SYSFS(flags))
	 /*
	  * When displaying extended statistics, or if /sys is mounted,
	  * we just need to get CPU info from /proc/stat.
	  */
	 continue;

      else if (!strncmp(line, "disk_rblk ", 10)) {
	 /*
	  * Read the number of blocks read from disk.
	  * A block is of indeterminate size.
	  * The size may vary depending on the device type.
	  */
	 sscanf(line + 10, "%u %u %u %u",
		&v_tmp[0], &v_tmp[1], &v_tmp[2], &v_tmp[3]);

	 st_iodev_tmp[0]->dk_drive_rblk = v_tmp[0];
	 st_iodev_tmp[1]->dk_drive_rblk = v_tmp[1];
	 st_iodev_tmp[2]->dk_drive_rblk = v_tmp[2];
	 st_iodev_tmp[3]->dk_drive_rblk = v_tmp[3];
      }

      else if (!strncmp(line, "disk_wblk ", 10)) {
	 /* Read the number of blocks written to disk */
	 sscanf(line + 10, "%u %u %u %u",
		&v_tmp[0], &v_tmp[1], &v_tmp[2], &v_tmp[3]);
	
	 st_iodev_tmp[0]->dk_drive_wblk = v_tmp[0];
	 st_iodev_tmp[1]->dk_drive_wblk = v_tmp[1];
	 st_iodev_tmp[2]->dk_drive_wblk = v_tmp[2];
	 st_iodev_tmp[3]->dk_drive_wblk = v_tmp[3];
      }

      else if (!strncmp(line, "disk ", 5)) {
	 /* Read the number of I/O done since the last reboot */
	 sscanf(line + 5, "%u %u %u %u",
		&v_tmp[0], &v_tmp[1], &v_tmp[2], &v_tmp[3]);
	
	 st_iodev_tmp[0]->dk_drive = v_tmp[0];
	 st_iodev_tmp[1]->dk_drive = v_tmp[1];
	 st_iodev_tmp[2]->dk_drive = v_tmp[2];
	 st_iodev_tmp[3]->dk_drive = v_tmp[3];
      }

      else if (!strncmp(line, "disk_io: ", 9)) {
	 pos = 9;

	 /* Every disk_io entry is potentially unregistered */
	 set_entries_inactive(iodev_nr);
	
	 /* Read disks I/O statistics (for 2.4 kernels) */
	 while (pos < strlen(line) - 1) {
	    /* Beware: a CR is already included in the line */
	    sscanf(line + pos, "(%u,%u):(%u,%*u,%u,%*u,%u) ",
		   &v_major, &v_index, &v_tmp[0], &v_tmp[1], &v_tmp[2]);

	    /* Look for disk entry */
	    for (i = 0; i < iodev_nr; i++) {
	       st_hdr_iodev_i = st_hdr_iodev + i;
	       if ((st_hdr_iodev_i->major == v_major) &&
		   (st_hdr_iodev_i->index == v_index))
		  break;
	    }

	    if (i == iodev_nr) {
	       /*
		* New disk registered. Assume that disks may be registered,
		* *and* unregistered dynamically...
		* Look for a free structure to store it.
		*/
	       for (i = 0; i < iodev_nr; i++) {
		  st_hdr_iodev_i = st_hdr_iodev + i;
		  if (!st_hdr_iodev_i->major) {
		     /* Free structure found! */
		     st_hdr_iodev_i->major = v_major;
		     st_hdr_iodev_i->index = v_index;
		     sprintf(st_hdr_iodev_i->name, "dev%d-%d", v_major, v_index);
		     st_iodev_i = st_iodev[!curr] + i;
		     memset(st_iodev_i, 0, sizeof(struct io_stats));
		     break;
		  }
	       }
	    }
	
	    if (i < iodev_nr) {
	       st_hdr_iodev_i = st_hdr_iodev + i;
	       st_hdr_iodev_i->active = 1;
	       st_iodev_i = st_iodev[curr] + i;
	       st_iodev_i->dk_drive      = v_tmp[0];
	       st_iodev_i->dk_drive_rblk = v_tmp[1];
	       st_iodev_i->dk_drive_wblk = v_tmp[2];
	    }
	    /* else the disk_io entry was not found,
	     * and there was no free structure to store it */

	    pos += strcspn(line + pos, " ") + 1;
	 }

	 /* Free structures corresponding to unregistered disks */
	 free_inactive_entries(iodev_nr);
      }
   }

   /* Close stat file */
   fclose(statfp);
}


/*
 ***************************************************************************
 * Read sysfs stat for current block device or partition
 ***************************************************************************
 */
int read_sysfs_file_stat(int curr, char *filename, char *dev_name,
			  int dev_type)
{
   FILE *sysfp;
   struct io_stats sdev;
   struct io_stats *st_iodev_i;
   struct io_hdr_stats *st_hdr_iodev_i;
   int i;

   /* Try to read given stat file */
   if ((sysfp = fopen(filename, "r")) == NULL)
      return 0;
	
   if (dev_type == DT_DEVICE)
      i = (fscanf(sysfp, "%d %d %d %d %d %d %d %d %d %d %d",
		  &sdev.rd_ios, &sdev.rd_merges,
		  &sdev.rd_sectors, &sdev.rd_ticks,
		  &sdev.wr_ios, &sdev.wr_merges,
		  &sdev.wr_sectors, &sdev.wr_ticks,
		  &sdev.ios_pgr, &sdev.tot_ticks, &sdev.rq_ticks) == 11);
   else
      i = (fscanf(sysfp, "%d %d %d %d",
		  &sdev.rd_ios, &sdev.rd_sectors,
		  &sdev.wr_ios, &sdev.wr_sectors) == 4);

   if (i) {
      /* Look for device in data table */
      for (i = 0; i < iodev_nr; i++) {
	 st_hdr_iodev_i = st_hdr_iodev + i;
	 if (!strcmp(st_hdr_iodev_i->name, dev_name)) {
	    break;
	 }
      }
	
      if (i == iodev_nr) {
	 /*
	  * This is a new device: look for an unused entry to store it.
	  * Thus we are able to handle dynamically registered devices.
	  */
	 for (i = 0; i < iodev_nr; i++) {
	    st_hdr_iodev_i = st_hdr_iodev + i;
	    if (!st_hdr_iodev_i->major) {
	       /* Unused entry found... */
	       st_hdr_iodev_i->major = 1;	/* Just to indicate it is now used! */
	       strcpy(st_hdr_iodev_i->name, dev_name);
	       st_iodev_i = st_iodev[!curr] + i;
	       memset(st_iodev_i, 0, sizeof(struct io_stats));
	       break;
	    }
	 }
      }
      if (i < iodev_nr) {
	 st_hdr_iodev_i = st_hdr_iodev + i;
	 st_hdr_iodev_i->active = 1;
	 st_iodev_i = st_iodev[curr] + i;
	 *st_iodev_i = sdev;
      }
   }

   fclose(sysfp);

   return 1;
}


/*
 ***************************************************************************
 * Read sysfs stats for all the partitions of a device
 ***************************************************************************
 */
void read_sysfs_dlist_part_stat(int curr, char *dev_name)
{
   DIR *dir;
   struct dirent *drd;
   char dfile[MAX_PF_NAME], filename[MAX_PF_NAME];

   sprintf(dfile, "%s/%s", SYSFS_BLOCK, dev_name);

   /* Open current device directory in /sys/block */
   if ((dir = opendir(dfile)) == NULL)
      return;

   /* Get current entry */
   while ((drd = readdir(dir)) != NULL) {
      if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
	 continue;
      sprintf(filename, "%s/%s/%s", dfile, drd->d_name, S_STAT);

      /* Read current partition stats */
      read_sysfs_file_stat(curr, filename, drd->d_name, DT_PARTITION);
   }

   /* Close device directory */
   (void) closedir(dir);
}


/*
 ***************************************************************************
 * Read stats from the sysfs filesystem
 * for the devices entered on the command line
 ***************************************************************************
 */
void read_sysfs_dlist_stat(int curr, int flags)
{
   int dev, ok;
   char filename[MAX_PF_NAME];
   struct io_dlist *st_dev_list_i;

   /* Every I/O device (or partition) is potentially unregistered */
   set_entries_inactive(iodev_nr);

   for (dev = 0; dev < dlist_idx; dev++) {
      st_dev_list_i = st_dev_list + dev;
      sprintf(filename, "%s/%s/%s",
	      SYSFS_BLOCK, st_dev_list_i->dev_name, S_STAT);

      /* Read device stats */
      ok = read_sysfs_file_stat(curr, filename, st_dev_list_i->dev_name, DT_DEVICE);

      if (ok && st_dev_list_i->disp_part)
	 /* Also read stats for its partitions */
	 read_sysfs_dlist_part_stat(curr, st_dev_list_i->dev_name);
   }

   /* Free structures corresponding to unregistered devices */
   free_inactive_entries(iodev_nr);
}


/*
 ***************************************************************************
 * Read stats from the sysfs filesystem
 * for every block devices found
 ***************************************************************************
 */
void read_sysfs_stat(int curr, int flags)
{
   DIR *dir;
   struct dirent *drd;
   char filename[MAX_PF_NAME];
   int ok;

   /* Every I/O device entry is potentially unregistered */
   set_entries_inactive(iodev_nr);

   /* Open /sys/block directory */
   if ((dir = opendir(SYSFS_BLOCK)) != NULL) {

      /* Get current entry */
      while ((drd = readdir(dir)) != NULL) {
	 if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
	    continue;
	 sprintf(filename, "%s/%s/%s", SYSFS_BLOCK, drd->d_name, S_STAT);
	
	 /* If current entry is a directory, try to read its stat file */
	 ok = read_sysfs_file_stat(curr, filename, drd->d_name, DT_DEVICE);
	
	 /*
	  * If '-p ALL' was entered on the command line,
	  * also try to read stats for its partitions
	  */
	 if (ok && DISPLAY_PART_ALL(flags))
	    read_sysfs_dlist_part_stat(curr, drd->d_name);
      }

      /* Close /sys/block directory */
      (void) closedir(dir);
   }

   /* Free structures corresponding to unregistered devices */
   free_inactive_entries(iodev_nr);
}


/*
 ***************************************************************************
 * Display CPU utilization
 ***************************************************************************
 */
void write_cpu_stat(int curr, unsigned long itv)
{
   printf(_("avg-cpu:  %%user   %%nice    %%sys %%iowait   %%idle\n"));

   printf("         %6.2f  %6.2f  %6.2f  %6.2f",
	  SP_VALUE(comm_stats[!curr].cpu_user,   comm_stats[curr].cpu_user,   itv),
	  SP_VALUE(comm_stats[!curr].cpu_nice,   comm_stats[curr].cpu_nice,   itv),
	  SP_VALUE(comm_stats[!curr].cpu_system, comm_stats[curr].cpu_system, itv),
	  SP_VALUE(comm_stats[!curr].cpu_iowait, comm_stats[curr].cpu_iowait, itv));

   if (comm_stats[curr].cpu_idle < comm_stats[!curr].cpu_idle)
      printf("    %.2f", 0.0);
   else
      printf("  %6.2f",
	     SP_VALUE(comm_stats[!curr].cpu_idle, comm_stats[curr].cpu_idle, itv));

   printf("\n\n");
}


/*
 ***************************************************************************
 * Display extended stats (those read from sysfs)
 ***************************************************************************
 */
void write_ext_stat(int curr, unsigned long itv)
{
   int i;
   struct io_stats sdev;
   struct io_hdr_stats *st_hdr_iodev_i;
   struct io_stats *st_iodev_i, *st_iodev_j;
   double tput, util, await, svctm, arqsz, nr_ios;
	
   printf(_("Device:    rrqm/s wrqm/s   r/s   w/s  rsec/s  wsec/s    rkB/s    wkB/s avgrq-sz avgqu-sz   await  svctm  %%util\n"));
   /*       "/dev/xxxxx 999.99 999.99 99.99 99.99 9999.99 9999.99 99999.99 99999.99 99999.99 99999.99 9999.99 999.99 %999.99\n" */
	
   for (i = 0; i < iodev_nr; i++) {
      st_hdr_iodev_i = st_hdr_iodev + i;
      if (st_hdr_iodev_i->major) {
	 st_iodev_i = st_iodev[curr] + i;
	 st_iodev_j = st_iodev[!curr] + i;

	 sdev.rd_ios     = st_iodev_i->rd_ios - st_iodev_j->rd_ios;
	 sdev.wr_ios     = st_iodev_i->wr_ios - st_iodev_j->wr_ios;
	 sdev.rd_ticks   = st_iodev_i->rd_ticks - st_iodev_j->rd_ticks;
	 sdev.wr_ticks   = st_iodev_i->wr_ticks - st_iodev_j->wr_ticks;
	 sdev.rd_merges  = st_iodev_i->rd_merges - st_iodev_j->rd_merges;
	 sdev.wr_merges  = st_iodev_i->wr_merges - st_iodev_j->wr_merges;
	 sdev.rd_sectors = st_iodev_i->rd_sectors - st_iodev_j->rd_sectors;
	 sdev.wr_sectors = st_iodev_i->wr_sectors - st_iodev_j->wr_sectors;
	 sdev.tot_ticks  = st_iodev_i->tot_ticks - st_iodev_j->tot_ticks;
	 sdev.rq_ticks   = st_iodev_i->rq_ticks - st_iodev_j->rq_ticks;
	
	 nr_ios = sdev.rd_ios + sdev.wr_ios;
	 tput   = nr_ios * HZ / itv;
	 util   = ((double) sdev.tot_ticks) / itv;
	 svctm  = tput ? util / tput : 0.0;
	 /*
	  * kernel gives ticks already in milliseconds for all platforms
	  * => no need for further scaling.
	  */
	 await  = nr_ios ?
	    (sdev.rd_ticks + sdev.wr_ticks) / nr_ios : 0.0;
	 arqsz  = nr_ios ?
	    (sdev.rd_sectors + sdev.wr_sectors) / nr_ios : 0.0;

	 printf("/dev/%-5s", st_hdr_iodev_i->name);
	 if (strlen(st_hdr_iodev_i->name) > 5)
	    printf("\n          ");
	 /*       rrq/s wrq/s   r/s   w/s  rsec  wsec   rkB   wkB  rqsz  qusz await svctm %util */
	 printf(" %6.2f %6.2f %5.2f %5.2f %7.2f %7.2f %8.2f %8.2f %8.2f %8.2f %7.2f %6.2f %6.2f\n",
		((double) sdev.rd_merges) / itv * HZ,
		((double) sdev.wr_merges) / itv * HZ,
		((double) sdev.rd_ios) / itv * HZ,
		((double) sdev.wr_ios) / itv * HZ,
		((double) sdev.rd_sectors) / itv * HZ,
		((double) sdev.wr_sectors) / itv * HZ,
		((double) sdev.rd_sectors) / itv * HZ / 2,
		((double) sdev.wr_sectors) / itv * HZ / 2,
		arqsz,
		((double) sdev.rq_ticks) / itv,
		await,
		/* again: ticks in milliseconds */
		svctm * 100.0,
		/*
		 * NB: the ticks output in current sard patches is biased
		 * to output 1000 ticks per second.
		 */
		util * 10.0);
      }
   }
}


/*
 ***************************************************************************
 * Write basic stats, read from /proc/stat file or from sysfs
 ***************************************************************************
 */
void write_basic_stat(int curr, unsigned long itv, int flags)
{
   int fctr = 1;
   int i, j;
   struct io_hdr_stats *st_hdr_iodev_i;
   struct io_stats *st_iodev_i, *st_iodev_j;
   struct io_dlist *st_dev_list_j;

   if (DISPLAY_KILOBYTES(flags)) {
      printf(_("Device:            tps    kB_read/s    kB_wrtn/s    kB_read    kB_wrtn\n"));
      fctr = 2;
   }
   else
      printf(_("Device:            tps   Blk_read/s   Blk_wrtn/s   Blk_read   Blk_wrtn\n"));

   for (i = 0; i < iodev_nr; i++) {
      st_hdr_iodev_i = st_hdr_iodev + i;
      if (st_hdr_iodev_i->major || HAS_OLD_KERNEL(flags)) {
	
	 if (dlist_idx && !HAS_SYSFS(flags)) {
	    /*
	     * If devices have been entered on the command line, display the
	     * stats only for them.
	     * No problem if sysfs is mounted, since in this case only the
	     * requested devices have been read...
	     */
	    for (j = 0; j < dlist_idx; j++) {
	       st_dev_list_j = st_dev_list + j;
	       if (!strcmp(st_hdr_iodev_i->name, st_dev_list_j->dev_name))
		  break;
	    }
	    if (j == dlist_idx)
	       /* Device not found in list: skip it */
	       continue;
	 }
	
	 printf("%-13s", st_hdr_iodev_i->name);
	 if (strlen(st_hdr_iodev_i->name) > 13)
	    printf("\n             ");
	 st_iodev_i = st_iodev[curr] + i;
	 st_iodev_j = st_iodev[!curr] + i;

	 if (HAS_SYSFS(flags)) {
	    /* Print stats coming from /sys */
	    printf(" %8.2f %12.2f %12.2f %10u %10u\n",
		   S_VALUE(st_iodev_j->rd_ios + st_iodev_j->wr_ios,
			   st_iodev_i->rd_ios + st_iodev_i->wr_ios, itv),
		   S_VALUE(st_iodev_j->rd_sectors,
			   st_iodev_i->rd_sectors, itv) / fctr,
		   S_VALUE(st_iodev_j->wr_sectors,
			   st_iodev_i->wr_sectors, itv) / fctr,
		   (st_iodev_i->rd_sectors -
		    st_iodev_j->rd_sectors) / fctr,
		   (st_iodev_i->wr_sectors -
		    st_iodev_j->wr_sectors) / fctr);
	 }
	 else {
	    /* Print stats coming from /proc/stat */
	    printf(" %8.2f %12.2f %12.2f %10u %10u\n",
		   S_VALUE(st_iodev_j->dk_drive,
			   st_iodev_i->dk_drive, itv),
		   S_VALUE(st_iodev_j->dk_drive_rblk,
			   st_iodev_i->dk_drive_rblk, itv) / fctr,
		   S_VALUE(st_iodev_j->dk_drive_wblk,
			   st_iodev_i->dk_drive_wblk, itv) / fctr,
		   (st_iodev_i->dk_drive_rblk -
		    st_iodev_j->dk_drive_rblk) / fctr,
		   (st_iodev_i->dk_drive_wblk -
		    st_iodev_j->dk_drive_wblk) / fctr);
	 }
      }
   }
}


/*
 ***************************************************************************
 * Print everything now (stats and uptime)
 * Notes about the formula used to display stats as:
 * (x(t2) - x(t1)) / (t2 - t1) = XX.YY:
 * We have the identity: a = (a / b) * b + a % b   (a and b are integers).
 * Apply this with a = x(t2) - x(t1) (values about which stats are to be
 * displayed), and b = t2 - t1 (elapsed time in seconds).
 * Since uptime is given in jiffies, it is always divided by HZ to get seconds.
 * The integer part XX is: a / b
 * The decimal part YY is: ((a % b) * HZ) / b  (multiplied by HZ since
 * we want YY and not 0.YY)
 ***************************************************************************
 */
int write_stat(int curr, int flags, struct tm *loc_time)
{
   unsigned long itv;

   /* Print time stamp */
   if (DISPLAY_TIMESTAMP(flags)) {
      strftime(timestamp, sizeof(timestamp), "%X  ", loc_time);
      printf(_("Time: %s\n"), timestamp);
   }

   /*
    * itv is multiplied by the number of processors.
    * This is OK to compute CPU usage since the number of jiffies spent in the
    * different modes (user, nice, etc.) is the sum for all the processors.
    * But itv should be reduced to one processor before displaying disk
    * utilization.
    */
   itv = comm_stats[curr].uptime - comm_stats[!curr].uptime; /* uptime in jiffies */

   if (!DISPLAY_DISK_ONLY(flags))
      /* Display CPU utilization */
      write_cpu_stat(curr, itv);

   itv /= (cpu_nr + 1); /* See note above */

   if (!DISPLAY_CPU_ONLY(flags)) {

      if (DISPLAY_EXTENDED(flags))
	 /* Write extended stats, read from /sys */
	 write_ext_stat(curr, itv);
      else
	 /* Write basic stats, read from /proc/stat or from /sys */
	 write_basic_stat(curr, itv, flags);

      printf("\n");
   }
   return 1;
}


/*
 ***************************************************************************
 * Main loop: read I/O stats from the relevant sources,
 * and display them.
 ***************************************************************************
 */
void rw_io_stat_loop(int flags, long int count)
{
   int curr = 1;
   int next;

   do {
      /* Read kernel statistics */
      read_stat(curr, flags);
      if (HAS_SYSFS(flags)) {
	 if (dlist_idx)
	    /*
	     * Read the sysfs stats
	     * for the devices entered on the command line only
	     */
	    read_sysfs_dlist_stat(curr, flags);
	 else
	    /*
	     * Read stats (extended or not) from sysfs filesystem
	     * for every block devices found, and also for every
	     * partitions if '-p ALL' was entered on the command line
	     */
	    read_sysfs_stat(curr, flags);
      }

      /* Save time */
      get_localtime(&loc_time);

      /* Print results */
      if ((next = write_stat(curr, flags, &loc_time))
	  && (count > 0))
	 count--;
      fflush(stdout);

      if (count) {
	 pause();

	 if (next)
	    curr ^= 1;
      }
   }
   while (count);
}


/*
 ***************************************************************************
 * Main entry to the iostat program
 ***************************************************************************
 */
int main(int argc, char **argv)
{
   int it = 0, flags = 0;
   int opt = 1;
   int i;
   long int count = 1;
   struct utsname header;
   struct io_dlist *st_dev_list_i;

#ifdef USE_NLS
   /* Init National Language Support */
   init_nls();
#endif

   /* Allocate structures for device list */
   if (argc > 1)
      salloc_dev_list(argc - 1);

   /* Process args... */
   while (opt < argc) {

      if (!strcmp(argv[opt], "-p")) {
	 if (!argv[++opt] || (strspn(argv[opt], DIGITS) == strlen(argv[opt])))
	    usage(argv[0]);
	 flags |= D_PARTITIONS;
	 if (!strcmp(argv[opt], K_ALL)) {
	    flags |= D_PART_ALL;
	    opt++;
	 }
	 else {
	    /* Store device name */
	    i = update_dev_list(&dlist_idx, device_name(argv[opt++]));
	    st_dev_list_i = st_dev_list + i;
	    st_dev_list_i->disp_part = TRUE;
	 }
      }

      else if (!strncmp(argv[opt], "-", 1)) {
	 for (i = 1; *(argv[opt] + i); i++) {

	    switch (*(argv[opt] + i)) {

	     case 'c':
	       flags |= D_CPU_ONLY;	/* Display cpu usage only */
	       flags &= ~D_DISK_ONLY;
	       break;

	     case 'd':
	       flags |= D_DISK_ONLY;	/* Display disk utilization only */
	       flags &= ~D_CPU_ONLY;
	       break;
	
	     case 'k':
	       flags |= D_KILOBYTES;	/* Display stats in kB/s */
	       break;

	     case 't':
	       flags |= D_TIMESTAMP;	/* Display timestamp */
	       break;
	
	     case 'x':
	       flags |= D_EXTENDED;	/* Display extended stats */
	       break;

	     case 'V':			/* Print usage and exit	*/
	     default:
	       usage(argv[0]);
	    }
	 }
	 opt++;
      }

      else if (!isdigit(argv[opt][0]))
	 /* Store device name */
	 update_dev_list(&dlist_idx, device_name(argv[opt++]));

      else if (!it) {
	 interval = atol(argv[opt++]);
	 if (interval < 1)
 	   usage(argv[0]);
	 count = -1;
	 it = 1;
      }

      else {
	 count = atol(argv[opt++]);
	 if (count < 1)
	   usage(argv[0]);
      }
   }

   /* Linux does not provide extended stats for partitions */
   if (DISPLAY_PARTITIONS(flags) && DISPLAY_EXTENDED(flags)) {
      fprintf(stderr, _("-x and -p options are mutually exclusive\n"));
      exit(1);
   }

   /* Ignore device list if '-p ALL' entered on the command line */
   if (DISPLAY_PART_ALL(flags))
      dlist_idx = 0;

   /* Init structures according to machine architecture */
   io_sys_init(&flags);

   get_localtime(&loc_time);

   /* Get system name, release number and hostname */
   uname(&header);
   print_gal_header(&loc_time,
		    header.sysname, header.release, header.nodename);
   printf("\n");

   /* Set a handler for SIGALRM */
   alarm_handler(0);

   /* Main loop */
   rw_io_stat_loop(flags, count);

   return 0;
}
