/* xscreensaver, Copyright (c) 1991-2002 Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <X11/Intrinsic.h>

#include "xscreensaver.h"
#include "resources.h"

#undef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))

void
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


int
string_width (XFontStruct *font, char *s)
{
  int direction, ascent, descent;
  XCharStruct overall;
  XTextExtents (font, s, strlen(s), &direction, &ascent, &descent, &overall);
  return overall.width;
}


static void update_splash_window (saver_info *si);
static void draw_splash_window (saver_info *si);
static void destroy_splash_window (saver_info *si);
static void unsplash_timer (XtPointer closure, XtIntervalId *id);

static void do_demo (saver_info *si);
#ifdef PREFS_BUTTON
static void do_prefs (saver_info *si);
#endif /* PREFS_BUTTON */
static void do_help (saver_info *si);


struct splash_dialog_data {

  saver_screen_info *prompt_screen;
  XtIntervalId timer;

  Dimension width;
  Dimension height;

  char *heading_label;
  char *body_label;
  char *body2_label;
  char *demo_label;
#ifdef PREFS_BUTTON
  char *prefs_label;
#endif /* PREFS_BUTTON */
  char *help_label;

  XFontStruct *heading_font;
  XFontStruct *body_font;
  XFontStruct *button_font;

  Pixel foreground;
  Pixel background;
  Pixel button_foreground;
  Pixel button_background;
  Pixel shadow_top;
  Pixel shadow_bottom;

  Dimension logo_width;
  Dimension logo_height;
  Dimension internal_border;
  Dimension shadow_width;

  Dimension button_width, button_height;
  Dimension demo_button_x, demo_button_y;
#ifdef PREFS_BUTTON
  Dimension prefs_button_x, prefs_button_y;
#endif /* PREFS_BUTTON */
  Dimension help_button_x, help_button_y;

  Pixmap logo_pixmap;
  int logo_npixels;
  unsigned long *logo_pixels;

  int pressed;
};


void
make_splash_dialog (saver_info *si)
{
  int x, y, bw;
  XSetWindowAttributes attrs;
  unsigned long attrmask = 0;
  splash_dialog_data *sp;
  saver_screen_info *ssi;
  Colormap cmap;
  char *f;

  if (si->sp_data)
    return;
  if (!si->prefs.splash_p ||
      si->prefs.splash_duration <= 0)
    return;

  ssi = &si->screens[mouse_screen (si)];
  cmap = DefaultColormapOfScreen (ssi->screen);

  sp = (splash_dialog_data *) calloc (1, sizeof(*sp));
  sp->prompt_screen = ssi;

  sp->heading_label = get_string_resource ("splash.heading.label",
					   "Dialog.Label.Label");
  sp->body_label = get_string_resource ("splash.body.label",
					"Dialog.Label.Label");
  sp->body2_label = get_string_resource ("splash.body2.label",
					 "Dialog.Label.Label");
  sp->demo_label = get_string_resource ("splash.demo.label",
					"Dialog.Button.Label");
#ifdef PREFS_BUTTON
  sp->prefs_label = get_string_resource ("splash.prefs.label",
					"Dialog.Button.Label");
#endif /* PREFS_BUTTON */
  sp->help_label = get_string_resource ("splash.help.label",
					"Dialog.Button.Label");

  if (!sp->heading_label)
    sp->heading_label = strdup("ERROR: REESOURCES NOT INSTALLED CORRECTLY");
  if (!sp->body_label)
    sp->body_label = strdup("ERROR: REESOURCES NOT INSTALLED CORRECTLY");
  if (!sp->body2_label)
    sp->body2_label = strdup("ERROR: REESOURCES NOT INSTALLED CORRECTLY");
  if (!sp->demo_label) sp->demo_label = strdup("ERROR");
#ifdef PREFS_BUTTON
  if (!sp->prefs_label) sp->prefs_label = strdup("ERROR");
#endif /* PREFS_BUTTON */
  if (!sp->help_label) sp->help_label = strdup("ERROR");

  /* Put the version number in the label. */
  {
    char *s = (char *) malloc (strlen(sp->heading_label) + 20);
    sprintf(s, sp->heading_label, si->version);
    free (sp->heading_label);
    sp->heading_label = s;
  }

  f = get_string_resource ("splash.headingFont", "Dialog.Font");
  sp->heading_font = XLoadQueryFont (si->dpy, (f ? f : "fixed"));
  if (!sp->heading_font) sp->heading_font = XLoadQueryFont (si->dpy, "fixed");
  if (f) free (f);

  f = get_string_resource("splash.bodyFont", "Dialog.Font");
  sp->body_font = XLoadQueryFont (si->dpy, (f ? f : "fixed"));
  if (!sp->body_font) sp->body_font = XLoadQueryFont (si->dpy, "fixed");
  if (f) free (f);

  f = get_string_resource("splash.buttonFont", "Dialog.Font");
  sp->button_font = XLoadQueryFont (si->dpy, (f ? f : "fixed"));
  if (!sp->button_font) sp->button_font = XLoadQueryFont (si->dpy, "fixed");
  if (f) free (f);

  sp->foreground = get_pixel_resource ("splash.foreground",
				       "Dialog.Foreground",
				       si->dpy, cmap);
  sp->background = get_pixel_resource ("splash.background",
				       "Dialog.Background",
				       si->dpy, cmap);

  if (sp->foreground == sp->background)
    {
      /* Make sure the error messages show up. */
      sp->foreground = BlackPixelOfScreen (ssi->screen);
      sp->background = WhitePixelOfScreen (ssi->screen);
    }

  sp->button_foreground = get_pixel_resource ("splash.Button.foreground",
					      "Dialog.Button.Foreground",
					      si->dpy, cmap);
  sp->button_background = get_pixel_resource ("splash.Button.background",
					      "Dialog.Button.Background",
					      si->dpy, cmap);
  sp->shadow_top = get_pixel_resource ("splash.topShadowColor",
				       "Dialog.Foreground",
				       si->dpy, cmap);
  sp->shadow_bottom = get_pixel_resource ("splash.bottomShadowColor",
					  "Dialog.Background",
					  si->dpy, cmap);

  sp->logo_width = get_integer_resource ("splash.logo.width",
					 "Dialog.Logo.Width");
  sp->logo_height = get_integer_resource ("splash.logo.height",
					  "Dialog.Logo.Height");
  sp->internal_border = get_integer_resource ("splash.internalBorderWidth",
					      "Dialog.InternalBorderWidth");
  sp->shadow_width = get_integer_resource ("splash.shadowThickness",
					   "Dialog.ShadowThickness");

  if (sp->logo_width == 0)  sp->logo_width = 150;
  if (sp->logo_height == 0) sp->logo_height = 150;
  if (sp->internal_border == 0) sp->internal_border = 15;
  if (sp->shadow_width == 0) sp->shadow_width = 4;

  {
    int direction, ascent, descent;
    XCharStruct overall;

    sp->width = 0;
    sp->height = 0;

    /* Measure the heading_label. */
    XTextExtents (sp->heading_font,
		  sp->heading_label, strlen(sp->heading_label),
		  &direction, &ascent, &descent, &overall);
    if (overall.width > sp->width) sp->width = overall.width;
    sp->height += ascent + descent;

    /* Measure the body_label. */
    XTextExtents (sp->body_font,
		  sp->body_label, strlen(sp->body_label),
		  &direction, &ascent, &descent, &overall);
    if (overall.width > sp->width) sp->width = overall.width;
    sp->height += ascent + descent;

    /* Measure the body2_label. */
    XTextExtents (sp->body_font,
		  sp->body2_label, strlen(sp->body2_label),
		  &direction, &ascent, &descent, &overall);
    if (overall.width > sp->width) sp->width = overall.width;
    sp->height += ascent + descent;

    {
      Dimension w2 = 0, w3 = 0, w4 = 0;
      Dimension h2 = 0, h3 = 0, h4 = 0;

      /* Measure the Demo button. */
      XTextExtents (sp->button_font,
		    sp->demo_label, strlen(sp->demo_label),
		    &direction, &ascent, &descent, &overall);
      w2 = overall.width;
      h2 = ascent + descent;

#ifdef PREFS_BUTTON
      /* Measure the Prefs button. */
      XTextExtents (sp->button_font,
		    sp->prefs_label, strlen(sp->prefs_label),
		    &direction, &ascent, &descent, &overall);
      w3 = overall.width;
      h3 = ascent + descent;
#else  /* !PREFS_BUTTON */
      w3 = 0;
      h3 = 0;
#endif /* !PREFS_BUTTON */

      /* Measure the Help button. */
      XTextExtents (sp->button_font,
		    sp->help_label, strlen(sp->help_label),
		    &direction, &ascent, &descent, &overall);
      w4 = overall.width;
      h4 = ascent + descent;

      w2 = MAX(w2, w3); w2 = MAX(w2, w4);
      h2 = MAX(h2, h3); h2 = MAX(h2, h4);

      /* Add some horizontal padding inside the buttons. */
      w2 += ascent;

      w2 += ((ascent + descent) / 2) + (sp->shadow_width * 2);
      h2 += ((ascent + descent) / 2) + (sp->shadow_width * 2);

      sp->button_width = w2;
      sp->button_height = h2;

#ifdef PREFS_BUTTON
      w2 *= 3;
#else  /* !PREFS_BUTTON */
      w2 *= 2;
#endif /* !PREFS_BUTTON */

      w2 += ((ascent + descent) * 2);  /* for space between buttons */

      if (w2 > sp->width) sp->width = w2;
      sp->height += h2;
    }

    sp->width  += (sp->internal_border * 2);
    sp->height += (sp->internal_border * 3);

    if (sp->logo_height > sp->height)
      sp->height = sp->logo_height;
    else if (sp->height > sp->logo_height)
      sp->logo_height = sp->height;

    sp->logo_width = sp->logo_height;

    sp->width += sp->logo_width;
  }

  attrmask |= CWOverrideRedirect; attrs.override_redirect = True;
  attrmask |= CWEventMask;
  attrs.event_mask = (ExposureMask | ButtonPressMask | ButtonReleaseMask);

  {
    int sx, sy, w, h;
    int mouse_x = 0, mouse_y = 0;

    {
      Window pointer_root, pointer_child;
      int root_x, root_y, win_x, win_y;
      unsigned int mask;
      if (XQueryPointer (si->dpy,
                         RootWindowOfScreen (ssi->screen),
                         &pointer_root, &pointer_child,
                         &root_x, &root_y, &win_x, &win_y, &mask))
        {
          mouse_x = root_x;
          mouse_y = root_y;
        }
    }

    get_screen_viewport (ssi, &sx, &sy, &w, &h, mouse_x, mouse_y, False);
    if (si->prefs.debug_p) w /= 2;
    x = sx + (((w + sp->width)  / 2) - sp->width);
    y = sy + (((h + sp->height) / 2) - sp->height);
    if (x < sx) x = sx;
    if (y < sy) y = sy;
  }

  bw = get_integer_resource ("splash.borderWidth", "Dialog.BorderWidth");

  si->splash_dialog =
    XCreateWindow (si->dpy,
		   RootWindowOfScreen(ssi->screen),
		   x, y, sp->width, sp->height, bw,
		   DefaultDepthOfScreen (ssi->screen), InputOutput,
		   DefaultVisualOfScreen(ssi->screen),
		   attrmask, &attrs);
  XSetWindowBackground (si->dpy, si->splash_dialog, sp->background);

  sp->logo_pixmap = xscreensaver_logo (si->dpy, si->splash_dialog, cmap,
                                       sp->background, 
                                       &sp->logo_pixels, &sp->logo_npixels,
                                       0, True);

  XMapRaised (si->dpy, si->splash_dialog);
  XSync (si->dpy, False);

  si->sp_data = sp;

  sp->timer = XtAppAddTimeOut (si->app, si->prefs.splash_duration,
			       unsplash_timer, (XtPointer) si);

  draw_splash_window (si);
  XSync (si->dpy, False);
}


static void
draw_splash_window (saver_info *si)
{
  splash_dialog_data *sp = si->sp_data;
  XGCValues gcv;
  GC gc1, gc2;
  int hspacing, vspacing, height;
  int x1, x2, x3, y1, y2;
  int sw;

#ifdef PREFS_BUTTON
  int nbuttons = 3;
#else  /* !PREFS_BUTTON */
  int nbuttons = 2;
#endif /* !PREFS_BUTTON */

  height = (sp->heading_font->ascent + sp->heading_font->descent +
	    sp->body_font->ascent + sp->body_font->descent +
	    sp->body_font->ascent + sp->body_font->descent +
	    sp->button_font->ascent + sp->button_font->descent);
  vspacing = ((sp->height
	       - (4 * sp->shadow_width)
	       - (2 * sp->internal_border)
	       - height) / 5);
  if (vspacing < 0) vspacing = 0;
  if (vspacing > (sp->heading_font->ascent * 2))
    vspacing = (sp->heading_font->ascent * 2);
	    
  gcv.foreground = sp->foreground;
  gc1 = XCreateGC (si->dpy, si->splash_dialog, GCForeground, &gcv);
  gc2 = XCreateGC (si->dpy, si->splash_dialog, GCForeground, &gcv);
  x1 = sp->logo_width;
  x3 = sp->width - (sp->shadow_width * 2);
  y1 = sp->internal_border;

  /* top heading
   */
  XSetFont (si->dpy, gc1, sp->heading_font->fid);
  sw = string_width (sp->heading_font, sp->heading_label);
  x2 = (x1 + ((x3 - x1 - sw) / 2));
  y1 += sp->heading_font->ascent;
  XDrawString (si->dpy, si->splash_dialog, gc1, x2, y1,
	       sp->heading_label, strlen(sp->heading_label));
  y1 += sp->heading_font->descent;

  /* text below top heading
   */
  XSetFont (si->dpy, gc1, sp->body_font->fid);
  y1 += vspacing + sp->body_font->ascent;
  sw = string_width (sp->body_font, sp->body_label);
  x2 = (x1 + ((x3 - x1 - sw) / 2));
  XDrawString (si->dpy, si->splash_dialog, gc1, x2, y1,
	       sp->body_label, strlen(sp->body_label));
  y1 += sp->body_font->descent;

  y1 += sp->body_font->ascent;
  sw = string_width (sp->body_font, sp->body2_label);
  x2 = (x1 + ((x3 - x1 - sw) / 2));
  XDrawString (si->dpy, si->splash_dialog, gc1, x2, y1,
	       sp->body2_label, strlen(sp->body2_label));
  y1 += sp->body_font->descent;

  /* The buttons
   */
  XSetForeground (si->dpy, gc1, sp->button_foreground);
  XSetForeground (si->dpy, gc2, sp->button_background);

/*  y1 += (vspacing * 2);*/
  y1 = sp->height - sp->internal_border - sp->button_height;

  x1 += sp->internal_border;
  y2 = (y1 + ((sp->button_height -
	       (sp->button_font->ascent + sp->button_font->descent))
	      / 2)
	+ sp->button_font->ascent);
  hspacing = ((sp->width - x1 - (sp->shadow_width * 2) -
	       sp->internal_border - (sp->button_width * nbuttons))
	      / 2);

  x2 = x1 + ((sp->button_width - string_width(sp->button_font, sp->demo_label))
	     / 2);
  XFillRectangle (si->dpy, si->splash_dialog, gc2, x1, y1,
		  sp->button_width, sp->button_height);
  XDrawString (si->dpy, si->splash_dialog, gc1, x2, y2,
	       sp->demo_label, strlen(sp->demo_label));
  sp->demo_button_x = x1;
  sp->demo_button_y = y1;
  
#ifdef PREFS_BUTTON
  x1 += hspacing + sp->button_width;
  x2 = x1 + ((sp->button_width - string_width(sp->button_font,sp->prefs_label))
	     / 2);
  XFillRectangle (si->dpy, si->splash_dialog, gc2, x1, y1,
		  sp->button_width, sp->button_height);
  XDrawString (si->dpy, si->splash_dialog, gc1, x2, y2,
	       sp->prefs_label, strlen(sp->prefs_label));
  sp->prefs_button_x = x1;
  sp->prefs_button_y = y1;
#endif /* PREFS_BUTTON */

#ifdef PREFS_BUTTON
  x1 += hspacing + sp->button_width;
#else  /* !PREFS_BUTTON */
  x1 = (sp->width - sp->button_width -
        sp->internal_border - (sp->shadow_width * 2));
#endif /* !PREFS_BUTTON */

  x2 = x1 + ((sp->button_width - string_width(sp->button_font,sp->help_label))
	     / 2);
  XFillRectangle (si->dpy, si->splash_dialog, gc2, x1, y1,
		  sp->button_width, sp->button_height);
  XDrawString (si->dpy, si->splash_dialog, gc1, x2, y2,
	       sp->help_label, strlen(sp->help_label));
  sp->help_button_x = x1;
  sp->help_button_y = y1;


  /* The logo
   */
  x1 = sp->shadow_width * 6;
  y1 = sp->shadow_width * 6;
  x2 = sp->logo_width - (sp->shadow_width * 12);
  y2 = sp->logo_height - (sp->shadow_width * 12);

  if (sp->logo_pixmap)
    {
      Window root;
      int x, y;
      unsigned int w, h, bw, d;
      XGetGeometry (si->dpy, sp->logo_pixmap, &root, &x, &y, &w, &h, &bw, &d);
      XSetForeground (si->dpy, gc1, sp->foreground);
      XSetBackground (si->dpy, gc1, sp->background);
      if (d == 1)
        XCopyPlane (si->dpy, sp->logo_pixmap, si->splash_dialog, gc1,
                    0, 0, w, h,
                    x1 + ((x2 - (int)w) / 2),
                    y1 + ((y2 - (int)h) / 2),
                    1);
      else
        XCopyArea (si->dpy, sp->logo_pixmap, si->splash_dialog, gc1,
                   0, 0, w, h,
                   x1 + ((x2 - (int)w) / 2),
                   y1 + ((y2 - (int)h) / 2));
    }

  /* Solid border inside the logo box. */
#if 0
  XSetForeground (si->dpy, gc1, sp->foreground);
  XDrawRectangle (si->dpy, si->splash_dialog, gc1, x1, y1, x2-1, y2-1);
#endif

  /* The shadow around the logo
   */
  draw_shaded_rectangle (si->dpy, si->splash_dialog,
			 sp->shadow_width * 4,
			 sp->shadow_width * 4,
			 sp->logo_width - (sp->shadow_width * 8),
			 sp->logo_height - (sp->shadow_width * 8),
			 sp->shadow_width,
			 sp->shadow_bottom, sp->shadow_top);

  /* The shadow around the whole window
   */
  draw_shaded_rectangle (si->dpy, si->splash_dialog,
			 0, 0, sp->width, sp->height, sp->shadow_width,
			 sp->shadow_top, sp->shadow_bottom);

  XFreeGC (si->dpy, gc1);
  XFreeGC (si->dpy, gc2);

  update_splash_window (si);
}


static void
update_splash_window (saver_info *si)
{
  splash_dialog_data *sp = si->sp_data;
  int pressed;
  if (!sp) return;
  pressed = sp->pressed;

  /* The shadows around the buttons
   */
  draw_shaded_rectangle (si->dpy, si->splash_dialog,
			 sp->demo_button_x, sp->demo_button_y,
			 sp->button_width, sp->button_height, sp->shadow_width,
			 (pressed == 1 ? sp->shadow_bottom : sp->shadow_top),
			 (pressed == 1 ? sp->shadow_top : sp->shadow_bottom));
#ifdef PREFS_BUTTON
  draw_shaded_rectangle (si->dpy, si->splash_dialog,
			 sp->prefs_button_x, sp->prefs_button_y,
			 sp->button_width, sp->button_height, sp->shadow_width,
			 (pressed == 2 ? sp->shadow_bottom : sp->shadow_top),
			 (pressed == 2 ? sp->shadow_top : sp->shadow_bottom));
#endif /* PREFS_BUTTON */
  draw_shaded_rectangle (si->dpy, si->splash_dialog,
			 sp->help_button_x, sp->help_button_y,
			 sp->button_width, sp->button_height, sp->shadow_width,
			 (pressed == 3 ? sp->shadow_bottom : sp->shadow_top),
			 (pressed == 3 ? sp->shadow_top : sp->shadow_bottom));
}

static void
destroy_splash_window (saver_info *si)
{
  splash_dialog_data *sp = si->sp_data;
  saver_screen_info *ssi = sp->prompt_screen;
  Colormap cmap = DefaultColormapOfScreen (ssi->screen);
  Pixel black = BlackPixelOfScreen (ssi->screen);
  Pixel white = WhitePixelOfScreen (ssi->screen);

  if (sp->timer)
    XtRemoveTimeOut (sp->timer);

  if (si->splash_dialog)
    {
      XDestroyWindow (si->dpy, si->splash_dialog);
      si->splash_dialog = 0;
    }
  
  if (sp->heading_label) free (sp->heading_label);
  if (sp->body_label)    free (sp->body_label);
  if (sp->body2_label)   free (sp->body2_label);
  if (sp->demo_label)    free (sp->demo_label);
#ifdef PREFS_BUTTON
  if (sp->prefs_label)   free (sp->prefs_label);
#endif /* PREFS_BUTTON */
  if (sp->help_label)    free (sp->help_label);

  if (sp->heading_font) XFreeFont (si->dpy, sp->heading_font);
  if (sp->body_font)    XFreeFont (si->dpy, sp->body_font);
  if (sp->button_font)  XFreeFont (si->dpy, sp->button_font);

  if (sp->foreground != black && sp->foreground != white)
    XFreeColors (si->dpy, cmap, &sp->foreground, 1, 0L);
  if (sp->background != black && sp->background != white)
    XFreeColors (si->dpy, cmap, &sp->background, 1, 0L);
  if (sp->button_foreground != black && sp->button_foreground != white)
    XFreeColors (si->dpy, cmap, &sp->button_foreground, 1, 0L);
  if (sp->button_background != black && sp->button_background != white)
    XFreeColors (si->dpy, cmap, &sp->button_background, 1, 0L);
  if (sp->shadow_top != black && sp->shadow_top != white)
    XFreeColors (si->dpy, cmap, &sp->shadow_top, 1, 0L);
  if (sp->shadow_bottom != black && sp->shadow_bottom != white)
    XFreeColors (si->dpy, cmap, &sp->shadow_bottom, 1, 0L);

  if (sp->logo_pixmap)
    XFreePixmap (si->dpy, sp->logo_pixmap);
  if (sp->logo_pixels)
    {
      if (sp->logo_npixels)
        XFreeColors (si->dpy, cmap, sp->logo_pixels, sp->logo_npixels, 0L);
      free (sp->logo_pixels);
      sp->logo_pixels = 0;
      sp->logo_npixels = 0;
    }

  memset (sp, 0, sizeof(*sp));
  free (sp);
  si->sp_data = 0;
}

void
handle_splash_event (saver_info *si, XEvent *event)
{
  splash_dialog_data *sp = si->sp_data;
  int which = 0;
  if (!sp) return;

  switch (event->xany.type)
    {
    case Expose:
      draw_splash_window (si);
      break;

    case ButtonPress: case ButtonRelease:

      if (event->xbutton.x >= sp->demo_button_x &&
	  event->xbutton.x < sp->demo_button_x + sp->button_width &&
	  event->xbutton.y >= sp->demo_button_y &&
	  event->xbutton.y < sp->demo_button_y + sp->button_height)
	which = 1;

#ifdef PREFS_BUTTON
      else if (event->xbutton.x >= sp->prefs_button_x &&
	       event->xbutton.x < sp->prefs_button_x + sp->button_width &&
	       event->xbutton.y >= sp->prefs_button_y &&
	       event->xbutton.y < sp->prefs_button_y + sp->button_height)
	which = 2;
#endif /* PREFS_BUTTON */

      else if (event->xbutton.x >= sp->help_button_x &&
	       event->xbutton.x < sp->help_button_x + sp->button_width &&
	       event->xbutton.y >= sp->help_button_y &&
	       event->xbutton.y < sp->help_button_y + sp->button_height)
	which = 3;

      if (event->xany.type == ButtonPress)
	{
	  sp->pressed = which;
	  update_splash_window (si);
	  if (which == 0)
	    XBell (si->dpy, False);
	}
      else if (event->xany.type == ButtonRelease)
	{
	  if (which && sp->pressed == which)
	    {
	      destroy_splash_window (si);
              sp = si->sp_data;
	      switch (which)
		{
		case 1: do_demo (si); break;
#ifdef PREFS_BUTTON
		case 2: do_prefs (si); break;
#endif /* PREFS_BUTTON */
		case 3: do_help (si); break;
		default: abort();
		}
	    }
          else if (which == 0 && sp->pressed == 0)
            {
              /* click and release on the window but not in a button:
                 treat that as "dismiss the splash dialog." */
	      destroy_splash_window (si);
              sp = si->sp_data;
            }
	  if (sp) sp->pressed = 0;
	  update_splash_window (si);
	}
      break;

    default:
      break;
    }
}

static void
unsplash_timer (XtPointer closure, XtIntervalId *id)
{
  saver_info *si = (saver_info *) closure;
  if (si && si->sp_data)
    destroy_splash_window (si);
}


/* Button callbacks */

#ifdef VMS
# define pid_t int
# define fork  vfork
#endif /* VMS */

static void
fork_and_exec (saver_info *si, const char *command, const char *desc)
{
  saver_preferences *p = &si->prefs;
  pid_t forked;
  char buf [512];
  char *av[5];
  int ac;

  if (!command || !*command)
    {
      fprintf (stderr, "%s: no %s command has been specified.\n",
	       blurb(), desc);
      return;
    }

  switch ((int) (forked = fork ()))
    {
    case -1:
      sprintf (buf, "%s: couldn't fork", blurb());
      perror (buf);
      break;

    case 0:
      close (ConnectionNumber (si->dpy));		/* close display fd */
      hack_subproc_environment (si->default_screen);	/* set $DISPLAY */
      ac = 0;
      av [ac++] = (char *) p->shell;
      av [ac++] = (char *) "-c";
      av [ac++] = (char *) command;
      av [ac]   = 0;
      execvp (av[0], av);				/* shouldn't return. */

      sprintf (buf, "%s: execvp(\"%s\", \"%s\", \"%s\") failed",
	       blurb(), av[0], av[1], av[2]);
      perror (buf);
      fflush (stderr);
      fflush (stdout);
      exit (1);			 /* Note that this only exits a child fork.  */
      break;

    default:
      /* parent fork. */
      break;
    }
}


static void
do_demo (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  fork_and_exec (si, p->demo_command, "demo-mode");
}

#ifdef PREFS_BUTTON
static void
do_prefs (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  fork_and_exec (si, p->prefs_command, "preferences");
}
#endif /* PREFS_BUTTON */

static void
do_help (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  char *help_command;

  if (!p->help_url || !*p->help_url)
    {
      fprintf (stderr, "%s: no Help URL has been specified.\n", blurb());
      return;
    }

  help_command = (char *) malloc (strlen (p->load_url_command) +
				  (strlen (p->help_url) * 2) + 10);
  sprintf (help_command, p->load_url_command, p->help_url, p->help_url);
  fork_and_exec (si, help_command, "URL-loading");
  free (help_command);
}
