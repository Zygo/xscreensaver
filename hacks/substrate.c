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
#include <X11/Xutil.h>
#include <stdio.h>
#include <sys/time.h>

#ifndef MAX_WIDTH
#include <limits.h>
#define MAX_WIDTH SHRT_MAX
#endif

#ifdef TIME_ME
#include <time.h>
#endif

#include <math.h>

/* this program goes faster if some functions are inline.  The following is
 * borrowed from ifs.c */
#if !defined( __GNUC__ ) && !defined(__cplusplus) && !defined(c_plusplus)
#undef inline
#define inline			/* */
#endif

#define STEP 0.42

/* Raw colormap extracted from pollockEFF.gif */
char *rgb_colormap[] = {
    "rgb:20/1F/21", "rgb:26/2C/2E", "rgb:35/26/26", "rgb:37/2B/27",
    "rgb:30/2C/2E", "rgb:39/2B/2D", "rgb:32/32/29", "rgb:3F/32/29",
    "rgb:38/32/2E", "rgb:2E/33/3D", "rgb:33/3A/3D", "rgb:47/33/29",
    "rgb:40/39/2C", "rgb:40/39/2E", "rgb:47/40/2C", "rgb:47/40/2E",
    "rgb:4E/40/2C", "rgb:4F/40/2E", "rgb:4E/47/38", "rgb:58/40/37",
    "rgb:65/47/2D", "rgb:6D/5D/3D", "rgb:74/55/30", "rgb:75/55/32",
    "rgb:74/5D/32", "rgb:74/64/33", "rgb:7C/6C/36", "rgb:52/31/52",
    "rgb:44/48/42", "rgb:4C/56/47", "rgb:65/5D/45", "rgb:6D/5D/44",
    "rgb:6C/5D/4E", "rgb:74/6C/43", "rgb:7C/6C/42", "rgb:7C/6C/4B",
    "rgb:6B/73/4B", "rgb:73/73/4B", "rgb:7B/7B/4A", "rgb:6B/6C/55",
    "rgb:69/6D/5E", "rgb:7B/6C/5D", "rgb:6B/73/53", "rgb:6A/74/5D",
    "rgb:72/7B/52", "rgb:7B/7B/52", "rgb:57/74/6E", "rgb:68/74/66",
    "rgb:9C/54/2B", "rgb:9D/54/32", "rgb:9D/5B/35", "rgb:93/6B/36",
    "rgb:AA/73/30", "rgb:C4/5A/27", "rgb:D9/52/23", "rgb:D8/5A/20",
    "rgb:DB/5A/23", "rgb:E5/70/37", "rgb:83/6C/4B", "rgb:8C/6B/4B",
    "rgb:82/73/5C", "rgb:93/73/52", "rgb:81/7B/63", "rgb:81/7B/6D",
    "rgb:92/7B/63", "rgb:D9/89/3B", "rgb:E4/98/32", "rgb:DF/A1/33",
    "rgb:E5/A0/37", "rgb:F0/AB/3B", "rgb:8A/8A/59", "rgb:B2/9A/58",
    "rgb:89/82/6B", "rgb:9A/82/62", "rgb:88/8B/7C", "rgb:90/9A/7A",
    "rgb:A2/82/62", "rgb:A1/8A/69", "rgb:A9/99/68", "rgb:99/A1/60",
    "rgb:99/A1/68", "rgb:CA/81/48", "rgb:EB/8D/43", "rgb:C2/91/60",
    "rgb:C2/91/68", "rgb:D1/A9/77", "rgb:C9/B9/7F", "rgb:F0/E2/7B",
    "rgb:9F/92/8B", "rgb:C0/B9/99", "rgb:E6/B8/8F", "rgb:C8/C1/87",
    "rgb:E0/C8/86", "rgb:F2/CC/85", "rgb:F5/DA/83", "rgb:EC/DE/9D",
    "rgb:F5/D2/94", "rgb:F5/DA/94", "rgb:F4/E7/84", "rgb:F4/E1/8A",
    "rgb:F4/E1/93", "rgb:E7/D8/A7", "rgb:F1/D4/A5", "rgb:F1/DC/A5",
    "rgb:F4/DB/AD", "rgb:F1/DC/AE", "rgb:F4/DB/B5", "rgb:F5/DB/BD",
    "rgb:F4/E2/AD", "rgb:F5/E9/AD", "rgb:F4/E3/BE", "rgb:F5/EA/BE",
    "rgb:F7/F0/B6", "rgb:D9/D1/C1", "rgb:E0/D0/C0", "rgb:E7/D8/C0",
    "rgb:F1/DD/C6", "rgb:E8/E1/C0", "rgb:F3/ED/C7", "rgb:F6/EC/CE",
    "rgb:F8/F2/C7", "rgb:EF/EF/D0", 0
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

struct field 
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

inline void start_crack(struct field *f, crack *cr) {
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

inline void make_crack(struct field *f) {
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

inline void point2rgb(int depth, unsigned long c, int *r, int *g, int *b) {
    switch(depth) {
        case 32:
        case 24:
            *g = (c & 0xff00) >> 8; 
            *r = (c & 0xff0000) >> 16; 
            *b = c & 0xff; 
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

inline unsigned long rgb2point(int depth, int r, int g, int b) {
    unsigned long ret = 0;

    switch(depth) {
        case 32:
            ret = 0xff000000;
        case 24:
            ret |= (r << 16) | (g << 8) | b;
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
inline unsigned long trans_point(int x1, int y1, unsigned long myc, float a, 
                                 struct field *f) {
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

    return 0;
}

inline void region_color(Display *dpy, Window window, GC fgc, struct field *f, 
                         crack *cr) {
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

        /* Draw sand bit */
        c = trans_point(drawx, drawy, cr->sandcolor, (0.1 - i / (grains * 10.0)), f);

        XSetForeground(dpy, fgc, c);
        XDrawPoint(dpy, window, fgc, (int) drawx, (int) drawy);
        XSetForeground(dpy, fgc, f->fgcolor);
    }
}

void build_substrate(struct field *f) {
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
    memset(f->cgrid, 10001, f->height * f->width * sizeof(int));

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


inline void movedrawcrack(Display *dpy, Window window, GC fgc, struct field *f, 
                          int cracknum) {
    /* Basically Crack::move() */

    int cx, cy;
    crack *cr = &(f->cracks[cracknum]);

    /* continue cracking */
    if ( !cr->curved ) {
        cr->x += ((float) STEP * cos(cr->t * M_PI/180));
        cr->y += ((float) STEP * sin(cr->t * M_PI/180));
    }
    else {
        float oldx, oldy;

        oldx = cr->x;
        oldy = cr->y;

        cr->x += ((float) cr->ys * cos(cr->t * M_PI/180));
        cr->y += ((float) cr->ys * sin(cr->t * M_PI/180));

        cr->x += ((float) cr->xs * cos(cr->t * M_PI/180 - M_PI / 2));
        cr->x += ((float) cr->xs * sin(cr->t * M_PI/180 - M_PI / 2));

        cr->t += cr->t_inc;
        cr->degrees_drawn += abs(cr->t_inc);
    }

    /* bounds check */
    /* modification of random(-0.33,0.33) */
    cx = (int) (cr->x + (frand(0.66) - 0.33));
    cy = (int) (cr->y + (frand(0.66) - 0.33));


    if ((cx >= 0) && (cx < f->width) && (cy >= 0) && (cy < f->height)) {
        /* draw sand painter if we're not wireframe */
        if (!f->wireframe)
            region_color(dpy, window, fgc, f, cr);

        /* draw fgcolor crack */
        ref_pixel(f, cx, cy) = f->fgcolor;
        XDrawPoint(dpy, window, fgc, cx, cy);

        if ( cr->curved && (cr->degrees_drawn > 360) ) {
            /* completed the circle, stop cracking */
            start_crack(f, cr); /* restart ourselves */
            make_crack(f); /* generate a new crack */
        }
        /* safe to check */
        else if ((f->cgrid[cy * f->width + cx] > 10000) ||
                 (abs(f->cgrid[cy * f->width + cx] - cr->t) < 5)) {
            /* continue cracking */
            f->cgrid[cy * f->width + cx] = (int) cr->t;
        } else if (abs(f->cgrid[cy * f->width + cx] - cr->t) > 2) {
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

char *progclass = "Substrate";

char *defaults[] = {
    ".background: white",
    ".foreground: black",
    "*wireFrame: false",
    "*maxCycles: 10000",
    "*growthDelay: 18000",
    "*initialCracks: 3",
    "*maxCracks: 100",
    "*sandGrains: 64",
    "*circlePercent: 0",
    0
};

XrmOptionDescRec options[] = {
    {"-background", ".background", XrmoptionSepArg, 0},
    {"-foreground", ".foreground", XrmoptionSepArg, 0},
    {"-wireframe", ".wireFrame", XrmoptionNoArg, "true"},
    {"-max-cycles", ".maxCycles", XrmoptionSepArg, 0},
    {"-growth-delay", ".growthDelay", XrmoptionSepArg, 0},
    {"-initial-cracks", ".initialCracks", XrmoptionSepArg, 0},
    {"-max-cracks", ".maxCracks", XrmoptionSepArg, 0},
    {"-sand-grains", ".sandGrains", XrmoptionSepArg, 0},
    {"-circle-percent", ".circlePercent", XrmoptionSepArg, 0},
    {0, 0, 0, 0}
};

void build_img(Display *dpy, Window window, XWindowAttributes xgwa, GC fgc, 
               struct field *f) {
    if (f->off_img) {
        free(f->off_img);
        f->off_img = NULL;
    }

    f->off_img = (unsigned long *) xrealloc(f->off_img, sizeof(unsigned long) * 
                                            f->width * f->height);

    memset(f->off_img, f->bgcolor, sizeof(unsigned long) * f->width * f->height);
}

void screenhack(Display * dpy, Window window)
{
    struct field *f = init_field();

    unsigned int max_cycles = 0;
    int growth_delay = 0;
    int tempx;

    GC fgc;
    XGCValues gcv;
    XWindowAttributes xgwa;
    XColor tmpcolor;

    growth_delay = (get_integer_resource("growthDelay", "Integer"));
    max_cycles = (get_integer_resource("maxCycles", "Integer"));
    f->initial_cracks = (get_integer_resource("initialCracks", "Integer"));
    f->max_num = (get_integer_resource("maxCracks", "Integer"));
    f->wireframe = (get_boolean_resource("wireFrame", "Boolean"));
    f->grains = (get_integer_resource("sandGrains", "Integer"));
    f->circle_percent = (get_integer_resource("circlePercent", "Integer"));

    if (f->initial_cracks <= 2) {
        fprintf(stderr, "%s: Initial cracks must be greater than 2\n", progname);
        return;
    }

    if (f->max_num <= 10) {
        fprintf(stderr, "%s: Maximum number of cracks must be less than 10\n", 
                progname);
        return;
    }

    if (f->circle_percent < 0) {
        fprintf(stderr, "%s: circle percent must be at least 0\n", progname);
        return;
    }

    if (f->circle_percent > 100) {
        fprintf(stderr, "%s: circle percent must be less than 100\n", progname);
        return;
    }
    
    XGetWindowAttributes(dpy, window, &xgwa);

    f->height = xgwa.height;
    f->width = xgwa.width;
    f->visdepth = xgwa.depth;
 
    /* Count the colors in our map and assign them in a horrifically inefficient 
     * manner but it only happens once */
    while (rgb_colormap[f->numcolors] != NULL) {
        f->parsedcolors = (unsigned long *) xrealloc(f->parsedcolors, 
                                                     sizeof(unsigned long) * 
                                                     (f->numcolors + 1));
        if (!XParseColor(dpy, xgwa.colormap, rgb_colormap[f->numcolors], &tmpcolor)) {
            fprintf(stderr, "%s: couldn't parse color %s\n", progname,
                    rgb_colormap[f->numcolors]);
            exit(1);
        }

        if (!XAllocColor(dpy, xgwa.colormap, &tmpcolor)) {
            fprintf(stderr, "%s: couldn't allocate color %s\n", progname,
                    rgb_colormap[f->numcolors]);
            exit(1);
        }

        f->parsedcolors[f->numcolors] = tmpcolor.pixel;

        f->numcolors++;
    }

    gcv.foreground = get_pixel_resource("foreground", "Foreground",
					dpy, xgwa.colormap);
    gcv.background = get_pixel_resource("background", "Background",
					dpy, xgwa.colormap);
    fgc = XCreateGC(dpy, window, GCForeground, &gcv);

    f->fgcolor = gcv.foreground;
    f->bgcolor = gcv.background;

    /* Initialize stuff */
    build_img(dpy, window, xgwa, fgc, f);
    build_substrate(f);
    
    while (1) {
        if ((f->cycles % 10) == 0) {
            /* Restart if the window size changes */
            XGetWindowAttributes(dpy, window, &xgwa);

            if (f->height != xgwa.height || f->width != xgwa.width) {
                f->height = xgwa.height;
                f->width = xgwa.width;
                f->visdepth = xgwa.depth;

                build_substrate(f);
                build_img(dpy, window, xgwa, fgc, f);
                XSetForeground(dpy, fgc, gcv.background);
                XFillRectangle(dpy, window, fgc, 0, 0, xgwa.width, xgwa.height);
                XSetForeground(dpy, fgc, gcv.foreground);
            }
        }

        for (tempx = 0; tempx < f->num; tempx++) {
            movedrawcrack(dpy, window, fgc, f, tempx);
        }

        f->cycles++;

        XSync(dpy, False);

        screenhack_handle_events(dpy);

        if (f->cycles >= max_cycles && max_cycles != 0) {
            build_substrate(f);
            build_img(dpy, window, xgwa, fgc, f);
            XSetForeground(dpy, fgc, gcv.background);
            XFillRectangle(dpy, window, fgc, 0, 0, xgwa.width, xgwa.height);
            XSetForeground(dpy, fgc, gcv.foreground);
        }

        if (growth_delay)
            usleep(growth_delay);
    }
}

