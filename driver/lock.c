/* lock.c --- handling the password dialog for locking-mode.
 * xscreensaver, Copyright (c) 1993-2002 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Athena locking code contributed by Jon A. Christopher <jac8782@tamu.edu> */
/* Copyright 1997, with the same permissions as above. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <X11/Intrinsic.h>
#include <X11/Xos.h>		/* for time() */
#include <time.h>
#include <sys/time.h>
#include "xscreensaver.h"
#include "resources.h"

#ifndef NO_LOCKING              /* (mostly) whole file */

#ifdef HAVE_SYSLOG
# include <syslog.h>
#endif /* HAVE_SYSLOG */

#ifdef HAVE_XHPDISABLERESET
# include <X11/XHPlib.h>
  static void hp_lock_reset (saver_info *si, Bool lock_p);
#endif /* HAVE_XHPDISABLERESET */

#ifdef HAVE_VT_LOCKSWITCH
# include <fcntl.h>
# include <sys/ioctl.h>
# include <sys/vt.h>
  static void linux_lock_vt_switch (saver_info *si, Bool lock_p);
#endif /* HAVE_VT_LOCKSWITCH */

#ifdef HAVE_XF86VMODE
# include <X11/extensions/xf86vmode.h>
  static void xfree_lock_mode_switch (saver_info *si, Bool lock_p);
#endif /* HAVE_XF86VMODE */


#ifdef _VROOT_H_
ERROR!  You must not include vroot.h in this file.
#endif

#ifndef VMS
# include <pwd.h>
#else /* VMS */

extern char *getenv(const char *name);
extern int validate_user(char *name, char *password);

static Bool
vms_passwd_valid_p(char *pw, Bool verbose_p)
{
  return (validate_user (getenv("USER"), typed_passwd) == 1);
}
# undef passwd_valid_p
# define passwd_valid_p vms_passwd_valid_p

#endif /* VMS */


#undef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))

enum passwd_state { pw_read, pw_ok, pw_null, pw_fail, pw_cancel, pw_time };

struct passwd_dialog_data {

  saver_screen_info *prompt_screen;
  int previous_mouse_x, previous_mouse_y;

  enum passwd_state state;
  char typed_passwd [80];
  XtIntervalId timer;
  int i_beam;

  float ratio;
  Position x, y;
  Dimension width;
  Dimension height;
  Dimension border_width;

  char *heading_label;
  char *body_label;
  char *user_label;
  char *passwd_label;
  char *date_label;
  char *user_string;
  char *passwd_string;

  XFontStruct *heading_font;
  XFontStruct *body_font;
  XFontStruct *label_font;
  XFontStruct *passwd_font;
  XFontStruct *date_font;

  Pixel foreground;
  Pixel background;
  Pixel passwd_foreground;
  Pixel passwd_background;
  Pixel thermo_foreground;
  Pixel thermo_background;
  Pixel shadow_top;
  Pixel shadow_bottom;

  Dimension logo_width;
  Dimension logo_height;
  Dimension thermo_width;
  Dimension internal_border;
  Dimension shadow_width;

  Dimension passwd_field_x, passwd_field_y;
  Dimension passwd_field_width, passwd_field_height;

  Dimension thermo_field_x, thermo_field_y;
  Dimension thermo_field_height;

  Pixmap logo_pixmap;
  int logo_npixels;
  unsigned long *logo_pixels;

  Pixmap save_under;
};

static void draw_passwd_window (saver_info *si);
static void update_passwd_window (saver_info *si, const char *printed_passwd,
				  float ratio);
static void destroy_passwd_window (saver_info *si);
static void undo_vp_motion (saver_info *si);


static void
make_passwd_window (saver_info *si)
{
  struct passwd *p = getpwuid (getuid ());
  XSetWindowAttributes attrs;
  unsigned long attrmask = 0;
  passwd_dialog_data *pw = (passwd_dialog_data *) calloc (1, sizeof(*pw));
  Screen *screen;
  Colormap cmap;
  char *f;

  pw->prompt_screen = &si->screens [mouse_screen (si)];
  if (si->prefs.verbose_p)
    fprintf (stderr, "%s: %d: creating password dialog.\n",
             blurb(), pw->prompt_screen->number);

  screen = pw->prompt_screen->screen;
  cmap = DefaultColormapOfScreen (screen);

  pw->ratio = 1.0;

  pw->heading_label = get_string_resource ("passwd.heading.label",
					   "Dialog.Label.Label");
  pw->body_label = get_string_resource ("passwd.body.label",
					"Dialog.Label.Label");
  pw->user_label = get_string_resource ("passwd.user.label",
					"Dialog.Label.Label");
  pw->passwd_label = get_string_resource ("passwd.passwd.label",
					  "Dialog.Label.Label");
  pw->date_label = get_string_resource ("dateFormat", "DateFormat");

  if (!pw->heading_label)
    pw->heading_label = strdup("ERROR: REESOURCES NOT INSTALLED CORRECTLY");
  if (!pw->body_label)
    pw->body_label = strdup("ERROR: REESOURCES NOT INSTALLED CORRECTLY");
  if (!pw->user_label) pw->user_label = strdup("ERROR");
  if (!pw->passwd_label) pw->passwd_label = strdup("ERROR");
  if (!pw->date_label) pw->date_label = strdup("ERROR");

  /* Put the version number in the label. */
  {
    char *s = (char *) malloc (strlen(pw->heading_label) + 20);
    sprintf(s, pw->heading_label, si->version);
    free (pw->heading_label);
    pw->heading_label = s;
  }

  pw->user_string = strdup (p && p->pw_name ? p->pw_name : "???");
  pw->passwd_string = strdup("");

  f = get_string_resource ("passwd.headingFont", "Dialog.Font");
  pw->heading_font = XLoadQueryFont (si->dpy, (f ? f : "fixed"));
  if (!pw->heading_font) pw->heading_font = XLoadQueryFont (si->dpy, "fixed");
  if (f) free (f);

  f = get_string_resource("passwd.bodyFont", "Dialog.Font");
  pw->body_font = XLoadQueryFont (si->dpy, (f ? f : "fixed"));
  if (!pw->body_font) pw->body_font = XLoadQueryFont (si->dpy, "fixed");
  if (f) free (f);

  f = get_string_resource("passwd.labelFont", "Dialog.Font");
  pw->label_font = XLoadQueryFont (si->dpy, (f ? f : "fixed"));
  if (!pw->label_font) pw->label_font = XLoadQueryFont (si->dpy, "fixed");
  if (f) free (f);

  f = get_string_resource("passwd.passwdFont", "Dialog.Font");
  pw->passwd_font = XLoadQueryFont (si->dpy, (f ? f : "fixed"));
  if (!pw->passwd_font) pw->passwd_font = XLoadQueryFont (si->dpy, "fixed");
  if (f) free (f);

  f = get_string_resource("passwd.dateFont", "Dialog.Font");
  pw->date_font = XLoadQueryFont (si->dpy, (f ? f : "fixed"));
  if (!pw->date_font) pw->date_font = XLoadQueryFont (si->dpy, "fixed");
  if (f) free (f);

  pw->foreground = get_pixel_resource ("passwd.foreground",
				       "Dialog.Foreground",
				       si->dpy, cmap);
  pw->background = get_pixel_resource ("passwd.background",
				       "Dialog.Background",
				       si->dpy, cmap);

  if (pw->foreground == pw->background)
    {
      /* Make sure the error messages show up. */
      pw->foreground = BlackPixelOfScreen (screen);
      pw->background = WhitePixelOfScreen (screen);
    }

  pw->passwd_foreground = get_pixel_resource ("passwd.text.foreground",
					      "Dialog.Text.Foreground",
					      si->dpy, cmap);
  pw->passwd_background = get_pixel_resource ("passwd.text.background",
					      "Dialog.Text.Background",
					      si->dpy, cmap);
  pw->thermo_foreground = get_pixel_resource ("passwd.thermometer.foreground",
					      "Dialog.Thermometer.Foreground",
					      si->dpy, cmap);
  pw->thermo_background = get_pixel_resource ("passwd.thermometer.background",
					      "Dialog.Thermometer.Background",
					      si->dpy, cmap);
  pw->shadow_top = get_pixel_resource ("passwd.topShadowColor",
				       "Dialog.Foreground",
				       si->dpy, cmap);
  pw->shadow_bottom = get_pixel_resource ("passwd.bottomShadowColor",
					  "Dialog.Background",
					  si->dpy, cmap);

  pw->logo_width = get_integer_resource ("passwd.logo.width",
					 "Dialog.Logo.Width");
  pw->logo_height = get_integer_resource ("passwd.logo.height",
					  "Dialog.Logo.Height");
  pw->thermo_width = get_integer_resource ("passwd.thermometer.width",
					   "Dialog.Thermometer.Width");
  pw->internal_border = get_integer_resource ("passwd.internalBorderWidth",
					      "Dialog.InternalBorderWidth");
  pw->shadow_width = get_integer_resource ("passwd.shadowThickness",
					   "Dialog.ShadowThickness");

  if (pw->logo_width == 0)  pw->logo_width = 150;
  if (pw->logo_height == 0) pw->logo_height = 150;
  if (pw->internal_border == 0) pw->internal_border = 15;
  if (pw->shadow_width == 0) pw->shadow_width = 4;
  if (pw->thermo_width == 0) pw->thermo_width = pw->shadow_width;

  {
    int direction, ascent, descent;
    XCharStruct overall;

    pw->width = 0;
    pw->height = 0;

    /* Measure the heading_label. */
    XTextExtents (pw->heading_font,
		  pw->heading_label, strlen(pw->heading_label),
		  &direction, &ascent, &descent, &overall);
    if (overall.width > pw->width) pw->width = overall.width;
    pw->height += ascent + descent;

    /* Measure the body_label. */
    XTextExtents (pw->body_font,
		  pw->body_label, strlen(pw->body_label),
		  &direction, &ascent, &descent, &overall);
    if (overall.width > pw->width) pw->width = overall.width;
    pw->height += ascent + descent;

    {
      Dimension w2 = 0, w3 = 0;
      Dimension h2 = 0, h3 = 0;
      const char *passwd_string = "MMMMMMMMMMMM";

      /* Measure the user_label. */
      XTextExtents (pw->label_font,
		    pw->user_label, strlen(pw->user_label),
		    &direction, &ascent, &descent, &overall);
      if (overall.width > w2)  w2 = overall.width;
      h2 += ascent + descent;

      /* Measure the passwd_label. */
      XTextExtents (pw->label_font,
		    pw->passwd_label, strlen(pw->passwd_label),
		    &direction, &ascent, &descent, &overall);
      if (overall.width > w2)  w2 = overall.width;
      h2 += ascent + descent;

      /* Measure the user_string. */
      XTextExtents (pw->passwd_font,
		    pw->user_string, strlen(pw->user_string),
		    &direction, &ascent, &descent, &overall);
      overall.width += (pw->shadow_width * 4);
      ascent += (pw->shadow_width * 4);
      if (overall.width > w3)  w3 = overall.width;
      h3 += ascent + descent;

      /* Measure the (maximally-sized, dummy) passwd_string. */
      XTextExtents (pw->passwd_font,
		    passwd_string, strlen(passwd_string),
		    &direction, &ascent, &descent, &overall);
      overall.width += (pw->shadow_width * 4);
      ascent += (pw->shadow_width * 4);
      if (overall.width > w3)  w3 = overall.width;
      h3 += ascent + descent;

      w2 = w2 + w3 + (pw->shadow_width * 2);
      h2 = MAX (h2, h3);

      if (w2 > pw->width)  pw->width  = w2;
      pw->height += h2;
    }

    pw->width  += (pw->internal_border * 2);
    pw->height += (pw->internal_border * 4);

    pw->width += pw->thermo_width + (pw->shadow_width * 3);

    if (pw->logo_height > pw->height)
      pw->height = pw->logo_height;
    else if (pw->height > pw->logo_height)
      pw->logo_height = pw->height;

    pw->logo_width = pw->logo_height;

    pw->width += pw->logo_width;
  }

  attrmask |= CWOverrideRedirect; attrs.override_redirect = True;
  attrmask |= CWEventMask; attrs.event_mask = ExposureMask|KeyPressMask;

  /* We need to remember the mouse position and restore it afterward, or
     sometimes (perhaps only with Xinerama?) the mouse gets warped to
     inside the bounds of the lock dialog window.
   */
  {
    Window pointer_root, pointer_child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;
    pw->previous_mouse_x = 0;
    pw->previous_mouse_y = 0;
    if (XQueryPointer (si->dpy, RootWindowOfScreen (pw->prompt_screen->screen),
                       &pointer_root, &pointer_child,
                       &root_x, &root_y, &win_x, &win_y, &mask))
      {
        pw->previous_mouse_x = root_x;
        pw->previous_mouse_y = root_y;
        if (si->prefs.verbose_p)
          fprintf (stderr, "%s: %d: mouse is at %d,%d.\n",
                   blurb(), pw->prompt_screen->number,
                   pw->previous_mouse_x, pw->previous_mouse_y);
      }
    else if (si->prefs.verbose_p)
      fprintf (stderr, "%s: %d: unable to determine mouse position?\n",
               blurb(), pw->prompt_screen->number);
  }

  /* Figure out where on the desktop to place the window so that it will
     actually be visible; this takes into account virtual viewports as
     well as Xinerama. */
  {
    int x, y, w, h;
    get_screen_viewport (pw->prompt_screen, &x, &y, &w, &h,
                         pw->previous_mouse_x, pw->previous_mouse_y,
                         si->prefs.verbose_p);
    if (si->prefs.debug_p) w /= 2;
    pw->x = x + ((w + pw->width) / 2) - pw->width;
    pw->y = y + ((h + pw->height) / 2) - pw->height;
    if (pw->x < x) pw->x = x;
    if (pw->y < y) pw->y = y;
  }

  pw->border_width = get_integer_resource ("passwd.borderWidth",
                                           "Dialog.BorderWidth");

  si->passwd_dialog =
    XCreateWindow (si->dpy,
		   RootWindowOfScreen(screen),
		   pw->x, pw->y, pw->width, pw->height, pw->border_width,
		   DefaultDepthOfScreen (screen), InputOutput,
		   DefaultVisualOfScreen(screen),
		   attrmask, &attrs);
  XSetWindowBackground (si->dpy, si->passwd_dialog, pw->background);

  pw->logo_pixmap = xscreensaver_logo (si->dpy, si->passwd_dialog, cmap,
                                       pw->background, 
                                       &pw->logo_pixels, &pw->logo_npixels,
                                       0, True);

  /* Before mapping the window, save the bits that are underneath the
     rectangle the window will occlude.  When we lower the window, we
     restore these bits.  This works, because the running screenhack
     has already been sent SIGSTOP, so we know nothing else is drawing
     right now! */
  {
    XGCValues gcv;
    GC gc;
    pw->save_under = XCreatePixmap (si->dpy,
                                    pw->prompt_screen->screensaver_window,
                                    pw->width + (pw->border_width*2) + 1,
                                    pw->height + (pw->border_width*2) + 1,
                                    pw->prompt_screen->current_depth);
    gcv.function = GXcopy;
    gc = XCreateGC (si->dpy, pw->save_under, GCFunction, &gcv);
    XCopyArea (si->dpy, pw->prompt_screen->screensaver_window,
               pw->save_under, gc,
               pw->x - pw->border_width, pw->y - pw->border_width,
               pw->width + (pw->border_width*2) + 1,
               pw->height + (pw->border_width*2) + 1,
               0, 0);
    XFreeGC (si->dpy, gc);
  }

  XMapRaised (si->dpy, si->passwd_dialog);
  XSync (si->dpy, False);

  move_mouse_grab (si, si->passwd_dialog,
                   pw->prompt_screen->cursor,
                   pw->prompt_screen->number);
  undo_vp_motion (si);

  si->pw_data = pw;

  if (cmap)
    XInstallColormap (si->dpy, cmap);
  draw_passwd_window (si);
  XSync (si->dpy, False);
}


static void
draw_passwd_window (saver_info *si)
{
  passwd_dialog_data *pw = si->pw_data;
  XGCValues gcv;
  GC gc1, gc2;
  int spacing, height;
  int x1, x2, x3, y1, y2;
  int sw;
  int tb_height;

  height = (pw->heading_font->ascent + pw->heading_font->descent +
	    pw->body_font->ascent + pw->body_font->descent +
	    (2 * MAX ((pw->label_font->ascent + pw->label_font->descent),
		      (pw->passwd_font->ascent + pw->passwd_font->descent +
		       (pw->shadow_width * 4)))) +
            pw->date_font->ascent + pw->date_font->descent
            );
  spacing = ((pw->height - (2 * pw->shadow_width) -
	      pw->internal_border - height)) / 8;
  if (spacing < 0) spacing = 0;

  gcv.foreground = pw->foreground;
  gc1 = XCreateGC (si->dpy, si->passwd_dialog, GCForeground, &gcv);
  gc2 = XCreateGC (si->dpy, si->passwd_dialog, GCForeground, &gcv);
  x1 = pw->logo_width + pw->thermo_width + (pw->shadow_width * 3);
  x3 = pw->width - (pw->shadow_width * 2);
  y1 = (pw->shadow_width * 2) + spacing + spacing;

  /* top heading
   */
  XSetFont (si->dpy, gc1, pw->heading_font->fid);
  sw = string_width (pw->heading_font, pw->heading_label);
  x2 = (x1 + ((x3 - x1 - sw) / 2));
  y1 += spacing + pw->heading_font->ascent + pw->heading_font->descent;
  XDrawString (si->dpy, si->passwd_dialog, gc1, x2, y1,
	       pw->heading_label, strlen(pw->heading_label));

  /* text below top heading
   */
  XSetFont (si->dpy, gc1, pw->body_font->fid);
  y1 += spacing + pw->body_font->ascent + pw->body_font->descent;
  sw = string_width (pw->body_font, pw->body_label);
  x2 = (x1 + ((x3 - x1 - sw) / 2));
  XDrawString (si->dpy, si->passwd_dialog, gc1, x2, y1,
	       pw->body_label, strlen(pw->body_label));


  tb_height = (pw->passwd_font->ascent + pw->passwd_font->descent +
	       (pw->shadow_width * 4));

  /* the "User:" prompt
   */
  y1 += spacing;
  y2 = y1;
  XSetForeground (si->dpy, gc1, pw->foreground);
  XSetFont (si->dpy, gc1, pw->label_font->fid);
  y1 += (spacing + tb_height);
  x2 = (x1 + pw->internal_border +
	MAX(string_width (pw->label_font, pw->user_label),
	    string_width (pw->label_font, pw->passwd_label)));
  XDrawString (si->dpy, si->passwd_dialog, gc1,
	       x2 - string_width (pw->label_font, pw->user_label),
	       y1,
	       pw->user_label, strlen(pw->user_label));

  /* the "Password:" prompt
   */
  y1 += (spacing + tb_height);
  XDrawString (si->dpy, si->passwd_dialog, gc1,
	       x2 - string_width (pw->label_font, pw->passwd_label),
	       y1,
	       pw->passwd_label, strlen(pw->passwd_label));


  XSetForeground (si->dpy, gc2, pw->passwd_background);

  /* the "user name" text field
   */
  y1 = y2;
  XSetForeground (si->dpy, gc1, pw->passwd_foreground);
  XSetFont (si->dpy, gc1, pw->passwd_font->fid);
  y1 += (spacing + tb_height);
  x2 += (pw->shadow_width * 4);

  pw->passwd_field_width = x3 - x2 - pw->internal_border;
  pw->passwd_field_height = (pw->passwd_font->ascent +
			     pw->passwd_font->descent +
			     pw->shadow_width);

  XFillRectangle (si->dpy, si->passwd_dialog, gc2,
		  x2 - pw->shadow_width,
		  y1 - (pw->passwd_font->ascent + pw->passwd_font->descent),
		  pw->passwd_field_width, pw->passwd_field_height);
  XDrawString (si->dpy, si->passwd_dialog, gc1, x2, y1,
	       pw->user_string, strlen(pw->user_string));

  /* the "password" text field
   */
  y1 += (spacing + tb_height);

  pw->passwd_field_x = x2 - pw->shadow_width;
  pw->passwd_field_y = y1 - (pw->passwd_font->ascent +
			     pw->passwd_font->descent);

  /* The shadow around the text fields
   */
  y1 = y2;
  y1 += (spacing + (pw->shadow_width * 3));
  x1 = x2 - (pw->shadow_width * 2);
  x2 = pw->passwd_field_width + (pw->shadow_width * 2);
  y2 = pw->passwd_field_height + (pw->shadow_width * 2);

  draw_shaded_rectangle (si->dpy, si->passwd_dialog,
			 x1, y1, x2, y2,
			 pw->shadow_width,
			 pw->shadow_bottom, pw->shadow_top);

  y1 += (spacing + pw->passwd_font->ascent + pw->passwd_font->descent +
	 (pw->shadow_width * 4));
  draw_shaded_rectangle (si->dpy, si->passwd_dialog,
			 x1, y1, x2, y2,
			 pw->shadow_width,
			 pw->shadow_bottom, pw->shadow_top);


  /* The date, below the text fields
   */
  {
    char buf[100];
    time_t now = time ((time_t *) 0);
    struct tm *tm = localtime (&now);
    memset (buf, 0, sizeof(buf));
    strftime (buf, sizeof(buf)-1, pw->date_label, tm);

    XSetFont (si->dpy, gc1, pw->date_font->fid);
    y1 += pw->shadow_width;
    y1 += (spacing + tb_height);
    y1 += spacing/2;
    sw = string_width (pw->date_font, buf);
    x2 = x1 + x2 - sw;
    XDrawString (si->dpy, si->passwd_dialog, gc1, x2, y1, buf, strlen(buf));
  }


  /* The logo
   */
  x1 = pw->shadow_width * 6;
  y1 = pw->shadow_width * 6;
  x2 = pw->logo_width - (pw->shadow_width * 12);
  y2 = pw->logo_height - (pw->shadow_width * 12);

  if (pw->logo_pixmap)
    {
      Window root;
      int x, y;
      unsigned int w, h, bw, d;
      XGetGeometry (si->dpy, pw->logo_pixmap, &root, &x, &y, &w, &h, &bw, &d);
      XSetForeground (si->dpy, gc1, pw->foreground);
      XSetBackground (si->dpy, gc1, pw->background);
      if (d == 1)
        XCopyPlane (si->dpy, pw->logo_pixmap, si->passwd_dialog, gc1,
                    0, 0, w, h,
                    x1 + ((x2 - (int)w) / 2),
                    y1 + ((y2 - (int)h) / 2),
                    1);
      else
        XCopyArea (si->dpy, pw->logo_pixmap, si->passwd_dialog, gc1,
                   0, 0, w, h,
                   x1 + ((x2 - (int)w) / 2),
                   y1 + ((y2 - (int)h) / 2));
    }

  /* The thermometer
   */
  XSetForeground (si->dpy, gc1, pw->thermo_foreground);
  XSetForeground (si->dpy, gc2, pw->thermo_background);

  pw->thermo_field_x = pw->logo_width + pw->shadow_width;
  pw->thermo_field_y = pw->shadow_width * 5;
  pw->thermo_field_height = pw->height - (pw->shadow_width * 10);

#if 0
  /* Solid border inside the logo box. */
  XSetForeground (si->dpy, gc1, pw->foreground);
  XDrawRectangle (si->dpy, si->passwd_dialog, gc1, x1, y1, x2-1, y2-1);
#endif

  /* The shadow around the logo
   */
  draw_shaded_rectangle (si->dpy, si->passwd_dialog,
			 pw->shadow_width * 4,
			 pw->shadow_width * 4,
			 pw->logo_width - (pw->shadow_width * 8),
			 pw->logo_height - (pw->shadow_width * 8),
			 pw->shadow_width,
			 pw->shadow_bottom, pw->shadow_top);

  /* The shadow around the thermometer
   */
  draw_shaded_rectangle (si->dpy, si->passwd_dialog,
			 pw->logo_width,
			 pw->shadow_width * 4,
			 pw->thermo_width + (pw->shadow_width * 2),
			 pw->height - (pw->shadow_width * 8),
			 pw->shadow_width,
			 pw->shadow_bottom, pw->shadow_top);

#if 1
  /* Solid border inside the thermometer. */
  XSetForeground (si->dpy, gc1, pw->foreground);
  XDrawRectangle (si->dpy, si->passwd_dialog, gc1, 
		  pw->thermo_field_x, pw->thermo_field_y,
                  pw->thermo_width - 1, pw->thermo_field_height - 1);
#endif

  /* The shadow around the whole window
   */
  draw_shaded_rectangle (si->dpy, si->passwd_dialog,
			 0, 0, pw->width, pw->height, pw->shadow_width,
			 pw->shadow_top, pw->shadow_bottom);

  XFreeGC (si->dpy, gc1);
  XFreeGC (si->dpy, gc2);

  update_passwd_window (si, pw->passwd_string, pw->ratio);
}


static void
update_passwd_window (saver_info *si, const char *printed_passwd, float ratio)
{
  passwd_dialog_data *pw = si->pw_data;
  XGCValues gcv;
  GC gc1, gc2;
  int x, y;
  XRectangle rects[1];

  pw->ratio = ratio;
  gcv.foreground = pw->passwd_foreground;
  gcv.font = pw->passwd_font->fid;
  gc1 = XCreateGC (si->dpy, si->passwd_dialog, GCForeground|GCFont, &gcv);
  gcv.foreground = pw->passwd_background;
  gc2 = XCreateGC (si->dpy, si->passwd_dialog, GCForeground, &gcv);

  if (printed_passwd)
    {
      char *s = strdup (printed_passwd);
      if (pw->passwd_string) free (pw->passwd_string);
      pw->passwd_string = s;
    }

  /* the "password" text field
   */
  rects[0].x =  pw->passwd_field_x;
  rects[0].y =  pw->passwd_field_y;
  rects[0].width = pw->passwd_field_width;
  rects[0].height = pw->passwd_field_height;

  XFillRectangle (si->dpy, si->passwd_dialog, gc2,
                  rects[0].x, rects[0].y, rects[0].width, rects[0].height);

  XSetClipRectangles (si->dpy, gc1, 0, 0, rects, 1, Unsorted);

  XDrawString (si->dpy, si->passwd_dialog, gc1,
               rects[0].x + pw->shadow_width,
               rects[0].y + (pw->passwd_font->ascent +
                             pw->passwd_font->descent),
               pw->passwd_string, strlen(pw->passwd_string));

  XSetClipMask (si->dpy, gc1, None);

  /* The I-beam
   */
  if (pw->i_beam != 0)
    {
      x = (rects[0].x + pw->shadow_width +
	   string_width (pw->passwd_font, pw->passwd_string));
      y = rects[0].y + pw->shadow_width;

      if (x > rects[0].x + rects[0].width - 1)
        x = rects[0].x + rects[0].width - 1;
      XDrawLine (si->dpy, si->passwd_dialog, gc1, 
		 x, y, x, y + pw->passwd_font->ascent);
    }

  pw->i_beam = (pw->i_beam + 1) % 4;


  /* the thermometer
   */
  y = (pw->thermo_field_height - 2) * (1.0 - pw->ratio);
  if (y > 0)
    {
      XFillRectangle (si->dpy, si->passwd_dialog, gc2,
		      pw->thermo_field_x + 1,
		      pw->thermo_field_y + 1,
		      pw->thermo_width-2,
		      y);
      XSetForeground (si->dpy, gc1, pw->thermo_foreground);
      XFillRectangle (si->dpy, si->passwd_dialog, gc1,
		      pw->thermo_field_x + 1,
		      pw->thermo_field_y + 1 + y,
		      pw->thermo_width-2,
		      MAX (0, pw->thermo_field_height - y - 2));
    }

  XFreeGC (si->dpy, gc1);
  XFreeGC (si->dpy, gc2);
  XSync (si->dpy, False);
}


static void
destroy_passwd_window (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  passwd_dialog_data *pw = si->pw_data;
  saver_screen_info *ssi = pw->prompt_screen;
  Colormap cmap = DefaultColormapOfScreen (ssi->screen);
  Pixel black = BlackPixelOfScreen (ssi->screen);
  Pixel white = WhitePixelOfScreen (ssi->screen);
  XEvent event;

  memset (pw->typed_passwd, 0, sizeof(pw->typed_passwd));
  memset (pw->passwd_string, 0, strlen(pw->passwd_string));

  if (pw->timer)
    XtRemoveTimeOut (pw->timer);

  move_mouse_grab (si, RootWindowOfScreen (ssi->screen),
                   ssi->cursor, ssi->number);

  if (p->verbose_p)
    fprintf (stderr, "%s: %d: moving mouse back to %d,%d.\n",
             blurb(), ssi->number,
             pw->previous_mouse_x, pw->previous_mouse_y);

  XWarpPointer (si->dpy, None, RootWindowOfScreen (ssi->screen),
                0, 0, 0, 0,
                pw->previous_mouse_x, pw->previous_mouse_y);

  XSync (si->dpy, False);
  while (XCheckMaskEvent (si->dpy, PointerMotionMask, &event))
    if (p->verbose_p)
      fprintf (stderr, "%s: discarding MotionNotify event.\n", blurb());

  if (si->passwd_dialog)
    {
      XDestroyWindow (si->dpy, si->passwd_dialog);
      si->passwd_dialog = 0;
    }
  
  if (pw->save_under)
    {
      XGCValues gcv;
      GC gc;
      gcv.function = GXcopy;
      gc = XCreateGC (si->dpy, ssi->screensaver_window, GCFunction, &gcv);
      XCopyArea (si->dpy, pw->save_under,
                 ssi->screensaver_window, gc,
                 0, 0,
                 pw->width + (pw->border_width*2) + 1,
                 pw->height + (pw->border_width*2) + 1,
                 pw->x - pw->border_width, pw->y - pw->border_width);
      XFreePixmap (si->dpy, pw->save_under);
      pw->save_under = 0;
      XFreeGC (si->dpy, gc);
    }

  if (pw->heading_label) free (pw->heading_label);
  if (pw->body_label)    free (pw->body_label);
  if (pw->user_label)    free (pw->user_label);
  if (pw->passwd_label)  free (pw->passwd_label);
  if (pw->date_label)    free (pw->date_label);
  if (pw->user_string)   free (pw->user_string);
  if (pw->passwd_string) free (pw->passwd_string);

  if (pw->heading_font) XFreeFont (si->dpy, pw->heading_font);
  if (pw->body_font)    XFreeFont (si->dpy, pw->body_font);
  if (pw->label_font)   XFreeFont (si->dpy, pw->label_font);
  if (pw->passwd_font)  XFreeFont (si->dpy, pw->passwd_font);
  if (pw->date_font)    XFreeFont (si->dpy, pw->date_font);

  if (pw->foreground != black && pw->foreground != white)
    XFreeColors (si->dpy, cmap, &pw->foreground, 1, 0L);
  if (pw->background != black && pw->background != white)
    XFreeColors (si->dpy, cmap, &pw->background, 1, 0L);
  if (pw->passwd_foreground != black && pw->passwd_foreground != white)
    XFreeColors (si->dpy, cmap, &pw->passwd_foreground, 1, 0L);
  if (pw->passwd_background != black && pw->passwd_background != white)
    XFreeColors (si->dpy, cmap, &pw->passwd_background, 1, 0L);
  if (pw->thermo_foreground != black && pw->thermo_foreground != white)
    XFreeColors (si->dpy, cmap, &pw->thermo_foreground, 1, 0L);
  if (pw->thermo_background != black && pw->thermo_background != white)
    XFreeColors (si->dpy, cmap, &pw->thermo_background, 1, 0L);
  if (pw->shadow_top != black && pw->shadow_top != white)
    XFreeColors (si->dpy, cmap, &pw->shadow_top, 1, 0L);
  if (pw->shadow_bottom != black && pw->shadow_bottom != white)
    XFreeColors (si->dpy, cmap, &pw->shadow_bottom, 1, 0L);

  if (pw->logo_pixmap)
    XFreePixmap (si->dpy, pw->logo_pixmap);
  if (pw->logo_pixels)
    {
      if (pw->logo_npixels)
        XFreeColors (si->dpy, cmap, pw->logo_pixels, pw->logo_npixels, 0L);
      free (pw->logo_pixels);
      pw->logo_pixels = 0;
      pw->logo_npixels = 0;
    }

  if (pw->save_under)
    XFreePixmap (si->dpy, pw->save_under);

  if (cmap)
    XInstallColormap (si->dpy, cmap);

  memset (pw, 0, sizeof(*pw));
  free (pw);
  si->pw_data = 0;
}


#ifdef HAVE_XHPDISABLERESET
/* This function enables and disables the C-Sh-Reset hot-key, which
   normally resets the X server (logging out the logged-in user.)
   We don't want random people to be able to do that while the
   screen is locked.
 */
static void
hp_lock_reset (saver_info *si, Bool lock_p)
{
  static Bool hp_locked_p = False;

  /* Calls to XHPDisableReset and XHPEnableReset must be balanced,
     or BadAccess errors occur.  (It's ok for this to be global,
     since it affects the whole machine, not just the current screen.)
  */
  if (hp_locked_p == lock_p)
    return;

  if (lock_p)
    XHPDisableReset (si->dpy);
  else
    XHPEnableReset (si->dpy);
  hp_locked_p = lock_p;
}
#endif /* HAVE_XHPDISABLERESET */



/* This function enables and disables the C-Sh-F1 ... F12 hot-keys,
   which, on Linux systems, switches to another virtual console.
   We'd like the whole screen/keyboard to be locked, not just one
   virtual console, so this function disables that while the X
   screen is locked.

   Unfortunately, this doesn't work -- this ioctl only works when
   called by root, and we have disavowed our privileges long ago.
 */
#ifdef HAVE_VT_LOCKSWITCH
static void
linux_lock_vt_switch (saver_info *si, Bool lock_p)
{
  saver_preferences *p = &si->prefs;
  static Bool vt_locked_p = False;
  const char *dev_console = "/dev/console";
  int fd;

  if (lock_p == vt_locked_p)
    return;

  if (lock_p && !p->lock_vt_p)
    return;

  fd = open (dev_console, O_RDWR);
  if (fd < 0)
    {
      char buf [255];
      sprintf (buf, "%s: couldn't %s VTs: %s", blurb(),
	       (lock_p ? "lock" : "unlock"),
	       dev_console);
#if 1 /* #### doesn't work yet, so don't bother complaining */
      perror (buf);
#endif
      return;
    }

  if (ioctl (fd, (lock_p ? VT_LOCKSWITCH : VT_UNLOCKSWITCH)) == 0)
    {
      vt_locked_p = lock_p;

      if (p->verbose_p)
	fprintf (stderr, "%s: %s VTs\n", blurb(),
		 (lock_p ? "locked" : "unlocked"));
    }
  else
    {
      char buf [255];
      sprintf (buf, "%s: couldn't %s VTs: ioctl", blurb(),
	       (lock_p ? "lock" : "unlock"));
#if 0 /* #### doesn't work yet, so don't bother complaining */
      perror (buf);
#endif
    }

  close (fd);
}
#endif /* HAVE_VT_LOCKSWITCH */


/* This function enables and disables the C-Alt-Plus and C-Alt-Minus
   hot-keys, which normally change the resolution of the X server.
   We don't want people to be able to switch the server resolution
   while the screen is locked, because if they switch to a higher
   resolution, it could cause part of the underlying desktop to become
   exposed.
 */
#ifdef HAVE_XF86VMODE

static int ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error);
static Bool vp_got_error = False;

static void
xfree_lock_mode_switch (saver_info *si, Bool lock_p)
{
  static Bool any_mode_locked_p = False;
  saver_preferences *p = &si->prefs;
  int screen;
  int event, error;
  Bool status;
  XErrorHandler old_handler;

  if (any_mode_locked_p == lock_p)
    return;
  if (!XF86VidModeQueryExtension (si->dpy, &event, &error))
    return;

  for (screen = 0; screen < si->nscreens; screen++)
    {
      XSync (si->dpy, False);
      old_handler = XSetErrorHandler (ignore_all_errors_ehandler);
      status = XF86VidModeLockModeSwitch (si->dpy, screen, lock_p);
      XSync (si->dpy, False);
      XSetErrorHandler (old_handler);
      if (vp_got_error) status = False;

      if (status)
        any_mode_locked_p = lock_p;

      if (!status && (p->verbose_p || !lock_p))
        /* Only print this when verbose, or when we locked but can't unlock.
           I tried printing this message whenever it comes up, but
           mode-locking always fails if DontZoom is set in XF86Config. */
        fprintf (stderr, "%s: %d: unable to %s mode switching!\n",
                 blurb(), screen, (lock_p ? "lock" : "unlock"));
      else if (p->verbose_p)
        fprintf (stderr, "%s: %d: %s mode switching.\n",
                 blurb(), screen, (lock_p ? "locked" : "unlocked"));
    }
}

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  vp_got_error = True;
  return 0;
}

#endif /* HAVE_XF86VMODE */


/* If the viewport has been scrolled since the screen was blanked,
   then scroll it back to where it belongs.  This function only exists
   to patch over a very brief race condition.
 */
static void
undo_vp_motion (saver_info *si)
{
#ifdef HAVE_XF86VMODE
  saver_preferences *p = &si->prefs;
  int screen;
  int event, error;

  if (!XF86VidModeQueryExtension (si->dpy, &event, &error))
    return;

  for (screen = 0; screen < si->nscreens; screen++)
    {
      saver_screen_info *ssi = &si->screens[screen];
      int x, y;
      Bool status;

      if (ssi->blank_vp_x == -1 && ssi->blank_vp_y == -1)
        break;
      if (!XF86VidModeGetViewPort (si->dpy, screen, &x, &y))
        return;
      if (ssi->blank_vp_x == x && ssi->blank_vp_y == y)
        return;
    
      /* We're going to move the viewport.  The mouse has just been grabbed on
         (and constrained to, thus warped to) the password window, so it is no
         longer near the edge of the screen.  However, wait a bit anyway, just
         to make sure the server drains its last motion event, so that the
         screen doesn't continue to scroll after we've reset the viewport.
       */
      XSync (si->dpy, False);
      usleep (250000);  /* 1/4 second */
      XSync (si->dpy, False);

      status = XF86VidModeSetViewPort (si->dpy, screen,
                                       ssi->blank_vp_x, ssi->blank_vp_y);

      if (!status)
        fprintf (stderr,
                 "%s: %d: unable to move vp from (%d,%d) back to (%d,%d)!\n",
                 blurb(), screen, x, y, ssi->blank_vp_x, ssi->blank_vp_y);
      else if (p->verbose_p)
        fprintf (stderr,
                 "%s: %d: vp moved to (%d,%d); moved it back to (%d,%d).\n",
                 blurb(), screen, x, y, ssi->blank_vp_x, ssi->blank_vp_y);
    }
#endif /* HAVE_XF86VMODE */
}



/* Interactions
 */

static void
passwd_animate_timer (XtPointer closure, XtIntervalId *id)
{
  saver_info *si = (saver_info *) closure;
  int tick = 166;
  passwd_dialog_data *pw = si->pw_data;

  if (!pw) return;

  pw->ratio -= (1.0 / ((double) si->prefs.passwd_timeout / (double) tick));
  if (pw->ratio < 0)
    {
      pw->ratio = 0;
      if (pw->state == pw_read)
	pw->state = pw_time;
    }

  update_passwd_window (si, 0, pw->ratio);

  if (pw->state == pw_read)
    pw->timer = XtAppAddTimeOut (si->app, tick, passwd_animate_timer,
				 (XtPointer) si);
  else
    pw->timer = 0;

  idle_timer ((XtPointer) si, id);
}


static XComposeStatus *compose_status;

static void
handle_passwd_key (saver_info *si, XKeyEvent *event)
{
  saver_preferences *p = &si->prefs;
  passwd_dialog_data *pw = si->pw_data;
  int pw_size = sizeof (pw->typed_passwd) - 1;
  char *typed_passwd = pw->typed_passwd;
  char s[2];
  char *stars = 0;
  int i;
  int size = XLookupString (event, s, 1, 0, compose_status);

  if (size != 1) return;

  s[1] = 0;

  switch (*s)
    {
    case '\010': case '\177':				/* Backspace */
      if (!*typed_passwd)
	XBell (si->dpy, 0);
      else
	typed_passwd [strlen(typed_passwd)-1] = 0;
      break;

    case '\025': case '\030':				/* Erase line */
      memset (typed_passwd, 0, pw_size);
      break;

    case '\012': case '\015':				/* Enter */
      if (pw->state != pw_read)
	;  /* already done? */
      else if (typed_passwd[0] == 0)
	pw->state = pw_null;
      else
        {
          update_passwd_window (si, "Checking...", pw->ratio);
          XSync (si->dpy, False);
          if (passwd_valid_p (typed_passwd, p->verbose_p))
            pw->state = pw_ok;
          else
            pw->state = pw_fail;
          update_passwd_window (si, "", pw->ratio);
        }
      break;

    default:
      i = strlen (typed_passwd);
      if (i >= pw_size-1)
	XBell (si->dpy, 0);
      else
	{
	  typed_passwd [i] = *s;
	  typed_passwd [i+1] = 0;
	}
      break;
    }

  i = strlen(typed_passwd);
  stars = (char *) malloc(i+1);
  memset (stars, '*', i);
  stars[i] = 0;
  update_passwd_window (si, stars, pw->ratio);
  free (stars);
}


static void
passwd_event_loop (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  char *msg = 0;
  XEvent event;
  passwd_animate_timer ((XtPointer) si, 0);

  while (si->pw_data && si->pw_data->state == pw_read)
    {
      XtAppNextEvent (si->app, &event);
      if (event.xany.window == si->passwd_dialog && event.xany.type == Expose)
	draw_passwd_window (si);
      else if (event.xany.type == KeyPress)
	handle_passwd_key (si, &event.xkey);
      else
	XtDispatchEvent (&event);
    }

  switch (si->pw_data->state)
    {
    case pw_ok:   msg = 0; break;
    case pw_null: msg = ""; break;
    case pw_time: msg = "Timed out!"; break;
    default:      msg = "Sorry!"; break;
    }

  if (si->pw_data->state == pw_fail)
    si->unlock_failures++;

  if (p->verbose_p)
    switch (si->pw_data->state)
      {
      case pw_ok:
	fprintf (stderr, "%s: password correct.\n", blurb()); break;
      case pw_fail:
	fprintf (stderr, "%s: password incorrect!\n", blurb()); break;
      case pw_null:
      case pw_cancel:
	fprintf (stderr, "%s: password entry cancelled.\n", blurb()); break;
      case pw_time:
	fprintf (stderr, "%s: password entry timed out.\n", blurb()); break;
      default: break;
      }

#ifdef HAVE_SYSLOG
  if (si->pw_data->state == pw_fail)
    {
      /* If they typed a password (as opposed to just hitting return) and
	 the password was invalid, log it.
      */
      struct passwd *pw = getpwuid (getuid ());
      char *d = DisplayString (si->dpy);
      char *u = (pw->pw_name ? pw->pw_name : "???");
      int opt = 0;
      int fac = 0;

# ifdef LOG_PID
      opt = LOG_PID;
# endif

# if defined(LOG_AUTHPRIV)
      fac = LOG_AUTHPRIV;
# elif defined(LOG_AUTH)
      fac = LOG_AUTH;
# else
      fac = LOG_DAEMON;
# endif

      if (!d) d = "";
      openlog (progname, opt, fac);
      syslog (LOG_NOTICE, "FAILED LOGIN %d ON DISPLAY \"%s\", FOR \"%s\"",
	      si->unlock_failures, d, u);
      closelog ();
    }
#endif /* HAVE_SYSLOG */

  if (si->pw_data->state == pw_fail)
    XBell (si->dpy, False);

  if (si->pw_data->state == pw_ok && si->unlock_failures != 0)
    {
      if (si->unlock_failures == 1)
	fprintf (real_stderr,
		 "%s: WARNING: 1 failed attempt to unlock the screen.\n",
		 blurb());
      else
	fprintf (real_stderr,
		 "%s: WARNING: %d failed attempts to unlock the screen.\n",
		 blurb(), si->unlock_failures);
      fflush (real_stderr);

      si->unlock_failures = 0;
    }

  if (msg)
    {
      si->pw_data->i_beam = 0;
      update_passwd_window (si, msg, 0.0);
      XSync (si->dpy, False);
      sleep (1);

      /* Swallow all pending KeyPress/KeyRelease events. */
      {
	XEvent e;
	while (XCheckMaskEvent (si->dpy, KeyPressMask|KeyReleaseMask, &e))
	  ;
      }
    }
}


static void
handle_typeahead (saver_info *si)
{
  passwd_dialog_data *pw = si->pw_data;
  int i;
  if (!si->unlock_typeahead)
    return;

  i = strlen (si->unlock_typeahead);
  if (i >= sizeof(pw->typed_passwd) - 1)
    i = sizeof(pw->typed_passwd) - 1;

  memcpy (pw->typed_passwd, si->unlock_typeahead, i);
  pw->typed_passwd [i] = 0;

  memset (si->unlock_typeahead, '*', strlen(si->unlock_typeahead));
  si->unlock_typeahead[i] = 0;
  update_passwd_window (si, si->unlock_typeahead, pw->ratio);

  free (si->unlock_typeahead);
  si->unlock_typeahead = 0;
}


Bool
unlock_p (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  Bool status;

  raise_window (si, True, True, True);

  if (p->verbose_p)
    fprintf (stderr, "%s: prompting for password.\n", blurb());

  if (si->pw_data || si->passwd_dialog)
    destroy_passwd_window (si);

  make_passwd_window (si);

  compose_status = calloc (1, sizeof (*compose_status));

  handle_typeahead (si);
  passwd_event_loop (si);

  status = (si->pw_data->state == pw_ok);
  destroy_passwd_window (si);

  free (compose_status);
  compose_status = 0;

  return status;
}


void
set_locked_p (saver_info *si, Bool locked_p)
{
  si->locked_p = locked_p;

#ifdef HAVE_XHPDISABLERESET
  hp_lock_reset (si, locked_p);                 /* turn off/on C-Sh-Reset */
#endif
#ifdef HAVE_VT_LOCKSWITCH
  linux_lock_vt_switch (si, locked_p);          /* turn off/on C-Alt-F1 */
#endif
#ifdef HAVE_XF86VMODE
  xfree_lock_mode_switch (si, locked_p);        /* turn off/on C-Alt-Plus */
#endif

  store_saver_status (si);			/* store locked-p */
}


#else  /*  NO_LOCKING -- whole file */

void
set_locked_p (saver_info *si, Bool locked_p)
{
  if (locked_p) abort();
}

#endif /* !NO_LOCKING */
