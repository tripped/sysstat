/*
 * sysstat: system monitoring tools for Linux
 * (C) 1999,2000 by Sebastien Godard <sebastien.godard@wanadoo.fr>
 */

#ifndef _COMMON_H
#define _COMMON_H

#include <time.h>


/*
 * Old formulas:
 * INT_PART(a,b,c)	(((a) - (b)) / (c))
 * DEC_PART(a,b,c)	(((((a) - (b)) % (c)) * 100) / (c))
 */
#define INT_PART(a,b,c)	((((a) - (b)) * 100) / (c))
#define DEC_PART(a,b,c)	((((((a) - (b)) * 100) % (c)) * 100) / (c))

/* Keywords */
#define K_ISO	"ISO"
#define K_ALL	"ALL"

/* Files */
#define STAT		"/proc/stat"
#define INTERRUPTS	"/proc/interrupts"

#define MAX_FILE_LEN	256

/* Define flags */
#define F_BOOT_STATS	0x10

#define WANT_BOOT_STATS(m)	(((m) & F_BOOT_STATS) == F_BOOT_STATS)

/*
 * 0: stats at t,
 * 1: stats at t' (t+T or t-T),
 * 2: average.
 */
#define DIM	3

/* Environment variable */
#define TM_FMT_VAR	"S_TIME_FORMAT"

#define DIGITS		"0123456789"

#define UTSNAME_LEN	65

/* Size of a long int: 8 bytes because of 64-bit systems */
#define SIZEOF_LONG	8

#define NR_DISKS	4

#define DISP_HDR	1

/* Functions */
extern time_t get_localtime(struct tm *);
extern int    get_nb_proc_used(int *, unsigned int);
extern int    get_win_height(void);
extern void   init_nls(char *);
extern void   print_gal_header(struct tm *, char *, char *, char *);


#endif  /* _COMMON_H */
