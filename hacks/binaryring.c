/*
 * Binary Ring
 * Copyright (c) 2006-2014 Emilio Del Tessandoro <emilio.deltessa@gmail.com>
 *
 *  Directly ported code from complexification.net Binary Ring art
 *  http://www.complexification.net/gallery/machines/binaryRing/appletm/BinaryRing_m.pde
 *
 *  Binary Ring code:
 *  j.tarbell   June, 2004
 *  Albuquerque, New Mexico
 *  complexification.net
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

#include "screenhack.h"
#include "colors.h"
#include "hsv.h"

#define ANTIALIAS   1
#define BLACK       0
#define WHITE       1

#define swap(a, b)  do { __typeof__(a) tmp;  tmp = a; a = b; b = tmp; } while(0)
#define frand1()    ((((int) random() ) << 1) * 4.65661287524579692411E-10)
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

/* better if signed */
typedef unsigned long pixel_t;


typedef struct {
    float x, y;
    float xx, yy;
    float vx, vy;
    float color;
    int age;            /* age from 0 to ageMax */
} particle_t;


struct state {
    int epoch;
    int growth_delay;
    int ring_radius;
    int max_age;
    float curliness;
    int particles_number;
    particle_t* particles;


    Display *display;               /* variables for the X stuff */
    Window window;
    GC gc;
    XGCValues gcv;
    Visual* visual;
    int depth;

    int width;
    int height;

    Pixmap pix;		                /* for reshape */
    XImage* buf;		            /* buffer */
    pixel_t* buffer;
    pixel_t colors[2];

    Bool color;
};



static void point2rgb(int depth, pixel_t c, int *r, int *g, int *b) 
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

static pixel_t rgb2point(int depth, int r, int g, int b) 
{
    pixel_t ret = 0;

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

static
void draw_point ( struct state* st,
                 int x, int y, pixel_t myc, float a ) {

    int or = 0, og = 0, ob = 0;
    int r = 0, g = 0, b = 0;
    int nr, ng, nb;
    register pixel_t c;

    /*or = st->buffer[ 3 * ( y * st->width + x ) + 0 ];
    og = st->buffer[ 3 * ( y * st->width + x ) + 1 ];
    ob = st->buffer[ 3 * ( y * st->width + x ) + 2 ];*/
    
    c = st->buffer[ y * st->width + x ];
    point2rgb( st->depth, c, &or, &og, &ob );
    point2rgb( st->depth, myc, &r, &g, &b );

    nr = or + (r - or) * a;
    ng = og + (g - og) * a;
    nb = ob + (b - ob) * a;

    /*st->buffer[ 3 * ( y * st->width + x ) + 0 ] = nr;
    st->buffer[ 3 * ( y * st->width + x ) + 1 ] = ng;
    st->buffer[ 3 * ( y * st->width + x ) + 2 ] = nb;*/
    c = rgb2point(st->depth, nr, ng, nb);
    st->buffer[ y * st->width + x ] = c;

    XPutPixel( st->buf, x, y, c );
}




#if ANTIALIAS
#define plot_(X,Y,D) do{    \
    _dla_plot(st, (X), (Y), color, (D) * alpha); }while(0)

static
void _dla_plot(struct state* st, int x, int y, pixel_t col, float br)
{
    if ( ( x >= 0 && x < st->width ) && ( y >= 0 && y < st->height ) ) {
        if ( br > 1.0f ) br = 1.0f;
        draw_point( st, x, y, col, br );
    }
}

#define ipart_(X) ((int)(X))
#define round_(X) ((int)(((float)(X))+0.5))
#define fpart_(X) (((float)(X))-(float)ipart_(X))
#define rfpart_(X) (1.0-fpart_(X))

static
void draw_line_antialias(
        struct state* st,
        int x1, int y1,
        int x2, int y2,
        pixel_t color, float alpha )
{
    float dx = (float)x2 - (float)x1;
    float dy = (float)y2 - (float)y1;
    float gradient;
    float xend;
    float yend;
    float xgap;
    float ygap;
    int xpxl1;
    int ypxl1;
    float intery;
    float interx;
    int xpxl2;
    int ypxl2;
    int x;
    int y;

    /* hard clipping, because this routine has some problems with negative coordinates */
    /* TODO: fix the alpha for that boundary cases. Using fabs() could solve? */
    if ( ( x1 < 0 || x1 > st->width ) ||
         ( x2 < 0 || x2 > st->width ) ||
         ( y1 < 0 || y1 > st->height ) ||
         ( y2 < 0 || y2 > st->height ) )
        return;

    if ( fabs(dx) > fabs(dy) ) {
        if ( x2 < x1 ) {
            swap(x1, x2);
            swap(y1, y2);
        }
        gradient = dy / dx;
        xend = round_(x1);
        yend = y1 + gradient*(xend - x1);
        xgap = rfpart_(x1 + 0.5);
        xpxl1 = xend;
        ypxl1 = ipart_(yend);
        plot_(xpxl1, ypxl1, rfpart_(yend)*xgap);
        plot_(xpxl1, ypxl1+1, fpart_(yend)*xgap);
        intery = yend + gradient;

        xend = round_(x2);
        yend = y2 + gradient*(xend - x2);
        xgap = fpart_(x2+0.5);
        xpxl2 = xend;
        ypxl2 = ipart_(yend);
        plot_(xpxl2, ypxl2, rfpart_(yend) * xgap);
        plot_(xpxl2, ypxl2 + 1, fpart_(yend) * xgap);

        /*if ( rfpart_(yend) * xgap < 0 || fpart_(yend) * xgap < 0)
        printf("%f %f\n", rfpart_(yend) * xgap, fpart_(yend) * xgap);*/
        for(x=xpxl1+1; x <= (xpxl2-1); x++) {
            plot_(x, ipart_(intery), rfpart_(intery));
            plot_(x, ipart_(intery) + 1, fpart_(intery));
            /*if ( rfpart_(intery) < 0 || fpart_(intery) < 0)
            printf("%f %f\n", rfpart_(intery), fpart_(intery));*/
            intery += gradient;
        }
    } else {
        if ( y2 < y1 ) {
            swap(x1, x2);
            swap(y1, y2);
        }
        gradient = dx / dy;
        yend = round_(y1);
        xend = x1 + gradient*(yend - y1);
        ygap = rfpart_(y1 + 0.5);
        ypxl1 = yend;
        xpxl1 = ipart_(xend);
        plot_(xpxl1, ypxl1, rfpart_(xend)*ygap);
        plot_(xpxl1, ypxl1+1, fpart_(xend)*ygap);
        interx = xend + gradient;

        yend = round_(y2);
        xend = x2 + gradient*(yend - y2);
        ygap = fpart_(y2+0.5);
        ypxl2 = yend;
        xpxl2 = ipart_(xend);
        plot_(xpxl2, ypxl2, rfpart_(xend) * ygap);
        plot_(xpxl2, ypxl2 + 1, fpart_(xend) * ygap);

        for(y=ypxl1+1; y <= (ypxl2-1); y++) {
            plot_(ipart_(interx), y, rfpart_(interx));
            plot_(ipart_(interx) + 1, y, fpart_(interx));
            interx += gradient;
        }
    }
}
#undef plot_
#undef ipart_
#undef fpart_
#undef round_
#undef rfpart_

#else

static
void draw_line ( struct state* st, int x0, int y0, int x1, int y1, int color, float a ) {
    register int steep;
    register int deltax;
    register int deltay;
    register int error;
    register int ystep;
    register int y;
    register int x;


    steep = abs ( y1 - y0 ) > abs ( x1 - x0 );
    if ( steep ) { swap(x0,y0); swap(x1,y1); }
    if ( x0 > x1 ) { swap(x0,x1); swap(y0,y1); }
    deltax = x1 - x0;
    deltay = abs(y1-y0);
    error = 0;
    y = y0;
    ystep = y0 < y1 ? 1 : -1;
    if ( a > 1.0f ) a = 1.0f;

    for ( x = x0; x < x1; x++ ) {
        if ( steep ) {
            if ( ( y >= 0 && y < st->width ) && ( x >= 0 && x < st->height ) )
                draw_point( st, y, x, color, a );
        } else {
            if ( ( x >= 0 && x < st->width ) && ( y >= 0 && y < st->height ) )
                draw_point( st, x, y, color, a );
        }
        error += deltay;
        if ( ( error << 1 ) > deltax ) {
            y += ystep;
            error -= deltax;
        }
    }
}
#endif

static void create_buffers ( struct state* st, Display* display, Screen* screen, Window window, GC gc ) {
    
    XWindowAttributes xgwa;
    XGetWindowAttributes( display, window, &xgwa );

    /* Initialize the pixmap */
    if ( st->buf != NULL ) XFreePixmap( display, st->pix );
    
    XSetForeground( display, gc, st->colors[BLACK] );
    st->pix = XCreatePixmap( display, window, st->width, st->height,
                             xgwa.depth );
    XFillRectangle( display, st->pix, gc, 0, 0, st->width, st->height);

    /* Initialize the bitmap */
    if ( st->buf != NULL ) XDestroyImage ( st->buf );
    st->buf = XGetImage( display, st->pix, 0, 0, st->width, st->height, visual_depth(screen, st->visual), ZPixmap );
    st->buffer = (pixel_t*) calloc(sizeof(pixel_t), st->width * st->height);
    /*int i;
    for ( i = 0; i < st->width * st->height; ++i ) st->buffer[i] = st->colors[BLACK];*/
}

static void init_particle ( particle_t* p, float dx, float dy, float direction, int color, int max_age ) {
    float max_initial_velocity = 2.0f;
    p->x = -dx;
    p->y = -dy;
    p->xx = 0;
    p->yy = 0;
    p->vx = max_initial_velocity * cos(direction);
    p->vy = max_initial_velocity * sin(direction);

    p->age = random() % max_age;

    p->color = color;
}

static void clamp ( int* value, int l, int h ) {
    if ( *value < l ) *value = l;
    if ( *value > h ) *value = h;
}

static pixel_t next_color ( struct state* st, pixel_t current ) {
    int r=0, g=0, b=0;

    point2rgb(st->depth, current, &r, &g, &b);
    r += random() % 5 - 2;
    g += random() % 5 - 2;
    b += random() % 5 - 2;
    clamp(&r, 0, 255);
    clamp(&g, 0, 255);
    clamp(&b, 0, 255);

    return rgb2point( st->depth, r, g, b );
}


static void create_particles ( struct state* st ) {
    int i;
    float emitx, emity;
    float direction;

    for ( i = 0; i < st->particles_number; i++ ) {
        emitx = ( st->ring_radius * sinf( M_PI * 2 * ( (float) i / st->particles_number ) ) );
        emity = ( st->ring_radius * cosf( M_PI * 2 * ( (float) i / st->particles_number ) ) );
        direction = (M_PI * i) / st->particles_number;

        if ( st->epoch == WHITE && st->color )
            st->colors[WHITE] = next_color(st, st->colors[WHITE]);

        init_particle(st->particles + i, emitx, emity, direction, st->colors[WHITE], st->max_age);
    }
}


/* randomly move one particle and draw it */
static void move ( particle_t* p, struct state * st ) {
    int w = st->width / 2;
    int h = st->height / 2;
    float max_dv = 1.0f;

    p->xx = p->x;
    p->yy = p->y;
    p->x += p->vx;
    p->y += p->vy;
    p->vx += frand1() * st->curliness * max_dv;
    p->vy += frand1() * st->curliness * max_dv;


#if ANTIALIAS
    draw_line_antialias( st,
        w+p->xx, h+p->yy, w+p->x, h+p->y, p->color, 0.15 );
    draw_line_antialias( st,
        w-p->xx, h+p->yy, w-p->x, h+p->y, p->color, 0.15 );
#else
    draw_line( st, w+p->xx, h+p->yy, w+p->x, h+p->y, p->color, 0.15 );
    draw_line( st, w-p->xx, h+p->yy, w-p->x, h+p->y, p->color, 0.15 );
#endif

    p->age++;
    /* if this is too old, die and reborn */
    if ( p->age > st->max_age ) {
        float dir = frand1() * 2 * M_PI;
        p->x = st->ring_radius * sin(dir);
        p->y = st->ring_radius * cos(dir);
        p->xx = p->yy = p->vx = p->vy = 0;
        p->age = 0;

        if ( st->epoch == WHITE && st->color )
            st->colors[WHITE] = next_color(st, st->colors[WHITE] );
        
        p->color = st->colors[st->epoch];
    }
}




/* Initialize Everything */
static void* binaryring_init ( Display* display, Window window ) {
    XWindowAttributes xgwa;

    struct state *st = ( struct state* ) calloc ( 1, sizeof( *st ) );

    XGetWindowAttributes( display, window, &xgwa );

    st->epoch = WHITE;
    st->display = display;
    st->window = window;
    st->particles_number = (get_integer_resource(st->display, "particles_number", "Integer"));
    st->growth_delay = (get_integer_resource(st->display, "growth_delay", "Integer"));
    st->ring_radius = (get_integer_resource(st->display, "ring_radius", "Integer"));
    st->max_age = (get_integer_resource(st->display, "max_age", "Integer"));
    st->color =  get_boolean_resource(st->display, "color", "Boolean");
    st->height = xgwa.height;
    st->width = xgwa.width;
    st->visual = xgwa.visual;
    st->curliness = 0.5;


    st->depth = visual_depth(xgwa.screen, st->visual);
    st->colors[0] = rgb2point(st->depth, 0, 0, 0);
    st->colors[1] = rgb2point(st->depth, 255, 255, 255);

    /*describe_visual (stdout, xgwa.screen, xgwa.visual, False);*/

    st->particles = malloc (st->particles_number * sizeof( particle_t ) );
    create_particles ( st );

    st->gc = XCreateGC( st->display, st->window, 0, &st->gcv );
    create_buffers ( st, display, xgwa.screen, window, st->gc );


    return st;
}


static unsigned long binaryring_draw ( Display* display, Window win, void *closure ) {
    struct state *st = (struct state *) closure;
    int i;

    for ( i = 0; i < st->particles_number; i++ )
        move( &(st->particles[i]), st );

    /* draw the XImage in the Pixmap and the put the Pixmap on the screen */
    XPutImage( display, st->pix, st->gc, st->buf, 0, 0, 0, 0, st->width, st->height);
    XCopyArea( display, st->pix, win, st->gc, 0, 0, st->width, st->height, 0, 0 );

    /* randomly switch ageColor periods */
    if ( random() % 10000 > 9950 ) 
        st->epoch = (st->epoch == WHITE) ? BLACK : WHITE;

    return st->growth_delay;
}



static void binaryring_reshape ( Display* display, Window win, void *closure, unsigned int w, unsigned int h ) {
    struct state *st = (struct state *) closure;

    XWindowAttributes tmp;
    XGetWindowAttributes(display, win, &tmp);

    if ( tmp.height != st->height || tmp.width != st->width ) {
        st->height = tmp.height;
        st->width = tmp.width;


        st->epoch = WHITE;
        create_particles ( st );

        create_buffers ( st, display, tmp.screen, win, st->gc );
    }
}


/* if someone press a key switch the color */
static Bool binaryring_event ( Display* display, Window win, void *closure, XEvent* event ) {
    struct state *st = (struct state *) closure;

    if ( (*event).xany.type == KeyPress ) {
        st->epoch = (st->epoch == WHITE) ? BLACK : WHITE;
    }

    return False;
}


/* delete everything */
static void binaryring_free ( Display* display, Window win, void *closure ) {
    struct state *st = (struct state *) closure;
    XWindowAttributes tmp;
    XGetWindowAttributes(display, win, &tmp);

    if (st->gc) XFreeGC (display, st->gc);
    if (st->pix) XFreePixmap (display, st->pix);
    if (st->buf) XDestroyImage (st->buf);
    free (st->buffer);
    free (st->particles);
    free (st);
}


/* Default resources of the program */
static const char* binaryring_defaults [] = {
    ".background:	black",
    ".foreground:	white",
    "*growth_delay:     10000",
    "*particles_number: 5000",
    "*ring_radius:      40",
    "*max_age:          400",
    "*color:            True",
    "*ignoreRotation:   True",
    0
};

static XrmOptionDescRec binaryring_options [] = {
    { "-particles-number", ".particles_number", XrmoptionSepArg, 0 },
    { "-growth-delay",     ".growth_delay",     XrmoptionSepArg, 0 },
    { "-ring-radius",      ".ring_radius",      XrmoptionSepArg, 0 },
    { "-max-age",          ".max_age",          XrmoptionSepArg, 0 },
    { "-color",            ".color",            XrmoptionNoArg,  "True"  },
    { "-no-color",         ".color",            XrmoptionNoArg,  "False" },
    { 0, 0, 0, 0}
};

XSCREENSAVER_MODULE("BinaryRing", binaryring)
