/* xlockmore.h --- xscreensaver compatibility layer for xlockmore modules.
 * xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * See xlockmore.h and xlockmore.c.
 */

#ifndef __XLOCKMORE_INTERNAL_H__
#define __XLOCKMORE_INTERNAL_H__

#include "screenhack.h"

/* Compatibility with the xlockmore RNG API
   (note that the xlockmore hacks never expect negative numbers.)
 */
#define LRAND()			((long) (random() & 0x7fffffff))
#define NRAND(n)		((int) (LRAND() % (n)))
#define MAXRAND			(2147483648.0) /* unsigned 1<<31 as a float */
#define SRAND(n)		/* already seeded by screenhack.c */


typedef struct ModeInfo {
  Display *dpy;
  Window window;
  Bool root_p;
  int npixels;
  unsigned long *pixels;
  XColor *colors;
  Bool writable_p;
  unsigned long white;
  unsigned long black;
  XWindowAttributes xgwa;
  GC gc;
  long pause;
  Bool fullrandom;
  long cycles;
  long batchcount;
  long size;
  Bool threed;
  long threed_left_color;
  long threed_right_color;
  long threed_both_color;
  long threed_none_color;
  long threed_delta;
  Bool wireframe_p;
} ModeInfo;

typedef enum {  t_String, t_Float, t_Int, t_Bool } xlockmore_type;

typedef struct {
  void *var;
  char *name;
  char *classname;
  char *def;
  xlockmore_type type;
} argtype;

typedef struct {
  char *opt;
  char *desc;
} OptionStruct;

typedef struct {
  int numopts;
  XrmOptionDescRec *opts;
  int numvarsdesc;
  argtype *vars;
  OptionStruct *desc;
} ModeSpecOpt;

extern void xlockmore_screenhack (Display *dpy, Window window,
				  Bool want_writable_colors,
				  Bool want_uniform_colors,
				  Bool want_smooth_colors,
				  Bool want_bright_colors,
				  void (*hack_init) (ModeInfo *),
				  void (*hack_draw) (ModeInfo *),
				  void (*hack_free) (ModeInfo *));

#ifdef USE_GL
extern Visual *get_gl_visual (Screen *screen, char *name, char *class);
#endif

#endif /* __XLOCKMORE_INTERNAL_H__ */
