
#if 0
 * Found in Don Hopkin`s .plan file:
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
 *         favorite_color = 0; /* Black. */
 *         /* Whoops! No, I mean: */
 *         favorite_color = BlackPixel(display, DefaultScreen(display));
 *         /* AAAYYYYEEEEE!! (client dumps core & falls into the chasm) */
 *
 *   WHAT IS YOUR DISPLAY?
 *         display = XOpenDisplay("unix:0");
 *   WHAT IS YOUR VISUAL?
 *         struct XVisualInfo vinfo;
 *         if (XMatchVisualInfo(display, DefaultScreen(display),
 *                              8, PseudoColor, &vinfo) != 0)
 *            visual = vinfo.visual;
 *   AND WHAT IS THE NET SPEED VELOCITY OF AN XConfigureWindow REQUEST?
 *         /* Is that a SubStructureRedirectMask or a ResizeRedirectMask? */
 *   WHAT??! HOW AM I SUPPOSED TO KNOW THAT?
 *   AAAAUUUGGGHHH!!!! (server dumps core & falls into the chasm)
 *
#endif /* 0 */

#ifndef _SCREENHACK_H_
#define _SCREENHACK_H_

#if __STDC__
#include <stdlib.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xos.h>
#include "vroot.h"

extern Bool mono_p;
extern char *progname;
extern char *progclass;
extern XrmDatabase db;
extern XrmOptionDescRec options [];
extern int options_size;
extern char *defaults [];

#if __STDC__

# define bcopy(from,to,size) memcpy((to),(from),(size))
# define bzero(addr,size) memset((addr),0,(size))

# if defined(SVR4) || defined(SYSV)
extern int rand (void);
extern void srand (unsigned int);
#  define random() rand()
#  define srandom(i) srand((unsigned int)(i))
# else /* !totally-losing-SYSV */
extern long random (void);
extern void srandom (int);
# endif /* !totally-losing-SYSV */
#endif /* __STDC__ */

#if __STDC__
# define P(x)x
#else
# define P(x)()
#endif

extern void screenhack P((Display*,Window));

#define usleep screenhack_usleep

extern void screenhack_usleep P((unsigned long));
extern char *get_string_resource P((char*,char*));
extern Bool get_boolean_resource P((char*,char*));
extern int get_integer_resource P((char*,char*));
extern double get_float_resource P((char*,char*));
extern unsigned int get_pixel_resource P((char*,char*,Display*,Colormap));
extern unsigned int get_minutes_resource P((char*,char*));
extern unsigned int get_seconds_resource P((char*,char*));

extern Visual *get_visual_resource P((Display *, char *, char *));
extern int get_visual_depth P((Display *, Visual *));

void hsv_to_rgb P((int,double,double,unsigned short*,
		   unsigned short*,unsigned short*));
void rgb_to_hsv P((unsigned short,unsigned short,unsigned short,
		   int*,double*,double*));
void cycle_hue P((XColor*,int));

void make_color_ramp P((int h1, double s1, double v1,
			int h2, double s2, double v2,
			XColor *pixels, int npixels));

static double _frand_tmp_;
#define frand(f)							\
 (_frand_tmp_ = (((double) random()) / 					\
		 (((double) ((unsigned int)~0)) / ((double) (f+f)))),	\
  _frand_tmp_ < 0 ? -_frand_tmp_ : _frand_tmp_)

#undef P
#endif /* _SCREENHACK_H_ */
