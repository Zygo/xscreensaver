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

#define NCOLORS 100
#define FULL_CYCLE 429496729
#define START_ARC 0
#define END_ARC 23040

struct info {
    Bool            writable;               /* Is the screen writable */
    double         xspeed;               /* A factor to modify the horizontal movement */
    double         yspeed;               /* A factor to modify vertical movement */
    double         xamplitude;
    double         yamplitude;
    int                whirlies;               /*  How many whirlies per line do you want? */
    int                nlines;                 /*  How many lines of whirlies do you want? */
    int                half_width;         /* 1/2 the width of the screen */
    int                half_height;
    int                speed;
    int                trail;
    int                color_modifier;
    double                xoffset;
    double                yoffset;
    double                offset_period;
    int                       wrap;
};

enum object_mode {
    spin_mode, funky_mode, circle_mode, linear_mode, test_mode, fun_mode, innie_mode, lissajous_mode
} mode;

static void explain(int, struct info *, Display *, Window, GC);

static void spin(unsigned long int, struct info *, int *, int);
static void funky(unsigned long int, struct info *, int *, int);
static void circle(unsigned long int, struct info *, int *, int);
static void fun(unsigned long int, struct info *, int *, int);
static void linear(unsigned long int, struct info *, int *, int);
static void lissajous(unsigned long int, struct info *, int *, int);
static void test(unsigned long int, struct info *, int *, int);
static void innie(unsigned long int, struct info *, int *, int, double);



XColor colors[NCOLORS];
int ncolors = NCOLORS;
char *progclass = "Whirlygig";

char *defaults [] = {
  ".background: black",
  ".foreground: white",
  "*xspeed: 1.0",
  "*yspeed: 1.0",
  "*xamplitude: 1.0",
  "*yamplitude: 1.0",
  "*whirlies: -1",
  "*nlines: -1",
  "*xmode: change",
  "*ymode: change",
  "*speed: 1",
  "*trail: 0",
  "*color_modifier: -1",
  "*start_time: -1",
  "*explain: 0",
  "*xoffset: 1.0",
  "*yoffset: 1.0",
  "*offset_period:    1",
  "*wrap:               0",
  0
};

XrmOptionDescRec options [] = {
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
  { "-trail",           ".trail", XrmoptionSepArg, 0 },
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
  { "-explain",                    ".explain", XrmoptionSepArg, 0 },
      /*  Specify whether or not to print an explanation of the function used. */
  { "-wrap",                      ".wrap", XrmoptionSepArg, 0 },
      /* Specify if you want whirlies which are out of the boundary of the screen to be
         wrapped around to the other side */
  { 0, 0, 0, 0 }
};


static const char funky_explanation[] =
"Funky mode is me goofing off.";

static const char test_explanation[] =
"Test mode is a mode that I play around with ideas in.";

static const char spin_explanation[] =
"Spin mode is a simple sin/cos with every argument modified";

static const char circle_explanation[] =
"Circle mode graphs the x and y positions as you trace the edge of a circle over time.";

static const char fun_explanation[] =
"Fun mode is the coolest.";

static const char linear_explanation[] =
"I draw a straight line -- woo hoo";

static const char lissajous_explanation[] =
"This draws a slightly modified lissajous curve";

static void
explain(int mode, struct info *info, Display *display, Window window, GC fgc)
{
    XClearWindow(display, window);
    switch(mode) {
        case spin_mode:
            XDrawString(display, window, fgc, 50, info->half_height-100, spin_explanation, strlen(spin_explanation));
            break;
        case funky_mode:
            XDrawString(display, window, fgc, 50, info->half_height-100, funky_explanation, strlen(funky_explanation));
            break;
        case circle_mode:
            XDrawString(display, window, fgc, 50, info->half_height-100, circle_explanation, strlen(circle_explanation));
            break;
        case fun_mode:
            XDrawString(display, window, fgc, 50, info->half_height-100, fun_explanation, strlen(fun_explanation));
            break;
        case linear_mode:
            XDrawString(display, window, fgc, 50, info->half_height-100, linear_explanation, strlen(linear_explanation));
            break;
    case lissajous_mode:
            XDrawString(display, window, fgc, 50, info->half_height-100, lissajous_explanation, strlen(linear_explanation));
      
    }
    XSync(display, False);
    sleep(3);
    XClearWindow(display, window);
}

static void
funky(unsigned long int the_time, struct info *info, int pos[], int index)
{
    double new_time = ((the_time % 360 ) / 180.0) * M_PI;
    if (index == 0) {
        double time_modifier = cos(new_time / 180.0);
        double the_cos = cos((new_time * (double)info->xspeed) + (time_modifier * 80.0));
        double dist_mod_x = cos(new_time) * (info->half_width - 50);
        pos[index]= info->xamplitude * (the_cos * dist_mod_x) + info->half_width;
    }
    else {
        double new_time = ((the_time % 360 ) / 180.0) * M_PI;
        double time_modifier = sin(new_time / 180.0);
        double the_sin = sin((new_time * (double)info->yspeed) + (time_modifier * 80.0));
        double dist_mod_y = sin(new_time) * (info->half_height - 50);
        pos[index] = info->yamplitude * (the_sin * dist_mod_y) + info->half_height;
    }
}

static void
innie(unsigned long int the_time, struct info *info, int pos[], int index, double modifier)
{
    double frequency = 2000000.0 + (modifier * cos(((double)the_time / 100.0)));
    double arg = (double)the_time / frequency;
    double amplitude = 200.0 * cos(arg);
    double fun = 150.0 * cos((double)the_time / 2000.0);
    int vert_mod, horiz_mod;
    if (index == 0) {
        horiz_mod = (int)(fun * cos((double)the_time / 100.0)) + info->half_width;
        pos[index] = (amplitude * cos((double)the_time / 100.0 * info->xspeed)) + horiz_mod;
    }
    else {
        vert_mod = (int)(fun * sin((double)the_time / 100.0)) + info->half_height;
        pos[index] = (amplitude * sin((double)the_time / 100.0 * info->yspeed)) + vert_mod;
    }
}

static void
lissajous(unsigned long int the_time, struct info *info, int pos[], int index)
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
      result = info->xamplitude * 200.0 * sin((weird * time) + fun) + info->half_width;
  }
  else {
      result = info->yamplitude * 200.0 * sin(time) + info->half_height;
  }
  pos[index] = result;
} 

static void
circle(unsigned long int the_time, struct info *info, int pos[], int index)
{
    int result;
    if (index == 0) {
        result = info->xamplitude * (cos((double)the_time / 100.0 * info->xspeed) * (info->half_width / 2)) + info->half_width;
    }
    else {
        result = info->yamplitude * (sin((double)the_time / 100.0 * info->yspeed) * (info->half_height / 2)) + info->half_height;
    }
    pos[index] = result;
}

#if 0
static void
mod(unsigned long int the_time, struct info *info, int pos[], int index)
{
    int amplitude;
    int max = info->half_width;
    if ((the_time % (max * 2)) < max)
        amplitude = max - ((the_time % (max * 2)) - max);
    else
        amplitude = the_time % (max * 2);
    amplitude = amplitude - max;
}
#endif

static void
spin(unsigned long int the_time, struct info *info, int pos[], int index)
{
    double funky = (((the_time % 360) * 1.0) / 180.0) * M_PI;
    if (index ==0) {
    double the_cos = cos((double)the_time / (180.0 * info->xspeed));
    double dist_mod_x = cos((double)funky) * (info->half_width - 50);
    pos[index] = info->xamplitude * (the_cos * dist_mod_x) + info->half_width;
    }
    else {
    double the_sin = sin((double)the_time / (180.0 * info->yspeed));
    double dist_mod_y = sin((double)funky) * (info->half_height - 50);
    pos[index] = info->yamplitude * (the_sin * dist_mod_y) + info->half_height;
    }
}

static void
fun(unsigned long int the_time, struct info *info, int pos[], int index)
{
    int amplitude;
    int max = info->half_width;
    if ((the_time % (max * 2)) < max)
        amplitude = max - ((the_time % (max * 2)) - max);
    else
        amplitude = the_time % (max * 2);
    amplitude = amplitude - max;
    if (index ==0) {
        pos[index] = (amplitude * cos((double)the_time / 100.0 * info->xspeed)) + info->half_width;
    }
    else {
        pos[index] = (amplitude * sin((double)the_time / 100.0 * info->yspeed)) + info->half_height;
    }
}

static void
linear(unsigned long int the_time, struct info *info, int pos[], int index)
{
    if (index == 0)   /* Calculate for the x axis */
        pos[index] = ((the_time / 2) % (info->half_width * 2));
    else
        pos[index] = ((the_time / 2) % (info->half_height * 2));
}

static void
test(unsigned long int the_time, struct info *info, int pos[], int index)
{
    if (index == 0) {
        pos[index] = info->xamplitude * (cos((double)the_time / 100.0 * info->xspeed) * (info->half_width / 2)) + info->half_width;
    }
    else {
        pos[index] = info->yamplitude * (sin((double)the_time / 100.0 * info->yspeed) * (info->half_height / 2)) + info->half_height;
    }   
}

static int preen(int current, int max) {
    if (current > max)
        current=current-max;
    if (current < 0)
        current=current+max;
    return(current);
}

static void
smoothen(int x, int lastx, int y, int lasty, int size, int last_color, XColor *colors, Display *display, Window window, GC bgc, int screen, struct info *info)
{
    double xdistance = abs((double)x-(double)lastx);
    double ydistance = abs((double)y-(double)lasty);
    double distance = sqrt(((xdistance * xdistance) + (ydistance * ydistance)) );
    double slope = (((double)y-(double)lasty) / ((double)x-(double)lastx));
    printf("Starting smoothen with values: %f, %f, %f, %f\n", xdistance, ydistance, distance, slope);
    if (distance > 2.0) {
        int newx = (int)((xdistance / distance) * slope);
        int newy = (int)((ydistance / distance) * slope);
        if (! info->trail) {
            XSetForeground(display, bgc, BlackPixel(display, screen));
            XFillArc(display, window, bgc, lastx, lasty, size, size, START_ARC, END_ARC);
        }
        XSetForeground(display, bgc, colors[last_color].pixel);
        XFillArc(display, window, bgc, newx, newy, size, size, START_ARC, END_ARC);
        XSync(display, False);
        smoothen(newx, x, newy, y, size, last_color, colors, display, window, bgc, screen, info);
    }
}


void
screenhack (Display *display, Window window)
{
        /*  The following are all X related toys */
    XGCValues gcv;      /* The structure to hold the GC data */
    XWindowAttributes xgwa;       /*  A structure to hold window data */
    GC fgc, bgc;
    int screen;

    unsigned long int current_time = 0; /* The global int telling the current time */
    unsigned long int start_time = current_time;
    struct info  *info = (struct info *)malloc(sizeof(struct info));      /* Dont forget to call free() later */
    char *xmode_str, *ymode_str; /* holds the current mode for x and y computation */
        /* pos is the current position x,y -- last_x contains one cell of every x coordinate
           for every position of every whirly in every line up to 100 whirlies in 100 lines
           -- lasy_y and last_size hold the same information for y and size respectively */
    int pos[2], last_x[100][100], last_y[100][100], last_size[100][100];
    int current_color;
    int wrap;
    int xmode, ymode;
    double modifier;    /* for innie */
        /* Set up the X toys that I will be using later */
    screen = DefaultScreen(display);
    XGetWindowAttributes (display, window, &xgwa);
    gcv.foreground = get_pixel_resource("foreground", "Foreground", display, xgwa.colormap);
    fgc = XCreateGC (display, window, GCForeground, &gcv);
    gcv.foreground = get_pixel_resource("background", "Background", display, xgwa.colormap);
    bgc = XCreateGC (display, window, GCForeground, &gcv);
    make_uniform_colormap (display, xgwa.visual, xgwa.colormap, colors, &ncolors, True, &info->writable, True);
        /* info is a structure holding all the random pieces of information I may want to 
           pass to my baby functions -- much of it I may never use, but it is nice to
           have around just in case I want it to make a funky function funkier */
    info->writable = get_boolean_resource ("cycle", "Boolean");
    info->xspeed = get_float_resource("xspeed" , "Float");
    info->yspeed = get_float_resource("yspeed" , "Float");
    info->xamplitude = get_float_resource("xamplitude", "Float");
    info->yamplitude = get_float_resource("yamplitude", "Float");
    info->offset_period = get_float_resource("offset_period", "Float");
    info->whirlies = get_integer_resource("whirlies", "Integer");
    info->nlines = get_integer_resource("nlines", "Integer");
    info->half_width = xgwa.width / 2;
    info->half_height = xgwa.height / 2;
    info->speed = get_integer_resource("speed" , "Integer");
    info->trail = get_integer_resource("trail", "Integer");
    info->color_modifier = get_integer_resource("color_modifier", "Integer");
    info->xoffset = get_float_resource("xoffset", "Float");
    info->yoffset = get_float_resource("yoffset", "Float");
    xmode_str = get_string_resource("xmode", "Mode");
    ymode_str = get_string_resource("ymode", "Mode");
    wrap = get_integer_resource("wrap", "Integer");
    modifier = 3000.0 + (1500.0 * random() / (RAND_MAX + 1.0));
    if (! xmode_str) xmode = spin_mode;
    else if (! strcmp (xmode_str, "spin")) xmode = spin_mode;
    else if (! strcmp (xmode_str, "funky")) xmode = funky_mode;
    else if (! strcmp (xmode_str, "linear")) xmode = linear_mode;
    else if (! strcmp (xmode_str, "fun")) xmode = fun_mode;
    else if (! strcmp (xmode_str, "innie")) xmode = innie_mode;
    else if (! strcmp (xmode_str, "lissajous")) xmode = lissajous_mode;
    else if (! strcmp (xmode_str, "test")) xmode = test_mode;
    else {
        xmode = spin_mode;
    }
    if (! ymode_str) ymode = spin_mode;
    else if (! strcmp (ymode_str, "spin")) ymode = spin_mode;
    else if (! strcmp (ymode_str, "funky")) ymode = funky_mode;
    else if (! strcmp (ymode_str, "linear")) ymode = linear_mode;
    else if (! strcmp (ymode_str, "fun")) ymode = fun_mode;
    else if (! strcmp (ymode_str, "innie")) ymode = innie_mode;
    else if (! strcmp (ymode_str, "lissajous")) ymode = lissajous_mode;
    else if (! strcmp (ymode_str, "test")) ymode = test_mode;
    else {
        ymode = spin_mode;
    }

    if (get_integer_resource("start_time", "Integer") == -1)
        current_time = (unsigned long int)(random());
    else
        current_time = get_integer_resource("start_time", "Integer");
    if (info->whirlies == -1)
        info->whirlies = 1 + (int)(15.0 * random() / (RAND_MAX + 1.0));
    if (info->nlines == -1)
        info->nlines = 1 + (int)(5.0 * random() / (RAND_MAX + 1.0));
    if (info->color_modifier == -1)
        info->color_modifier = 1 + (int)(25.0 * random() / (RAND_MAX + 1.0));
    if (get_integer_resource("explain", "Integer") == 1)
        explain(mode, info, display, window, fgc);
    current_color = 1 + (int)((double)NCOLORS * random() / (RAND_MAX + 1.0));
        /* Now that info is full, lets play! */
    
    while (1) {
        int wcount;  /* wcount is a counter incremented for every whirly take note of
                        internal_time before you mess with it */
        int change_time = 4000;
        if (! strcmp (xmode_str, "change") && ! strcmp (ymode_str, "change")) {
            if ((current_time - start_time) > change_time) {
                start_time = current_time;
                xmode = 1 + (int)(4.0 * random() / (RAND_MAX + 1.0));
                ymode = 1 + (int)(4.0 * random() / (RAND_MAX + 1.0));
            }
        }
        else if (! strcmp (xmode_str, "change")) {
            if ((current_time - start_time) > change_time) {
                start_time = current_time;
                xmode = 1 + (int)(3.5 * random() / (RAND_MAX + 1.0));
            }
        }
        else if (! strcmp (ymode_str, "change")) {
            if ((current_time - start_time) > change_time) {
                start_time = current_time;
                ymode = 1 + (int)(3.0 * random() / (RAND_MAX + 1.0));
                printf("Changing ymode to %d\n", ymode);
            }
        }
        if (++current_color >= NCOLORS)
            current_color = 0;
        for (wcount = 0; wcount < info->whirlies; wcount++) {
            int lcount; /* lcount is a counter for every line -- take note of the offsets changing */
            int internal_time = current_time;
            int color_offset = (current_color + (info->color_modifier * wcount)) % NCOLORS;
            if (current_time == 0)
                internal_time = 0;
            else
                    /* I want the distance between whirlies to increase more each whirly */
                internal_time = current_time + (10 * wcount) + (wcount * wcount); 
            switch (xmode) {
                    /* All these functions expect an int time, the struct info, a pointer to an array of positions, 
                       and the index that the the function will fill of the array */
                case spin_mode:
                    spin(internal_time, info, pos, 0);
                    break;
                case funky_mode:
                    funky(internal_time, info, pos, 0);
                    break;
                case circle_mode:
                    circle(internal_time, info, pos, 0);
                    break;
                case linear_mode:
                    linear(internal_time, info, pos, 0);
                    break;
                case fun_mode:
                    fun(internal_time, info, pos, 0);
                    break;
                case test_mode:
                    test(internal_time, info, pos, 0);
                    break;
                case innie_mode:
                    innie(internal_time, info, pos, 0, modifier);
                    break;
                case lissajous_mode:
                    lissajous(internal_time, info, pos, 0);
                    break;
                default:
                    spin(internal_time, info, pos, 0);
                    break;
            }   /* End of the switch for the x position*/
            switch (ymode) {
                case spin_mode:
                    spin(internal_time, info, pos, 1);
                    break;
                case funky_mode:
                    funky(internal_time, info, pos, 1);
                    break;
                case circle_mode:
                    circle(internal_time, info, pos, 1);
                    break;
                case linear_mode:
                    linear(internal_time, info, pos, 1);
                    break;
                case fun_mode:
                    fun(internal_time, info, pos, 1);
                    break;
                case test_mode:
                    test(internal_time, info, pos, 1);
                    break;
                case innie_mode:
                    innie(internal_time, info, pos, 1, modifier);
                    break;
                case lissajous_mode:
                    lissajous(internal_time, info, pos, 1);
                    break;
                default:
                    spin(internal_time, info, pos, 1);
                    break;
            } /* End of the switch for the y position*/
            for (lcount = 0; lcount < info->nlines; lcount++) {
                double arg = (double)((internal_time * info->offset_period) / 90.0); 
                double line_offset = 20.0 * (double)lcount * sin(arg); 
                int size;
                size = (int)(15.0 + 5.0 * sin((double)internal_time / 180.0));
/* First delete the old circle... */
                if (! info->trail) {
                    XSetForeground(display, bgc, BlackPixel(display, screen));
                    XFillArc(display, window, bgc, last_x[wcount][lcount], last_y[wcount][lcount], last_size[wcount][lcount], last_size[wcount][lcount], START_ARC, END_ARC);
                }
                    /* Now, lets draw in the new circle */
                {  /* Starting new scope for local x_pos and y_pos */
                    int xpos, ypos;
                    if (wrap) {
                        xpos = preen((int)(info->xoffset*line_offset)+pos[0], info->half_width * 2);
                        ypos = preen((int)(info->yoffset*line_offset)+pos[1], info->half_height * 2);
                    }
                    else {
                        xpos = (int)(info->xoffset*line_offset)+pos[0];
                        ypos = (int)(info->yoffset*line_offset)+pos[1]; 
                    }
                    if (start_time == current_time) {
                            /* smoothen should move from one mode to another prettily... */
/* 
   smoothen(xpos, last_x[wcount][lcount], ypos, last_y[wcount][lcount], size, color_offset, colors, display, window, bgc, screen, info);
 */
                    }
                    last_x[wcount][lcount] = xpos;
                    last_y[wcount][lcount] = ypos;
                    last_size[wcount][lcount] = size;
                    XSetForeground(display, bgc, colors[color_offset].pixel);
                    XFillArc(display, window, bgc, xpos, ypos, size, size, START_ARC, END_ARC);
                } /* End of my temporary scope for xpos and ypos */
            }  /* End of the for each line in nlines */
        } /* End of the for each whirly in whirlies */
        XSync(display, False);
        if (current_time == FULL_CYCLE)
            current_time = 1;
        else
            current_time = current_time + info->speed;
        screenhack_handle_events(display);
        if (!info->writable)
            usleep(10000);
    }   /*  End the while loop! */
    free(info);
}
