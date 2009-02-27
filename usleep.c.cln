#ifndef lint
static char sccsid[] = "@(#)usleep.c	1.3 91/05/24 XLOCK";
#endif
/*-
 * usleep.c - OS dependant implementation of usleep().
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * Revision History:
 * 30-Aug-90: written.
 *
 */

#include "xlock.h"

int
usleep(usec)
    unsigned long usec;
{
#ifdef SYSV
    poll((struct poll *) 0, (size_t) 0, usec / 1000);	/* ms resolution */
#else
    struct timeval timeout;
    timeout.tv_usec = usec % (unsigned long) 1000000;
    timeout.tv_sec = usec / (unsigned long) 1000000;
    select(0, (void *) 0, (void *) 0, (void *) 0, &timeout);
#endif
    return 0;
}

/*
 * returns the number of seconds since 01-Jan-70.
 * This is used to control rate and timeout in many of the animations.
 */
long
seconds()
{
    struct timeval now;

    gettimeofday(&now, (struct timezone *) 0);
    return now.tv_sec;
}
