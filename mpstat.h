/*
 * mpstat: per-processor statistics
 * (C) 2000-2003 by Sebastien Godard <sebastien.godard@wanadoo.fr>
 */

#ifndef _MPSTAT_H
#define _MPSTAT_H


struct mp_stats {
   unsigned long cpu_idle			__attribute__ ((aligned (8)));
   unsigned long cpu_iowait			__attribute__ ((aligned (8)));
   unsigned int  cpu_user			__attribute__ ((aligned (8)));
   unsigned int  cpu_nice			__attribute__ ((packed));
   unsigned int  cpu_system			__attribute__ ((packed));
   unsigned int  irq				__attribute__ ((packed));
   /* Structure must be a multiple of 8 bytes, since we use an array of structures.
    * Each structure is *aligned*, and we want the structures to be packed together. */
};

#define MP_STATS_SIZE	(sizeof(int) * 4 + \
			 SIZEOF_LONG * 2)


struct mp_timestamp {
   unsigned long uptime;
   unsigned char hour;		/* (0-23) */
   unsigned char minute;	/* (0-59) */
   unsigned char second;	/* (0-59) */
};

#endif
