/* test-yarandom.c --- generate a file of random bytes for analysis.
 * xscreensaver, Copyright (c) 2018 Jamie Zawinski <jwz@jwz.org>
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

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include "blurb.h"
#include "yarandom.h"

int
main (int argc, char **argv)
{
  unsigned long i, n;
  char *f;
  FILE *fd;

  progname = argv[0];
  if (argc != 3)
    {
      fprintf(stderr, "usage: %s bytes outfile\n", argv[0]);
      exit(1);
    }

  n = atol (argv[1]);
  f = argv[2];

# undef ya_rand_init
  ya_rand_init(0);

  fd = fopen (f, "w");
  if (!fd) { perror (f); exit (1); }

  n /= sizeof(uint32_t);
  for (i = 0; i < n; i++)
    {
      union { uint32_t i; char s[sizeof(uint32_t)]; } rr;
      rr.i = random();
      if (! fwrite (rr.s, sizeof(rr.s), 1, fd))
        {
          perror ("write");
          exit (1);
        }
    }
  fclose (fd);
  fprintf (stderr, "%s: %s: wrote %ld bytes\n",
           progname, f, i * sizeof(uint32_t));
  exit(0);
}
