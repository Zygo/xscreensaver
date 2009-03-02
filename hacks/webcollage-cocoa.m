/* xscreensaver, Copyright (c) 2006 Jamie Zawinski <jwz@jwz.org>
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
display_image (state *st, const char *file)
{
  NSImage *image = [[NSImage alloc] 
                     initWithContentsOfFile:
                       [NSString stringWithCString: file
                                          encoding: kCFStringEncodingUTF8]];

  if (! image) {
    fprintf (stderr, "%s: failed to load \"%s\"\n", progname, file);
    return;
  }

  [image drawAtPoint: NSMakePoint (0, 0)
            fromRect: NSMakeRect (0, 0, [image size].width, [image size].height)
           operation: NSCompositeCopy
            fraction: 1.0];
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

  av[ac++] = "webcollage";
  av[ac++] = "-cocoa";

  av[ac++] = "-size";
  sprintf (buf, "%dx%d", st->xgwa.width, st->xgwa.height);
  av[ac++] = strdup (buf);

  av[ac++] = "-timeout"; sprintf (buf, "%d", timeout);
  av[ac++] = strdup (buf);
  av[ac++] = "-delay";   sprintf (buf, "%d", delay);
  av[ac++] = strdup (buf);
  av[ac++] = "-opacity"; sprintf (buf, "%.2f", opacity);
  av[ac++] = strdup (buf);

  if (filter && *filter) {
    av[ac++] = "-filter";
    av[ac++] = filter;
  }
  if (filter2 && *filter2) {
    av[ac++] = "-filter2";
    av[ac++] = filter2;
  }

  av[ac] = 0;


  if (st->verbose_p) {
    fprintf (stderr, "%s: launching:", progname);
    int i;
    for (i = 0; i < ac; i++)
      fprintf (stderr, " %s", av[i]);
    fprintf (stderr, "\n");
  }


  if (pipe (fds))
    {
      perror ("error creating pipe:");
      exit (1);
    }

  in = fds [0];
  out = fds [1];

  switch ((int) (forked = fork ()))
    {
    case -1:
      {
        sprintf (buf, "%s: couldn't fork", progname);
        perror (buf);
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
            sprintf (buf, "%s: %s", progname, av[0]);
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

  if (! st->pipe_fd) abort();

  st->pid = forked;
  st->pipe_id =
    XtAppAddInput (XtDisplayToApplicationContext (st->dpy), 
                   fileno (st->pipe_fd),
                   (XtPointer) (XtInputReadMask | XtInputExceptMask),
                   subproc_cb, (XtPointer) st);

  if (st->verbose_p)
    fprintf (stderr, "%s: subprocess pid: %d\n", progname, st->pid);
}


static void *
webcollage_init (Display *dpy, Window window)
{
  state *st = (state *) calloc (1, sizeof(*st));
  st->dpy = dpy;
  st->window = window;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  st->delay = 1000000;  /* check once a second */

  open_pipe (st);

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
        fprintf (stderr, "%s: subprocess has exited: bailing.\n", progname);

      return st->delay * 10;
    }

  buf[n] = 0;
  char *s = strchr (buf, '\n');
  if (s) *s = 0;

  const char *target = "COCOA LOAD ";
  if (!strncmp (target, buf, strlen(target))) {
    const char *file = buf + strlen(target);
    display_image (st, file);
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
webcollage_free (Display *dpy, Window window, void *closure)
{
  state *st = (state *) closure;

  // Dammit dammit dammit!  Why won't this shit die when we pclose it!
//  killpg (0, SIGTERM);

  if (st->pid) {
    if (st->verbose_p)
      fprintf (stderr, "%s: kill %d\n", progname, st->pid);
    if (kill (st->pid, SIGTERM) < 0) {
      fprintf (stderr, "%s: kill (%d, TERM): ", progname, st->pid);
      perror ("kill");
    }
    st->pid = 0;
  }

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
