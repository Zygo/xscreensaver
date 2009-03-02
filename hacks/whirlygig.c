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
#include <math.h>
#include "screenhack.h"
#include <stdio.h>

static XColor colors[1000];
static int ncolors = 1000;
static int current_color = 0;

int 
draw_dot(Display *dpy, 
         Window window, 
         Colormap cmap, 
         GC fgc, GC bgc,
         int current_time,
         int origin_x, int origin_y,
         int screen_num, double xspeed, double yspeed,
         int whirlies, int nlines) 
{
    int size, last_size;  /* The size of my dot */
    int start_arc = 0;  /* Start my circle at 0 degrees */
    int end_arc = 23040;  /*  The number of degrees in a X circle (360 * 64) */
    int horiz, vert;    /*  These will contain the new x,y coordinates to put my dot */
    int last_horiz ,last_vert;  /*  These contain the positions to black out */
    int last_time;
    int count, line_count;
    int line_offset, last_line_offset;
    int color_offset;
    XWindowAttributes xgwa;
    XGetWindowAttributes (dpy, window, &xgwa);
    if (++current_color >= ncolors)
        current_color = 0;
    for (count = 0; count < whirlies; count++) { 
        color_offset = (current_color + (10 * count )) % ncolors;
        if (current_time == count) { 
            last_time = current_time; 
        }
        else {
            current_time = current_time + count;
            last_time = current_time - 1;
        }
        last_horiz = compute_x(last_time, origin_x, xspeed);
        last_vert = compute_y(last_time, origin_y, yspeed);
        horiz = compute_x(current_time, origin_x, xspeed);
        vert = compute_y(current_time, origin_y, yspeed);
        for (line_count = 0; line_count < nlines; line_count++) { 
            last_line_offset = (int)(80.0 * line_count * sin((double)last_time / 90));
            line_offset = (int)(80.0 * line_count * sin((double)current_time / 90));
            last_size = (int)(15.0 + 5.0 * sin((double)last_time / 180.0));
            size = (int)(15.0 + 5.0 * sin((double)current_time / 180.0));
                /* Get rid of the old circle */
            XSetForeground(dpy, bgc, BlackPixel(dpy, screen_num));
            XFillArc(dpy, window, bgc, last_horiz, last_line_offset+last_vert, last_size, last_size, start_arc, end_arc);
                /* Draw in the new circle */
            XSetForeground(dpy, bgc, colors[color_offset].pixel);
            XFillArc(dpy, window, bgc, horiz, line_offset+vert, size, size, start_arc, end_arc);
        } 
    }
    XSync (dpy, False); 
    if (current_time == 23040)
    { 
        return(1);
    }
    else 
    {
        return(0);
    }
}

int compute_x(int the_time, int origin, double xspeed) 
{
    double funky = (((the_time % 360) * 1.0) / 180.0) * M_PI;
    double the_cosine = cos((double)the_time / (180.0 * xspeed));
    double dist_mod_x = cos((double)funky) * (origin - 50);
    int horiz_pos = origin + (the_cosine * dist_mod_x);
    return(horiz_pos);
}

int compute_y(int the_time, int origin, double yspeed)
{
    double funky = (((the_time % 360) * 1.0) / 180.0) * M_PI;
    double the_sine = sin((double)the_time / (180.0 * yspeed));
    double dist_mod_y = sin((double)funky) * (origin - 50);
    int vert_pos = origin + (the_sine * dist_mod_y);
    return(vert_pos);
}

char *progclass = "Whirlygig";

char *defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*xspeed:             1.0",
  "*yspeed:             1.0",
  "*whirlies:           10",
  "*nlines:             1",
  0
};

XrmOptionDescRec options [] = {
  { "-xspeed",          ".xspeed", XrmoptionSepArg, 0 },
  { "-yspeed",          ".yspeed", XrmoptionSepArg, 0 },
  { "-whirlies",         ".whirlies",XrmoptionSepArg, 0 },
  { "-nlines",         ".nlines",XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void screenhack (Display *dpy, Window window)
{
    GC fgc, bgc;  /* the foreground and background graphics contexts */
    XGCValues gcv;      /* The structure to hold the GC data */
    XWindowAttributes xgwa;       /*  A structure to hold window data */
    Bool writable = get_boolean_resource ("cycle", "Boolean");
    double xspeed = get_float_resource( "xspeed", "Float");
    double yspeed = get_float_resource( "yspeed", "Float");
    int whirlies = get_integer_resource( "whirlies", "Integer");
    int nlines = get_integer_resource( "nlines", "Integer");
    int time_counter = 0;  /* This will be incremented in my while loop so that I can move my circle */
    int screen_num = DefaultScreen(dpy);
    int half_width, half_height;
    XGetWindowAttributes (dpy, window, &xgwa);

    half_width = xgwa.width / 2;
    half_height = xgwa.height / 2;

    gcv.foreground = get_pixel_resource("foreground", "Foreground", dpy, xgwa.colormap);
    fgc = XCreateGC (dpy, window, GCForeground, &gcv);
    gcv.foreground = get_pixel_resource("background", "Background", dpy, xgwa.colormap);
    bgc = XCreateGC (dpy, window, GCForeground, &gcv);
    make_uniform_colormap (dpy, xgwa.visual, xgwa.colormap, 
                           colors, &ncolors, 
                           True, &writable, True);
    mono_p = True;
    if (!mono_p)
    {
        GC tmp = fgc;
        fgc = bgc;
        bgc = tmp;
    }

    while (1)
    {
            /* draw_dot will return an interger.  If this interger is 0, then continue incrementing 
               time_counter, if it is 1, then reset it to zero...  It should only become 1 with the sine
               is 0 and the cosine is 1 */
        if (draw_dot(dpy, window, xgwa.colormap, fgc, bgc, time_counter, half_width, half_height, screen_num, xspeed , yspeed, whirlies, nlines) == 1) {
            time_counter = 1;
        }
        else {
            time_counter++;
        }
        screenhack_handle_events (dpy);

        if (!writable)
            usleep(10);
    }
}

