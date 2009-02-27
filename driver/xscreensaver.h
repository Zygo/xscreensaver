/* xscreensaver, Copyright (c) 1993 Jamie Zawinski <jwz@mcom.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef VMS
#include "config.h"
#endif /* VMS */

#if __STDC__
# include <stdlib.h>
# include <unistd.h>
#endif

#if __STDC__
# define P(x)x
#else
# define P(x)()
# ifndef const
#  define const /**/
# endif
#endif

#ifdef NO_MOTIF
# define NO_DEMO_MODE
# ifndef NO_LOCKING
#  define NO_LOCKING
# endif
#endif

extern char *progname, *progclass;
extern char *screensaver_version;

extern Display *dpy;
extern Screen *screen;
extern Visual *visual;
extern int visual_depth;

extern Bool verbose_p;

extern void initialize_screensaver_window P((void));
extern void raise_window P((Bool inhibit_fade, Bool between_hacks_p));
extern void blank_screen P((void));
extern void unblank_screen P((void));
extern void restart_process P((void));

extern void restore_real_vroot P((void));

extern void spawn_screenhack P((Bool));
extern void kill_screenhack P((void));

extern Colormap copy_colormap P((Display *, Colormap, Colormap));
extern void fade_colormap P((Display*, Colormap, Colormap, int, int, Bool));
extern void blacken_colormap P((Display *, Colormap));

extern int BadWindow_ehandler P((Display *dpy, XErrorEvent *error));
