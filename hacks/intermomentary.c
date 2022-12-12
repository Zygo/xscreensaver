/*
 *  InterMomentary (dragorn@kismetwireless.net)
 *  Directly ported code from complexification.net InterMomentary art
 *  http://www.complexification.net/gallery/machines/interMomentary/applet_l/interMomentary_l.pde
 *
 * Intersecting Circles, Instantaneous
 * J. Tarbell                              + complexification.net
 * Albuquerque, New Mexico
 * May, 2004
 * 
 * a REAS collaboration for the            + groupc.net
 * Whitney Museum of American Art ARTPORT  + artport.whitney.org
 * Robert Hodgin                           + flight404.com
 * William Ngan                            + metaphorical.net
 * 
 *
 * 1.0  Oct 10 2004  dragorn  Completed first port 
 *
 *
 * Based, of course, on other hacks in:
 *
 * xscreensaver, Copyright (c) 1997, 1998, 2002 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <math.h>
#include "screenhack.h"
#include "hsv.h"

/* this program goes faster if some functions are inline.  The following is
 * borrowed from ifs.c */
#if !defined( __GNUC__ ) && !defined(__cplusplus) && !defined(c_plusplus)
#undef inline
#define inline			/* */
#endif

/* Pixel rider */
typedef struct {
    float t;
    float vt;
    float mycharge;
} PxRider;

/* disc of light */
typedef struct {
    /* index identifier */
    int id;
    /* position */
    float x, y;
    /* radius */
    float r, dr;
    /* velocity */
    float vx, vy;

    /* pixel riders */
    int numr;
    PxRider *pxRiders;
} Disc;

struct field {
    unsigned int height;
    unsigned int width;

    int initial_discs;
    Disc *discs;
    
    unsigned int num;

    unsigned int maxrider;
    unsigned int maxradius;

    /* color parms */
    unsigned long fgcolor;
    unsigned long bgcolor;
    int visdepth;

    unsigned int cycles;

    /* Offscreen image we draw to */
    Pixmap off_map;
    unsigned char *off_alpha;
};


struct state {
  Display *dpy;
  Window window;

  struct field *f;
  GC fgc, copygc;
  XWindowAttributes xgwa;
  int draw_delay;

  XColor *colors;
  int ncolors;
  int pscale;
};


static void *xrealloc(void *p, size_t size) 
{
    void *ret;
    if ((ret = realloc(p, size)) == NULL) {
	fprintf(stderr, "%s: out of memory\n", progname);
	exit(1);
    }
    return ret;
}

static struct field *init_field(void) 
{
    struct field *f = xrealloc(NULL, sizeof(struct field));
    f->height = 0;
    f->width = 0;
    f->initial_discs = 0;
    f->discs = NULL;
    f->num = 0;
    f->maxrider = 0;
    f->maxradius = 0;
    f->cycles = 0;
    f->fgcolor = 0;
    f->bgcolor = 0;
    f->off_alpha = NULL;
    f->visdepth = 0;
    return f;
}

/* Quick-ref to pixels in the alpha map */
#define ref_pixel(f, x, y)   ((f)->off_alpha[(y) * (f)->width + (x)])

static inline void make_disc(struct field *f, float x, float y, float vx, float vy, float r) 
{
    /* Synthesis of Disc::Disc and PxRider::PxRider */
    Disc *nd;
    int ix;

    /* allocate a new disc */
    f->discs = (Disc *) xrealloc(f->discs, sizeof(Disc) * (f->num + 1));

    nd = &(f->discs[f->num]);

    nd->id = f->num++;
    nd->x = x;
    nd->y = y;
    nd->vx = vx;
    nd->vy = vy;
    nd->dr = r;
    nd->r = frand(r) / 3;

    nd->numr = (frand(r) / 2.62);
    if (nd->numr > f->maxrider)
        nd->numr = f->maxrider;

    nd->pxRiders = NULL;
    nd->pxRiders = (PxRider *) xrealloc(nd->pxRiders, sizeof(PxRider) * (f->maxrider));
    for (ix = 0; ix < f->maxrider; ix++) {
        nd->pxRiders[ix].vt = 0.0;
        nd->pxRiders[ix].t = frand(M_PI * 2);
        nd->pxRiders[ix].mycharge = 0;
    }
}


/* alpha blended point drawing */
static inline unsigned long
trans_point(struct state *st,
            int x1, int y1, unsigned char myc, float a, struct field *f) 
{
    if ((x1 >= 0) && (x1 < f->width) && (y1 >= 0) && (y1 < f->height)) {
        if (a >= 1.0) {
            ref_pixel(f, x1, y1) = myc;
        } else {
            unsigned long c = ref_pixel(f, x1, y1);
            c = c + (myc - c) * a;
            ref_pixel(f, x1, y1) = c;
            return c;
        }
    }

    return 0;
}

static inline unsigned long
get_pixel (struct state *st, unsigned char v)
{
  return st->colors [v * (st->ncolors-1) / 255].pixel;
}


static inline void move_disc(struct field *f, int dnum) 
{
    Disc *d = &(f->discs[dnum]);

    /* add velocity to position */
    d->x += d->vx;
    d->y += d->vy;

    /* bound check */
    if (d->x + d->r < 0)
        d->x += f->width + d->r + d->r;
    if (d->x - d->r > f->width)
        d->x -= f->width + d->r + d->r;
    if (d->y + d->r < 0)
        d->y += f->height + d->r + d->r;
    if (d->y - d->r > f->height)
        d->y -= f->height + d->r + d->r;

    /* increase to destination radius */
    if (d->r < d->dr)
        d->r += 0.1;
}

static inline void 
draw_glowpoint(struct state *st, Drawable drawable, 
               GC fgc, struct field *f, float px, float py) 
{
    int i, j;
    float a;
    unsigned long c;

    for (i = -2; i < 3; i++) {
        for (j = -2; j < 3; j++) {
            a = 0.8 - i * i * 0.1 - j * j * 0.1;

            c = trans_point(st, px+i, py+j, 255, a, f);
            XSetForeground(st->dpy, fgc, get_pixel (st, c));
            XFillRectangle(st->dpy, drawable, fgc, px + i, py + j,
                           st->pscale, st->pscale);
            XSetForeground(st->dpy, fgc, f->fgcolor);
        }
    }
}

static inline void 
moverender_rider(struct state *st, Drawable drawable, 
                 GC fgc, struct field *f, PxRider *rid, 
                 float x, float y, float r) 
{
    float px, py;
    unsigned long int c;
    double cv;

    /* add velocity to theta */
    rid->t = fmod((rid->t + rid->vt + M_PI), (2 * M_PI)) - M_PI;
    
    rid->vt += frand(0.002) - 0.001;

    /* apply friction brakes */
    if (fabsf(rid->vt) > 0.02)
        rid->vt *= 0.9;

    /* draw */
    px = x + r * cos(rid->t);
    py = y + r * sin(rid->t);

    if ((px < 0) || (px >= f->width) || (py < 0) || (py >= f->height))
        return;

    /* max brightness seems to be 0.003845 */

    c = ref_pixel(f, (int) px, (int) py);
    cv = c / 255.0;

    /* guestimated - 40 is 18% of 255, so scale this to 0.0 to 0.003845 */
    if (cv > 0.0006921) {
        draw_glowpoint(st, drawable, fgc, f, px, py); 

        rid->mycharge = 0.003845;
    } else {
        rid->mycharge *= 0.98;

        c = 255 * rid->mycharge;

        trans_point(st, px, py, c, 0.5, f);

        XSetForeground(st->dpy, fgc, get_pixel(st, c));
        XFillRectangle(st->dpy, drawable, fgc, px, py,
                       st->pscale, st->pscale);
        XSetForeground(st->dpy, fgc, f->fgcolor);
    }
}

static inline void 
render_disc(struct state *st, Drawable drawable, GC fgc, struct field *f, int dnum) 
{
    Disc *di = &(f->discs[dnum]);
    int n, m;
    float dx, dy, d;
    float a, p2x, p2y, h, p3ax, p3ay, p3bx, p3by;
    unsigned long c;

    /* Find intersecting points with all ascending discs */
    for (n = di->id + 1; n < f->num; n++) {
        dx = f->discs[n].x - di->x;
        dy = f->discs[n].y - di->y;
        d = sqrt(dx * dx + dy * dy);

        /* intersection test */
        if (d < (f->discs[n].r + di->r)) {
            /* complete containment test */
            if (d > fabsf(f->discs[n].r - di->r)) {
                /* find solutions */
                a = (di->r * di->r - f->discs[n].r * f->discs[n].r + d * d) / (2 * d);
                p2x = di->x + a * (f->discs[n].x - di->x) / d;
                p2y = di->y + a * (f->discs[n].y - di->y) / d;

                h = sqrt(di->r * di->r - a * a);

                p3ax = p2x + h * (f->discs[n].y - di->y) / d;
                p3ay = p2y - h * (f->discs[n].x - di->x) / d;

                p3bx = p2x - h * (f->discs[n].y - di->y) / d;
                p3by = p2y + h * (f->discs[n].x - di->x) / d;

                /* bounds check */
                if ((p3ax < 0) || (p3ax >= f->width) || (p3ay < 0) || (p3ay >= f->height) ||
                    (p3bx < 0) || (p3bx >= f->width) || (p3by < 0) || (p3by >= f->height))
                    continue;
                
                /* p3a and p3b might be identical, ignore this case for now */
                /* XPutPixel(f->off_map, p3ax, p3ay, f->fgcolor); */
                c = trans_point(st, p3ax, p3ay, 255, 0.75, f);
                XSetForeground(st->dpy, fgc, get_pixel (st, c));
                XFillRectangle(st->dpy, drawable, fgc, p3ax, p3ay,
                               st->pscale, st->pscale);

                /* XPutPixel(f->off_map, p3bx, p3by, f->fgcolor); */
                c = trans_point(st, p3bx, p3by, 255, 0.75, f);
                XSetForeground(st->dpy, fgc, get_pixel (st, c));
                XFillRectangle(st->dpy, drawable, fgc, p3bx, p3by,
                               st->pscale, st->pscale);
                XSetForeground(st->dpy, fgc, f->fgcolor);
            }
        }

    }

    /* Render all the pixel riders */
    for (m = 0; m < di->numr; m++) {
        moverender_rider(st, drawable, fgc, f, &(di->pxRiders[m]), 
                         di->x, di->y, di->r);
    }
}

static void build_img(Display *dpy, Window window, struct field *f) 
{
    if (f->off_alpha) {
        free(f->off_alpha);
        f->off_alpha = NULL;

        /* Assume theres also an off pixmap */
        if (f->off_map != window)
          XFreePixmap(dpy, f->off_map);
    }

    f->off_alpha = (unsigned char *) 
      xrealloc(f->off_alpha, sizeof(unsigned char) * f->width * f->height);

    memset(f->off_alpha, 0, sizeof(unsigned char) * f->width * f->height);

# ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
                        /* Or Android's */
    f->off_map = window;
# else
    f->off_map = XCreatePixmap(dpy, window, f->width, f->height, f->visdepth);
# endif

}

static inline void blank_img(Display *dpy, Window window, XWindowAttributes xgwa, GC fgc, struct field *f) 
{
    memset(f->off_alpha, 0, sizeof(unsigned char) * f->width * f->height);

    XSetForeground(dpy, fgc, f->bgcolor);
    XFillRectangle(dpy, window, fgc, 0, 0, xgwa.width, xgwa.height);
    XSetForeground(dpy, fgc, f->fgcolor);
}


static void *
intermomentary_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));

#ifdef TIME_ME
    time_t start_time = time(NULL);
#endif

    int tempx;
    XGCValues gcv;

    st->dpy = dpy;
    st->window = window;

    XGetWindowAttributes(dpy, window, &st->xgwa);
 
    st->ncolors = get_integer_resource (st->dpy, "colors", "Colors");
    st->ncolors++;
    st->colors = (XColor *) malloc(sizeof(*st->colors) * (st->ncolors+1));

    gcv.foreground = get_pixel_resource(dpy, st->xgwa.colormap,
                                        "foreground", "Foreground");
    gcv.background = get_pixel_resource(dpy, st->xgwa.colormap,
                                        "background", "Background");

    st->pscale = 1;
    if (st->xgwa.width > 2560 || st->xgwa.height > 2560)
      st->pscale *= 3;  /* Retina displays */
    /* We aren't increasing the spacing between the pixels, just the size. */
    /* #### This doesn't seem to help. Still needs lowrez. */

    {
      XColor fgc, bgc;
      int fgh, bgh;
      double fgs, fgv, bgs, bgv;
      fgc.pixel = gcv.foreground;
      bgc.pixel = gcv.background;
      XQueryColor (st->dpy, st->xgwa.colormap, &fgc);
      XQueryColor (st->dpy, st->xgwa.colormap, &bgc);
      rgb_to_hsv (fgc.red, fgc.green, fgc.blue, &fgh, &fgs, &fgv);
      rgb_to_hsv (bgc.red, bgc.green, bgc.blue, &bgh, &bgs, &bgv);
#if 0
      bgh = fgh;
      bgs = fgs;
      bgv = fgv / 10.0;
#endif
      make_color_ramp (st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
                       bgh, bgs, bgv,
                       fgh, fgs, fgv,
                       st->colors, &st->ncolors,
                       False, /* closed */
                       True, False);
    }

    st->f = init_field();

    st->f->height = st->xgwa.height;
    st->f->width = st->xgwa.width;
    st->f->visdepth = st->xgwa.depth;

    st->draw_delay = (get_integer_resource(dpy, "drawDelay", "Integer"));
    st->f->maxrider = (get_integer_resource(dpy, "maxRiders", "Integer"));
    st->f->maxradius = (get_integer_resource(dpy, "maxRadius", "Integer"));
    st->f->initial_discs = (get_integer_resource(dpy, "numDiscs", "Integer"));

    if (st->f->initial_discs <= 10) {
        fprintf(stderr, "%s: Initial discs must be greater than 10\n", progname);
        exit (1);
    }

    if (st->f->maxradius <= 30) {
        fprintf(stderr, "%s: Max radius must be greater than 30\n", progname);
        exit (1);
    }

    if (st->f->maxrider <= 10) {
        fprintf(stderr, "%s: Max riders must be greater than 10\n", progname);
        exit (1);
    }
    
    st->fgc = XCreateGC(dpy, window, GCForeground, &gcv);
    st->copygc = XCreateGC(dpy, window, GCForeground, &gcv);

    st->f->fgcolor = gcv.foreground;
    st->f->bgcolor = gcv.background;

    /* Initialize stuff */
    build_img(dpy, window, st->f);

    for (tempx = 0; tempx < st->f->initial_discs; tempx++) {
        float fx, fy, x, y, r;
        int bt;

        /* Arrange in anti-collapsing circle */
        fx = 0.4 * st->f->width * cos((2 * M_PI) * tempx / st->f->initial_discs);
        fy = 0.4 * st->f->height * sin((2 * M_PI) * tempx / st->f->initial_discs);
        x = frand(st->f->width / 2) + fx;
        y = frand(st->f->height / 2) + fy;
        r = 5 + frand(st->f->maxradius);
        bt = 1;

        if ((random() % 100) < 50)
            bt = -1;

        make_disc(st->f, x, y, bt * fx / 1000.0, bt * fy / 1000.0, r);
        
    }
    
    return st;
}

static unsigned long
intermomentary_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int tempx;

  if ((st->f->cycles % 10) == 0) {
    /* Restart if the window size changes */
    XGetWindowAttributes(dpy, window, &st->xgwa);

    if (st->f->height != st->xgwa.height || st->f->width != st->xgwa.width) {
      st->f->height = st->xgwa.height;
      st->f->width = st->xgwa.width;
      st->f->visdepth = st->xgwa.depth;

      build_img(dpy, window, st->f);
    }
  }

  blank_img(dpy, st->f->off_map, st->xgwa, st->fgc, st->f);
  for (tempx = 0; tempx < st->f->num; tempx++) {
    move_disc(st->f, tempx);
    render_disc(st, st->f->off_map, st->fgc, st->f, tempx);
  }

#if 0
  XSetFillStyle(dpy, st->copygc, FillTiled);
  XSetTile(dpy, st->copygc, st->f->off_map);
  XFillRectangle(dpy, window, st->copygc, 0, 0, st->f->width, st->f->height);
#else
  if (st->f->off_map != window)
    XCopyArea (dpy, st->f->off_map, window, st->copygc, 0, 0, 
               st->f->width, st->f->height, 0, 0);

#endif

  st->f->cycles++;

  return st->draw_delay;
}


static void
intermomentary_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
intermomentary_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
intermomentary_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  if (st->f) {
    if (st->f->off_alpha) free(st->f->off_alpha);
    if (st->f->off_map != window) XFreePixmap (dpy, st->f->off_map);
    if (st->f->discs) {
      int i;
      for (i = 0; i < st->f->num; i++) {
        if (st->f->discs[i].pxRiders) free (st->f->discs[i].pxRiders);
      }
      free (st->f->discs);
    }
    free (st->f);
  }
  if (st->colors) free (st->colors);
  XFreeGC (dpy, st->fgc);
  XFreeGC (dpy, st->copygc);

  free (st);
}


static const char *intermomentary_defaults[] = {
    ".lowrez: true",
    ".background: black",
    ".foreground: yellow",
    "*drawDelay: 30000",
    "*numDiscs: 85",
    "*maxRiders: 40",
    "*maxRadius: 100",
    "*colors: 256",
# ifdef HAVE_MOBILE
    "*ignoreRotation: True",
# endif
    0
};

static XrmOptionDescRec intermomentary_options[] = {
    {"-background", ".background", XrmoptionSepArg, 0},
    {"-foreground", ".foreground", XrmoptionSepArg, 0},
    {"-draw-delay", ".drawDelay", XrmoptionSepArg, 0},
    {"-num-discs", ".numDiscs", XrmoptionSepArg, 0},
    {"-max-riders", ".maxRiders", XrmoptionSepArg, 0},
    {"-max-radius", ".maxRadius", XrmoptionSepArg, 0},
    { "-colors",    ".colors",    XrmoptionSepArg, 0 },
    {0, 0, 0, 0}
};


XSCREENSAVER_MODULE ("Intermomentary", intermomentary)
