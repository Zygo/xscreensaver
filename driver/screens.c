/* screens.c --- dealing with RANDR, Xinerama, and VidMode Viewports.
 * xscreensaver, Copyright (c) 1991-2008 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/*   There are a bunch of different mechanisms for multiple monitors
 *   available in X.  XScreenSaver needs to care about this for two
 *   reasons: first, to ensure that all visible areas go black; and
 *   second, so that the windows of screen savers exactly fill the
 *   glass of each monitor (instead of one saver spanning multiple
 *   monitors, or a monitor displaying only a sub-rectangle of the
 *   screen saver.)
 *
 *   1) Multi-screen:
 *
 *      This is the original way.  Each monitor gets its own display
 *      number.  :0.0 is the first one, :0.1 is the next, etc.  The
 *      value of $DISPLAY determines which screen windows open on by
 *      default.  A single app can open windows on multiple screens
 *      with the same display connection, but windows cannot be moved
 *      from one screen to another.  The mouse can be moved from one
 *      screen to another, though.  Screens may be different depths
 *      (e.g., one can be TrueColor and one can be PseudoColor.)
 *      Screens cannot be resized or moved without restarting X.
 *
 *      Everyone hates this way of doing things because of the
 *      inability to move a window from one screen to another without
 *      restarting the application.
 *
 *   2) Xinerama:
 *
 *      There is a single giant root window that spans all the
 *      monitors.  All monitors are the same depth, and windows can be
 *      moved around.  Applications can learn which rectangles are
 *      actually visible on monitors by querying the Xinerama server
 *      extension.  (If you don't do that, you end up with dialog
 *      boxes that try to appear in the middle of the screen actually
 *      spanning the gap between two monitors.)
 *
 *      Xinerama doesn't work with DRI, which means that if you use
 *      it, you lose hardware acceleration on OpenGL programs.  Also,
 *      screens can't be resized or moved without restarting X.
 *
 *   3) Vidmode Viewports:
 *
 *      With this extension, the root window can be bigger than the
 *      monitor.  Moving the mouse near the edges of the screen
 *      scrolls around, like a pan-and-scan movie.  There can also be
 *      a hot key for changing the monitor's resolution (zooming
 *      in/out).
 *
 *      Trying to combine this with Xinerama crashes the server, so
 *      you can only use this if you have only a single screen, or are
 *      in old-multi-screen mode.
 *
 *      Also, half the time it doesn't work at all: it tends to lie
 *      about the size of the rectangle in use.
 *
 *   4) RANDR 1.0:
 *
 *      The first version of the "Resize and Rotate" extension let you
 *      change the resolution of a screen on the fly.  The root window
 *      would actually resize.  However, it was also incompatible with
 *      Xinerama (did it crash, or just do nothing? I can't remember)
 *      so you needed to be in single-screen or old multi-screen mode.
 *      I believe RANDR could co-exist with Vidmode Viewports, but I'm
 *      not sure.
 *
 *   5) RANDR 1.2:
 *
 *      Finally, RANDR added the functionality of Xinerama, plus some.
 *      Each X screen (in the sense of #1, "multi-screen") can have a
 *      number of sub-rectangles that are displayed on monitors, and
 *      each of those sub-rectangles can be displayed on more than one
 *      monitor.  So it's possible (I think) to have a hybrid of
 *      multi-screen and Xinerama (e.g., to have two monitors running
 *      in one depth, and three monitors running in another?)
 *      Typically though, there will be a single X screen, with
 *      Xinerama-like division of that large root window onto multiple
 *      monitors.  Also everything's dynamic: monitors can be added,
 *      removed, and resized at runtime.
 *
 *      I believe that as of RANDR 1.2, the Xinerama extension still
 *      exists but only as a compatiblity layer: it's actually
 *      returning data from the RANDR extension.
 *
 *      Though RANDR 1.2 allows the same image to be cloned onto more
 *      than one monitor, and also allows one monitor to show a
 *      subsection of something on another monitor (e.g., the
 *      rectangles can be enclosed or overlap).  Since there's no way
 *      to put seperate savers on those duplicated-or-overlapping
 *      monitors, xscreensaver just ignores them (which allows them to
 *      display duplicates or overlaps).
 *
 *  5a) Nvidia fucks it up:
 *
 *      Nvidia drivers as of Aug 2008 running in "TwinView" mode
 *      apparently report correct screen geometry via Xinerama, but
 *      report one giant screen via RANDR.  The response from the
 *      nvidia developers is, "we don't support RANDR, use Xinerama
 *      instead."  Which is a seriously lame answer.  So, xscreensaver
 *      has to query *both* extensions, and make a guess as to which
 *      is to be believed.
 *
 *  5b) Also sometimes RANDR says stupid shit like, "You have one
 *      screen, and it has no available orientations or sizes."
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <X11/Xlib.h>

#ifdef HAVE_RANDR
# include <X11/extensions/Xrandr.h>
#endif /* HAVE_RANDR */

#ifdef HAVE_XINERAMA
# include <X11/extensions/Xinerama.h>
#endif /* HAVE_XINERAMA */

#ifdef HAVE_XF86VMODE
# include <X11/extensions/xf86vmode.h>
#endif /* HAVE_XF86VMODE */

/* This file doesn't need the Xt headers, so stub these types out... */
#undef XtPointer
#define XtAppContext void*
#define XrmDatabase  void*
#define XtIntervalId void*
#define XtPointer    void*
#define Widget       void*

#include "xscreensaver.h"
#include "visual.h"


typedef enum { S_SANE, S_ENCLOSED, S_DUPLICATE, S_OVERLAP, 
               S_OFFSCREEN, S_DISABLED } monitor_sanity;

/* 'typedef monitor' is in types.h */
struct _monitor {
  int id;
  char *desc;
  Screen *screen;
  int x, y, width, height;
  monitor_sanity sanity;	/* I'm not crazy you're the one who's crazy */
  int enemy;			/* which monitor it overlaps or duplicates */
  char *err;			/* msg to print at appropriate later time;
                                   exists only on monitor #0. */
};

static Bool layouts_differ_p (monitor **a, monitor **b);


static void
free_monitors (monitor **monitors)
{
  monitor **m2 = monitors;
  if (! monitors) return;
  while (*m2) 
    {
      if ((*m2)->desc) free ((*m2)->desc);
      if ((*m2)->err) free ((*m2)->err);
      free (*m2);
      m2++;
    }
  free (monitors);
}


static char *
append (char *s1, const char *s2)
{
  char *s = (char *) malloc ((s1 ? strlen(s1) : 0) +
                             (s2 ? strlen(s2) : 0) + 3);
  *s = 0;
  if (s1) strcat (s, s1);
  if (s1 && s2) strcat (s, "\n");
  if (s2) strcat (s, s2);
  if (s1) free (s1);
  return s;
}


#ifdef HAVE_XINERAMA

static monitor **
xinerama_scan_monitors (Display *dpy, char **errP)
{
  Screen *screen = DefaultScreenOfDisplay (dpy);
  int event, error, nscreens, i;
  XineramaScreenInfo *xsi;
  monitor **monitors;

  if (! XineramaQueryExtension (dpy, &event, &error))
    return 0;

  if (! XineramaIsActive (dpy)) 
    return 0;

  xsi = XineramaQueryScreens (dpy, &nscreens);
  if (!xsi) return 0;

  monitors = (monitor **) calloc (nscreens + 1, sizeof(*monitors));
  if (!monitors) return 0;

  for (i = 0; i < nscreens; i++)
    {
      monitor *m = (monitor *) calloc (1, sizeof (monitor));
      monitors[i] = m;
      m->id       = i;
      m->screen   = screen;
      m->x        = xsi[i].x_org;
      m->y        = xsi[i].y_org;
      m->width    = xsi[i].width;
      m->height   = xsi[i].height;
    }
  return monitors;
}

#endif /* HAVE_XINERAMA */


#ifdef HAVE_XF86VMODE

static monitor **
vidmode_scan_monitors (Display *dpy, char **errP)
{
  int event, error, nscreens, i;
  monitor **monitors;

  /* Note that XF86VidModeGetViewPort() tends to be full of lies on laptops
     that have a docking station or external monitor that runs in a different
     resolution than the laptop's screen:

         http://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=81593
         http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=208417
         http://bugs.xfree86.org/show_bug.cgi?id=421

     Presumably this is fixed by using RANDR instead of VidMode.
   */

# ifdef HAVE_XINERAMA
  /* Attempts to use the VidMode extension when the Xinerama extension is
     active can result in a server crash! Yay! */
  if (XQueryExtension (dpy, "XINERAMA", &error, &event, &error))
    return 0;
#  endif /* !HAVE_XINERAMA */

  if (! XF86VidModeQueryExtension (dpy, &event, &error))
    return 0;

  nscreens = ScreenCount (dpy);
  monitors = (monitor **) calloc (nscreens + 1, sizeof(*monitors));
  if (!monitors) return 0;

  for (i = 0; i < nscreens; i++)
    {
      monitor *m = (monitor *) calloc (1, sizeof (monitor));
      XF86VidModeModeLine ml;
      int dot;
      Screen *screen = ScreenOfDisplay (dpy, i);

      monitors[i] = m;
      m->id       = i;
      m->screen   = screen;

      if (! safe_XF86VidModeGetViewPort (dpy, i, &m->x, &m->y))
        m->x = m->y = -1;

      if (XF86VidModeGetModeLine (dpy, i, &dot, &ml))
        {
          m->width  = ml.hdisplay;
          m->height = ml.vdisplay;
        }

      /* Apparently, though the server stores the X position in increments of
         1 pixel, it will only make changes to the *display* in some other
         increment.  With XF86_SVGA on a Thinkpad, the display only updates
         in multiples of 8 pixels when in 8-bit mode, and in multiples of 4
         pixels in 16-bit mode.  I don't know what it does in 24- and 32-bit
         mode, because I don't have enough video memory to find out.

         I consider it a bug that XF86VidModeGetViewPort() is telling me the
         server's *target* scroll position rather than the server's *actual*
         scroll position.  David Dawes agrees, and says they may fix this in
         XFree86 4.0, but it's notrivial.

         He also confirms that this behavior is server-dependent, so the
         actual scroll position cannot be reliably determined by the client.
         So... that means the only solution is to provide a ``sandbox''
         around the blackout window -- we make the window be up to N pixels
         larger than the viewport on both the left and right sides.  That
         means some part of the outer edges of each hack might not be
         visible, but screw it.

         I'm going to guess that 16 pixels is enough, and that the Y dimension
         doesn't have this problem.

         The drawback of doing this, of course, is that some of the screenhacks
         will still look pretty stupid -- for example, "slidescreen" will cut
         off the left and right edges of the grid, etc.
      */
#  define FUDGE 16
      if (m->x > 0 && m->x < m->width - ml.hdisplay)
        {
          /* Not at left edge or right edge:
             Round X position down to next lower multiple of FUDGE.
             Increase width by 2*FUDGE in case some server rounds up.
           */
          m->x = ((m->x - 1) / FUDGE) * FUDGE;
          m->width += (FUDGE * 2);
        }
#  undef FUDGE
    }

  return monitors;
}

#endif /* HAVE_XF86VMODE */


#ifdef HAVE_RANDR

static monitor **
randr_scan_monitors (Display *dpy, char **errP)
{
  int event, error, major, minor, nscreens, i, j;
  monitor **monitors;
  Bool new_randr_p = False;

  if (! XRRQueryExtension (dpy, &event, &error))
    return 0;

  if (! XRRQueryVersion (dpy, &major, &minor))
    return 0;

  if (major <= 0)    /* Protocol was still in flux back then -- fuck it. */
    return 0;

# ifdef HAVE_RANDR_12
  new_randr_p = (major > 1 || (major == 1 && minor >= 2));
# endif

  if (! new_randr_p)
    /* RANDR 1.0 -- no Xinerama-like virtual screens. */
    nscreens = ScreenCount (dpy);
  else  /* RANDR 1.2 or newer -- built-in Xinerama */
    {
# ifdef HAVE_RANDR_12
      int xsc = ScreenCount (dpy);
      nscreens = 0;
      /* Add up the virtual screens on each X screen. */
      for (i = 0; i < xsc; i++)
        {
          XRRScreenResources *res = 
            XRRGetScreenResources (dpy, RootWindow (dpy, i));
          nscreens += res->noutput;
          XRRFreeScreenResources (res);
        }
# endif /* HAVE_RANDR_12 */
    }

  if (nscreens <= 0)
    {
      *errP = append (*errP,
                      "WARNING: RANDR reported no screens!  Ignoring it.");
      return 0;
    }

  monitors = (monitor **) calloc (nscreens + 1, sizeof(*monitors));
  if (!monitors) return 0;

  for (i = 0, j = 0; i < ScreenCount (dpy); i++)
    {
      Screen *screen = ScreenOfDisplay (dpy, i);

      if (! new_randr_p)  /* RANDR 1.0 */
        {
          XRRScreenConfiguration *rrc;
          monitor *m = (monitor *) calloc (1, sizeof (monitor));
          monitors[i] = m;
          m->screen   = screen;
          m->id       = i;

          rrc = XRRGetScreenInfo (dpy, RootWindowOfScreen (screen));
          if (rrc)
            {
              SizeID size = -1;
              Rotation rot = ~0;
              XRRScreenSize *rrsizes;
              int nsizes = 0;

              size = XRRConfigCurrentConfiguration (rrc, &rot);
              rrsizes = XRRConfigSizes (rrc, &nsizes);

              if (nsizes <= 0)  /* WTF?  Shouldn't happen but does. */
                {
                  m->width  = DisplayWidth (dpy, i);
                  m->height = DisplayHeight (dpy, i);
                }
              else if (rot & (RR_Rotate_90|RR_Rotate_270))
                {
                  m->width  = rrsizes[size].height;
                  m->height = rrsizes[size].width;
                }
              else
                {
                  m->width  = rrsizes[size].width;
                  m->height = rrsizes[size].height;
                }

              /* don't free 'rrsizes' */
              XRRFreeScreenConfigInfo (rrc);
            }
        }
      else   /* RANDR 1.2 or newer */
        {
# ifdef HAVE_RANDR_12
          int k;
          XRRScreenResources *res = 
            XRRGetScreenResources (dpy, RootWindowOfScreen (screen));
          for (k = 0; k < res->noutput; k++, j++)
            {
              monitor *m = (monitor *) calloc (1, sizeof (monitor));
              XRROutputInfo *rroi = XRRGetOutputInfo (dpy, res, 
                                                      res->outputs[k]);
              RRCrtc crtc = (rroi->crtc  ? rroi->crtc :
                             rroi->ncrtc ? rroi->crtcs[0] : 0);
              XRRCrtcInfo *crtci = (crtc ? XRRGetCrtcInfo(dpy, res, crtc) : 0);

              monitors[j] = m;
              m->screen   = screen;
              m->id       = (i * 1000) + j;
              m->desc     = (rroi->name ? strdup (rroi->name) : 0);

              if (crtci)
                {
                  /* Note: if the screen is rotated, XRRConfigSizes contains
                     the unrotated WxH, but XRRCrtcInfo contains rotated HxW.
                   */
                  m->x      = crtci->x;
                  m->y      = crtci->y;
                  m->width  = crtci->width;
                  m->height = crtci->height;
                }

              if (rroi->connection == RR_Disconnected)
                m->sanity = S_DISABLED;
              /* #### do the same for RR_UnknownConnection? */

              if (crtci) 
                XRRFreeCrtcInfo (crtci);
              XRRFreeOutputInfo (rroi);
            }
          XRRFreeScreenResources (res);
# endif /* HAVE_RANDR_12 */
        }
    }

  /* Work around more fucking brain damage. */
  {
    int ok = 0;
    int i = 0;
    while (monitors[i]) 
      {
        if (monitors[i]->width != 0 && monitors[i]->height != 0)
          ok++;
        i++;
      }
    if (! ok)
      {
        *errP = append (*errP,
              "WARNING: RANDR says all screens are 0x0!  Ignoring it.");
        free_monitors (monitors);
        monitors = 0;
      }
  }

  return monitors;
}

#endif /* HAVE_RANDR */


static monitor **
basic_scan_monitors (Display *dpy, char **errP)
{
  int nscreens = ScreenCount (dpy);
  int i;
  monitor **monitors = (monitor **) calloc (nscreens + 1, sizeof(*monitors));
  if (!monitors) return 0;

  for (i = 0; i < nscreens; i++)
    {
      Screen *screen = ScreenOfDisplay (dpy, i);
      monitor *m = (monitor *) calloc (1, sizeof (monitor));
      monitors[i] = m;
      m->id       = i;
      m->screen   = screen;
      m->x        = 0;
      m->y        = 0;
      m->width    = WidthOfScreen (screen);
      m->height   = HeightOfScreen (screen);
    }
  return monitors;
}


#if defined(HAVE_RANDR) && defined(HAVE_XINERAMA)

/*   From: Aaron Plattner <aplattner@nvidia.com>
     Date: August 7, 2008 10:21:25 AM PDT
     To: linux-bugs@nvidia.com

     The NVIDIA X driver does not yet support RandR 1.2.  The X server has
     a compatibility layer in it that allows RandR 1.2 clients to talk to
     RandR 1.1 drivers through an RandR 1.2 pseudo-output called "default".
     This reports the total combined resolution of the TwinView display,
     since it doesn't have any visibility into TwinView metamodes.  There
     is no way for the driver to prevent the server from turning on this
     compatibility layer.

     The intention is for X client applications to continue to use the
     Xinerama extension to query the screen geometry.  RandR 1.2 reports
     its own Xinerama info for this purpose.  I would recommend against
     modifying xscreensaver to try to get this information from RandR.
 */
static monitor **
randr_versus_xinerama_fight (Display *dpy, monitor **randr_monitors, 
                             char **errP)
{
  monitor **xinerama_monitors;

  if (!randr_monitors) 
    return 0;

  xinerama_monitors = xinerama_scan_monitors (dpy, errP);
  if (!xinerama_monitors)
    return randr_monitors;

  if (! layouts_differ_p (randr_monitors, xinerama_monitors))
    {
      free_monitors (xinerama_monitors);
      return randr_monitors;
    }
  else if (   randr_monitors[0] &&   !randr_monitors[1] &&  /* 1 monitor */
           xinerama_monitors[0] && xinerama_monitors[1])    /* >1 monitor */
    {
      *errP = append (*errP,
                      "WARNING: RANDR reports 1 screen but Xinerama\n"
                      "\t\treports multiple.  Believing Xinerama.");
      free_monitors (randr_monitors);
      return xinerama_monitors;
    }
  else
    {
      *errP = append (*errP,
                      "WARNING: RANDR and Xinerama report different\n"
                      "\t\tscreen layouts!  Believing RANDR.");
      free_monitors (xinerama_monitors);
      return randr_monitors;
    }
}

#endif /* HAVE_RANDR && HAVE_XINERAMA */


#ifdef DEBUG_MULTISCREEN

/* If DEBUG_MULTISCREEN is defined, then in "-debug" mode, xscreensaver
   will pretend that it is changing the number of connected monitors
   every few seconds, using the geometries in the following list,
   for stress-testing purposes.
 */
static monitor **
debug_scan_monitors (Display *dpy, char **errP)
{
  static const char * const geoms[] = {
    "1600x1028+0+22",
    "1024x768+0+22",
    "800x600+0+22",
    "800x600+0+22,800x600+800+22",
    "800x600+0+22,800x600+800+22,800x600+300+622",
    "800x600+0+22,800x600+800+22,800x600+0+622,800x600+800+622",
    "640x480+0+22,640x480+640+22,640x480+0+502,640x480+640+502",
    "640x480+240+22,640x480+0+502,640x480+640+502",
    "640x480+0+200,640x480+640+200",
    "800x600+400+22",
    "320x200+0+22,320x200+320+22,320x200+640+22,320x200+960+22,320x200+0+222,320x200+320+222,320x200+640+222,320x200+960+222,320x200+0+422,320x200+320+422,320x200+640+422,320x200+960+422,320x200+0+622,320x200+320+622,320x200+640+622,320x200+960+622,320x200+0+822,320x200+320+822,320x200+640+822,320x200+960+822"
  };
  static int index = 0;
  monitor **monitors = (monitor **) calloc (100, sizeof(*monitors));
  int nscreens = 0;
  Screen *screen = DefaultScreenOfDisplay (dpy);

  char *s = strdup (geoms[index]);
  char *token = strtok (s, ",");
  while (token)
    {
      monitor *m = calloc (1, sizeof (monitor));
      char c;
      m->id = nscreens;
      m->screen = screen;
      if (4 != sscanf (token, "%dx%d+%d+%d%c", 
                       &m->width, &m->height, &m->x, &m->y, &c))
        abort();
      m->width -= 2;
      m->height -= 2;
      monitors[nscreens++] = m;
      token = strtok (0, ",");
    }
  free (s);
  
  index = (index+1) % countof(geoms);
  return monitors;
}

#endif /* DEBUG_MULTISCREEN */


#ifdef QUAD_MODE
static monitor **
quadruple (monitor **monitors, Bool debug_p, char **errP)
{
  int i, j, count = 0;
  monitor **monitors2;
  while (monitors[count])
    count++;
  monitors2 = (monitor **) calloc (count * 4 + 1, sizeof(*monitors));
  if (!monitors2) abort();

  for (i = 0, j = 0; i < count; i++)
    {
      int k;
      for (k = 0; k < 4; k++)
        {
          monitors2[j+k] = (monitor *) calloc (1, sizeof (monitor));
          *monitors2[j+k] = *monitors[i];
          monitors2[j+k]->width  /= (debug_p ? 4 : 2);
          monitors2[j+k]->height /= 2;
          monitors2[j+k]->id = (monitors[i]->id * 4) + k;
          monitors2[j+k]->name = (monitors[i]->name
                                  ? strdup (monitors[i]->name) : 0);
        }
      monitors2[j+1]->x += monitors2[j]->width;
      monitors2[j+2]->y += monitors2[j]->height;
      monitors2[j+3]->x += monitors2[j]->width;
      monitors2[j+3]->y += monitors2[j]->height;
      j += 4;
    }

  free_monitors (monitors);
  return monitors2;
}
#endif /* QUAD_MODE */


static monitor **
scan_monitors (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  monitor **monitors = 0;
  char *err = 0;

# ifdef DEBUG_MULTISCREEN
    if (! monitors) monitors = debug_scan_monitors (si->dpy, &err);
# endif

# ifdef HAVE_RANDR
  if (! p->getviewport_full_of_lies_p)
    if (! monitors) monitors = randr_scan_monitors (si->dpy, &err);

#  ifdef HAVE_XINERAMA
   monitors = randr_versus_xinerama_fight (si->dpy, monitors, &err);
#  endif
# endif /* HAVE_RANDR */

# ifdef HAVE_XF86VMODE
  if (! monitors) monitors = vidmode_scan_monitors (si->dpy, &err);
# endif

# ifdef HAVE_XINERAMA
  if (! monitors) monitors = xinerama_scan_monitors (si->dpy, &err);
# endif

  if (! monitors) monitors = basic_scan_monitors (si->dpy, &err);

# ifdef QUAD_MODE
  if (p->quad_p)
    monitors = quadruple (monitors, p->debug_p, &err);
# endif

  if (monitors && err) monitors[0]->err = err;

  return monitors;
}


static Bool
monitors_overlap_p (monitor *a, monitor *b)
{
  /* Two rectangles overlap if the max of the tops is less than the
     min of the bottoms and the max of the lefts is less than the min
     of the rights.
   */
# undef MAX
# undef MIN
# define MAX(A,B) ((A)>(B)?(A):(B))
# define MIN(A,B) ((A)<(B)?(A):(B))

  int maxleft  = MAX(a->x, b->x);
  int maxtop   = MAX(a->y, b->y);
  int minright = MIN(a->x + a->width  - 1, b->x + b->width);
  int minbot   = MIN(a->y + a->height - 1, b->y + b->height);
  return (maxtop < minbot && maxleft < minright);
}


static Bool
plausible_aspect_ratio_p (monitor **monitors)
{
  /* Modern wide-screen monitors come in the following aspect ratios:

            One monitor:        If you tack a 640x480 monitor
                                onto the right, the ratio is:
         16 x 9    --> 1.78
        852 x 480  --> 1.77        852+640 x 480  --> 3.11      "SD 480p"
       1280 x 720  --> 1.78       1280+640 x 720  --> 2.67      "HD 720p"
       1280 x 920  --> 1.39       1280+640 x 920  --> 2.09
       1366 x 768  --> 1.78       1366+640 x 768  --> 2.61      "HD 768p"
       1440 x 900  --> 1.60       1440+640 x 900  --> 2.31
       1680 x 1050 --> 1.60       1680+640 x 1050 --> 2.21
       1690 x 1050 --> 1.61       1690+640 x 1050 --> 2.22
       1920 x 1080 --> 1.78       1920+640 x 1080 --> 2.37      "HD 1080p"
       1920 x 1200 --> 1.60       1920+640 x 1200 --> 2.13
       2560 x 1600 --> 1.60       2560+640 x 1600 --> 2.00

     So that implies that if we ever see an aspect ratio >= 2.0,
     we can be pretty sure that the X server is lying to us, and
     that's actually two monitors, not one.
   */
  if (monitors[0] && !monitors[1] &&    /* exactly 1 monitor */
      monitors[0]->height &&
      monitors[0]->width / (double) monitors[0]->height >= 1.9)
    return False;
  else
    return True;
}


/* Mark the ones that overlap, etc.
 */
static void
check_monitor_sanity (monitor **monitors)
{
  int i, j, count = 0;

  while (monitors[count])
    count++;

#  define X1 monitors[i]->x
#  define X2 monitors[j]->x
#  define Y1 monitors[i]->y
#  define Y2 monitors[j]->y
#  define W1 monitors[i]->width
#  define W2 monitors[j]->width
#  define H1 monitors[i]->height
#  define H2 monitors[j]->height

  /* If a monitor is enclosed by any other monitor, that's insane.
   */
  for (i = 0; i < count; i++)
    for (j = 0; j < count; j++)
      if (i != j &&
          monitors[i]->sanity == S_SANE &&
          monitors[j]->sanity == S_SANE &&
          monitors[i]->screen == monitors[j]->screen &&
          X2 >= X1 &&
          Y2 >= Y1 &&
          (X2+W2) <= (X1+W1) &&
          (Y2+H2) <= (Y1+H1))
        {
          if (X1 == X2 &&
              Y1 == Y2 &&
              W1 == W2 &&
              H1 == H2)
            monitors[j]->sanity = S_DUPLICATE;
          else 
            monitors[j]->sanity = S_ENCLOSED;
          monitors[j]->enemy = i;
        }

  /* After checking for enclosure, check for other lossage against earlier
     monitors.  We do enclosure first so that we make sure to pick the
     larger one.
   */
  for (i = 0; i < count; i++)
    for (j = 0; j < i; j++)
      {
        if (monitors[i]->sanity != S_SANE) continue; /* already marked */
        if (monitors[j]->sanity != S_SANE) continue;
        if (monitors[i]->screen != monitors[j]->screen) continue;

        if (monitors_overlap_p (monitors[i], monitors[j]))
          {
            monitors[i]->sanity = S_OVERLAP;
            monitors[i]->enemy = j;
          }
      }

  /* Finally, make sure all monitors have sane positions and sizes.
     Xinerama sometimes reports 1024x768 VPs at -1936862040, -1953705044.
   */
  for (i = 0; i < count; i++)
    {
      if (monitors[i]->sanity != S_SANE) continue; /* already marked */
      if (X1    <  0      || Y1    <  0 || 
          W1    <= 0      || H1    <= 0 || 
          X1+W1 >= 0x7FFF || Y1+H1 >= 0x7FFF)
        {
          monitors[i]->sanity = S_OFFSCREEN;
          monitors[i]->enemy = 0;
        }
    }

#  undef X1
#  undef X2
#  undef Y1
#  undef Y2
#  undef W1
#  undef W2
#  undef H1
#  undef H2
}


static Bool
layouts_differ_p (monitor **a, monitor **b)
{
  if (!a || !b) return True;
  while (1)
    {
      if (!*a) break;
      if (!*b) break;
      if ((*a)->screen != (*b)->screen ||
          (*a)->x      != (*b)->x      ||
          (*a)->y      != (*b)->y      ||
          (*a)->width  != (*b)->width  ||
          (*a)->height != (*b)->height)
        return True;
      a++;
      b++;
    }
  if (*a) return True;
  if (*b) return True;

  return False;
}


void
describe_monitor_layout (saver_info *si)
{
  monitor **monitors = si->monitor_layout;
  int count = 0;
  int good_count = 0;
  int bad_count = 0;
  int implausible_p = !plausible_aspect_ratio_p (monitors);

  while (monitors[count])
    {
      if (monitors[count]->sanity == S_SANE)
        good_count++;
      else
        bad_count++;
      count++;
    }

  if (monitors[0]->err)		/* deferred error msg */
    {
      char *token = strtok (monitors[0]->err, "\n");
      while (token)
        {
          fprintf (stderr, "%s: %s\n", blurb(), token);
          token = strtok (0, "\n");
        }
      free (monitors[0]->err);
      monitors[0]->err = 0;
    }

  if (count == 0)
    fprintf (stderr, "%s: no screens!\n", blurb());
  else
    {
      int i;
      fprintf (stderr, "%s: screens in use: %d\n", blurb(), good_count);
      for (i = 0; i < count; i++)
        {
          monitor *m = monitors[i];
          if (m->sanity != S_SANE) continue;
          fprintf (stderr, "%s:  %3d/%d: %dx%d+%d+%d",
                   blurb(), m->id, screen_number (m->screen),
                   m->width, m->height, m->x, m->y);
          if (m->desc && *m->desc) fprintf (stderr, " (%s)", m->desc);
          fprintf (stderr, "\n");
        }
      if (bad_count > 0)
        {
          fprintf (stderr, "%s: rejected screens: %d\n", blurb(), bad_count);
          for (i = 0; i < count; i++)
            {
              monitor *m = monitors[i];
              monitor *e = monitors[m->enemy];
              if (m->sanity == S_SANE) continue;
              fprintf (stderr, "%s:  %3d/%d: %dx%d+%d+%d",
                       blurb(), m->id, screen_number (m->screen),
                       m->width, m->height, m->x, m->y);
              if (m->desc && *m->desc) fprintf (stderr, " (%s)", m->desc);
              fprintf (stderr, " -- ");
              switch (m->sanity)
                {
                case S_SANE: abort(); break;
                case S_ENCLOSED:
                  fprintf (stderr, "enclosed by %d (%dx%d+%d+%d)\n",
                           e->id, e->width, e->height, e->x, e->y);
                  break;
                case S_DUPLICATE:
                  fprintf (stderr, "duplicate of %d\n", e->id);
                  break;
                case S_OVERLAP:
                  fprintf (stderr, "overlaps %d (%dx%d+%d+%d)\n",
                           e->id, e->width, e->height, e->x, e->y);
                  break;
                case S_OFFSCREEN:
                  fprintf (stderr, "off screen (%dx%d)\n",
                           WidthOfScreen (e->screen), 
                           HeightOfScreen (e->screen));
                  break;
                case S_DISABLED:
                  fprintf (stderr, "output disabled\n");
                  break;
                }
            }
        }

      if (implausible_p)
        fprintf (stderr,
                 "%s: WARNING: single screen aspect ratio is %dx%d = %.2f\n"
                 "%s:          probable X server bug in Xinerama/RANDR!\n",
                 blurb(), monitors[0]->width, monitors[0]->height,
                 monitors[0]->width / (double) monitors[0]->height,
                 blurb());
    }
}


/* Synchronize the contents of si->ssi to the current state of the monitors.
   Doesn't change anything if nothing has changed; otherwise, alters and
   reuses existing saver_screen_info structs as much as possible.
   Returns True if anything changed.
 */
Bool
update_screen_layout (saver_info *si)
{
  monitor **monitors = scan_monitors (si);
  int count = 0;
  int good_count = 0;
  int i, j;
  int seen_screens[100] = { 0, };

  if (! layouts_differ_p (monitors, si->monitor_layout))
    {
      free_monitors (monitors);
      return False;
    }

  free_monitors (si->monitor_layout);
  si->monitor_layout = monitors;
  check_monitor_sanity (si->monitor_layout);

  while (monitors[count])
    {
      if (monitors[count]->sanity == S_SANE)
        good_count++;
      count++;
    }

  if (si->ssi_count == 0)
    {
      si->ssi_count = 10;
      si->screens = (saver_screen_info *)
        calloc (sizeof(*si->screens), si->ssi_count);
    }

  if (si->ssi_count <= good_count)
    {
      si->ssi_count = good_count + 10;
      si->screens = (saver_screen_info *)
        realloc (si->screens, sizeof(*si->screens) * si->ssi_count);
      memset (si->screens + si->nscreens, 0, 
              sizeof(*si->screens) * (si->ssi_count - si->nscreens));
    }

  if (! si->screens) abort();

  si->nscreens = good_count;

  /* Regenerate the list of GL visuals as needed. */
  if (si->best_gl_visuals)
    free (si->best_gl_visuals);
  si->best_gl_visuals = 0;

  for (i = 0, j = 0; i < count; i++)
    {
      monitor *m = monitors[i];
      saver_screen_info *ssi = &si->screens[j];
      Screen *old_screen = ssi->screen;
      int sn;
      if (monitors[i]->sanity != S_SANE) continue;

      ssi->global = si;
      ssi->number = j;

      sn = screen_number (m->screen);
      ssi->screen = m->screen;
      ssi->real_screen_number = sn;
      ssi->real_screen_p = (seen_screens[sn] == 0);
      seen_screens[sn]++;

      ssi->default_visual =
	get_visual_resource (ssi->screen, "visualID", "VisualID", False);
      ssi->current_visual = ssi->default_visual;
      ssi->current_depth = visual_depth (ssi->screen, ssi->current_visual);

      /* If the screen changed (or if this is the first time) we need
         a new toplevel shell for this screen's depth.
       */
      if (ssi->screen != old_screen)
        initialize_screen_root_widget (ssi);

      ssi->last_poll_mouse.root_x = -1;
      ssi->last_poll_mouse.root_y = -1;

      ssi->x      = m->x;
      ssi->y      = m->y;
      ssi->width  = m->width;
      ssi->height = m->height;

# ifndef DEBUG_MULTISCREEN
      {
        saver_preferences *p = &si->prefs;
        if (p->debug_p
#  ifdef QUAD_MODE
            && !p->quad_p
#  endif
            )
          ssi->width /= 2;
      }
# endif

      j++;
    }

  si->default_screen = &si->screens[0];
  return True;
}
