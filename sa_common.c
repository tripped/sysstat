/*
 * sar and sadf common routines.
 * (C) 1999-2007 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "sa.h"
#include "common.h"
#include "ioconf.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

/*
 ***************************************************************************
 * Init a bitmap (CPU, IRQ, etc.)
 *
 * IN:
 * @value	Value used to init bitmap.
 * @nr		Size of the bitmap in bits.
 *
 * OUT:
 * @bitmap	Bitmap initialized.
 ***************************************************************************
 */
void init_bitmap(unsigned char bitmap[], unsigned char value, unsigned int nr)
{
	register int i;

	for (i = 0; i <= nr >> 3; i++)
		bitmap[i] = value;
}

/*
 ***************************************************************************
 * Init stats structures
 *
 * OUT:
 * @file_stats[]	Array of structures for general statisitcs initialized.
 * @interrupts[]	Array of structures for interrupts statistics
 *			initialized.
 ***************************************************************************
 */
void init_stats(struct file_stats file_stats[],
		unsigned int interrupts[][NR_IRQS])
{
	int i;

	for (i = 0; i < 3; i++) {
		memset(&file_stats[i], 0, FILE_STATS_SIZE);
		memset(interrupts[i], 0, STATS_ONE_IRQ_SIZE);
	}
}

/*
 ***************************************************************************
 * Allocate stats_one_cpu structures
 * (only on SMP machines)
 *
 * IN:
 * @nr_cpu	Number of CPU for which a structure must be allocated.
 *
 * OUT:
 * @st_cpu[]	Array of structures that have been allocated.
 ***************************************************************************
 */
void salloc_cpu_array(struct stats_one_cpu *st_cpu[], unsigned int nr_cpu)
{
	int i;

	for (i = 0; i < 3; i++)
		SREALLOC(st_cpu[i], struct stats_one_cpu, STATS_ONE_CPU_SIZE * nr_cpu);
}

/*
 ***************************************************************************
 * Allocate stats_serial structures
 *
 * IN:
 * @nr_serial	Number of serial lines for which a structure must be allocated.
 *
 * OUT:
 * @st_serial[]	Array of structures that have been allocated.
 ***************************************************************************
 */
void salloc_serial_array(struct stats_serial *st_serial[], int nr_serial)
{
	int i;

	for (i = 0; i < 3; i++)
		SREALLOC(st_serial[i], struct stats_serial, STATS_SERIAL_SIZE * nr_serial);
}

/*
 ***************************************************************************
 * Allocate stats_irq_cpu structures
 *
 * IN:
 * @nr_cpu		Number of processors.
 * @nr_irqcpu		Number of interrupts per processor.
 *
 * OUT:
 * @st_irq_cpu[]	Array of structures that have been allocated.
 ***************************************************************************
 */
void salloc_irqcpu_array(struct stats_irq_cpu *st_irq_cpu[],
			 unsigned int nr_cpu, unsigned int nr_irqcpu)
{
	int i;

	for (i = 0; i < 3; i++)
		SREALLOC(st_irq_cpu[i], struct stats_irq_cpu,
			 STATS_IRQ_CPU_SIZE * nr_cpu * nr_irqcpu);
}

/*
 ***************************************************************************
 * Allocate stats_net_dev structures
 *
 * IN:
 * @nr_iface		Number of network interfaces for which a structure
 *			must be allocated.
 *
 * OUT:
 * @st_net_dev[]	Array of structures that have been allocated.
 ***************************************************************************
 */
void salloc_net_dev_array(struct stats_net_dev *st_net_dev[],
			  unsigned int nr_iface)
{
	int i;

	for (i = 0; i < 3; i++)
		SREALLOC(st_net_dev[i], struct stats_net_dev, STATS_NET_DEV_SIZE * nr_iface);
}

/*
 ***************************************************************************
 * Allocate disk_stats structures
 *
 * IN:
 * @nr_disk	Number of disks for which a structure must be allocated.
 *
 * OUT:
 * @st_disk[]	Array of structures that have been allocated.
 ***************************************************************************
 */
void salloc_disk_array(struct disk_stats *st_disk[], int nr_disk)
{
	int i;

	for (i = 0; i < 3; i++)
		SREALLOC(st_disk[i], struct disk_stats, DISK_STATS_SIZE * nr_disk);
}

/*
 ***************************************************************************
 * Get device real name if possible.
 * Warning: This routine may return a bad name on 2.4 kernels where
 * disk activities are read from /proc/stat.
 *
 * IN:
 * @major	Major number of the device.
 * @minor	Minor number of the device.
 * @pretty	TRUE if the real name of the device (as it appears in /dev)
 * 		should be returned.
 *
 * RETURNS:
 * The name of the device, which may be the real name (as it appears in /dev)
 * or a string with the following format devM-n.
 ***************************************************************************
 */
char *get_devname(unsigned int major, unsigned int minor, int pretty)
{
	static char buf[32];
	char *name;

	snprintf(buf, 32, "dev%d-%d", major, minor);

	if (!pretty)
		return (buf);

	name = ioc_name(major, minor);
	if ((name == NULL) || !strcmp(name, K_NODEV))
		return (buf);

	return (name);
}

/*
 ***************************************************************************
 * Check if we are close enough to desired interval
 *
 * IN:
 * @uptime_ref	Uptime used as reference. This is the system uptime for the
 *		first sample statistics, or the first system uptime after a
 *		LINUX RESTART.
 * @uptime	Current system uptime.
 * @reset	TRUE if @last_uptime should be reset with @uptime_ref.
 * @interval	Interval of time.
 *
 * RETURNS:
 * 1 if we are actually close enough to desired interval, 0 otherwise.
 ***************************************************************************
*/
int next_slice(unsigned long long uptime_ref, unsigned long long uptime,
	       int reset, long interval)
{
	unsigned long file_interval, entry;
	static unsigned long long last_uptime = 0;
	int min, max, pt1, pt2;
	double f;

   /* uptime is expressed in jiffies (basis of 1 processor) */
	if (!last_uptime || reset)
		last_uptime = uptime_ref;

   /* Interval cannot be greater than 0xffffffff here */
	f = ((double) ((uptime - last_uptime) & 0xffffffff)) / HZ;
	file_interval = (unsigned long) f;
	if ((f * 10) - (file_interval * 10) >= 5)
		file_interval++; /* Rounding to correct value */

	last_uptime = uptime;

   /*
    * A few notes about the "algorithm" used here to display selected entries
    * from the system activity file (option -f with -i flag):
    * Let 'Iu' be the interval value given by the user on the command line,
    *     'If' the interval between current and previous line in the system
    * activity file,
    * and 'En' the nth entry (identified by its time stamp) of the file.
    * We choose In = [ En - If/2, En + If/2 [ if If is even,
    *        or In = [ En - If/2, En + If/2 ] if not.
    * En will be displayed if
    *       (Pn * Iu) or (P'n * Iu) belongs to In
    * with  Pn = En / Iu and P'n = En / Iu + 1
    */
	f = ((double) ((uptime - uptime_ref) & 0xffffffff)) / HZ;
	entry = (unsigned long) f;
	if ((f * 10) - (entry * 10) >= 5)
		entry++;

	min = entry - (file_interval / 2);
	max = entry + (file_interval / 2) + (file_interval & 0x1);
	pt1 = (entry / interval) * interval;
	pt2 = ((entry / interval) + 1) * interval;

	return (((pt1 >= min) && (pt1 < max)) || ((pt2 >= min) && (pt2 < max)));
}

/*
 ***************************************************************************
 * Use time stamp to fill tstamp structure
 *
 * IN:
 * @timestamp	Timestamp to decode (format: HH:MM:SS).
 *
 * OUT:
 * @tse		Structure containing the decoded timestamp.
 *
 * RETURNS:
 * 0 if the timestamp has been successfully decoded, 1 otherwise.
 ***************************************************************************
 */
int decode_timestamp(char timestamp[], struct tstamp *tse)
{
	timestamp[2] = timestamp[5] = '\0';
	tse->tm_sec  = atoi(&(timestamp[6]));
	tse->tm_min  = atoi(&(timestamp[3]));
	tse->tm_hour = atoi(timestamp);

	if ((tse->tm_sec < 0) || (tse->tm_sec > 59) ||
	    (tse->tm_min < 0) || (tse->tm_min > 59) ||
	    (tse->tm_hour < 0) || (tse->tm_hour > 23))
		return 1;

	tse->use = TRUE;

	return 0;
}

/*
 ***************************************************************************
 * Compare two timestamps
 *
 * IN:
 * @rectime	Date and time for current sample.
 * @tse		Timestamp used as reference.
 *
 * RETURNS:
 * A positive value if @rectime is greater than @tse,
 * a negative one otherwise.
 ***************************************************************************
 */
int datecmp(struct tm *rectime, struct tstamp *tse)
{
	if (rectime->tm_hour == tse->tm_hour) {
		if (rectime->tm_min == tse->tm_min)
			return (rectime->tm_sec - tse->tm_sec);
		else
			return (rectime->tm_min - tse->tm_min);
	}
	else
		return (rectime->tm_hour - tse->tm_hour);
}

/*
 ***************************************************************************
 * Parse a time stamp entered on the command line (hh:mm:ss) and decode it.
 *
 * IN:
 * @argv		Arguments list.
 * @opt			Index in the arguments list.
 * @def_timestamp	Default timestamp to use.
 *
 * OUT:
 * @tse			Structure containing the decoded timestamp.
 *
 * RETURNS:
 * 0 if the timestamp has been successfully decoded, 1 otherwise.
 ***************************************************************************
 */
int parse_timestamp(char *argv[], int *opt, struct tstamp *tse,
		    const char *def_timestamp)
{
	char timestamp[9];

	if ((argv[++(*opt)]) && (strlen(argv[*opt]) == 8))
		strcpy(timestamp, argv[(*opt)++]);
	else
		strcpy(timestamp, def_timestamp);

	return decode_timestamp(timestamp, tse);
}

/*
 ***************************************************************************
 * Set current daily data file name.
 *
 * OUT:
 * @rectime	Current date and time.
 * @datafile	Name of daily data file.
 ***************************************************************************
 */
void set_default_file(struct tm *rectime, char *datafile)
{
	get_time(rectime);
	snprintf(datafile, MAX_FILE_LEN,
		 "%s/sa%02d", SA_DIR, rectime->tm_mday);
	datafile[MAX_FILE_LEN - 1] = '\0';
}

/*
 ***************************************************************************
 * Set interval value.
 *
 * IN:
 * @file_stats_curr	Structure with current sample statistics.
 * @file_stats_prev	Structure with previous sample statistics.
 * @nr_proc		Number of CPU.
 *
 * OUT:
 * @g_itv		Interval in jiffies multiplied by the # of proc.
 * @itv			Interval in jiffies.
 ***************************************************************************
 */
void get_itv_value(struct file_stats *file_stats_curr,
		   struct file_stats *file_stats_prev,
		   unsigned int nr_proc,
		   unsigned long long *itv, unsigned long long *g_itv)
{
   /* Interval value in jiffies */
	*g_itv = get_interval(file_stats_prev->uptime,
			      file_stats_curr->uptime);

	if (nr_proc > 1)
		*itv = get_interval(file_stats_prev->uptime0,
				    file_stats_curr->uptime0);
	else
		*itv = *g_itv;
}

/*
 ***************************************************************************
 * Fill rectime structure according to data saved in file header
 *
 * IN:
 * @flags	Flags for common options and system state.
 * @file_hdr	System activity file standard header.
 *
 * OUT:
 * @rectime	Date and time from file header.
 ***************************************************************************
 */
void set_hdr_rectime(unsigned int flags, struct tm *rectime,
		     struct file_hdr *file_hdr)
{
	struct tm *loc_t;

	if (PRINT_TRUE_TIME(flags)) {
      /* Get local time */
		get_time(rectime);

		rectime->tm_mday = file_hdr->sa_day;
		rectime->tm_mon  = file_hdr->sa_month;
		rectime->tm_year = file_hdr->sa_year;
      /*
       * Call mktime() to set DST (Daylight Saving Time) flag.
       * Has anyone a better way to do it?
       */
		rectime->tm_hour = rectime->tm_min = rectime->tm_sec = 0;
		mktime(rectime);
	}
	else {
		loc_t = localtime((const time_t *) &(file_hdr->sa_ust_time));
		*rectime = *loc_t;
	}
}

/*
 ***************************************************************************
 * Print report header
 *
 * IN:
 * @flags	Flags for common options and system state.
 * @file_hdr	System activity file standard header.
 *
 * OUT:
 * @rectime	Date and time from file header.
 ***************************************************************************
 */
void print_report_hdr(unsigned int flags, struct tm *rectime,
		      struct file_hdr *file_hdr)
{

	set_hdr_rectime(flags, rectime, file_hdr);

	print_gal_header(rectime, file_hdr->sa_sysname, file_hdr->sa_release,
			 file_hdr->sa_nodename, file_hdr->sa_machine);
}

/*
 ***************************************************************************
 * Network interfaces may now be registered (and unregistered) dynamically.
 * This is what we try to guess here.
 *
 * IN:
 * @file_hdr	System activity file standard header.
 * @st_dev_dev	Structures with network interfaces statistics.
 * @curr	Index in array for current sample statistics.
 * @ref		Index in array for sample statistics used as reference.
 * @pos		Index on current network interface.
 *
 * RETURNS:
 * Position of current network interface in array of sample statistics used
 * as reference.
 ***************************************************************************
 */
unsigned int check_iface_reg(struct file_hdr *file_hdr,
			     struct stats_net_dev *st_net_dev[], int curr,
			     int ref, unsigned int pos)
{
	struct stats_net_dev *st_net_dev_i, *st_net_dev_j;
	unsigned int index = 0;

	st_net_dev_i = st_net_dev[curr] + pos;

	while (index < file_hdr->sa_iface) {
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

	    /*
	     * Special processing for rx_bytes (_packets) and tx_bytes (_packets)
	     * counters: If the number of bytes (packets) has decreased, whereas
	     * the number of packets (bytes) has increased, then assume that the
	     * relevant counter has met an overflow condition, and that the
	     * interface was not unregistered, which is all the more plausible
	     * that the previous value for the counter was > ULONG_MAX/2.
	     * NB: the average value displayed will be wrong in this case...
	     *
	     * If such an overflow is detected, just set the flag. There is no
	     * need to handle this in a special way: the difference is still
	     * properly calculated if the result is of the same type (i.e.
	     * unsigned long) as the two values.
	     */
				int ovfw = FALSE;

				if ((st_net_dev_i->rx_bytes < st_net_dev_j->rx_bytes) &&
				    (st_net_dev_i->rx_packets > st_net_dev_j->rx_packets) &&
				    (st_net_dev_j->rx_bytes > (~0UL >> 1)))
					ovfw = TRUE;
				if ((st_net_dev_i->tx_bytes < st_net_dev_j->tx_bytes) &&
				    (st_net_dev_i->tx_packets > st_net_dev_j->tx_packets) &&
				    (st_net_dev_j->tx_bytes > (~0UL >> 1)))
					ovfw = TRUE;
				if ((st_net_dev_i->rx_packets < st_net_dev_j->rx_packets) &&
				    (st_net_dev_i->rx_bytes > st_net_dev_j->rx_bytes) &&
				    (st_net_dev_j->rx_packets > (~0UL >> 1)))
					ovfw = TRUE;
				if ((st_net_dev_i->tx_packets < st_net_dev_j->tx_packets) &&
				    (st_net_dev_i->tx_bytes > st_net_dev_j->tx_bytes) &&
				    (st_net_dev_j->tx_packets > (~0UL >> 1)))
					ovfw = TRUE;

				if (!ovfw) {
	       /* OK: assume here that the device was actually unregistered */
					memset(st_net_dev_j, 0, STATS_NET_DEV_SIZE);
					strcpy(st_net_dev_j->interface, st_net_dev_i->interface);
				}
			}
			return index;
		}
		index++;
	}

   /* Network interface not found: Look for the first free structure */
	for (index = 0; index < file_hdr->sa_iface; index++) {
		st_net_dev_j = st_net_dev[ref] + index;
		if (!strcmp(st_net_dev_j->interface, "?")) {
			memset(st_net_dev_j, 0, STATS_NET_DEV_SIZE);
			strcpy(st_net_dev_j->interface, st_net_dev_i->interface);
			break;
		}
	}
	if (index >= file_hdr->sa_iface)
      /* No free structure: Default is structure of same rank */
		index = pos;

	st_net_dev_j = st_net_dev[ref] + index;
   /* Since the name is not the same, reset all the structure */
	memset(st_net_dev_j, 0, STATS_NET_DEV_SIZE);
	strcpy(st_net_dev_j->interface, st_net_dev_i->interface);

	return  index;
}

/*
 ***************************************************************************
 * Disks may be registered dynamically (true in /proc/stat file).
 * This is what we try to guess here.
 *
 * IN:
 * @file_hdr	System activity file standard header.
 * @st_disk	Structures with disks statistics.
 * @curr	Index in array for current sample statistics.
 * @ref		Index in array for sample statistics used as reference.
 * @pos		Index on current disk.
 *
 * RETURNS:
 * Position of current disk in array of sample statistics used as reference.
 ***************************************************************************
 */
int check_disk_reg(struct file_hdr *file_hdr, struct disk_stats *st_disk[],
		   int curr, int ref, int pos)
{
	struct disk_stats *st_disk_i, *st_disk_j;
	int index = 0;

	st_disk_i = st_disk[curr] + pos;

	while (index < file_hdr->sa_nr_disk) {
		st_disk_j = st_disk[ref] + index;
		if ((st_disk_i->major == st_disk_j->major) &&
		    (st_disk_i->minor == st_disk_j->minor)) {
	 /*
	  * Disk found.
	  * If a counter has decreased, then we may assume that the
	  * corresponding device was unregistered, then registered again.
	  * NB: AFAIK, such a device cannot be unregistered with current
	  * kernels.
	  */
			if ((st_disk_i->nr_ios < st_disk_j->nr_ios) ||
			    (st_disk_i->rd_sect < st_disk_j->rd_sect) ||
			    (st_disk_i->wr_sect < st_disk_j->wr_sect)) {

				memset(st_disk_j, 0, DISK_STATS_SIZE);
				st_disk_j->major = st_disk_i->major;
				st_disk_j->minor = st_disk_i->minor;
			}
			return index;
		}
		index++;
	}

   /* Disk not found: Look for the first free structure */
	for (index = 0; index < file_hdr->sa_nr_disk; index++) {
		st_disk_j = st_disk[ref] + index;
		if (!(st_disk_j->major + st_disk_j->minor)) {
			memset(st_disk_j, 0, DISK_STATS_SIZE);
			st_disk_j->major = st_disk_i->major;
			st_disk_j->minor = st_disk_i->minor;
			break;
		}
	}
	if (index >= file_hdr->sa_nr_disk)
      /* No free structure found: Default is structure of same rank */
		index = pos;

	st_disk_j = st_disk[ref] + index;
   /* Since the device is not the same, reset all the structure */
	memset(st_disk_j, 0, DISK_STATS_SIZE);
	st_disk_j->major = st_disk_i->major;
	st_disk_j->minor = st_disk_i->minor;

	return index;
}

/*
 ***************************************************************************
 * Since ticks may vary slightly from CPU to CPU, we'll want
 * to recalculate itv based on this CPU's tick count, rather
 * than that reported by the "cpu" line. Otherwise we
 * occasionally end up with slightly skewed figures, with
 * the skew being greater as the time interval grows shorter.
 *
 * IN:
 * @st_cpu_i	Current sample statistics for current CPU.
 * @st_cpu_j	Previous sample statistics for current CPU.
 *
 * RETURNS:
 * Interval of time based on current CPU.
 ***************************************************************************
 */
unsigned long long get_per_cpu_interval(struct stats_one_cpu *st_cpu_i,
					struct stats_one_cpu *st_cpu_j)
{
	return ((st_cpu_i->per_cpu_user + st_cpu_i->per_cpu_nice +
		 st_cpu_i->per_cpu_system + st_cpu_i->per_cpu_iowait +
		 st_cpu_i->per_cpu_idle + st_cpu_i->per_cpu_steal) -
		(st_cpu_j->per_cpu_user + st_cpu_j->per_cpu_nice +
		 st_cpu_j->per_cpu_system + st_cpu_j->per_cpu_iowait +
		 st_cpu_j->per_cpu_idle + st_cpu_j->per_cpu_steal));
}

/*
 ***************************************************************************
 * Read data from a system activity data file
 *
 * IN:
 * @ifd		Input file descriptor.
 * @buffer	Buffer where data are read.
 * @size	Number of bytes to read.
 * @mode	If set to HARD_SIZE, indicate that an EOF should be considered
 * 		as an error.
 *
 * RETURNS:
 * 1 if EOF has been reached, 0 otherwise.
 ***************************************************************************
 */
int sa_fread(int ifd, void *buffer, int size, int mode)
{
	int n;

	if ((n = read(ifd, buffer, size)) < 0) {
		fprintf(stderr, _("Error while reading system activity file: %s\n"),
			strerror(errno));
		close(ifd);
		exit(2);
	}

	if (!n && (mode == SOFT_SIZE))
		return 1;	/* EOF */

	if (n < size) {
		fprintf(stderr, _("End of system activity file unexpected\n"));
		close(ifd);
		exit(2);
	}

	return 0;
}

/*
 ***************************************************************************
 * Display sysstat version used to create system activity data file.
 *
 * IN:
 * @file_magic	File magic header
 ***************************************************************************
 */
void display_sa_file_version(struct file_magic *file_magic)
{
	fprintf(stderr, _("File created using sar/sadc from sysstat version %d.%d.%d"),
		file_magic->sysstat_version,
		file_magic->sysstat_patchlevel,
		file_magic->sysstat_sublevel);

	if (file_magic->sysstat_extraversion) {
		fprintf(stderr, ".%d", file_magic->sysstat_extraversion);
	}
	fprintf(stderr, "\n");
}

/*
 ***************************************************************************
 * An invalid system activity file has been opened for reading.
 * If this file was created by an old version of sysstat, tell it to the
 * user...
 *
 * IN:
 * @fd		Descriptor of the file that has been opened.
 * @file_magic	file_magic structure filled with file magic header data.
 *		May contain invalid data.
 * @file	Name of the file being read.
 * @n		Number of bytes read while reading file magic header.
 * 		This function may also be called after failing to read file
 		standard header. In this case, n is set to 0.
 ***************************************************************************
 */
void handle_invalid_sa_file(int *fd, struct file_magic *file_magic, char *file,
			    int n)
{
	fprintf(stderr, _("Invalid system activity file: %s\n"), file);

	if ((n == FILE_MAGIC_SIZE) && (file_magic->sysstat_magic == SYSSTAT_MAGIC)) {
      /* This is a sysstat file, but this file has an old format */
		display_sa_file_version(file_magic);

		fprintf(stderr,
			_("Current sysstat version can no longer read the format of this file (%#x)\n"),
			file_magic->format_magic);
	}

	close (*fd);
	exit(3);
}

/*
 ***************************************************************************
 * Open a data file, and perform various checks before reading
 *
 * IN:
 * @dfile	Name of system activity data file
 * @actflag	Flag giving activities the user wants to display from the
 * 		file
 * @flags	Options other than activities entered on the command line
 * @ignore	Set to 1 if a true sysstat activity file but with a bad
 * 		format should not yield an error message. Useful with
 * 		sadf -H.
 *
 * OUT:
 * @ifd		System activity data file descriptor
 * @file_magic	file_magic structure containing data read from file magic
 *		header
 * @file_hdr	file_hdr structure containing data read from file standard
 * 		header
 ***************************************************************************
 */
void prep_file_for_reading(int *ifd, char *dfile, struct file_magic *file_magic,
			   struct file_hdr *file_hdr, unsigned int *actflag,
			   unsigned int flags, int ignore)
{
	int n;

   /* Open sa data file */
	if ((*ifd = open(dfile, O_RDONLY)) < 0) {
		fprintf(stderr, _("Cannot open %s: %s\n"), dfile, strerror(errno));
		exit(2);
	}

   /* Read file magic data */
	n = read(*ifd, file_magic, FILE_MAGIC_SIZE);

	if ((n != FILE_MAGIC_SIZE) ||
	    (file_magic->sysstat_magic != SYSSTAT_MAGIC) ||
	    (file_magic->format_magic != FORMAT_MAGIC)) {

		if (ignore &&
		    (n == FILE_MAGIC_SIZE) &&
		    (file_magic->sysstat_magic == SYSSTAT_MAGIC))
	 /* Don't display error message. This is for sadf -H */
			return;
		else {
	 /* Display error message and exit */
			handle_invalid_sa_file(ifd, file_magic, dfile, n);
		}
	}

   /* Read sa data file standard header */
	sa_fread(*ifd, file_hdr, FILE_HDR_SIZE, HARD_SIZE);

	*actflag &= file_hdr->sa_actflag;
	if (!(*actflag) ||
	    (WANT_PER_PROC(flags) && !WANT_ALL_PROC(flags) && !file_hdr->sa_proc
	     && !(*actflag & ~(A_CPU + A_IRQ)))) {
      /*
       * We want stats that are not available,
       * maybe because this is an old version of the sa data file.
       * Error message is displayed if:
       * -> no activities remain in sar_actflag
       * -> or if the user entered eg 'sar -u -P 0 -f file' or
       * 'sar -I SUM -P 0 -f file', with file created on a UP machine.
       * NOTE1: If A_ONE_IRQ stats are available, stats
       * concerning _all_ the IRQs are available.
       * NOTE2: If file_hdr.sa_proc > 0, stats
       * concerning _all_ the CPUs are available.
       * NOTE3: If file_hdr.sa_irqcpu != 0, stats
       * concerning the IRQs per processor are available.
       */
		fprintf(stderr, _("Requested activities not available in file %s\n"),
			dfile);
		close(*ifd);
		exit(1);
	}
}

/*
 ***************************************************************************
 * Parse sar activities options (also used by sadf)
 *
 * IN:
 * @argv	Arguments list.
 * @opt		Index in list of arguments.
 * @caller	Indicate whether it's sar or sadf that called this function.
 *
 * OUT:
 * @actflag	Flag activities set.
 * @flags	Common flags and system state.
 * @irq_bitmap	Bitmap for interrupts.
 * @cpu_bitmap	Bitmap for CPUs.
 *
 * RETURNS:
 * 0 on success, 1 otherwise.
 ***************************************************************************
 */
int parse_sar_opt(char *argv[], int opt, unsigned int *actflag,
		  unsigned int *flags, int caller,
		  unsigned char irq_bitmap[], unsigned char cpu_bitmap[])
{
	int i;

	for (i = 1; *(argv[opt] + i); i++) {

		switch (*(argv[opt] + i)) {

		case 'A':
			*actflag |= A_PROC + A_PAGE + A_IRQ + A_IO + A_CPU +
				A_CTXSW + A_SWAP + A_MEMORY + A_SERIAL +
				A_MEM_AMT + A_KTABLES + A_NET_DEV +
				A_NET_EDEV + A_NET_SOCK + A_NET_NFS + A_NET_NFSD +
				A_QUEUE + A_DISK + A_ONE_IRQ;
	 /* Force '-P ALL -I XALL' */
			*flags |= S_F_A_OPTION + S_F_ALL_PROC + S_F_PER_PROC;
			init_bitmap(irq_bitmap, ~0, NR_IRQS);
			init_bitmap(cpu_bitmap, ~0, NR_CPUS);
			break;
		case 'B':
			*actflag |= A_PAGE;
			break;
		case 'b':
			*actflag |= A_IO;
			break;
		case 'C':
			*flags |= S_F_COMMENT;
			break;
		case 'c':
			*actflag |= A_PROC;
			break;
		case 'd':
			*actflag |= A_DISK;
			break;
		case 'p':
			*flags |= S_F_DEV_PRETTY;
			break;
		case 'q':
			*actflag |= A_QUEUE;
			break;
		case 'r':
			*actflag |= A_MEM_AMT;
			break;
		case 'R':
			*actflag |= A_MEMORY;
			break;
		case 't':
			if (caller == C_SAR)
				*flags |= S_F_TRUE_TIME;
			else
				return 1;
			break;
		case 'u':
			*actflag |= A_CPU;
			break;
		case 'v':
			*actflag |= A_KTABLES;
			break;
		case 'w':
			*actflag |= A_CTXSW;
			break;
		case 'W':
			*actflag |= A_SWAP;
			break;
		case 'y':
			*actflag |= A_SERIAL;
			break;
		case 'V':
			print_version();
			break;
		default:
			return 1;
		}
	}
	return 0;
}

/*
 ***************************************************************************
 * Parse sar "-n" option
 *
 * IN:
 * @argv	Arguments list.
 * @opt		Index in list of arguments.
 *
 * OUT:
 * @actflag	Flag activities set.
 *
 * RETURNS:
 * 0 on success, 1 otherwise.
 ***************************************************************************
 */
int parse_sar_n_opt(char *argv[], int *opt, unsigned int *actflag)
{
	if (!strcmp(argv[*opt], K_DEV))
		*actflag |= A_NET_DEV;
	else if (!strcmp(argv[*opt], K_EDEV))
		*actflag |= A_NET_EDEV;
	else if (!strcmp(argv[*opt], K_SOCK))
		*actflag |= A_NET_SOCK;
	else if (!strcmp(argv[*opt], K_NFS))
		*actflag |= A_NET_NFS;
	else if (!strcmp(argv[*opt], K_NFSD))
		*actflag |= A_NET_NFSD;
	else if (!strcmp(argv[*opt], K_ALL))
		*actflag |= A_NET_DEV + A_NET_EDEV + A_NET_SOCK + A_NET_NFS + A_NET_NFSD;
	else
		return 1;

	(*opt)++;
	return 0;
}

/*
 ***************************************************************************
 * Parse sar "-I" option
 *
 * IN:
 * @argv	Arguments list.
 * @opt		Index in list of arguments.
 *
 * OUT:
 * @actflag	Flag activities set.
 * @irq_bitmap	Bitmap for interrupts.
 *
 * RETURNS:
 * 0 on success, 1 otherwise.
 ***************************************************************************
 */
int parse_sar_I_opt(char *argv[], int *opt, unsigned int *actflag,
		    unsigned char irq_bitmap[])
{
	int i;

	if (!strcmp(argv[*opt], K_SUM))
		*actflag |= A_IRQ;
	else {
		*actflag |= A_ONE_IRQ;
		if (!strcmp(argv[*opt], K_ALL)) {
	 /* Set bit for the first 16 irq */
			irq_bitmap[0] = 0xff;
			irq_bitmap[1] = 0xff;
		}
		else if (!strcmp(argv[*opt], K_XALL))
	 /* Set every bit */
			init_bitmap(irq_bitmap, ~0, NR_IRQS);
		else {
	 /*
	  * Get irq number.
	  */
			if (strspn(argv[*opt], DIGITS) != strlen(argv[*opt]))
				return 1;
			i = atoi(argv[*opt]);
			if ((i < 0) || (i >= NR_IRQS))
				return 1;
			irq_bitmap[i >> 3] |= 1 << (i & 0x07);
		}
	}
	(*opt)++;
	return 0;
}

/*
 ***************************************************************************
 * Parse sar and sadf "-P" option
 *
 * IN:
 * @argv	Arguments list.
 * @opt		Index in list of arguments.
 *
 * OUT:
 * @flags	Common flags and system state.
 * @cpu_bitmap	Bitmap for CPUs.
 *
 * RETURNS:
 * 0 on success, 1 otherwise.
 ***************************************************************************
 */
int parse_sa_P_opt(char *argv[], int *opt, unsigned int *flags,
		   unsigned char cpu_bitmap[])
{
	int i;

	if (argv[++(*opt)]) {
		*flags |= S_F_PER_PROC;
		if (!strcmp(argv[*opt], K_ALL)) {
	 /*
	  * Set bit for every processor.
	  * We still don't know if we are going to read stats
	  * from a file or not...
	  */
			init_bitmap(cpu_bitmap, ~0, NR_CPUS);
			*flags |= S_F_ALL_PROC;
		}
		else {
			if (strspn(argv[*opt], DIGITS) != strlen(argv[*opt]))
				return 1;
			i = atoi(argv[*opt]);	/* Get cpu number */
			if ((i < 0) || (i >= NR_CPUS))
				return 1;
			cpu_bitmap[i >> 3] |= 1 << (i & 0x07);
		}
		(*opt)++;
	}
	else
		return 1;

	return 0;
}
