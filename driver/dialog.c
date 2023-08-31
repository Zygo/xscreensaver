/* dialog.c --- the password dialog and splash screen.
 * xscreensaver, Copyright © 1993-2023 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */


/* This file renders the unlock dialog and splash screen, using Xlib and Xft.
 * One significant complication is that it must read raw XInput2 events to
 * get keyboard and mouse input, as the "xscreensaver" process has the mouse
 * and keyboard grabbed while this is running.
 *
 * It might be possible to implement this file using Gtk instead of Xlib,
 * but the grab situation might make that tricky: those events would have to
 * be re-sent to the toolkit widgets in a way that it would understand them.
 * Also, toolkits tend to assume that a window manager exists, and this
 * window must be an OverrideRedirect window with no focus management.
 *
 * Crashes here are interpreted as "unauthorized" and do not unlock the
 * screen.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <pwd.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_UNAME
# include <sys/utsname.h>
#endif /* HAVE_UNAME */
#include <ctype.h>
#include <pwd.h>

#ifndef HAVE_XINPUT
# error The XInput2 extension is required
#endif

#include <X11/Xproto.h>		/* for CARD32 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>
#include <X11/Intrinsic.h>

#ifdef ENABLE_NLS
# include <locale.h>
# include <libintl.h>
# define _(S) gettext(S)
#else
# define _(S) (S)
#endif

#ifdef HAVE_XKB
# include <X11/XKBlib.h>
# include <X11/extensions/XKB.h>
#endif

#include "version.h"
#include "blurb.h"
#include "auth.h"
#include "atoms.h"
#include "screens.h"
#include "xft.h"
#include "xftwrap.h"
#include "xinput.h"
#include "resources.h"
#include "visual.h"
#include "font-retry.h"
#include "prefs.h"
#include "usleep.h"
#include "utf8wc.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

extern Bool debug_p;

#undef DEBUG_METRICS
#undef DEBUG_STACKING

#define LOCK_FAILURE_ATOM "_XSCREENSAVER_AUTH_FAILURES"

#undef MAX
#undef MIN
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

#define MAX_BYTES_PER_CHAR 8	/* UTF-8 uses up to 6 bytes */
#define MAX_PASSWD_CHARS   280	/* Longest possible passphrase */

typedef struct window_state window_state;


typedef enum {
  AUTH_READ,		/* reading input or ready to do so */
  AUTH_SUCCESS,		/* auth success, unlock */
  AUTH_FAIL,		/* auth fail */
  AUTH_CANCEL,		/* user canceled, or typed blank password */
  AUTH_TIME,		/* timed out */
  AUTH_FINISHED,	/* user pressed enter */
  AUTH_NOTIFY		/* displaying message after finished */
} auth_state;


/* A mini-toolkit for rendering text labels, input fields, and buttons.
 */
typedef enum { CENTER, LEFT, RIGHT } line_align;

typedef struct {
  Bool down_p;
  XRectangle rect;
  char *cmd;
  void (*fn) (window_state *ws);
  Bool disabled_p;
} line_button_state;


typedef struct {
  char *text;
  XftFont *font;
  XftColor fg, fg2;
  Pixel bg;
  enum { LABEL, BUTTON, TEXT, TEXT_RO } type;
  line_align align;
  Bool float_p;
  Bool i_beam;
  line_button_state *button;
} dialog_line;


/* Global state.
 */
struct window_state {
  XtAppContext app;
  Display *dpy;
  Screen *screen;
  Position cx, cy, x, y;
  Dimension min_height;
  Window window;
  Colormap cmap;

  int splash_p;
  auth_state auth_state;
  int xi_opcode;
  int xkb_opcode;

  /* Variant strings
   */
  char *version;
  char *user;
  int nmsgs;
  const auth_message *msgs;

  /* "Characters" in the password may be a variable number of bytes long.
     plaintext_passwd contains the raw bytes.
     plaintext_passwd_char_size indicates the size in bytes of each character,
     so that we can make backspace work.
     censored_passwd is the asterisk version.

     Maybe it would be more sensible to use uint32_t and utils/utf8wc.c here,
     but the multi-byte string returned by XLookupString might not be UTF-8
     (see comment in handle_keypress).
   */
  char plaintext_passwd [MAX_PASSWD_CHARS * MAX_BYTES_PER_CHAR];
  char censored_passwd  [MAX_PASSWD_CHARS * MAX_BYTES_PER_CHAR];
  char plaintext_passwd_char_size [MAX_PASSWD_CHARS];

  XComposeStatus compose_status;
  
  XtIntervalId timer;

  XtIntervalId cursor_timer;		/* Blink the I-beam */
  XtIntervalId bs_timer;		/* Auto-repeat Backspace */
  int i_beam;

  double start_time, end_time;
  int passwd_timeout;

  Bool show_stars_p; /* "I regret that I have but one asterisk for my country."
                        -- Nathan Hale, 1776. */
  Bool caps_p;		 /* Whether we saw a keypress with caps-lock on */

  char *dialog_theme;
  char *heading_label;
  char *body_label;
  char *hostname_label;
  char *date_format;
  char *kbd_layout_label;
  char *newlogin_cmd;
  const char *asterisk_utf8;

  /* Resources for fonts and colors */
  XftDraw *xftdraw;
  XftFont *heading_font;
  XftFont *body_font;
  XftFont *error_font;
  XftFont *label_font;
  XftFont *date_font;
  XftFont *button_font;
  XftFont *hostname_font;

  Pixel foreground;
  Pixel background;
  XftColor xft_foreground;
  XftColor xft_text_foreground;
  XftColor xft_button_foreground;
  XftColor xft_button_disabled;
  XftColor xft_error_foreground;
  Pixel passwd_background;
  Pixel thermo_foreground;
  Pixel thermo_background;
  Pixel shadow_top;
  Pixel shadow_bottom;
  Pixel border_color;
  Pixel button_background;
  Pixel logo_background;

  Dimension preferred_logo_width;
  Dimension preferred_logo_height;
  Dimension thermo_width;
  Dimension internal_padding;
  Dimension shadow_width;
  Dimension border_width;

  Pixmap logo_pixmap;
  Pixmap logo_clipmask;
  unsigned int logo_width, logo_height;
  int logo_npixels;
  unsigned long *logo_pixels;

  line_button_state newlogin_button_state;
  line_button_state unlock_button_state;
  line_button_state demo_button_state;
  line_button_state help_button_state;
};


static void
draw_shaded_rectangle (Display *dpy, Window window,
		       int x, int y,
		       int width, int height,
		       int thickness,
		       unsigned long top_color,
		       unsigned long bottom_color)
{
  XPoint points[4];
  XGCValues gcv;
  GC gc1, gc2;
  if (thickness == 0) return;

  gcv.foreground = top_color;
  gc1 = XCreateGC (dpy, window, GCForeground, &gcv);
  gcv.foreground = bottom_color;
  gc2 = XCreateGC (dpy, window, GCForeground, &gcv);

  points [0].x = x;
  points [0].y = y;
  points [1].x = x + width;
  points [1].y = y;
  points [2].x = x + width - thickness;
  points [2].y = y + thickness;
  points [3].x = x;
  points [3].y = y + thickness;
  XFillPolygon (dpy, window, gc1, points, 4, Convex, CoordModeOrigin);

  points [0].x = x;
  points [0].y = y + thickness;
  points [1].x = x;
  points [1].y = y + height;
  points [2].x = x + thickness;
  points [2].y = y + height - thickness;
  points [3].x = x + thickness;
  points [3].y = y + thickness;
  XFillPolygon (dpy, window, gc1, points, 4, Convex, CoordModeOrigin);

  points [0].x = x + width;
  points [0].y = y;
  points [1].x = x + width - thickness;
  points [1].y = y + thickness;
  points [2].x = x + width - thickness;
  points [2].y = y + height - thickness;
  points [3].x = x + width;
  points [3].y = y + height - thickness;
  XFillPolygon (dpy, window, gc2, points, 4, Convex, CoordModeOrigin);

  points [0].x = x;
  points [0].y = y + height;
  points [1].x = x + width;
  points [1].y = y + height;
  points [2].x = x + width;
  points [2].y = y + height - thickness;
  points [3].x = x + thickness;
  points [3].y = y + height - thickness;
  XFillPolygon (dpy, window, gc2, points, 4, Convex, CoordModeOrigin);

  XFreeGC (dpy, gc1);
  XFreeGC (dpy, gc2);
}

#define IBEAM_WIDTH 2

static void
draw_i_beam (Display *dpy, Drawable d, Pixel color, int x, int y, int height)
{
  XGCValues gcv;
  GC gc;
  gcv.foreground = color;
  gcv.line_width = IBEAM_WIDTH;
  gc = XCreateGC (dpy, d, GCForeground | GCLineWidth, &gcv);
  XDrawLine (dpy, d, gc, x, y, x, y + height);  /* Ceci n'est pas une pipe */
  XFreeGC (dpy, gc);
}


static int
draw_dialog_line (window_state *ws, Drawable d, dialog_line *line,
                  int left, int right, int y, Bool clear_p)
{
  int w = right - left;
  int h;
  int xpad = 0, ypad = 0;
  XGlyphInfo overall;
  line_align align = line->align;
  int oleft  = left;
  int tleft  = left;
  int oright = right;
  int clip_w = 0;
  int gutter = 0;
  XRectangle rect;
  int xoff2 = 0;
  int yoff2 = 0;
  char *text2 = 0;
  char *text = line->text;
  int nlines = 1;

  /* Adjust left/right margins based on the type of the line.
   */
  switch (line->type) {
  case LABEL:
    if (line->float_p && line->align == LEFT)
      {
        /* Add 1px to leave a little padding between the top border of the
           label and the ascenders. */
        ypad = ws->shadow_width + 1;
        right = left + w/2 - ws->shadow_width * 2 - line->font->ascent / 2;
        align = RIGHT;
      }

    if (*line->text)
      text = text2 = xft_word_wrap (ws->dpy, line->font, line->text,
                                    right - left);
    break;

  case BUTTON:			/* box is fixed width at 1/3, text centered */
    align = CENTER;
    xpad = 0;
    /* make the buttons a little taller than everything else */
    /* Add 1px as above */
    ypad = ws->shadow_width + line->font->ascent / 2 + 1;
    gutter = ws->shadow_width;
    clear_p = True;

    switch (line->align) {
    case LEFT:
      right = left + w/3 - xpad;
      break;
    case CENTER:
      xpad = ws->shadow_width * 2;
      left  += w/3 + xpad;
      right -= w/3 + xpad;
      break;
    case RIGHT:
      left  = right - w/3 + xpad;
      break;
    }
    oright = right;
    xpad = 0;
    break;

  case TEXT:			/* box is fixed width at 1/2, text left */
  case TEXT_RO:
    align = LEFT;
    oleft = left + xoff2;
    clear_p = True;
    xpad = ws->shadow_width + line->font->ascent / 4;
    /* Add 1px as above */
    ypad = ws->shadow_width + 1;
    gutter = ws->shadow_width;
    if (gutter < 2) gutter = 2;

    switch (line->align) {
    case LEFT:
      right = left + w/2;
      break;
    case RIGHT:
      left  = right - w/2;
      break;
    case CENTER:
      abort();
      break;
    }

    /* If the text is longer than the field, scroll horizontally to show
       the end of the text instead of the beginning.
     */
    XftTextExtentsUtf8_multi (ws->dpy, line->font, (FcChar8 *) text,
                              strlen(text), &overall);
    if (overall.width >= w/2 - ws->shadow_width * 2 - IBEAM_WIDTH)
      {
        align = RIGHT;
        left = right - w/2;
      }
    break;

  default: abort(); break;
  }

  /* Clear out the area we're about to overwrite.
   */
  h = nlines * (line->font->ascent + line->font->descent) + ypad*2;
  if (clear_p)
    {
      GC gc;
      XGCValues gcv;
      gcv.foreground = line->bg;
      gc = XCreateGC (ws->dpy, d, GCForeground, &gcv);
      XFillRectangle (ws->dpy, d, gc, left, y, oright-left, h);
      XFreeGC (ws->dpy, gc);
    }

  /* Draw borders if necessary.
   */
  switch (line->type) {
  case LABEL: break;
  case BUTTON: case TEXT: case TEXT_RO:
    {
      Bool in_p = (line->type != BUTTON);
      if (line->button)
        {
          line->button->rect.x = left;
          line->button->rect.y = y;
          line->button->rect.width = right-left;
          line->button->rect.height = h;
          in_p = line->button->down_p || line->button->disabled_p;
        }
      tleft = left;
      draw_shaded_rectangle (ws->dpy, d,
                             left, y, right-left, h,
                             ws->shadow_width,
                             (in_p ? ws->shadow_bottom : ws->shadow_top),
                             (in_p ? ws->shadow_top : ws->shadow_bottom));
      clip_w = ws->shadow_width;
    }
    break;
  default: abort(); break;
  }

  /* Draw the text inside our box.
   */
  nlines = XftTextExtentsUtf8_multi (ws->dpy, line->font, (FcChar8 *) text,
                                     strlen(text), &overall);
  w = overall.width - overall.x;
  switch (align) {
  case LEFT:   left = left + xpad; break;
  case RIGHT:  left = right - w - xpad; break;
  case CENTER:
    oleft = left;
    left = left + xpad + (right - left - w) / 2;
    if (left < oleft) left = oleft;
    break;
  default: abort(); break;
  }

  rect.x = MAX (oleft, MAX (left, tleft + clip_w));
  rect.width = MIN (oright, right) - rect.x - clip_w;
  rect.y = y + ypad - overall.y + line->font->ascent;
  rect.height = overall.height;

  XftDrawSetClipRectangles (ws->xftdraw, 0, 0, &rect, 1);

  if (line->type == BUTTON &&
      line->button &&
      line->button->down_p)
    xoff2 = yoff2 = MIN (ws->shadow_width, line->font->ascent/2);

  XftDrawStringUtf8_multi (ws->xftdraw, 
                           (line->button && line->button->disabled_p
                            ? &line->fg2 : &line->fg),
                           line->font,
                           left + xoff2,
                           y + ypad + yoff2 + line->font->ascent,
                           (FcChar8 *) text, strlen (text),
                           (align == LEFT ? 1 : align == CENTER ? 0 : -1));
# ifdef DEBUG_METRICS
  {
    GC gc;
    XGCValues gcv;
    int yy = y + ypad + yoff2 + line->font->ascent;
    gcv.foreground = line->fg.pixel;
    gc = XCreateGC (ws->dpy, d, GCForeground, &gcv);
    /* draw a line on the baseline of the text */
    XDrawLine (ws->dpy, d, gc, 0, yy, right, yy);
    yy -= line->font->ascent;
    /* a line above the ascenders */
    XDrawLine (ws->dpy, d, gc, left, yy, right, yy);
    yy += line->font->ascent + line->font->descent;
    /* and below the descenders */
    XDrawLine (ws->dpy, d, gc, left, yy, right, yy);
    XFreeGC (ws->dpy, gc);
  }
# endif

  if (line->i_beam)
    draw_i_beam (ws->dpy, d,
                 ws->foreground,
                 left + xoff2 + overall.width,
                 y + ypad + yoff2,
                 line->font->ascent + line->font->descent);

  XftDrawSetClip (ws->xftdraw, 0);

  if (text2) free (text2);

  y += ypad*2 + (nlines * (line->font->ascent + line->font->descent)) + gutter;
  return y;
}


static int
draw_dialog_lines (window_state *sp, Drawable d, dialog_line *lines,
                   int left, int right, int top)
{
  int i;
  int maxy = 0;
  for (i = 0; lines[i].text; i++)
    {
      Bool clear_p = (i > 0 && lines[i-1].float_p ? False : True);
      int y = draw_dialog_line (sp, d, &lines[i], left, right, top, clear_p);
      if (y > maxy) maxy = y;
      if (! lines[i].float_p)
        top = maxy;
    }
  return top;
}


static pid_t
fork_and_exec (Display *dpy, int argc, char **argv)
{
  char buf [255];
  pid_t forked = fork();
  switch ((int) forked) {
  case -1:
    sprintf (buf, "%s: couldn't fork", blurb());
    perror (buf);
    break;

  case 0:
    close (ConnectionNumber (dpy));	/* close display fd */
    execvp (argv[0], argv);		/* shouldn't return. */

    sprintf (buf, "%s: pid %lu: couldn't exec %s", blurb(),
             (unsigned long) getpid(), argv[0]);
    perror (buf);
    exit (1);				/* exits child fork */
    break;

  default:				/* parent fork */
    if (verbose_p)
      {
        int i;
        fprintf (stderr, "%s: pid %lu: launched",
                 blurb(), (unsigned long) forked);
        for (i = 0; i < argc; i++)
          fprintf (stderr, " %s", argv[i]);
        fprintf (stderr, "\n");
      }
    break;
  }

  return forked;
}


/* Loading resources
 */
static void
resource_keys (window_state *ws, const char **name, const char **rclass)
{
  const char *theme = ws->dialog_theme;
  const char *name2  = (ws->splash_p ? "splash" : "passwd");
  const char *class2 = "Dialog";
  static char res[200], rclass2[200];
  char *s;

  /* First try $THEME."Dialog.value" */
  sprintf (res,     "%s.%s.%s", theme, name2,  *name);
  sprintf (rclass2, "%s.%s.%s", theme, class2, *rclass);
  s = get_string_resource (ws->dpy, res, rclass2);
  if (s && *s) goto DONE;

  /* Next try "default.Dialog.value" */
  if (s) free (s);
  theme = "default";
  sprintf (res,     "%s.%s.%s", theme, name2,  *name);
  sprintf (rclass2, "%s.%s.%s", theme, class2, *rclass);
  s = get_string_resource (ws->dpy, res, rclass2);
  if (s && *s) goto DONE;

  /* Next try "Dialog.value" */
  if (s) free (s);
  sprintf (res,     "%s.%s", theme,  *name);
  sprintf (rclass2, "%s.%s", theme, *rclass);
  s = get_string_resource (ws->dpy, res, rclass2);
  if (s && *s) goto DONE;

 DONE:
  *name   = res;
  *rclass = rclass2;
  if (s) free (s);
}


static char *
get_str (window_state *ws, const char *name, const char *rclass)
{
  resource_keys (ws, &name, &rclass);
  return get_string_resource (ws->dpy, (char *) name, (char *) rclass);
}


static XftFont *
get_font (window_state *ws, const char *name)
{
  const char *rclass = "Font";
  XftFont *f;
  char *s;
  resource_keys (ws, &name, &rclass);
  s = get_string_resource (ws->dpy, (char *) name, (char *) rclass);
  if (!s || !*s)
    s = "sans-serif 14";
  f = load_xft_font_retry (ws->dpy, DefaultScreen(ws->dpy), s);
  if (!f) abort();
  return f;
}

static unsigned long
get_color (window_state *ws, const char *name, const char *rclass)
{
  resource_keys (ws, &name, &rclass);
  return get_pixel_resource (ws->dpy, DefaultColormapOfScreen (ws->screen),
                             (char *) name, (char *) rclass);
}

static void
get_xft_color (window_state *ws, XftColor *ret, 
               const char *name, const char *rclass)
{
  char *s;
  resource_keys (ws, &name, &rclass);
  s = get_string_resource (ws->dpy, (char *) name, (char *) rclass);
  if (!s || !*s) s = "black";
  XftColorAllocName (ws->dpy,
                     DefaultVisualOfScreen(ws->screen),
                     DefaultColormapOfScreen (ws->screen),
                     s, ret);
}

static void
dim_xft_color (window_state *ws, const XftColor *in, Pixel bg, XftColor *out)
{
  double dim = 0.6;
# if 0  /* Turns out Xft alpha doesn't work. How very. */
  XRenderColor rc = in->color;
  rc.alpha *= dim;
# else
  XRenderColor rc;
  XColor xc;
  xc.pixel = bg;
  XQueryColor (ws->dpy, DefaultColormapOfScreen (ws->screen), &xc);
  rc.red   = dim * in->color.red   + (1-dim) * xc.red;
  rc.green = dim * in->color.green + (1-dim) * xc.green;
  rc.blue  = dim * in->color.blue  + (1-dim) * xc.blue;
  rc.alpha = in->color.alpha;
# endif
  if (! XftColorAllocValue (ws->dpy,
                            DefaultVisualOfScreen(ws->screen),
                            DefaultColormapOfScreen (ws->screen),
                            &rc, out))
    abort();
}


static int
get_int (window_state *ws, const char *name, const char *rclass)
{
  resource_keys (ws, &name, &rclass);
  return get_integer_resource (ws->dpy, (char *) name, (char *) rclass);
}


static const char *
choose_asterisk (window_state *ws)
{
  static char picked[8];
  const unsigned long candidates[] = { 0x25CF,  /* Black Circle */
                                       0x2022,  /* Bullet */
                                       0x2731,  /* Heavy Asterisk */
                                       '*' };   /* Ἀστερίσκος */
  const unsigned long *uc = candidates;
  int i, L;
  for (i = 0; i < countof (candidates) - 1; i++)
    {
# ifdef HAVE_XFT
      if (XftCharExists (ws->dpy, ws->label_font, (FcChar32) *uc))
        break;
      if (debug_p)
        fprintf (stderr, "%s: char U+%0lX does not exist\n", blurb(), *uc);
# endif
      uc++;
    }

  L = utf8_encode (*uc, picked, sizeof (picked) - 1);
  picked[L] = 0;

  return picked;
}


/* Decide where on the X11 screen to place the dialog.
   This is complicated because, in the face of RANDR and Xinerama, we want
   to center it on a *monitor*, not on what X calls a 'Screen'.  So get the
   monitor state, then figure out which one of those the mouse is in.
 */
static void
splash_pick_window_position (Display *dpy, Position *xP, Position *yP,
                             Screen **screenP)
{
  Screen *mouse_screen = DefaultScreenOfDisplay (dpy);
  int root_x = 0, root_y = 0;
  int nscreens = ScreenCount (dpy);
  int i, screen;
  monitor **monitors;
  monitor *m = 0;

  /* Find the mouse screen, and position on it. */
  for (screen = 0; screen < nscreens; screen++)
    {
      Window pointer_root, pointer_child;
      int win_x, win_y;
      unsigned int mask;
      int status = XQueryPointer (dpy, RootWindow (dpy, screen),
                                  &pointer_root, &pointer_child,
                                  &root_x, &root_y, &win_x, &win_y, &mask);
      if (status != None)
        {
          mouse_screen = ScreenOfDisplay (dpy, screen);
          break;
        }
    }

  monitors = scan_monitors (dpy);  /* This scans all Screens */
  if (!monitors || !*monitors) abort();

  for (i = 0; monitors[i]; i++)
    {
      monitor *m0 = monitors[i];
      if (m0->sanity == S_SANE &&
          mouse_screen == m0->screen &&
          root_x >= m0->x &&
          root_y >= m0->y &&
          root_x < m0->x + m0->width &&
          root_y < m0->y + m0->height)
        {
          m = m0;
          break;
        }
    }

  if (!m)
    {
      if (verbose_p)
        fprintf (stderr, "%s: mouse is not on any monitor?\n", blurb());
      m = monitors[0];
    }
  else if (verbose_p)
    fprintf (stderr,
             "%s: mouse is at %d,%d on monitor %d %dx%d+%d+%d \"%s\"\n",
             blurb(), root_x, root_y, m->id,
             m->width, m->height, m->x, m->y,
             (m->desc ? m->desc : ""));

  *xP = m->x + m->width/2;
  *yP = m->y + m->height/2;
  *screenP = mouse_screen;
  
  free_monitors (monitors);
}


static void unlock_cb (window_state *ws);


/* This program only needs a few options from .xscreensaver.
   Read that file directly and store those into the Xrm database.
 */
static void init_line_handler (int lineno,
                               const char *key, const char *val,
                               void *closure)
{
  window_state *ws = (window_state *) closure;
  if (!val || !*val)
    ;
  else if (!strcmp (key, "dialogTheme") ||
           !strcmp (key, "passwdTimeout"))
    {
      XrmDatabase db = XtDatabase (ws->dpy);
      char *key2 = (char *) malloc (strlen (progname) + strlen (val) + 10);
      sprintf (key2, "%s.%s", progname, key);
      XrmPutStringResource (&db, key2, val);
      free (key2);
    }
  /* We read additional resources, such as "PROGCLASS.THEME.Dialog.foreground",
     but those are from the .ad file only, not from .xscreensaver, so they
     don't need a clause here in the file parser.  They have already been
     read into the DB by Xt's Xrm initialization. */
}


static void
read_init_file_simple (window_state *ws)
{
  const char *home = getenv("HOME");
  char *fn;
  if (!home || !*home) return;
  fn = (char *) malloc (strlen(home) + 40);
  sprintf (fn, "%s/.xscreensaver", home);
  if (debug_p)
    fprintf (stderr, "%s: reading %s\n", blurb(), fn);
  parse_init_file (fn, init_line_handler, ws);
  free (fn);
}


static void
grab_keyboard_and_mouse (window_state *ws)
{
  /* If we have been launched by xscreensaver, these grabs won't succeed,
     and that is expected.  But if we are being run manually for debugging,
     they are necessary to avoid having events seen by two apps at once.
     (We don't bother to ungrab, that happens when we exit.)
   */
  Display *dpy = ws->dpy;
  Window root = RootWindowOfScreen (ws->screen);
  XGrabKeyboard (dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
  XGrabPointer (dpy, root, True, 
                (ButtonPressMask   | ButtonReleaseMask |
                 EnterWindowMask   | LeaveWindowMask |
                 PointerMotionMask | PointerMotionHintMask |
                 Button1MotionMask | Button2MotionMask |
                 Button3MotionMask | Button4MotionMask |
                 Button5MotionMask | ButtonMotionMask),
                GrabModeAsync, GrabModeAsync, root,
                None, CurrentTime);
}


static void
get_keyboard_layout (window_state *ws)
{
# ifdef HAVE_XKB
  XkbStateRec state;
  XkbDescPtr desc = 0;
  Atom name = 0;
  char *namestr = 0;

  if (! ws->xkb_opcode)
    {
      if (! XkbQueryExtension (ws->dpy, 0, &ws->xkb_opcode, 0, 0, 0))
        {
          ws->xkb_opcode = -1;  /* Only try once */
          if (verbose_p)
            fprintf (stderr, "%s: XkbQueryExtension failed\n", blurb());
          return;
        }

      if (! XkbSelectEvents (ws->dpy, XkbUseCoreKbd,
                             XkbMapNotifyMask | XkbStateNotifyMask,
                             XkbMapNotifyMask | XkbStateNotifyMask))
        {
          if (verbose_p)
            fprintf (stderr, "%s: XkbSelectEvents failed\n", blurb());
        }
    }

  if (XkbGetState (ws->dpy, XkbUseCoreKbd, &state))
    {
      if (verbose_p)
        fprintf (stderr, "%s: XkbGetState failed\n", blurb());
      return;
    }
  desc = XkbGetKeyboard (ws->dpy, XkbAllComponentsMask, XkbUseCoreKbd);
  if (!desc || !desc->names)
    {
      if (verbose_p)
        fprintf (stderr, "%s: XkbGetKeyboard failed\n", blurb());
      goto DONE;
    }
  name = desc->names->groups[state.group];
  namestr = (name ? XGetAtomName (ws->dpy, name) : 0);
  if (!namestr)
    {
      if (verbose_p)
        fprintf (stderr, "%s: XkbGetKeyboard returned null layout\n", blurb());
      goto DONE;
    }

  if (ws->kbd_layout_label)
    free (ws->kbd_layout_label);
  ws->kbd_layout_label = namestr;

  if (verbose_p)
    fprintf (stderr, "%s: kbd layout: %s\n", blurb(),
             namestr ? namestr : "null");

 DONE:
  if (desc) XFree (desc);
# endif /* HAVE_XKB */
}


static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}


static void
create_window (window_state *ws, int w, int h)
{
  XSetWindowAttributes attrs;
  unsigned long attrmask;
  Window ow = ws->window;

  attrmask = CWOverrideRedirect | CWEventMask;
  attrs.override_redirect = True;
  attrs.event_mask = ExposureMask | VisibilityChangeMask;
  ws->window = XCreateWindow (ws->dpy,
                              RootWindowOfScreen(ws->screen),
                              ws->x, ws->y, w, h, 0,
                              DefaultDepthOfScreen (ws->screen),
                              InputOutput,
                              DefaultVisualOfScreen(ws->screen),
                              attrmask, &attrs);
  XSetWindowBackground (ws->dpy, ws->window, ws->background);
  XSetWindowColormap (ws->dpy, ws->window, ws->cmap);
  xscreensaver_set_wm_atoms (ws->dpy, ws->window, w, h, 0);

  if (ow)
    {
      XMapRaised (ws->dpy, ws->window);
      XDestroyWindow (ws->dpy, ow);
    }
}


/* Loads resources and creates and returns the global window state.
 */
static window_state *
window_init (Widget root_widget, int splash_p)
{
  Display *dpy = XtDisplay (root_widget);
  window_state *ws;
  Bool resource_error_p = False;

  ws = (window_state *) calloc (1, sizeof(*ws));
  if (!ws) abort();

  ws->splash_p = splash_p;
  ws->dpy = dpy;
  ws->app = XtWidgetToApplicationContext (root_widget);

  splash_pick_window_position (ws->dpy, &ws->cx, &ws->cy, &ws->screen);

  ws->cmap = XCreateColormap (dpy,
                              RootWindowOfScreen (ws->screen), /* Old skool */
                              DefaultVisualOfScreen (ws->screen),
                              AllocNone);

  {
    struct passwd *p = getpwuid (getuid());
    if (!p || !p->pw_name || !*p->pw_name) abort();
    ws->user = p->pw_name;
  }

  /* Read resources and .xscreensaver file settings.
   */
  read_init_file_simple (ws);

  ws->dialog_theme =  /* must be first */
    get_string_resource (ws->dpy, "dialogTheme", "DialogTheme");
  if (!ws->dialog_theme || !*ws->dialog_theme)
    ws->dialog_theme = strdup ("default");
  if (verbose_p)
    fprintf (stderr, "%s: theme: %s\n", blurb(), ws->dialog_theme);

  ws->newlogin_cmd = get_str (ws, "newLoginCommand", "NewLoginCommand");
  ws->date_format = get_str (ws, "dateFormat", "DateFormat"); 
  ws->show_stars_p =
    get_boolean_resource (ws->dpy, "passwd.asterisks", "Passwd.Boolean");

  ws->passwd_timeout = get_seconds_resource (ws->dpy, "passwdTimeout", "Time");
  if (ws->passwd_timeout <= 5) ws->passwd_timeout = 5;

  /* Put the version number in the label. */
  {
    char *version = strdup (screensaver_id + 17);
    char *year = strchr (version, '-');
    char *s = strchr (version, ' ');
    *s = 0;
    year = strchr (year+1, '-') + 1;
    s = strchr (year, ')');
    *s = 0;
    ws->heading_label = (char *) malloc (100);
    ws->version = strdup(version);
    sprintf (ws->heading_label, "XScreenSaver %.4s, v%.10s", year, version);

    if (splash_p)
      {
        ws->body_label = (char *) malloc (100);
        sprintf (ws->body_label,
           _("Copyright \xC2\xA9 1991-%.4s by\nJamie Zawinski <jwz@jwz.org>"),
                  year);
      }
  }

  ws->heading_font  = get_font (ws, "headingFont");
  ws->button_font   = get_font (ws, "buttonFont");
  ws->body_font     = get_font (ws, "bodyFont");
  ws->error_font    = get_font (ws, "errorFont");
  ws->label_font    = get_font (ws, "labelFont");
  ws->date_font     = get_font (ws, "dateFont");
  ws->hostname_font = get_font (ws, "unameFont");

  ws->asterisk_utf8 = choose_asterisk (ws);
  
  ws->foreground = get_color (ws, "foreground", "Foreground");
  ws->background = get_color (ws, "background", "Background");

  get_xft_color (ws, &ws->xft_foreground, "foreground", "Foreground");
  get_xft_color (ws, &ws->xft_text_foreground,
                 "text.foreground", "Text.Foreground");
  get_xft_color (ws, &ws->xft_error_foreground,
                 "error.foreground", "Error.Foreground");
  get_xft_color (ws, &ws->xft_button_foreground,
                 "button.foreground", "Button.Foreground");
  dim_xft_color (ws, &ws->xft_button_foreground, ws->background,
                 &ws->xft_button_disabled);

  ws->shadow_top    = get_color (ws, "topShadowColor", "Foreground");
  ws->shadow_bottom = get_color (ws, "bottomShadowColor", "Background");
  ws->border_color  = get_color (ws, "borderColor", "BorderColor");
  ws->passwd_background = get_color (ws, "text.background", "Text.Background");
  ws->button_background =
    get_color (ws, "button.background", "Button.Background");
  ws->thermo_foreground =
    get_color (ws, "thermometer.foreground", "Thermometer.Foreground");
  ws->thermo_background =
    get_color ( ws, "thermometer.background", "Thermometer.Background");
  ws->logo_background = get_color ( ws, "logo.background", "Logo.Background");

  if (resource_error_p)
    {
      /* Make sure the error messages show up. */
      ws->foreground = BlackPixelOfScreen (ws->screen);
      ws->background = WhitePixelOfScreen (ws->screen);
    }

  ws->preferred_logo_width  = get_int (ws, "logo.width",  "Logo.Width");
  ws->preferred_logo_height = get_int (ws, "logo.height", "Logo.Height");
  ws->thermo_width = get_int (ws, "thermometer.width", "Thermometer.Width");
  ws->shadow_width = get_int (ws, "shadowWidth", "ShadowWidth"); 
  ws->border_width = get_int (ws, "borderWidth", "BorderWidth"); 
  ws->internal_padding =
    get_int (ws, "internalPadding", "InternalPadding");

  if (ws->preferred_logo_width == 0)  ws->preferred_logo_width  = 150;
  if (ws->preferred_logo_height == 0) ws->preferred_logo_height = 150;
  if (ws->internal_padding == 0) ws->internal_padding = 15;
  if (ws->thermo_width == 0) ws->thermo_width = ws->shadow_width;

  if (ws->splash_p) ws->thermo_width = 0;

# ifdef HAVE_UNAME
  if (!splash_p &&
      get_boolean_resource (ws->dpy, "passwd.uname", "Passwd.Boolean"))
    {
      struct utsname uts;
      if (!uname (&uts) && *uts.nodename)
        ws->hostname_label = strdup (uts.nodename);
    }
# endif

  get_keyboard_layout (ws);

  /* Load the logo pixmap, based on font size */
  {
    int x, y;
    unsigned int bw, d;
    Window root = RootWindowOfScreen(ws->screen);
    Visual *visual = DefaultVisualOfScreen (ws->screen);
    int logo_size = (ws->heading_font->ascent > 24 ? 2 : 1);
    ws->logo_pixmap = xscreensaver_logo (ws->screen, visual, root, ws->cmap,
                                         ws->background, 
                                         &ws->logo_pixels, &ws->logo_npixels,
                                         &ws->logo_clipmask, logo_size);
    if (!ws->logo_pixmap) abort();
    XGetGeometry (dpy, ws->logo_pixmap, &root, &x, &y,
                  &ws->logo_width, &ws->logo_height, &bw, &d);
  }

  ws->x = ws->y = 0;
  create_window (ws, 1, 1);

  /* Select SubstructureNotifyMask on the root window so that we know
     when another process has mapped a window, so that we can make our
     window always be on top. */
  {
    Window root = RootWindowOfScreen (ws->screen);
    XWindowAttributes xgwa;
    XGetWindowAttributes (ws->dpy, root, &xgwa);
    XSelectInput (ws->dpy, root, 
                  xgwa.your_event_mask | SubstructureNotifyMask);
  }


  ws->newlogin_button_state.cmd = ws->newlogin_cmd;
  ws->demo_button_state.cmd =
    get_string_resource (ws->dpy, "demoCommand", "Command");
  {
    char *load = get_string_resource (ws->dpy, "loadURL", "Command");
    char *url  = get_string_resource (ws->dpy, "helpURL", "URL");
    if (load && *load && url && *url)
      {
        char *cmd = (char *) malloc (strlen(load) + (strlen(url) * 5) + 10);
        sprintf (cmd, load, url, url, url, url, url);
        ws->help_button_state.cmd = cmd;         
      }
  }

  ws->unlock_button_state.fn = unlock_cb;

  grab_keyboard_and_mouse (ws);

  return ws;
}


#ifdef DEBUG_STACKING
static void
describe_window (Display *dpy, Window w)
{
  XClassHint ch;
  char *name = 0;
  if (XGetClassHint (dpy, w, &ch))
    {
      fprintf (stderr, "0x%lx \"%s\", \"%s\"\n", (unsigned long) w,
               ch.res_class, ch.res_name);
      XFree (ch.res_class);
      XFree (ch.res_name);
    }
  else if (XFetchName (dpy, w, &name) && name)
    {
      fprintf (stderr, "0x%lx \"%s\"\n", (unsigned long) w, name);
      XFree (name);
    }
  else
    {
      fprintf (stderr, "0x%lx (untitled)\n", (unsigned long) w);
    }
}
#endif /* DEBUG_STACKING */


/* Returns true if some other window is on top of this one.
 */
static Bool
window_occluded_p (Display *dpy, Window window)
{
  int screen;

# ifdef DEBUG_STACKING
  fprintf (stderr, "\n");
# endif

  for (screen = 0; screen < ScreenCount (dpy); screen++)
    {
      int i;
      Window root = RootWindow (dpy, screen);
      Window root2 = 0, parent = 0, *kids = 0;
      unsigned int nkids = 0;
      Bool saw_our_window_p = False;
      Bool saw_later_window_p = False;
      if (! XQueryTree (dpy, root, &root2, &parent, &kids, &nkids))
        {
# ifdef DEBUG_STACKING
          fprintf (stderr, "%s: XQueryTree failed\n", blurb());
# endif
        continue;
        }

      for (i = 0; i < nkids; i++)
        {
          if (kids[i] == window)
            {
              saw_our_window_p = True;
# ifdef DEBUG_STACKING
              fprintf (stderr, "%s: our window:    ", blurb());
              describe_window (dpy, kids[i]);
# endif
            }
          else if (saw_our_window_p)
            {
              saw_later_window_p = True;
# ifdef DEBUG_STACKING
              fprintf (stderr, "%s: higher window: ", blurb());
              describe_window (dpy, kids[i]);
# endif
              break;
            }
          else
            {
# ifdef DEBUG_STACKING
              fprintf (stderr, "%s: lower window:  ", blurb());
              describe_window (dpy, kids[i]);
# endif
            }
        }

      if (kids)
        XFree ((char *) kids);

      if (saw_later_window_p)
        return True;
      else if (saw_our_window_p)
        return False;
      /* else our window is not on this screen; keep going, try the next. */
    }

  /* Window doesn't exist? */
# ifdef DEBUG_STACKING
  fprintf (stderr, "%s: our window isn't on the screen\n", blurb());
# endif
  return False;
}


/* Strip leading and trailing whitespace. */
static char *
trim (const char *s)
{
  char *s2;
  int L;
  if (!s) return 0;
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
    s++;
  s2 = strdup (s);
  L = strlen (s2);
  while (L > 0 &&
         (s2[L-1] == ' ' || s2[L-1] == '\t' ||
          s2[L-1] == '\r' || s2[L-1] == '\n'))
    s2[--L] = 0;
  return s2;
}


/* Repaint the entire window.
 */
static void
window_draw (window_state *ws)
{
  Display *dpy = ws->dpy;
  Screen *screen = ws->screen;
  Window root = RootWindowOfScreen (screen);
  Visual *visual = DefaultVisualOfScreen(screen);
  int depth = DefaultDepthOfScreen (screen);
  XWindowAttributes xgwa;

# define MIN_COLUMNS 22   /* Set window width based on headingFont ascent. */

  int ext_border = (ws->internal_padding / 2 + 
                    ws->shadow_width + ws->border_width);

  Pixmap dbuf;
  unsigned int logo_frame_width, logo_frame_height;
  unsigned int window_width, window_height;
  unsigned int text_left, text_right;
  unsigned int thermo_x;
  unsigned int x, y;
  GC gc;
  XGCValues gcv;
  char date_text[100];
  time_t now = time ((time_t *) 0);
  struct tm *tm = localtime (&now);
  dialog_line *lines =
    (dialog_line *) calloc (ws->nmsgs + 40, sizeof(*lines));
  Bool emitted_user_p = False;
  int i = 0, j;

  XGetWindowAttributes (ws->dpy, ws->window, &xgwa);

  if (!lines) abort();

  strftime (date_text, sizeof(date_text)-2, ws->date_format, tm);

  logo_frame_width   = (ws->logo_width + ws->internal_padding * 2 +
                       ws->shadow_width * 2);
  logo_frame_height  = logo_frame_width;
  if (logo_frame_width < ws->preferred_logo_width)
    logo_frame_width = ws->preferred_logo_width;
  if (logo_frame_height < ws->preferred_logo_height)
    logo_frame_height = ws->preferred_logo_height;

  thermo_x = ext_border * 1.5 + logo_frame_width + ws->shadow_width;
  text_left = (thermo_x + ws->internal_padding +
               (ws->thermo_width
                ? ws->thermo_width + ws->shadow_width * 3
                : 0));
  text_right = text_left + ws->heading_font->ascent * MIN_COLUMNS;
  window_width = text_right + ws->internal_padding + ext_border;
  window_height = window_width * 3;  /* reduced later */

  dbuf = XCreatePixmap (dpy, root, window_width, window_height, depth);
  gc = XCreateGC (dpy, dbuf, 0, &gcv);
  XSetForeground (dpy, gc, ws->background);
  XFillRectangle (dpy, dbuf, gc, 0, 0, window_width, window_height);

  if (ws->xftdraw)
    XftDrawDestroy (ws->xftdraw);
  ws->xftdraw = XftDrawCreate (dpy, dbuf, visual, xgwa.colormap);

  lines[i].text  = ws->heading_label;			/* XScreenSaver */
  lines[i].font  = ws->heading_font;
  lines[i].fg    = ws->xft_foreground;
  lines[i].fg2   = lines[i].fg;
  lines[i].bg    = ws->background;
  lines[i].type  = LABEL;
  lines[i].align = CENTER;
  i++;

  /* If you are in here because you're planning on disabling this notice
     before redistributing my software, please don't.

     I sincerely request that you do one of the following:

         1: leave this code intact and this warning in place, -OR-

         2: Remove xscreensaver from your distribution.

     I would seriously prefer that you not distribute my software at all
     than that you distribute one version and then never update it for
     years.

     I am *constantly* getting email from users reporting bugs that have
     been fixed for literally years who have no idea that the software
     they are running is years out of date.  Yes, it would be great if we
     lived in the ideal world where people checked that they were running
     the latest release before they report a bug, but we don't.  To most
     people, "running the latest release" is synonymous with "running the
     latest release that my distro packages for me."

     When they even bother to tell me what version they're running, I
     say, "That version is three years old!", and they say "But this is
     the latest version my distro ships".  Then I say, "your distro
     sucks", and they say "but I don't know how to compile from source,
     herp derp I eat paste", and *everybody* goes away unhappy.

     It wastes an enormous amount of my time, but worse than that, it
     does a grave disservice to the users, who are stuck experiencing
     bugs that are already fixed!  These users think they are running the
     latest release, and they are not.  They would like to be running the
     actual latest release, but they don't know how, because their distro
     makes that very difficult for them.  It's terrible for everyone, and
     kind of makes me regret ever having released this software in the
     first place.

     So seriously. I ask that if you're planning on disabling this
     obsolescence warning, that you instead just remove xscreensaver from
     your distro entirely.  Everybody will be happier that way.  Check
     out gnome-screensaver instead, I understand it's really nice.

     Of course, my license allows you to ignore me and do whatever the
     fuck you want, but as the author, I hope you will have the common
     courtesy of complying with my request.

     Thank you!

     jwz, 2014, 2016, 2018, 2021.

     PS: In particular, since Debian refuses to upgrade software on any
     kind of rational timeline, I have asked that they stop shipping
     xscreensaver at all.  They have refused.  Instead of upgrading the
     software, they simply patched out this warning.

     If you want to witness the sad state of the open source peanut
     gallery, look no farther than the comments on my blog:
     http://jwz.org/b/yiYo

     Many of these people fall back on their go-to argument of, "If it is
     legal, it must be right."  If you believe in that rhetorical device
     then you are a terrible person, and possibly a sociopath.

     There are also the armchair lawyers who say "Well, instead of
     *asking* people to do the right thing out of common courtesy, you
     should just change your license to prohibit them from acting
     amorally."  Again, this is the answer of a sociopath, but that aside,
     if you devote even a second's thought to this you will realize that
     the end result of this would be for distros like Debian to just keep
     shipping the last version with the old license and then never
     upgrading it again -- which would be the worst possible outcome for
     everyone involved, most especially the users.

     Also, some have incorrectly characterized this as a "time bomb".
     It is a software update notification, nothing more.  A "time bomb"
     makes software stop working.  This merely alerts the user that the
     security-critical software that they are running is dangerously out
     of date.

     If you have read all of the above, and still decide to intentionally
     disrespect the wishes of the person who wrote all of this software for
     you -- you are a terrible person.  Kindly go fuck yourself.
  */
  if (time ((time_t *) 0) - XSCREENSAVER_RELEASED > 60*60*24*30*17)
    {
      lines[i].text  = _("Update available!\nThis version is very old.\n");
      lines[i].font  = ws->error_font;
      lines[i].fg    = ws->xft_error_foreground;
      lines[i].fg2   = lines[i].fg;
      lines[i].bg    = ws->background;
      lines[i].type  = LABEL;
      lines[i].align = CENTER;
      i++;
    }
  else if (strstr (ws->version, "a") || strstr (ws->version, "b"))
    {
      lines[i].text  = _("PRE-RELEASE VERSION");
      lines[i].font  = ws->error_font;
      lines[i].fg    = ws->xft_error_foreground;
      lines[i].fg2   = lines[i].fg;
      lines[i].bg    = ws->background;
      lines[i].type  = LABEL;
      lines[i].align = CENTER;
      i++;
    }

  if (ws->hostname_label && *ws->hostname_label)
    {
      lines[i].text  = ws->hostname_label;
      lines[i].font  = ws->hostname_font;
      lines[i].fg    = ws->xft_foreground;
      lines[i].fg2   = lines[i].fg;
      lines[i].bg    = ws->background;
      lines[i].type  = LABEL;
      lines[i].align = CENTER;
      i++;
    }

# define BLANK_LINE                    \
  lines[i].text  = "";                 \
  lines[i].font  = ws->body_font;      \
  lines[i].fg    = ws->xft_foreground; \
  lines[i].fg2   = lines[i].fg;        \
  lines[i].bg    = ws->background;     \
  lines[i].type  = LABEL;              \
  lines[i].align = CENTER;             \
  i++

  BLANK_LINE;

  if (debug_p && !ws->splash_p)
    {
      lines[i].text  =
        _("DEBUG MODE:\nAll keystrokes are being logged to stderr.\n");
      lines[i].font  = ws->error_font;
      lines[i].fg    = ws->xft_error_foreground;
      lines[i].fg2   = lines[i].fg;
      lines[i].bg    = ws->background;
      lines[i].type  = LABEL;
      lines[i].align = CENTER;
      i++;
    }

  if (ws->body_label && *ws->body_label)
    {
      lines[i].text  = ws->body_label;	    /* Copyright or error message */
      lines[i].font  = ws->body_font;
      lines[i].fg    = ws->xft_foreground;
      lines[i].fg2   = lines[i].fg;
      lines[i].bg    = ws->background;
      lines[i].type  = LABEL;
      lines[i].align = CENTER;
      i++;

      BLANK_LINE;
    }

  for (j = 0; j < ws->nmsgs; j++)			/* PAM msgs */
    {
      switch (ws->msgs[j].type) {
      case AUTH_MSGTYPE_INFO:
      case AUTH_MSGTYPE_ERROR:
        lines[i].text  = trim (ws->msgs[j].msg);
        lines[i].font  = (ws->msgs[j].type == AUTH_MSGTYPE_ERROR
                          ? ws->error_font
                          : ws->body_font);
        lines[i].fg    = (ws->msgs[j].type == AUTH_MSGTYPE_ERROR
                          ? ws->xft_error_foreground
                          : ws->xft_foreground);
        lines[i].fg2   = lines[i].fg;
        lines[i].bg    = ws->background;
        lines[i].type  = LABEL;
        lines[i].align = CENTER;
        i++;
        break;

      case AUTH_MSGTYPE_PROMPT_NOECHO:
      case AUTH_MSGTYPE_PROMPT_ECHO:

        /* Show the logged in user before the first password field. */
        if (!emitted_user_p)
          {
            lines[i].text    = _("Username:");
            lines[i].font    = ws->label_font;
            lines[i].fg      = ws->xft_foreground;
            lines[i].fg2     = lines[i].fg;
            lines[i].bg      = ws->background;
            lines[i].type    = LABEL;
            lines[i].align   = LEFT;
            lines[i].float_p = True;
            i++;

            lines[i].text    = ws->user;		/* $USER */
            lines[i].font    = ws->label_font;
            lines[i].fg      = ws->xft_text_foreground;
            lines[i].fg2     = lines[i].fg;
            lines[i].bg      = ws->passwd_background;
            lines[i].type    = TEXT_RO;
            lines[i].align   = RIGHT;
            i++;
          }

        lines[i].text    = trim (ws->msgs[j].msg);	/* PAM prompt text */
        lines[i].font    = ws->label_font;
        lines[i].fg      = ws->xft_foreground;
        lines[i].fg2     = lines[i].fg;
        lines[i].bg      = ws->background;
        lines[i].type    = LABEL;
        lines[i].align   = LEFT;
        lines[i].float_p = True;
        i++;

        lines[i].text    = (ws->auth_state == AUTH_FINISHED
                              ? _("Checking...") :
                            ws->msgs[j].type == AUTH_MSGTYPE_PROMPT_ECHO
                              ? ws->plaintext_passwd  /* Hopefully UTF-8 */
                            : ws->show_stars_p
                              ? ws->censored_passwd
                              : "");
        lines[i].font    = ws->label_font;
        lines[i].fg      = ws->xft_text_foreground;
        lines[i].fg2     = lines[i].fg;
        lines[i].bg      = ws->passwd_background;
        lines[i].type    = TEXT;
        lines[i].align   = RIGHT;
        lines[i].i_beam  = (ws->i_beam && ws->auth_state != AUTH_FINISHED);
        i++;

        /* Show the current time below the first password field only. */
        if (*date_text && !emitted_user_p)
          {
            lines[i].text   = date_text;
            lines[i].font   = ws->date_font;
            lines[i].fg     = ws->xft_foreground;
            lines[i].fg2    = lines[i].fg;
            lines[i].bg     = ws->background;
            lines[i].type   = LABEL;
            lines[i].align  = RIGHT;
            i++;
          }

        /* Show the current keyboard layout below that. */
        if (ws->kbd_layout_label && *ws->kbd_layout_label && !emitted_user_p)
          {
            lines[i].text   = ws->kbd_layout_label;
            lines[i].font   = ws->date_font;
            lines[i].fg     = ws->xft_foreground;
            lines[i].fg2    = lines[i].fg;
            lines[i].bg     = ws->background;
            lines[i].type   = LABEL;
            lines[i].align  = RIGHT;
            i++;
          }

        emitted_user_p = True;
        break;

      default:
        abort();
        break;
      }
    }

  lines[i].text = 0;
  y = draw_dialog_lines (ws, dbuf, lines,
                         text_left, text_right,
                         ws->border_width + ws->internal_padding +
                         ws->shadow_width);
  window_height = y;
  window_height += (ws->button_font->ascent * 4);
  window_height += (ws->internal_padding + ws->shadow_width * 2 +
                    ws->border_width);


  /* Keep logo area square or taller */
  if (window_height < logo_frame_height + ws->shadow_width * 4)
    window_height = logo_frame_height + ws->shadow_width * 4;

  /* Fitt's Law: It is distracting to reduce the height of the window
     after creation. */
  if (window_height < ws->min_height)
    window_height = ws->min_height;
  ws->min_height = window_height;



  /* Now do a second set of lines for the buttons at the bottom. */

  memset (lines, 0, sizeof(*lines));
  i = 0;

  if (ws->splash_p)
    {
      lines[i].text  = _("Settings");
      lines[i].font  = ws->button_font;
      lines[i].fg    = ws->xft_button_foreground;
      lines[i].fg2   = ws->xft_button_disabled;
      lines[i].bg    = ws->button_background;
      lines[i].type  = BUTTON;
      lines[i].align = LEFT;
      lines[i].float_p = True;
      lines[i].button = &ws->demo_button_state;
      i++;

      if (ws->splash_p > 1)
        /* Settings button is disabled with --splash --splash */
        ws->demo_button_state.disabled_p = True;

      lines[i].text  = _("Help");
      lines[i].font  = ws->button_font;
      lines[i].fg    = ws->xft_button_foreground;
      lines[i].fg2   = ws->xft_button_disabled;
      lines[i].bg    = ws->button_background;
      lines[i].type  = BUTTON;
      lines[i].align = RIGHT;
      lines[i].button = &ws->help_button_state;
      i++;
    }
  else
    {
      if (ws->newlogin_cmd && *ws->newlogin_cmd)
        {
          lines[i].text  = _("New Login");
          lines[i].font  = ws->button_font;
          lines[i].fg    = ws->xft_button_foreground;
          lines[i].fg2   = ws->xft_button_disabled;
          lines[i].bg    = ws->button_background;
          lines[i].type  = BUTTON;
          lines[i].align = LEFT;
          lines[i].float_p = True;
          lines[i].button = &ws->newlogin_button_state;
          i++;
        }

      lines[i].text  = _("OK");
      lines[i].font  = ws->button_font;
      lines[i].fg    = ws->xft_button_foreground;
      lines[i].fg2   = ws->xft_button_disabled;
      lines[i].bg    = ws->button_background;
      lines[i].type  = BUTTON;
      lines[i].align = RIGHT;
      lines[i].button = &ws->unlock_button_state;
      i++;
    }

  lines[i].text = 0;
  y = draw_dialog_lines (ws, dbuf, lines,
                         text_left, text_right,
                         window_height - ws->internal_padding -
                         ext_border -
                         ws->shadow_width -
                         (ws->button_font->ascent * 2));

  /* The thermometer */
  if (ws->thermo_width)
    {
      if (ws->auth_state != AUTH_NOTIFY)
        {
          double remain = ws->end_time - double_time();
          double ratio = remain / ws->passwd_timeout;
          int thermo_w = ws->thermo_width;
          int thermo_h = window_height - ext_border * 2;
          int thermo_h2 = thermo_h - ws->shadow_width * 2;
          int thermo_h3 = thermo_h2 * (1.0 - (ratio > 1 ? 1 : ratio));

          XSetForeground (dpy, gc, ws->thermo_foreground);
          XFillRectangle (dpy, dbuf, gc,
                          thermo_x + ws->shadow_width,
                          ext_border + ws->shadow_width,
                          thermo_w, thermo_h2);
          if (thermo_h3 > 0)
            {
              XSetForeground (dpy, gc, ws->thermo_background);
              XFillRectangle (dpy, dbuf, gc,
                              thermo_x + ws->shadow_width,
                              ext_border + ws->shadow_width,
                              thermo_w, thermo_h3);
            }
        }

      draw_shaded_rectangle (dpy, dbuf,
                             thermo_x, ext_border,
                             ws->thermo_width + ws->shadow_width * 2,
                             window_height - ext_border * 2,
                             ws->shadow_width,
                             ws->shadow_bottom, ws->shadow_top);
    }

  /* The logo, centered vertically.
   */
  {
    int bot  = window_height - ext_border * 2;
    int xoff = (logo_frame_width - ws->logo_width)  / 2;
    int yoff = (bot - ws->logo_height) / 2;
    x = ext_border;
    y = ext_border;
    XSetForeground (dpy, gc, ws->logo_background);
    XFillRectangle (dpy, dbuf, gc, x, y, logo_frame_width, bot);
    XSetForeground (dpy, gc, ws->foreground);
    XSetBackground (dpy, gc, ws->background);
    XSetClipMask (dpy, gc, ws->logo_clipmask);
    XSetClipOrigin (dpy, gc, x + xoff, y + yoff);
    XCopyArea (dpy, ws->logo_pixmap, dbuf, gc, 0, 0,
               ws->logo_width, ws->logo_height,
               x + xoff, y + yoff);
    XSetClipMask (dpy, gc, 0);
    draw_shaded_rectangle (dpy, dbuf,
                           x, y,
                           logo_frame_width,
                           bot,
                           ws->shadow_width,
                           ws->shadow_bottom, ws->shadow_top);
  }

  /* The window's shadow */
  draw_shaded_rectangle (dpy, dbuf,
                         ws->border_width, ws->border_width,
                         window_width - ws->border_width * 2,
                         window_height - ws->border_width * 2,
			 ws->shadow_width,
			 ws->shadow_top, ws->shadow_bottom);

  /* The window's border */
  draw_shaded_rectangle (dpy, dbuf,
                         0, 0, window_width, window_height,
			 ws->border_width,
			 ws->border_color, ws->border_color);


  /* Now that everything has been rendered into the pixmap, reshape the window
     and copy the pixmap to it.  This double-buffering is to prevent flicker.

     You'd think we could just reshape the window and then XMapRaised, but no.
     With the XCompose extension enabled and the "Mutter" window manager, the
     dialog window was sometimes not appearing on the screen, despite the fact
     that XQueryTree reported it as the topmost window.  The mouse pointer also
     reflected it being there, but it wasn't visible.  This is probably related
     to the "XCompositeGetOverlayWindow" window in some way, which is a magic,
     invisible window that is secretly excluded from the list returned by
     XQueryTree, but I can't figure out what was really going on, except that
     XMapRaised did not make my OverrideRedirect window appear topmost on the
     screen.

     However!  Though XMapRaised was not working, it turns out that destroying
     and re-creating the window *does* make it appear.  So we do that, any time
     the window's shape has changed, or some other window has raised above it.

     Calling XQueryTree at 30fps could conceivably be a performance problem,
     if there are thousands of windows on the screen.  But here we are.
   */
  {
    Bool size_changed_p, occluded_p;

    /* It's distracting to move or shrink the window after creating it. */
    if (xgwa.height > 100 && xgwa.height > window_height)
      window_height = xgwa.height;
    if (! ws->x)
      {
        ws->x = ws->cx - (window_width  / 2);
        ws->y = ws->cy - (window_height / 2);
      }

    /* If there is any change to the window's size, or if the window is
       not on top, destroy and re-create the window. */
    size_changed_p = !(xgwa.x == ws->x &&
                       xgwa.y == ws->y &&
                       xgwa.width  == window_width &&
                       xgwa.height == window_height);
    occluded_p = (!size_changed_p && 
                  window_occluded_p (ws->dpy, ws->window));

    if (size_changed_p || occluded_p)
      {
# if 0  /* Window sometimes disappears under Mutter 3.30.2, Feb 2021. */
        XWindowChanges wc;
        wc.x = ws->x;
        wc.y = ws->y;
        wc.width  = window_width;
        wc.height = window_height;
        if (verbose_p)
          fprintf (stderr, "%s: reshaping window %dx%d+%d+%d\n", blurb(),
                   wc.width, wc.height, wc.x, wc.y);
        XConfigureWindow (ws->dpy, ws->window, CWX|CWY|CWWidth|CWHeight, &wc);
# else
        if (verbose_p)
          fprintf (stderr, "%s: re-creating window: %s\n", blurb(),
                   size_changed_p ? "size changed" : "occluded");
        create_window (ws, window_width, window_height);
# endif
        XMapRaised (ws->dpy, ws->window);
        XInstallColormap (ws->dpy, ws->cmap);
      }
  }

  XFreeGC (dpy, gc);
  gc = XCreateGC (dpy, ws->window, 0, &gcv);
  XCopyArea (dpy, dbuf, ws->window, gc, 0, 0,
             window_width, window_height, 0, 0);
  XSync (dpy, False);
  XFreeGC (dpy, gc);
  XFreePixmap (dpy, dbuf);
  free (lines);

  if (verbose_p > 1)
    {
      static time_t last = 0;
      static int count = 0;
      count++;
      if (now > last)
        {
          double fps = count / (double) (now - last);
          fprintf (stderr, "%s: FPS: %0.1f\n", blurb(), fps);
          count = 0;
          last = now;
        }
    }
}


/* Unmaps the window and frees window_state.
 */
static void
destroy_window (window_state *ws)
{
  XEvent event;

  memset (ws->plaintext_passwd, 0, sizeof(ws->plaintext_passwd));
  memset (ws->plaintext_passwd_char_size, 0,
          sizeof(ws->plaintext_passwd_char_size));
  memset (ws->censored_passwd, 0, sizeof(ws->censored_passwd));

  if (ws->timer)
    {
      XtRemoveTimeOut (ws->timer);
      ws->timer = 0;
    }

  if (ws->cursor_timer)
    {
      XtRemoveTimeOut (ws->cursor_timer);
      ws->cursor_timer = 0;
    }
  if (ws->bs_timer)
    {
      XtRemoveTimeOut (ws->bs_timer);
      ws->bs_timer = 0;
    }

  while (XCheckMaskEvent (ws->dpy, PointerMotionMask, &event))
    if (verbose_p)
      fprintf (stderr, "%s: discarding MotionNotify event\n", blurb());

  if (ws->window)
    {
      XDestroyWindow (ws->dpy, ws->window);
      ws->window = 0;
    }
  
  if (ws->heading_label)    free (ws->heading_label);
  if (ws->date_format)      free (ws->date_format);
  if (ws->hostname_label)   free (ws->hostname_label);
  if (ws->kbd_layout_label) free (ws->kbd_layout_label);

  if (ws->heading_font)  XftFontClose (ws->dpy, ws->heading_font);
  if (ws->body_font)     XftFontClose (ws->dpy, ws->body_font);
  if (ws->label_font)    XftFontClose (ws->dpy, ws->label_font);
  if (ws->date_font)     XftFontClose (ws->dpy, ws->date_font);
  if (ws->button_font)   XftFontClose (ws->dpy, ws->button_font);
  if (ws->hostname_font) XftFontClose (ws->dpy, ws->hostname_font);

  XftColorFree (ws->dpy, DefaultVisualOfScreen (ws->screen),
                DefaultColormapOfScreen (ws->screen),
                &ws->xft_foreground);
  XftColorFree (ws->dpy, DefaultVisualOfScreen (ws->screen),
                DefaultColormapOfScreen (ws->screen),
                &ws->xft_button_foreground);
  XftColorFree (ws->dpy, DefaultVisualOfScreen (ws->screen),
                DefaultColormapOfScreen (ws->screen),
                &ws->xft_text_foreground);
  XftColorFree (ws->dpy, DefaultVisualOfScreen (ws->screen),
                DefaultColormapOfScreen (ws->screen),
                &ws->xft_error_foreground);
  if (ws->xftdraw) XftDrawDestroy (ws->xftdraw);

# if 0  /* screw this, we're exiting anyway */
  if (ws->foreground != black && ws->foreground != white)
    XFreeColors (ws->dpy, ws->cmap, &ws->foreground, 1, 0L);
  if (ws->background != black && ws->background != white)
    XFreeColors (ws->dpy, ws->cmap, &ws->background, 1, 0L);
  if (ws->button_background != black && ws->button_background != white)
    XFreeColors (ws->dpy, ws->cmap, &ws->button_background, 1, 0L);
  if (ws->passwd_background != black && ws->passwd_background != white)
    XFreeColors (ws->dpy, ws->cmap, &ws->passwd_background, 1, 0L);
  if (ws->thermo_foreground != black && ws->thermo_foreground != white)
    XFreeColors (ws->dpy, ws->cmap, &ws->thermo_foreground, 1, 0L);
  if (ws->thermo_background != black && ws->thermo_background != white)
    XFreeColors (ws->dpy, ws->cmap, &ws->thermo_background, 1, 0L);
  if (ws->logo_background != black && ws->logo_background != white)
    XFreeColors (ws->dpy, ws->cmap, &ws->logo_background, 1, 0L);
  if (ws->shadow_top != black && ws->shadow_top != white)
    XFreeColors (ws->dpy, ws->cmap, &ws->shadow_top, 1, 0L);
  if (ws->shadow_bottom != black && ws->shadow_bottom != white)
    XFreeColors (ws->dpy, ws->cmap, &ws->shadow_bottom, 1, 0L);
# endif

  if (ws->logo_pixmap)
    XFreePixmap (ws->dpy, ws->logo_pixmap);
  if (ws-> logo_clipmask)
    XFreePixmap (ws->dpy, ws->logo_clipmask);
  if (ws->logo_pixels)
    {
      if (ws->logo_npixels)
        XFreeColors (ws->dpy, ws->cmap, ws->logo_pixels, ws->logo_npixels, 0L);
      free (ws->logo_pixels);
      ws->logo_pixels = 0;
      ws->logo_npixels = 0;
    }

  XSync (ws->dpy, False);
  memset (ws, 0, sizeof(*ws));
  free (ws);

}


static void
unlock_cb (window_state *ws)
{
  if (ws->auth_state == AUTH_READ)
    ws->auth_state = AUTH_FINISHED;
}


/* We store the count and last time of authorization failures on a property
   on the root window, so that on subsequent runs of this program that
   succeed, we can warn the user that someone tried to log in and failed.
 */
static void
persistent_auth_status_failure (window_state *ws,
                                Bool increment_p, Bool clear_p,
                                int *count_ret,
                                time_t *time_ret)
{
  Display *dpy = ws->dpy;
  Window w = RootWindow (dpy, 0);  /* always screen 0 */
  Atom prop = XInternAtom (ws->dpy, LOCK_FAILURE_ATOM, False);
  int count = 0;
  time_t tt = 0;

  Atom type;
  unsigned char *dataP = 0;
  int format;
  unsigned long nitems, bytesafter;

  if (increment_p && clear_p) abort();

  /* Read the old property so that we can increment it. */
  if (XGetWindowProperty (dpy, w, prop,
                          0, 999, False, XA_INTEGER,
                          &type, &format, &nitems, &bytesafter,
                          &dataP)
      == Success
      && type == XA_INTEGER
      && nitems >= 2
      && dataP)
    {
      count = ((PROP32 *) dataP) [0];
      tt    = ((PROP32 *) dataP) [1];  /* Y2038 bug: unsigned 32 bit time_t */
      if (verbose_p)
        fprintf (stderr, "%s: previous auth failures: %d @ %lu\n",
                 blurb(), count, (unsigned long) tt);
    }

  if (dataP)
    XFree (dataP);

  if (clear_p)
    {
      XDeleteProperty (dpy, w, prop);
      if (verbose_p)
        fprintf (stderr, "%s: deleted auth failure property\n", blurb());
    }
  else if (increment_p)
    {
      PROP32 vv[2];
      count++;

      /* Remember the time of the *oldest* failed login.  A failed login
         5 seconds ago does not mean we should skip warning about a failed
         login yesterday.
       */
      if (tt <= 0) tt = time ((time_t *) 0);

      vv[0] = (PROP32) count;
      vv[1] = (PROP32) tt;
      XChangeProperty (dpy, w, prop, XA_INTEGER, 32,
                       PropModeReplace, (unsigned char *) vv, 2);
      if (verbose_p)
        fprintf (stderr, "%s: saved auth failure: %d @ %lu\n",
                 blurb(), count, (unsigned long) tt);
    }

  if (count_ret) *count_ret = count;
  if (time_ret)  *time_ret  = tt;
}


static void bs_timer (XtPointer, XtIntervalId *);

static void
handle_keypress (window_state *ws, XKeyEvent *event)
{
  unsigned char decoded [MAX_BYTES_PER_CHAR * 10]; /* leave some slack */
  KeySym keysym = 0;

  /* XLookupString may return more than one character via XRebindKeysym;
     and on some systems it returns multi-byte UTF-8 characters (contrary
     to its documentation, which says it returns only Latin1.)

     It seems to only do so, however, if setlocale() has been called.
     See the code inside ENABLE_NLS in xscreensaver-auth.c.

     The X Keyboard Extension X11R6.4 documentation says: "When Xkb is
     present, XLookupString is allowed, but not required, to return strings
     in character sets other than ISO Latin-1, depending on the current
     locale."  So I guess that means that multi-byte strings returned by
     XLookupString might not be UTF-8, and thus might not be compatible
     with XftDrawStringUtf8.
   */
  int decoded_size = XLookupString (event, (char *)decoded, sizeof(decoded),
                                    &keysym, &ws->compose_status);

  if (decoded_size > MAX_BYTES_PER_CHAR)
    {
      /* The multi-byte character returned is too large. */
      XBell (ws->dpy, 0);
      return;
    }

  decoded[decoded_size] = 0;

  /* Add 10% to the time remaining every time a key is pressed, but don't
     go past the max duration. */
  {
    double now = double_time();
    double remain = ws->end_time - now;
    remain *= 1.1;
    if (remain > ws->passwd_timeout) remain = ws->passwd_timeout;
    if (remain < 3) remain = 3;
    ws->end_time = now + remain;
  }

  if (decoded_size == 1)		/* Handle single-char commands */
    {
      switch (*decoded)
        {
        case '\010': case '\177':			/* Backspace */
          {
            /* kludgey way to get the number of "logical" characters. */
            int nchars = strlen (ws->plaintext_passwd_char_size);
            int nbytes = strlen (ws->plaintext_passwd);
            if (nbytes <= 0)
              XBell (ws->dpy, 0);
            else
              {
                int i;
                for (i = ws->plaintext_passwd_char_size[nchars-1]; i >= 0; i--)
                  {
                    if (nbytes < 0) abort();
                    ws->plaintext_passwd[nbytes--] = 0;
                  }
                ws->plaintext_passwd_char_size[nchars-1] = 0;
              }

            /* The XInput2 extension does not send auto-repeat KeyPress
               events, and it annoys people that you can't hold down the
               Backspace key to clear the line.  So clear the whole line
               if the key is held down for a little while.  */
            if (ws->bs_timer)
              XtRemoveTimeOut (ws->bs_timer);
            ws->bs_timer =
              XtAppAddTimeOut (ws->app, 1000 * 0.6, bs_timer, (XtPointer) ws);
          }
          break;

        case '\012': case '\015':			/* Enter */
          unlock_cb (ws);
          break;

        case '\033':					/* Escape */
          ws->auth_state = AUTH_CANCEL;
          break;

        case '\025': case '\030':			/* Erase line ^U ^X */
          memset (ws->plaintext_passwd, 0, sizeof (ws->plaintext_passwd));
          memset (ws->plaintext_passwd_char_size, 0, 
                  sizeof (ws->plaintext_passwd_char_size));
          break;

        default:
          if (*decoded < ' ' && *decoded != '\t')	/* Other ctrl char */
            XBell (ws->dpy, 0);
          else
            goto SELF_INSERT;
          break;
        }
    }
  else
    {
      int nbytes, nchars;
    SELF_INSERT:
      nbytes = strlen (ws->plaintext_passwd);
      nchars = strlen (ws->plaintext_passwd_char_size);
      if (nchars + 1 >= sizeof (ws->plaintext_passwd_char_size)-1 ||
          nbytes + decoded_size >= sizeof (ws->plaintext_passwd)-1)
        XBell (ws->dpy, 0);  /* overflow */
      else if (decoded_size == 0)
        ;  /* Non-inserting keysym (Shift, Ctrl) */
      else
        {
          ws->plaintext_passwd_char_size[nchars] = decoded_size;
          ws->plaintext_passwd_char_size[nchars+1] = 0;
          memcpy (ws->plaintext_passwd + nbytes, decoded, decoded_size);
          ws->plaintext_passwd[nbytes + decoded_size] = 0;
        }
    }

  /* Construct the string of asterisks. */
  {
    char *out = ws->censored_passwd;
    int i;
    *out = 0;
    for (i = 0; i < MAX_PASSWD_CHARS && ws->plaintext_passwd_char_size[i]; i++)
      {
        strcat (out, ws->asterisk_utf8);
        out += strlen(out);
      }
  }
}


static Bool
handle_button (window_state *ws, XEvent *xev, line_button_state *bs)
{
  Bool mouse_in_box = 
    (xev->xbutton.x_root >= (ws->x + bs->rect.x) &&
     xev->xbutton.x_root <  (ws->x + bs->rect.x + bs->rect.width) &&
     xev->xbutton.y_root >= (ws->y + bs->rect.y) &&
     xev->xbutton.y_root <  (ws->y + bs->rect.y + bs->rect.height));

  bs->down_p = (!bs->disabled_p &&
                mouse_in_box &&
                xev->xany.type != ButtonRelease);

  if (xev->xany.type == ButtonRelease && mouse_in_box && !bs->disabled_p)
    {
      bs->disabled_p = True;  /* Only allow them to press the button once. */
      if (bs->fn)
        bs->fn (ws);
      else if (bs->cmd)
        {
          int ac = 0;
          char *av[10];
          av[ac++] = "/bin/sh";
          av[ac++] = "-c";
          av[ac++] = bs->cmd;
          av[ac]   = 0;
          fork_and_exec (ws->dpy, ac, av);
        }
      else
        XBell (ws->dpy, 0);
    }
  return mouse_in_box;
}


static Bool
handle_event (window_state *ws, XEvent *xev)
{
  Bool refresh_p = False;
  switch (xev->xany.type) {
  case KeyPress:
    if (ws->splash_p)
      ws->auth_state = AUTH_CANCEL;
    else
      {
        handle_keypress (ws, &xev->xkey);
        ws->caps_p = (xev->xkey.state & LockMask);
        if (ws->auth_state == AUTH_NOTIFY)
          ws->auth_state = AUTH_CANCEL;
      }
    refresh_p = True;
    break;

  case KeyRelease:
    if (ws->bs_timer)
      {
        XtRemoveTimeOut (ws->bs_timer);
        ws->bs_timer = 0;
      }
    break;

  case ButtonPress:
  case ButtonRelease:
    {
      if (! (handle_button (ws, xev, &ws->newlogin_button_state) ||
             handle_button (ws, xev, &ws->unlock_button_state) ||
             handle_button (ws, xev, &ws->demo_button_state) ||
             handle_button (ws, xev, &ws->help_button_state)))
        if (ws->splash_p && xev->xany.type == ButtonRelease)
          ws->auth_state = AUTH_CANCEL;
    refresh_p = True;
    }
  default:
    break;
  }
  return refresh_p;
}


/* Blink the I-beam cursor. */
static void
cursor_timer (XtPointer closure, XtIntervalId *id)
{
  window_state *ws = (window_state *) closure;
  int timeout = 0.7 * 1000 * (ws->i_beam ? 0.25 : 0.75);
  if (ws->cursor_timer)
    XtRemoveTimeOut (ws->cursor_timer);
  ws->cursor_timer =
    XtAppAddTimeOut (ws->app, timeout, cursor_timer, (XtPointer) ws);
  ws->i_beam = !ws->i_beam;
}


/* Auto-repeat Backspace, since XInput2 doesn't do autorepeat. */
static void
bs_timer (XtPointer closure, XtIntervalId *id)
{
  window_state *ws = (window_state *) closure;
  if (ws->bs_timer)
    XtRemoveTimeOut (ws->bs_timer);
  ws->bs_timer = 0;
  /* Erase line */
  memset (ws->plaintext_passwd, 0, sizeof (ws->plaintext_passwd));
  memset (ws->plaintext_passwd_char_size, 0, 
          sizeof (ws->plaintext_passwd_char_size));
  memset (ws->censored_passwd, 0, sizeof(ws->censored_passwd));
  window_draw (ws);
}


/* Redraw the window for the thermometer, and exit if the time has expired. 
 */
static void
thermo_timer (XtPointer closure, XtIntervalId *id)
{
  window_state *ws = (window_state *) closure;
  int timeout = 1000/30;  /* FPS */
  double now = double_time();
  if (now >= ws->end_time)
    ws->auth_state = AUTH_TIME;
  if (ws->timer) XtRemoveTimeOut (ws->timer);
  ws->timer = XtAppAddTimeOut (ws->app, timeout, thermo_timer, (XtPointer) ws);
}


static void
gui_main_loop (window_state *ws, Bool splash_p, Bool notification_p)
{
  int timeout;
  Bool refresh_p = True;

  if (splash_p)
    {
      timeout = get_seconds_resource (ws->dpy, "splashDuration", "Time");
      if (timeout <= 1) timeout = 1;
    }
  else if (ws->auth_state == AUTH_NOTIFY)
    timeout = 5;
  else
    {
      timeout = ws->passwd_timeout;
      cursor_timer (ws, 0);
    }

  ws->start_time = double_time();
  ws->end_time = ws->start_time + timeout;

  /* Since the "xscreensaver" process holds the mouse and keyboard grabbed
     while "xscreensaver-auth" is running, we don't receive normal KeyPress
     events.  That means that the XInput2 extension is required in order to
     snoop on the keyboard in a way that bypasses grabs.
   */
  if (! ws->xi_opcode)
    {
      Bool ov = verbose_p;
      verbose_p = False;
      init_xinput (ws->dpy, &ws->xi_opcode);
      verbose_p = ov;
    }

  thermo_timer (ws, 0);
  window_draw (ws);

  while (ws->auth_state == AUTH_READ ||
         ws->auth_state == AUTH_NOTIFY)
    {
      XEvent xev;
      XtInputMask m = XtAppPending (ws->app);

      if (m & XtIMXEvent)
        /* Process timers then block on an X event (which we know is there) */
        XtAppNextEvent (ws->app, &xev);
      else
        {
          if (m)
            /* Process timers only, don't block */
            XtAppProcessEvent (ws->app, m);
          else
            {
              if (refresh_p)
                {
                  /* Redraw when outstanding events have been processed. */
                  window_draw (ws);
                  refresh_p = False;
                }

              /* No way to say "block until timer *or* X pending".
                 Without this, the timer that changes auth_state will fire but
                 then we will still be blocked until the next X event. */
              usleep (1000000/30);
            }
          continue;
        }

      if ((m & ~XtIMXEvent) && !ws->splash_p)
        refresh_p = True;   /* In auth mode, all timers refresh */

      if (verbose_p || debug_p)
        print_xinput_event (ws->dpy, &xev, "");

      /* Convert XInput events to Xlib events, for simplicity and familiarity.
       */
      if (xev.xcookie.type == GenericEvent &&
          xev.xcookie.extension == ws->xi_opcode &&
          (xev.xcookie.data || XGetEventData (ws->dpy, &xev.xcookie)))
        {
          XEvent ev2;
          Bool ok =
            xinput_event_to_xlib (xev.xcookie.evtype, xev.xcookie.data, &ev2);
          XFreeEventData (ws->dpy, &xev.xcookie);
          if (ok)
            xev = ev2;
        }

      if (handle_event (ws, &xev))
        refresh_p = True;

      XtDispatchEvent (&xev);

      switch (xev.xany.type) {

      /* I don't think we ever receive these, but if we do, redraw. */
      case Expose: case GraphicsExpose:
        refresh_p = True;
        break;

      /* Likewise, receiving this event would be ideal, but we don't. */
      case VisibilityNotify:
        refresh_p = True;
        if (verbose_p > 1)
          fprintf (stderr, "%s: VisibilityNotify\n", blurb());
        break;

      /* When another override-redirect window is raised above us,
         we receive several ConfigureNotify events on the root window. */
      case ConfigureNotify:
        if (verbose_p > 1)
          fprintf (stderr, "%s: ConfigureNotify\n", blurb());
        break;

      case MappingNotify:
        /* This event is supposed to be sent when the keymap is changed.
           You would think that typing XK_ISO_Next_Group to change the
           keyboard layout would count as such.  It does not. */
        if (verbose_p)
          fprintf (stderr, "%s: MappingNotify\n", blurb());
        get_keyboard_layout (ws);
        refresh_p = True;
        break;

      default:
        break;
      }

      /* Since MappingNotify doesn't work, we have to do this crap instead.
         Probably some of these events could be ignored, as it seems that
         any.xkb_type == XkbStateNotify comes in every time a modifier key is
         touched.  What event comes in when there is a keyboard layout change,
         the only thing we actually care about?
       */
      if (xev.xany.type == ws->xkb_opcode)
        {
          XkbEvent *xkb = (XkbEvent *) &xev;
          if (verbose_p)
            fprintf (stderr, "%s: XKB event %d\n", blurb(), xkb->any.xkb_type);
          get_keyboard_layout (ws);
          refresh_p = True;
        }
    }

  /* Re-raw the window one last time, since it might sit here for a while
     while PAM does it's thing. */
  window_draw (ws);
  XSync (ws->dpy, False);

  if (verbose_p) {
    const char *kind = (splash_p ? "splash" :
                        notification_p ? "notification" : "authentication");
    switch (ws->auth_state) {
    case AUTH_FINISHED:
      fprintf (stderr, "%s: %s input finished\n", blurb(), kind); break;
    case AUTH_CANCEL:
      fprintf (stderr, "%s: %s canceled\n", blurb(), kind); break;
    case AUTH_TIME:
      fprintf (stderr, "%s: %s timed out\n", blurb(), kind); break;
    default: break;
    }
  }
}


/* Pops up a dialog and waits for the user to complete it.
   Returns 0 on successful completion.
   Updates 'resp' with any entered response.
 */
static Bool
dialog_session (window_state *ws,
                int nmsgs,
                const auth_message *msgs,
                auth_response *resp)
{
  int i;

  ws->auth_state = AUTH_READ;
  ws->nmsgs = nmsgs;
  ws->msgs  = msgs;

  memset (ws->plaintext_passwd, 0, sizeof(ws->plaintext_passwd));
  memset (ws->plaintext_passwd_char_size, 0,
          sizeof(ws->plaintext_passwd_char_size));
  memset (ws->censored_passwd, 0, sizeof(ws->censored_passwd));
  ws->unlock_button_state.disabled_p = False;

  gui_main_loop (ws, False, False);

  if (ws->auth_state != AUTH_FINISHED)
    return True;   /* Timed out or canceled */

  /* Find the (at most one) input field in the previous batch and return
     the entered plaintext to it. */
  for (i = 0; i < nmsgs; i++)
    {
      if (msgs[i].type == AUTH_MSGTYPE_PROMPT_ECHO ||
          msgs[i].type == AUTH_MSGTYPE_PROMPT_NOECHO)
        {
          if (resp[i].response) abort();
          resp[i].response = strdup(ws->plaintext_passwd);
        }
    }

  ws->nmsgs = 0;
  ws->msgs  = 0;

  return False;
}


/* To retain this across multiple calls from PAM to xscreensaver_auth_conv. */
window_state *global_ws = 0;


/* The authentication conversation function.

   Like a PAM conversation function, this accepts multiple messages in a
   single round.  We can only do one text entry field in the dialog at a
   time, so if there is more than one entry, multiple dialogs will be used.

   PAM might call this multiple times before authenticating.  We are unable
   to combine multiple messages onto a single dialog if PAM splits them
   between calls to this function.

   Returns True on success.  If the user timed out or cancelled, we just exit.
 */
Bool
xscreensaver_auth_conv (void *closure,
                        int nmsgs,
                        const auth_message *msgs,
                        auth_response **resp)
{
  Widget root_widget = (Widget) closure;
  int i;
  int prev_msg = 0;
  int field_count = 0;
  auth_response *responses;
  window_state *ws = global_ws;

  if (!ws)
    ws = global_ws = window_init (root_widget, False);

  responses = calloc (nmsgs, sizeof(*responses));
  if (!responses) abort();

  for (i = 0; i < nmsgs; i++)
    {
      if (msgs[i].type == AUTH_MSGTYPE_PROMPT_ECHO ||
          msgs[i].type == AUTH_MSGTYPE_PROMPT_NOECHO)
        {
          /* A text input field. */

          if (field_count > 0)
            {
              /* This is the second one -- we must run the dialog on
                 the field collected so far. */
              if (dialog_session (ws,
                                  i - prev_msg,
                                  msgs + prev_msg,
                                  responses + prev_msg))
                goto END;
              prev_msg = i;
              field_count = 0;
            }

          field_count++;
        }
    }

  if (prev_msg < i || nmsgs == 0)
    /* Run the dialog on the stuff that's left.  This happens if there was
       more than one text field. */
    dialog_session (ws,
                    i - prev_msg,
                    msgs + prev_msg,
                    responses + prev_msg);

 END:

  switch (ws->auth_state) {
  case AUTH_CANCEL:
  case AUTH_TIME:
    /* No need to return to PAM or clean up. We're outta here!
       Exit with 0 to distinguish it from our "success" or "failure"
       exit codes. */
    destroy_window (ws);
    exit (0);
    break;
  case AUTH_FINISHED:
    *resp = responses;
    return True;
  default:
    abort();
    break;
  }
}


/* Called after authentication is complete so that we can present a "nope"
   dialog if it failed, or snitch on previous failed login attempts.
 */
void
xscreensaver_auth_finished (void *closure, Bool authenticated_p)
{
  Widget root_widget = (Widget) closure;
  window_state *ws = global_ws;
  char msg[1024];
  int unlock_failures = 0;
  time_t failure_time = 0;
  Bool prompted_p = !!ws;

  /* If this was called without xscreensaver_auth_conv() ever having been
     called, then either PAM decided that the user is authenticated without
     a prompt (e.g. a bluetooth fob); or there was an error initializing
     passwords (e.g., shadow passwords but not setuid.)
   */
  if (!ws)
    ws = global_ws = window_init (root_widget, False);

  if (authenticated_p)
    {
      /* Read the old failure count, and delete it. */
      persistent_auth_status_failure (ws, False, True,
                                      &unlock_failures, &failure_time);
    }
  else
    {
      /* Increment the failure count. */
      persistent_auth_status_failure (ws, True, False,
                                      &unlock_failures, &failure_time);
    }

  /* If we have something to say, put the dialog back up for a few seconds
     to display it.  Otherwise, don't bother.
   */
  if (!authenticated_p && !prompted_p)
    strcpy (msg, _("Password initialization failed"));
  else if (!authenticated_p && ws && ws->caps_p)
    strcpy (msg, _("Authentication failed (Caps Lock?)"));
  else if (!authenticated_p)
    strcpy (msg, _("Authentication failed!"));
  else if (authenticated_p && unlock_failures > 0)
    {
      time_t now = time ((time_t *) 0);
      int sec    = now - failure_time;
      int min    = (sec + 30)   / 60;
      int hours  = (min + 30)   / 60;
      int days   = (hours + 12) / 24;
      char ago[100];
      int warning_slack =
        get_integer_resource (ws->dpy, "authWarningSlack", "AuthWarningSlack");

      if (sec < warning_slack)
        {
          if (verbose_p)
            fprintf (stderr, "%s: ignoring recent unlock failures:"
                     " %d within %d sec\n",
                     blurb(), unlock_failures, warning_slack);
          goto END;
        }
      else if (days  > 1) sprintf (ago, _("%d days ago"), days);
      else if (hours > 1) sprintf (ago, _("%d hours ago"), hours);
      else if (min   > 1) sprintf (ago, _("%d minutes ago"), min);
      else                sprintf (ago, _("just now"));

      if (unlock_failures == 1)
        sprintf (msg, _("There has been 1 failed login attempt, %s."), ago);
      else
        sprintf (msg,
                 _("There have been %d failed login attempts, oldest %s."),
                 unlock_failures, ago);
    }
  else
    {
      /* No need to pop up a window.  Authenticated, and there are no previous
         failures to report.
       */
      goto END;
    }

  if (!*msg) abort();
  ws->body_label = strdup (msg);

  ws->auth_state = AUTH_NOTIFY;
  gui_main_loop (ws, False, True);

 END:
  destroy_window (global_ws);
}


void
xscreensaver_splash (void *closure, Bool disable_settings_p)
{
  Widget root_widget = (Widget) closure;
  window_state *ws = window_init (root_widget, disable_settings_p ? 2 : 1);
  ws->auth_state = AUTH_READ;
  gui_main_loop (ws, True, False);
  destroy_window (ws);
  exit (0);
}
