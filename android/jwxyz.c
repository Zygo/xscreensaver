/* xscreensaver, Copyright (c) 1991-2015 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* JWXYZ Is Not Xlib.

   But it's a bunch of function definitions that bear some resemblance to
   Xlib and that do things that bear some resemblance to the
   things that Xlib might have done.
 */

#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>
#include <android/log.h>

#include "jwxyz.h"
#include "jwxyz-timers.h"
#include "yarandom.h"
#include "screenhackI.h"

typedef signed char BOOL;
#define YES             (BOOL)1
#define NO              (BOOL)0

struct CGPoint {
    float x;
    float y;
};
typedef struct CGPoint CGPoint;

struct CGSize {
    float width;
    float height;
};
typedef struct CGSize CGSize;

struct CGRect {
    CGPoint origin;
    CGSize size;
};
typedef struct CGRect CGRect;

struct jwxyz_Drawable {
    enum { WINDOW, PIXMAP } type;
    CGRect frame;
    union {
	struct {
	    unsigned long background;
	    int last_mouse_x, last_mouse_y;
	} window;
	struct {
	    int depth;
	    void *cgc_buffer;	
	} pixmap;
    };
};

struct jwxyz_Display {
    Window main_window;
    Screen *screen;
    int screen_count;
    struct jwxyz_sources_data *timers_data;
};

struct jwxyz_Screen {
    Display *dpy;
    Visual *visual;
    unsigned long black, white;
    int screen_number;
};


Screen *
XDefaultScreenOfDisplay (Display *dpy)
{
  return dpy->screen;
}

unsigned long
XBlackPixelOfScreen(Screen *screen)
{
  return screen->black;
}

unsigned long
XWhitePixelOfScreen(Screen *screen)
{
  return screen->white;
}


static void draw_rect(Display *, Drawable, GC,
		      int x, int y, unsigned int width,
		      unsigned int height, BOOL foreground_p, BOOL fill_p);

Status
XParseColor(Display * dpy, Colormap cmap, const char *spec, XColor * ret)
{
    unsigned char r = 0, g = 0, b = 0;
    if (*spec == '#' && strlen(spec) == 7) {
	static unsigned const char hex[] = {	// yeah yeah, shoot me.
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4,
		5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
	    0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	r = (hex[spec[1]] << 4) | hex[spec[2]];
	g = (hex[spec[3]] << 4) | hex[spec[4]];
	b = (hex[spec[5]] << 4) | hex[spec[6]];
    } else if (!strcasecmp(spec, "black")) {
//  r = g = b = 0;
    } else if (!strcasecmp(spec, "white")) {
	r = g = b = 255;
    } else if (!strcasecmp(spec, "red")) {
	r = 255;
    } else if (!strcasecmp(spec, "green")) {
	g = 255;
    } else if (!strcasecmp(spec, "blue")) {
	b = 255;
    } else if (!strcasecmp(spec, "cyan")) {
	g = b = 255;
    } else if (!strcasecmp(spec, "magenta")) {
	r = b = 255;
    } else if (!strcasecmp(spec, "yellow")) {
	r = g = 255;
    } else {
	return 0;
    }

    ret->red = (r << 8) | r;
    ret->green = (g << 8) | g;
    ret->blue = (b << 8) | b;
    ret->flags = DoRed | DoGreen | DoBlue;
    return 1;
}

Status XAllocColor(Display * dpy, Colormap cmap, XColor * color)
{
    // store 32 bit ARGB in the pixel field.
    // (The uint32_t is so that 0xFF000000 doesn't become 0xFFFFFFFFFF000000)
    color->pixel = (uint32_t)
	((0xFF << 24) |
	 (((color->red >> 8) & 0xFF) << 16) |
	 (((color->green >> 8) & 0xFF) << 8) |
	 (((color->blue >> 8) & 0xFF)));
    return 1;
}

//  needs to be implemented in Android...
int
XFillRectangle(Display * dpy, Drawable d, GC gc, int x, int y,
	       unsigned int width, unsigned int height)
{
    return 0;
}

//  needs to be implemented in Android...
int
XDrawString(Display * dpy, Drawable d, GC gc, int x, int y,
	    const char *str, int len)
{
    return 0;			// try this for now...
}


//  needs to be implemented in Android...
int XFreeGC(Display * dpy, GC gc)
{
    return 0;
}



int XFreeFont(Display * dpy, XFontStruct * f)
{
    return 0;
}

int XFreeFontInfo(char **names, XFontStruct * info, int n)
{
    int i;
    if (names) {
	for (i = 0; i < n; i++)
	    if (names[i])
		free(names[i]);
	free(names);
    }
    if (info) {
	for (i = 0; i < n; i++)
	    if (info[i].per_char)
		free(info[i].per_char);
	free(info);
    }
    return 0;
}


//  needs to be implemented in Android...
int XUnloadFont(Display * dpy, Font fid)
{
    return 0;
}


//  needs to be implemented in Android...
GC
XCreateGC(Display * dpy, Drawable d, unsigned long mask, XGCValues * xgcv)
{
}


//  needs to be implemented in Android...
XFontStruct *XLoadQueryFont(Display * dpy, const char *name)
{
}


Status
XGetWindowAttributes(Display * dpy, Window w, XWindowAttributes * xgwa)
{

//  Assert (w && w->type == WINDOW, "not a window");

    memset(xgwa, 0, sizeof(*xgwa));
    xgwa->x = w->frame.origin.x;
    xgwa->y = w->frame.origin.y;
    xgwa->width = w->frame.size.width;
    xgwa->height = w->frame.size.height;
    xgwa->depth = 32;
    xgwa->screen = dpy->screen;
    xgwa->visual = dpy->screen->visual;

    return 0;
}

//  needs to be implemented in Android...
int XSetFont(Display * dpy, GC gc, Font fid)
{
    return 0;
}


//  needs to be implemented in Android...
int XClearWindow(Display * dpy, Window win)
{
}

// declared in utils/visual.h
int has_writable_cells(Screen * s, Visual * v)
{
    return 0;
}

Status
XAllocColorCells(Display * dpy, Colormap cmap, Bool contig,
		 unsigned long *pmret, unsigned int npl,
		 unsigned long *pxret, unsigned int npx)
{
    return 0;
}

int XStoreColors(Display * dpy, Colormap cmap, XColor * colors, int n)
{
    //Assert(0, "XStoreColors called");
    return 0;
}

int
XFreeColors(Display * dpy, Colormap cmap, unsigned long *px, int npixels,
	    unsigned long planes)
{
    return 0;
}

int XFlush(Display * dpy)
{
    return 0;
}

Display *XDisplayOfScreen(Screen * s)
{
    return s->dpy;
}

//  needs to be implemented in Android...
int
XLookupString(XKeyEvent * e, char *buf, int size, KeySym * k_ret,
	      XComposeStatus * xc)
{
    return 0;
}

int XScreenNumberOfScreen(Screen * s)
{
    return s->screen_number;
}

int jwxyz_ScreenCount(Display * dpy)
{
    return dpy->screen_count;
}


/*
// should this be defined?
static Display *jwxyz_live_displays[20] = { 0, };
*/

Display * jwxyz_make_display (void *nsview_arg, void *cgc_arg)
{
    Display *d = (Display *) calloc(1, sizeof(*d));
    d->screen = (Screen *) calloc(1, sizeof(Screen));
    d->screen->dpy = d;

    d->screen_count = 1;
    d->screen->screen_number = 0;
    d->screen->black = 0xFF000000;
    d->screen->white = 0xFFFFFFFF;

    Visual *v = (Visual *) calloc(1, sizeof(Visual));
    v->class = TrueColor;
    v->red_mask = 0x00FF0000;
    v->green_mask = 0x0000FF00;
    v->blue_mask = 0x000000FF;
    v->bits_per_rgb = 8;
    d->screen->visual = v;

    Window w = (Window) calloc(1, sizeof(*w));
    w->type = WINDOW;
    w->window.background = BlackPixelOfScreen(d->screen);

    d->main_window = w;

    return d;
}

void
jwxyz_free_display (Display *dpy)
{
  free (dpy->screen->visual);
  free (dpy->screen);
  free (dpy->main_window);
  free (dpy);
}


/* Call this when the Renderer calls onSurfaceChanged
 */
void
jwxyz_window_resized (Display *dpy, Window w, 
                      int new_x, int new_y, int new_width, int new_height,
                      void *cgc_arg)
{
    w->frame.origin.x = new_x;
    w->frame.origin.y = new_y;
    w->frame.size.width = new_width;
    w->frame.size.height = new_height;
}

Window XRootWindow(Display * dpy, int screen)
{
    return dpy->main_window;
}

/* Handle an abort on Android
   TODO: Test that Android handles aborts properly
 */
void
jwxyz_abort (const char *fmt, ...)
{
  char s[10240];
  if (!fmt || !*fmt)
    strcpy (s, "abort");
  else
    {
      va_list args;
      va_start (args, fmt);
      vsprintf (s, fmt, args);
      va_end (args);
    }
  /* Send error to Android device log */
  __android_log_write(ANDROID_LOG_ERROR, "xscreensaver", s);

  abort();  
}

Pixmap
XCreatePixmap (Display *dpy, Drawable d,
               unsigned int width, unsigned int height, unsigned int depth)
{
}

int
XDestroyImage (XImage *ximage)
{
  if (ximage->data) free (ximage->data);
  free (ximage);
  return 0;
}

int
XDrawString16 (Display *dpy, Drawable d, GC gc, int x, int y,
             const XChar2b *str, int len)
{
}

int
XFreePixmap (Display *d, Pixmap p)
{
}

XImage *
XGetImage (Display *dpy, Drawable d, int x, int y,
           unsigned int width, unsigned int height,
           unsigned long plane_mask, int format)
{
}

unsigned long
XGetPixel (XImage *ximage, int x, int y)
{
}

int
XSetForeground (Display *dpy, GC gc, unsigned long fg)
{
}

int
XTextExtents16 (XFontStruct *f, const XChar2b *s, int length,
                int *dir_ret, int *ascent_ret, int *descent_ret,
                XCharStruct *cs)
{
}

int
XPutPixel (XImage *ximage, int x, int y, unsigned long pixel)
{
}

XImage *
XCreateImage (Display *dpy, Visual *visual, unsigned int depth,
              int format, int offset, char *data,
              unsigned int width, unsigned int height,
              int bitmap_pad, int bytes_per_line)
{
}
