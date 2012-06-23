/* fps, Copyright (c) 2001-2011 Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include "screenhackI.h"
#include "fpsI.h"

fps_state *
fps_init (Display *dpy, Window window)
{
  fps_state *st;
  const char *font;
  XFontStruct *f;

  if (! get_boolean_resource (dpy, "doFPS", "DoFPS"))
    return 0;

  st = (fps_state *) calloc (1, sizeof(*st));

  st->dpy = dpy;
  st->window = window;
  st->clear_p = get_boolean_resource (dpy, "fpsSolid", "FPSSolid");

  font = get_string_resource (dpy, "fpsFont", "Font");

  if (!font) font = "-*-courier-bold-r-normal-*-180-*";
  f = XLoadQueryFont (dpy, font);
  if (!f) f = XLoadQueryFont (dpy, "fixed");

  {
    XWindowAttributes xgwa;
    XGCValues gcv;
    XGetWindowAttributes (dpy, window, &xgwa);
    gcv.font = f->fid;
    gcv.foreground = 
      get_pixel_resource (st->dpy, xgwa.colormap, "foreground", "Foreground");
    st->draw_gc = XCreateGC (dpy, window, GCFont|GCForeground, &gcv);
    gcv.foreground =
      get_pixel_resource (st->dpy, xgwa.colormap, "background", "Background");
    st->erase_gc = XCreateGC (dpy, window, GCFont|GCForeground, &gcv);
  }

  st->font = f;
  st->x = 10;
  st->y = 10;
  if (get_boolean_resource (dpy, "fpsTop", "FPSTop"))
    /* don't leave a blank line in GL top-fps. */
    st->y = - (/*st->font->ascent +*/ st->font->descent + 10);

  strcpy (st->string, "FPS: ... ");

  return st;
}

void
fps_free (fps_state *st)
{
  if (st->draw_gc)  XFreeGC (st->dpy, st->draw_gc);
  if (st->erase_gc) XFreeGC (st->dpy, st->erase_gc);
  if (st->font) XFreeFont (st->dpy, st->font);
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
          int L = strlen (st->string);
          char *s = st->string + L;
          strcat (s, "\nDepth: ");
          sprintf (s + strlen(s), "%.1f", depth);
          L = strlen (s);
          /* Remove trailing ".0" in case depth is not a fraction. */
          if (s[L-2] == '.' && s[L-1] == '0')
            s[L-2] = 0;
        }
    }

  return st->last_fps;
}



/* Width (and optionally height) of the string in pixels.
 */
static int
string_width (XFontStruct *f, const char *c, int *height_ret)
{
  int x = 0;
  int max_w = 0;
  int h = f->ascent + f->descent;
  while (*c)
    {
      int cc = *((unsigned char *) c);
      if (*c == '\n')
        {
          if (x > max_w) max_w = x;
          x = 0;
          h += f->ascent + f->descent;
        }
      else
        x += (f->per_char
              ? f->per_char[cc-f->min_char_or_byte2].width
              : f->min_bounds.rbearing);
      c++;
    }
  if (x > max_w) max_w = x;
  if (height_ret) *height_ret = h;

  return max_w;
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
      w = string_width (st->font, string, &h);
      XFillRectangle (st->dpy, st->window, st->erase_gc,
                      x - st->font->descent,
                      y - lh,
                      w + 2*st->font->descent,
                      h + 2*st->font->descent);
    }

  /* draw the text */
  while (lines)
    {
      s = strchr (string, '\n');
      if (! s) s = string + strlen(string);
      XDrawString (st->dpy, st->window, st->draw_gc,
                   x, y, string, s - string);
      string = s;
      string++;
      lines--;
      y += lh;
    }
}
