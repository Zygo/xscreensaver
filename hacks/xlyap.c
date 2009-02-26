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
 * by A. K. Dewdney who gives credit to Mario Markus of the Max Planck Institute
 * for its creation. Additional information and ideas were gleaned from the
 * discussion on alt.fractals involving Stephen Hall, Ed Kubaitis, Dave Platt
 * and Baback Moghaddam. Assistance with colormaps and spinning color wheels
 * and X was gleaned from Hiram Clawson. Rubber banding code was adapted from
 * an existing Mandelbrot program written by Stacey Campbell.
 */

#define LYAP_PATCHLEVEL 4
#define LYAP_VERSION "#(@) lyap 2.3 2/20/92"

#include <assert.h>
#include <math.h>

#include "screenhack.h"
#include "yarandom.h"
#include "hsv.h"
#include "vroot.h"

#include <X11/cursorfont.h> 
#include <X11/Xutil.h> 

char *progclass = "XLyap";

char *defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*randomize:		false",
  "*builtin:		-1",
  "*minColor:		1",
  "*maxColor:		256",
  "*dwell:		50",
  "*useLog:		false",
  "*colorExponent:	1.0",
  "*colorOffset:	0",
  "*randomForce:	",		/* 0.5 */
  "*settle:		50",
  "*minA:		2.0",
  "*minB:		2.0",
  "*wheels:		7",
  "*function:		10101010",
  "*forcingFunction:	abbabaab",
  "*bRange:		",		/* 2.0 */
  "*startX:		0.65",
  "*mapIndex:		",		/* 0 */
  "*outputFile:		",
  "*beNegative:		false",
  "*rgbMax:		65000",
  "*spinLength:		256",
  "*show:		false",
  "*aRange:		",		/* 2.0 */
  0
};

XrmOptionDescRec options [] = {
  { "-randomize", ".randomize",	XrmoptionNoArg, "true" },
  { "-builtin",   ".builtin",	XrmoptionSepArg, 0 },
  { "-C", ".minColor",		XrmoptionSepArg, 0 },	/* n */
  { "-D", ".dwell",		XrmoptionSepArg, 0 },	/* n */
  { "-L", ".useLog",		XrmoptionNoArg, "true" },
  { "-M", ".colorExponent",	XrmoptionSepArg, 0 },	/* r */
  { "-O", ".colorOffset",	XrmoptionSepArg, 0 },	/* n */
  { "-R", ".randomForce",	XrmoptionSepArg, 0 },	/* p */
  { "-S", ".settle",		XrmoptionSepArg, 0 },	/* n */
  { "-a", ".minA",		XrmoptionSepArg, 0 },	/* r */
  { "-b", ".minB",		XrmoptionSepArg, 0 },	/* n */
  { "-c", ".wheels",		XrmoptionSepArg, 0 },	/* n */
  { "-F", ".function",		XrmoptionSepArg, 0 },	/* 10101010 */
  { "-f", ".forcingFunction",	XrmoptionSepArg, 0 },	/* abbabaab */
  { "-h", ".bRange",		XrmoptionSepArg, 0 },	/* r */
  { "-i", ".startX",		XrmoptionSepArg, 0 },	/* r */
  { "-m", ".mapIndex",		XrmoptionSepArg, 0 },	/* n */
  { "-o", ".outputFile",	XrmoptionSepArg, 0 },	/* filename */
  { "-p", ".beNegative",	XrmoptionNoArg, "true" },
  { "-r", ".rgbMax",		XrmoptionSepArg, 0 },	/* n */
  { "-s", ".spinLength",	XrmoptionSepArg, 0 },	/* n */
  { "-v", ".show",		XrmoptionNoArg, "true" },
  { "-w", ".aRange",		XrmoptionSepArg, 0 },	/* r */
  { 0, 0, 0, 0 }
};


#define ABS(a)	(((a)<0) ? (0-(a)) : (a) )
#define Min(x,y) ((x < y)?x:y)
#define Max(x,y) ((x > y)?x:y)

#ifdef SIXTEEN_COLORS
#define MAXPOINTS  128
#ifdef BIGMEM
#define MAXFRAMES 4
#else
#define MAXFRAMES 2
#endif
#define MAXCOLOR 16
static int maxcolor=16, startcolor=0, color_offset=0, mincolindex=1;
static int dwell=50, settle=25;
static int width=128, height=128, xposition=128, yposition=128;
#else
#define MAXPOINTS  256
#ifdef BIGMEM
#define MAXFRAMES 8
#else
#define MAXFRAMES 2
#endif
#define MAXCOLOR 256
static int maxcolor=256, startcolor=17, color_offset=96, mincolindex=33;
static int dwell=100, settle=50;
static int width=256, height=256;
#endif

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

static int screen;
static Display*	dpy;
static Visual *visual;

static unsigned long foreground, background;

static Window canvas;

typedef struct {
	int x, y;
} xy_t;

typedef struct {
	int start_x, start_y;
	int last_x, last_y;
	} rubber_band_data_t;

typedef struct {
	Cursor band_cursor;
	double p_min, p_max, q_min, q_max;
	rubber_band_data_t rubber_band;
	} image_data_t;

typedef struct points_t {
	XPoint data[MAXCOLOR][MAXPOINTS];
	int npoints[MAXCOLOR];
	} points_t;

static points_t Points;
static image_data_t rubber_data;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

static GC Data_GC[MAXCOLOR], RubberGC;

#define MAXINDEX 64
#define FUNCMAXINDEX 16
#define MAXWHEELS 7
#define NUMMAPS 5

typedef double (*PFD)(double,double);

static double logistic(double,double), circle(double,double), leftlog(double,double), rightlog(double,double), doublelog(double,double);
static double dlogistic(double,double), dcircle(double,double), dleftlog(double,double), drightlog(double,double), ddoublelog(double,double);
static PFD map, deriv;
static PFD Maps[NUMMAPS] = { logistic, circle, leftlog, rightlog, doublelog };
static PFD Derivs[NUMMAPS] = { dlogistic, dcircle, dleftlog, drightlog, ddoublelog };

static int aflag=0, bflag=0, wflag=0, hflag=0, Rflag=0;
static double pmins[NUMMAPS] = { 2.0, 0.0, 0.0, 0.0, 0.0 };
static double pmaxs[NUMMAPS] = { 4.0, 1.0, 6.75, 6.75, 16.0 };
static double amins[NUMMAPS] = { 2.0, 0.0, 0.0, 0.0, 0.0 };
static double aranges[NUMMAPS] = { 2.0, 1.0, 6.75, 6.75, 16.0 };
static double bmins[NUMMAPS] = { 2.0, 0.0, 0.0, 0.0, 0.0 };
static double branges[NUMMAPS] = { 2.0, 1.0, 6.75, 6.75, 16.0 };

static int   forcing[MAXINDEX] = { 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,
			0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,
			0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1 };
static int   Forcing[FUNCMAXINDEX] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

static int   maxindex = MAXINDEX;
static int   funcmaxindex = FUNCMAXINDEX;
static double	min_a=2.0, min_b=2.0, a_range=2.0, b_range=2.0, minlyap=1.0;
static double  max_a=4.0, max_b=4.0;
static double  start_x=0.65, lyapunov, a_inc, b_inc, a, b;
static int	numcolors=16, numfreecols, displayplanes, lowrange;
static xy_t	point;
static Pixmap  pixmap;
static Colormap cmap;
static XColor	Colors[MAXCOLOR];
static double  *exponents[MAXFRAMES];
static double  a_minimums[MAXFRAMES], b_minimums[MAXFRAMES];
static double  a_maximums[MAXFRAMES], b_maximums[MAXFRAMES];
static double  minexp, maxexp, prob=0.5;
static int     expind[MAXFRAMES]={0}, resized[MAXFRAMES]={0};
static int	numwheels=MAXWHEELS, force=0, Force=0, negative=1;
static int     rgb_max=65000, nostart=1, stripe_interval=7;
static int	save=1, show=0, useprod=1, spinlength=256, savefile=0;
static int	maxframe=0, frame=0, dorecalc=0, mapindex=0, run=1;
static char	*outname="lyap.out";


const char * const version = LYAP_VERSION;

static void resize(void);
static void redisplay(Window w, XExposeEvent *event);
static void Spin(Window w);
static void show_defaults(void);
static void StartRubberBand(Window w, image_data_t *data, XEvent *event);
static void TrackRubberBand(Window w, image_data_t *data, XEvent *event);
static void EndRubberBand(Window w, image_data_t *data, XEvent *event);
static void CreateXorGC(void);
static void InitBuffer(void);
static void BufferPoint(Display *display, Window window, int color,
			int x, int y);
static void FlushBuffer(void);
static void init_canvas(void);
static void init_data(void);
static void init_color(void);
static void parseargs(void);
static void Clear(void);
static void setupmem(void);
static void main_event(void);
static int complyap(void);
static void Getkey(XKeyEvent *event);
static int sendpoint(double expo);
static void save_to_file(void);
static void setforcing(void);
static void check_params(int mapnum, int parnum);
static void usage(void);
static void Destroy_frame(void);
static void freemem(void);
static void Redraw(void);
static void redraw(double *exparray, int index, int cont);
static void recalc(void);
static void SetupCorners(XPoint *corners, image_data_t *data);
static void set_new_params(Window w, image_data_t *data);
static void go_down(void);
static void go_back(void);
static void go_init(void);
static void jumpwin(void);
static void print_help(void);
static void print_values(void);


void
screenhack (Display *d, Window window)
{
  XWindowAttributes xgwa;
  int builtin = -1;
  dpy = d;
  XGetWindowAttributes (dpy, window, &xgwa);
  width = xgwa.width;
  height = xgwa.height;
  visual = xgwa.visual;
  cmap = xgwa.colormap;

  parseargs();

  if (get_boolean_resource("randomize", "Boolean"))
    builtin = random() % 22;
  else {
    char *s = get_string_resource("builtin", "Integer");
    if (s && *s)
      builtin = atoi(s);
    if (s) free (s);
  }
    
  if (builtin >= 0)
    {
      char *ff = 0;
      switch (builtin) {
      case 0:
	min_a = 3.75; aflag++;
	min_b = 3.299999; bflag++;
	a_range = 0.05; wflag++;
	b_range = 0.05; hflag++;
	dwell = 200;
	settle = 100;
	ff = "abaabbaaabbb";
	break;

      case 1:
	min_a = 3.8; aflag++;
	min_b = 3.2; bflag++;
	b_range = .05; hflag++;
	a_range = .05; wflag++;
	ff = "bbbbbaaaaa";
	break;

      case 2:
	min_a =  3.4; aflag++;
	min_b =  3.04; bflag++;
	a_range =  .5; wflag++;
	b_range =  .5; hflag++;
	ff = "abbbbbbbbb";
	settle = 500;
	dwell = 1000;
	break;

      case 3:
	min_a = 3.5; aflag++;
	min_b = 3.0; bflag++;
	a_range = 0.2; wflag++;
	b_range = 0.2; hflag++;
	dwell = 600;
	settle = 300;
	ff = "aaabbbab";
	break;

      case 4:
	min_a = 3.55667; aflag++;
	min_b = 3.2; bflag++;
	b_range = .05; hflag++;
	a_range = .05; wflag++;
	ff = "bbbbbaaaaa";
	break;

      case 5:
	min_a = 3.79; aflag++;
	min_b = 3.22; bflag++;
	b_range = .02999; hflag++;
	a_range = .02999; wflag++;
	ff = "bbbbbaaaaa";
	break;

      case 6:
	min_a = 3.7999; aflag++;
	min_b = 3.299999; bflag++;
	a_range = 0.2; wflag++;
	b_range = 0.2; hflag++;
	dwell = 300;
	settle = 150;
	ff = "abaabbaaabbb";
	break;

      case 7:
	min_a = 3.89; aflag++;
	min_b = 3.22; bflag++;
	b_range = .028; hflag++;
	a_range = .02999; wflag++;
	ff = "bbbbbaaaaa";
	settle = 600;
	dwell = 1000;
	break;

      case 8:
	min_a = 3.2; aflag++;
	min_b = 3.7; bflag++;
	a_range = 0.05; wflag++;
	b_range = .005; hflag++;
	ff = "abbbbaa";
	break;

      case 9:
	ff = "aaaaaabbbbbb";
	mapindex = 1;
	dwell =  400;
	settle =  200;
	minlyap = maxexp = ABS(-0.85);
	minexp = -1.0 * minlyap;
	break;

      case 10:
	ff = "aaaaaabbbbbb";
	mapindex = 1;
	dwell =  400;
	settle = 200;
	minlyap = maxexp = ABS(-0.85);
	minexp = -1.0 * minlyap;
	break;

      case 11:
	mapindex = 1;
	dwell =  400;
	settle = 200;
	minlyap = maxexp = ABS(-0.85);
	minexp = -1.0 * minlyap;
	break;

      case 12:
	ff = "abbb";
	mapindex = 1;
	dwell =  400;
	settle = 200;
	minlyap = maxexp = ABS(-0.85);
	minexp = -1.0 * minlyap;
	break;

      case 13:
	ff = "abbabaab";
	mapindex = 1;
	dwell =  400;
	settle = 200;
	minlyap = maxexp = ABS(-0.85);
	minexp = -1.0 * minlyap;
	break;

      case 14:
	ff = "abbabaab";
	dwell =  800;
	settle = 200;
	minlyap = maxexp = ABS(-0.85);
	minexp = -1.0 * minlyap;
	/* ####  -x 0.05 */
	min_a = 3.91; aflag++;
	a_range =  0.0899999999; wflag++;
	min_b =  3.28; bflag++;
	b_range =  0.35; hflag++;
	break;

      case 15:
	ff = "aaaaaabbbbbb";
	dwell =  400;
	settle = 200;
	minlyap = maxexp = ABS(-0.85);
	minexp = -1.0 * minlyap;
	break;

      case 16:
	dwell =  400;
	settle = 200;
	minlyap = maxexp = ABS(-0.85);
	minexp = -1.0 * minlyap;
	break;

      case 17:
	ff = "abbb";
	dwell =  400;
	settle = 200;
	minlyap = maxexp = ABS(-0.85);
	minexp = -1.0 * minlyap;
	break;

      case 18:
	ff = "abbabaab";
	dwell =  400;
	settle = 200;
	minlyap = maxexp = ABS(-0.85);
	minexp = -1.0 * minlyap;
	break;

      case 19:
	mapindex = 2;
	ff = "aaaaaabbbbbb";
	dwell =  400;
	settle = 200;
	minlyap = maxexp = ABS(-0.85);
	minexp = -1.0 * minlyap;
	break;

      case 20:
	mapindex = 2;
	dwell =  400;
	settle = 200;
	minlyap = maxexp = ABS(-0.85);
	minexp = -1.0 * minlyap;
	break;

      case 21:
	mapindex = 2;
	ff = "abbb";
	dwell =  400;
	settle = 200;
	minlyap = maxexp = ABS(-0.85);
	minexp = -1.0 * minlyap;
	break;

      case 22:
	mapindex = 2;
	ff = "abbabaab";
	dwell =  400;
	settle = 200;
	minlyap = maxexp = ABS(-0.85);
	minexp = -1.0 * minlyap;
	break;
      }

      if (ff) {
	char *ch;
	int bindex = 0;
	maxindex = strlen(ff);
	if (maxindex > MAXINDEX)
	  usage();
	ch = ff;
	force++;
	while (bindex < maxindex) {
	  if (*ch == 'a')
	    forcing[bindex++] = 0;
	  else if (*ch == 'b')
	    forcing[bindex++] = 1;
	  else
	    usage();
	  ch++;
	}
      }
    }

  screen = DefaultScreen(dpy);
  background = BlackPixel(dpy, screen);
  setupmem();
  init_data();
  if (displayplanes > 1)
    foreground = startcolor;
  else
    foreground = WhitePixel(dpy, screen);

  /*
  * Create the window to display the Lyapunov exponents
  */
  canvas = window;
  init_canvas();

  if (window != DefaultRootWindow(dpy))
    XSelectInput(dpy,canvas,KeyPressMask|ButtonPressMask|ButtonMotionMask|
		 ButtonReleaseMask|ExposureMask|StructureNotifyMask);
  if (displayplanes > 1) {
    init_color();
  } else {
    XQueryColors(dpy, DefaultColormap(dpy, DefaultScreen(dpy)),
	Colors, numcolors);
  }
  pixmap = XCreatePixmap(dpy, DefaultRootWindow(dpy),
			 width, height, DefaultDepth(dpy, screen));
  rubber_data.band_cursor = XCreateFontCursor(dpy, XC_hand2);
  CreateXorGC();
  Clear();
  for(;;)
      main_event();
}

static void
main_event(void)
{
  int n;
  XEvent event;

  if (complyap() == TRUE)
      run=0;
  n = XEventsQueued(dpy, QueuedAfterFlush);
  while (n--) {
	  XNextEvent(dpy, &event);
	    switch(event.type)
	    {
	    case KeyPress:
    Getkey(&event.xkey);
    break;
	    case Expose:
    redisplay(canvas, &event.xexpose);
	  break;
	    case ConfigureNotify:
    resize();
	  break;
	    case ButtonPress:
    StartRubberBand(canvas, &rubber_data, &event);
	  break;
	    case MotionNotify:
    TrackRubberBand(canvas, &rubber_data, &event);
	  break;
	    case ButtonRelease:
    EndRubberBand(canvas, &rubber_data, &event);
	  break;
            default: 
    screenhack_handle_event (dpy, &event);
	  break;
	    }
	}
}

/* complyap() is the guts of the program. This is where the Lyapunov exponent
 * is calculated. For each iteration (past some large number of iterations)
 * calculate the logarithm of the absolute value of the derivative at that
 * point. Then average them over some large number of iterations. Some small
 * speed up is achieved by utilizing the fact that log(a*b) = log(a) + log(b).
 */
static int
complyap(void)
{
  register i, bindex;
  double total, prod, x, dx, r;

  if (!run)
    return TRUE;
  a += a_inc;
  if (a >= max_a)
    if (sendpoint(lyapunov) == TRUE)
      return FALSE;
    else {
      FlushBuffer();
      if (savefile)
	save_to_file();
      return TRUE;
    }
  if (b >= max_b) {
    FlushBuffer();
    if (savefile)
      save_to_file();
    return TRUE;
  }
  prod = 1.0;
  total = 0.0;
  bindex = 0;
  x = start_x;
  r = (forcing[bindex]) ? b : a;
#ifdef MAPS
  findex = 0;
  map = Maps[Forcing[findex]];
#endif
  for (i=0;i<settle;i++) {     /* Here's where we let the thing */
    x = (*map)(x, r);	 /* "settle down". There is usually */
    if (++bindex >= maxindex) { /* some initial "noise" in the */
      bindex = 0;    /* iterations. How can we optimize */
      if (Rflag)      /* the value of settle ??? */
	  setforcing();
    }
    r = (forcing[bindex]) ? b : a;
#ifdef MAPS
    if (++findex >= funcmaxindex)
      findex = 0;
    map = Maps[Forcing[findex]];
#endif
  }
#ifdef MAPS
  deriv = Derivs[Forcing[findex]];
#endif
  if (useprod) {      /* using log(a*b) */
    for (i=0;i<dwell;i++) {
      x = (*map)(x, r);
      dx = (*deriv)(x, r); /* ABS is a macro, so don't be fancy */
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
      if (++bindex >= maxindex) {
	bindex = 0;
	if (Rflag)
	  setforcing();
      }
      r = (forcing[bindex]) ? b : a;
#ifdef MAPS
      if (++findex >= funcmaxindex)
	findex = 0;
      map = Maps[Forcing[findex]];
      deriv = Derivs[Forcing[findex]];
#endif
    }
    total += log(prod);
    lyapunov = (total * M_LOG2E) / (double)i;   
  }
  else {	/* use log(a) + log(b) */
    for (i=0;i<dwell;i++) {
      x = (*map)(x, r);
      dx = (*deriv)(x, r); /* ABS is a macro, so don't be fancy */
      dx = ABS(dx);
      if (x == 0.0)  /* log(0) check */
      {
        i++;
        break;
      }
      total += log(dx);
      if (++bindex >= maxindex) {
	bindex = 0;
	if (Rflag)
	  setforcing();
      }
      r = (forcing[bindex]) ? b : a;
#ifdef MAPS
      if (++findex >= funcmaxindex)
	findex = 0;
      map = Maps[Forcing[findex]];
      deriv = Derivs[Forcing[findex]];
#endif
    }
    lyapunov = (total * M_LOG2E) / (double)i;
  }

  if (sendpoint(lyapunov) == TRUE)
    return FALSE;
  else {
    FlushBuffer();
    if (savefile)
      save_to_file();
    return TRUE;
  }
}

static double
logistic(double x, double r)	    /* the familiar logistic map */
{
  return(r * x * (1.0 - x));
}

static double
dlogistic(double x, double r)	    /* the derivative of logistic map */
{
  return(r - (2.0 * r * x));
}

static double
circle(double x, double r)	  /* sin() hump or sorta like the circle map */
{
  return(r * sin(M_PI * x));
}

static double
dcircle(double x, double r)	  /* derivative of the "sin() hump" */
{
  return(r * M_PI * cos(M_PI * x));
}

static double
leftlog(double x, double r)	  /* left skewed logistic */
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
init_data(void)
{
  numcolors = XDisplayCells(dpy, XDefaultScreen(dpy));
  displayplanes = DisplayPlanes(dpy, XDefaultScreen(dpy));
  if (numcolors > maxcolor)
    numcolors = maxcolor;
  numfreecols = numcolors - mincolindex;
  lowrange = mincolindex - startcolor;
  a_inc = a_range / (double)width;
  b_inc = b_range / (double)height;
  point.x = -1;
  point.y = 0;
  a = rubber_data.p_min = min_a;
  b = rubber_data.q_min = min_b;
  rubber_data.p_max = max_a;
  rubber_data.q_max = max_b;
  if (show)
    show_defaults();
  InitBuffer();
}

static void
init_canvas(void)
{
  static int i;

  /*
  * create default, writable, graphics contexts for the canvas.
  */
	for (i=0; i<maxcolor; i++) {
	    Data_GC[i] = XCreateGC(dpy, DefaultRootWindow(dpy),
		(unsigned long) NULL, (XGCValues *) NULL);
	    /* set the background to black */
	    XSetBackground(dpy,Data_GC[i],BlackPixel(dpy,XDefaultScreen(dpy)));
	    /* set the foreground of the ith context to i */
	    XSetForeground(dpy, Data_GC[i], i);
	}
	if (displayplanes == 1) {
	    XSetForeground(dpy,Data_GC[0],BlackPixel(dpy,XDefaultScreen(dpy)));
	    XSetForeground(dpy,Data_GC[1],WhitePixel(dpy,XDefaultScreen(dpy)));
	}
}

#if 0
static void
hls2rgb(int hue_light_sat[3],
	int rgb[3])		/*	Each in range [0..65535]	*/
{
  unsigned short r, g, b;
  hsv_to_rgb((int) (hue_light_sat[0] / 10),		/* 0-3600 -> 0-360 */
	     (int) ((hue_light_sat[2]/1000.0) * 64435), /* 0-1000 -> 0-65535 */
	     (int) ((hue_light_sat[1]/1000.0) * 64435), /* 0-1000 -> 0-65535 */
	     &r, &g, &b);
  rgb[0] = r;
  rgb[1] = g;
  rgb[2] = b;
}
#endif /* 0 */


static void
init_color(void)
{
#if 1

  int i;
  XColor colors[256];
  int ncolors = maxcolor;
  Bool writable = False;
  make_smooth_colormap(dpy, visual, cmap,
			colors, &ncolors, True, &writable, True);

  for (i = 0; i < maxcolor; i++)
    XSetForeground(dpy, Data_GC[i],
		   colors[((int) ((i / ((float)maxcolor)) * ncolors))].pixel);

#else
  static int i, j, colgap, leg, step;
  static Visual *visual;
  Colormap def_cmap;
  int hls[3], rgb[3];

  def_cmap = DefaultColormap(dpy, DefaultScreen(dpy));
  for (i=0; i<numcolors; i++) {
    Colors[i].pixel = i;
    Colors[i].flags = DoRed|DoGreen|DoBlue;
  }

  /* Try to write into a new color map */
  visual = DefaultVisual(dpy, DefaultScreen(dpy));
  cmap = XCreateColormap(dpy, canvas, visual, AllocAll);
  XQueryColors(dpy, def_cmap, Colors, numcolors);
  if (mincolindex)
    colgap = rgb_max / mincolindex;
  else
    colgap = rgb_max;
  hls[0] = 50;	/* Hue in low range */
  hls[2] = 1000;  /* Fully saturated */
  for (i=startcolor; i<lowrange + startcolor; i++) {
    hls[1] = 1000L * (i-startcolor) / lowrange;
    hls2rgb(hls, rgb);
    Colors[i].red = rgb[0];
    Colors[i].green = rgb[1];
    Colors[i].blue = rgb[2];
  }
  colgap = rgb_max / numcolors;
  if (numwheels == 0)
    XQueryColors(dpy, def_cmap, Colors, numcolors);
  else if (numwheels == 1) {
    colgap = 2*rgb_max/(numcolors - color_offset);
    for (i=mincolindex; i<(numcolors/2); i++) {
      Colors[i].blue = 0;
      Colors[i].green=((i+color_offset)*colgap);
      Colors[i].red=((i+color_offset)*colgap);
    }
    for (i=(numcolors/2); i<(numcolors); i++) {
      Colors[i].blue = 0;
      Colors[i].green=(((numcolors-i)+color_offset)*colgap);
      Colors[i].red=(((numcolors-i)+color_offset)*colgap);
    }
  }
  else if (numwheels == 2) {
	  hls[0] = 800; /* Hue in mid range */
	  hls[2] = 1000;  /* Fully saturated */
	  for (i=startcolor; i<lowrange + startcolor; i++) {
      hls[1] = 1000L * (i-startcolor) / lowrange;
      hls2rgb(hls, rgb);
      Colors[i].red = rgb[0];
      Colors[i].green = rgb[1];
      Colors[i].blue = rgb[2];
	  }
    for (i=mincolindex; i<(numcolors/2); i++) {
      Colors[i].blue = rgb_max;
      Colors[i].green = 0;
      Colors[i].red=(i*2*rgb_max/numcolors);
    }
    for (i=(numcolors/2); i<numcolors; i++) {
      Colors[i].blue = rgb_max;
      Colors[i].green = 0;
      Colors[i].red=((numcolors - i)*2*rgb_max/numcolors);
    }
  }
  else if (numwheels == 3) {
	  hls[0] = 800; /* Hue in mid range */
	  hls[2] = 1000;  /* Fully saturated */
	  for (i=startcolor; i<lowrange + startcolor; i++) {
      hls[1] = 1000L * (i-startcolor) / lowrange;
      hls2rgb(hls, rgb);
      Colors[i].red = rgb[0];
      Colors[i].green = rgb[1];
      Colors[i].blue = rgb[2];
	  }
    colgap = 4*rgb_max/numcolors;
    for (i=mincolindex; i<(numcolors/4); i++) {
      Colors[i].blue = rgb_max;
      Colors[i].green = 0;
      Colors[i].red=(i*colgap);
    }
    for (i=(numcolors/4); i<(numcolors/2); i++) {
      Colors[i].red = rgb_max;
      Colors[i].green = 0;
      Colors[i].blue=((numcolors/2) - i) * colgap;
    }
    for (i=(numcolors/2); i<(0.75*numcolors); i++) {
      Colors[i].red = rgb_max;
      Colors[i].blue=(i * colgap);
      Colors[i].green = 0;
    }
    for (i=(0.75*numcolors); i<numcolors; i++) {
      Colors[i].blue = rgb_max;
      Colors[i].green = 0;
      Colors[i].red=(numcolors-i)*colgap;
    }
  }
  else if (numwheels == 4) {
	  hls[0] = 800; /* Hue in mid range */
	  hls[2] = 1000;  /* Fully saturated */
	  for (i=startcolor; i<lowrange + startcolor; i++) {
      hls[1] = 1000L * (i-startcolor) / lowrange;
      hls2rgb(hls, rgb);
      Colors[i].red = rgb[0];
      Colors[i].green = rgb[1];
      Colors[i].blue = rgb[2];
	  }
    colgap = numwheels * rgb_max / numcolors;
    for (i=mincolindex; i<(numcolors/numwheels); i++) {
      Colors[i].blue = rgb_max;
      Colors[i].green = 0;
      Colors[i].red=(i*colgap);
    }
    for (i=(numcolors/numwheels); i<(2*numcolors/numwheels); i++) {
      Colors[i].red = rgb_max;
      Colors[i].green = 0;
      Colors[i].blue=((2*numcolors/numwheels) - i) * colgap;
    }
    for (i=(2*numcolors/numwheels); i<numcolors; i++) {
      Colors[i].red = rgb_max;
      Colors[i].green=(i - (2*numcolors/numwheels)) * colgap;
      Colors[i].blue = 0;
    }
  }
  else if (numwheels == 5) {
    hls[1] = 700; /* Lightness in midrange */
    hls[2] = 1000;  /* Fully saturated */
    for (i=mincolindex; i<numcolors; i++) {
      hls[0] = 3600L * i / numcolors;
      hls2rgb(hls, rgb);
      Colors[i].red = rgb[0];
      Colors[i].green = rgb[1];
      Colors[i].blue = rgb[2];
    }
    for (i=mincolindex; i<numcolors; i+=stripe_interval) {
      hls[0] = 3600L * i / numcolors;
      hls2rgb(hls, rgb);
      Colors[i].red = rgb[0] / 2;
      Colors[i].green = rgb[1] / 2;
      Colors[i].blue = rgb[2] / 2;
    }
  }
  else if (numwheels == 6) {
      hls[0] = 800; /* Hue in mid range */
      hls[2] = 1000;  /* Fully saturated */
      for (i=startcolor; i<lowrange + startcolor; i++) {
    hls[1] = 1000L * (i-startcolor) / lowrange;
    hls2rgb(hls, rgb);
    Colors[i].red = rgb[0];
    Colors[i].green = rgb[1];
    Colors[i].blue = rgb[2];
      }
      step = numfreecols / 3;
      leg = step+mincolindex;
      for (i = mincolindex; i < leg; ++i)
      {
    Colors[i].pixel = i;
    Colors[i].red = fabs(65535 - (double)i / step * 65535.0);
    Colors[i].blue = (double)i / step * 65535.0;
    Colors[i].green = 0;
    Colors[i].flags = DoRed | DoGreen | DoBlue;
      }
      for (j = 0, i = leg, leg += step; i < leg; ++i, ++j)
      {
    Colors[i].pixel = i;
    Colors[i].red = (double)j / step * 65535.0;
    Colors[i].blue = 65535;
    Colors[i].green = Colors[i].red;
    Colors[i].flags = DoRed | DoGreen | DoBlue;
      }
      for (j = 0, i = leg, leg += step; i < leg; ++i, ++j)
      {
    Colors[i].pixel = i;
    Colors[i].red = 65535;
    Colors[i].blue = fabs(65535 - (double)j / step * 65535.0);
    Colors[i].green = Colors[i].blue;
    Colors[i].flags = DoRed | DoGreen | DoBlue;
      }
  }
  else if (numwheels == MAXWHEELS) {  /* rainbow palette */
    hls[1] = 500; /* Lightness in midrange */
    hls[2] = 1000;  /* Fully saturated */
    for (i=mincolindex; i<numcolors; i++) {
      hls[0] = 3600L * i / numcolors;
      hls2rgb(hls, rgb);
      Colors[i].red = rgb[0];
      Colors[i].green = rgb[1];
      Colors[i].blue = rgb[2];
    }
  }
  XStoreColors(dpy, cmap, Colors, numcolors);

  XSetWindowColormap(dpy, canvas, cmap);
#endif
}

static void
parseargs()
{
  static int i;
  int bindex=0, findex;
  char *s, *ch;

  map = Maps[0];
  deriv = Derivs[0];
  maxexp=minlyap; minexp= -1.0 * minlyap;

  mincolindex = get_integer_resource("minColor", "Integer");
  dwell = get_integer_resource("dwell", "Integer");
#ifdef MAPS
  {
    char *optarg = get_string_resource("function", "String");
    funcmaxindex = strlen(optarg);
    if (funcmaxindex > FUNCMAXINDEX)
      usage();
    ch = optarg;
    Force++;
    for (findex=0;findex<funcmaxindex;findex++) {
      Forcing[findex] = (int)(*ch++ - '0');;
      if (Forcing[findex] >= NUMMAPS)
	usage();
    }
  }
#endif
  if (get_boolean_resource("useLog", "Boolean"))
    useprod=0;

  minlyap=ABS(get_float_resource("colorExponent", "Float"));
  maxexp=minlyap;
  minexp= -1.0 * minlyap;

  color_offset = get_integer_resource("colorOffset", "Integer");

  maxcolor=ABS(get_integer_resource("maxColor", "Integer"));
  if ((maxcolor - startcolor) <= 0)
    startcolor = 0;
  if ((maxcolor - mincolindex) <= 0) {
    mincolindex = 1;
    color_offset = 0;
  }

  s = get_string_resource("randomForce", "Float");
  if (s && *s) {
    prob=atof(s); Rflag++; setforcing();
  }

  settle = get_integer_resource("settle", "Integer");

  s = get_string_resource("minA", "Float");
  if (s && *s) {
    min_a = atof(s);
    aflag++;
  }
  
  s = get_string_resource("minB", "Float");
  if (s && *s) {
    min_b=atof(s); bflag++;
  }
  
  numwheels = get_integer_resource("wheels", "Integer");

  s = get_string_resource("forcingFunction", "String");
  if (s && *s) {
    maxindex = strlen(s);
    if (maxindex > MAXINDEX)
      usage();
    ch = s;
    force++;
    while (bindex < maxindex) {
      if (*ch == 'a')
	forcing[bindex++] = 0;
      else if (*ch == 'b')
	forcing[bindex++] = 1;
      else
	usage();
      ch++;
    }
  }

  s = get_string_resource("bRange", "Float");
  if (s && *s) {
    b_range = atof(s);
    hflag++;
  }

  start_x = get_float_resource("startX", "Float");

  s = get_string_resource("mapIndex", "Integer");
  if (s && *s) {
    mapindex=atoi(s);
    if ((mapindex >= NUMMAPS) || (mapindex < 0))
      usage();
    map = Maps[mapindex];
    deriv = Derivs[mapindex];
    if (!aflag)
      min_a = amins[mapindex];
    if (!wflag)
      a_range = aranges[mapindex];
    if (!bflag)
      min_b = bmins[mapindex];
    if (!hflag)
      b_range = branges[mapindex];
    if (!Force)
      for (i=0;i<FUNCMAXINDEX;i++)
	Forcing[i] = mapindex;
  }

  outname = get_string_resource("outputFile", "Integer");

  if (get_boolean_resource("beNegative", "Boolean"))
    negative--;

  rgb_max = get_integer_resource("rgbMax", "Integer");
  spinlength = get_integer_resource("spinLength", "Integer");
  show = get_boolean_resource("show", "Boolean");

  s = get_string_resource("aRange", "Float");
  if (s && *s) {
    a_range = atof(s); wflag++;
  }

  max_a = min_a + a_range;
  max_b = min_b + b_range;

  a_minimums[0] = min_a; b_minimums[0] = min_b;
  a_maximums[0] = max_a; b_maximums[0] = max_b;

  if (Force)
    if (maxindex == funcmaxindex)
      for (findex=0;findex<funcmaxindex;findex++)
	check_params(Forcing[findex],forcing[findex]);
    else
      fprintf(stderr, "Warning! Unable to check parameters\n");
  else
    check_params(mapindex,2);
}

static void
check_params(int mapnum, int parnum)
{

  if (parnum != 1) {
      if ((max_a > pmaxs[mapnum]) || (min_a < pmins[mapnum])) {
    fprintf(stderr, "Warning! Parameter 'a' out of range.\n");
    fprintf(stderr, "You have requested a range of (%f,%f).\n",
      min_a,max_a);
    fprintf(stderr, "Valid range is (%f,%f).\n",
      pmins[mapnum],pmaxs[mapnum]);
      }
  }
  if (parnum != 0) {
      if ((max_b > pmaxs[mapnum]) || (min_b < pmins[mapnum])) {
    fprintf(stderr, "Warning! Parameter 'b' out of range.\n");
    fprintf(stderr, "You have requested a range of (%f,%f).\n",
      min_b,max_b);
    fprintf(stderr, "Valid range is (%f,%f).\n",
      pmins[mapnum],pmaxs[mapnum]);
      }
  }
}

static void
usage(void)
{
    fprintf(stderr,"lyap [-BLs][-W#][-H#][-a#][-b#][-w#][-h#][-x xstart]\n");
    fprintf(stderr,"\t[-M#][-S#][-D#][-f string][-r#][-O#][-C#][-c#][-m#]\n");
#ifdef MAPS
    fprintf(stderr,"\t[-F string]\n");
#endif
    fprintf(stderr,"\tWhere: -C# specifies the minimum color index\n");
    fprintf(stderr,"\t	     -r# specifies the maxzimum rgb value\n");
    fprintf(stderr,"\t	     -u displays this message\n");
    fprintf(stderr,"\t	     -a# specifies the minimum horizontal parameter\n");
    fprintf(stderr,"\t	     -b# specifies the minimum vertical parameter\n");
    fprintf(stderr,"\t	     -w# specifies the horizontal parameter range\n");
    fprintf(stderr,"\t	     -h# specifies the vertical parameter range\n");
    fprintf(stderr,"\t	     -D# specifies the dwell\n");
    fprintf(stderr,"\t	     -S# specifies the settle\n");
    fprintf(stderr,"\t	     -H# specifies the initial window height\n");
    fprintf(stderr,"\t	     -W# specifies the initial window width\n");
    fprintf(stderr,"\t	     -O# specifies the color offset\n");
    fprintf(stderr,"\t	     -c# specifies the desired color wheel\n");
    fprintf(stderr,"\t	     -m# specifies the desired map (0-4)\n");
    fprintf(stderr,"\t	     -f aabbb specifies a forcing function of 00111\n");
#ifdef MAPS
    fprintf(stderr,"\t	     -F 00111 specifies the function forcing function\n");
#endif
    fprintf(stderr,"\t	     -L indicates use log(x)+log(y) rather than log(xy)\n");
    fprintf(stderr,"\tDuring display :\n");
    fprintf(stderr,"\t	   Use the mouse to zoom in on an area\n");
    fprintf(stderr,"\t	   e or E recalculates color indices\n");
    fprintf(stderr,"\t	   f or F saves exponents to a file\n");
    fprintf(stderr,"\t	   KJmn increase/decrease minimum negative exponent\n");
    fprintf(stderr,"\t	   r or R redraws\n");
    fprintf(stderr,"\t	   s or S spins the colorwheel\n");
    fprintf(stderr,"\t	   w or W changes the color wheel\n");
    fprintf(stderr,"\t	   x or X clears the window\n");
    fprintf(stderr,"\t	   q or Q exits\n");
    exit(1);
}

static void
Cycle_frames(void)
{
  static int i;
  for (i=0;i<=maxframe;i++)
    redraw(exponents[i], expind[i], 1);
}

static void
Spin(Window w)
{
  static int i, j;
  long tmpxcolor;

  if (displayplanes > 1) {
    for (j=0;j<spinlength;j++) {
      tmpxcolor = Colors[mincolindex].pixel;
      for (i=mincolindex;i<numcolors-1;i++)
	Colors[i].pixel = Colors[i+1].pixel;
      Colors[numcolors-1].pixel = tmpxcolor;
      XStoreColors(dpy, cmap, Colors, numcolors);
    }
    for (j=0;j<spinlength;j++) {
      tmpxcolor = Colors[numcolors-1].pixel;
      for (i=numcolors-1;i>mincolindex;i--)
	Colors[i].pixel = Colors[i-1].pixel;
      Colors[mincolindex].pixel = tmpxcolor;
      XStoreColors(dpy, cmap, Colors, numcolors);
    }
  }
}

static void
Getkey(XKeyEvent *event)
{
  unsigned char key;
  static int i;
  if (XLookupString(event, (char *)&key, sizeof(key), (KeySym *)0,
	    (XComposeStatus *) 0) > 0)
    switch (key) {
  case '<': dwell /= 2; if (dwell < 1) dwell = 1; break;
  case '>': dwell *= 2; break;
  case '[': settle /= 2; if (settle < 1) settle = 1; break;
  case ']': settle *= 2; break;
  case 'd': go_down(); break;
  case 'D': FlushBuffer(); break;
  case 'e':
  case 'E': FlushBuffer();
      dorecalc = (!dorecalc);
      if (dorecalc)
      recalc();
      else {
      maxexp = minlyap; minexp = -1.0 * minlyap;
      }
      redraw(exponents[frame], expind[frame], 1);
      break;
  case 'f':
  case 'F': save_to_file(); break;
  case 'i': if (stripe_interval > 0) {
      stripe_interval--;
	if (displayplanes > 1) {
	    init_color();
	}
      }
      break;
  case 'I': stripe_interval++;
      if (displayplanes > 1) {
	init_color();
      }
      break;
  case 'K': if (minlyap > 0.05)
      minlyap -= 0.05;
       break;
  case 'J': minlyap += 0.05;
       break;
  case 'm': mapindex++;
		  if (mapindex >= NUMMAPS)
			mapindex=0;
		  map = Maps[mapindex];
		  deriv = Derivs[mapindex];
      if (!aflag)
			min_a = amins[mapindex];
		  if (!wflag)
			a_range = aranges[mapindex];
		  if (!bflag)
			min_b = bmins[mapindex];
		  if (!hflag)
			b_range = branges[mapindex];
		  if (!Force)
			for (i=0;i<FUNCMAXINDEX;i++)
			     Forcing[i] = mapindex;
	    max_a = min_a + a_range;
	    max_b = min_b + b_range;
	    a_minimums[0] = min_a; b_minimums[0] = min_b;
	    a_maximums[0] = max_a; b_maximums[0] = max_b;
	    a_inc = a_range / (double)width;
	    b_inc = b_range / (double)height;
	    point.x = -1;
	    point.y = 0;
	    a = rubber_data.p_min = min_a;
	    b = rubber_data.q_min = min_b;
	    rubber_data.p_max = max_a;
	    rubber_data.q_max = max_b;
		  Clear();
		  break;
  case 'M': if (minlyap > 0.005)
      minlyap -= 0.005;
       break;
  case 'N': minlyap += 0.005;
       break;
  case 'p':
  case 'P': negative = (!negative);
      FlushBuffer(); redraw(exponents[frame], expind[frame], 1);
      break;
  case 'r': FlushBuffer(); redraw(exponents[frame], expind[frame], 1);
      break;
  case 'R': FlushBuffer(); Redraw(); break;
  case 's':
       spinlength=spinlength/2;
  case 'S': if (displayplanes > 1)
      Spin(canvas);
       spinlength=spinlength*2; break;
  case 'u': go_back(); break;
  case 'U': go_init(); break;
  case 'v':
  case 'V': print_values(); break;
  case 'W': if (numwheels < MAXWHEELS)
      numwheels++;
       else
      numwheels = 0;
       if (displayplanes > 1) {
	init_color();
       }
       break;
  case 'w': if (numwheels > 0)
      numwheels--;
       else
      numwheels = MAXWHEELS;
       if (displayplanes > 1) {
	init_color();
       }
       break;
  case 'x': Clear(); break;
  case 'X': Destroy_frame(); break;
  case 'z': Cycle_frames(); redraw(exponents[frame], expind[frame], 1);
      break;
  case 'Z': while (!XPending(dpy)) Cycle_frames();
      redraw(exponents[frame], expind[frame], 1); break;
  case 'q':
  case 'Q': exit(0); break;
  case '?':
  case 'h':
  case 'H': print_help(); break;
  default:  break;
  }
}

/* Here's where we index into a color map. After the Lyapunov exponent is
 * calculated, it is used to determine what color to use for that point.
 * I suppose there are a lot of ways to do this. I used the following :
 * if it's non-negative then there's a reserved area at the lower range
 * of the color map that i index into. The ratio of some "minimum exponent
 * value" and the calculated value is used as a ratio of how high to index
 * into this reserved range. Usually these colors are dark red (see init_color).
 * If the exponent is negative, the same ratio (expo/minlyap) is used to index
 * into the remaining portion of the colormap (which is usually some light
 * shades of color or a rainbow wheel). The coloring scheme can actually make
 * a great deal of difference in the quality of the picture. Different colormaps
 * bring out different details of the dynamics while different indexing
 * algorithms also greatly effect what details are seen. Play around with this.
 */
static int
sendpoint(double expo)
{
  static int index;
  static double tmpexpo;

#if 0
/* The relationship minexp <= expo <= maxexp should always be true. This test
   enforces that. But maybe not enforcing it makes better pictures. */
  if (expo < minexp)
    expo = minexp;
  else if (expo > maxexp)
    expo = maxexp;
#endif

  point.x++;
  tmpexpo = (negative) ? expo : -1.0 * expo;
  if (tmpexpo > 0) {
    if (displayplanes >1) {
	index = (int)(tmpexpo*lowrange/maxexp);
	index = (index % lowrange) + startcolor;
    }
    else
	index = 0;
  }
  else {
    if (displayplanes >1) {
	index = (int)(tmpexpo*numfreecols/minexp);
	index = (index % numfreecols) + mincolindex;
    }
    else
	index = 1;
  }
    BufferPoint(dpy, canvas, index, point.x, point.y);
  if (save)
    exponents[frame][expind[frame]++] = expo;
  if (point.x >= width) {
    point.y++;
    point.x = 0;
    if (save) {
      b += b_inc;
      a = min_a;
    }
    if (point.y >= height)
      return FALSE;
    else
      return TRUE;
  }
  return TRUE;
}

static void
redisplay (Window w, XExposeEvent *event)
{
  /*
  * Extract the exposed area from the event and copy
  * from the saved pixmap to the window.
  */
  XCopyArea(dpy, pixmap, canvas, Data_GC[0],
	   event->x, event->y, event->width, event->height,
	   event->x, event->y);
}

static void
resize(void)
{
  Window r;
  int n, x, y;
  unsigned int bw, d, new_w, new_h;

  XGetGeometry(dpy,canvas,&r,&x,&y,&new_w,&new_h,&bw,&d);
  if ((new_w == width) && (new_h == height))
    return;
  width = new_w; height = new_h;
  XClearWindow(dpy, canvas);
  if (pixmap)
    XFreePixmap(dpy, pixmap);
  pixmap = XCreatePixmap(dpy, DefaultRootWindow(dpy),
      width, height, DefaultDepth(dpy, screen));
  a_inc = a_range / (double)width;
  b_inc = b_range / (double)height;
  point.x = -1;
  point.y = 0;
  run = 1;
  a = rubber_data.p_min = min_a;
  b = rubber_data.q_min = min_b;
  rubber_data.p_max = max_a;
  rubber_data.q_max = max_b;
  freemem();
  setupmem();
	for (n=0;n<MAXFRAMES;n++)
    if ((n <= maxframe) && (n != frame))
	resized[n] = 1;
  InitBuffer();
  Clear();
  Redraw();
}

static void
redraw(double *exparray, int index, int cont)
{
  static int i;
  static int x_sav, y_sav;

  x_sav = point.x;
  y_sav = point.y;

  point.x = -1;
  point.y = 0;

  save=0;
  for (i=0;i<index;i++)
    sendpoint(exparray[i]);
  save=1;

  if (cont) {
    point.x = x_sav;
    point.y = y_sav;
  }
  else {
    a = point.x * a_inc + min_a;
    b = point.y * b_inc + min_b;
  }
  FlushBuffer();
}

static void
Redraw(void)
{
  FlushBuffer();
	point.x = -1;
	point.y = 0;
  run = 1;
	a = min_a;
	b = min_b;
  expind[frame] = 0;
  resized[frame] = 0;
}

/* Store color pics in PPM format and monochrome in PGM */
static void
save_to_file(void)
{
  FILE *outfile;
  unsigned char c;
  XImage *ximage;
  static int i,j;
  struct Colormap {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
  };
  struct Colormap *colormap=NULL;

  if (colormap)
    free(colormap);
  if ((colormap=
    (struct Colormap *)malloc(sizeof(struct Colormap)*maxcolor))
      == NULL) {
    fprintf(stderr,"Error malloc'ing colormap array\n");
    exit(-1);
  }
  outfile = fopen(outname,"w");
  if(!outfile) {
    perror(outname);
    exit(-1);
  }

  ximage=XGetImage(dpy, pixmap, 0, 0, width, height, AllPlanes, XYPixmap);

  if (displayplanes > 1) {
    for (i=0;i<maxcolor;i++) {
      colormap[i].red=(unsigned char)(Colors[i].red >> 8);
      colormap[i].green=(unsigned char)(Colors[i].green >> 8);
      colormap[i].blue =(unsigned char)(Colors[i].blue >> 8);
    }
    fprintf(outfile,"P%d %d %d\n",6,width,height);
  }
  else
    fprintf(outfile,"P%d %d %d\n",5,width,height);
  fprintf(outfile,"# settle=%d	dwell=%d start_x=%f\n",settle,dwell,
	start_x);
  fprintf(outfile,"# min_a=%f  a_rng=%f	 max_a=%f\n",min_a,a_range,max_a);
  fprintf(outfile,"# min_b=%f  b_rng=%f	 max_b=%f\n",min_b,b_range,max_b);
  if (Rflag)
    fprintf(outfile,"# pseudo-random forcing\n");
  else if (force) {
    fprintf(outfile,"# periodic forcing=");
    for (i=0;i<maxindex;i++) {
      fprintf(outfile,"%d",forcing[i]);
    }
    fprintf(outfile,"\n");
  }
  else
    fprintf(outfile,"# periodic forcing=01\n");
  if (Force) {
    fprintf(outfile,"# function forcing=");
    for (i=0;i<funcmaxindex;i++) {
      fprintf(outfile,"%d",Forcing[i]);
    }
    fprintf(outfile,"\n");
  }
  fprintf(outfile,"%d\n",numcolors-1);

  for (j=0;j<height;j++)
      for (i=0;i<width;i++) {
    c = (unsigned char)XGetPixel(ximage,i,j);
    if (displayplanes > 1)
	fwrite((char *)&colormap[c],sizeof colormap[0],1,outfile);
    else
	fwrite((char *)&c,sizeof c,1,outfile);
      }
  fclose(outfile);
}

static void
recalc(void)
{
  static int i, x, y;

  minexp = maxexp = 0.0;
  x = y = 0;
  for (i=0;i<expind[frame];i++) {
    if (exponents[frame][i] < minexp)
      minexp = exponents[frame][i];
    if (exponents[frame][i] > maxexp)
      maxexp = exponents[frame][i];
  }
}

static void
Clear(void)
{
      XClearWindow(dpy, canvas);
  XCopyArea(dpy, canvas, pixmap, Data_GC[0],
	    0, 0, width, height, 0, 0);
  InitBuffer();
}

static void
show_defaults(void)
{

  printf("Width=%d  Height=%d  numcolors=%d  settle=%d	dwell=%d\n",
    width,height,numcolors,settle,dwell);
  printf("min_a=%f  a_range=%f	max_a=%f\n", min_a,a_range,max_a);
  printf("min_b=%f  b_range=%f	max_b=%f\n", min_b,b_range,max_b);
  printf("minlyap=%f  minexp=%f	 maxexp=%f\n", minlyap,minexp,maxexp);
  exit(0);
}

static void
CreateXorGC(void)
{
  XGCValues values;

  values.foreground = foreground;
  values.line_style = LineSolid;
  values.function = GXxor;
  RubberGC = XCreateGC(dpy, DefaultRootWindow(dpy),
	GCForeground | GCBackground | GCFunction | GCLineStyle, &values);
}

static void
StartRubberBand(Window w, image_data_t *data, XEvent *event)
{
  XPoint corners[5];

  nostart = 0;
  data->rubber_band.last_x = data->rubber_band.start_x = event->xbutton.x;
  data->rubber_band.last_y = data->rubber_band.start_y = event->xbutton.y;
  SetupCorners(corners, data);
  XDrawLines(dpy, canvas, RubberGC,
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
TrackRubberBand(Window w, image_data_t *data, XEvent *event)
{
  XPoint corners[5];
  int xdiff, ydiff;

  if (nostart)
    return;
  SetupCorners(corners, data);
  XDrawLines(dpy, canvas, RubberGC,
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
  XDrawLines(dpy, canvas, RubberGC,
      corners, sizeof(corners) / sizeof(corners[0]), CoordModeOrigin);
}

static void
EndRubberBand(Window w, image_data_t *data, XEvent *event)
{
  XPoint corners[5];
  XPoint top, bot;
  double delta, diff;

  nostart = 1;
  SetupCorners(corners, data);
  XDrawLines(dpy, canvas, RubberGC,
      corners, sizeof(corners) / sizeof(corners[0]), CoordModeOrigin);
  if (data->rubber_band.start_x >= data->rubber_band.last_x ||
      data->rubber_band.start_y >= data->rubber_band.last_y)
    return;
  top.x = data->rubber_band.start_x;
  bot.x = data->rubber_band.last_x;
  top.y = data->rubber_band.start_y;
  bot.y = data->rubber_band.last_y;
  diff = data->q_max - data->q_min;
  delta = (double)top.y / (double)height;
  data->q_min += diff * delta;
  delta = (double)(height - bot.y) / (double)height;
  data->q_max -= diff * delta;
  diff = data->p_max - data->p_min;
  delta = (double)top.x / (double)width;
  data->p_min += diff * delta;
  delta = (double)(width - bot.x) / (double)width;
  data->p_max -= diff * delta;
  fflush(stdout);
  set_new_params(w, data);
}

static void
set_new_params(Window w, image_data_t *data)
{
  frame = (maxframe + 1) % MAXFRAMES;
  if (frame > maxframe)
    maxframe = frame;
  a_range = data->p_max - data->p_min;
  b_range = data->q_max - data->q_min;
	a_minimums[frame] = min_a = data->p_min;
	b_minimums[frame] = min_b = data->q_min;
	a_inc = a_range / (double)width;
	b_inc = b_range / (double)height;
	point.x = -1;
	point.y = 0;
  run = 1;
	a = min_a;
	b = min_b;
	a_maximums[frame] = max_a = data->p_max;
	b_maximums[frame] = max_b = data->q_max;
  expind[frame] = 0;;
  Clear();
}

static void
go_down(void)
{
  frame++;
  if (frame > maxframe)
    frame = 0;
  jumpwin();
}

static void
go_back(void)
{
  frame--;
  if (frame < 0)
    frame = maxframe;
  jumpwin();
}

static void
jumpwin(void)
{
  rubber_data.p_min = min_a = a_minimums[frame];
  rubber_data.q_min = min_b = b_minimums[frame];
  rubber_data.p_max = max_a = a_maximums[frame];
  rubber_data.q_max = max_b = b_maximums[frame];
  a_range = max_a - min_a;
  b_range = max_b - min_b;
	a_inc = a_range / (double)width;
	b_inc = b_range / (double)height;
	point.x = -1;
	point.y = 0;
	a = min_a;
	b = min_b;
  Clear();
  if (resized[frame])
    Redraw();
  else
    redraw(exponents[frame], expind[frame], 0);
}

static void
go_init(void)
{
  frame = 0;
  jumpwin();
}

static void
Destroy_frame(void)
{
  static int i;

  for (i=frame; i<maxframe; i++) {
    exponents[frame] = exponents[frame+1];
    expind[frame] = expind[frame+1];
    a_minimums[frame] = a_minimums[frame+1];
    b_minimums[frame] = b_minimums[frame+1];
    a_maximums[frame] = a_maximums[frame+1];
    b_maximums[frame] = b_maximums[frame+1];
  }
  maxframe--;
  go_back();
}

static void
InitBuffer(void)
{
  int i;

  for (i = 0 ; i < maxcolor; ++i)
    Points.npoints[i] = 0;
}

static void
BufferPoint(Display *display, Window window, int color, int x, int y)
{

/* Guard against bogus color values. Shouldn't be necessary but paranoia
   is good. */
  if (color < 0)
    color = 0;
  else if (color >= maxcolor)
    color = maxcolor - 1;

  if (Points.npoints[color] == MAXPOINTS)
  {
    XDrawPoints(display, window, Data_GC[color],
	Points.data[color], Points.npoints[color], CoordModeOrigin);
    XDrawPoints(display, pixmap, Data_GC[color],
	Points.data[color], Points.npoints[color], CoordModeOrigin);
    Points.npoints[color] = 0;
  }
  Points.data[color][Points.npoints[color]].x = x;
  Points.data[color][Points.npoints[color]].y = y;
  ++Points.npoints[color];
}

static void
FlushBuffer(void)
{
  int color;

  for (color = 0; color < maxcolor; ++color)
    if (Points.npoints[color])
    {
	XDrawPoints(dpy, canvas, Data_GC[color],
	  Points.data[color], Points.npoints[color],
	  CoordModeOrigin);
	XDrawPoints(dpy, pixmap, Data_GC[color],
	  Points.data[color], Points.npoints[color],
	  CoordModeOrigin);
	Points.npoints[color] = 0;
    }
}

static void
print_help(void)
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
print_values(void)
{
    static int i;

    printf("\nminlyap=%f minexp=%f maxexp=%f\n",minlyap,minexp,maxexp);
    printf("width=%d height=%d\n",width,height);
    printf("settle=%d  dwell=%d start_x=%f\n",settle,dwell, start_x);
    printf("min_a=%f  a_rng=%f	max_a=%f\n",min_a,a_range,max_a);
    printf("min_b=%f  b_rng=%f	max_b=%f\n",min_b,b_range,max_b);
    if (Rflag)
  printf("pseudo-random forcing\n");
    else if (force) {
  printf("periodic forcing=");
  for (i=0;i<maxindex;i++)
    printf("%d",forcing[i]);
  printf("\n");
    }
    else
  printf("periodic forcing=01\n");
    if (Force) {
  printf("function forcing=");
  for (i=0;i<funcmaxindex;i++) {
    printf("%d",Forcing[i]);
  }
  printf("\n");
    }
    printf("numcolors=%d\n",numcolors-1);
}

static void
freemem(void)
{
  static int i;

	for (i=0;i<MAXFRAMES;i++)
		free(exponents[i]);
}

static void
setupmem(void)
{
  static int i;

	for (i=0;i<MAXFRAMES;i++) {
		if((exponents[i]=
		    (double *)malloc(sizeof(double)*width*height))==NULL){
		    fprintf(stderr,"Error malloc'ing exponent array.\n");
		    exit(-1);
		}
	}
}

static void
setforcing(void)
{
  static int i;
  for (i=0;i<MAXINDEX;i++)
    forcing[i] = (random() > prob) ? 0 : 1;
}
