/* xscreensaver, Copyright (c) 1991-1995 Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* stderr hackery - Why Unix Sucks, reason number 32767.
 */

#if __STDC__
#include <stdlib.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <fcntl.h>
#include <time.h>

#include <X11/Intrinsic.h>

#include "xscreensaver.h"

extern XtAppContext app;
extern Colormap cmap;
extern Window screensaver_window;

extern char *get_string_resource P((char *, char *));
extern Bool get_boolean_resource P((char *, char *));
extern unsigned int get_pixel_resource P((char *, char *,
					  Display *, Colormap));

static char stderr_buffer [1024];
static char *stderr_tail = 0;
static time_t stderr_last_read = 0;
static XtIntervalId stderr_popup_timer = 0;

FILE *real_stderr = 0;
FILE *real_stdout = 0;

static int text_x = 0;
static int text_y = 0;

void
reset_stderr ()
{
  text_x = text_y = 0;
}

static void
print_stderr (string)
     char *string;
{
  int h_border = 20;
  int v_border = 20;
  static int line_height;
  static XFontStruct *font = 0;
  static GC gc = 0;
  char *head = string;
  char *tail;

  /* In verbose mode, copy it to stderr as well. */
  if (verbose_p)
    fprintf (real_stderr, "%s", string);

  if (! gc)
    {
      XGCValues gcv;
      Pixel fg, bg;
      char *font_name = get_string_resource ("font", "Font");
      if (!font_name) font_name = "fixed";
      font = XLoadQueryFont (dpy, font_name);
      if (! font) font = XLoadQueryFont (dpy, "fixed");
      line_height = font->ascent + font->descent;
      fg = get_pixel_resource ("textForeground", "Foreground", dpy, cmap);
      bg = get_pixel_resource ("textBackground", "Background", dpy, cmap);
      gcv.font = font->fid;
      gcv.foreground = fg;
      gcv.background = bg;
      gc = XCreateGC (dpy, screensaver_window,
		      (GCFont | GCForeground | GCBackground), &gcv);
    }

  for (tail = string; *tail; tail++)
    {
      if (*tail == '\n' || *tail == '\r')
	{
	  int maxy = HeightOfScreen (screen) - v_border - v_border;
	  if (tail != head)
	    XDrawImageString (dpy, screensaver_window, gc,
			      text_x + h_border,
			      text_y + v_border + font->ascent,
			      head, tail - head);
	  text_x = 0;
	  text_y += line_height;
	  head = tail + 1;
	  if (*tail == '\r' && *head == '\n')
	    head++, tail++;

	  if (text_y > maxy - line_height)
	    {
#if 0
	      text_y = 0;
#else
	      int offset = line_height * 5;
	      XCopyArea (dpy, screensaver_window, screensaver_window, gc,
			 0, v_border + offset,
			 WidthOfScreen (screen),
			 (HeightOfScreen (screen) - v_border - v_border
			  - offset),
			 0, v_border);
	      XClearArea (dpy, screensaver_window,
			  0, HeightOfScreen (screen) - v_border - offset,
			  WidthOfScreen (screen), offset, False);
	      text_y -= offset;
#endif
	    }
	}
    }
  if (tail != head)
    {
      int direction, ascent, descent;
      XCharStruct overall;
      XDrawImageString (dpy, screensaver_window, gc,
			text_x + h_border, text_y + v_border + font->ascent,
			head, tail - head);
      XTextExtents (font, tail, tail - head,
		    &direction, &ascent, &descent, &overall);
      text_x += overall.width;
    }
}


static void
stderr_popup_timer_fn (closure, id)
     XtPointer closure;
     XtIntervalId *id;
{
  char *s = stderr_buffer;
  if (*s)
    {
      /* If too much data was printed, then something has gone haywire,
	 so truncate it. */
      char *trailer = "\n\n<< stderr diagnostics have been truncated >>\n\n";
      int max = sizeof (stderr_buffer) - strlen (trailer) - 5;
      if (strlen (s) > max)
	strcpy (s + max, trailer);
      /* Now show the user. */
      print_stderr (s);
    }

  stderr_tail = stderr_buffer;
  stderr_popup_timer = 0;
}


static void
stderr_callback (closure, fd, id)
     XtPointer closure;
     int *fd;
     XtIntervalId *id;
{
  char *s;
  int left;
  int size;
  int read_this_time = 0;

  if (stderr_tail == 0)
    stderr_tail = stderr_buffer;

  left = ((sizeof (stderr_buffer) - 2) - (stderr_tail - stderr_buffer));

  s = stderr_tail;
  *s = 0;

  /* Read as much data from the fd as we can, up to our buffer size. */
  if (left > 0)
    {
      while ((size = read (*fd, (void *) s, left)) > 0)
	{
	  left -= size;
	  s += size;
	  read_this_time += size;
	}
      *s = 0;
    }
  else
    {
      char buf2 [1024];
      /* The buffer is full; flush the rest of it. */
      while (read (*fd, (void *) buf2, sizeof (buf2)) > 0)
	;
    }

  stderr_tail = s;
  stderr_last_read = time ((time_t *) 0);

  /* Now we have read some data that we would like to put up in a dialog
     box.  But more data may still be coming in - so don't pop up the
     dialog right now, but instead, start a timer that will pop it up
     a second from now.  Should more data come in in the meantime, we
     will be called again, and will reset that timer again.  So the
     dialog will only pop up when a second has elapsed with no new data
     being written to stderr.

     However, if the buffer is full (meaning lots of data has been written)
     then we don't reset the timer.
   */
  if (read_this_time > 0)
    {
      if (stderr_popup_timer)
	XtRemoveTimeOut (stderr_popup_timer);

      stderr_popup_timer =
	XtAppAddTimeOut (app, 1 * 1000, stderr_popup_timer_fn, 0);
    }
}

void
initialize_stderr ()
{
  static Boolean done = False;
  int fds [2];
  int in, out;
  int new_stdout, new_stderr;
  int stdout_fd = 1;
  int stderr_fd = 2;
  int flags;
  Boolean stderr_dialog_p = get_boolean_resource ("captureStderr", "Boolean");
  Boolean stdout_dialog_p = get_boolean_resource ("captureStdout", "Boolean");

  real_stderr = stderr;
  real_stdout = stdout;

  if (!stderr_dialog_p && !stdout_dialog_p)
    return;

  if (done) return;
  done = True;

  if (pipe (fds))
    {
      perror ("error creating pipe:");
      return;
    }

  in = fds [0];
  out = fds [1];

# ifdef O_NONBLOCK
  flags = O_NONBLOCK;
# else
#  ifdef O_NDELAY
  flags = O_NDELAY;
#  else
  ERROR!! neither O_NONBLOCK nor O_NDELAY are defined.
#  endif
# endif

    /* Set both sides of the pipe to nonblocking - this is so that
       our reads (in stderr_callback) will terminate, and so that
       out writes (in the client programs) will silently fail when
       the pipe is full, instead of hosing the program. */
  if (fcntl (in, F_SETFL, flags) != 0)
    {
      perror ("fcntl:");
      return;
    }
  if (fcntl (out, F_SETFL, flags) != 0)
    {
      perror ("fcntl:");
      return;
    }

  if (stderr_dialog_p)
    {
      FILE *new_stderr_file;
      new_stderr = dup (stderr_fd);
      if (new_stderr < 0)
	{
	  perror ("could not dup() a stderr:");
	  return;
	}
      if (! (new_stderr_file = fdopen (new_stderr, "w")))
	{
	  perror ("could not fdopen() the new stderr:");
	  return;
	}
      real_stderr = new_stderr_file;

      close (stderr_fd);
      if (dup2 (out, stderr_fd) < 0)
	{
	  perror ("could not dup() a new stderr:");
	  return;
	}
    }

  if (stdout_dialog_p)
    {
      FILE *new_stdout_file;
      new_stdout = dup (stdout_fd);
      if (new_stdout < 0)
	{
	  perror ("could not dup() a stdout:");
	  return;
	}
      if (! (new_stdout_file = fdopen (new_stdout, "w")))
	{
	  perror ("could not fdopen() the new stdout:");
	  return;
	}
      real_stdout = new_stdout_file;

      close (stdout_fd);
      if (dup2 (out, stdout_fd) < 0)
	{
	  perror ("could not dup() a new stdout:");
	  return;
	}
    }

  XtAppAddInput (app, in, (XtPointer) XtInputReadMask, stderr_callback, 0);
}
