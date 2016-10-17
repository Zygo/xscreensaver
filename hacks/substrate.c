/*
 *  Substrate (dragorn@kismetwireless.net)
 *  Directly ported code from complexification.net Substrate art
 *  http://complexification.net/gallery/machines/substrate/applet_s/substrate_s.pde
 *
 *  Substrate code:
 *  j.tarbell   June, 2004
 *  Albuquerque, New Mexico
 *  complexification.net
 *
 *  CHANGES
 *
 *  1.1  dragorn  Jan 04 2005    Fixed some indenting, typo in errors for parsing
 *                                cmdline args
 *  1.1  dagraz   Jan 04 2005    Added option for circular cracks (David Agraz)
 *                               Cleaned up issues with timeouts in start_crack (DA)
 *  1.0  dragorn  Oct 10 2004    First port done
 *
 * Directly based the hacks of: 
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

/* this program goes faster if some functions are inline.  The following is
 * borrowed from ifs.c */
#if !defined( __GNUC__ ) && !defined(__cplusplus) && !defined(c_plusplus)
#undef inline
#define inline			/* */
#endif

#define STEP 0.42

/* Raw colormap extracted from pollockEFF.gif */
static const char *rgb_colormap[] = {
    "#201F21", "#262C2E", "#352626", "#372B27",
    "#302C2E", "#392B2D", "#323229", "#3F3229",
    "#38322E", "#2E333D", "#333A3D", "#473329",
    "#40392C", "#40392E", "#47402C", "#47402E",
    "#4E402C", "#4F402E", "#4E4738", "#584037",
    "#65472D", "#6D5D3D", "#745530", "#755532",
    "#745D32", "#746433", "#7C6C36", "#523152",
    "#444842", "#4C5647", "#655D45", "#6D5D44",
    "#6C5D4E", "#746C43", "#7C6C42", "#7C6C4B",
    "#6B734B", "#73734B", "#7B7B4A", "#6B6C55",
    "#696D5E", "#7B6C5D", "#6B7353", "#6A745D",
    "#727B52", "#7B7B52", "#57746E", "#687466",
    "#9C542B", "#9D5432", "#9D5B35", "#936B36",
    "#AA7330", "#C45A27", "#D95223", "#D85A20",
    "#DB5A23", "#E57037", "#836C4B", "#8C6B4B",
    "#82735C", "#937352", "#817B63", "#817B6D",
    "#927B63", "#D9893B", "#E49832", "#DFA133",
    "#E5A037", "#F0AB3B", "#8A8A59", "#B29A58",
    "#89826B", "#9A8262", "#888B7C", "#909A7A",
    "#A28262", "#A18A69", "#A99968", "#99A160",
    "#99A168", "#CA8148", "#EB8D43", "#C29160",
    "#C29168", "#D1A977", "#C9B97F", "#F0E27B",
    "#9F928B", "#C0B999", "#E6B88F", "#C8C187",
    "#E0C886", "#F2CC85", "#F5DA83", "#ECDE9D",
    "#F5D294", "#F5DA94", "#F4E784", "#F4E18A",
    "#F4E193", "#E7D8A7", "#F1D4A5", "#F1DCA5",
    "#F4DBAD", "#F1DCAE", "#F4DBB5", "#F5DBBD",
    "#F4E2AD", "#F5E9AD", "#F4E3BE", "#F5EABE",
    "#F7F0B6", "#D9D1C1", "#E0D0C0", "#E7D8C0",
    "#F1DDC6", "#E8E1C0", "#F3EDC7", "#F6ECCE",
    "#F8F2C7", "#EFEFD0", 0
};

typedef struct {
    /* Synthesis of data from Crack:: and SandPainter:: */
    float x, y;
    float t;
    float ys, xs, t_inc; /* for curvature calculations */

    int curved;

    unsigned long sandcolor;
    float sandp, sandg;

    float degrees_drawn;

    int crack_num;

} crack;

struct field {
    unsigned int height;
    unsigned int width;

    unsigned int initial_cracks;
    
    unsigned int num;
    unsigned int max_num;

    int grains; /* number of grains in the sand painting */

    int circle_percent;

    crack *cracks; /* grid of cracks */
    int *cgrid; /* grid of actual crack placement */

    /* Raw map of pixels we need to keep for alpha blending */
    unsigned long int *off_img;
   
    /* color parms */
    int numcolors;
    unsigned long *parsedcolors;
    unsigned long fgcolor;
    unsigned long bgcolor;
    int visdepth;

    unsigned int cycles;

    unsigned int wireframe;
    unsigned int seamless;
};

struct state {
  Display *dpy;
  Window window;

  struct field *f;
  unsigned int max_cycles;
  int growth_delay;
  GC fgc;
  XWindowAttributes xgwa;
  XGCValues gcv;
};


static void 
*xrealloc(void *p, size_t size)
{
    void *ret;
    if ((ret = realloc(p, size)) == NULL) {
	fprintf(stderr, "%s: out of memory\n", progname);
	exit(1);
    }
    return ret;
}

static struct field 
*init_field(void)
{
    struct field *f = xrealloc(NULL, sizeof(struct field));
    f->height = 0;
    f->width = 0;
    f->initial_cracks = 0;
    f->num = 0;
    f->max_num = 0;
    f->cracks = NULL;
    f->cgrid = NULL;
    f->off_img = NULL;
    f->numcolors = 0;
    f->parsedcolors = NULL;
    f->cycles = 0;
    f->wireframe = 0;
    f->seamless = 0;
    f->fgcolor = 0;
    f->bgcolor = 0;
    f->visdepth = 0;
    f->grains = 0;
    f->circle_percent = 0;
    return f;
}

/* Quick references to pixels in the offscreen map and in the crack grid */
#define ref_pixel(f, x, y)   ((f)->off_img[(y) * (f)->width + (x)])
#define ref_cgrid(f, x, y)   ((f)->cgrid[(y) * (f)->width + (x)])

static inline void start_crack(struct field *f, crack *cr) 
{
    /* synthesis of Crack::findStart() and crack::startCrack() */
    int px = 0;
    int py = 0;
    int found = 0;
    int timeout = 0;
    float a;

    /* shift until crack is found */
    while ((!found) && (timeout++ < 10000)) {
        px = (int) (random() % f->width);
        py = (int) (random() % f->height);

        if (ref_cgrid(f, px, py) < 10000)
            found = 1;
    }

    if ( !found ) {
        /* We timed out.  Use our default values */
        px = cr->x;
        py = cr->y;

        /* Sanity check needed */
        if (px < 0) px = 0;
        if (px >= f->width) px = f->width - 1;
        if (py < 0) py = 0;
        if (py >= f->height) py = f->height - 1;

        ref_cgrid(f, px, py) = cr->t;
    }

    /* start a crack */
    a = ref_cgrid(f, px, py);

    if ((random() % 100) < 50) {
        /* conversion of the java int(random(-2, 2.1)) */
        a -= 90 + (frand(4.1) - 2.0);
    } else {
        a += 90 + (frand(4.1) - 2.0);
    }

    if ((random() % 100) < f->circle_percent) {
        float r; /* radius */
        float radian_inc;

        cr->curved = 1;
        cr->degrees_drawn = 0;

        r = 10 + (random() % ((f->width + f->height) / 2));

        if ((random() % 100) < 50) {
            r *= -1;
        }

        /* arc length = r * theta => theta = arc length / r */
        radian_inc = STEP / r;
        cr->t_inc = radian_inc * 360 / 2 / M_PI;

        cr->ys = r * sin(radian_inc);
        cr->xs = r * ( 1 - cos(radian_inc));

    }
    else {
        cr->curved = 0;
    }

    /* Condensed from Crack::startCrack */
    cr->x = px + ((float) 0.61 * cos(a * M_PI / 180));
    cr->y = py + ((float) 0.61 * sin(a * M_PI / 180));
    cr->t = a;

}

static inline void make_crack(struct field *f) 
{
    crack *cr;

    if (f->num < f->max_num) {
        /* make a new crack */
        f->cracks = (crack *) xrealloc(f->cracks, sizeof(crack) * (f->num + 1));

        cr = &(f->cracks[f->num]);
        /* assign colors */
        cr->sandp = 0;
        cr->sandg = (frand(0.2) - 0.01);
        cr->sandcolor = f->parsedcolors[random() % f->numcolors];
        cr->crack_num = f->num;
        cr->curved = 0;
        cr->degrees_drawn = 0;

        /* We could use these values in the timeout case of start_crack */

        cr->x = random() % f->width;
        cr->y = random() % f->height;
        cr->t = random() % 360;

        /* start it */
        start_crack(f, cr);

        f->num++;
    }
}

static inline void point2rgb(int depth, unsigned long c, int *r, int *g, int *b) 
{
    switch(depth) {
        case 32:
        case 24:
#ifdef HAVE_JWXYZ
            /* This program idiotically does not go through a color map, so
               we have to hardcode in knowledge of how jwxyz.a packs pixels!
               Fix it to go through st->colors[st->ncolors] instead!
             */
            *r = (c & 0x00ff0000) >> 16; 
            *g = (c & 0x0000ffff) >>  8;
            *b = (c & 0x000000ff); 
#else
            *g = (c & 0xff00) >> 8; 
            *r = (c & 0xff0000) >> 16; 
            *b = c & 0xff; 
#endif
            break;
        case 16:
            *g = ((c >> 5) & 0x3f) << 2;
            *r = ((c >> 11) & 0x1f) << 3; 
            *b = (c & 0x1f) << 3; 
            break;
        case 15:
            *g = ((c >> 5) & 0x1f) << 3;
            *r = ((c >> 10) & 0x1f) << 3;
            *b = (c & 0x1f) << 3;
            break;
    }
}

static inline unsigned long rgb2point(int depth, int r, int g, int b) 
{
    unsigned long ret = 0;

    switch(depth) {
        case 32:
        case 24:
#ifdef HAVE_JWXYZ
            /* This program idiotically does not go through a color map, so
               we have to hardcode in knowledge of how jwxyz.a packs pixels!
               Fix it to go through st->colors[st->ncolors] instead!
             */
            ret = 0xFF000000 | (r << 16) | (g << 8) | b;
#else
            ret |= (r << 16) | (g << 8) | b;
#endif
            break;
        case 16:
            ret = ((r>>3) << 11) | ((g>>2)<<5) | (b>>3);
            break;
        case 15:
            ret = ((r>>3) << 10) | ((g>>3)<<5) | (b>>3);
            break;
    }

    return ret;
}

/* alpha blended point drawing -- this is Not Right and will likely fail on 
 * non-intel platforms as it is now, needs fixing */
static inline unsigned long
trans_point(struct state *st,
            int x1, int y1, unsigned long myc, float a, 
            struct field *f) 
{
    if ((x1 >= 0) && (x1 < f->width) && (y1 >= 0) && (y1 < f->height)) {
        if (a >= 1.0) {
            ref_pixel(f, x1, y1) = myc;
        } else {
            int or = 0, og = 0, ob = 0;
            int r = 0, g = 0, b = 0;
            int nr, ng, nb;
            unsigned long c;

            c = ref_pixel(f, x1, y1);

            point2rgb(f->visdepth, c, &or, &og, &ob);
            point2rgb(f->visdepth, myc, &r, &g, &b);

            nr = or + (r - or) * a;
            ng = og + (g - og) * a;
            nb = ob + (b - ob) * a;

            c = rgb2point(f->visdepth, nr, ng, nb);

            ref_pixel(f, x1, y1) = c;

            return c;
        }
    }

    return f->bgcolor;
}

static inline void 
region_color(struct state *st, GC fgc, struct field *f, crack *cr) 
{
    /* synthesis of Crack::regionColor() and SandPainter::render() */

    float rx = cr->x;
    float ry = cr->y;
    int openspace = 1;
    int cx, cy;
    float maxg;
    int grains, i;
    float w;
    float drawx, drawy;
    unsigned long c;

    while (openspace) {
        /* move perpendicular to crack */
        rx += (0.81 * sin(cr->t * M_PI/180));
        ry -= (0.81 * cos(cr->t * M_PI/180));

        cx = (int) rx;
        cy = (int) ry;
        if (f->seamless) {
            cx %= f->width;
            cy %= f->height;
        }

        if ((cx >= 0) && (cx < f->width) && (cy >= 0) && (cy < f->height)) {
            /* safe to check */
            if (f->cgrid[cy * f->width + cx] > 10000) {
                /* space is open */
            } else {
                openspace = 0;
            }
        } else {
            openspace = 0;
        }
    }

    /* SandPainter stuff here */

    /* Modulate gain */
    cr->sandg += (frand(0.1) - 0.050);
    maxg = 1.0;

    if (cr->sandg < 0)
        cr->sandg = 0;

    if (cr->sandg > maxg)
        cr->sandg = maxg;

    grains = f->grains;

    /* Lay down grains of sand */
    w = cr->sandg / (grains - 1);

    for (i = 0; i < grains; i++) {
        drawx = (cr->x + (rx - cr->x) * sin(cr->sandp + sin((float) i * w)));
        drawy = (cr->y + (ry - cr->y) * sin(cr->sandp + sin((float) i * w)));
        if (f->seamless) {
            drawx = fmod(drawx + f->width, f->width);
            drawy = fmod(drawy + f->height, f->height);
        }

        /* Draw sand bit */
        c = trans_point(st, drawx, drawy, cr->sandcolor, (0.1 - i / (grains * 10.0)), f);

        XSetForeground(st->dpy, fgc, c);
        XDrawPoint(st->dpy, st->window, fgc, (int) drawx, (int) drawy);
        XSetForeground(st->dpy, fgc, f->fgcolor);
    }
}

static void build_substrate(struct field *f) 
{
    int tx;
    /* int ty; */

    f->cycles = 0;

    if (f->cgrid) {
        free(f->cgrid);
        f->cgrid = NULL;
    }

    if (f->cracks) {
        free(f->cracks);
        f->cracks = NULL;
    }

    f->num = 0;

    /* erase the crack grid */
    f->cgrid = (int *) xrealloc(f->cgrid, sizeof(int) * f->height * f->width);
    {
        int j;
        int *p = f->cgrid;
        for (j = 0; j < f->height * f->width; j++)
            *p++ = 10001;
    }

    /* Not necessary now that make_crack ensures we have usable default
     *  values in start_crack's timeout case 
    * make random crack seeds *
    for (tx = 0; tx < 16; tx++) {
        ty = (int) (random() % (f->width * f->height - 1));
        f->cgrid[ty] = (int) random() % 360;
    }
    */

    /* make the initial cracks */
    for (tx = 0; tx < f->initial_cracks; tx++)
        make_crack(f);
}


static inline void
movedrawcrack(struct state *st, GC fgc, struct field *f, int cracknum) 
{
    /* Basically Crack::move() */

    int cx, cy;
    crack *cr = &(f->cracks[cracknum]);

    /* continue cracking */
    if ( !cr->curved ) {
        cr->x += ((float) STEP * cos(cr->t * M_PI/180));
        cr->y += ((float) STEP * sin(cr->t * M_PI/180));
    }
    else {
        cr->x += ((float) cr->ys * cos(cr->t * M_PI/180));
        cr->y += ((float) cr->ys * sin(cr->t * M_PI/180));

        cr->x += ((float) cr->xs * cos(cr->t * M_PI/180 - M_PI / 2));
        cr->y += ((float) cr->xs * sin(cr->t * M_PI/180 - M_PI / 2));

        cr->t += cr->t_inc;
        cr->degrees_drawn += fabsf(cr->t_inc);
    }
    if (f->seamless) {
        cr->x = fmod(cr->x + f->width, f->width);
        cr->y = fmod(cr->y + f->height, f->height);
    }

    /* bounds check */
    /* modification of random(-0.33,0.33) */
    cx = (int) (cr->x + (frand(0.66) - 0.33));
    cy = (int) (cr->y + (frand(0.66) - 0.33));
    if (f->seamless) {
        cx %= f->width;
        cy %= f->height;
    }

    if ((cx >= 0) && (cx < f->width) && (cy >= 0) && (cy < f->height)) {
        /* draw sand painter if we're not wireframe */
        if (!f->wireframe)
            region_color(st, fgc, f, cr);

        /* draw fgcolor crack */
        ref_pixel(f, cx, cy) = f->fgcolor;
        XDrawPoint(st->dpy, st->window, fgc, cx, cy);

        if ( cr->curved && (cr->degrees_drawn > 360) ) {
            /* completed the circle, stop cracking */
            start_crack(f, cr); /* restart ourselves */
            make_crack(f); /* generate a new crack */
        }
        /* safe to check */
        else if ((f->cgrid[cy * f->width + cx] > 10000) ||
                 (fabsf(f->cgrid[cy * f->width + cx] - cr->t) < 5)) {
            /* continue cracking */
            f->cgrid[cy * f->width + cx] = (int) cr->t;
        } else if (fabsf(f->cgrid[cy * f->width + cx] - cr->t) > 2) {
            /* crack encountered (not self), stop cracking */
            start_crack(f, cr); /* restart ourselves */
            make_crack(f); /* generate a new crack */
        }
    } else {
        /* out of bounds, stop cracking */

	/* need these in case of timeout in start_crack */
        cr->x = random() % f->width;
        cr->y = random() % f->height;
        cr->t = random() % 360;

        start_crack(f, cr); /* restart ourselves */
        make_crack(f); /* generate a new crack */
    }

}


static void build_img(Display *dpy, Window window, XWindowAttributes xgwa, GC fgc, 
               struct field *f) 
{
    if (f->off_img) {
        free(f->off_img);
        f->off_img = NULL;
    }

    f->off_img = (unsigned long *) xrealloc(f->off_img, sizeof(unsigned long) * 
                                            f->width * f->height);

    memset(f->off_img, f->bgcolor, sizeof(unsigned long) * f->width * f->height);
}


static void *
substrate_init (Display *dpy, Window window)
{
    struct state *st = (struct state *) calloc (1, sizeof(*st));
    XColor tmpcolor;

    st->dpy = dpy;
    st->window = window;
    st->f = init_field();

    st->growth_delay = (get_integer_resource(st->dpy, "growthDelay", "Integer"));
    st->max_cycles = (get_integer_resource(st->dpy, "maxCycles", "Integer"));
    st->f->initial_cracks = (get_integer_resource(st->dpy, "initialCracks", "Integer"));
    st->f->max_num = (get_integer_resource(st->dpy, "maxCracks", "Integer"));
    st->f->wireframe = (get_boolean_resource(st->dpy, "wireFrame", "Boolean"));
    st->f->grains = (get_integer_resource(st->dpy, "sandGrains", "Integer"));
    st->f->circle_percent = (get_integer_resource(st->dpy, "circlePercent", "Integer"));
    st->f->seamless = (get_boolean_resource(st->dpy, "seamless", "Boolean"));

    if (st->f->initial_cracks <= 2) {
        fprintf(stderr, "%s: Initial cracks must be greater than 2\n", progname);
        exit (1);
    }

    if (st->f->max_num <= 10) {
        fprintf(stderr, "%s: Maximum number of cracks must be less than 10\n", 
                progname);
        exit (1);
    }

    if (st->f->circle_percent < 0) {
        fprintf(stderr, "%s: circle percent must be at least 0\n", progname);
        exit (1);
    }

    if (st->f->circle_percent > 100) {
        fprintf(stderr, "%s: circle percent must be less than 100\n", progname);
        exit (1);
    }
    
    XGetWindowAttributes(st->dpy, st->window, &st->xgwa);

    st->f->height = st->xgwa.height;
    st->f->width = st->xgwa.width;
    st->f->visdepth = st->xgwa.depth;
 
    /* Count the colors in our map and assign them in a horrifically inefficient 
     * manner but it only happens once */
    while (rgb_colormap[st->f->numcolors] != NULL) {
        st->f->parsedcolors = (unsigned long *) xrealloc(st->f->parsedcolors, 
                                                     sizeof(unsigned long) * 
                                                     (st->f->numcolors + 1));
        if (!XParseColor(st->dpy, st->xgwa.colormap, rgb_colormap[st->f->numcolors], &tmpcolor)) {
            fprintf(stderr, "%s: couldn't parse color %s\n", progname,
                    rgb_colormap[st->f->numcolors]);
            exit(1);
        }

        if (!XAllocColor(st->dpy, st->xgwa.colormap, &tmpcolor)) {
            fprintf(stderr, "%s: couldn't allocate color %s\n", progname,
                    rgb_colormap[st->f->numcolors]);
            exit(1);
        }

        st->f->parsedcolors[st->f->numcolors] = tmpcolor.pixel;

        st->f->numcolors++;
    }

    st->gcv.foreground = get_pixel_resource(st->dpy, st->xgwa.colormap,
                                        "foreground", "Foreground");
    st->gcv.background = get_pixel_resource(st->dpy, st->xgwa.colormap,
                                        "background", "Background");
    st->fgc = XCreateGC(st->dpy, st->window, GCForeground, &st->gcv);

    st->f->fgcolor = st->gcv.foreground;
    st->f->bgcolor = st->gcv.background;

    /* Initialize stuff */
    build_img(st->dpy, st->window, st->xgwa, st->fgc, st->f);
    build_substrate(st->f);
    
    return st;
}

static unsigned long
substrate_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int tempx;

  if ((st->f->cycles % 10) == 0) {

    /* Restart if the window size changes */
    XGetWindowAttributes(st->dpy, st->window, &st->xgwa);

    if (st->f->height != st->xgwa.height || st->f->width != st->xgwa.width) {
      st->f->height = st->xgwa.height;
      st->f->width = st->xgwa.width;
      st->f->visdepth = st->xgwa.depth;

      build_substrate(st->f);
      build_img(st->dpy, st->window, st->xgwa, st->fgc, st->f);
      XSetForeground(st->dpy, st->fgc, st->gcv.background);
      XFillRectangle(st->dpy, st->window, st->fgc, 0, 0, st->xgwa.width, st->xgwa.height);
      XSetForeground(st->dpy, st->fgc, st->gcv.foreground);
    }
  }

  for (tempx = 0; tempx < st->f->num; tempx++) {
    movedrawcrack(st, st->fgc, st->f, tempx);
  }

  st->f->cycles++;

  if (st->f->cycles >= st->max_cycles && st->max_cycles != 0) {
    build_substrate(st->f);
    build_img(st->dpy, st->window, st->xgwa, st->fgc, st->f);
    XSetForeground(st->dpy, st->fgc, st->gcv.background);
    XFillRectangle(st->dpy, st->window, st->fgc, 0, 0, st->xgwa.width, st->xgwa.height);
    XSetForeground(st->dpy, st->fgc, st->gcv.foreground);
  }

  /* #### mi->recursion_depth = st->f->cycles; */
  return st->growth_delay;
}


static void
substrate_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
substrate_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->f->cycles = st->max_cycles;
      return True;
    }
  return False;
}

static void
substrate_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}

static const char *substrate_defaults[] = {
    ".background: white",
    ".foreground: black",
    "*fpsSolid:	true",
    "*wireFrame: false",
    "*seamless: false",
    "*maxCycles: 10000",
    "*growthDelay: 18000",
    "*initialCracks: 3",
    "*maxCracks: 100",
    "*sandGrains: 64",
    "*circlePercent: 33",
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
#endif
    0
};

static XrmOptionDescRec substrate_options[] = {
    {"-background", ".background", XrmoptionSepArg, 0},
    {"-foreground", ".foreground", XrmoptionSepArg, 0},
    {"-wireframe", ".wireFrame", XrmoptionNoArg, "true"},
    {"-seamless", ".seamless", XrmoptionNoArg, "true"},
    {"-max-cycles", ".maxCycles", XrmoptionSepArg, 0},
    {"-growth-delay", ".growthDelay", XrmoptionSepArg, 0},
    {"-initial-cracks", ".initialCracks", XrmoptionSepArg, 0},
    {"-max-cracks", ".maxCracks", XrmoptionSepArg, 0},
    {"-sand-grains", ".sandGrains", XrmoptionSepArg, 0},
    {"-circle-percent", ".circlePercent", XrmoptionSepArg, 0},
    {0, 0, 0, 0}
};

XSCREENSAVER_MODULE ("Substrate", substrate)
