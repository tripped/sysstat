/*
 * iostat: report I/O statistics
 * (C) 1998-2001 by Sebastien GODARD <sebastien.godard@wanadoo.fr>
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


struct disk_stats disk_stats[2][MAX_PART];
struct disk_hdr_stats disk_hdr_stats[MAX_PART];
struct comm_stats  comm_stats[2];
struct tm loc_time;
int part_nr = 0;	/* Nb of partitions */
long int interval = 0;
int proc_used = -1;	/* Nb of proc on the machine. A value of 1 means two procs... */
unsigned char timestamp[14];


/*
 * Print usage and exit
 */
void usage(char *progname)
{
   fprintf(stderr, _("sysstat version %s\n"
		   "(C) S. Godard <sebastien.godard@wanadoo.fr>\n"
	           "Usage: %s [ options... ]\n"
		   "Options are:\n"
		   "[ -c | -d ] [ -t ] [ -V ] [ -x [ <device> ] ]\n"
		   "[ <interval> [ <count> ] ]\n"),
	   VERSION, progname);
   exit(1);
}


/*
 * SIGALRM signal handler
 */
void alarm_handler(int sig)
{
   signal(SIGALRM, alarm_handler);
   alarm(interval);
}


/*
 * Initialize stats structures
 */
void init_stats(void)
{
   int i;

   for (i = 0; i < MAX_PART; i++) {
      memset(&disk_stats[0][i], 0, DISK_STATS_SIZE);
      memset(&disk_stats[1][i], 0, DISK_STATS_SIZE);
      sprintf(disk_hdr_stats[i].name, "hdisk%d", i);
   }

   memset(&comm_stats[0], 0, COMM_STATS_SIZE);
   memset(&comm_stats[1], 0, COMM_STATS_SIZE);
}


/*
 * Read stats from /proc/stat file...
 * (see linux source file linux/fs/proc/array.c)
 */
void read_stat(int curr, int flags)
{
   FILE *statfp;
   char line[1024];
   int pos, i;
   unsigned int v_tmp[3], v_major, v_index;
#if 0
   FILE *partfp;
   unsigned int major, minor;
   char pline[1024], disk_name[64];
#endif

   /* Open stat file */
   if ((statfp = fopen(STAT, "r")) == NULL) {
      perror("fopen");
      exit(2);
   }

   while (fgets(line, 1024, statfp) != NULL) {

      if (!strncmp(line, "cpu ", 4)) {
	 /*
	  * Read the number of jiffies spent in user, nice, system and idle mode
	  * and compute system uptime in jiffies (1/100ths of a second if HZ=100)
	  */
	 sscanf(line + 5, "%u %u %u %lu",
	        &(comm_stats[curr].cpu_user), &(comm_stats[curr].cpu_nice),
		&(comm_stats[curr].cpu_system), &(comm_stats[curr].cpu_idle));

	 /*
	  * Compute system uptime in jiffies.
	  * Uptime is multiplied by the number of processors.
	  */
	 comm_stats[curr].uptime = comm_stats[curr].cpu_user   + comm_stats[curr].cpu_nice +
	                           comm_stats[curr].cpu_system + comm_stats[curr].cpu_idle;
      }

      else if (DISPLAY_EXTENDED(flags))
	 /*
	  * When displaying extended statistics, we just need to get
	  * CPU info from /proc/stat.
	  */
	 continue;

      else if (!strncmp(line, "disk_rblk ", 10)) {
	 /*
	  * Read the number of blocks read from disk.
	  * A block is of indeterminate size. The size may vary depending on the device type.
	  */
	 sscanf(line + 10, "%u %u %u %u",
		&(disk_stats[curr][0].dk_drive_rblk), &(disk_stats[curr][1].dk_drive_rblk),
		&(disk_stats[curr][2].dk_drive_rblk), &(disk_stats[curr][3].dk_drive_rblk));

	 /* Statistics handled for the first four disks with pre 2.4 kernels */
	 part_nr = 4;
      }

      else if (!strncmp(line, "disk_wblk ", 10))
	 /* Read the number of blocks written to disk */
	 sscanf(line + 10, "%u %u %u %u",
		&(disk_stats[curr][0].dk_drive_wblk), &(disk_stats[curr][1].dk_drive_wblk),
		&(disk_stats[curr][2].dk_drive_wblk), &(disk_stats[curr][3].dk_drive_wblk));

      else if (!strncmp(line, "disk ", 5))
	 /* Read the number of I/O done since the last reboot */
	 sscanf(line + 5, "%u %u %u %u",
		&(disk_stats[curr][0].dk_drive), &(disk_stats[curr][1].dk_drive),
		&(disk_stats[curr][2].dk_drive), &(disk_stats[curr][3].dk_drive));

      else if (!strncmp(line, "disk_io: ", 9)) {
	 pos = 9;
	
	 /* Read disks I/O statistics (for 2.4 kernels) */
	 while (pos < strlen(line) - 1) {	/* Beware: a CR is already included in the line */
	    sscanf(line + pos, "(%u,%u):(%u,%*u,%u,%*u,%u) ",
		   &v_major, &v_index, &v_tmp[0], &v_tmp[1], &v_tmp[2]);
	    i = 0;
	    while ((i < part_nr) && ((v_major != disk_hdr_stats[i].major) ||
				     (v_index != disk_hdr_stats[i].minor)))
	       i++;
	    if (i == part_nr) {
	       /*
		* New device registered.
		* Assume that devices may be registered, but not unregistered dynamically...
		*/
	       disk_hdr_stats[i].major = v_major;
	       disk_hdr_stats[i].minor = v_index;
	       sprintf(disk_hdr_stats[i].name, "dev%d-%d", v_major, v_index);

#if 0
	       /* This part of the code tries to guess the real name of the device */
	
	       /* Open partitons file */
	       if ((partfp = fopen(PARTITIONS, "r")) == NULL) {
		  perror("fopen");
		  exit(2);
	       }

	       fgets(pline, 1024, partfp);
	       fgets(pline, 1024, partfp);

	       while (fgets(pline, 1024, partfp) != NULL) {
		  sscanf(pline, "%u %u %*u %63s", &major, &minor, disk_name);

		  if (((minor & 0x0f) == 0) && (major == v_major) && ((minor >> 4) == v_index)) {
		     sprintf(disk_hdr_stats[i].name, "/dev/%s", disk_name);
		     break;
		  }
	       }

	       /* Close partitions file */
	       fclose(partfp);
#endif	
	       part_nr++;
	    }
	    disk_stats[curr][i].dk_drive      = v_tmp[0];
	    disk_stats[curr][i].dk_drive_rblk = v_tmp[1];
	    disk_stats[curr][i].dk_drive_wblk = v_tmp[2];

	    pos += strcspn(line + pos, " ") + 1;
	 }
      }
   }

   /* Close stat file */
   fclose(statfp);

   /* Compute total number of I/O done */
   comm_stats[curr].dk_drive_sum = 0;
   for (i = 0; i < part_nr; i++)
      comm_stats[curr].dk_drive_sum += disk_stats[curr][i].dk_drive;
}


/*
 * Read extended stats from /proc/partitions file
 */
void read_ext_stat(int curr, int flags)
{
   FILE *partfp;
   int i;
   char line[1024];
   struct disk_stats part;
   struct disk_hdr_stats part_hdr;

   /* Open partitions file */
   if ((partfp = fopen(PARTITIONS, "r")) == NULL) {
      perror("fopen");
      exit(2);
   }

   while (fgets(line, 1024, partfp) != NULL) {

      if (sscanf(line, "%*d %*d %*d %63s %d %d %d %d %d %d %d %d %*d %d %d",
	     part_hdr.name,	/* No need to read major and minor numbers */
	     &part.rd_ios, &part.rd_merges, &part.rd_sectors, &part.rd_ticks,
	     &part.wr_ios, &part.wr_merges, &part.wr_sectors, &part.wr_ticks,
	     &part.ticks, &part.aveq) == 11) {

	 /*
	  * We have just read a line from /proc/partitions containing stats
	  * for a partition (ie this is not a fake line: title, etc.).
	  * Moreover, we now know that the kernel has the patch applied.
	  */

	 /* Look for partition in data table */
	 for (i = 0; i < part_nr; i++) {
	    if (!strcmp(disk_hdr_stats[i].name, part_hdr.name)) {
	       /* Partition found */
	       disk_hdr_stats[i].active = 1;
	       disk_stats[curr][i] = part;
	       break;
	    }
	 }

	 if ((i == part_nr) && DISPLAY_EXTENDED_ALL(flags) && (part_nr < MAX_PART) && part.ticks) {
	    /* Allocate new partition */
	    disk_stats[curr][part_nr] = part;
	    disk_hdr_stats[part_nr].active = 1;
	    strcpy(disk_hdr_stats[part_nr++].name, part_hdr.name);
	 }
      }
   }

   /* Close file */
   fclose(partfp);
}


/*
 * Print everything now (stats and uptime)
 * Notes about the formula used to display stats as:
 * (x(t2) - x(t1)) / (t2 - t1) = XX.YY:
 * We have the identity: a = (a / b) * b + a % b   (a and b are integers).
 * Apply this with a = x(t2) - x(t1) (values about which stats are to be displayed)
 *             and b = t2 - t1 (elapsed time in seconds).
 * Since uptime is given in jiffies, it is always divided by HZ to get seconds.
 * The integer part XX is: a / b
 * The decimal part YY is: ((a % b) * HZ) / b  (multiplied by HZ since we want YY and not 0.YY)
 */
int write_stat(int curr, int flags, struct tm *loc_time)
{
   int disk_index;
   unsigned long itv;

   /* Print time stamp */
   if (DISPLAY_TIMESTAMP(flags)) {
      strftime(timestamp, 14, "%X  ", loc_time);
      printf(_("Time: %s\n"), timestamp);
   }

   /*
    * itv is multiplied by the number of processors.
    * This is OK to compute CPU usage since the number of jiffies spent in the different
    * modes (user, nice, etc.) is the sum for all the processors.
    * But itv should be reduced to one processor before displaying disk utilization.
    */
   itv = comm_stats[curr].uptime - comm_stats[!curr].uptime;	/* uptime in jiffies */

   if (!DISPLAY_DISK_ONLY(flags)) {

      printf(_("avg-cpu:  %%user   %%nice    %%sys   %%idle\n"));

      printf("         %6.2f  %6.2f  %6.2f",
	     SP_VALUE(comm_stats[!curr].cpu_user,   comm_stats[curr].cpu_user,   itv),
	     SP_VALUE(comm_stats[!curr].cpu_nice,   comm_stats[curr].cpu_nice,   itv),
	     SP_VALUE(comm_stats[!curr].cpu_system, comm_stats[curr].cpu_system, itv));

      if (comm_stats[curr].cpu_idle < comm_stats[!curr].cpu_idle)
	 printf("    %.2f", 0.0);
      else
	 printf("  %6.2f",
		SP_VALUE(comm_stats[!curr].cpu_idle, comm_stats[curr].cpu_idle, itv));

      printf("\n\n");
   }

   itv /= (proc_used + 1); /* See note above */

   if (!DISPLAY_CPU_ONLY(flags)) {

      if (DISPLAY_EXTENDED(flags)) {
	
	 struct disk_stats current;
	 double tput, util, await, svctm, arqsz, nr_ios;
	
	 printf(_("Device:    rrqm/s wrqm/s   r/s   w/s  rsec/s  wsec/s avgrq-sz avgqu-sz   await  svctm  %%util\n"));
	
	 for (disk_index = 0; disk_index < part_nr; disk_index++) {

	    if (disk_hdr_stats[disk_index].active) {
	
	       current.rd_ios     = disk_stats[curr][disk_index].rd_ios     - disk_stats[!curr][disk_index].rd_ios;
	       current.wr_ios     = disk_stats[curr][disk_index].wr_ios     - disk_stats[!curr][disk_index].wr_ios;
	       current.rd_ticks   = disk_stats[curr][disk_index].rd_ticks   - disk_stats[!curr][disk_index].rd_ticks;
	       current.wr_ticks   = disk_stats[curr][disk_index].wr_ticks   - disk_stats[!curr][disk_index].wr_ticks;
	       current.rd_merges  = disk_stats[curr][disk_index].rd_merges  - disk_stats[!curr][disk_index].rd_merges;
	       current.wr_merges  = disk_stats[curr][disk_index].wr_merges  - disk_stats[!curr][disk_index].wr_merges;
	       current.rd_sectors = disk_stats[curr][disk_index].rd_sectors - disk_stats[!curr][disk_index].rd_sectors;
	       current.wr_sectors = disk_stats[curr][disk_index].wr_sectors - disk_stats[!curr][disk_index].wr_sectors;
	       current.ticks      = disk_stats[curr][disk_index].ticks      - disk_stats[!curr][disk_index].ticks;
	       current.aveq       = disk_stats[curr][disk_index].aveq       - disk_stats[!curr][disk_index].aveq;
	
	       nr_ios = current.rd_ios + current.wr_ios;
	       tput   = nr_ios * HZ / itv;
	       util   = ((double) current.ticks) / itv;
	       svctm  = tput ? util / tput : 0.0;
	       await  = nr_ios ? (current.rd_ticks + current.wr_ticks) / nr_ios * 1000.0 / HZ : 0.0;
	       arqsz  = nr_ios ? (current.rd_sectors + current.wr_sectors) / nr_ios : 0.0;

	       printf("/dev/%-5s", disk_hdr_stats[disk_index].name);
	       if (strlen(disk_hdr_stats[disk_index].name) > 5)
		  printf("\n          ");
	       printf(" %6.2f %6.2f %5.2f %5.2f %7.2f %7.2f %8.2f %8.2f %7.2f %6.2f %6.2f\n",
		      ((double) current.rd_merges) / itv * HZ, ((double) current.wr_merges) / itv * HZ,
		      ((double) current.rd_ios) / itv * HZ, ((double) current.wr_ios) / itv * HZ,
		      ((double) current.rd_sectors) / itv * HZ, ((double) current.wr_sectors) / itv * HZ,
		      arqsz,
		      ((double) current.aveq) / itv,
		      await,
		      svctm * 1000.0,
		      /* NB: the ticks output in current sard patches is biased to output 1000 ticks per second */
		      util * 10.0);
	    }
	 }
      }

      else {

	 printf(_("Device:            tps   Blk_read/s   Blk_wrtn/s   Blk_read   Blk_wrtn\n"));

	 for (disk_index = 0; disk_index < part_nr; disk_index++) {

	    printf("%-13s", disk_hdr_stats[disk_index].name);
	    if (strlen(disk_hdr_stats[disk_index].name) > 13)
	       printf("\n             ");
	    printf(" %8.2f %12.2f %12.2f %10u %10u\n",
		   S_VALUE(disk_stats[!curr][disk_index].dk_drive,      disk_stats[curr][disk_index].dk_drive,      itv),
		   S_VALUE(disk_stats[!curr][disk_index].dk_drive_rblk, disk_stats[curr][disk_index].dk_drive_rblk, itv),
		   S_VALUE(disk_stats[!curr][disk_index].dk_drive_wblk, disk_stats[curr][disk_index].dk_drive_wblk, itv),
		   (disk_stats[curr][disk_index].dk_drive_rblk - disk_stats[!curr][disk_index].dk_drive_rblk),
		   (disk_stats[curr][disk_index].dk_drive_wblk - disk_stats[!curr][disk_index].dk_drive_wblk));
	 }
      }
      printf("\n");
   }

   return 1;
}


/*
 * Main entry to the program
 */
int main(int argc, char **argv)
{
   int it = 0, flags = 0;
   int opt = 1, curr = 1;
   int i, next;
   long int count = 1;
   struct utsname header;

#ifdef USE_NLS
   /* Init National Language Support */
   init_nls();
#endif

   /* Init stat counters */
   init_stats();

   /* How many processors on this machine ? */
   get_nb_proc_used(&proc_used, ~0);

   /* Process args... */
   while (opt < argc) {

      if (!strcmp(argv[opt], "-x")) {
	 flags |= D_EXTENDED + D_EXTENDED_ALL;	/* Extended statistics		*/
	 /* Get device names */
	 while (argv[++opt] && strncmp(argv[opt], "-", 1) && !isdigit(argv[opt][0])) {
	    flags &= ~D_EXTENDED_ALL;
	    if (part_nr < MAX_PART)
	       strncpy(disk_hdr_stats[part_nr++].name, device_name(argv[opt]), MAX_NAME_LEN - 1);
	 }
      }

      else if (!strncmp(argv[opt], "-", 1)) {
	 for (i = 1; *(argv[opt] + i); i++) {

	    switch (*(argv[opt] + i)) {

	     case 'c':
	       flags |= D_CPU_ONLY;	/* Display cpu usage only		*/
	       flags &= ~D_DISK_ONLY;
	       break;

	     case 'd':
	       flags |= D_DISK_ONLY;	/* Display disk utilization only	*/
	       flags &= ~D_CPU_ONLY;
	       break;
	
	     case 't':
	       flags |= D_TIMESTAMP;	/* Display timestamp	       		*/
	       break;

	     case 'V':			/* Print usage and exit			*/
	     default:
	       usage(argv[0]);
	    }
	 }
	 opt++;
      }

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

   get_localtime(&loc_time);

   /* Get system name, release number and hostname */
   uname(&header);
   print_gal_header(&loc_time, header.sysname, header.release, header.nodename);
   printf("\n");

   /* Set a handler for SIGALRM */
   alarm_handler(0);

   /* Main loop */
   do {
      /* Read kernel statistics */
      read_stat(curr, flags);
      if (DISPLAY_EXTENDED(flags))
	  read_ext_stat(curr, flags);

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

   return 0;
}
