#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)util.c	4.10 98/04/23 xlockmore";

#endif

/*-
 * util.c - various utilities - usleep, seconds, strdup, matherr
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * Revision History:
 *
 * Changes maintained by David Bagley <bagleyd@bigfoot.com>
 * 23-Apr-98: Separated stuff out of this file, changed name to util.c
 * 08-Jul-96: Bug in strcat_firstword fixed thanks to
 *            <Jeffrey_Doggett@caradon.com>.  Fix for ":not found" text
 *            that appears after about 40 minutes.
 * 04-Apr-96: Added procedures to handle wildcards on filenames
 *            J. Jansen <joukj@crys.chem.uva.nl>
 * 15-May-95: random number generator added, moved hsbramp.c to utils.c .
 *            Also renamed file from usleep.c to utils.c .
 * 14-Mar-95: patches for rand and seconds for VMS
 * 27-Feb-95: fixed nanosleep for times >= 1 second
 * 05-Jan-95: nanosleep for Solaris 2.3 and greater Greg Onufer
 *            <Greg.Onufer@Eng.Sun.COM>
 * 22-Jun-94: Fudged for VMS by Anthony Clarke
 *            <Anthony.D.Clarke@Support.Hatfield.Raytheon.bae.eurokom.ie>
 * 10-Jun-94: patch for BSD from Victor Langeveld <vic@mbfys.kun.nl>
 * 02-May-94: patch for Linux, got ideas from Darren Senn's xlock
 *            <sinster@scintilla.capitola.ca.us>
 * 21-Mar-94: patch fix for HP from <R.K.Lloyd@csc.liv.ac.uk> 
 * 01-Dec-93: added patch for HP
 *
 * Changes of Patrick J. Naughton
 * 30-Aug-90: written.
 *
 */

#include "xlock.h"

#include <sys/stat.h>

#ifdef USE_OLD_EVENT_LOOP
#if !defined( VMS ) || defined( XVMSUTILS ) || ( __VMS_VER >= 70000000 )
#ifdef USE_XVMSUTILS
#include <X11/unix_time.h>
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif
#else
#include <starlet.h>
#endif
#if defined( SYSV ) || defined( SVR4 )
#ifdef LESS_THAN_AIX3_2
#include <sys/poll.h>
#else /* !LESS_THAN_AIX3_2 */
#include <poll.h>
#endif /* !LESS_THAN_AIX3_2 */
#endif /* defined( SYSV ) || defined( SVR4 ) */

#ifndef HAVE_USLEEP
 /* usleep should be defined */
int
usleep(unsigned long usec)
{
#if (defined( SYSV ) || defined( SVR4 )) && !defined( __hpux )
#ifdef HAVE_NANOSLEEP
	{
		struct timespec rqt;

		rqt.tv_nsec = 1000 * (usec % (unsigned long) 1000000);
		rqt.tv_sec = usec / (unsigned long) 1000000;
		return nanosleep(&rqt, NULL);
	}
#else /* !HAVE_NANOSLEEP */
	(void) poll((void *) 0, (int) 0, usec / 1000);	/* ms resolution */
#endif /* !HAVE_NANOSLEEP */
#else /* !SYSV */
#if HAVE_GETTIMEOFDAY
	struct timeval time_out;

	time_out.tv_usec = usec % (unsigned long) 1000000;
	time_out.tv_sec = usec / (unsigned long) 1000000;
	(void) select(0, (void *) 0, (void *) 0, (void *) 0, &time_out);
#else
	long        timadr[2];

	if (usec != 0) {
		timadr[0] = -usec * 10;
		timadr[1] = -1;

		sys$setimr(4, &timadr, 0, 0, 0);
		sys$waitfr(4);
	}
#endif
#endif /* !SYSV */
	return 0;
}
#endif /* !HAVE_USLEEP */
#endif /* USE_OLD_EVENT_LOOP */

/*-
 * returns the number of seconds since 01-Jan-70.
 * This is used to control rate and timeout in many of the animations.
 */
unsigned long
seconds(void)
{
#if HAVE_GETTIMEOFDAY
	struct timeval now;

	GETTIMEOFDAY(&now);
	return (unsigned long) now.tv_sec;
#else
	return (unsigned long) time((time_t *) 0);
#endif
}

#if (! HAVE_STRDUP )
char       *
strdup(char *str)
{
	register char *ptr;

	ptr = (char *) malloc(strlen(str) + 1);
	(void) strcpy(ptr, str);
	return ptr;
}
#endif

#ifdef USE_MATHERR
/* Handle certain math exception errors */
int
matherr(register struct exception *x)
{
	extern Bool debug;

	switch (x->type) {
		case DOMAIN:
			/* Suppress "atan2: DOMAIN error" stderr message */
			if (!strcmp(x->name, "atan2")) {
				x->retval = 0.0;
				return ((debug) ? 0 : 1);	/* suppress message unless debugging */
			}
			if (!strcmp(x->name, "sqrt")) {
				x->retval = sqrt(-x->arg1);
				/* x->retval = 0.0; */
				return ((debug) ? 0 : 1);	/* suppress message unless debugging */
			}
			break;
#ifdef __hpux
			/* Fix how HP-UX does not like sin and cos of angles >= 360.  */
		case TLOSS:
			if (!strcmp(x->name, "cos")) {
				x->retval = cos(fmod(x->arg1, 360.0));
				return (1);	/* suppress message */
			}
			if (!strcmp(x->name, "sin")) {
				x->retval = sin(fmod(x->arg1, 360.0));
				return (1);	/* suppress message */
			}
			break;
		case PLOSS:
			return (1);
			break;
#endif
	}
	return (0);		/* all other exceptions, execute default procedure */
}
#endif
