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
    unsigned long int *off_alpha;
};

static void *xrealloc(void *p, size_t size) {
    void *ret;
    if ((ret = realloc(p, size)) == NULL) {
	fprintf(stderr, "%s: out of memory\n", progname);
	exit(1);
    }
    return ret;
}

struct field *init_field(void) {
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

inline void make_disc(struct field *f, float x, float y, float vx, float vy, float r) {
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

inline void point2rgb(int depth, unsigned long c, unsigned short int *r, 
                      unsigned short int *g, unsigned short int *b) {
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

inline unsigned long rgb2point(int depth, unsigned short int r, 
                               unsigned short int g, unsigned short int b) {
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

/* alpha blended point drawing */
inline unsigned long trans_point(int x1, int y1, unsigned long myc, float a, struct field *f) {
    if ((x1 >= 0) && (x1 < f->width) && (y1 >= 0) && (y1 < f->height)) {
        if (a >= 1.0) {
            ref_pixel(f, x1, y1) = myc;
        } else {
            unsigned short int or, og, ob;
            unsigned short int r, g, b;
            unsigned short int nr, ng, nb;
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

inline void move_disc(struct field *f, int dnum) {
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

inline void draw_glowpoint(Display *dpy, Window window, GC fgc, struct field *f, float px, float py) {
    int i, j;
    float a;
    unsigned long c;

    for (i =- 2; i < 3; i++) {
        for (j =- 2; j < 3; j++) {
            a = 0.8 - i * i * 0.1 - j * j * 0.1;

            c = trans_point(px+i, py+j, f->fgcolor, a, f);
            XSetForeground(dpy, fgc, c);
            XDrawPoint(dpy, window, fgc, px + i, py + j);
            XSetForeground(dpy, fgc, f->fgcolor);
        }
    }
}

inline void moverender_rider(Display *dpy, Window window, GC fgc, struct field *f, PxRider *rid, 
                             float x, float y, float r) {
    float px, py;
    unsigned long int c;
    unsigned short int cr, cg, cb;
    int ch;
    double cs, cv;

    /* add velocity to theta */
    rid->t = fmod((rid->t + rid->vt + M_PI), (2 * M_PI)) - M_PI;
    
    rid->vt += frand(0.002) - 0.001;

    /* apply friction brakes */
    if (abs(rid->vt) > 0.02)
        rid->vt *= 0.9;

    /* draw */
    px = x + r * cos(rid->t);
    py = y + r * sin(rid->t);

    if ((px < 0) || (px >= f->width) || (py < 0) || (py >= f->height))
        return;

    /* max brightness seems to be 0.003845 */

    c = ref_pixel(f, (int) px, (int) py);
    point2rgb(f->visdepth, c, &cr, &cg, &cb);
    rgb_to_hsv(cr, cg, cb, &ch, &cs, &cv);

    /* guestimated - 40 is 18% of 255, so scale this to 0.0 to 0.003845 */
    if (cv > 0.0006921) {
        draw_glowpoint(dpy, window, fgc, f, px, py); 

        rid->mycharge = 0.003845;
    } else {
        rid->mycharge *= 0.98;

        hsv_to_rgb(ch, cs, rid->mycharge, &cr, &cg, &cb);
        c = rgb2point(f->visdepth, cr, cg, cb);

        trans_point(px, py, c, 0.5, f);

        XSetForeground(dpy, fgc, c);
        XDrawPoint(dpy, window, fgc, px, py);
        XSetForeground(dpy, fgc, f->fgcolor);
    }
}

inline void render_disc(Display *dpy, Window window, GC fgc, struct field *f, int dnum) {
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
            if (d > abs(f->discs[n].r - di->r)) {
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
                c = trans_point(p3ax, p3ay, f->fgcolor, 0.75, f);
                XSetForeground(dpy, fgc, c);
                XDrawPoint(dpy, window, fgc, p3ax, p3ay);

                /* XPutPixel(f->off_map, p3bx, p3by, f->fgcolor); */
                c = trans_point(p3bx, p3by, f->fgcolor, 0.75, f);
                XSetForeground(dpy, fgc, c);
                XDrawPoint(dpy, window, fgc, p3bx, p3by);
                XSetForeground(dpy, fgc, f->fgcolor);
            }
        }

    }

    /* Render all the pixel riders */
    for (m = 0; m < di->numr; m++) {
        moverender_rider(dpy, window, fgc, f, &(di->pxRiders[m]), 
                         di->x, di->y, di->r);
    }
}

char *progclass = "InterMomentary";

char *defaults[] = {
    ".background: black",
    ".foreground: white",
    "*drawDelay: 30000",
    "*numDiscs: 85",
    "*maxRiders: 40",
    "*maxRadius: 100",
    0
};

XrmOptionDescRec options[] = {
    {"-background", ".background", XrmoptionSepArg, 0},
    {"-foreground", ".foreground", XrmoptionSepArg, 0},
    {"-draw-delay", ".drawDelay", XrmoptionSepArg, 0},
    {"-num-discs", ".numDiscs", XrmoptionSepArg, 0},
    {"-max-riders", ".maxRiders", XrmoptionSepArg, 0},
    {"-max-radius", ".maxRadius", XrmoptionSepArg, 0},
    {0, 0, 0, 0}
};

void build_img(Display *dpy, Window window, struct field *f) {
    if (f->off_alpha) {
        free(f->off_alpha);
        f->off_alpha = NULL;

        /* Assume theres also an off pixmap */
        XFreePixmap(dpy, f->off_map);
    }

    f->off_alpha = (unsigned long *) xrealloc(f->off_alpha, sizeof(unsigned long) * 
                                              f->width * f->height);

    memset(f->off_alpha, f->bgcolor, sizeof(unsigned long) * f->width * f->height);

    f->off_map = XCreatePixmap(dpy, window, f->width, f->height, f->visdepth);

}

inline void blank_img(Display *dpy, Window window, XWindowAttributes xgwa, GC fgc, struct field *f) {
    memset(f->off_alpha, f->bgcolor, sizeof(unsigned long) * f->width * f->height);

    XSetForeground(dpy, fgc, f->bgcolor);
    XFillRectangle(dpy, window, fgc, 0, 0, xgwa.width, xgwa.height);
    XSetForeground(dpy, fgc, f->fgcolor);
}

void screenhack(Display * dpy, Window window)
{
    struct field *f = init_field();

#ifdef TIME_ME
    time_t start_time = time(NULL);
#endif

    int draw_delay = 0;
    int tempx;

    GC fgc, copygc;
    XGCValues gcv;
    XWindowAttributes xgwa;

    draw_delay = (get_integer_resource("drawDelay", "Integer"));
    f->maxrider = (get_integer_resource("maxRiders", "Integer"));
    f->maxradius = (get_integer_resource("maxRadius", "Integer"));
    f->initial_discs = (get_integer_resource("numDiscs", "Integer"));

    if (f->initial_discs <= 10) {
        fprintf(stderr, "%s: Initial discs must be greater than 10\n", progname);
        return;
    }

    if (f->maxradius <= 30) {
        fprintf(stderr, "%s: Max radius must be greater than 30\n", progname);
        return;
    }

    if (f->maxrider <= 10) {
        fprintf(stderr, "%s: Max riders must be greater than 10\n", progname);
        return;
    }
    
    XGetWindowAttributes(dpy, window, &xgwa);

    f->height = xgwa.height;
    f->width = xgwa.width;
    f->visdepth = xgwa.depth;
 
    gcv.foreground = get_pixel_resource("foreground", "Foreground",
					dpy, xgwa.colormap);
    gcv.background = get_pixel_resource("background", "Background",
					dpy, xgwa.colormap);
    fgc = XCreateGC(dpy, window, GCForeground, &gcv);
    copygc = XCreateGC(dpy, window, GCForeground, &gcv);

    f->fgcolor = gcv.foreground;
    f->bgcolor = gcv.background;

    /* Initialize stuff */
    build_img(dpy, window, f);

    for (tempx = 0; tempx < f->initial_discs; tempx++) {
        float fx, fy, x, y, r;
        int bt;

        /* Arrange in anti-collapsing circle */
        fx = 0.4 * f->width * cos((2 * M_PI) * tempx / f->initial_discs);
        fy = 0.4 * f->height * sin((2 * M_PI) * tempx / f->initial_discs);
        x = frand(f->width / 2) + fx;
        y = frand(f->height / 2) + fy;
        r = 5 + frand(f->maxradius);
        bt = 1;

        if ((random() % 100) < 50)
            bt = -1;

        make_disc(f, x, y, bt * fx / 1000.0, bt * fy / 1000.0, r);
        
    }
    
    while (1) {
        if ((f->cycles % 10) == 0) {
            /* Restart if the window size changes */
            XGetWindowAttributes(dpy, window, &xgwa);

            if (f->height != xgwa.height || f->width != xgwa.width) {
                f->height = xgwa.height;
                f->width = xgwa.width;
                f->visdepth = xgwa.depth;

                build_img(dpy, window, f);
            }
        }

        blank_img(dpy, f->off_map, xgwa, fgc, f);
        for (tempx = 0; tempx < f->num; tempx++) {
            move_disc(f, tempx);
            render_disc(dpy, f->off_map, fgc, f, tempx);
        }

        XSetFillStyle(dpy, copygc, FillTiled);
        XSetTile(dpy, copygc, f->off_map);
        XFillRectangle(dpy, window, copygc, 0, 0, f->width, f->height);

        f->cycles++;

/*        XSync(dpy, False); */

        screenhack_handle_events(dpy);

        if (draw_delay)
            usleep(draw_delay);
    }
}
