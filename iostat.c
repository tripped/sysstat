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
#include <sys/stat.h>
#include <sys/utsname.h>

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

char dp = '.';    	/* Decimal point */


/*
 * Print usage and exit
 */
void usage(char *progname)
{
   fprintf(stderr, _("sysstat version %s\n"
		   "(C) S. Godard <sebastien.godard@wanadoo.fr>\n"
	           "Usage: %s [ options... ]\n"
		   "Options are:\n"
		   "[ -c | -d ] [ -t ] [ -V ]\n"
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
 * Read stats...
 * (see linux source file linux/fs/proc/array.c)
 */
void read_stat(int curr)
{
   FILE *statfp;
   char resource_name[16];
   char line[128];
   unsigned int cc_user, cc_nice, cc_system;
   unsigned long cc_idle;
   int pos, i;
   unsigned int v_tmp[3], v_major, v_index;


   /* Open stat file */
   if ((statfp = fopen(STAT, "r")) == NULL) {
      perror("fopen");
      exit(2);
   }

   while (fgets(line, 128, statfp) != NULL) {

      if (!strncmp(line, "cpu ", 4)) {
	 /*
	  * Read the number of jiffies spent in user, nice, system and idle mode
	  * and compute system uptime in jiffies (1/100ths of a second)
	  */
	 sscanf(line, "%s  %u %u %u %lu",
	        resource_name, &cc_user, &cc_nice, &cc_system, &cc_idle);

	 /* Note that CPU usage is *not* reduced to one processor */
	 comm_stats[curr].cpu_user   = cc_user;
	 comm_stats[curr].cpu_nice   = cc_nice;
	 comm_stats[curr].cpu_system = cc_system;
	 comm_stats[curr].cpu_idle   = cc_idle;

	 /*
	  * Compute system uptime in jiffies (1/100ths of a second).
	  * Uptime is multiplied by the number of processors.
	  */
	 comm_stats[curr].uptime = cc_user + cc_nice + cc_system + cc_idle;
      }

      else if (!strncmp(line, "disk_rblk ", 10)) {
	 /*
	  * Read the number of blocks read from disk (one block is 1024 bytes).
	  * A quick glance at the linux kernel source file linux/drivers/block/ll_rw_blk.c
	  * once made me think this was a number of sectors. Yet, it seems to be
	  * a number of kilobytes... Please tell me if I'm wrong.
	  */
	 sscanf(line, "%s %u %u %u %u",
		resource_name, &(disk_stats[curr][0].dk_drive_rblk), &(disk_stats[curr][1].dk_drive_rblk),
		&(disk_stats[curr][2].dk_drive_rblk), &(disk_stats[curr][3].dk_drive_rblk));

	 /* Statistics handled for the first four disks with pre 2.4 kernels */
	 part_nr = 4;
      }

      else if (!strncmp(line, "disk_wblk ", 10))
	 /* Read the number of blocks written to disk */
	 sscanf(line, "%s %u %u %u %u",
		resource_name, &(disk_stats[curr][0].dk_drive_wblk), &(disk_stats[curr][1].dk_drive_wblk),
		&(disk_stats[curr][2].dk_drive_wblk), &(disk_stats[curr][3].dk_drive_wblk));

      else if (!strncmp(line, "disk ", 5))
	 /* Read the number of I/O done since the last reboot */
	 sscanf(line, "%s %u %u %u %u",
		resource_name, &(disk_stats[curr][0].dk_drive), &(disk_stats[curr][1].dk_drive),
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
		* Assume that devices may be registered, but not unregistered...
		*/
	       disk_hdr_stats[i].major = v_major;
	       disk_hdr_stats[i].minor = v_index;
	       sprintf(disk_hdr_stats[i].name, "dev%d-%d", v_major, v_index);
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
 * Print everything now (stats and uptime)
 * Notes about the formula used to display stats as:
 * (x(t2) - x(t1)) / (t2 - t1) = XX.YY:
 * We have the identity: a = (a / b) * b + a % b   (a and b are integers).
 * Apply this with a = x(t2) - x(t1) (values about which stats are to be displayed)
 *             and b = t2 - t1 (elapsed time in seconds).
 * Since uptime is given in jiffies, it is always divided by 100 to get seconds.
 * The integer part XX is: a / b
 * The decimal part YY is: ((a % b) * 100) / b  (multiplied by 100 since we want YY and not 0.YY)
 */
int write_stat(int curr, int disp, struct tm *loc_time)
{
   int disk_index;
   unsigned long udec_part, ndec_part, sdec_part;
   unsigned long wdec_part = 0;
   unsigned long itv;


   /* Print time stamp */
   if (DISPLAY_TIMESTAMP(disp)) {
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

   if (!DISPLAY_DISK_ONLY(disp)) {

      printf(_("avg-cpu:  %%user   %%nice    %%sys   %%idle\n"));

      udec_part = DEC_PART(comm_stats[curr].cpu_user,   comm_stats[!curr].cpu_user,   itv);
      ndec_part = DEC_PART(comm_stats[curr].cpu_nice,   comm_stats[!curr].cpu_nice,   itv);
      sdec_part = DEC_PART(comm_stats[curr].cpu_system, comm_stats[!curr].cpu_system, itv);

      printf("         %3lu%c%02lu  %3lu%c%02lu  %3lu%c%02lu",
	     INT_PART(comm_stats[curr].cpu_user,   comm_stats[!curr].cpu_user,   itv), dp, udec_part,
	     INT_PART(comm_stats[curr].cpu_nice,   comm_stats[!curr].cpu_nice,   itv), dp, ndec_part,
	     INT_PART(comm_stats[curr].cpu_system, comm_stats[!curr].cpu_system, itv), dp, sdec_part);

      if (comm_stats[curr].cpu_idle < comm_stats[!curr].cpu_idle)
	 printf("    0%c%02lu", dp, (400 - (udec_part + ndec_part + sdec_part + wdec_part)) % 100);
      else
	 printf("  %3lu%c%02lu",
		INT_PART(comm_stats[curr].cpu_idle, comm_stats[!curr].cpu_idle, itv), dp,
		/* Correct rounding error */
		(400 - (udec_part + ndec_part + sdec_part + wdec_part)) % 100);

      printf("\n");
   }

   itv /= (proc_used + 1); /* See note above */

   if (!DISPLAY_CPU_ONLY(disp)) {

      printf(_("Disks:         tps   Blk_read/s   Blk_wrtn/s   Blk_read   Blk_wrtn\n"));

      for (disk_index = 0; disk_index < part_nr; disk_index++) {

	 printf("%s %8lu%c%02lu %9lu%c%02lu %9lu%c%02lu %10u %10u\n",
		disk_hdr_stats[disk_index].name,
		INT_PART(disk_stats[curr][disk_index].dk_drive,
			 disk_stats[!curr][disk_index].dk_drive, itv),
		dp,
		DEC_PART(disk_stats[curr][disk_index].dk_drive,
			 disk_stats[!curr][disk_index].dk_drive, itv),
		INT_PART(disk_stats[curr][disk_index].dk_drive_rblk,
			 disk_stats[!curr][disk_index].dk_drive_rblk, itv),
		dp,
		DEC_PART(disk_stats[curr][disk_index].dk_drive_rblk,
			 disk_stats[!curr][disk_index].dk_drive_rblk, itv),
		INT_PART(disk_stats[curr][disk_index].dk_drive_wblk,
			 disk_stats[!curr][disk_index].dk_drive_wblk, itv),
		dp,
		DEC_PART(disk_stats[curr][disk_index].dk_drive_wblk,
			 disk_stats[!curr][disk_index].dk_drive_wblk, itv),
		(disk_stats[curr][disk_index].dk_drive_rblk - disk_stats[!curr][disk_index].dk_drive_rblk),
		(disk_stats[curr][disk_index].dk_drive_wblk - disk_stats[!curr][disk_index].dk_drive_wblk));
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
   int it = 0, disp = 0;
   int opt = 1, curr = 1;
   int i, next;
   long int count = 1;
   struct utsname header;


#ifdef USE_NLS
   /* Init National Language Support */
   init_nls(&dp);
#endif

   /* Init stat counters */
   init_stats();

   /* How many processors on this machine ? */
   get_nb_proc_used(&proc_used, ~0);

   /* Process args... */
   while (opt < argc) {

      if (!strncmp(argv[opt], "-", 1)) {
	 for (i = 1; *(argv[opt] + i); i++) {

	    switch (*(argv[opt] + i)) {

	     case 'c':
	       disp |= D_CPU_ONLY;	/* Display cpu usage only		*/
	       break;

	     case 'd':
	       disp |= D_DISK_ONLY;	/* Display disk utilization only	*/
	       break;
	
	     case 't':
	       disp |= D_TIMESTAMP;	/* Display timestamp	       		*/
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
      read_stat(curr);

      /* Save time */
      get_localtime(&loc_time);

      /* Print results */
      if ((next = write_stat(curr, disp, &loc_time))
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
