/* xscreensaver, Copyright (c) 2016-2020 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Utility functions related to the hacks/ APIs.
 */

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <android/log.h>
#include "screenhackI.h"
#include "xlockmoreI.h"
#include "textclient.h"

#if defined(HAVE_IPHONE) || (HAVE_ANDROID)
# include "jwzgles.h"
#else
# include <OpenGL/OpenGL.h>
#endif

#ifndef isupper
# define isupper(c)  ((c) >= 'A' && (c) <= 'Z')
#endif
#ifndef _tolower
# define _tolower(c)  ((c) - 'A' + 'a')
#endif

Bool 
get_boolean_resource (Display *dpy, char *res_name, char *res_class)
{
  char *tmp, buf [100];
  char *s = get_string_resource (dpy, res_name, res_class);
  char *os = s;
  if (! s) return 0;
  for (tmp = buf; *s; s++)
    *tmp++ = isupper (*s) ? _tolower (*s) : *s;
  *tmp = 0;
  free (os);

  while (*buf &&
         (buf[strlen(buf)-1] == ' ' ||
          buf[strlen(buf)-1] == '\t'))
    buf[strlen(buf)-1] = 0;

  if (!strcmp (buf, "on") || !strcmp (buf, "true") || !strcmp (buf, "yes"))
    return 1;
  if (!strcmp (buf,"off") || !strcmp (buf, "false") || !strcmp (buf,"no"))
    return 0;
  fprintf (stderr, "%s: %s must be boolean, not %s.\n",
           progname, res_name, buf);
  return 0;
}

int 
get_integer_resource (Display *dpy, char *res_name, char *res_class)
{
  int val;
  char c, *s = get_string_resource (dpy, res_name, res_class);
  char *ss = s;
  if (!s) return 0;

  while (*ss && *ss <= ' ') ss++;                       /* skip whitespace */

  if (ss[0] == '0' && (ss[1] == 'x' || ss[1] == 'X'))   /* 0x: parse as hex */
    {
      if (1 == sscanf (ss+2, "%x %c", (unsigned int *) &val, &c))
        {
          free (s);
          return val;
        }
    }
  else                                                  /* else parse as dec */
    {
      /* Allow integer values to end in ".0". */
      int L = strlen(ss);
      if (L > 2 && ss[L-2] == '.' && ss[L-1] == '0')
        ss[L-2] = 0;

      if (1 == sscanf (ss, "%d %c", &val, &c))
        {
          free (s);
          return val;
        }
    }

  fprintf (stderr, "%s: %s must be an integer, not %s.\n",
           progname, res_name, s);
  free (s);
  return 0;
}

double
get_float_resource (Display *dpy, char *res_name, char *res_class)
{
  double val;
  char c, *s = get_string_resource (dpy, res_name, res_class);
  if (! s) return 0.0;
  if (1 == sscanf (s, " %lf %c", &val, &c))
    {
      free (s);
      return val;
    }
  fprintf (stderr, "%s: %s must be a float, not %s.\n",
           progname, res_name, s);
  free (s);
  return 0.0;
}


char *
textclient_mobile_date_string (void)
{
  struct utsname uts;
  if (uname (&uts) < 0)
    return strdup("uname() failed");
  else
    {
      time_t now = time ((time_t *) 0);
      char *ts = ctime (&now);
      char *buf, *s;
      if ((s = strchr(uts.nodename, '.')))
        *s = 0;
      buf = (char *) malloc(strlen(uts.machine) +
                            strlen(uts.sysname) +
                            strlen(uts.release) +
                            strlen(ts) + 10);
      sprintf (buf, "%s %s %s\n%s", uts.machine, uts.sysname, uts.release, ts);
      return buf;
    }
}


/* used by the OpenGL screen savers
 */

/* Does nothing - prepareContext already did the work.
 */
void
glXMakeCurrent (Display *dpy, Window window, GLXContext context)
{
}


/* clear away any lingering error codes */
void
clear_gl_error (void)
{
  while (glGetError() != GL_NO_ERROR)
    ;
}


// needs to be implemented in Android...
/* Copy the back buffer to the front buffer.
 */
void
glXSwapBuffers (Display *dpy, Window window)
{
}


/* Called by OpenGL savers using the XLockmore API.
 */
GLXContext *
init_GL (ModeInfo *mi)
{
  // Window win = mi->window;

  // Caller expects a pointer to an opaque struct...  which it dereferences.
  // Don't ask me, it's historical...
  static int blort = -1;
  return (void *) &blort;
}

/* report a GL error. */
void
check_gl_error (const char *type)
{
  char buf[100];
  char buf2[200];
  GLenum i;
  const char *e;
  switch ((i = glGetError())) {
    case GL_NO_ERROR: return;
    case GL_INVALID_ENUM:          e = "invalid enum";      break;
    case GL_INVALID_VALUE:         e = "invalid value";     break;
    case GL_INVALID_OPERATION:     e = "invalid operation"; break;
    case GL_STACK_OVERFLOW:        e = "stack overflow";    break;
    case GL_STACK_UNDERFLOW:       e = "stack underflow";   break;
    case GL_OUT_OF_MEMORY:         e = "out of memory";     break;
#ifdef GL_TABLE_TOO_LARGE_EXT
    case GL_TABLE_TOO_LARGE_EXT:   e = "table too large";   break;
#endif
#ifdef GL_TEXTURE_TOO_LARGE_EXT
    case GL_TEXTURE_TOO_LARGE_EXT: e = "texture too large"; break;
#endif
    default:
      e = buf; sprintf (buf, "unknown GL error %d", (int) i); break;
  }
  sprintf (buf2, "%.50s: %.50s", type, e);
  __android_log_write(ANDROID_LOG_ERROR, "xscreensaver", buf2);
}
