/* fps, Copyright © 2001-2021 Jamie Zawinski <jwz@jwz.org>
 * Draw a frames-per-second display (Xlib and OpenGL).
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "screenhackI.h"
#include "xft.h"
#include "fpsI.h"

#include <time.h>

fps_state *
fps_init (Display *dpy, Window window)
{
  fps_state *st;
  const char *font;
  XftFont *f;
  Bool top_p;
  XWindowAttributes xgwa;
  XGCValues gcv;
  char *s;

  if (! get_boolean_resource (dpy, "doFPS", "DoFPS"))
    return 0;

  if (!strcasecmp (progname, "BSOD")) return 0;  /* Never worked right */

  top_p = get_boolean_resource (dpy, "fpsTop", "FPSTop");

  st = (fps_state *) calloc (1, sizeof(*st));

  st->dpy = dpy;
  st->window = window;
  st->clear_p = get_boolean_resource (dpy, "fpsSolid", "FPSSolid");

  font = get_string_resource (dpy, "fpsFont", "Font");

  XGetWindowAttributes (dpy, window, &xgwa);

  if (!font)
    font = "monospace bold 18";   /* also texfont.c */
  f = load_xft_font_retry (dpy, screen_number (xgwa.screen), font);
  if (!f) abort();

  s = get_string_resource (st->dpy, "foreground", "Foreground");
  if (!s) s = strdup ("white");
  XftColorAllocName (st->dpy, xgwa.visual, xgwa.colormap, s, &st->fg);
  free (s);
  st->xftdraw = XftDrawCreate (dpy, window, xgwa.visual, xgwa.colormap);
  gcv.foreground =
    get_pixel_resource (st->dpy, xgwa.colormap, "background", "Background");
  st->erase_gc = XCreateGC (dpy, window, GCForeground, &gcv);

  st->font = f;
  st->x = 10;
  st->y = 10;
  if (top_p)
    st->y = - (st->font->ascent + st->font->descent + 10);

  {
    XGlyphInfo overall;
    XftTextExtentsUtf8 (st->dpy, st->font, (FcChar8 *) "m", 1, &overall);
    st->em = overall.xOff;
  }

# ifdef HAVE_IPHONE
  /* Don't hide the FPS display under the iPhone X bezel.
     #### This is the worst of all possible ways to do this!  But how else?
     This magic number should catch iPhone X and larger, but unfortunately
     also catches iPads which do not have the stupid bezel.
   */
  if (xgwa.width >= 1218 || xgwa.height >= 1218)
    {
      st->x += 18;
      st->y += 18 * (top_p ? -1 : 1);
    }
# endif

  strcpy (st->string, "FPS: ... ");

  return st;
}

void
fps_free (fps_state *st)
{
  if (st->xftdraw) XftDrawDestroy (st->xftdraw);
  if (st->erase_gc) XFreeGC (st->dpy, st->erase_gc);
  if (st->font) XftFontClose (st->dpy, st->font);
  free (st);
}


void
fps_slept (fps_state *st, unsigned long usecs)
{
  st->slept += usecs;
}


double
fps_compute (fps_state *st, unsigned long polys, double depth)
{
  if (! st) return 0;  /* too early? */

  /* Every N frames (where N is approximately one second's worth of frames)
     check the wall clock.  We do this because checking the wall clock is
     a slow operation.
   */
  if (st->frame_count++ >= st->last_ifps)
    {
# ifdef GETTIMEOFDAY_TWO_ARGS
      struct timezone tzp;
      gettimeofday(&st->this_frame_end, &tzp);
# else
      gettimeofday(&st->this_frame_end);
# endif

      if (st->prev_frame_end.tv_sec == 0)
        st->prev_frame_end = st->this_frame_end;
    }

  /* If we've probed the wall-clock time, regenerate the string.
   */
  if (st->this_frame_end.tv_sec != st->prev_frame_end.tv_sec)
    {
      double uprev_frame_end = (st->prev_frame_end.tv_sec +
                                ((double) st->prev_frame_end.tv_usec
                                 * 0.000001));
      double uthis_frame_end = (st->this_frame_end.tv_sec +
                                ((double) st->this_frame_end.tv_usec
                                 * 0.000001));
      double fps = st->frame_count / (uthis_frame_end - uprev_frame_end);
      double idle = (((double) st->slept * 0.000001) /
                     (uthis_frame_end - uprev_frame_end));
      double load = 100 * (1 - idle);

      if (load < 0) load = 0;  /* well that's obviously nonsense... */

      st->prev_frame_end = st->this_frame_end;
      st->frame_count = 0;
      st->slept       = 0;
      st->last_ifps   = fps;
      st->last_fps    = fps;

      sprintf (st->string, (polys 
                            ? "FPS:   %.1f \nLoad:  %.1f%% "
                            : "FPS:  %.1f \nLoad: %.1f%% "),
               fps, load);

      if (polys > 0)
        {
          const char *s = "";
# if 0
          if      (polys >= (1024 * 1024)) polys >>= 20, s = "M";
          else if (polys >= 2048)          polys >>= 10, s = "K";
# endif

          strcat (st->string, "\nPolys: ");
          if (polys >= 1000000)
            sprintf (st->string + strlen(st->string), "%lu,%03lu,%03lu%s ",
                     (polys / 1000000), ((polys / 1000) % 1000),
                     (polys % 1000), s);
          else if (polys >= 1000)
            sprintf (st->string + strlen(st->string), "%lu,%03lu%s ",
                     (polys / 1000), (polys % 1000), s);
          else
            sprintf (st->string + strlen(st->string), "%lu%s ", polys, s);
        }

      if (depth >= 0.0)
        {
          const char *s = "";
          unsigned long ldepth = depth;
# if 0
          if (depth >= (1024 * 1024 * 1024))
            ldepth = depth / (1024 * 1024 * 1024), s = "G";
          else if (depth >= (1024 * 1024)) ldepth >>= 20, s = "M";
          else if (depth >= 2048)          ldepth >>= 10, s = "K";
# endif
          strcat (st->string, "\nDepth: ");
          if (ldepth >= 1000000000)
            sprintf (st->string + strlen(st->string),
                     "%lu,%03lu,%03lu,%03lu%s ",
                     (ldepth / 1000000000),
                     ((ldepth / 1000000) % 1000),
                     ((ldepth / 1000) % 1000),
                     (ldepth % 1000), s);
          else if (ldepth >= 1000000)
            sprintf (st->string + strlen(st->string), "%lu,%03lu,%03lu%s ",
                     (ldepth / 1000000), ((ldepth / 1000) % 1000),
                     (ldepth % 1000), s);
          else if (ldepth >= 1000)
            sprintf (st->string + strlen(st->string), "%lu,%03lu%s ",
                     (ldepth / 1000), (ldepth % 1000), s);
          else if (*s)
            sprintf (st->string + strlen(st->string), "%lu%s ",
                     ldepth, s);
          else
            {
              int L;
              sprintf (st->string + strlen(st->string), "%.1f", depth);
              L = strlen (st->string);
              /* Remove trailing ".0" in case depth is not a fraction. */
              if (st->string[L-2] == '.' && st->string[L-1] == '0')
                st->string[L-2] = 0;
            }
        }
    }

  return st->last_fps;
}


/* This function is used only in Xlib mode.  For GL mode, see glx/fps-gl.c.
 */
void
fps_draw (fps_state *st)
{
  XWindowAttributes xgwa;
  const char *string = st->string;
  const char *s;
  int x = st->x;
  int y = st->y;
  int lines = 1;
  int lh = st->font->ascent + st->font->descent;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);

  for (s = string; *s; s++) 
    if (*s == '\n') lines++;

  if (y < 0)
    y = -y + (lines-1) * lh;
  else
    y = xgwa.height - y;

  y -= lh * (lines-1) + st->font->descent;

  /* clear the background */
  if (st->clear_p)
    {
      int w, h;
      int olines = lines;
      const char *ostring = string;
      int maxw = 0;
      while (lines)
        {
          s = strchr (string, '\n');
          if (! s) s = string + strlen(string);
# if 0
          {
            XGlyphInfo overall;
            XftTextExtentsUtf8 (st->dpy, st->font, (FcChar8 *) string, 
                                s - string, &overall);
            w = overall.width - overall.x + st->em;
          }
# else
          /* Measuring the font is slow, let's just assume this will fit. */
          w = st->em * 12;   /* "Load: 100.0%" */
# endif
          if (w > maxw) maxw = w;
          string = s;
          string++;
          lines--;
        }
      w = maxw;
      h = olines * (st->font->ascent + st->font->descent);

      XFillRectangle (st->dpy, st->window, st->erase_gc,
                      x - st->font->descent,
                      y - lh,
                      w + 2*st->font->descent,
                      h + 2*st->font->descent);


      lines = olines;
      string = ostring;
    }

  /* draw the text */
  while (lines)
    {
      s = strchr (string, '\n');
      if (! s) s = string + strlen(string);
      XftDrawStringUtf8 (st->xftdraw, &st->fg, st->font,
                         x, y, (FcChar8 *) string, s - string);
      string = s;
      string++;
      lines--;
      y += lh;
    }
}
