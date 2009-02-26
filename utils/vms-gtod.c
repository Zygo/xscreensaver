/*
 *	UNIX-style Time Functions, by pmoreau@cena.dgac.fr <Patrick MOREAU>
 *      (picked up from XVMSUTILS unix emulation routines for VMS by
 *       Trevor Taylor, Patrick Mahans and Martin P.J. Zinser)
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include "vms-gtod.h"

/*
 *	gettimeofday(2) - Returns the current time
 */

int gettimeofday(tv)
struct timeval  *tv;
{
    timeb_t tmp_time;
    ftime(&tmp_time);
    tv->tv_sec  = tmp_time.time;
    tv->tv_usec = tmp_time.millitm * 1000;
    return (0);
}
