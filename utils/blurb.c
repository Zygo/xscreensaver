/* xscreensaver, Copyright © 1991-2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "blurb.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>

const char *progname = "";
int verbose_p = 0;

/* #define BLURB_CENTISECONDS */

const char *
blurb (void)
{
  static char buf[255] = { 0 };
  struct tm tm;
  struct timeval now;
  int i;

# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday (&now, &tzp);
# else
  gettimeofday (&now);
# endif

  localtime_r (&now.tv_sec, &tm);
  i = strlen (progname);
  if (i > 40) i = 40;
  memcpy (buf, progname, i);
  buf[i++] = ':';
  buf[i++] = ' ';
  buf[i++] = '0' + (tm.tm_hour >= 10 ? tm.tm_hour/10 : 0);
  buf[i++] = '0' + (tm.tm_hour % 10);
  buf[i++] = ':';
  buf[i++] = '0' + (tm.tm_min >= 10 ? tm.tm_min/10 : 0);
  buf[i++] = '0' + (tm.tm_min % 10);
  buf[i++] = ':';
  buf[i++] = '0' + (tm.tm_sec >= 10 ? tm.tm_sec/10 : 0);
  buf[i++] = '0' + (tm.tm_sec % 10);

# ifdef BLURB_CENTISECONDS
  {
    int c = now.tv_usec / 10000;
    buf[i++] = '.';
    buf[i++] = '0' + (c >= 10 ? c/10 : 0);
    buf[i++] = '0' + (c % 10);
  }
# endif

  buf[i] = 0;
  return buf;
}

