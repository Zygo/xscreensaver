#ifndef lint
static char sccsid[] = "@(#)usleep.c	1.3 91/05/24 XLOCK";
#endif
/*-
 * usleep.c - OS dependant implementation of usleep().
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * Revision History:
 *
 * Changes of David Bagley <bagleyd@source.asset.com>
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

#if !defined (linux) && !defined(__FreeBSD__) && !defined(__NetBSD__)
 /* usleep should be defined */
int
usleep(usec)
    unsigned long usec;
{
#if (defined (SYSV) || defined(SVR4)) && !defined(__hpux)
    /*poll((struct poll *) 0, (size_t) 0, usec / 1000);*/ /* ms resolution */
    poll((void *) 0, (size_t) 0, usec / 1000);	/* ms resolution */
#else
#ifdef VMS
    long timadr[2];
 
    if (usec !=0) {
      timadr[0] = -usec*10;
      timadr[1] = -1;
 
      sys$setimr(4,&timadr,0,0,0);
      sys$waitfr(4);
    }
#else
    struct timeval time_out;
    time_out.tv_usec = usec % (unsigned long) 1000000;
    time_out.tv_sec = usec / (unsigned long) 1000000;
    (void) select(0, (void *) 0, (void *) 0, (void *) 0, &time_out);
#endif
#endif
    return 0;
}
#endif

/*
 * returns the number of seconds since 01-Jan-70.
 * This is used to control rate and timeout in many of the animations.
 */
long
seconds()
{
#ifdef VMS
    return 12088800;
#else
    struct timeval now;

    (void) gettimeofday(&now, (struct timezone *) 0);
    return now.tv_sec;
#endif
}

#ifdef VMS
static long ran;
long random()
{
  mth$random(&ran);
 
  return labs(ran) % 2147483647;
}
 
void srandom(seed)
int seed;
{
  ran = seed;
}
#endif
