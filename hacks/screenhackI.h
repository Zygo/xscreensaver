/* xscreensaver, Copyright (c) 1992-2018 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Found in Don Hopkins' .plan file:
 *
 *   The color situation is a total flying circus.  The X approach to
 *   device independence is to treat everything like a MicroVax framebuffer
 *   on acid.  A truely portable X application is required to act like the
 *   persistent customer in the Monty Python ``Cheese Shop'' sketch.  Even
 *   the simplest applications must answer many difficult questions, like:
 *
 *   WHAT IS YOUR DISPLAY?
 *       display = XOpenDisplay("unix:0");
 *   WHAT IS YOUR ROOT?
 *       root = RootWindow(display, DefaultScreen(display));
 *   AND WHAT IS YOUR WINDOW?
 *       win = XCreateSimpleWindow(display, root, 0, 0, 256, 256, 1,
 *                                 BlackPixel(display, DefaultScreen(display)),
 *                                 WhitePixel(display, DefaultScreen(display)))
 *   OH ALL RIGHT, YOU CAN GO ON.
 *
 *   WHAT IS YOUR DISPLAY?
 *         display = XOpenDisplay("unix:0");
 *   WHAT IS YOUR COLORMAP?
 *         cmap = DefaultColormap(display, DefaultScreen(display));
 *   AND WHAT IS YOUR FAVORITE COLOR?
 *         favorite_color = 0; / * Black. * /
 *         / * Whoops! No, I mean: * /
 *         favorite_color = BlackPixel(display, DefaultScreen(display));
 *         / * AAAYYYYEEEEE!! (client dumps core & falls into the chasm) * /
 *
 *   WHAT IS YOUR DISPLAY?
 *         display = XOpenDisplay("unix:0");
 *   WHAT IS YOUR VISUAL?
 *         struct XVisualInfo vinfo;
 *         if (XMatchVisualInfo(display, DefaultScreen(display),
 *                              8, PseudoColor, &vinfo) != 0)
 *            visual = vinfo.visual;
 *   AND WHAT IS THE NET SPEED VELOCITY OF AN XConfigureWindow REQUEST?
 *         / * Is that a SubStructureRedirectMask or a ResizeRedirectMask? * /
 *   WHAT?! HOW AM I SUPPOSED TO KNOW THAT?
 *   AAAAUUUGGGHHH!!!! (server dumps core & falls into the chasm)
 */

#ifndef __SCREENHACK_I_H__
#define __SCREENHACK_I_H__

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifdef __hpux
 /* Which of the ten billion standards does values.h belong to?
    What systems always have it? */
# include <values.h>
#endif

#ifdef HAVE_JWXYZ
# include "jwxyz.h"
# include <string.h> /* X11/Xos.h brings this in. */
/* From utils/visual.c. */
# define DEFAULT_VISUAL	-1
# define GL_VISUAL	-6
#else  /* real X11 */
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <X11/Xresource.h>
# include <X11/Xos.h>
#endif /* !HAVE_JWXYZ */

#if defined(USE_IPHONE) || defined(HAVE_ANDROID)
# define HAVE_MOBILE
#endif

#ifdef HAVE_ANDROID
 /* So that hacks' debug output shows up in logcat... */
# undef  fprintf
# define fprintf(S, ...) Log(__VA_ARGS__)
#endif

/* M_PI ought to have been defined in math.h, but... */
#ifndef M_PI
# define M_PI 3.1415926535
#endif

#ifndef M_PI_2
# define M_PI_2 1.5707963267
#endif

#ifndef Button6
# define Button6 6
# define Button7 7
#endif

#include "yarandom.h"
#include "usleep.h"
#include "resources.h"
#include "hsv.h"
#include "colors.h"
#include "grabscreen.h"
#include "visual.h"
#include "fps.h"
#include "font-retry.h"

#ifdef HAVE_RECORD_ANIM
# include "recanim.h"
#endif

/* Be Posixly correct */
#undef  bzero
#define bzero  __ERROR_use_memset_not_bzero_in_xscreensaver__
#undef  bcopy
#define bcopy  __ERROR_use_memcpy_not_bcopy_in_xscreensaver__
#undef  ftime
#define ftime  __ERROR_use_gettimeofday_not_ftime_in_xscreensaver__
#undef  sleep
#define sleep  __ERROR_do_not_sleep_in_xscreensaver__

extern Bool mono_p;

struct xscreensaver_function_table {

  const char *progclass;
  const char * const *defaults;
  const XrmOptionDescRec *options;

  void           (*setup_cb)   (struct xscreensaver_function_table *, void *);
  void *         setup_arg;

  void *         (*init_cb)    (Display *, Window);
  unsigned long  (*draw_cb)    (Display *, Window, void *);
  void           (*reshape_cb) (Display *, Window, void *,
                                unsigned int w, unsigned int h);
  Bool           (*event_cb)   (Display *, Window, void *, XEvent *);
  void           (*free_cb)    (Display *, Window, void *);
  void           (*fps_cb)     (Display *, Window, fps_state *, void *);
  void           (*fps_free)   (fps_state *);

# ifndef HAVE_JWXYZ
  Visual *       (*pick_visual_hook) (Screen *);
  Bool           (*validate_visual_hook) (Screen *, const char *, Visual *);
# else
  int            visual;
# endif

};

extern const char *progname;

#endif /* __SCREENHACK_I_H__ */
