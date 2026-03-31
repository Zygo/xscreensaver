/*  Whirlygig -- an experiment in X programming
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 *  When I was in trigonometry class as a kid, I remember being fascinated
 *  by the beauty of the shapes one receives when playing with sine waves
 *  Here is a little experiment to show that beauty is simple
 */

#include <stdio.h>
#include <math.h>
#include "screenhack.h"

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
# include "xdbe.h"
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

#define NCOLORS 100
#define FULL_CYCLE 429496729
#define START_ARC 0
#define END_ARC 23040

struct info {
/*    Bool writable; / * Is the screen writable */
    double xspeed; /* A factor to modify the horizontal movement */
    double yspeed; /* A factor to modify vertical movement */
    double xamplitude;
    double yamplitude;
    int whirlies; /*  How many whirlies per line do you want? */
    int nlines;   /*  How many lines of whirlies do you want? */
    int half_width;         /* 1/2 the width of the screen */
    int half_height;
    int speed;
    int trail;
    int color_modifier;
    double xoffset;
    double yoffset;
    double offset_period;
    Bool wrap;
};

enum object_mode {
    spin_mode, funky_mode, circle_mode, linear_mode, test_mode, fun_mode, innie_mode, lissajous_mode
};

struct state {
  Display *dpy;
  Window window;

    XGCValues gcv;      /* The structure to hold the GC data */
    XWindowAttributes xgwa;       /*  A structure to hold window data */
    Pixmap b, ba;	/* double-buffer to reduce flicker */
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
    Bool dbeclear_p;
    XdbeBackBuffer backb;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

    GC fgc, bgc;
    int screen;
    Bool dbuf;

    unsigned long int current_time; /* The global int telling the current time */
    unsigned long int start_time;
    struct info  *info;
    char *xmode_str, *ymode_str; /* holds the current mode for x and y computation */

 /* pos is the current position x,y -- last_x contains one cell of
    every x coordinate for every position of every whirly in every
    line up to 100 whirlies in 100 lines -- lasy_y and last_size hold
    the same information for y and size respectively */

    int pos[2], last_x[100][100], last_y[100][100], last_size[100][100];
    int current_color;
    Bool wrap;
    int xmode, ymode;
    double modifier;    /* for innie */

   XColor colors[NCOLORS];
   int ncolors;
   int explaining;
};

static void draw_explain_string(struct state *, int, int, Display *, Window, GC);
static void spin(struct state *, unsigned long int, struct info *, int *, int);
static void funky(struct state *, unsigned long int, struct info *, int *, int);
static void circle(struct state *, unsigned long int, struct info *, int *, int);
static void fun(struct state *, unsigned long int, struct info *, int *, int);
static void linear(struct state *, unsigned long int, struct info *, int *, int);
static void lissajous(struct state *, unsigned long int, struct info *, int *, int);
static void test(struct state *, unsigned long int, struct info *, int *, int);
static void innie(struct state *, unsigned long int, struct info *, int *, int, double);



static const char spin_explanation[] =
"Spin mode is a simple sin/cos with every argument modified";

static const char funky_explanation[] =
"Funky mode is me goofing off.";

static const char circle_explanation[] =
"Circle mode graphs the x and y positions as you trace the edge of a circle over time.";

static const char linear_explanation[] =
"Linear mode draws a straight line";

static const char test_explanation[] =
"Test mode is a mode that I play around with ideas in.";

static const char fun_explanation[] =
"Fun mode is the coolest.";

static const char innie_explanation[] =
"Innie mode does something or other. Looks cool, though.";

static const char lissajous_explanation[] =
"Lissajous mode draws a slightly modified lissajous curve";

static void
draw_explain_string(struct state *st, int mode, int offset, Display *dpy, Window window, GC fgc) 
{
  switch(mode) {
  case spin_mode:
    XDrawString(st->dpy, st->window, st->fgc, 50, offset, 
                (char*) spin_explanation, strlen(spin_explanation));
    break;
  case funky_mode:
    XDrawString(st->dpy, st->window, st->fgc, 50, offset, 
                (char*) funky_explanation, strlen(funky_explanation));
    break;
  case circle_mode:
    XDrawString(st->dpy, st->window, st->fgc, 50, offset, 
                (char*) circle_explanation, strlen(circle_explanation));
    break;
  case linear_mode:
    XDrawString(st->dpy, st->window, st->fgc, 50, offset, 
                (char*) linear_explanation, strlen(linear_explanation));
    break;
  case test_mode:
    XDrawString(st->dpy, st->window, st->fgc, 50, offset, 
                (char*) test_explanation, strlen(test_explanation));
    break;
  case fun_mode:
    XDrawString(st->dpy, st->window, st->fgc, 50, offset, 
                (char*) fun_explanation, strlen(fun_explanation));
    break;
  case innie_mode:
    XDrawString(st->dpy, st->window, st->fgc, 50, offset, 
                (char*) innie_explanation, strlen(innie_explanation));
    break;
  case lissajous_mode:
    XDrawString(st->dpy, st->window, st->fgc, 50, offset, 
                (char*) lissajous_explanation, strlen(linear_explanation));    
  }
}

static void
funky(struct state *st, unsigned long int the_time, struct info *info, int pos[], int index)
{
    double new_time = ((the_time % 360 ) / 180.0) * M_PI;
    if (index == 0) {
        double time_modifier = cos(new_time / 180.0);
        double the_cos = cos((new_time * (double)st->info->xspeed) + (time_modifier * 80.0));
        double dist_mod_x = cos(new_time) * (st->info->half_width - 50);
        st->pos[index]= st->info->xamplitude * (the_cos * dist_mod_x) + st->info->half_width;
    }
    else {
        double new_time = ((the_time % 360 ) / 180.0) * M_PI;
        double time_modifier = sin(new_time / 180.0);
        double the_sin = sin((new_time * (double)st->info->yspeed) + (time_modifier * 80.0));
        double dist_mod_y = sin(new_time) * (st->info->half_height - 50);
        st->pos[index] = st->info->yamplitude * (the_sin * dist_mod_y) + st->info->half_height;
    }
}

static void
innie(struct state *st, unsigned long int the_time, struct info *info, int pos[], int index, double modifier)
{
    double frequency = 2000000.0 + (st->modifier * cos(((double)the_time / 100.0)));
    double arg = (double)the_time / frequency;
    double amplitude = 200.0 * cos(arg);
    double fun = 150.0 * cos((double)the_time / 2000.0);
    int vert_mod, horiz_mod;
    if (index == 0) {
        horiz_mod = (int)(fun * cos((double)the_time / 100.0)) + st->info->half_width;
        st->pos[index] = (amplitude * cos((double)the_time / 100.0 * st->info->xspeed)) + horiz_mod;
    }
    else {
        vert_mod = (int)(fun * sin((double)the_time / 100.0)) + st->info->half_height;
        st->pos[index] = (amplitude * sin((double)the_time / 100.0 * st->info->yspeed)) + vert_mod;
    }
}

static void
lissajous(struct state *st, unsigned long int the_time, struct info *info, int pos[], int index)
{
      /*  This is a pretty standard lissajous curve
           x = a sin(nt + c) 
           y = b sin(t) 
           The n and c modifiers are cyclic as well, however... */
  int result;
  double time = (double)the_time / 100.0;
  double fun = 15.0 * cos((double)the_time / 800.0);
  double weird = cos((time / 1100000.0) / 1000.0);

  if (index == 0) {
      result = st->info->xamplitude * 200.0 * sin((weird * time) + fun) + st->info->half_width;
  }
  else {
      result = st->info->yamplitude * 200.0 * sin(time) + st->info->half_height;
  }
  st->pos[index] = result;
} 

static void
circle(struct state *st, unsigned long int the_time, struct info *info, int pos[], int index)
{
    int result;
    if (index == 0) {
        result = st->info->xamplitude * (cos((double)the_time / 100.0 * st->info->xspeed) * (st->info->half_width / 2)) + st->info->half_width;
    }
    else {
        result = st->info->yamplitude * (sin((double)the_time / 100.0 * st->info->yspeed) * (st->info->half_height / 2)) + st->info->half_height;
    }
    st->pos[index] = result;
}

#if 0
static void
mod(unsigned long int the_time, struct info *info, int pos[], int index)
{
    int amplitude;
    int max = st->info->half_width;
    if ((the_time % (max * 2)) < max)
        amplitude = max - ((the_time % (max * 2)) - max);
    else
        amplitude = the_time % (max * 2);
    amplitude = amplitude - max;
}
#endif

static void
spin(struct state *st, unsigned long int the_time, struct info *info, int pos[], int index)
{
    double funky = (((the_time % 360) * 1.0) / 180.0) * M_PI;
    if (index ==0) {
    double the_cos = cos((double)the_time / (180.0 * st->info->xspeed));
    double dist_mod_x = cos((double)funky) * (st->info->half_width - 50);
    st->pos[index] = st->info->xamplitude * (the_cos * dist_mod_x) + st->info->half_width;
    }
    else {
    double the_sin = sin((double)the_time / (180.0 * st->info->yspeed));
    double dist_mod_y = sin((double)funky) * (st->info->half_height - 50);
    st->pos[index] = st->info->yamplitude * (the_sin * dist_mod_y) + st->info->half_height;
    }
}

static void
fun(struct state *st, unsigned long int the_time, struct info *info, int pos[], int index)
{
    int amplitude;
    int max = st->info->half_width;
    if ((the_time % (max * 2)) < max)
        amplitude = max - ((the_time % (max * 2)) - max);
    else
        amplitude = the_time % (max * 2);
    amplitude = amplitude - max;
    if (index ==0) {
        st->pos[index] = (amplitude * cos((double)the_time / 100.0 * st->info->xspeed)) + st->info->half_width;
    }
    else {
        st->pos[index] = (amplitude * sin((double)the_time / 100.0 * st->info->yspeed)) + st->info->half_height;
    }
}

static void
linear(struct state *st, unsigned long int the_time, struct info *info, int pos[], int index)
{
    if (index == 0)   /* Calculate for the x axis */
        st->pos[index] = ((the_time / 2) % (st->info->half_width * 2));
    else
        st->pos[index] = ((the_time / 2) % (st->info->half_height * 2));
}

static void
test(struct state *st, unsigned long int the_time, struct info *info, int pos[], int index)
{
    if (index == 0) {
        st->pos[index] = st->info->xamplitude * (cos((double)the_time / 100.0 * st->info->xspeed) * (st->info->half_width / 2)) + st->info->half_width;
    }
    else {
        st->pos[index] = st->info->yamplitude * (sin((double)the_time / 100.0 * st->info->yspeed) * (st->info->half_height / 2)) + st->info->half_height;
    }   
}

static int preen(int current, int max) {
    if (current > max)
        current=current-max;
    if (current < 0)
        current=current+max;
    return(current);
}

#if 0
static void
smoothen(struct state *st, int x, int lastx, int y, int lasty, int size, int last_color, XColor *colors, Display *dpy, Window window, GC bgc, int screen, struct info *info)
{
    double xdistance = abs((double)x-(double)lastx);
    double ydistance = abs((double)y-(double)lasty);
    double distance = sqrt(((xdistance * xdistance) + (ydistance * ydistance)) );
    double slope = (((double)y-(double)lasty) / ((double)x-(double)lastx));
    printf("Starting smoothen with values: %f, %f, %f, %f\n", xdistance, ydistance, distance, slope);
    if (distance > 2.0) {
        int newx = (int)((xdistance / distance) * slope);
        int newy = (int)((ydistance / distance) * slope);
        if (! st->info->trail) {
            XSetForeground(st->dpy, st->bgc, BlackPixel(st->dpy, st->screen));
            XFillArc(st->dpy, st->window, st->bgc, lastx, lasty, size, size, START_ARC, END_ARC);
        }
        XSetForeground(st->dpy, st->bgc, st->colors[last_color].pixel);
        XFillArc(st->dpy, st->window, st->bgc, newx, newy, size, size, START_ARC, END_ARC);
        smoothen(st, newx, x, newy, y, size, last_color, st->colors, st->dpy, st->window, st->bgc, st->screen, st->info);
    }
}
#endif


static void *
whirlygig_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = dpy;
  st->window = window;

  st->ncolors = NCOLORS;

    st->dbuf = get_boolean_resource (st->dpy, "doubleBuffer", "Boolean");

# ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
    st->dbuf = False;
# endif

    st->start_time = st->current_time;
    st->info = (struct info *)malloc(sizeof(struct info));

    st->screen = DefaultScreen(st->dpy);
    XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
    if (st->dbuf)
      {
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
        if (get_boolean_resource(st->dpy,"useDBE","Boolean"))
          {
            st->dbeclear_p = get_boolean_resource (st->dpy, "useDBEClear",
                                                   "Boolean");
            if (st->dbeclear_p)
              st->b = xdbe_get_backbuffer (st->dpy, st->window, XdbeBackground);
            else
              st->b = xdbe_get_backbuffer (st->dpy, st->window, XdbeUndefined);
            st->backb = st->b;
          }
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

	if (!st->b)
	  {
	    st->ba = XCreatePixmap (st->dpy, st->window, st->xgwa.width, st->xgwa.height,st->xgwa.depth);
	    st->b = st->ba;
	  }
      }
    else
      {
	st->b = st->window;
      }

    st->gcv.foreground = get_pixel_resource(st->dpy, st->xgwa.colormap, "foreground", "Foreground");
    st->fgc = XCreateGC (st->dpy, st->b, GCForeground, &st->gcv);
    st->gcv.foreground = get_pixel_resource(st->dpy, st->xgwa.colormap, "background", "Background");
    st->bgc = XCreateGC (st->dpy, st->b, GCForeground, &st->gcv);

#ifdef HAVE_JWXYZ  /* #### should turn off double-buffering instead */
    jwxyz_XSetAntiAliasing (dpy, st->fgc, False);
    jwxyz_XSetAntiAliasing (dpy, st->bgc, False);
#endif

    {
      Bool writable_p = False;
    make_uniform_colormap (st->xgwa.screen, st->xgwa.visual,
                           st->xgwa.colormap, st->colors, &st->ncolors,
                           True, &writable_p, True);
    }

    if (st->ba) XFillRectangle (st->dpy, st->ba, st->bgc, 0, 0, st->xgwa.width, st->xgwa.height);

        /* info is a structure holding all the random pieces of information I may want to 
           pass to my baby functions -- much of it I may never use, but it is nice to
           have around just in case I want it to make a funky function funkier */
/*    info->writable = get_boolean_resource (dpy, "cycle", "Boolean"); */
    st->info->xspeed = get_float_resource(st->dpy, "xspeed" , "Float");
    st->info->yspeed = get_float_resource(st->dpy, "yspeed" , "Float");
    st->info->xamplitude = get_float_resource(st->dpy, "xamplitude", "Float");
    st->info->yamplitude = get_float_resource(st->dpy, "yamplitude", "Float");
    st->info->offset_period = get_float_resource(st->dpy, "offset_period", "Float");
    st->info->whirlies = get_integer_resource(st->dpy, "whirlies", "Integer");
    st->info->nlines = get_integer_resource(st->dpy, "nlines", "Integer");
    st->info->half_width = st->xgwa.width / 2;
    st->info->half_height = st->xgwa.height / 2;
    st->info->speed = get_integer_resource(st->dpy, "speed" , "Integer");
    st->info->trail = get_boolean_resource(st->dpy, "trail", "Integer");
    st->info->color_modifier = get_integer_resource(st->dpy, "color_modifier", "Integer");
    st->info->xoffset = get_float_resource(st->dpy, "xoffset", "Float");
    st->info->yoffset = get_float_resource(st->dpy, "yoffset", "Float");
    st->xmode_str = get_string_resource(st->dpy, "xmode", "Mode");
    st->ymode_str = get_string_resource(st->dpy, "ymode", "Mode");
    st->wrap = get_boolean_resource(st->dpy, "wrap", "Boolean");
    st->modifier = 3000.0 + frand(1500.0);
    if (! st->xmode_str) st->xmode = spin_mode;
    else if (! strcmp (st->xmode_str, "spin")) st->xmode = spin_mode;
    else if (! strcmp (st->xmode_str, "funky")) st->xmode = funky_mode;
    else if (! strcmp (st->xmode_str, "circle")) st->xmode = circle_mode;
    else if (! strcmp (st->xmode_str, "linear")) st->xmode = linear_mode;
    else if (! strcmp (st->xmode_str, "test")) st->xmode = test_mode;
    else if (! strcmp (st->xmode_str, "fun")) st->xmode = fun_mode;
    else if (! strcmp (st->xmode_str, "innie")) st->xmode = innie_mode;
    else if (! strcmp (st->xmode_str, "lissajous")) st->xmode = lissajous_mode;
    else {
        st->xmode = random() % (int) lissajous_mode;
    }
    if (! st->ymode_str) st->ymode = spin_mode;
    else if (! strcmp (st->ymode_str, "spin")) st->ymode = spin_mode;
    else if (! strcmp (st->ymode_str, "funky")) st->ymode = funky_mode;
    else if (! strcmp (st->ymode_str, "circle")) st->ymode = circle_mode;
    else if (! strcmp (st->ymode_str, "linear")) st->ymode = linear_mode;
    else if (! strcmp (st->ymode_str, "test")) st->ymode = test_mode;
    else if (! strcmp (st->ymode_str, "fun")) st->ymode = fun_mode;
    else if (! strcmp (st->ymode_str, "innie")) st->ymode = innie_mode;
    else if (! strcmp (st->ymode_str, "lissajous")) st->ymode = lissajous_mode;
    else {
        st->ymode = random() % (int) lissajous_mode;
    }

    if (get_integer_resource(st->dpy, "start_time", "Integer") == -1)
        st->current_time = (unsigned long int)(random());
    else
        st->current_time = get_integer_resource(st->dpy, "start_time", "Integer");
    if (st->info->whirlies == -1)
        st->info->whirlies = 1 + (random() % 15);
    if (st->info->nlines == -1)
        st->info->nlines = 1 + (random() % 5);
    if (st->info->color_modifier == -1)
        st->info->color_modifier = 1 + (random() % 25);
    if (get_boolean_resource(st->dpy, "explain", "Integer"))
      st->explaining = 1;
    st->current_color = 1 + (random() % NCOLORS);

  return st;
}

static unsigned long
whirlygig_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int wcount;  /* wcount is a counter incremented for every whirly take note of
                  internal_time before you mess with it */
  int change_time = 4000;

  if (st->explaining == 1) {
    XClearWindow (st->dpy, st->window);
    draw_explain_string(st, st->xmode, st->info->half_height-100, 
                        st->dpy, st->window, st->fgc);
    st->explaining++;
    return 3000000;
  } else if (st->explaining == 2) {
    XClearWindow (st->dpy, st->window);
    st->explaining = 0;
  }

  if (! strcmp (st->xmode_str, "change") && ! strcmp (st->ymode_str, "change")) {
    if ((st->current_time - st->start_time) > change_time) {
      st->start_time = st->current_time;
      st->xmode = 1 + (random() % 4);
      st->ymode = 1 + (random() % 4);
    }
  }
  else if (! strcmp (st->xmode_str, "change")) {
    if ((st->current_time - st->start_time) > change_time) {
      st->start_time = st->current_time;
      st->xmode = 1 + (random() % 4);
    }
  }
  else if (! strcmp (st->ymode_str, "change")) {
    if ((st->current_time - st->start_time) > change_time) {
      st->start_time = st->current_time;
      st->ymode = 1 + (random() % 3);
      printf("Changing ymode to %d\n", st->ymode);
    }
  }
  if (++st->current_color >= NCOLORS)
    st->current_color = 0;
  for (wcount = 0; wcount < st->info->whirlies; wcount++) {
    int lcount; /* lcount is a counter for every line -- take note of the offsets changing */
    int internal_time = 0;
    int color_offset = (st->current_color + (st->info->color_modifier * wcount)) % NCOLORS;
    if (st->current_time != 0)
      /* I want the distance between whirlies to increase more each whirly */
      internal_time = st->current_time + (10 * wcount) + (wcount * wcount); 
    switch (st->xmode) {
      /* All these functions expect an int time, the struct info,
         a pointer to an array of positions, and the index that the 
         the function will fill of the array */
    case spin_mode:
      spin(st, internal_time, st->info, st->pos, 0);
      break;
    case funky_mode:
      funky(st, internal_time, st->info, st->pos, 0);
      break;
    case circle_mode:
      circle(st, internal_time, st->info, st->pos, 0);
      break;
    case linear_mode:
      linear(st, internal_time, st->info, st->pos, 0);
      break;
    case fun_mode:
      fun(st, internal_time, st->info, st->pos, 0);
      break;
    case test_mode:
      test(st, internal_time, st->info, st->pos, 0);
      break;
    case innie_mode:
      innie(st, internal_time, st->info, st->pos, 0, st->modifier);
      break;
    case lissajous_mode:
      lissajous(st, internal_time, st->info, st->pos, 0);
      break;
    default:
      spin(st, internal_time, st->info, st->pos, 0);
      break;
    }   /* End of the switch for the x position*/
    switch (st->ymode) {
    case spin_mode:
      spin(st, internal_time, st->info, st->pos, 1);
      break;
    case funky_mode:
      funky(st, internal_time, st->info, st->pos, 1);
      break;
    case circle_mode:
      circle(st, internal_time, st->info, st->pos, 1);
      break;
    case linear_mode:
      linear(st, internal_time, st->info, st->pos, 1);
      break;
    case fun_mode:
      fun(st, internal_time, st->info, st->pos, 1);
      break;
    case test_mode:
      test(st, internal_time, st->info, st->pos, 1);
      break;
    case innie_mode:
      innie(st, internal_time, st->info, st->pos, 1, st->modifier);
      break;
    case lissajous_mode:
      lissajous(st, internal_time, st->info, st->pos, 1);
      break;
    default:
      spin(st, internal_time, st->info, st->pos, 1);
      break;
    } /* End of the switch for the y position*/
    for (lcount = 0; lcount < st->info->nlines; lcount++) {
      double arg = (double)((internal_time * st->info->offset_period) / 90.0); 
      double line_offset = 20.0 * (double)lcount * sin(arg); 
      int size;
      size = (int)(15.0 + 5.0 * sin((double)internal_time / 180.0));
      /* First delete the old circle... */
      if (!st->info->trail
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
          && ( !st->dbeclear_p || !st->backb)
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
          ) {
        XSetForeground(st->dpy, st->bgc, BlackPixel(st->dpy, st->screen));
        XFillArc(st->dpy, st->b, st->bgc, st->last_x[wcount][lcount], st->last_y[wcount][lcount], st->last_size[wcount][lcount], st->last_size[wcount][lcount], START_ARC, END_ARC);
      }
      /* Now, lets draw in the new circle */
      {  /* Starting new scope for local x_pos and y_pos */
        int xpos, ypos;
        if (st->wrap) {
          xpos = preen((int)(st->info->xoffset*line_offset)+st->pos[0], st->info->half_width * 2);
          ypos = preen((int)(st->info->yoffset*line_offset)+st->pos[1], st->info->half_height * 2);
        }
        else {
          xpos = (int)(st->info->xoffset*line_offset)+st->pos[0];
          ypos = (int)(st->info->yoffset*line_offset)+st->pos[1]; 
        }
        if (st->start_time == st->current_time) {
          /* smoothen should move from one mode to another prettily... */

          /* Note: smoothen has not been modified to take the double
             buffering code into account, and needs to be hacked on
             before uncommenting.
          */
          /* 
             smoothen(xpos, last_x[wcount][lcount], ypos, last_y[wcount][lcount], size, color_offset, colors, dpy, window, bgc, screen, info);
          */
        }
        st->last_x[wcount][lcount] = xpos;
        st->last_y[wcount][lcount] = ypos;
        st->last_size[wcount][lcount] = size;
        XSetForeground(st->dpy, st->bgc, st->colors[color_offset].pixel);
        XFillArc(st->dpy, st->b, st->bgc, xpos, ypos, size, size, START_ARC, END_ARC);
      } /* End of my temporary scope for xpos and ypos */
    }  /* End of the for each line in nlines */
  } /* End of the for each whirly in whirlies */


#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  if (st->backb)
    {
      XdbeSwapInfo info[1];
      info[0].swap_window = st->window;
      info[0].swap_action = (st->dbeclear_p ? XdbeBackground : XdbeUndefined);
      XdbeSwapBuffers (st->dpy, info, 1);
    }
  else
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
    if (st->dbuf)
      {
        XCopyArea (st->dpy, st->b, st->window, st->bgc, 0, 0,
                   st->xgwa.width, st->xgwa.height, 0, 0);
      }

  if (st->current_time == FULL_CYCLE)
    st->current_time = 1;
  else
    st->current_time = st->current_time + st->info->speed;

  return 10000;
}


static void
whirlygig_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
whirlygig_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
whirlygig_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st->info);
  XFreeGC (dpy, st->fgc);
  XFreeGC (dpy, st->bgc);
  if (st->xmode_str) free (st->xmode_str);
  if (st->ymode_str) free (st->ymode_str);
  free (st);
}


static const char *whirlygig_defaults [] = {
  ".background: black",
  ".foreground: white",
  "*fpsSolid:	true",
  "*xspeed: 1.0",
  "*yspeed: 1.0",
  "*xamplitude: 1.0",
  "*yamplitude: 1.0",
  "*whirlies: -1",
  "*nlines: -1",
  "*xmode: change",
  "*ymode: change",
  "*speed: 1",
  "*trail: false",
  "*color_modifier: -1",
  "*start_time: -1",
  "*explain: False",
  "*xoffset: 1.0",
  "*yoffset: 1.0",
  "*offset_period:    1",
  "*wrap:               False",
  "*doubleBuffer:	True",
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  "*useDBEClear:	True",
  "*useDBE:		True",
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
  0
};

static XrmOptionDescRec whirlygig_options [] = {
  { "-xspeed",          ".xspeed", XrmoptionSepArg, 0 },
      /* xspeed is a modifier of the argument to cos -- changing it thus
         changes the frequency of cos */
  { "-yspeed",          ".yspeed", XrmoptionSepArg, 0 },
      /*  Similiarly, yspeed changes the frequency of sin */
  { "-xamplitude",      ".xamplitude", XrmoptionSepArg, 0 },
      /* A factor by which to increase/decrease the amplitude of the sin */
  { "-yamplitude",      ".yamplitude", XrmoptionSepArg, 0 },
      /* A factor by which to increase/decrease the amplitude of the cos */
  { "-whirlies",         ".whirlies",XrmoptionSepArg, 0 },
      /*  whirlies defines the number of circles to draw per line */
  { "-nlines",         ".nlines",XrmoptionSepArg, 0 },
      /* nlines is the number of lines of whirlies to draw */
  { "-xmode",        ".xmode", XrmoptionSepArg, 0 },
      /*  There are a few different modes that I have written -- each mode
          is in theory a different experiment with the possible modifiers to sin/cos */
  { "-ymode",       ".ymode", XrmoptionSepArg, 0 },
  { "-speed",        ".speed", XrmoptionSepArg, 0 },
      /*  This will modify how often it should draw, changing it will probably suck */
  { "-trail",           ".trail", XrmoptionNoArg, "True" },
      /* Control whether or not you want the old circles to be erased */
  { "-color_modifier",          ".color_modifier", XrmoptionSepArg, 0 },
      /*  How many colors away from the current should the next whirly be? */
  { "-start_time",                ".start_time", XrmoptionSepArg, 0 },
      /*  Specify exactly at what time to start graphing...  */
  { "-xoffset",                    ".xoffset", XrmoptionSepArg, 0 },
      /*  Tell the whirlies to be offset by this factor of a sin */
  { "-yoffset",                    ".yoffset", XrmoptionSepArg, 0 },
      /*  Tell the whirlies to be offset by this factor of a cos */
  { "-offset_period",          ".offset_period", XrmoptionSepArg, 0 },
      /*  Change the period of an offset cycle */
  { "-explain",                    ".explain", XrmoptionNoArg, "True" },
      /*  Specify whether or not to print an explanation of the function used. */
  { "-wrap",                      ".wrap", XrmoptionNoArg, "True" },
  { "-no-wrap",                   ".wrap", XrmoptionNoArg, "False" },
      /* Specify if you want whirlies which are out of the boundary of the screen to be
         wrapped around to the other side */
  { "-db",		".doubleBuffer", XrmoptionNoArg,  "True" },
  { "-no-db",		".doubleBuffer", XrmoptionNoArg,  "False" },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Whirlygig", whirlygig)
