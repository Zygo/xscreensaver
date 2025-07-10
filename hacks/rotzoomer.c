/* rotzoomer - creates a collage of rotated and scaled portions of the screen
 * Copyright (C) 2001-2016 Claudio Matsuoka <claudio@helllabs.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

/* Circle-mode by jwz, 2014, 2016. */

/*
 * Options:
 *
 * -shm		enable MIT shared memory extension
 * -no-shm	disable MIT shared memory extension
 * -n <num>	number of zoomboxes
 * -move	enable mobile zoomboxes
 * -sweep	enable sweep mode
 * -circle	enable circle mode
 * -anim	enable snapshot mode
 * -no-anim	enable snapshot mode
 * -delay	delay in milliseconds
 */

#include <math.h>
#include "screenhack.h"
#include "xshm.h"

struct zoom_area {
  int w, h;		/* rectangle width and height */
  int inc1, inc2;	/* rotation and zoom angle increments */
  int dx, dy;		/* translation increments */
  int a1, a2;		/* rotation and zoom angular variables */
  int ox, oy;		/* origin in the background copy */
  int xx, yy;		/* left-upper corner position (* 256) */
  int x, y;		/* left-upper corner position */
  int ww, hh;		/* valid area to place left-upper corner */
  int n;		/* number of iteractions */
  int count;		/* current iteraction */
};

struct state {
  Display *dpy;
  Window window;

  GC gc;
  Visual *visual;
  XImage *orig_map, *buffer_map;
  Colormap colormap;

  int width, height;
  struct zoom_area **zoom_box;
  int num_zoom;
  int move;
  int sweep;
  int circle;
  int delay;
  int anim;
  int duration;
  time_t start_time;

  async_load_state *img_loader;
  Pixmap pm;

  XShmSegmentInfo shm_info;
};


static void
rotzoom (struct state *st, struct zoom_area *za)
{
  int x, y, c, s, zoom, z;
  int x2 = za->x + za->w - 1, y2 = za->y + za->h - 1;
  int ox = 0, oy = 0;
  int w2 = (za->w/2) * (za->w/2);

  z = 8100 * sin (M_PI * za->a2 / 8192);
  zoom = 8192 + z;

  for (y = za->y; y <= y2; y++) {
    for (x = za->x; x <= x2; x++) {
      Bool copyp = True;
      double a = M_PI * za->a1 / 8192;
      c = zoom * cos (a);
      s = zoom * sin (a);
      if (st->circle) {
        int cx = za->x + za->w / 2;
        int cy = za->y + za->h / 2;
        int dx = x - cx;
        int dy = y - cy;
        int d2 = (dx*dx) + (dy*dy);

        if (d2 > w2) {
          copyp = False;
        } else {
          double r = sqrt ((double) d2);
          double th = atan ((double)dy / (double) (dx == 0 ? 1 : dx));
          copyp = 1;
          if (dx < 0) th += M_PI;
          th += M_PI * (za->a1 / 600.0);
          ox = cx + (int) (r * cos(th));
          oy = cy + (int) (r * sin(th));
        }
      } else {
        ox = (x * c + y * s) >> 13;
        oy = (-x * s + y * c) >> 13;
      }

      if (copyp) {
        while (ox < 0)
          ox += st->width;
        while (oy < 0)
          oy += st->height;
        while (ox >= st->width)
          ox -= st->width;
        while (oy >= st->height)
          oy -= st->height;

        XPutPixel (st->buffer_map, x, y, XGetPixel (st->orig_map, ox, oy));
      }
    }
  }

  za->a1 += za->inc1;		/* Rotation angle */
  za->a1 &= 0x3fff;

  za->a2 += za->inc2;		/* Zoom */
  za->a2 &= 0x3fff;

  za->ox = ox;			/* Save state for next iteration */
  za->oy = oy;

  if (st->circle && za->n <= 1)
    {
      /* Done rotating the circle: copy the bits from the working set back
         into the origin, so that subsequent rotations pick up these changes.
       */
      int cx = za->x + za->w / 2;
      int cy = za->y + za->h / 2;
      int w2 = (za->w/2) * (za->w/2);
      for (y = za->y; y < za->y + za->h; y++)
        for (x = za->x; x < za->x + za->w; x++)
          {
            int dx = x - cx;
            int dy = y - cy;
            int d2 = (dx*dx) + (dy*dy);
            if (d2 <= w2)
              XPutPixel (st->orig_map, x, y, XGetPixel (st->buffer_map, x, y));
          }
    }

  za->count++;
}


static void
reset_zoom (struct state *st, struct zoom_area *za)
{
  if (st->sweep) {
    int speed = random () % 100 + 100;
    switch (random () % 4) {
    case 0:
      za->w = st->width;
      za->h = 10;
      za->x = 0;
      za->y = 0;
      za->dx = 0;
      za->dy = speed;
      za->n = (st->height - 10) * 256 / speed;
      break;
    case 1:
      za->w = 10;
      za->h = st->height;
      za->x = st->width - 10;
      za->y = 0;
      za->dx = -speed;
      za->dy = 0;
      za->n = (st->width - 10) * 256 / speed;
      break;
    case 2:
      za->w = st->width;
      za->h = 10;
      za->x = 0;
      za->y = st->height - 10;
      za->dx = 0;
      za->dy = -speed;
      za->n = (st->height - 10) * 256 / speed;
      break;
    case 3:
      za->w = 10;
      za->h = st->height;
      za->x = 0;
      za->y = 0;
      za->dx = speed;
      za->dy = 0;
      za->n = (st->width - 10) * 256 / speed;
      break;
    }
    za->ww = st->width - za->w;
    za->hh = st->height - za->h;

    /* We want smaller angle increments in sweep mode (looks better) */

    za->a1 = 0;
    za->a2 = 0;
    za->inc1 = ((2 * (random() & 1)) - 1) * (1 + random () % 7);
    za->inc2 = ((2 * (random() & 1)) - 1) * (1 + random () % 7);
  } else if (st->circle) {

    za->w = 50 + random() % 300;
    if (za->w > st->width / 3)
      za->w = st->width / 3;
    if (za->w > st->height / 3)
      za->w = st->height / 3;
    za->h = za->w;

    za->ww = st->width  - za->w;
    za->hh = st->height - za->h;

    za->x = (za->ww ? random() % za->ww : 0);
    za->y = (za->hh ? random() % za->hh : 0);
    za->dx = 0;
    za->dy = 0;
    za->a1 = 0;
    za->a2 = 0;
    za->count = 0;

    /* #### If we go clockwise, it doesn't start rotating from 0.
       So only go counter-clockwise for now. Sigh. */
    za->inc1 = (random () % 30);
    za->inc2 = 0;
    za->n = 50 + random() % 100;

    if (!st->anim) {
      za->count = random() % (za->n / 2);
      za->a1 = random();
    }

  } else {
    za->w = 50 + random() % 300;
    za->h = 50 + random() % 300;

    if (za->w > st->width / 3)
      za->w = st->width / 3;
    if (za->h > st->height / 3)
      za->h = st->height / 3;

    za->ww = st->width - za->w;
    za->hh = st->height - za->h;

    za->x = (za->ww ? random() % za->ww : 0);
    za->y = (za->hh ? random() % za->hh : 0);

    za->dx = ((2 * (random() & 1)) - 1) * (100 + random() % 300);
    za->dy = ((2 * (random() & 1)) - 1) * (100 + random() % 300);

    if (st->anim) {
      za->n = 50 + random() % 1000;
      za->a1 = 0;
      za->a2 = 0;
    } else {
      za->n = 5 + random() % 10;
      za->a1 = random ();
      za->a2 = random ();
    }

    za->inc1 = ((2 * (random() & 1)) - 1) * (random () % 30);
    za->inc2 = ((2 * (random() & 1)) - 1) * (random () % 30);
  }

  za->xx = za->x * 256;
  za->yy = za->y * 256;

  za->count = 0;
}


static struct zoom_area *
create_zoom (struct state *st)
{
  struct zoom_area *za;

  za = calloc (1, sizeof (struct zoom_area));
  reset_zoom (st, za);

  return za;
}


static void
update_position (struct zoom_area *za)
{
  za->xx += za->dx;
  za->yy += za->dy;

  za->x = za->xx >> 8;
  za->y = za->yy >> 8;

  if (za->x < 0) {
    za->x = 0;
    za->dx = 100 + random() % 100;
  }
		
  if (za->y < 0) {
    za->y = 0;
    za->dy = 100 + random() % 100;
  }
		
  if (za->x > za->ww) {
    za->x = za->ww;
    za->dx = -(100 + random() % 100);
  }

  if (za->y > za->hh) {
    za->y = za->hh;
    za->dy = -(100 + random() % 100);
  }
}


static void
DisplayImage (struct state *st, int x, int y, int w, int h)
{
  put_xshm_image (st->dpy, st->window, st->gc, st->buffer_map, x, y, x, y,
                  w, h, &st->shm_info);
}


static void
set_mode(struct state *st)
{
  char *s = get_string_resource (st->dpy, "mode", "Mode");
  if (!s || !*s || !strcasecmp (s, "random"))
    {
      free (s);
      switch (random() % 4) {
      case 0: s = "stationary"; break;
      case 1: s = "move"; break;
      case 2: s = "sweep"; break;
      case 3: s = "circle"; break;
      default: abort();
      }
    }

  st->move = False;
  st->sweep = False;
  st->circle = False;

  if (!strcasecmp (s, "stationary"))
    ;
  else if (!strcasecmp (s, "move"))
    st->move = True;
  else if (!strcasecmp (s, "sweep"))
    st->sweep = True;
  else if (!strcasecmp (s, "circle"))
    st->circle = True;
  else
    fprintf (stderr, "%s: bogus mode: \"%s\"\n", progname, s);
}


static void
init_hack (struct state *st)
{
  int i;

  set_mode (st);

  st->start_time = time ((time_t *) 0);

  if (st->zoom_box) {
    for (i = 0; i < st->num_zoom; i++)
      if (st->zoom_box[i]) free (st->zoom_box[i]);
    free (st->zoom_box);
  }
  st->zoom_box = calloc (st->num_zoom, sizeof (struct zoom_area *));
  for (i = 0; i < st->num_zoom; i++) {
    if (st->zoom_box[i]) free (st->zoom_box[i]);
    st->zoom_box[i] = create_zoom (st);
  }

  if (st->height && st->orig_map->data)
    memcpy (st->buffer_map->data, st->orig_map->data,
	    st->height * st->buffer_map->bytes_per_line);

  DisplayImage(st, 0, 0, st->width, st->height);
}


static unsigned long
rotzoomer_draw (Display *disp, Window win, void *closure)
{
  struct state *st = (struct state *) closure;
  int delay = st->delay;
  int i;

  if (st->img_loader)   /* still loading */
    {
      st->img_loader = load_image_async_simple (st->img_loader, 0, 0, 0, 0, 0);
      if (! st->img_loader) {  /* just finished */
        if (! st->pm) abort();
        if (st->orig_map) XDestroyImage (st->orig_map);
	st->orig_map = XGetImage (st->dpy, st->pm,
                                  0, 0, st->width, st->height,
                                  ~0L, ZPixmap);
        init_hack (st);
      }
      return st->delay;
    }

  if (!st->img_loader &&
      st->start_time + st->duration < time ((time_t *) 0)) {
    XWindowAttributes xgwa;
    XGetWindowAttributes(st->dpy, st->window, &xgwa);
    /* On MacOS X11, XGetImage on a Window often gets an inexplicable BadMatch,
       possibly due to the window manager having occluded something?  It seems
       nondeterministic. Loading the image into a pixmap instead fixes it. */
    if (st->pm) XFreePixmap (st->dpy, st->pm);
    st->pm = XCreatePixmap (st->dpy, st->window,
                            xgwa.width, xgwa.height, xgwa.depth);
    st->img_loader = load_image_async_simple (0, xgwa.screen, st->window,
                                              st->pm, 0, 0);
    st->start_time = time ((time_t *) 0);
    return st->delay;
  }

  for (i = 0; i < st->num_zoom; i++) {
    if (st->move || st->sweep)
      update_position (st->zoom_box[i]);

    if (st->zoom_box[i]->n > 0) {
      if (st->anim || st->zoom_box[i]->count == 0) {
        rotzoom (st, st->zoom_box[i]);
      } else {
        delay = 1000000;
      }
      st->zoom_box[i]->n--;
    } else {
      reset_zoom (st, st->zoom_box[i]);
    }
  }

  for (i = 0; i < st->num_zoom; i++) {
    DisplayImage(st, st->zoom_box[i]->x, st->zoom_box[i]->y,
                 st->zoom_box[i]->w, st->zoom_box[i]->h);
  }

  return delay;
}


static void
setup_X (struct state *st)
{
  XWindowAttributes xgwa;
  int depth;
  XGCValues gcv;
  long gcflags;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  depth = xgwa.depth;
  st->colormap = xgwa.colormap;
  st->width = xgwa.width;
  st->height = xgwa.height;
  st->visual = xgwa.visual;

  if (st->width % 2)
    st->width--;
  if (st->height % 2)
    st->height--;

  gcv.function = GXcopy;
  gcflags = GCFunction;
  st->gc = XCreateGC (st->dpy, st->window, gcflags, &gcv);
  if (st->pm) XFreePixmap (st->dpy, st->pm);
  st->pm = XCreatePixmap (st->dpy, st->window,
                          xgwa.width, xgwa.height, xgwa.depth);
  st->img_loader = load_image_async_simple (0, xgwa.screen, st->window,
                                            st->pm, 0, 0);

  st->buffer_map = create_xshm_image(st->dpy, xgwa.visual, depth,
                                     ZPixmap, &st->shm_info, st->width, st->height);
}


static void *
rotzoomer_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = dpy;
  st->window = window;
  st->num_zoom = get_integer_resource (st->dpy, "numboxes", "Integer");

  set_mode(st);

  st->anim = get_boolean_resource (st->dpy, "anim", "Boolean");
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->duration = get_integer_resource (st->dpy, "duration", "Seconds");
  if (st->delay < 0) st->delay = 0;
  if (st->duration < 1) st->duration = 1;

  /* In sweep or static mode, we want only one box */
  if (st->sweep || !st->anim)
    st->num_zoom = 1;

  /* Can't have static sweep mode */
  if (!st->anim)
    st->sweep = 0;

  if (st->circle) {
    st->move = 0;
    st->sweep = 0;
  }

  st->start_time = time ((time_t *) 0);

  setup_X (st);

  return st;
}

static void
rotzoomer_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
rotzoomer_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->start_time = 0;
      return True;
    }
  return False;
}

static void
rotzoomer_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  if (st->pm) XFreePixmap (dpy, st->pm);
  if (st->gc) XFreeGC (dpy, st->gc);
  if (st->orig_map) XDestroyImage (st->orig_map);
  if (st->buffer_map) destroy_xshm_image (dpy, st->buffer_map, &st->shm_info);
  if (st->zoom_box) {
    int i;
    for (i = 0; i < st->num_zoom; i++)
      if (st->zoom_box[i]) free (st->zoom_box[i]);
    free (st->zoom_box);
  }
  free (st);
}


static const char *rotzoomer_defaults[] = {
  ".background: black",
  ".foreground: white",
  "*fpsSolid:	true",
#ifdef HAVE_XSHM_EXTENSION
  "*useSHM: True",
#else
  "*useSHM: False",
#endif
  "*anim: True",
  "*mode: random",
  "*numboxes: 2",
  "*delay: 10000",
  "*duration: 120",
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
  "*rotateImages:   True",
#endif
  0
};


static XrmOptionDescRec rotzoomer_options[] = {
  { "-shm",	".useSHM",	XrmoptionNoArg, "True"  },
  { "-no-shm",	".useSHM",	XrmoptionNoArg, "False" },
  { "-mode",	".mode",	XrmoptionSepArg, 0      },
  { "-move",	".mode",	XrmoptionNoArg, "move"  },
  { "-sweep",	".mode",	XrmoptionNoArg, "sweep" },
  { "-circle",	".mode",	XrmoptionNoArg, "circle"},
  { "-anim",	".anim",	XrmoptionNoArg, "True"  },
  { "-no-anim",	".anim",	XrmoptionNoArg, "False" },
  { "-delay",	".delay",	XrmoptionSepArg, 0      },
  {"-duration",	".duration",	XrmoptionSepArg, 0      },
  { "-n",	".numboxes",	XrmoptionSepArg, 0      },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("RotZoomer", rotzoomer)
