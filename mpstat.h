/*
 * mpstat: per-processor statistics
 * (C) 2000-2007 by Sebastien Godard (sysstat <at> wanadoo.fr)
 */

#ifndef _MPSTAT_H
#define _MPSTAT_H


struct mp_stats {
   unsigned long long cpu_idle			__attribute__ ((aligned (8)));
   unsigned long long cpu_iowait		__attribute__ ((packed));
   unsigned long long cpu_user			__attribute__ ((packed));
   unsigned long long cpu_nice			__attribute__ ((packed));
   unsigned long long cpu_system		__attribute__ ((packed));
   unsigned long long cpu_hardirq		__attribute__ ((packed));
   unsigned long long cpu_softirq		__attribute__ ((packed));
   unsigned long long cpu_steal			__attribute__ ((packed));
   unsigned long long irq			__attribute__ ((packed));
};

#define MP_STATS_SIZE	(sizeof(struct mp_stats))

#endif
