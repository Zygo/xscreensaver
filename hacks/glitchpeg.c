/* glitchpeg, Copyright (c) 2018 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Insert errors into an image file, then display the corrupted result.
 *
 * This only works on X11 and MacOS because iOS and Android don't have
 * access to the source files of images, only the decoded image data.
 */

#include "screenhack.h"
#include "ximage-loader.h"

#ifndef HAVE_JWXYZ
# include <X11/Intrinsic.h>   /* for XtInputId, etc */
#endif

#include <sys/stat.h>

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

struct state {
  Display *dpy;
  Window window;
  GC gc;
  XWindowAttributes xgwa;
  int delay;
  int count;
  int duration;
  time_t start_time;
  unsigned char *image_data; unsigned long image_size;
  XtInputId pipe_id;
  FILE *pipe;
  Bool button_down_p;
};


static Bool
bigendian (void)
{
  union { int i; char c[sizeof(int)]; } u;
  u.i = 1;
  return !u.c[0];
}


/* Given a bitmask, returns the position and width of the field.
   Duplicated from ximage-loader.c.
 */
static void
decode_mask (unsigned long mask, unsigned long *pos_ret,
             unsigned long *size_ret)
{
  int i;
  for (i = 0; i < 32; i++)
    if (mask & (1L << i))
      {
        int j = 0;
        *pos_ret = i;
        for (; i < 32; i++, j++)
          if (! (mask & (1L << i)))
            break;
        *size_ret = j;
        return;
      }
}


/* Renders a scaled, cropped version of the RGBA XImage onto the window.
 */
static void
draw_image (Display *dpy, Window window, Visual *v, GC gc, 
            int w, int h, int depth, XImage *in)
{
  XImage *out;
  int x, y, w2, h2, xoff, yoff;
  double xs, ys, s;

  unsigned long crpos=0, cgpos=0, cbpos=0, capos=0; /* bitfield positions */
  unsigned long srpos=0, sgpos=0, sbpos=0;
  unsigned long srmsk=0, sgmsk=0, sbmsk=0;
  unsigned long srsiz=0, sgsiz=0, sbsiz=0;

# ifdef HAVE_JWXYZ
  // BlackPixel has alpha: 0xFF000000.
  unsigned long black = BlackPixelOfScreen (DefaultScreenOfDisplay (dpy));
#else
  unsigned long black = 0;
# endif

  xs = in->width  / (double) w;
  ys = in->height / (double) h;
  s = (xs > ys ? ys : xs);
  w2 = in->width  / s;
  h2 = in->height / s;
  xoff = (w - w2) / 2;
  yoff = (h - h2) / 2;

  /* Create a new image in the depth and bit-order of the server. */
  out = XCreateImage (dpy, v, depth, ZPixmap, 0, 0, w, h, 8, 0);
  out->bitmap_bit_order = in->bitmap_bit_order;
  out->byte_order = in->byte_order;

  out->bitmap_bit_order = BitmapBitOrder (dpy);
  out->byte_order = ImageByteOrder (dpy);

  out->data = (char *) malloc (out->height * out->bytes_per_line);
  if (!out->data) abort();

  /* Find the server's color masks.
     We could cache this and just do it once, but it's a small number
     of instructions compared to the per-pixel operations happening next.
   */
  srmsk = out->red_mask;
  sgmsk = out->green_mask;
  sbmsk = out->blue_mask;

  if (!(srmsk && sgmsk && sbmsk)) abort();  /* No server color masks? */

  decode_mask (srmsk, &srpos, &srsiz);
  decode_mask (sgmsk, &sgpos, &sgsiz);
  decode_mask (sbmsk, &sbpos, &sbsiz);

  /* 'in' is RGBA in client endianness.  Convert to what the server wants. */
  if (bigendian())
    crpos = 24, cgpos = 16, cbpos =  8, capos =  0;
  else
    crpos =  0, cgpos =  8, cbpos = 16, capos = 24;

  /* Iterate the destination rectangle and pull in the corresponding
     scaled and cropped source pixel, or black. Nearest-neighbor is fine.
   */
  for (y = 0; y < out->height; y++)
    {
      int iy = (out->height - y - yoff - 1) * s;
      for (x = 0; x < out->width; x++)
        {
          int ix = (x - xoff) * s;
          unsigned long p = (ix >= 0 && ix < in->width &&
                             iy >= 0 && iy < in->height
                             ? XGetPixel (in, ix, iy)
                             : black);
       /* unsigned char a = (p >> capos) & 0xFF; */
          unsigned char b = (p >> cbpos) & 0xFF;
          unsigned char g = (p >> cgpos) & 0xFF;
          unsigned char r = (p >> crpos) & 0xFF;
          XPutPixel (out, x, y, ((r << srpos) |
                                 (g << sgpos) |
                                 (b << sbpos) |
                                 black));
        }
    }

  XPutImage (dpy, window, gc, out, 0, 0, 0, 0, out->width, out->height);
  XDestroyImage (out);
}


# define BACKSLASH(c) \
  (! ((c >= 'a' && c <= 'z') || \
      (c >= 'A' && c <= 'Z') || \
      (c >= '0' && c <= '9') || \
      c == '.' || c == '_' || c == '-' || c == '+' || c == '/'))

/* Gets the name of an image file to load by running xscreensaver-getimage-file
   at the end of a pipe.  This can be very slow!
   Duplicated from utils/grabclient.c
 */
static FILE *
open_image_name_pipe (void)
{
  char *s;

  /* /bin/sh on OS X 10.10 wipes out the PATH. */
  const char *path = getenv("PATH");
  char *cmd = s = malloc (strlen(path) * 2 + 100);
  strcpy (s, "/bin/sh -c 'export PATH=");
  s += strlen (s);
  while (*path) {
    char c = *path++;
    if (BACKSLASH(c)) *s++ = '\\';
    *s++ = c;
  }
  strcpy (s, "; ");
  s += strlen (s);

  strcpy (s, "xscreensaver-getimage-file --name --absolute");
  s += strlen (s);

  strcpy (s, "'");
  s += strlen (s);

  *s = 0;

  FILE *pipe = popen (cmd, "r");
  free (cmd);
  return pipe;
}


/* Duplicated from utils/grabclient.c */
static void
xscreensaver_getimage_file_cb (XtPointer closure, int *source, XtInputId *id)
{
  /* This is not called from a signal handler, so doing stuff here is fine.
   */
  struct state *st = (struct state *) closure;
  char buf[10240];
  char *file = buf;
  FILE *fp;
  struct stat stat;
  int n;
  unsigned char *s;
  int L;

  *buf = 0;
  fgets (buf, sizeof(buf)-1, st->pipe);
  pclose (st->pipe);
  st->pipe = 0;
  XtRemoveInput (st->pipe_id);
  st->pipe_id = 0;

  /* strip trailing newline */
  L = strlen(buf);
  while (L > 0 && (buf[L-1] == '\r' || buf[L-1] == '\n'))
    buf[--L] = 0;

  fp = fopen (file, "r");
  if (! fp)
    {
      fprintf (stderr, "%s: unable to read %s\n", progname, file);
      return;
    }

  if (fstat (fileno (fp), &stat))
    {
      fprintf (stderr, "%s: %s: stat failed\n", progname, file);
      return;
    }

  if (st->image_data) free (st->image_data);
  st->image_size = stat.st_size;
  st->image_data = malloc (st->image_size);
  
  s = st->image_data;
  do {
    n = fread (s, 1, st->image_data + st->image_size - s, fp);
    if (n > 0) s += n;
  } while (n > 0);

  fclose (fp);

  /* fprintf (stderr, "loaded %s (%lu)\n", file, st->image_size); */

  st->start_time = time((time_t *) 0);
}


static void *
glitchpeg_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;

  st->dpy = dpy;
  st->window = window;
  st->gc = XCreateGC (dpy, window, 0, &gcv);

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  if (st->delay < 1) st->delay = 1;

  st->duration = get_integer_resource (st->dpy, "duration", "Integer");
  if (st->duration < 0) st->duration = 0;

  st->count = get_integer_resource (st->dpy, "count", "Integer");
  if (st->count < 1) st->count = 1;

  XClearWindow (st->dpy, st->window);

  return st;
}


static unsigned long
glitchpeg_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if ((!st->image_data ||
       time((time_t *) 0) >= st->start_time + st->duration) &&
      !st->pipe)
    {
      /* Time to reload */
      st->pipe = open_image_name_pipe();
      st->pipe_id =
        XtAppAddInput (XtDisplayToApplicationContext (dpy), 
                       fileno (st->pipe),
                       (XtPointer) (XtInputReadMask | XtInputExceptMask),
                       xscreensaver_getimage_file_cb, (XtPointer) st);
    }

  if (st->image_data && !st->button_down_p)
    {
      int n;
      XImage *image;
      unsigned char *glitched = malloc (st->image_size);
      int nn = random() % st->count;
      if (nn <= 0) nn = 1;

      memcpy (glitched, st->image_data, st->image_size);

      for (n = 0; n < nn; n++)
        {
          int start = 255;
          int end = st->image_size - 255;
          int size = end - start;
          Bool byte_p = True;  /* random() % 100; */
          if (size <= 100) break;
          if (byte_p)
            {
              int i = start + (random() % size);
              if (!(random() % 10))
                /* Take one random byte and randomize it. */
                glitched[i] = random() % 0xFF; 
              else
                /* Take one random byte and add 5% to it. */
                glitched[i] += 
                  (1 + (random() % 0x0C)) * ((random() & 1) ? 1 : -1);
            }
          else
            {
              /* Take two randomly-sized chunks of the file and swap them.
                 This tends to just destroy the image.  Doesn't look good. */
              int s2 = 2 + size * 0.05;
              char *swap = malloc (s2);
              int start1 = start + (random() % (size - s2));
              int start2 = start + (random() % (size - s2));
              memcpy (glitched + start1, swap, s2);
              memmove (glitched + start2, glitched + start1, s2);
              memcpy (swap, glitched + start2, s2);
              free (swap);
            }
        }

      image = image_data_to_ximage (dpy, st->xgwa.visual,
                                    glitched, st->image_size);
      free (glitched);

      if (image)  /* Might be null if decode fails */
        {
          draw_image (dpy, window, st->xgwa.visual, st->gc,
                      st->xgwa.width, st->xgwa.height, st->xgwa.depth,
                      image);
          XDestroyImage (image);
        }
    }

  return st->delay;
}


static void
glitchpeg_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
}


static Bool
glitchpeg_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (event->xany.type == ButtonPress)
    {
      st->button_down_p = True;
      return True;
    }
  else if (event->xany.type == ButtonRelease)
    {
      st->button_down_p = False;
      return True;
    }
  else if (screenhack_event_helper (dpy, window, event))
    {
      st->start_time = 0;  /* reload */
      return True;
    }

  return False;
}


static void
glitchpeg_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (dpy, st->gc);
  if (st->pipe_id) XtRemoveInput (st->pipe_id);
  if (st->pipe) fclose (st->pipe);
  if (st->image_data) free (st->image_data);
  free (st);
}


static const char *glitchpeg_defaults [] = {
  ".background:			black",
  ".foreground:			white",
  ".lowrez:                     True",
  "*fpsSolid:			true",
  "*delay:			30000",
  "*duration:			120",
  "*count:			100",
  "*grabDesktopImages:		False",   /* HAVE_JWXYZ */
  "*chooseRandomImages:		True",    /* HAVE_JWXYZ */
#ifdef HAVE_MOBILE
  "*ignoreRotation:             True",
  "*rotateImages:               True",
#endif
  0
};

static XrmOptionDescRec glitchpeg_options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-duration",	".duration",		XrmoptionSepArg, 0 },
  { "-count",		".count",		XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("GlitchPEG", glitchpeg)
