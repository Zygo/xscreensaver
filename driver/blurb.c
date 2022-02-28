/* xscreensaver, Copyright © 1991-2021 Jamie Zawinski <jwz@jwz.org>
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

const char *progname = "";
int verbose_p = 0;

const char *
blurb (void)
{
  static char buf[255] = { 0 };
  struct tm tm;
  time_t now;
  int i;

  now = time ((time_t *) 0);
  localtime_r (&now, &tm);
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
  buf[i] = 0;
  return buf;
}

