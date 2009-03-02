/* rotzoomer - creates a collage of rotated and scaled portions of the screen
 * Copyright (C) 2001 Claudio Matsuoka <claudio@helllabs.org>
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

#ifdef HAVE_XSHM_EXTENSION
#include "xshm.h"
static Bool use_shm;
static XShmSegmentInfo shm_info;
#endif

struct zoom_area {
	int w, h;		/* rectangle width and height */
	int inc1, inc2;		/* rotation and zoom angle increments */
	int dx, dy;		/* translation increments */
	int a1, a2;		/* rotation and zoom angular variables */
	int ox, oy;		/* origin in the background copy */
	int xx, yy;		/* left-upper corner position (* 256) */
	int x, y;		/* left-upper corner position */
	int ww, hh;		/* valid area to place left-upper corner */
	int n;			/* number of iteractions */
};

static Window window;
static Display *display;
static GC gc;
static Visual *visual;
static XImage *orig_map, *buffer_map;
static Colormap colormap;

static int width, height;
static struct zoom_area **zoom_box;
static int num_zoom = 2;
static int move = 1;
static int delay = 0;


static void rotzoom (struct zoom_area *za)
{
	int x, y, c, s, zoom, z;
	int x2 = za->x + za->w - 1, y2 = za->y + za->h - 1;
	int ox = 0, oy = 0;

	z = 8100 * sin (M_PI * za->a2 / 8192);
	zoom = 8192 + z;

	c = zoom * cos (M_PI * za->a1 / 8192);
	s = zoom * sin (M_PI * za->a1 / 8192);
	for (y = za->y; y <= y2; y++) {
		for (x = za->x; x <= x2; x++) {
			ox = (x * c + y * s) >> 13;
			oy = (-x * s + y * c) >> 13;

			while (ox < 0)
				ox += width;
			while (oy < 0)
				oy += height;
			while (ox >= width)
				ox -= width;
			while (oy >= height)
				oy -= height;

			XPutPixel (buffer_map, x, y, XGetPixel (orig_map, ox, oy));
		}
	}

	za->a1 += za->inc1;		/* Rotation angle */
	za->a1 &= 0x3fff;;

	za->a2 += za->inc2;		/* Zoom */
	za->a2 &= 0x3fff;

	za->ox = ox;			/* Save state for next iteration */
	za->oy = oy;
}


static void reset_zoom (struct zoom_area *za)
{
	za->w = 50 + random() % 300;
	za->h = 50 + random() % 300;

	if (za->w > width / 3)
		za->w = width / 3;

	if (za->h > height / 3)
		za->h = height / 3;

	za->ww = width - za->w;
	za->hh = height - za->h;

	za->x = (random() % za->ww);
	za->y = (random() % za->hh);
	za->xx = za->x * 256;
	za->yy = za->y * 256;

	za->a1 = 0;
	za->a2 = 0;
	za->dx = ((2 * (random() & 1)) - 1) * (100 + random() % 300);
	za->dy = ((2 * (random() & 1)) - 1) * (100 + random() % 300);
	za->inc1 = ((2 * (random() & 1)) - 1) * (random () % 30);
	za->inc2 = ((2 * (random() & 1)) - 1) * (random () % 30);
	za->n = 50 + random() % 1000;
}


static struct zoom_area *create_zoom (void)
{
	struct zoom_area *za;

	za = malloc (sizeof (struct zoom_area));
	reset_zoom (za);

	return za;
}


static void update_position (struct zoom_area *za)
{
	za->xx += za->dx;
	za->yy += za->dy;

	za->x = za->xx >> 8;
	za->y = za->yy >> 8;

	if (za->x < 0) {
		za->x = 0;
		za->dx = 100 + random() % 100;
	}
		
	if (za->y < 0) {
		za->y = 0;
		za->dy = 100 + random() % 100;
	}
		
	if (za->x > za->ww) {
		za->x = za->ww;
		za->dx = -(100 + random() % 100);
	}

	if (za->y > za->hh) {
		za->y = za->hh;
		za->dy = -(100 + random() % 100);
	}
}


static void DisplayImage (int x, int y, int w, int h)
{
#ifdef HAVE_XSHM_EXTENSION
	if (use_shm)
		XShmPutImage (display, window, gc, buffer_map, x, y, x, y,
			w, h, False);
	else
#endif /* HAVE_XSHM_EXTENSION */
		XPutImage(display, window, gc, buffer_map, x, y, x, y, w, h);
}


static void hack_main (void)
{
	int i;

	for (i = 0; i < num_zoom; i++) {
		if (move)
			update_position (zoom_box[i]);

		if (zoom_box[i]->n) {
			rotzoom (zoom_box[i]);
			zoom_box[i]->n--;
		} else {
			reset_zoom (zoom_box[i]);
		}
	}

	for (i = 0; i < num_zoom; i++) {
		DisplayImage(zoom_box[i]->x, zoom_box[i]->y,
			zoom_box[i]->w, zoom_box[i]->h);
	}

	XSync(display,False);
	screenhack_handle_events(display);
}


static void init_hack (void)
{
	int i;

	zoom_box = calloc (num_zoom, sizeof (struct zoom_area *));
	for (i = 0; i < num_zoom; i++) {
		zoom_box[i] = create_zoom ();
	}

	memcpy (buffer_map->data, orig_map->data,
		height * buffer_map->bytes_per_line);

	DisplayImage(0, 0, width, height);
	XSync(display,False);
}


static void setup_X (Display * disp, Window win)
{
	XWindowAttributes xwa;
	int depth;
	XGCValues gcv;
	long gcflags;

	XGetWindowAttributes (disp, win, &xwa);
	window = win;
	display = disp;
	depth = xwa.depth;
	colormap = xwa.colormap;
	width = xwa.width;
	height = xwa.height;
	visual = xwa.visual;

	if (width % 2)
		width++;
	if (height % 2)
		height++;

	gcv.function = GXcopy;
	gcv.subwindow_mode = IncludeInferiors;
	gcflags = GCForeground | GCFunction;
	if (use_subwindow_mode_p (xwa.screen, window))	/* see grabscreen.c */
		gcflags |= GCSubwindowMode;
	gc = XCreateGC (display, window, gcflags, &gcv);
	grab_screen_image (xwa.screen, window);

	orig_map = XGetImage (display, window, 0, 0, width, height, ~0L, ZPixmap);

	if (!gc) {
		fprintf(stderr, "XCreateGC failed\n");
		exit(1);
	}

	buffer_map = 0;

#ifdef HAVE_XSHM_EXTENSION
	if (use_shm) {
		buffer_map = create_xshm_image(display, xwa.visual, depth,
			ZPixmap, 0, &shm_info, width, height);
		if (!buffer_map) {
			use_shm = False;
			fprintf(stderr, "create_xshm_image failed\n");
		}
	}
#endif /* HAVE_XSHM_EXTENSION */

	if (!buffer_map) {
		buffer_map = XCreateImage(display, xwa.visual,
			depth, ZPixmap, 0, 0, width, height, 8, 0);
		buffer_map->data = (char *)calloc (buffer_map->height,
			buffer_map->bytes_per_line);
	}
}



char *progclass = "Rotzoomer";

char *defaults[] = {
#ifdef HAVE_XSHM_EXTENSION
	"*useSHM: True",
#endif
	"*delay: 10000",
	"*move: False",
	"*numboxes: 2",
	0
};


XrmOptionDescRec options[] = {
#ifdef HAVE_XSHM_EXTENSION
	{ "-shm",	".useSHM",	XrmoptionNoArg, "True" },
	{ "-no-shm",	".useSHM",	XrmoptionNoArg, "False" },
#endif
	{ "-move",	".move",	XrmoptionNoArg, "True"},
	{ "-no-move",	".move",	XrmoptionNoArg, "False"},
	{ "-delay",	".delay",	XrmoptionSepArg, 0},
	{ "-n",		".numboxes",	XrmoptionSepArg, 0},
	{ 0, 0, 0, 0 }
};


void screenhack(Display *disp, Window win)
{
#ifdef HAVE_XSHM_EXTENSION
	use_shm = get_boolean_resource ("useSHM", "Boolean");
#endif
	num_zoom = get_integer_resource("numboxes", "Integer");
	move = get_boolean_resource("move", "Boolean");
	delay = get_integer_resource("delay", "Integer");

	setup_X (disp, win);

	init_hack ();

	/* Main drawing loop */
	while (42) {
		hack_main ();
		usleep (delay);
	}
}
