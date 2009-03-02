/* xscreensaver, Copyright (c) 1992 Jamie Zawinski <jwz@mcom.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#if __STDC__
#include <stdlib.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xos.h>	/* lazy way out */

/* usleep() doesn't exist everywhere, and select() is faster anyway.
 */

#ifndef VMS

#ifdef NO_SELECT
  /* If you don't have select() or usleep(), I guess you lose...
     Maybe you have napms() instead?  Let me know.
   */
void
screenhack_usleep (usecs)
     unsigned long usecs;
{
  usleep (usecs);
}

#else /* ! NO_SELECT */

void
screenhack_usleep (usecs)
     unsigned long usecs;
{
  struct timeval tv;
  tv.tv_sec  = usecs / 1000000L;
  tv.tv_usec = usecs % 1000000L;
  (void) select (0, 0, 0, 0, &tv);
}

#endif /* ! NO_SELECT */

#else /* VMS */

#define SEC_DELTA  "0000 00:00:01.00"
#define TICK_DELTA "0000 00:00:00.08"
static int bin_sec_delta[2], bin_tick_delta[2], deltas_set = 0;

static void
set_deltas ()
{
  int status;
  extern int SYS$BINTIM ();
  $DESCRIPTOR (str_sec_delta,  SEC_DELTA);
  $DESCRIPTOR (str_tick_delta, TICK_DELTA);
  if (!deltas_set)
    {
      status = SYS$BINTIM (&str_sec_delta, &bin_sec_delta);
      if (!(status & 1))
	{
	  fprintf (stderr, "%s: cannot convert delta time ", progname);
	  fprintf (stderr, SEC_DELTA);
	  fprintf (stderr, "; status code = %d\n", status);
	  exit (status);
	}
      status = SYS$BINTIM (&str_tick_delta, &bin_tick_delta);
      if (!(status & 1))
	{
	  fprintf (stderr, "%s: cannot convert delta time ", progname);
	  fprintf (stderr, TICK_DELTA);
	  fprintf (stderr, "; status code = %d\n", status);
	  exit (status);
	}
      deltas_set = 1;
    }
}

void
screenhack_usleep (usecs)
     unsigned long usecs;
{
  int status, *bin_delta;
  extern int SYS$SCHWDK (), SYS$HIBER (); 
  
  if (!deltas_set) set_deltas ();
  bin_delta = (usecs == TICK_INTERVAL) ? &bin_tick_delta : &bin_sec_delta;
  status = SYS$SCHDWK (0, 0, bin_delta, 0);
  if ((status & 1)) (void) SYS$HIBER ();
}

#endif /*VMS */
