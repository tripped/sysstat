/*
 * iostat: report I/O statistics
 * (C) 1998-2000 by Sebastien GODARD <sebastien.godard@wanadoo.fr>
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


struct file_hdr file_hdr;
struct file_stats file_stats[2];
struct tm loc_time;

long int interval = 0;
unsigned long uptime0 = 0;
unsigned long last_uptime = 0;

int proc_used = -1;	/* Nb of proc on the machine. A value of 1 means two procs... */
unsigned char timestamp[14];

char dp = '.';    /* Decimal point */


/*
 * Print usage and exit
 */
void usage(char *progname)
{
#ifdef HAS_DISK_ACCT
   fprintf(stderr, _("sysstat version %s\n"
		   "(C) S. Godard <sebastien.godard@wanadoo.fr>\n"
	           "Usage: %s [ options... ]\n"
		   "Options are:\n"
		   "[ -c | -d ] [ -p | -l ] [ -t ] [ -V ] [ -o <filename> | -f <filename> ]\n"
		   "[ <interval> [ <count> ] ]\n"),
	   VERSION, progname);
#else		
   fprintf(stderr, _("sysstat version %s\n"
		   "(C) S. Godard <sebastien.godard@wanadoo.fr>\n"
	           "Usage: %s [ options... ]\n"
		   "Options are:\n"
		   "[ -c | -d ] [ -t ] [ -V ] [ -o <filename> | -f <filename> ]\n"
		   "[ <interval> [ <count> ] ]\n"),
	   VERSION, progname);
#endif		
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

   memset(&file_stats[0], 0, FILE_STATS_SIZE);
   memset(&file_stats[1], 0, FILE_STATS_SIZE);
}


int next_slice(int curr)
{
   unsigned long file_interval;
   int min, max, pt1, pt2;


   file_interval = (file_stats[curr].uptime - last_uptime) / 100;

   /*
    * A few notes about the "algorithm" used here to display selected entries
    * from the system activity file (option -f with a given interval number):
    * Let 'Iu' be the interval value given by the user on the command line,
    *     'If' the interval between current and previous line in the system activity file,
    * and 'En' the nth entry (identified by its time stamp) of the file.
    * We choose In = [ En - If/2, En + If/2 [ if If is even,
    *        or In = [ En - If/2, En + If/2 ] if not.
    * En will be displayed if
    *       (Pn * Iu) or (P'n * Iu) belongs to In
    * with  Pn = En / Iu and P'n = En / Iu + 1
    */
   min = (file_stats[curr].uptime / 100) - (file_interval / 2);
   max = (file_stats[curr].uptime / 100) + (file_interval / 2) +
         (file_interval & 0x1);

   pt1 = (( file_stats[curr].uptime / 100) / interval)      * interval;
   pt2 = (((file_stats[curr].uptime / 100) / interval) + 1) * interval;

#ifdef DEBUG
   printf("uptime[curr]= %ld  last_uptime= %ld\n", file_stats[curr].uptime, last_uptime);
   printf("min= %d  max= %d  pt1= %d  pt2= %d  file_interval= %ld\n",
	  min, max, pt1, pt2, file_interval);
#endif

   return (((pt1 >= min) && (pt1 < max)) || ((pt2 >= min) && (pt2 < max)) || (min == max));
}


/*
 * Open file for reading or writing
 */
void open_files(char from_file[], char to_file[], int *from_fd, int *to_fd)
{

   if (from_file[0]) {
      if ((*from_fd = open(from_file, O_RDONLY)) < 0) {
	 fprintf(stderr, _("Cannot open %s: %s\n"), from_file, strerror(errno));
	 exit(3);
      }
   }
   else if (to_file[0]) {
      if ((*to_fd = open(to_file, O_WRONLY | O_CREAT | O_TRUNC,
			 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
	 fprintf(stderr, _("Cannot open %s: %s\n"), to_file, strerror(errno));
	 exit(3);
      }
   }
}


/*
 * Close files
 */
void close_files(int read_file, int write_file, int from_fd, int to_fd)
{
   if (read_file)
      close(from_fd);
   if(write_file)
      close(to_fd);
}


/*
 * Open iostat file for reading or writing
 */
void rw_io_header(int read_from_file, int write_to_file, int from_fd, int to_fd)
{
   int nb;


   if (read_from_file) {
      /* Read file header */
      nb = read(from_fd, &file_hdr, FILE_HDR_SIZE);
      if ((nb != FILE_HDR_SIZE) || (file_hdr.io_magic != IO_MAGIC)) {
      fprintf(stderr, _("Invalid iostat file\n"));
      exit(3);
      }

      /* Get date and time stored in file */
      loc_time.tm_mday = (int) file_hdr.io_day;
      loc_time.tm_mon  = (int) file_hdr.io_month;
      loc_time.tm_year = (int) file_hdr.io_year;
   }
   /* 'else' because you read OR you write... */
   else if (write_to_file){

      file_hdr.io_magic = IO_MAGIC;

#ifdef HAS_DISK_ACCT
      file_hdr.io_patch = (char) 1;
#else
      file_hdr.io_patch = (char) 0;
#endif

      file_hdr.io_day   = (char) loc_time.tm_mday;	/* Store date in file */
      file_hdr.io_month = (char) loc_time.tm_mon;
      file_hdr.io_year  = (char) loc_time.tm_year;

      /* Write iostat file header */
      if ((nb = write(to_fd, &file_hdr, FILE_HDR_SIZE)) != FILE_HDR_SIZE) {
	 fprintf(stderr, _("Cannot write iostat file header: %s\n"), strerror(errno));
	 exit(2);
      }
   }
}


/*
 * Read stats from iostat file
 */
int read_stat_from_file(int curr, int from_fd)
{
   int nb;


   nb = read(from_fd, &file_stats[curr], FILE_STATS_SIZE);
   if (!nb)
      return 1;		/* End of iostat file */
   if (nb < 0) {
      fprintf(stderr, _("Error while reading iostat file: %s\n"), strerror(errno));
      exit(2);
   }
   else if (nb < FILE_STATS_SIZE) {
      fprintf(stderr, _("End of iostat file unexpected\n"));
      exit(1);
   }

   if (!uptime0)
      uptime0 = file_stats[curr].uptime;

   return 0;
}


/*
 * Write stats to iostat file
 */
void write_stat_to_file(int curr, int to_fd)
{
   int nb;


   if ((nb = write(to_fd, &file_stats[curr], FILE_STATS_SIZE)) != FILE_STATS_SIZE) {
      fprintf(stderr, _("Cannot write data to iostat file: %s\n"), strerror(errno));
      exit(2);
   }
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
#ifdef CPU_WAIT
   unsigned int cc_wait;
#endif


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
#ifdef CPU_WAIT	
	 sscanf(line, "%s  %u %u %u %lu %u",
	        resource_name, &cc_user, &cc_nice, &cc_system, &cc_idle, &cc_wait);
	 file_stats[curr].cpu_wait = cc_wait   / (proc_used + 1);

	 file_stats[curr].uptime = cc_wait;
#else	
	 sscanf(line, "%s  %u %u %u %lu",
	        resource_name, &cc_user, &cc_nice, &cc_system, &cc_idle);

	 file_stats[curr].uptime = 0L;
#endif
	 /* Note that CPU usage is *not* reduced to one processor */
	 file_stats[curr].cpu_user   = cc_user;
	 file_stats[curr].cpu_nice   = cc_nice;
	 file_stats[curr].cpu_system = cc_system;
	 file_stats[curr].cpu_idle   = cc_idle;

	 /*
	  * Compute system uptime in jiffies (1/100ths of a second).
	  * Uptime is multiplied by the number of processors.
	  */
	 file_stats[curr].uptime += cc_user + cc_nice + cc_system + cc_idle;
      }

      else if (!strncmp(line, "disk_rblk ", 10))
	 /*
	  * Read the number of blocks read from disk (one block is 1024 bytes).
	  * A quick glance at the linux kernel source file linux/drivers/block/ll_rw_blk.c
	  * once made me think this was a number of sectors. Yet, it seems to be
	  * a number of kilobytes... Please tell me if I'm wrong.
	  */
	 sscanf(line, "%s %u %u %u %u",
		resource_name, &(file_stats[curr].dk_drive_rblk[0]), &(file_stats[curr].dk_drive_rblk[1]),
		&(file_stats[curr].dk_drive_rblk[2]), &(file_stats[curr].dk_drive_rblk[3]));

      else if (!strncmp(line, "disk_wblk ", 10))
	 /* Read the number of blocks written to disk */
	 sscanf(line, "%s %u %u %u %u",
		resource_name, &(file_stats[curr].dk_drive_wblk[0]), &(file_stats[curr].dk_drive_wblk[1]),
		&(file_stats[curr].dk_drive_wblk[2]), &(file_stats[curr].dk_drive_wblk[3]));

      else if (!strncmp(line, "disk ", 5))
	 /* Read the number of I/O done since the last reboot */
	 sscanf(line, "%s %u %u %u %u",
		resource_name, &(file_stats[curr].dk_drive[0]), &(file_stats[curr].dk_drive[1]),
		&(file_stats[curr].dk_drive[2]), &(file_stats[curr].dk_drive[3]));

#ifdef TTY
      else if (!strncmp(line, "tty ", 4))
	 /* Read the number of characters read and written for all the ttys */
	 sscanf(line, "%s %lu %lu",
		resource_name, &(file_stats[curr].tty_in), &(file_stats[curr].tty_out));
#endif

#ifdef HAS_DISK_ACCT
      /* The following exists only if the disk accounting patch has been applied */
      else if (!strncmp(line, "disk_pgin ", 10))
	 /* Read the number of 512-byte blocks read from disk */
	 sscanf(line, "%s %u %u %u %u",
		resource_name, &(file_stats[curr].dk_drive_pgin[0]), &(file_stats[curr].dk_drive_pgin[1]),
		&(file_stats[curr].dk_drive_pgin[2]), &(file_stats[curr].dk_drive_pgin[3]));

      else if (!strncmp(line, "disk_pgout ", 11))
	 /* Read the number of 512-byte blocks written to disk */
	 sscanf(line, "%s %u %u %u %u",
		resource_name, &(file_stats[curr].dk_drive_pgout[0]), &(file_stats[curr].dk_drive_pgout[1]),
		&(file_stats[curr].dk_drive_pgout[2]), &(file_stats[curr].dk_drive_pgout[3]));
#endif
   }

   /* Close stat file */
   fclose(statfp);

   /* Compute total number of I/O done */
   file_stats[curr].dk_drive_sum = file_stats[curr].dk_drive[0] + file_stats[curr].dk_drive[1] +
                                   file_stats[curr].dk_drive[2] + file_stats[curr].dk_drive[3];
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
int write_stat(int curr, int disp, int write_to_file, int to_fd, int read_from_file)
{
   int disk_index;
   unsigned long udec_part, ndec_part, sdec_part;
   unsigned long wdec_part = 0;
   unsigned long itv;
   short close_enough;


   /* Check time */
   if (read_from_file) {
      file_stats[curr].uptime -= uptime0;
      file_stats[!curr].uptime -= uptime0;
      close_enough = next_slice(curr);
      file_stats[curr].uptime += uptime0;
      file_stats[!curr].uptime += uptime0;
      if (!close_enough)
	 /* Not close enough to desired interval */
	 return 0;
   }

   /* Print time stamp */
   if (DISPLAY_TIMESTAMP(disp) || read_from_file) {

      loc_time.tm_hour = file_stats[curr].hour;
      loc_time.tm_min  = file_stats[curr].minute;
      loc_time.tm_sec  = file_stats[curr].second;
      strftime(timestamp, 14, "%X  ", &loc_time);

      printf(_("Time: %s\n"), timestamp);
   }

   itv = file_stats[curr].uptime - file_stats[!curr].uptime;	/* uptime in jiffies */

   if (!DISPLAY_DISK_ONLY(disp)) {

#ifdef TTY
      printf(_("tty:     tin       tout   "));
#endif

      printf(_("avg-cpu:  %%user   %%nice    %%sys   %%idle"));

#ifdef CPU_WAIT
      printf(_("  %%iowait"));
#endif

      printf("\n");


#ifdef TTY	
      printf("  %10lu %10lu   ",
	     (file_stats[curr].tty_in  - file_stats[!curr].tty_in),
	     (file_stats[curr].tty_out - file_stats[!curr].tty_out));
#endif	

      udec_part = DEC_PART(file_stats[curr].cpu_user,   file_stats[!curr].cpu_user,   itv);
      ndec_part = DEC_PART(file_stats[curr].cpu_nice,   file_stats[!curr].cpu_nice,   itv);
      sdec_part = DEC_PART(file_stats[curr].cpu_system, file_stats[!curr].cpu_system, itv);

      printf("         %3lu%c%02lu  %3lu%c%02lu  %3lu%c%02lu",
	     INT_PART(file_stats[curr].cpu_user,   file_stats[!curr].cpu_user,   itv), dp, udec_part,
	     INT_PART(file_stats[curr].cpu_nice,   file_stats[!curr].cpu_nice,   itv), dp, ndec_part,
	     INT_PART(file_stats[curr].cpu_system, file_stats[!curr].cpu_system, itv), dp, sdec_part);

#ifdef CPU_WAIT
      wdec_part = DEC_PART(file_stats[curr].cpu_wait, file_stats[!curr].cpu_wait, itv);
      printf("  %3lu%c%02lu",
	     INT_PART(file_stats[curr].cpu_wait, file_stats[!curr].cpu_wait, itv), dp, wdec_part);
#endif

      if (file_stats[curr].cpu_idle < file_stats[!curr].cpu_idle)
	 printf("    0%c%02lu", dp, (400 - (udec_part + ndec_part + sdec_part + wdec_part)) % 100);
      else
	 printf("  %3lu%c%02lu",
		INT_PART(file_stats[curr].cpu_idle, file_stats[!curr].cpu_idle, itv), dp,
		/* Correct rounding error */
		(400 - (udec_part + ndec_part + sdec_part + wdec_part)) % 100);

      printf("\n");
   }

   if (!DISPLAY_CPU_ONLY(disp)) {

      if (!DISPLAY_LOGICAL_IO(disp)) {

	 /* Only if DISK ACCOUNTING patch applied... */
	 printf(_("Disks:         tps    KB_read/s    KB_wrtn/s    KB_read    KB_wrtn\n"));

	 for (disk_index = 0; disk_index < NR_DISKS; disk_index++) {

	    printf(_("hdisk%d %8lu%c%02lu %9lu%c%02lu %9lu%c%02lu %10u %10u\n"),
		   disk_index,
		   INT_PART(file_stats[curr].dk_drive[disk_index], file_stats[!curr].dk_drive[disk_index], itv),
		   dp,
		   DEC_PART(file_stats[curr].dk_drive[disk_index], file_stats[!curr].dk_drive[disk_index], itv),
		   INT_PART(file_stats[curr].dk_drive_pgin[disk_index]   / 2,
			    file_stats[!curr].dk_drive_pgin[disk_index]  / 2, itv),
		   dp,
		   DEC_PART(file_stats[curr].dk_drive_pgin[disk_index]   / 2,
			    file_stats[!curr].dk_drive_pgin[disk_index]  / 2, itv),
		   INT_PART(file_stats[curr].dk_drive_pgout[disk_index]  / 2,
			    file_stats[!curr].dk_drive_pgout[disk_index] / 2, itv),
		   dp,
		   DEC_PART(file_stats[curr].dk_drive_pgout[disk_index]  /2,
			    file_stats[!curr].dk_drive_pgout[disk_index] /2, itv),
		   (file_stats[curr].dk_drive_pgin[disk_index]  / 2) - (file_stats[!curr].dk_drive_pgin[disk_index]  / 2),
		   (file_stats[curr].dk_drive_pgout[disk_index] / 2) - (file_stats[!curr].dk_drive_pgout[disk_index] / 2));
	 }
      }

      else {

	 printf(_("Disks:         tps   Blk_read/s   Blk_wrtn/s   Blk_read   Blk_wrtn\n"));

	 for (disk_index = 0; disk_index < NR_DISKS; disk_index++) {

	    printf(_("hdisk%d %8lu%c%02lu %9lu%c%02lu %9lu%c%02lu %10u %10u\n"),
		   disk_index,
		   INT_PART(file_stats[curr].dk_drive[disk_index],
			    file_stats[!curr].dk_drive[disk_index], itv),
		   dp,
		   DEC_PART(file_stats[curr].dk_drive[disk_index],
			    file_stats[!curr].dk_drive[disk_index], itv),
		   INT_PART(file_stats[curr].dk_drive_rblk[disk_index],
			    file_stats[!curr].dk_drive_rblk[disk_index], itv),
		   dp,
		   DEC_PART(file_stats[curr].dk_drive_rblk[disk_index],
			    file_stats[!curr].dk_drive_rblk[disk_index], itv),
		   INT_PART(file_stats[curr].dk_drive_wblk[disk_index],
			    file_stats[!curr].dk_drive_wblk[disk_index], itv),
		   dp,
		   DEC_PART(file_stats[curr].dk_drive_wblk[disk_index],
			    file_stats[!curr].dk_drive_wblk[disk_index], itv),
		   (file_stats[curr].dk_drive_rblk[disk_index] - file_stats[!curr].dk_drive_rblk[disk_index]),
		   (file_stats[curr].dk_drive_wblk[disk_index] - file_stats[!curr].dk_drive_wblk[disk_index]));
	 }
      }
      printf("\n");
   }

   /* Write data to file if necessary */
   if (write_to_file)
      write_stat_to_file(curr, to_fd);

   return 1;
}


/*
 * Main entry to the program
 */
int main(int argc, char **argv)
{
   int it = 0, disp = 0;
   int opt = 1, curr = 1;
   int from_fd, to_fd, i, next;
   long int count = 1;
   char from_file[MAX_FILE_LEN], to_file[MAX_FILE_LEN];
   struct utsname header;


   from_file[0] = to_file[0] = '\0';

#ifdef USE_NLS
   /* Init National Language Support */
   init_nls(&dp);
#endif

   /* Init stat counters */
   init_stats();

#ifndef HAS_DISK_ACCT
   disp = D_LOGICAL_IO;		/* Display logical I/O block stats */
#endif

   /* How many processors on this machine ? */
   get_nb_proc_used(&proc_used, ~0);

   /* Process args... */
   while (opt < argc) {

      if (!strcmp(argv[opt], "-o")) {		/* Output to specified file	*/
	 if (argv[++opt] && strncmp(argv[opt], "-", 1) &&
	     (strspn(argv[opt], DIGITS) != strlen(argv[opt])))
	   strcpy(to_file, argv[opt++]);
	 else
	   usage(argv[0]);
      }

      else if (!strcmp(argv[opt], "-f")) { 	/* Read from specified file	*/
	 if (argv[++opt] && strncmp(argv[opt], "-", 1) &&
	     (strspn(argv[opt], DIGITS) != strlen(argv[opt])))
	   strcpy(from_file, argv[opt++]);
	 else
	   usage(argv[0]);
      }

      else if (!strncmp(argv[opt], "-", 1)) {
	 for (i = 1; *(argv[opt] + i); i++) {

	    switch (*(argv[opt] + i)) {

	     case 'c':
	       disp |= D_CPU_ONLY;	/* Display cpu usage only		*/
	       break;

	     case 'd':
	       disp |= D_DISK_ONLY;	/* Display disk utilization only	*/
	       break;
	
#ifdef HAS_DISK_ACCT
	     case 'l':
	       disp |= D_LOGICAL_IO;	/* Display logical I/O block stats	*/
	       break;

	     case 'p':
	       disp &= ~D_LOGICAL_IO;	/* Display physical I/O block stats	*/
	       break;
#endif
	
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

   /* You read from a file OR you write to it... */
   if (from_file[0] && to_file[0])
      usage(argv[0]);

   /* Interval must be set if stats are written to file */
   if (!interval && to_file[0]) {
      fprintf(stderr, _("Please give an interval value\n"));
      exit(1);
   }

   if (!interval && from_file[0]) {
      interval = 1;
      count = -1;
   }

   /* Open iostat files then read or write header */
   open_files(from_file, to_file, &from_fd, &to_fd);

   get_localtime(&loc_time);

   /*
    * Get system name, release number and hostname.
    * This definition will be overriden if read from io file.
    */
   uname(&header);
   strncpy(file_hdr.io_sysname, header.sysname, UTSNAME_LEN);
   file_hdr.io_sysname[UTSNAME_LEN - 1] = '\0';
   strncpy(file_hdr.io_release, header.release, UTSNAME_LEN);
   file_hdr.io_release[UTSNAME_LEN - 1] = '\0';
   strncpy(file_hdr.io_nodename, header.nodename, UTSNAME_LEN);
   file_hdr.io_nodename[UTSNAME_LEN - 1] = '\0';

   /* Read or write io file header */
   rw_io_header(from_file[0], to_file[0], from_fd, to_fd);

   print_gal_header(&loc_time, file_hdr.io_sysname, file_hdr.io_release, file_hdr.io_nodename);
   printf("\n");

   /* Set a handler for SIGALRM */
   if (!from_file[0])
	 alarm_handler(0);

   /* Main loop */
   do {
      if (from_file[0]) {
	 /* Read stats from file */
	 if (read_stat_from_file(curr, from_fd))
	    /* End of iostat file: exit */
	    break;
      }
      else {
	 /* Read kernel statistics */
	 read_stat(curr);

	 /* Save time */
	 get_localtime(&loc_time);

	 file_stats[curr].hour   = loc_time.tm_hour;
	 file_stats[curr].minute = loc_time.tm_min;
	 file_stats[curr].second = loc_time.tm_sec;
      }

      /* Print results */
      if ((next = write_stat(curr, disp, to_file[0], to_fd, from_file[0]))
	  && (count > 0))
	 count--;
      fflush(stdout);

      if (count) {
	 if (!from_file[0])
	    pause();
	 else
	    last_uptime = file_stats[curr].uptime - uptime0;

	 if (next)
	    curr ^= 1;
      }
   }
   while (count);

   /* Close system activity files */
   close_files(from_file[0], to_file[0], from_fd, to_fd);

   return 0;
}
