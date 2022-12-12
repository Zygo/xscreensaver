/* xscreensaver, Copyright (c) 2006-2020 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

 /* This is the Cocoa shim for webcollage.

    It runs the webcollage perl script
    (in "WebCollage.saver/Contents/Resources/webcollage")
    at the end of a pipe; each time that script updates the image file on
    disk it prints the file name, and this program loads and displays that
   image.

    The script uses "WebCollage.saver/Contents/Resources/webcollage-helper"
    to paste the images together in the usual way.
  */

#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#import <Cocoa/Cocoa.h>

#include "screenhack.h"


typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  int delay;
  pid_t pid;
  FILE *pipe_fd;
  XtInputId pipe_id;
  Bool verbose_p;
} state;


/* Violating the cardinal rule of "don't use global variables",
   but we need to get at these from the atexit() handler, and
   the callback doesn't take a closure arg.  Because apparently
   those hadn't been invented yet in the seventies.
 */
static state *all_states[50] = { 0, };

static void webcollage_atexit (void);
static void signal_handler (int sig);


static void
subproc_cb (XtPointer closure, int *source, XtInputId *id)
{
  /* state *st = (state *) closure; */
  /* st->input_available_p = True; */
}


/* whether there is data available to be read on the file descriptor
 */
static int
input_available_p (int fd)
{
  struct timeval tv = { 0, };
  fd_set fds;
# if 0
  /* This breaks on BSD, which uses bzero() in the definition of FD_ZERO */
  FD_ZERO (&fds);
# else
  memset (&fds, 0, sizeof(fds));
# endif
  FD_SET (fd, &fds);
  return select (fd+1, &fds, NULL, NULL, &tv);
}


static void
display_image (Display *dpy, Window window, state *st, const char *file)
{
  NSImage *image = [[NSImage alloc] 
                     initWithContentsOfFile:
                       [NSString stringWithCString: file
                                          encoding: NSUTF8StringEncoding]];

  if (! image) {
    fprintf (stderr, "webcollage: failed to load \"%s\"\n", file);
    return;
  }

  CGFloat w = [image size].width;
  CGFloat h = [image size].height;
  if (w <= 1 || h <= 1) {
    fprintf (stderr, "webcollage: unparsable image \"%s\"\n", file);
    [image release];
    return;
  }

  jwxyz_draw_NSImage_or_CGImage (dpy, window, True, image, 0, 0);
  [image release];
}


static void
open_pipe (state *st)
{
  /* This mess is because popen() doesn't give us the pid.
   */

  pid_t forked;
  int fds [2];
  int in, out;
  char buf[1024];

  char *av[20];
  int ac = 0;

  int timeout   = get_integer_resource (st->dpy, "timeout", "Timeout");
  int delay     = get_integer_resource (st->dpy, "delay",   "Delay");
  float opacity = get_float_resource   (st->dpy, "opacity", "Opacity");
  char *filter  = get_string_resource  (st->dpy, "filter",  "Filter");
  char *filter2 = get_string_resource  (st->dpy, "filter2", "Filter2");

  av[ac++] = strdup ("webcollage.pl");
  av[ac++] = strdup ("-cocoa");

  av[ac++] = strdup ("-size");
  sprintf (buf, "%dx%d", st->xgwa.width, st->xgwa.height);
  av[ac++] = strdup (buf);

  av[ac++] = strdup ("-timeout"); sprintf (buf, "%d", timeout);
  av[ac++] = strdup (buf);
  av[ac++] = strdup ("-delay");   sprintf (buf, "%d", delay);
  av[ac++] = strdup (buf);
  av[ac++] = strdup ("-opacity"); sprintf (buf, "%.2f", opacity);
  av[ac++] = strdup (buf);

  if (filter && *filter) {
    av[ac++] = strdup ("-filter");
    av[ac++] = filter;
  }
  if (filter2 && *filter2) {
    av[ac++] = strdup ("-filter2");
    av[ac++] = filter2;
  }

  av[ac] = 0;


  if (st->verbose_p) {
    fprintf (stderr, "webcollage: launching:");
    int i;
    for (i = 0; i < ac; i++)
      fprintf (stderr, " %s", av[i]);
    fprintf (stderr, "\n");
  }


  if (pipe (fds))
    {
      perror ("webcollage: error creating pipe");
      exit (1);
    }

  in = fds [0];
  out = fds [1];

  switch ((int) (forked = fork ()))
    {
    case -1:
      {
        perror ("webcollage: couldn't fork");
        exit (1);
      }
    case 0:
      {
        int stdout_fd = 1;

        close (in);  /* don't need this one */

        if (dup2 (out, stdout_fd) < 0)		/* pipe stdout */
          {
            perror ("could not dup() a new stdout:");
            exit (1);
          }

        execvp (av[0], av);			/* shouldn't return. */

        if (errno != ENOENT)
          {
            /* Ignore "no such file or directory" errors, unless verbose.
               Issue all other exec errors, though. */
            sprintf (buf, "webcollage: %s", av[0]);
            perror (buf);
          }

        exit (1);                              /* exits fork */
        break;
      }
    default:
      {
        st->pipe_fd = fdopen (in, "r");
        close (out);  /* don't need this one */
      }
    }

  while (ac > 0)
    free (av[--ac]);

  if (! st->pipe_fd) abort();

  st->pid = forked;
  st->pipe_id =
    XtAppAddInput (XtDisplayToApplicationContext (st->dpy), 
                   fileno (st->pipe_fd),
                   (XtPointer) (XtInputReadMask | XtInputExceptMask),
                   subproc_cb, (XtPointer) st);

  if (st->verbose_p)
    fprintf (stderr, "webcollage: subprocess pid: %d\n", st->pid);
}


static void *
webcollage_init (Display *dpy, Window window)
{
  state *st = (state *) calloc (1, sizeof(*st));
  int i;
  st->dpy = dpy;
  st->window = window;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  st->delay = 1000000;  /* check once a second */

  // Log to syslog when FPS is turned on.
  st->verbose_p = get_boolean_resource (dpy, "doFPS", "DoFPS");


  static int done_once = 0;
  if (! done_once) {
    done_once = 1;

    if (atexit (webcollage_atexit)) {	// catch calls to exit()
      perror ("webcollage: atexit");
      exit (-1);
    }

    int sigs[] = { SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT,
                   SIGFPE, SIGBUS, SIGSEGV, SIGSYS, /*SIGPIPE,*/
                   SIGALRM, SIGTERM };
    for (i = 0; i < sizeof(sigs)/sizeof(*sigs); i++) {
      if (signal (sigs[i], signal_handler)) {
        perror ("webcollage: signal");
        //exit (1);
      }
    }
  }


  open_pipe (st);

  i = 0;
  while (all_states[i]) i++;
  all_states[i] = st;

  return st;
}


static unsigned long
webcollage_draw (Display *dpy, Window window, void *closure)
{
  state *st = (state *) closure;

  if (! st->pipe_fd) 
    exit (1);

  if (! input_available_p (fileno (st->pipe_fd)))
    return st->delay;

  char buf[10240];
  int n = read (fileno (st->pipe_fd),
                (void *) buf,
                sizeof(buf) - 1);
  if (n <= 0)
    {
      XtRemoveInput (st->pipe_id);
      st->pipe_id = 0;
      // #### sometimes hangs -- pclose (st->pipe_fd);
      st->pipe_fd = 0;

      if (st->verbose_p)
        fprintf (stderr, "webcollage: subprocess has exited: bailing.\n");

      return st->delay * 10;
    }

  buf[n] = 0;
  char *s = strchr (buf, '\n');
  if (s) *s = 0;

  const char *target = "COCOA LOAD ";
  if (!strncmp (target, buf, strlen(target))) {
    const char *file = buf + strlen(target);
    display_image (dpy, window, st, file);
  }

  return st->delay;
}


static void
webcollage_reshape (Display *dpy, Window window, void *closure, 
                    unsigned int w, unsigned int h)
{
}


static Bool
webcollage_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}


static void
webcollage_atexit (void)
{
  int i = 0;

  if (all_states[0] && all_states[0]->verbose_p)
    fprintf (stderr, "webcollage: atexit handler\n");

  while (all_states[i]) {
    state *st = all_states[i];
    if (st->pid) {
      if (st->verbose_p)
        fprintf (stderr, "webcollage: kill %d\n", st->pid);
      if (kill (st->pid, SIGTERM) < 0) {
        fprintf (stderr, "webcollage: kill (%d, TERM): ", st->pid);
        perror ("webcollage: kill");
      }
      st->pid = 0;
    }
    all_states[i] = 0;
    i++;
  }
}


static void
signal_handler (int sig)
{
  if (all_states[0] && all_states[0]->verbose_p)
    fprintf (stderr, "webcollage: signal %d\n", sig);
  webcollage_atexit ();
  exit (sig);
}


/* This is important because OSX doesn't actually kill the screen savers!
   It just sends them a [ScreenSaverView stopAnimation] method.  Were
   they to actually exit, the resultant SIGPIPE should reap the children,
   but instead, we need to do it here.

   On top of that, there's an atexit() handler because otherwise the
   inferior perl script process was failing to die when SaverTester or
   System Preferences exited.  I don't pretend to understand.

   It still fails to clean up when I hit the stop button in Xcode.
   WTF.
 */
static void
webcollage_free (Display *dpy, Window window, void *closure)
{
  state *st = (state *) closure;

  if (st->verbose_p)
    fprintf (stderr, "webcollage: free cb\n");

  // Dammit dammit dammit!  Why won't this shit die when we pclose it!
//  killpg (0, SIGTERM);

  webcollage_atexit();

  if (st->pipe_id)
    XtRemoveInput (st->pipe_id);

  if (st->pipe_fd)
    fclose (st->pipe_fd);

  // Reap zombies.
# undef sleep
  sleep (1);
  int wait_status = 0;
  waitpid (-1, &wait_status, 0);

  free (st);
}


static const char *webcollage_defaults [] = {
  ".background:		black",
  ".foreground:		white",

  "*timeout:		30",
  "*delay:		2",
  "*opacity:		0.85",
  "*filter:		",
  "*filter2:		",
  0
};

static XrmOptionDescRec webcollage_options [] = {
  { "-timeout",		".timeout",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-opacity",		".opacity",	XrmoptionSepArg, 0 },
  { "-filter",		".filter",	XrmoptionSepArg, 0 },
  { "-filter2",		".filter2",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("WebCollage", webcollage)
