/* stderr.c --- capturing stdout/stderr output onto the screensaver window.
 * xscreensaver, Copyright (c) 1991-1998 Jamie Zawinski <jwz@netscape.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include <stdio.h>
#include <time.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_FCNTL
# include <fcntl.h>
#endif

#include <X11/Intrinsic.h>

#include "xscreensaver.h"
#include "resources.h"
#include "visual.h"

FILE *real_stderr = 0;
FILE *real_stdout = 0;


/* It's ok for these to be global, since they refer to the one and only
   stderr stream, not to a particular screen or window or visual.
 */
static char stderr_buffer [4096];
static char *stderr_tail = 0;
static time_t stderr_last_read = 0;

static void make_stderr_overlay_window (saver_screen_info *);


void
reset_stderr (saver_screen_info *ssi)
{
  saver_info *si = ssi->global;

  if (si->prefs.debug_p)
    fprintf ((real_stderr ? real_stderr : stderr),
	     "%s: resetting stderr\n", progname);

  ssi->stderr_text_x = 0;
  ssi->stderr_text_y = 0;

  if (ssi->stderr_gc)
    XFreeGC (si->dpy, ssi->stderr_gc);
  ssi->stderr_gc = 0;

  if (ssi->stderr_overlay_window)
    XDestroyWindow(si->dpy, ssi->stderr_overlay_window);
  ssi->stderr_overlay_window = 0;

  if (ssi->stderr_cmap)
    XFreeColormap(si->dpy, ssi->stderr_cmap);
  ssi->stderr_cmap = 0;
}

void
clear_stderr (saver_screen_info *ssi)
{
  saver_info *si = ssi->global;
  ssi->stderr_text_x = 0;
  ssi->stderr_text_y = 0;
  if (ssi->stderr_overlay_window)
    XClearWindow (si->dpy, ssi->stderr_overlay_window);
}


static void
print_stderr_1 (saver_screen_info *ssi, char *string)
{
  saver_info *si = ssi->global;
  Display *dpy = si->dpy;
  Screen *screen = ssi->screen;
  Window window = (ssi->stderr_overlay_window ?
		   ssi->stderr_overlay_window :
		   ssi->screensaver_window);
  int h_border = 20;
  int v_border = 20;
  char *head = string;
  char *tail;

  if (! ssi->stderr_font)
    {
      char *font_name = get_string_resource ("font", "Font");
      if (!font_name) font_name = "fixed";
      ssi->stderr_font = XLoadQueryFont (dpy, font_name);
      if (! ssi->stderr_font) ssi->stderr_font = XLoadQueryFont (dpy, "fixed");
      ssi->stderr_line_height = (ssi->stderr_font->ascent +
				 ssi->stderr_font->descent);
    }

  if (! ssi->stderr_gc)
    {
      XGCValues gcv;
      Pixel fg, bg;
      Colormap cmap = ssi->cmap;

      if (!ssi->stderr_overlay_window &&
	  get_boolean_resource("overlayStderr", "Boolean"))
	{
	  make_stderr_overlay_window (ssi);
	  if (ssi->stderr_overlay_window)
	    window = ssi->stderr_overlay_window;
	  if (ssi->stderr_cmap)
	    cmap = ssi->stderr_cmap;
	}

      fg = get_pixel_resource ("overlayTextForeground","Foreground",dpy,cmap);
      bg = get_pixel_resource ("overlayTextBackground","Background",dpy,cmap);
      gcv.font = ssi->stderr_font->fid;
      gcv.foreground = fg;
      gcv.background = bg;
      ssi->stderr_gc = XCreateGC (dpy, window,
				  (GCFont | GCForeground | GCBackground),
				  &gcv);
    }


  if (ssi->stderr_cmap)
    XInstallColormap(si->dpy, ssi->stderr_cmap);

  for (tail = string; *tail; tail++)
    {
      if (*tail == '\n' || *tail == '\r')
	{
	  int maxy = HeightOfScreen (screen) - v_border - v_border;
	  if (tail != head)
	    XDrawImageString (dpy, window, ssi->stderr_gc,
			      ssi->stderr_text_x + h_border,
			      ssi->stderr_text_y + v_border +
			      ssi->stderr_font->ascent,
			      head, tail - head);
	  ssi->stderr_text_x = 0;
	  ssi->stderr_text_y += ssi->stderr_line_height;
	  head = tail + 1;
	  if (*tail == '\r' && *head == '\n')
	    head++, tail++;

	  if (ssi->stderr_text_y > maxy - ssi->stderr_line_height)
	    {
#if 0
	      ssi->stderr_text_y = 0;
#else
	      int offset = ssi->stderr_line_height * 5;
	      XCopyArea (dpy, window, window, ssi->stderr_gc,
			 0, v_border + offset,
			 WidthOfScreen (screen),
			 (HeightOfScreen (screen) - v_border - v_border
			  - offset),
			 0, v_border);
	      XClearArea (dpy, window,
			  0, HeightOfScreen (screen) - v_border - offset,
			  WidthOfScreen (screen), offset, False);
	      ssi->stderr_text_y -= offset;
#endif
	    }
	}
    }
  if (tail != head)
    {
      int direction, ascent, descent;
      XCharStruct overall;
      XDrawImageString (dpy, window, ssi->stderr_gc,
			ssi->stderr_text_x + h_border,
			ssi->stderr_text_y + v_border
			  + ssi->stderr_font->ascent,
			head, tail - head);
      XTextExtents (ssi->stderr_font, tail, tail - head,
		    &direction, &ascent, &descent, &overall);
      ssi->stderr_text_x += overall.width;
    }
}

static void
make_stderr_overlay_window (saver_screen_info *ssi)
{
  saver_info *si = ssi->global;
  unsigned long transparent_pixel = 0;
  Visual *visual = get_overlay_visual (ssi->screen, &transparent_pixel);
  if (visual)
    {
      int depth = visual_depth (ssi->screen, visual);
      XSetWindowAttributes attrs;
      unsigned long attrmask;

      if (si->prefs.debug_p)
	fprintf(real_stderr,
		"%s: using overlay visual 0x%0x for stderr text layer.\n",
		progname, (int) XVisualIDFromVisual (visual));

      ssi->stderr_cmap = XCreateColormap(si->dpy,
					 RootWindowOfScreen(ssi->screen),
					 visual, AllocNone);

      attrmask = (CWColormap | CWBackPixel | CWBackingPixel | CWBorderPixel |
		  CWBackingStore | CWSaveUnder);
      attrs.colormap = ssi->stderr_cmap;
      attrs.background_pixel = transparent_pixel;
      attrs.backing_pixel = transparent_pixel;
      attrs.border_pixel = transparent_pixel;
      attrs.backing_store = NotUseful;
      attrs.save_under = False;

      ssi->stderr_overlay_window =
	XCreateWindow(si->dpy, ssi->screensaver_window, 0, 0,
		      WidthOfScreen(ssi->screen), HeightOfScreen(ssi->screen),
		      0, depth, InputOutput, visual, attrmask, &attrs);
      XMapRaised(si->dpy, ssi->stderr_overlay_window);
    }
}


static void
print_stderr (saver_info *si, char *string)
{
  saver_preferences *p = &si->prefs;
  int i;

  /* In verbose mode, copy it to stderr as well. */
  if (p->verbose_p)
    fprintf (real_stderr, "%s", string);

  for (i = 0; i < si->nscreens; i++)
    print_stderr_1 (&si->screens[i], string);
}


static void
stderr_popup_timer_fn (XtPointer closure, XtIntervalId *id)
{
  saver_info *si = (saver_info *) closure;
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
      print_stderr (si, s);
    }

  stderr_tail = stderr_buffer;
  si->stderr_popup_timer = 0;
}


static void
stderr_callback (XtPointer closure, int *fd, XtIntervalId *id)
{
  saver_info *si = (saver_info *) closure;
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
      if (si->stderr_popup_timer)
	XtRemoveTimeOut (si->stderr_popup_timer);

      si->stderr_popup_timer =
	XtAppAddTimeOut (si->app, 1 * 1000, stderr_popup_timer_fn,
			 (XtPointer) si);
    }
}

void
initialize_stderr (saver_info *si)
{
  static Boolean done = False;
  int fds [2];
  int in, out;
  int new_stdout, new_stderr;
  int stdout_fd = 1;
  int stderr_fd = 2;
  int flags = 0;
  Boolean stderr_dialog_p, stdout_dialog_p;

  if (done) return;
  done = True;

  real_stderr = stderr;
  real_stdout = stdout;

  stderr_dialog_p = get_boolean_resource ("captureStderr", "Boolean");
  stdout_dialog_p = get_boolean_resource ("captureStdout", "Boolean");

  if (!stderr_dialog_p && !stdout_dialog_p)
    return;

  if (pipe (fds))
    {
      perror ("error creating pipe:");
      return;
    }

  in = fds [0];
  out = fds [1];

# ifdef HAVE_FCNTL

#  if defined(O_NONBLOCK)
   flags = O_NONBLOCK;
#  elif defined(O_NDELAY)
   flags = O_NDELAY;
#  else
   ERROR!! neither O_NONBLOCK nor O_NDELAY are defined.
#  endif

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

# endif /* !HAVE_FCNTL */

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

  XtAppAddInput (si->app, in, (XtPointer) XtInputReadMask, stderr_callback,
		 (XtPointer) si);
}
