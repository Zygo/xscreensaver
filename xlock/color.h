#if !defined( lint ) && !defined( SABER )
/* #ident	"@(#)color.h 4.14 99/06/17 xlockmore" */

#endif

/*-
 * Color stuff
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 17-06-99: Started log. :)
 */

extern unsigned long allocPixel(Display * display, Colormap cmap,
				char *name, char *def);

extern void setColormap(Display * display, Window window, Colormap map,
			Bool inwindow);
extern void fixColormap(ModeInfo * mi, int ncolors, float saturation,
	  Bool mono, Bool install, Bool inroot, Bool inwindow, Bool verbose);
#if 0
extern int  preserveColors(unsigned long black, unsigned long white,
			   unsigned long bg, unsigned long fg);
#endif
#ifdef HAVE_XPM
extern void reserveColors(ModeInfo * mi, Colormap cmap,
			  unsigned long *black);

#endif

/* the rest of this file is a modified version of colors.h, hsv.h and
 * visual.h from xscreensaver
 *
 * xscreensaver, Copyright (c) 1992, 1997 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifndef __COLORS_H__
#define __COLORS_H__

/* Like XFreeColors, but works on `XColor *' instead of `unsigned long *'
 */
extern void free_colors(Display *, Colormap, XColor *, int ncolors);


/* Allocates writable, non-contiguous color cells.  The number requested is
   passed in *ncolorsP, and the number actually allocated is returned there.
   (Unlike XAllocColorCells(), this will allocate as many as it can, instead
   of failing if they can't all be allocated.)
 */
#if 0
extern void allocate_writable_colors(Display * dpy, Colormap cmap,
				     unsigned long *pixels, int *ncolorsP);
#endif

/* Generates a sequence of colors evenly spaced between the given pair
   of HSV coordinates.

   If closed_p is true, the colors will go from the first point to the
   second then back to the first.

   If allocate_p is true, the colors will be allocated from the map;
   if enough colors can't be allocated, we will try for less, and the
   result will be returned to ncolorsP.

   If writable_p is true, writable color cells will be allocated;
   otherwise, read-only cells will be allocated.
 */
extern void make_color_ramp(Display * dpy, Colormap cmap,
			    int h1, double s1, double v1,
			    int h2, double s2, double v2,
			    XColor * colors, int *ncolorsP,
			    Bool closed_p,
			    Bool allocate_p,
			    Bool writable_p);

/* Generates a sequence of colors evenly spaced around the triangle
   indicated by the thee HSV coordinates.

   If allocate_p is true, the colors will be allocated from the map;
   if enough colors can't be allocated, we will try for less, and the
   result will be returned to ncolorsP.

   If writable_p is true, writable color cells will be allocated;
   otherwise, read-only cells will be allocated.
 */
#if 0
extern void make_color_loop(Display *, Colormap,
			    int h1, double s1, double v1,
			    int h2, double s2, double v2,
			    int h3, double s3, double v3,
			    XColor * colors, int *ncolorsP,
			    Bool allocate_p,
			    Bool writable_p);
#endif

/* Allocates a hopefully-interesting colormap, which will be a closed loop
   without any sudden transitions.

   If allocate_p is true, the colors will be allocated from the map;
   if enough colors can't be allocated, we will try for less, and the
   result will be returned to ncolorsP.  An error message will be
   printed on stderr depending on verbose and debug.

   If *writable_pP is true, writable color cells will be allocated;
   otherwise, read-only cells will be allocated.  If no writable cells
   cannot be allocated, we will try to allocate unwritable cells
   instead, and print a message on stderr to that effect.
 */
extern void make_smooth_colormap(ModeInfo * mi,
				 Colormap cmap,
				 XColor * colors, int *ncolorsP,
				 Bool allocate_p,
				 Bool * writable_pP);

/* Allocates a uniform colormap which touches each hue of the spectrum,
   evenly spaced.  The saturation and intensity are chosen randomly, but
   will be high enough to be visible.

   If allocate_p is true, the colors will be allocated from the map;
   if enough colors can't be allocated, we will try for less, and the
   result will be returned to ncolorsP.  An error message will be
   printed on stderr depending on verbose and debug.

   If *writable_pP is true, writable color cells will be allocated;
   otherwise, read-only cells will be allocated.  If no writable cells
   cannot be allocated, we will try to allocate unwritable cells
   instead, and print a message on stderr to that effect.
 */
extern void make_uniform_colormap(ModeInfo * mi,
				  Colormap cmap,
				  XColor * colors, int *ncolorsP,
				  Bool allocate_p,
				  Bool * writable_pP);

/* Allocates a random colormap (the colors are unrelated to one another.)
   If `bright_p' is false, the colors will be completely random; if it is
   true, all of the colors will be bright enough to see on a black background.

   If allocate_p is true, the colors will be allocated from the map;
   if enough colors can't be allocated, we will try for less, and the
   result will be returned to ncolorsP.  An error message will be
   printed on stderr depending on verbose and debug.

   If *writable_pP is true, writable color cells will be allocated;
   otherwise, read-only cells will be allocated.  If no writable cells
   cannot be allocated, we will try to allocate unwritable cells
   instead, and print a message on stderr to that effect.
 */
extern void make_random_colormap(ModeInfo * mi,
				 Colormap cmap,
				 XColor * colors, int *ncolorsP,
				 Bool bright_p,
				 Bool allocate_p,
				 Bool * writable_pP);


/* Assuming that the array of colors indicates the current state of a set
   of writable color cells, this rotates the contents of the array by
   `distance' steps, moving the colors of cell N to cell (N - distance).
 */
extern void rotate_colors(Display *, Colormap,
			  XColor *, int ncolors, int distance);

#endif /* __COLORS_H__ */
/* xscreensaver, Copyright (c) 1992, 1997 Jamie Zawinski <jwz@jwz.org>

 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifndef __HSV_H__
#define __HSV_H__

/* Converts between RGB and HSV color spaces.
   R, G, and B are in the range 0 - 65535;
   H is in the range 0 - 360;
   S and V are in the range 0.0 - 1.0.
 */
extern void hsv_to_rgb(int h, double s, double v,
		       unsigned short *r,
		       unsigned short *g,
		       unsigned short *b);
#if 0
extern void rgb_to_hsv(unsigned short r, unsigned short g, unsigned short b,
		       int *h, double *s, double *v);
#endif

#endif /* __HSV_H__ */
/* xscreensaver, Copyright (c) 1993-1998 by Jamie Zawinski <jwz@jwz.org>

 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifndef __VISUAL_H__
#define __VISUAL_H__

extern Visual *get_visual(Screen *, const char *name, Bool, Bool);
extern Visual *get_visual_resource(Screen *, char *, char *, Bool);
extern int  visual_depth(Screen *, Visual *);

/* extern int visual_pixmap_depth (Screen *, Visual *); */
extern int  visual_class(Screen *, Visual *);
extern int  visual_cells(Screen *, Visual *);
extern int  screen_number(Screen *);
extern Visual *find_similar_visual(Screen *, Visual * old);
extern void describe_visual(FILE * f, Screen *, Visual *);
extern Visual *get_overlay_visual(Screen *, unsigned long *pixel_return);

#ifdef __cplusplus
  extern "C" {
#endif
extern Bool has_writable_cells(ModeInfo * mi);
#ifdef __cplusplus
  }
#endif

#endif /* __VISUAL_H__ */
