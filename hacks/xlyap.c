/* Lyap - calculate and display Lyapunov exponents */

/* Written by Ron Record (rr@sco) 03 Sep 1991 */

/* The idea here is to calculate the Lyapunov exponent for a periodically
 * forced logistic map (later i added several other nonlinear maps of the unit
 * interval). In order to turn the 1-dimensional parameter space of the
 * logistic map into a 2-dimensional parameter space, select two parameter
 * values ('a' and 'b') then alternate the iterations of the logistic map using
 * first 'a' then 'b' as the parameter. This program accepts an argument to
 * specify a forcing function, so instead of just alternating 'a' and 'b', you
 * can use 'a' as the parameter for say 6 iterations, then 'b' for 6 iterations
 * and so on. An interesting forcing function to look at is abbabaab (the
 * Morse-Thue sequence, an aperiodic self-similar, self-generating sequence).
 * Anyway, step through all the values of 'a' and 'b' in the ranges you want,
 * calculating the Lyapunov exponent for each pair of values. The exponent
 * is calculated by iterating out a ways (specified by the variable "settle")
 * then on subsequent iterations calculating an average of the logarithm of
 * the absolute value of the derivative at that point. Points in parameter
 * space with a negative Lyapunov exponent are colored one way (using the
 * value of the exponent to index into a color map) while points with a
 * non-negative exponent are colored differently.
 *
 * The algorithm was taken from the September 1991 Scientific American article
 * by A. K. Dewdney who gives credit to Mario Markus of the Max Planck
 * Institute for its creation. Additional information and ideas were gleaned
 * from the discussion on alt.fractals involving Stephen Hall, Ed Kubaitis,
 * Dave Platt and Baback Moghaddam. Assistance with colormaps and spinning
 * color wheels and X was gleaned from Hiram Clawson. Rubber banding code was
 * adapted from an existing Mandelbrot program written by Stacey Campbell.
 */

#define LYAP_PATCHLEVEL 4
#define LYAP_VERSION "#(@) lyap 2.3 2/20/92"

#include <assert.h>
#include <math.h>

#include "screenhack.h"
#include "yarandom.h"
#include "hsv.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#ifndef HAVE_COCOA
# include <X11/cursorfont.h> 
#endif

static const char *xlyap_defaults [] = {
  ".background:         black",
  ".foreground:         white",
  "*fpsSolid:		true",
  "*randomize:          true",
  "*builtin:            -1",
  "*minColor:           1",
  "*maxColor:           256",
  "*dwell:              50",
  "*useLog:             false",
  "*colorExponent:      1.0",
  "*colorOffset:        0",
  "*randomForce:        ",              /* 0.5 */
  "*settle:             50",
  "*minA:               2.0",
  "*minB:               2.0",
  "*wheels:             7",
  "*function:           10101010",
  "*forcingFunction:    abbabaab",
  "*bRange:             ",              /* 2.0 */
  "*startX:             0.65",
  "*mapIndex:           ",              /* 0 */
  "*outputFile:         ",
  "*beNegative:         false",
  "*rgbMax:             65000",
  "*spinLength:         256",
  "*show:               false",
  "*aRange:             ",              /* 2.0 */
  "*delay:              10000",
  "*linger:             5",
  "*colors:             200",
#ifdef USE_IPHONE
  "*ignoreRotation:     True",
#endif
  0
};

static XrmOptionDescRec xlyap_options [] = {
  { "-randomize", ".randomize", XrmoptionNoArg, "true" },
  { "-builtin",   ".builtin",   XrmoptionSepArg, 0 },
  { "-C", ".minColor",          XrmoptionSepArg, 0 },   /* n */
  { "-D", ".dwell",             XrmoptionSepArg, 0 },   /* n */
  { "-L", ".useLog",            XrmoptionNoArg, "true" },
  { "-M", ".colorExponent",     XrmoptionSepArg, 0 },   /* r */
  { "-O", ".colorOffset",       XrmoptionSepArg, 0 },   /* n */
  { "-R", ".randomForce",       XrmoptionSepArg, 0 },   /* p */
  { "-S", ".settle",            XrmoptionSepArg, 0 },   /* n */
  { "-a", ".minA",              XrmoptionSepArg, 0 },   /* r */
  { "-b", ".minB",              XrmoptionSepArg, 0 },   /* n */
  { "-c", ".wheels",            XrmoptionSepArg, 0 },   /* n */
  { "-F", ".function",          XrmoptionSepArg, 0 },   /* 10101010 */
  { "-f", ".forcingFunction",   XrmoptionSepArg, 0 },   /* abbabaab */
  { "-h", ".bRange",            XrmoptionSepArg, 0 },   /* r */
  { "-i", ".startX",            XrmoptionSepArg, 0 },   /* r */
  { "-m", ".mapIndex",          XrmoptionSepArg, 0 },   /* n */
  { "-o", ".outputFile",        XrmoptionSepArg, 0 },   /* filename */
  { "-p", ".beNegative",        XrmoptionNoArg, "true" },
  { "-r", ".rgbMax",            XrmoptionSepArg, 0 },   /* n */
  { "-s", ".spinLength",        XrmoptionSepArg, 0 },   /* n */
  { "-v", ".show",              XrmoptionNoArg, "true" },
  { "-w", ".aRange",            XrmoptionSepArg, 0 },   /* r */
  { "-delay", ".delay",         XrmoptionSepArg, 0 },   /* delay */
  { "-linger", ".linger",       XrmoptionSepArg, 0 },   /* linger */
  { 0, 0, 0, 0 }
};


#define ABS(a)  (((a)<0) ? (0-(a)) : (a) )
#define Min(x,y) ((x < y)?x:y)
#define Max(x,y) ((x > y)?x:y)

#ifdef SIXTEEN_COLORS
# define MAXPOINTS  128
# ifdef BIGMEM
#  define MAXFRAMES 4
# else  /* !BIGMEM */
#  define MAXFRAMES 2
# endif /* !BIGMEM */
# define MAXCOLOR 16
#else  /* !SIXTEEN_COLORS */
# define MAXPOINTS  256
# ifdef BIGMEM
#  define MAXFRAMES 8
# else  /* !BIGMEM */
#  define MAXFRAMES 2
# endif /* !BIGMEM */
# define MAXCOLOR 256
#endif /* !SIXTEEN_COLORS */


#define MAXINDEX 64
#define FUNCMAXINDEX 16
#define MAXWHEELS 7
#define NUMMAPS 5
#define NBUILTINS 22

#ifndef TRUE
# define TRUE 1
# define FALSE 0
#endif


typedef struct {
  int x, y;
} xy_t;

#if 0
typedef struct {
  int start_x, start_y;
  int last_x, last_y;
} rubber_band_data_t;
#endif

typedef struct {
# ifndef HAVE_COCOA
  Cursor band_cursor;
# endif
  double p_min, p_max, q_min, q_max;
/*  rubber_band_data_t rubber_band;*/
} image_data_t;

typedef struct points_t {
  XPoint data[MAXCOLOR][MAXPOINTS];
  int npoints[MAXCOLOR];
} points_t;


typedef double (*PFD)(double,double);

/* #### What was this for?  Everything was drawn twice, to the window and 
   to this, and this was never displayed! */
/*#define BACKING_PIXMAP*/

struct state {
  Display *dpy;
  Screen *screen;
  Visual *visual;
  Colormap cmap;

  unsigned long foreground, background;

  Window canvas;
  int delay, linger;

  unsigned int maxcolor, startcolor, mincolindex;
  int color_offset;
  int dwell, settle;
  int width, height, xposition, yposition;

  points_t Points;
/*  image_data_t rubber_data;*/

  GC Data_GC[MAXCOLOR]/*, RubberGC*/;
  PFD map, deriv;

  int aflag, bflag, wflag, hflag, Rflag;

  int   maxindex;
  int   funcmaxindex;
  double  min_a, min_b, a_range, b_range, minlyap;
  double  max_a, max_b;
  double  start_x, lyapunov, a_inc, b_inc, a, b;
  int   numcolors, numfreecols, lowrange;
  xy_t  point;
#ifdef BACKING_PIXMAP
  Pixmap  pixmap;
#endif
/*  XColor  Colors[MAXCOLOR];*/
  double  *exponents[MAXFRAMES];
  double  a_minimums[MAXFRAMES], b_minimums[MAXFRAMES];
  double  a_maximums[MAXFRAMES], b_maximums[MAXFRAMES];
  double  minexp, maxexp, prob;
  int     expind[MAXFRAMES], resized[MAXFRAMES];
  int     numwheels, force, Force, negative;
  int     rgb_max, nostart, stripe_interval;
  int     save, show, useprod, spinlength;
  int     maxframe, frame, dorecalc, mapindex, run;
  char    *outname;

  int sendpoint_index;

  int   forcing[MAXINDEX];
  int   Forcing[FUNCMAXINDEX];

  int reset_countdown;

  int ncolors;
  XColor colors[MAXCOLOR];
};


static const double pmins[NUMMAPS] = { 2.0, 0.0, 0.0, 0.0, 0.0 };
static const double pmaxs[NUMMAPS] = { 4.0, 1.0, 6.75, 6.75, 16.0 };
static const double amins[NUMMAPS] = { 2.0, 0.0, 0.0, 0.0, 0.0 };
static const double aranges[NUMMAPS] = { 2.0, 1.0, 6.75, 6.75, 16.0 };
static const double bmins[NUMMAPS] = { 2.0, 0.0, 0.0, 0.0, 0.0 };
static const double branges[NUMMAPS] = { 2.0, 1.0, 6.75, 6.75, 16.0 };

/****************************************************************************/

/* callback function declarations
 */

static double logistic(double,double);
static double circle(double,double);
static double leftlog(double,double);
static double rightlog(double,double);
static double doublelog(double,double);
static double dlogistic(double,double);
static double dcircle(double,double);
static double dleftlog(double,double);
static double drightlog(double,double);
static double ddoublelog(double,double);

static const PFD Maps[NUMMAPS] = { logistic, circle, leftlog, rightlog, 
                                   doublelog };
static const PFD Derivs[NUMMAPS] = { dlogistic, dcircle, dleftlog, 
                                     drightlog, ddoublelog };


/****************************************************************************/

/* other function declarations
 */

static void resize(struct state *);
/*static void Spin(struct state *);*/
static void show_defaults(struct state *);
/*static void StartRubberBand(struct state *, image_data_t *, XEvent *);
static void TrackRubberBand(struct state *, image_data_t *, XEvent *);
static void EndRubberBand(struct state *, image_data_t *, XEvent *);*/
/*static void CreateXorGC(struct state *);*/
static void InitBuffer(struct state *);
static void BufferPoint(struct state *, int color, int x, int y);
static void FlushBuffer(struct state *);
static void init_data(struct state *);
static void init_color(struct state *);
static void parseargs(struct state *);
static void Clear(struct state *);
static void setupmem(struct state *);
static int complyap(struct state *);
static Bool Getkey(struct state *, XKeyEvent *);
static int sendpoint(struct state *, double expo);
/*static void save_to_file(struct state *);*/
static void setforcing(struct state *);
static void check_params(struct state *, int mapnum, int parnum);
static void usage(struct state *);
static void Destroy_frame(struct state *);
static void freemem(struct state *);
static void Redraw(struct state *);
static void redraw(struct state *, double *exparray, int index, int cont);
static void recalc(struct state *);
/*static void SetupCorners(XPoint *, image_data_t *);
static void set_new_params(struct state *, image_data_t *);*/
static void go_down(struct state *);
static void go_back(struct state *);
static void go_init(struct state *);
static void jumpwin(struct state *);
static void print_help(struct state *);
static void print_values(struct state *);


/****************************************************************************/


/* complyap() is the guts of the program. This is where the Lyapunov exponent
 * is calculated. For each iteration (past some large number of iterations)
 * calculate the logarithm of the absolute value of the derivative at that
 * point. Then average them over some large number of iterations. Some small
 * speed up is achieved by utilizing the fact that log(a*b) = log(a) + log(b).
 */
static int
complyap(struct state *st)
{
  int i, bindex;
  double total, prod, x, dx, r;

  if (st->maxcolor > MAXCOLOR)
    abort();

  if (!st->run)
    return TRUE;
  st->a += st->a_inc;
  if (st->a >= st->max_a) {
    if (sendpoint(st, st->lyapunov) == TRUE)
      return FALSE;
    else {
      FlushBuffer(st);
      /*      if (savefile)
              save_to_file(); */
      return TRUE;
    }
  }
  if (st->b >= st->max_b) {
    FlushBuffer(st);
    /*    if (savefile)
          save_to_file();*/
    return TRUE;
  }
  prod = 1.0;
  total = 0.0;
  bindex = 0;
  x = st->start_x;
  r = (st->forcing[bindex]) ? st->b : st->a;
#ifdef MAPS
  findex = 0;
  map = Maps[st->Forcing[findex]];
#endif
  for (i=0;i<st->settle;i++) {     /* Here's where we let the thing */
    x = st->map (x, r);  /* "settle down". There is usually */
    if (++bindex >= st->maxindex) { /* some initial "noise" in the */
      bindex = 0;    /* iterations. How can we optimize */
      if (st->Rflag)      /* the value of settle ??? */
        setforcing(st);
    }
    r = (st->forcing[bindex]) ? st->b : st->a;
#ifdef MAPS
    if (++findex >= funcmaxindex)
      findex = 0;
    map = Maps[st->Forcing[findex]];
#endif
  }
#ifdef MAPS
  deriv = Derivs[st->Forcing[findex]];
#endif
  if (st->useprod) {      /* using log(a*b) */
    for (i=0;i<st->dwell;i++) {
      x = st->map (x, r);
      dx = st->deriv (x, r); /* ABS is a macro, so don't be fancy */
      dx = ABS(dx);
      if (dx == 0.0) /* log(0) is nasty so break out. */
        {
          i++;
          break;
        }
      prod *= dx;
      /* we need to prevent overflow and underflow */
      if ((prod > 1.0e12) || (prod < 1.0e-12)) {
        total += log(prod);
        prod = 1.0;
      }
      if (++bindex >= st->maxindex) {
        bindex = 0;
        if (st->Rflag)
          setforcing(st);
      }
      r = (st->forcing[bindex]) ? st->b : st->a;
#ifdef MAPS
      if (++findex >= funcmaxindex)
        findex = 0;
      map = Maps[st->Forcing[findex]];
      deriv = Derivs[st->Forcing[findex]];
#endif
    }
    total += log(prod);
    st->lyapunov = (total * M_LOG2E) / (double)i;   
  }
  else {        /* use log(a) + log(b) */
    for (i=0;i<st->dwell;i++) {
      x = st->map (x, r);
      dx = st->deriv (x, r); /* ABS is a macro, so don't be fancy */
      dx = ABS(dx);
      if (x == 0.0)  /* log(0) check */
        {
          i++;
          break;
        }
      total += log(dx);
      if (++bindex >= st->maxindex) {
        bindex = 0;
        if (st->Rflag)
          setforcing(st);
      }
      r = (st->forcing[bindex]) ? st->b : st->a;
#ifdef MAPS
      if (++findex >= funcmaxindex)
        findex = 0;
      map = Maps[st->Forcing[findex]];
      deriv = Derivs[st->Forcing[findex]];
#endif
    }
    st->lyapunov = (total * M_LOG2E) / (double)i;
  }

  if (sendpoint(st, st->lyapunov) == TRUE)
    return FALSE;
  else {
    FlushBuffer(st);
    /*    if (savefile)
          save_to_file();*/
    return TRUE;
  }
}

static double
logistic(double x, double r)        /* the familiar logistic map */
{
  return(r * x * (1.0 - x));
}

static double
dlogistic(double x, double r)       /* the derivative of logistic map */
{
  return(r - (2.0 * r * x));
}

static double
circle(double x, double r)        /* sin() hump or sorta like the circle map */
{
  return(r * sin(M_PI * x));
}

static double
dcircle(double x, double r)       /* derivative of the "sin() hump" */
{
  return(r * M_PI * cos(M_PI * x));
}

static double
leftlog(double x, double r)       /* left skewed logistic */
{
  double d;

  d = 1.0 - x;
  return(r * x * d * d);
}

static double
dleftlog(double x, double r)    /* derivative of the left skewed logistic */
{
  return(r * (1.0 - (4.0 * x) + (3.0 * x * x)));
}

static double
rightlog(double x, double r)    /* right skewed logistic */
{
  return(r * x * x * (1.0 - x));
}

static double
drightlog(double x, double r)    /* derivative of the right skewed logistic */
{
  return(r * ((2.0 * x) - (3.0 * x * x)));
}

static double
doublelog(double x, double r)    /* double logistic */
{
  double d;

  d = 1.0 - x;
  return(r * x * x * d * d);
}

static double
ddoublelog(double x, double r)   /* derivative of the double logistic */
{
  double d;

  d = x * x;
  return(r * ((2.0 * x) - (6.0 * d) + (4.0 * x * d)));
}

static void
init_data(struct state *st)
{
  st->numcolors = get_integer_resource (st->dpy, "colors", "Integer");
  if (st->numcolors < 2)
    st->numcolors = 2;
  if (st->numcolors > st->maxcolor)
    st->numcolors = st->maxcolor;
  st->numfreecols = st->numcolors - st->mincolindex;
  st->lowrange = st->mincolindex - st->startcolor;
  st->a_inc = st->a_range / (double)st->width;
  st->b_inc = st->b_range / (double)st->height;
  st->point.x = -1;
  st->point.y = 0;
  st->a = /*st->rubber_data.p_min = */st->min_a;
  st->b = /*st->rubber_data.q_min = */st->min_b;
/*  st->rubber_data.p_max = st->max_a;
  st->rubber_data.q_max = st->max_b;*/
  if (st->show)
    show_defaults(st);
  InitBuffer(st);
}

#if 0
static void
hls2rgb(int hue_light_sat[3],
        int rgb[3])             /*      Each in range [0..65535]        */
{
  unsigned short r, g, b;
  hsv_to_rgb((int) (hue_light_sat[0] / 10),             /* 0-3600 -> 0-360 */
             (int) ((hue_light_sat[2]/1000.0) * 64435), /* 0-1000 -> 0-65535 */
             (int) ((hue_light_sat[1]/1000.0) * 64435), /* 0-1000 -> 0-65535 */
             &r, &g, &b);
  rgb[0] = r;
  rgb[1] = g;
  rgb[2] = b;
}
#endif /* 0 */


static void
init_color(struct state *st)
{
  int i;
  if (st->ncolors)
    free_colors (st->screen, st->cmap, st->colors, st->ncolors);
  st->ncolors = st->maxcolor;
  make_smooth_colormap(st->screen, st->visual, st->cmap,
                       st->colors, &st->ncolors, True, NULL, True);

  for (i = 0; i < st->maxcolor; i++) {
    if (! st->Data_GC[i]) {
      XGCValues gcv;
      gcv.background = BlackPixelOfScreen(st->screen);
      st->Data_GC[i] = XCreateGC(st->dpy, st->canvas, GCBackground, &gcv);
    }
    XSetForeground(st->dpy, st->Data_GC[i],
                   st->colors[((int) ((i / ((float)st->maxcolor)) *
                                      st->ncolors))].pixel);
  }
}


static void
parseargs(struct state *st)
{
  int i;
  int bindex=0, findex;
  char *s, *ch;

  st->map = Maps[0];
  st->deriv = Derivs[0];
  st->maxexp=st->minlyap; st->minexp= -1.0 * st->minlyap;

  st->mincolindex = get_integer_resource(st->dpy, "minColor", "Integer");
  st->dwell = get_integer_resource(st->dpy, "dwell", "Integer");
#ifdef MAPS
  {
    char *optarg = get_string_resource(st->dpy, "function", "String");
    funcmaxindex = strlen(optarg);
    if (funcmaxindex > FUNCMAXINDEX)
      usage();
    ch = optarg;
    st->Force++;
    for (findex=0;findex<funcmaxindex;findex++) {
      st->Forcing[findex] = (int)(*ch++ - '0');;
      if (st->Forcing[findex] >= NUMMAPS)
        usage();
    }
  }
#endif
  if (get_boolean_resource(st->dpy, "useLog", "Boolean"))
    st->useprod=0;

  st->minlyap=ABS(get_float_resource(st->dpy, "colorExponent", "Float"));
  st->maxexp=st->minlyap;
  st->minexp= -1.0 * st->minlyap;

  st->color_offset = get_integer_resource(st->dpy, "colorOffset", "Integer");

  st->maxcolor=ABS(get_integer_resource(st->dpy, "maxColor", "Integer"));
  if ((st->maxcolor - st->startcolor) <= 0)
    st->startcolor = get_pixel_resource(st->dpy, st->cmap,
                                        "background", "Background");
  if ((st->maxcolor - st->mincolindex) <= 0) {
    st->mincolindex = 1;
    st->color_offset = 0;
  }

  s = get_string_resource(st->dpy, "randomForce", "Float");
  if (s && *s) {
    st->prob=atof(s); st->Rflag++; setforcing(st);
  }

  st->settle = get_integer_resource(st->dpy, "settle", "Integer");

#if 0
  s = get_string_resource(st->dpy, "minA", "Float");
  if (s && *s) {
    st->min_a = atof(s);
    st->aflag++;
  }
  
  s = get_string_resource(st->dpy, "minB", "Float");
  if (s && *s) {
    st->min_b=atof(s); st->bflag++;
  }
#else
  st->min_a = get_float_resource (st->dpy, "minA", "Float");
  st->aflag++;
  st->min_b = get_float_resource (st->dpy, "minB", "Float");
  st->bflag++;
#endif

  
  st->numwheels = get_integer_resource(st->dpy, "wheels", "Integer");

  s = get_string_resource(st->dpy, "forcingFunction", "String");
  if (s && *s) {
    st->maxindex = strlen(s);
    if (st->maxindex > MAXINDEX)
      usage(st);
    ch = s;
    st->force++;
    while (bindex < st->maxindex) {
      if (*ch == 'a')
        st->forcing[bindex++] = 0;
      else if (*ch == 'b')
        st->forcing[bindex++] = 1;
      else
        usage(st);
      ch++;
    }
  }

  s = get_string_resource(st->dpy, "bRange", "Float");
  if (s && *s) {
    st->b_range = atof(s);
    st->hflag++;
  }

  st->start_x = get_float_resource(st->dpy, "startX", "Float");

  s = get_string_resource(st->dpy, "mapIndex", "Integer");
  if (s && *s) {
    st->mapindex=atoi(s);
    if ((st->mapindex >= NUMMAPS) || (st->mapindex < 0))
      usage(st);
    st->map = Maps[st->mapindex];
    st->deriv = Derivs[st->mapindex];
    if (!st->aflag)
      st->min_a = amins[st->mapindex];
    if (!st->wflag)
      st->a_range = aranges[st->mapindex];
    if (!st->bflag)
      st->min_b = bmins[st->mapindex];
    if (!st->hflag)
      st->b_range = branges[st->mapindex];
    if (!st->Force)
      for (i=0;i<FUNCMAXINDEX;i++)
        st->Forcing[i] = st->mapindex;
  }

  st->outname = get_string_resource(st->dpy, "outputFile", "Integer");

  if (get_boolean_resource(st->dpy, "beNegative", "Boolean"))
    st->negative--;

  st->rgb_max = get_integer_resource(st->dpy, "rgbMax", "Integer");
  st->spinlength = get_integer_resource(st->dpy, "spinLength", "Integer");
  st->show = get_boolean_resource(st->dpy, "show", "Boolean");

  s = get_string_resource(st->dpy, "aRange", "Float");
  if (s && *s) {
    st->a_range = atof(s); st->wflag++;
  }

  st->max_a = st->min_a + st->a_range;
  st->max_b = st->min_b + st->b_range;

  st->a_minimums[0] = st->min_a; st->b_minimums[0] = st->min_b;
  st->a_maximums[0] = st->max_a; st->b_maximums[0] = st->max_b;

  if (st->Force)
    if (st->maxindex == st->funcmaxindex)
      for (findex=0;findex<st->funcmaxindex;findex++)
        check_params(st, st->Forcing[findex],st->forcing[findex]);
    else
      fprintf(stderr, "Warning! Unable to check parameters\n");
  else
    check_params(st, st->mapindex,2);
}

static void
check_params(struct state *st, int mapnum, int parnum)
{

  if (parnum != 1) {
    if ((st->max_a > pmaxs[mapnum]) || (st->min_a < pmins[mapnum])) {
      fprintf(stderr, "Warning! Parameter 'a' out of range.\n");
      fprintf(stderr, "You have requested a range of (%f,%f).\n",
              st->min_a,st->max_a);
      fprintf(stderr, "Valid range is (%f,%f).\n",
              pmins[mapnum],pmaxs[mapnum]);
    }
  }
  if (parnum != 0) {
    if ((st->max_b > pmaxs[mapnum]) || (st->min_b < pmins[mapnum])) {
      fprintf(stderr, "Warning! Parameter 'b' out of range.\n");
      fprintf(stderr, "You have requested a range of (%f,%f).\n",
              st->min_b,st->max_b);
      fprintf(stderr, "Valid range is (%f,%f).\n",
              pmins[mapnum],pmaxs[mapnum]);
    }
  }
}

static void
usage(struct state *st)
{
  fprintf(stderr,"lyap [-BLs][-W#][-H#][-a#][-b#][-w#][-h#][-x xstart]\n");
  fprintf(stderr,"\t[-M#][-S#][-D#][-f string][-r#][-O#][-C#][-c#][-m#]\n");
#ifdef MAPS
  fprintf(stderr,"\t[-F string]\n");
#endif
  fprintf(stderr,"\tWhere: -C# specifies the minimum color index\n");
  fprintf(stderr,"\t       -r# specifies the maxzimum rgb value\n");
  fprintf(stderr,"\t       -u displays this message\n");
  fprintf(stderr,"\t       -a# specifies the minimum horizontal parameter\n");
  fprintf(stderr,"\t       -b# specifies the minimum vertical parameter\n");
  fprintf(stderr,"\t       -w# specifies the horizontal parameter range\n");
  fprintf(stderr,"\t       -h# specifies the vertical parameter range\n");
  fprintf(stderr,"\t       -D# specifies the dwell\n");
  fprintf(stderr,"\t       -S# specifies the settle\n");
  fprintf(stderr,"\t       -H# specifies the initial window height\n");
  fprintf(stderr,"\t       -W# specifies the initial window width\n");
  fprintf(stderr,"\t       -O# specifies the color offset\n");
  fprintf(stderr,"\t       -c# specifies the desired color wheel\n");
  fprintf(stderr,"\t       -m# specifies the desired map (0-4)\n");
  fprintf(stderr,"\t       -f aabbb specifies a forcing function of 00111\n");
#ifdef MAPS
  fprintf(stderr,"\t       -F 00111 specifies the function forcing function\n");
#endif
  fprintf(stderr,"\t       -L indicates use log(x)+log(y) rather than log(xy)\n");
  fprintf(stderr,"\tDuring display :\n");
  fprintf(stderr,"\t       Use the mouse to zoom in on an area\n");
  fprintf(stderr,"\t       e or E recalculates color indices\n");
  fprintf(stderr,"\t       f or F saves exponents to a file\n");
  fprintf(stderr,"\t       KJmn increase/decrease minimum negative exponent\n");
  fprintf(stderr,"\t       r or R redraws\n");
  fprintf(stderr,"\t       s or S spins the colorwheel\n");
  fprintf(stderr,"\t       w or W changes the color wheel\n");
  fprintf(stderr,"\t       x or X clears the window\n");
  fprintf(stderr,"\t       q or Q exits\n");
  exit(1);
}

static void
Cycle_frames(struct state *st)
{
  int i;
  for (i=0;i<=st->maxframe;i++)
    redraw(st, st->exponents[i], st->expind[i], 1);
}

#if 0
static void
Spin(struct state *st)
{
  int i, j;
  long tmpxcolor;

  if (!mono_p) {
    for (j=0;j<st->spinlength;j++) {
      tmpxcolor = st->Colors[st->mincolindex].pixel;
      for (i=st->mincolindex;i<st->numcolors-1;i++)
        st->Colors[i].pixel = st->Colors[i+1].pixel;
      st->Colors[st->numcolors-1].pixel = tmpxcolor;
      XStoreColors(st->dpy, st->cmap, st->Colors, st->numcolors);
    }
    for (j=0;j<st->spinlength;j++) {
      tmpxcolor = st->Colors[st->numcolors-1].pixel;
      for (i=st->numcolors-1;i>st->mincolindex;i--)
        st->Colors[i].pixel = st->Colors[i-1].pixel;
      st->Colors[st->mincolindex].pixel = tmpxcolor;
      XStoreColors(st->dpy, st->cmap, st->Colors, st->numcolors);
    }
  }
}
#endif

static Bool
Getkey(struct state *st, XKeyEvent *event)
{
  unsigned char key;
  int i;
  if (XLookupString(event, (char *)&key, sizeof(key), (KeySym *)0,
                    (XComposeStatus *) 0) > 0)

    if (st->reset_countdown)
      st->reset_countdown = st->linger;

    switch (key) {
    case '<': st->dwell /= 2; if (st->dwell < 1) st->dwell = 1; return True;
    case '>': st->dwell *= 2; return True;
    case '[': st->settle /= 2; if (st->settle < 1) st->settle = 1; return True;
    case ']': st->settle *= 2; return True;
    case 'd': go_down(st); return True;
    case 'D': FlushBuffer(st); return True;
    case 'e':
    case 'E': FlushBuffer(st);
      st->dorecalc = (!st->dorecalc);
      if (st->dorecalc)
        recalc(st);
      else {
        st->maxexp = st->minlyap; st->minexp = -1.0 * st->minlyap;
      }
      redraw(st, st->exponents[st->frame], st->expind[st->frame], 1);
      return True;
    case 'f':
      /*  case 'F': save_to_file(); return True;*/
    case 'i': if (st->stripe_interval > 0) {
        st->stripe_interval--;
        if (!mono_p) {
          init_color(st);
        }
      }
      return True;
    case 'I': st->stripe_interval++;
      if (!mono_p) {
        init_color(st);
      }
      return True;
    case 'K': if (st->minlyap > 0.05)
        st->minlyap -= 0.05;
      return True;
    case 'J': st->minlyap += 0.05;
      return True;
    case 'm': st->mapindex++;
      if (st->mapindex >= NUMMAPS)
        st->mapindex=0;
      st->map = Maps[st->mapindex];
      st->deriv = Derivs[st->mapindex];
      if (!st->aflag)
        st->min_a = amins[st->mapindex];
      if (!st->wflag)
        st->a_range = aranges[st->mapindex];
      if (!st->bflag)
        st->min_b = bmins[st->mapindex];
      if (!st->hflag)
        st->b_range = branges[st->mapindex];
      if (!st->Force)
        for (i=0;i<FUNCMAXINDEX;i++)
          st->Forcing[i] = st->mapindex;
      st->max_a = st->min_a + st->a_range;
      st->max_b = st->min_b + st->b_range;
      st->a_minimums[0] = st->min_a; st->b_minimums[0] = st->min_b;
      st->a_maximums[0] = st->max_a; st->b_maximums[0] = st->max_b;
      st->a_inc = st->a_range / (double)st->width;
      st->b_inc = st->b_range / (double)st->height;
      st->point.x = -1;
      st->point.y = 0;
      st->a = /*st->rubber_data.p_min = */st->min_a;
      st->b = /*st->rubber_data.q_min = */st->min_b;
/*      st->rubber_data.p_max = st->max_a;
      st->rubber_data.q_max = st->max_b;*/
      Clear(st);
      return True;
    case 'M': if (st->minlyap > 0.005)
        st->minlyap -= 0.005;
      return True;
    case 'N': st->minlyap += 0.005;
      return True;
    case 'p':
    case 'P': st->negative = (!st->negative);
      FlushBuffer(st); redraw(st, st->exponents[st->frame], 
                              st->expind[st->frame], 1);
      return True;
    case 'r': FlushBuffer(st); redraw(st, st->exponents[st->frame], 
                                      st->expind[st->frame], 1);
      return True;
    case 'R': FlushBuffer(st); Redraw(st); return True;
    case 's':
      st->spinlength=st->spinlength/2;
#if 0
    case 'S': if (!mono_p)
        Spin(st);
      st->spinlength=st->spinlength*2; return True;
#endif
    case 'u': go_back(st); return True;
    case 'U': go_init(st); return True;
    case 'v':
    case 'V': print_values(st); return True;
    case 'W': if (st->numwheels < MAXWHEELS)
        st->numwheels++;
      else
        st->numwheels = 0;
      if (!mono_p) {
        init_color(st);
      }
      return True;
    case 'w': if (st->numwheels > 0)
        st->numwheels--;
      else
        st->numwheels = MAXWHEELS;
      if (!mono_p) {
        init_color(st);
      }
      return True;
    case 'x': Clear(st); return True;
    case 'X': Destroy_frame(st); return True;
    case 'z': Cycle_frames(st); redraw(st, st->exponents[st->frame], 
                                       st->expind[st->frame], 1);
      return True;
#if 0
    case 'Z': while (!XPending(st->dpy)) Cycle_frames(st);
      redraw(st, st->exponents[st->frame], st->expind[st->frame], 1); 
      return True;
#endif
    case 'q':
    case 'Q': exit(0); return True;
    case '?':
    case 'h':
    case 'H': print_help(st); return True;
    default:  return False;
    }

  return False;
}

/* Here's where we index into a color map. After the Lyapunov exponent is
 * calculated, it is used to determine what color to use for that point.  I
 * suppose there are a lot of ways to do this. I used the following : if it's
 * non-negative then there's a reserved area at the lower range of the color
 * map that i index into. The ratio of some "minimum exponent value" and the
 * calculated value is used as a ratio of how high to index into this reserved
 * range. Usually these colors are dark red (see init_color).  If the exponent
 * is negative, the same ratio (expo/minlyap) is used to index into the
 * remaining portion of the colormap (which is usually some light shades of
 * color or a rainbow wheel). The coloring scheme can actually make a great
 * deal of difference in the quality of the picture. Different colormaps bring
 * out different details of the dynamics while different indexing algorithms
 * also greatly effect what details are seen. Play around with this.
 */
static int
sendpoint(struct state *st, double expo)
{
  double tmpexpo;

  if (st->maxcolor > MAXCOLOR)
    abort();

#if 0
  /* The relationship st->minexp <= expo <= maxexp should always be true. This
     test enforces that. But maybe not enforcing it makes better pictures. */
  if (expo < st->minexp)
    expo = st->minexp;
  else if (expo > maxexp)
    expo = maxexp;
#endif

  st->point.x++;
  tmpexpo = (st->negative) ? expo : -1.0 * expo;
  if (tmpexpo > 0) {
    if (!mono_p) {
      st->sendpoint_index = (int)(tmpexpo*st->lowrange/st->maxexp);
      st->sendpoint_index = ((st->sendpoint_index % st->lowrange) + 
                             st->startcolor);
    }
    else
      st->sendpoint_index = 0;
  }
  else {
    if (!mono_p) {
      st->sendpoint_index = (int)(tmpexpo*st->numfreecols/st->minexp);
      st->sendpoint_index = ((st->sendpoint_index % st->numfreecols)
                             + st->mincolindex);
    }
    else
      st->sendpoint_index = 1;
  }
  BufferPoint(st, st->sendpoint_index, st->point.x, st->point.y);
  if (st->save) {
    if (st->frame > MAXFRAMES)
      abort();
    st->exponents[st->frame][st->expind[st->frame]++] = expo;
  }
  if (st->point.x >= st->width) {
    st->point.y++;
    st->point.x = 0;
    if (st->save) {
      st->b += st->b_inc;
      st->a = st->min_a;
    }
    if (st->point.y >= st->height)
      return FALSE;
    else
      return TRUE;
  }
  return TRUE;
}


static void
resize(struct state *st)
{
  Window r;
  int n, x, y;
  unsigned int bw, d, new_w, new_h;

  XGetGeometry(st->dpy,st->canvas,&r,&x,&y,&new_w,&new_h,&bw,&d);
  if ((new_w == st->width) && (new_h == st->height))
    return;
  st->width = new_w; st->height = new_h;
  XClearWindow(st->dpy, st->canvas);
#ifdef BACKING_PIXMAP
  if (st->pixmap)
    XFreePixmap(st->dpy, st->pixmap);
  st->pixmap = XCreatePixmap(st->dpy, st->canvas, st->width, st->height, d);
#endif
  st->a_inc = st->a_range / (double)st->width;
  st->b_inc = st->b_range / (double)st->height;
  st->point.x = -1;
  st->point.y = 0;
  st->run = 1;
  st->a = /*st->rubber_data.p_min = */st->min_a;
  st->b = /*st->rubber_data.q_min = */st->min_b;
/*  st->rubber_data.p_max = st->max_a;
  st->rubber_data.q_max = st->max_b;*/
  freemem(st);
  setupmem(st);
  for (n=0;n<MAXFRAMES;n++)
    if ((n <= st->maxframe) && (n != st->frame))
      st->resized[n] = 1;
  InitBuffer(st);
  Clear(st);
  Redraw(st);
}

static void
redraw(struct state *st, double *exparray, int index, int cont)
{
  int i, x_sav, y_sav;

  x_sav = st->point.x;
  y_sav = st->point.y;

  st->point.x = -1;
  st->point.y = 0;

  st->save=0;
  for (i=0;i<index;i++)
    sendpoint(st, exparray[i]);
  st->save=1;

  if (cont) {
    st->point.x = x_sav;
    st->point.y = y_sav;
  }
  else {
    st->a = st->point.x * st->a_inc + st->min_a;
    st->b = st->point.y * st->b_inc + st->min_b;
  }
  FlushBuffer(st);
}

static void
Redraw(struct state *st)
{
  FlushBuffer(st);
  st->point.x = -1;
  st->point.y = 0;
  st->run = 1;
  st->a = st->min_a;
  st->b = st->min_b;
  st->expind[st->frame] = 0;
  st->resized[st->frame] = 0;
}

static void
recalc(struct state *st)
{
  int i;

  st->minexp = st->maxexp = 0.0;
  for (i=0;i<st->expind[st->frame];i++) {
    if (st->exponents[st->frame][i] < st->minexp)
      st->minexp = st->exponents[st->frame][i];
    if (st->exponents[st->frame][i] > st->maxexp)
      st->maxexp = st->exponents[st->frame][i];
  }
}

static void
Clear(struct state *st)
{
  XClearWindow(st->dpy, st->canvas);
#ifdef BACKING_PIXMAP
  XCopyArea(st->dpy, st->canvas, st->pixmap, st->Data_GC[0],
            0, 0, st->width, st->height, 0, 0);
#endif
  InitBuffer(st);
}

static void
show_defaults(struct state *st)
{

  printf("Width=%d  Height=%d  numcolors=%d  settle=%d  dwell=%d\n",
         st->width,st->height,st->numcolors,st->settle,st->dwell);
  printf("min_a=%f  a_range=%f  max_a=%f\n", st->min_a,st->a_range,st->max_a);
  printf("min_b=%f  b_range=%f  max_b=%f\n", st->min_b,st->b_range,st->max_b);
  printf("minlyap=%f  minexp=%f  maxexp=%f\n", st->minlyap,st->minexp,
         st->maxexp);
  exit(0);
}

#if 0
static void
CreateXorGC(struct state *st)
{
  XGCValues values;

  values.foreground = st->foreground;
  values.function = GXxor;
  st->RubberGC = XCreateGC(st->dpy, st->canvas,
                           GCForeground | GCFunction, &values);
}

static void
StartRubberBand(struct state *st, image_data_t *data, XEvent *event)
{
  XPoint corners[5];

  st->nostart = 0;
  data->rubber_band.last_x = data->rubber_band.start_x = event->xbutton.x;
  data->rubber_band.last_y = data->rubber_band.start_y = event->xbutton.y;
  SetupCorners(corners, data);
  XDrawLines(st->dpy, st->canvas, st->RubberGC,
             corners, sizeof(corners) / sizeof(corners[0]), CoordModeOrigin);
}

static void
SetupCorners(XPoint *corners, image_data_t *data)
{
  corners[0].x = data->rubber_band.start_x;
  corners[0].y = data->rubber_band.start_y;
  corners[1].x = data->rubber_band.start_x;
  corners[1].y = data->rubber_band.last_y;
  corners[2].x = data->rubber_band.last_x;
  corners[2].y = data->rubber_band.last_y;
  corners[3].x = data->rubber_band.last_x;
  corners[3].y = data->rubber_band.start_y;
  corners[4] = corners[0];
}

static void
TrackRubberBand(struct state *st, image_data_t *data, XEvent *event)
{
  XPoint corners[5];
  int xdiff, ydiff;

  if (st->nostart)
    return;
  SetupCorners(corners, data);
  XDrawLines(st->dpy, st->canvas, st->RubberGC,
             corners, sizeof(corners) / sizeof(corners[0]), CoordModeOrigin);
  ydiff = event->xbutton.y - data->rubber_band.start_y;
  xdiff = event->xbutton.x - data->rubber_band.start_x;
  data->rubber_band.last_x = data->rubber_band.start_x + xdiff;
  data->rubber_band.last_y = data->rubber_band.start_y + ydiff;
  if (data->rubber_band.last_y < data->rubber_band.start_y ||
      data->rubber_band.last_x < data->rubber_band.start_x)
    {
      data->rubber_band.last_y = data->rubber_band.start_y;
      data->rubber_band.last_x = data->rubber_band.start_x;
    }
  SetupCorners(corners, data);
  XDrawLines(st->dpy, st->canvas, st->RubberGC,
             corners, sizeof(corners) / sizeof(corners[0]), CoordModeOrigin);
}

static void
EndRubberBand(struct state *st, image_data_t *data, XEvent *event)
{
  XPoint corners[5];
  XPoint top, bot;
  double delta, diff;

  st->nostart = 1;
  SetupCorners(corners, data);
  XDrawLines(st->dpy, st->canvas, st->RubberGC,
             corners, sizeof(corners) / sizeof(corners[0]), CoordModeOrigin);
  if (data->rubber_band.start_x >= data->rubber_band.last_x ||
      data->rubber_band.start_y >= data->rubber_band.last_y)
    return;
  top.x = data->rubber_band.start_x;
  bot.x = data->rubber_band.last_x;
  top.y = data->rubber_band.start_y;
  bot.y = data->rubber_band.last_y;
  diff = data->q_max - data->q_min;
  delta = (double)top.y / (double)st->height;
  data->q_min += diff * delta;
  delta = (double)(st->height - bot.y) / (double)st->height;
  data->q_max -= diff * delta;
  diff = data->p_max - data->p_min;
  delta = (double)top.x / (double)st->width;
  data->p_min += diff * delta;
  delta = (double)(st->width - bot.x) / (double)st->width;
  data->p_max -= diff * delta;
  set_new_params(st, data);
}

static void
set_new_params(struct state *st, image_data_t *data)
{
  st->frame = (st->maxframe + 1) % MAXFRAMES;
  if (st->frame > st->maxframe)
    st->maxframe = st->frame;
  st->a_range = data->p_max - data->p_min;
  st->b_range = data->q_max - data->q_min;
  st->a_minimums[st->frame] = st->min_a = data->p_min;
  st->b_minimums[st->frame] = st->min_b = data->q_min;
  st->a_inc = st->a_range / (double)st->width;
  st->b_inc = st->b_range / (double)st->height;
  st->point.x = -1;
  st->point.y = 0;
  st->run = 1;
  st->a = st->min_a;
  st->b = st->min_b;
  st->a_maximums[st->frame] = st->max_a = data->p_max;
  st->b_maximums[st->frame] = st->max_b = data->q_max;
  st->expind[st->frame] = 0;
  Clear(st);
}
#endif

static void
go_down(struct state *st)
{
  st->frame++;
  if (st->frame > st->maxframe)
    st->frame = 0;
  jumpwin(st);
}

static void
go_back(struct state *st)
{
  st->frame--;
  if (st->frame < 0)
    st->frame = st->maxframe;
  jumpwin(st);
}

static void
jumpwin(struct state *st)
{
  /*st->rubber_data.p_min =*/ st->min_a = st->a_minimums[st->frame];
  /*st->rubber_data.q_min =*/ st->min_b = st->b_minimums[st->frame];
  /*st->rubber_data.p_max =*/ st->max_a = st->a_maximums[st->frame];
  /*st->rubber_data.q_max =*/ st->max_b = st->b_maximums[st->frame];
  st->a_range = st->max_a - st->min_a;
  st->b_range = st->max_b - st->min_b;
  st->a_inc = st->a_range / (double)st->width;
  st->b_inc = st->b_range / (double)st->height;
  st->point.x = -1;
  st->point.y = 0;
  st->a = st->min_a;
  st->b = st->min_b;
  Clear(st);
  if (st->resized[st->frame])
    Redraw(st);
  else
    redraw(st, st->exponents[st->frame], st->expind[st->frame], 0);
}

static void
go_init(struct state *st)
{
  st->frame = 0;
  jumpwin(st);
}

static void
Destroy_frame(struct state *st)
{
  int i;

  for (i=st->frame; i<st->maxframe; i++) {
    st->exponents[st->frame] = st->exponents[st->frame+1];
    st->expind[st->frame] = st->expind[st->frame+1];
    st->a_minimums[st->frame] = st->a_minimums[st->frame+1];
    st->b_minimums[st->frame] = st->b_minimums[st->frame+1];
    st->a_maximums[st->frame] = st->a_maximums[st->frame+1];
    st->b_maximums[st->frame] = st->b_maximums[st->frame+1];
  }
  st->maxframe--;
  go_back(st);
}

static void
InitBuffer(struct state *st)
{
  int i;

  for (i = 0 ; i < st->maxcolor; ++i)
    st->Points.npoints[i] = 0;
}

static void
BufferPoint(struct state *st, int color, int x, int y)
{
  if (st->maxcolor > MAXCOLOR)
    abort();

  /* Guard against bogus color values. Shouldn't be necessary but paranoia
     is good. */
  if (color < 0)
    color = 0;
  else if (color >= st->maxcolor)
    color = st->maxcolor - 1;

  if (st->Points.npoints[color] == MAXPOINTS)
    {
      XDrawPoints(st->dpy, st->canvas, st->Data_GC[color],
                  st->Points.data[color], st->Points.npoints[color], 
                  CoordModeOrigin);
#ifdef BACKING_PIXMAP
      XDrawPoints(st->dpy, st->pixmap, st->Data_GC[color],
                  st->Points.data[color], st->Points.npoints[color], 
                  CoordModeOrigin);
#endif
      st->Points.npoints[color] = 0;
    }
  st->Points.data[color][st->Points.npoints[color]].x = x;
  st->Points.data[color][st->Points.npoints[color]].y = y;
  ++st->Points.npoints[color];
}

static void
FlushBuffer(struct state *st)
{
  int color;

  for (color = 0; color < st->maxcolor; ++color)
    if (st->Points.npoints[color])
      {
        XDrawPoints(st->dpy, st->canvas, st->Data_GC[color],
                    st->Points.data[color], st->Points.npoints[color],
                    CoordModeOrigin);
#ifdef BACKING_PIXMAP
        XDrawPoints(st->dpy, st->pixmap, st->Data_GC[color],
                    st->Points.data[color], st->Points.npoints[color],
                    CoordModeOrigin);
#endif
        st->Points.npoints[color] = 0;
      }
}

static void
print_help(struct state *st)
{
  printf("During run-time, interactive control can be exerted via : \n");
  printf("Mouse buttons allow rubber-banding of a zoom box\n");
  printf("< halves the 'dwell', > doubles the 'dwell'\n");
  printf("[ halves the 'settle', ] doubles the 'settle'\n");
  printf("D flushes the drawing buffer\n");
  printf("e or E recalculates color indices\n");
  printf("f or F saves exponents to a file\n");
  printf("h or H or ? displays this message\n");
  printf("i decrements, I increments the stripe interval\n");
  printf("KJMN increase/decrease minimum negative exponent\n");
  printf("m increments the map index, changing maps\n");
  printf("p or P reverses the colormap for negative/positive exponents\n");
  printf("r redraws without recalculating\n");
  printf("R redraws, recalculating with new dwell and settle values\n");
  printf("s or S spins the colorwheel\n");
  printf("u pops back up to the last zoom\n");
  printf("U pops back up to the first picture\n");
  printf("v or V displays the values of various settings\n");
  printf("w decrements, W increments the color wheel index\n");
  printf("x or X clears the window\n");
  printf("q or Q exits\n");
}

static void
print_values(struct state *st)
{
  int i;
  printf("\nminlyap=%f minexp=%f maxexp=%f\n",
         st->minlyap,st->minexp, st->maxexp);
  printf("width=%d height=%d\n",st->width,st->height);
  printf("settle=%d  dwell=%d st->start_x=%f\n",
         st->settle,st->dwell, st->start_x);
  printf("min_a=%f  a_rng=%f        max_a=%f\n",
         st->min_a,st->a_range,st->max_a);
  printf("min_b=%f  b_rng=%f        max_b=%f\n",
         st->min_b,st->b_range,st->max_b);
  if (st->Rflag)
    printf("pseudo-random forcing\n");
  else if (st->force) {
    printf("periodic forcing=");
    for (i=0;i<st->maxindex;i++)
      printf("%d",st->forcing[i]);
    printf("\n");
  }
  else
    printf("periodic forcing=01\n");
  if (st->Force) {
    printf("function forcing=");
    for (i=0;i<st->funcmaxindex;i++) {
      printf("%d",st->Forcing[i]);
    }
    printf("\n");
  }
  printf("numcolors=%d\n",st->numcolors-1);
}

static void
freemem(struct state *st)
{
  int i;
  for (i=0;i<MAXFRAMES;i++)
    free(st->exponents[i]);
}

static void
setupmem(struct state *st)
{
  int i;
  for (i=0;i<MAXFRAMES;i++) {
    if((st->exponents[i]=
        (double *)malloc(sizeof(double)*st->width*(st->height+1)))==NULL){
      fprintf(stderr,"Error malloc'ing exponent array.\n");
      exit(-1);
    }
  }
}

static void
setforcing(struct state *st)
{
  int i;
  for (i=0;i<MAXINDEX;i++)
    st->forcing[i] = (random() > st->prob) ? 0 : 1;
}

/****************************************************************************/

static void
do_defaults (struct state *st)
{
  int i;

  memset (st->expind,  0, sizeof(st->expind));
  memset (st->resized, 0, sizeof(st->resized));

  st->aflag = 0;
  st->bflag = 0;
  st->hflag = 0;
  st->wflag = 0;
  st->minexp = 0;
  st->mapindex = 0;

# ifdef SIXTEEN_COLORS
  st->maxcolor=16;
  st->startcolor=0;
  st->color_offset=0;
  st->mincolindex=1;
  st->dwell=50;
  st->settle=25;
  st->xposition=128;
  st->yposition=128;
# else  /* !SIXTEEN_COLORS */
  st->maxcolor=256;
  st->startcolor=17;
  st->color_offset=96;
  st->mincolindex=33;
  st->dwell=100;
  st->settle=50;
# endif /* !SIXTEEN_COLORS */

  st->maxindex = MAXINDEX;
  st->funcmaxindex = FUNCMAXINDEX;
  st->min_a=2.0;
  st->min_b=2.0;
  st->a_range=2.0;
  st->b_range=2.0;
  st->minlyap=1.0;
  st->max_a=4.0;
  st->max_b=4.0;
  st->numcolors=16;
  st->prob=0.5;
  st->numwheels=MAXWHEELS;
  st->negative=1;
  st->rgb_max=65000;
  st->nostart=1;
  st->stripe_interval=7;
  st->save=1;
  st->useprod=1;
  st->spinlength=256;
  st->run=1;

  for (i = 0; i < countof(st->forcing); i++)
    st->forcing[i] = (i & 1) ? 1 : 0;
}

static void
do_preset (struct state *st, int builtin)
{
  char *ff = 0;
  switch (builtin) {
  case 0:
    st->min_a = 3.75; st->aflag++;
    st->min_b = 3.299999; st->bflag++;
    st->a_range = 0.05; st->wflag++;
    st->b_range = 0.05; st->hflag++;
    st->dwell = 200;
    st->settle = 100;
    ff = "abaabbaaabbb";
    break;

  case 1:
    st->min_a = 3.8; st->aflag++;
    st->min_b = 3.2; st->bflag++;
    st->b_range = .05; st->hflag++;
    st->a_range = .05; st->wflag++;
    ff = "bbbbbaaaaa";
    break;

  case 2:
    st->min_a =  3.4; st->aflag++;
    st->min_b =  3.04; st->bflag++;
    st->a_range =  .5; st->wflag++;
    st->b_range =  .5; st->hflag++;
    ff = "abbbbbbbbb";
    st->settle = 500;
    st->dwell = 1000;
    break;

  case 3:
    st->min_a = 3.5; st->aflag++;
    st->min_b = 3.0; st->bflag++;
    st->a_range = 0.2; st->wflag++;
    st->b_range = 0.2; st->hflag++;
    st->dwell = 600;
    st->settle = 300;
    ff = "aaabbbab";
    break;

  case 4:
    st->min_a = 3.55667; st->aflag++;
    st->min_b = 3.2; st->bflag++;
    st->b_range = .05; st->hflag++;
    st->a_range = .05; st->wflag++;
    ff = "bbbbbaaaaa";
    break;

  case 5:
    st->min_a = 3.79; st->aflag++;
    st->min_b = 3.22; st->bflag++;
    st->b_range = .02999; st->hflag++;
    st->a_range = .02999; st->wflag++;
    ff = "bbbbbaaaaa";
    break;

  case 6:
    st->min_a = 3.7999; st->aflag++;
    st->min_b = 3.299999; st->bflag++;
    st->a_range = 0.2; st->wflag++;
    st->b_range = 0.2; st->hflag++;
    st->dwell = 300;
    st->settle = 150;
    ff = "abaabbaaabbb";
    break;

  case 7:
    st->min_a = 3.89; st->aflag++;
    st->min_b = 3.22; st->bflag++;
    st->b_range = .028; st->hflag++;
    st->a_range = .02999; st->wflag++;
    ff = "bbbbbaaaaa";
    st->settle = 600;
    st->dwell = 1000;
    break;

  case 8:
    st->min_a = 3.2; st->aflag++;
    st->min_b = 3.7; st->bflag++;
    st->a_range = 0.05; st->wflag++;
    st->b_range = .005; st->hflag++;
    ff = "abbbbaa";
    break;

  case 9:
    ff = "aaaaaabbbbbb";
    st->mapindex = 1;
    st->dwell =  400;
    st->settle =  200;
    st->minlyap = st->maxexp = ABS(-0.85);
    st->minexp = -1.0 * st->minlyap;
    break;

  case 10:
    ff = "aaaaaabbbbbb";
    st->mapindex = 1;
    st->dwell =  400;
    st->settle = 200;
    st->minlyap = st->maxexp = ABS(-0.85);
    st->minexp = -1.0 * st->minlyap;
    break;

  case 11:
    st->mapindex = 1;
    st->dwell =  400;
    st->settle = 200;
    st->minlyap = st->maxexp = ABS(-0.85);
    st->minexp = -1.0 * st->minlyap;
    break;

  case 12:
    ff = "abbb";
    st->mapindex = 1;
    st->dwell =  400;
    st->settle = 200;
    st->minlyap = st->maxexp = ABS(-0.85);
    st->minexp = -1.0 * st->minlyap;
    break;

  case 13:
    ff = "abbabaab";
    st->mapindex = 1;
    st->dwell =  400;
    st->settle = 200;
    st->minlyap = st->maxexp = ABS(-0.85);
    st->minexp = -1.0 * st->minlyap;
    break;

  case 14:
    ff = "abbabaab";
    st->dwell =  800;
    st->settle = 200;
    st->minlyap = st->maxexp = ABS(-0.85);
    st->minexp = -1.0 * st->minlyap;
    /* ####  -x 0.05 */
    st->min_a = 3.91; st->aflag++;
    st->a_range =  0.0899999999; st->wflag++;
    st->min_b =  3.28; st->bflag++;
    st->b_range =  0.35; st->hflag++;
    break;

  case 15:
    ff = "aaaaaabbbbbb";
    st->dwell =  400;
    st->settle = 200;
    st->minlyap = st->maxexp = ABS(-0.85);
    st->minexp = -1.0 * st->minlyap;
    break;

  case 16:
    st->dwell =  400;
    st->settle = 200;
    st->minlyap = st->maxexp = ABS(-0.85);
    st->minexp = -1.0 * st->minlyap;
    break;

  case 17:
    ff = "abbb";
    st->dwell =  400;
    st->settle = 200;
    st->minlyap = st->maxexp = ABS(-0.85);
    st->minexp = -1.0 * st->minlyap;
    break;

  case 18:
    ff = "abbabaab";
    st->dwell =  400;
    st->settle = 200;
    st->minlyap = st->maxexp = ABS(-0.85);
    st->minexp = -1.0 * st->minlyap;
    break;

  case 19:
    st->mapindex = 2;
    ff = "aaaaaabbbbbb";
    st->dwell =  400;
    st->settle = 200;
    st->minlyap = st->maxexp = ABS(-0.85);
    st->minexp = -1.0 * st->minlyap;
    break;

  case 20:
    st->mapindex = 2;
    st->dwell =  400;
    st->settle = 200;
    st->minlyap = st->maxexp = ABS(-0.85);
    st->minexp = -1.0 * st->minlyap;
    break;

  case 21:
    st->mapindex = 2;
    ff = "abbb";
    st->dwell =  400;
    st->settle = 200;
    st->minlyap = st->maxexp = ABS(-0.85);
    st->minexp = -1.0 * st->minlyap;
    break;

  case 22:
    st->mapindex = 2;
    ff = "abbabaab";
    st->dwell =  400;
    st->settle = 200;
    st->minlyap = st->maxexp = ABS(-0.85);
    st->minexp = -1.0 * st->minlyap;
    break;

  default: 
    abort();
    break;
  }

  if (ff) {
    char *ch;
    int bindex = 0;
    st->maxindex = strlen(ff);
    if (st->maxindex > MAXINDEX)
      usage(st);
    ch = ff;
    st->force++;
    while (bindex < st->maxindex) {
      if (*ch == 'a')
        st->forcing[bindex++] = 0;
      else if (*ch == 'b')
        st->forcing[bindex++] = 1;
      else
        usage(st);
      ch++;
    }
  }
}


static void *
xlyap_init (Display *d, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XWindowAttributes xgwa;
  int builtin = -1;
  XGetWindowAttributes (d, window, &xgwa);
  st->dpy = d;
  st->width = xgwa.width;
  st->height = xgwa.height;
  st->visual = xgwa.visual;
  st->screen = xgwa.screen;
  st->cmap = xgwa.colormap;

  do_defaults(st);
  parseargs(st);

  if (get_boolean_resource(st->dpy, "randomize", "Boolean"))
    builtin = random() % NBUILTINS;
  else {
    char *s = get_string_resource(st->dpy, "builtin", "Integer");
    if (s && *s)
      builtin = atoi(s);
    if (s) free (s);
  }
    
  if (builtin >= 0)
    do_preset (st, builtin);

  st->background = BlackPixelOfScreen(st->screen);
  setupmem(st);
  init_data(st);
  if (!mono_p)
    st->foreground = st->startcolor;
  else
    st->foreground = WhitePixelOfScreen(st->screen);

  /*
   * Create the window to display the Lyapunov exponents
   */
  st->canvas = window;
  init_color(st);

#ifdef BACKING_PIXMAP
  st->pixmap = XCreatePixmap(st->dpy, window, st->width, st->height, 
                             xgwa.depth);
#endif
/*  st->rubber_data.band_cursor = XCreateFontCursor(st->dpy, XC_hand2);*/
/*  CreateXorGC(st);*/
  Clear(st);

  st->delay  = get_integer_resource(st->dpy, "delay", "Delay");
  st->linger = get_integer_resource(st->dpy, "linger", "Linger");
  if (st->linger < 1) st->linger = 1;

  return st;
}


static unsigned long
xlyap_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;

  if (!st->run && st->reset_countdown) {
    st->reset_countdown--;
    if (st->reset_countdown)
      return 1000000;
    else {
      do_defaults (st);
      do_preset (st, (random() % NBUILTINS));
      Clear (st);
      init_data(st);
      init_color(st);
      resize (st);
      st->frame = 0;
      st->run = 1;
    }
  }

  for (i = 0; i < 1000; i++)
    if (complyap(st) == TRUE)
      {
        st->run = 0;
        st->reset_countdown = st->linger;
        break;
      }
  return st->delay;
}

static void
xlyap_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  resize(st);
}

static Bool
xlyap_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;

  switch(event->type)
    {
    case KeyPress:
      if (Getkey(st, &event->xkey))
        return True;
      break;
#if 0
    case ButtonPress:
      StartRubberBand(st, &st->rubber_data, event);
      return True;
    case MotionNotify:
      TrackRubberBand(st, &st->rubber_data, event);
      return True;
    case ButtonRelease:
      EndRubberBand(st, &st->rubber_data, event);
      return True;
#endif
    default: 
      break;
    }

  if (screenhack_event_helper (dpy, window, event))
    {
      Clear(st);
      return True;
    }

  return False;
}

static void
xlyap_free (Display *dpy, Window window, void *closure)
{
  int i;
  struct state *st = (struct state *) closure;

  freemem (st);

#ifdef BACKING_PIXMAP
  XFreePixmap (st->dpy, st->pixmap);
#endif
/*  XFreeGC (st->dpy, st->RubberGC);*/
  for (i = 0; i < st->maxcolor; i++)
    XFreeGC (st->dpy, st->Data_GC[i]);

  free (st);
}


XSCREENSAVER_MODULE ("XLyap", xlyap)
