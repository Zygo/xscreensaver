/* prefs.c --- reading and writing the ~/.xscreensaver file.
 * xscreensaver, Copyright © 1998-2021 Jamie Zawinski <jwz@jwz.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "blurb.h"
#include "prefs.h"


static char *
strip (char *s)
{
  char *s2;
  while (*s == '\t' || *s == ' ' || *s == '\r' || *s == '\n')
    s++;
  for (s2 = s; *s2; s2++)
    ;
  for (s2--; s2 >= s; s2--) 
    if (*s2 == '\t' || *s2 == ' ' || *s2 == '\r' || *s2 =='\n') 
      *s2 = 0;
    else
      break;
  return s;
}


/* Parse the .xscreensaver or XScreenSaver.ad file and run the callback
   for each key-value pair.
*/
int
parse_init_file (const char *name,
                 void (*handler) (int lineno, 
                                  const char *key, const char *val,
                                  void *closure),
                 void *closure)
{
  int line = 0;
  struct stat st;
  FILE *in;
  int buf_size = 1024;
  char *buf = 0;

  if (!name) return 0;
  if (stat (name, &st) != 0) goto FAIL;

  buf = (char *) malloc (buf_size);
  if (!buf) goto FAIL;

  in = fopen(name, "r");
  if (!in)
    {
      sprintf (buf, "%s: error reading \"%.100s\"", blurb(), name);
      perror (buf);
      goto FAIL;
    }

  if (fstat (fileno (in), &st) != 0)
    {
      sprintf (buf, "%s: couldn't re-stat \"%.100s\"", blurb(), name);
      perror (buf);
      free (buf);
      return -1;
    }

  while (fgets (buf, buf_size-1, in))
    {
      char *key, *value;
      int L = strlen (buf);

      line++;
      while (L > 2 &&
	     (buf[L-1] != '\n' ||	/* whole line didn't fit in buffer */
	      buf[L-2] == '\\'))	/* or line ended with backslash */
	{
	  if (buf[L-2] == '\\')		/* backslash-newline gets swallowed */
	    {
	      buf[L-2] = 0;
	      L -= 2;
	    }
	  buf_size += 1024;
	  buf = (char *) realloc (buf, buf_size);
	  if (!buf) goto FAIL;

	  line++;
	  if (!fgets (buf + L, buf_size-L-1, in))
	    break;
	  L = strlen (buf);
	}

      /* Now handle other backslash escapes. */
      {
	int i, j;
	for (i = 0; buf[i]; i++)
	  if (buf[i] == '\\')
	    {
	      switch (buf[i+1])
		{
		case 'n': buf[i] = '\n'; break;
		case 'r': buf[i] = '\r'; break;
		case 't': buf[i] = '\t'; break;
		default:  buf[i] = buf[i+1]; break;
		}
	      for (j = i+2; buf[j]; j++)
		buf[j-1] = buf[j];
	      buf[j-1] = 0;
	    }
      }

      key = strip (buf);

      if (*key == '#' || *key == '!' || *key == ';' ||
	  *key == '\n' || *key == 0)
	continue;

      value = strchr (key, ':');
      if (!value)
	{
	  fprintf (stderr, "%s: %s:%d: unparsable line: %s\n", blurb(),
                   name, line, key);
	  continue;
	}
      else
	{
	  *value++ = 0;
	  value = strip (value);
	}

      handler (line, key, value, closure);
    }
  fclose (in);
  free (buf);
  return 0;

 FAIL:
  if (buf) free (buf);
  return -1;
}
