/* filmleader, Copyright © 2018-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Simulate an SMPTE Universal Film Leader playing on an analog television.
 */

#include "screenhack.h"
#include "analogtv.h"
#include "doubletime.h"

#include <time.h>

struct state {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  int w, h;
  unsigned long bg, text_color, ring_color, trace_color;
  XftColor xft_text_color_1, xft_text_color_2;

  XftFont *font, *font2, *font3;
  XftDraw *xftdraw;
  Pixmap pix;
  GC gc;
  double start, last_time;
  double value;
  int stop;
  double noise;
  analogtv *tv;
  analogtv_input *inp;
  analogtv_reception rec;
  Bool button_down_p;
};


static void *
filmleader_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  char *s;

  st->dpy = dpy;
  st->window = window;
  st->tv = analogtv_allocate (st->dpy, st->window);
  analogtv_set_defaults (st->tv, "");
  st->tv->need_clear = 1;
  st->inp = analogtv_input_allocate();
  analogtv_setup_sync (st->inp, 1, 0);
  st->rec.input = st->inp;
  st->rec.level = 2.0;
  st->tv->use_color = 1;
  st->rec.level = pow(frand(1.0), 3.0) * 2.0 + 0.05;
  st->rec.ofs = random() % ANALOGTV_SIGNAL_LEN;
  st->tv->powerup = 0;

  st->rec.multipath = 0.0;
  st->tv->color_control += frand(0.3);
  st->noise = get_float_resource (st->dpy, "noise", "Float");
  st->value = 18;  /* Leave time for powerup */
  st->stop = 2 + (random() % 5);
  XGetWindowAttributes (dpy, window, &st->xgwa);

  /* Let's render it into a 16:9 pixmap, since that's what most screens are
     these days.  That means the circle will be squashed on 4:3 screens. */
  {
    double r = 16/9.0;

# ifdef HAVE_MOBILE
    /* analogtv.c always fills whole screen on mobile, so use screen aspect. */
    r = st->xgwa.width / (double) st->xgwa.height;
    if (r < 1) r = 1/r;
# endif

    st->w = 712;
    st->h = st->w / r;
  }

  if (st->xgwa.width < st->xgwa.height)
    {
      int swap = st->w;
      st->w = st->h;
      st->h = swap;
    }

  st->pix = XCreatePixmap (dpy, window,
                           st->w > st->h ? st->w : st->h,
                           st->w > st->h ? st->w : st->h,
                           st->xgwa.depth);
  st->gc = XCreateGC (dpy, st->pix, 0, &gcv);

  st->xftdraw = XftDrawCreate (dpy, st->pix, st->xgwa.visual,
                               st->xgwa.colormap);
  s = get_string_resource (dpy, "numberFont", "Font");
  st->font = load_xft_font_retry (dpy, screen_number (st->xgwa.screen), s);
  if (s) free (s);
  s = get_string_resource (dpy, "numberFont2", "Font");
  st->font2 = load_xft_font_retry (dpy, screen_number (st->xgwa.screen), s);
  if (s) free (s);
  s = get_string_resource (dpy, "numberFont3", "Font");
  st->font3 = load_xft_font_retry (dpy, screen_number (st->xgwa.screen), s);
  if (s) free (s);

  st->bg = get_pixel_resource (dpy, st->xgwa.colormap,
                               "textBackground", "Background");
  st->text_color = get_pixel_resource (dpy, st->xgwa.colormap,
                                       "textColor", "Foreground");
  st->ring_color = get_pixel_resource (dpy, st->xgwa.colormap,
                                       "ringColor", "Foreground");
  st->trace_color = get_pixel_resource (dpy, st->xgwa.colormap,
                                        "traceColor", "Foreground");

  s = get_string_resource (dpy, "textColor", "Foreground");
  if (!s) s = strdup ("white");
  XftColorAllocName (dpy, st->xgwa.visual, st->xgwa.colormap, s,
                     &st->xft_text_color_1);
  free (s);

  s = get_string_resource (dpy, "textBackground", "Background");
  if (!s) s = strdup ("black");
  XftColorAllocName (dpy, st->xgwa.visual, st->xgwa.colormap, s,
                     &st->xft_text_color_2);
  if (s) free (s);

  return st;
}


static unsigned long
filmleader_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  const analogtv_reception *rec = &st->rec;
  double then = double_time(), now, timedelta;
  XImage *img;
  int i, x, y, w2, h2;
  XGlyphInfo extents;
  int lbearing, rbearing, ascent, descent;
  char s[20];
  double r = 1 - (st->value - (int) st->value);
  int ivalue = st->value;
  XftFont *xftfont;
  XftColor *xftcolor;

  const struct { double t; int k, f; const char * const s[4]; } blurbs[] = {
    {  9.1, 3, 1, { "PICTURE", "  START", 0, 0 }},
    { 10.0, 2, 1, { "    16", "SOUND", "START", 0 }},
    { 10.5, 2, 1, { "    32", "SOUND", "START", 0 }},
    { 11.6, 2, 0, { "PICTURE", "COMPANY", "SERIES", 0 }},
    { 11.7, 2, 0, { "XSCRNSAVER", 0, 0, 0 }},
    { 11.9, 2, 0, { "REEL No.", "PROD No.", "PLAY DATE", 0 }},
    { 12.2, 0, 0, { "    SMPTE     ", "UNIVERSAL", "   LEADER", 0 }},
    { 12.3, 0, 1, { "X           ", "X", "X", "X" }},
    { 12.4, 0, 0, { "    SMPTE     ", "UNIVERSAL", "   LEADER", 0 }},
    { 12.5, 3, 1, { "PICTURE", 0, 0, 0 }},
    { 12.7, 3, 1, { "HEAD", 0, 0, 0 }},
    { 12.8, 2, 1, { "OOOO", 0, "ASPECT", "TYPE OF" }},
    { 12.9, 2, 0, { "SOUND", 0, "RATIO", 0 }},
    { 13.2, 1, 1, { "                  ", "PICTURE", 0, 0 }},
    { 13.3, 1, 0, { "REEL No.      ", "COLOR", 0, 0 }},
    { 13.4, 1, 0, { "LENGTH        ", 0, 0, "ROLL" }},
    { 13.5, 1, 0, { "SUBJECT", 0, 0, 0 }},
    { 13.9, 1, 1, { "     \342\206\221", "SPLICE", " HERE", 0 }},
  };

  for (i = 0; i < countof(blurbs); i++)
    {
      if (st->value >= blurbs[i].t && st->value <= blurbs[i].t + 1/15.0)
        {
          int line_height;
          int j;
          xftfont = (blurbs[i].f == 1 ? st->font2 :
                     blurbs[i].f == 2 ? st->font : st->font3);

          XSetForeground (dpy, st->gc, 
                          blurbs[i].k == 3 ? st->bg : st->text_color);
          XFillRectangle (dpy, st->pix, st->gc, 0, 0, st->w, st->h);
          XSetForeground (dpy, st->gc,
                          blurbs[i].k == 3 ? st->text_color : st->bg);
          xftcolor = (blurbs[i].k == 3 ? 
                      &st->xft_text_color_1 : &st->xft_text_color_2);

          /* The height of a string of spaces is 0... */
          XftTextExtentsUtf8 (dpy, xftfont, (FcChar8 *) "My", 2, &extents);
          line_height = extents.height;

          XftTextExtentsUtf8 (dpy, xftfont, (FcChar8 *)
                              blurbs[i].s[0], strlen(blurbs[i].s[0]),
                              &extents);
          /* lbearing = -extents.x; */
          rbearing = extents.width - extents.x;
          ascent   = extents.y;
          /* descent  = extents.height - extents.y; */

          x = (st->w - rbearing) / 2;
          y = st->h * 0.1 + ascent;

          for (j = 0; j < countof(blurbs[i].s); j++)
            {
              if (blurbs[i].s[j])
                XftDrawStringUtf8 (st->xftdraw, xftcolor, xftfont, x, y,
                                   (FcChar8 *) blurbs[i].s[j],
                                   strlen(blurbs[i].s[j]));

              y += line_height * 1.5;

              if (blurbs[i].s[j])
                {
                  XftTextExtentsUtf8 (dpy, xftfont, (FcChar8 *)
                                      blurbs[i].s[0], strlen(blurbs[i].s[j]),
                                      &extents);
                  /* lbearing = -extents.x; */
                  /* rbearing = extents.width - extents.x; */
                  /* ascent   = extents.y; */
                  /* descent  = extents.height - extents.y; */
                }
            }

          if (blurbs[i].k == 2)  /* Rotate clockwise and flip */
            {
              int wh = st->w < st->h ? st->w : st->h;
              XImage *img1 = XGetImage (dpy, st->pix,
                                        (st->w - wh) / 2,
                                        (st->h - wh) / 2,
                                        wh, wh, ~0L, ZPixmap);
              XImage *img2 = XCreateImage (dpy, st->xgwa.visual,
                                           st->xgwa.depth, ZPixmap, 0, 0,
                                           wh, wh, 32, 0);
              img2->data = malloc (img2->bytes_per_line * img2->height);
              for (y = 0; y < wh; y++)
                for (x = 0; x < wh; x++)
                  XPutPixel (img2, y, x, XGetPixel (img1, x, y));
              XSetForeground (dpy, st->gc, 
                              blurbs[i].k == 3 ? st->bg : st->text_color);
              XFillRectangle (dpy, st->pix, st->gc, 0, 0, st->w, st->h);
              XPutImage (dpy, st->pix, st->gc, img2,
                         0, 0,
                         (st->w - wh) / 2,
                         (st->h - wh) / 2,
                         wh, wh);
              XDestroyImage (img1);
              XDestroyImage (img2);
            }
          else if (blurbs[i].k == 1)  /* Flip vertically */
            {
              XImage *img1 = XGetImage (dpy, st->pix, 0, 0,
                                        st->w, st->h, ~0L, ZPixmap);
              XImage *img2 = XCreateImage (dpy, st->xgwa.visual,
                                           st->xgwa.depth, ZPixmap, 0, 0,
                                           st->w, st->h, 32, 0);
              img2->data = malloc (img2->bytes_per_line * img2->height);
              for (y = 0; y < img2->height; y++)
                for (x = 0; x < img2->width; x++)
                  XPutPixel (img2, x, img2->height-y-1,
                             XGetPixel (img1, x, y));
              XPutImage (dpy, st->pix, st->gc, img2, 0, 0, 0, 0, st->w, st->h);
              XDestroyImage (img1);
              XDestroyImage (img2);
            }

          goto DONE;
        }
    }

  if (st->value < 2.0 || st->value >= 9.0)	/* Black screen */
    {
      XSetForeground (dpy, st->gc, st->text_color);
      XFillRectangle (dpy, st->pix, st->gc, 0, 0, st->w, st->h);
      goto DONE;
    }

  XSetForeground (dpy, st->gc, st->bg);
  XFillRectangle (dpy, st->pix, st->gc, 0, 0, st->w, st->h);

  if (r > 1/30.0)				/* Sweep line and background */
    {
      x = st->w/2 + st->w * cos (M_PI * 2 * r - M_PI/2);
      y = st->h/2 + st->h * sin (M_PI * 2 * r - M_PI/2);

      XSetForeground (dpy, st->gc, st->trace_color);
      XFillArc (dpy, st->pix, st->gc,
                -st->w, -st->h, st->w*3, st->h*3,
                90*64,
                90*64 - ((r + 0.25) * 360*64));

      XSetForeground (dpy, st->gc, st->text_color);
      XSetLineAttributes (dpy, st->gc, 1, LineSolid, CapRound, JoinRound);
      XDrawLine (dpy, st->pix, st->gc, st->w/2, st->h/2, x, y);
  
      XSetForeground (dpy, st->gc, st->text_color);
      XSetLineAttributes (dpy, st->gc, 2, LineSolid, CapRound, JoinRound);
      XDrawLine (dpy, st->pix, st->gc, st->w/2, 0, st->w/2, st->h);
      XDrawLine (dpy, st->pix, st->gc, 0, st->h/2, st->w, st->h/2);
    }

  /* Big number */

  s[0] = (char) (ivalue + '0');
  xftfont = st->font;
  xftcolor = &st->xft_text_color_1;
  XftTextExtentsUtf8 (dpy, xftfont, (FcChar8 *) s, 1, &extents);
  lbearing = -extents.x;
  rbearing = extents.width - extents.x;
  ascent   = extents.y;
  descent  = extents.height - extents.y;

  x = (st->w - (rbearing + lbearing)) / 2;
  y = (st->h + (ascent - descent)) / 2;
  XftDrawStringUtf8 (st->xftdraw, xftcolor, xftfont, x, y, (FcChar8 *) s, 1);

  /* Annotations on 7 and 4 */

  if ((st->value >= 7.75 && st->value <= 7.85) ||
      (st->value >= 4.00 && st->value <= 4.25))
    {
      XSetForeground (dpy, st->gc, st->ring_color);
      xftcolor = &st->xft_text_color_2;
      xftfont = st->font2;

      s[0] = (ivalue == 4 ? 'C' : 'M');
      s[1] = 0;

      XftTextExtentsUtf8 (dpy, xftfont, (FcChar8 *) s, strlen(s), &extents);
      lbearing = -extents.x;
      rbearing = extents.width - extents.x;
      ascent   = extents.y;
      /* descent  = extents.height - extents.y; */

      x = st->w * 0.1;
      y = st->h * 0.1 + ascent;
      XftDrawStringUtf8 (st->xftdraw, xftcolor, xftfont, x, y,
                         (FcChar8 *) s, strlen(s));
      x = st->w * 0.9 - (rbearing + lbearing);
      XftDrawStringUtf8 (st->xftdraw, xftcolor, xftfont, x, y,
                         (FcChar8 *) s, strlen(s));

      s[0] = (ivalue == 4 ? 'F' : '3');
      s[1] = (ivalue == 4 ? 0   : '5');
      s[2] = 0;

      XftTextExtentsUtf8 (dpy, xftfont, (FcChar8 *) s, strlen(s), &extents);
      lbearing = -extents.x;
      rbearing = extents.width - extents.x;
      /* ascent   = extents.y; */
      /* descent  = extents.height - extents.y; */

      x = st->w * 0.1;
      y = st->h * 0.95;
      XftDrawStringUtf8 (st->xftdraw, xftcolor, xftfont, x, y, 
                         (FcChar8 *) s, strlen(s));
      x = st->w * 0.9 - (rbearing + lbearing);
      XftDrawStringUtf8 (st->xftdraw, xftcolor, xftfont, x, y,
                         (FcChar8 *) s, strlen(s));
    }

  if (r > 1/30.0)				/* Two rings around number */
    {
      double r2 = st->w / (double) st->h;
      double ss = 1;

      if (st->xgwa.width < st->xgwa.height)
        ss = 0.5;

      XSetForeground (dpy, st->gc, st->ring_color);
      XSetLineAttributes (dpy, st->gc, st->w * 0.025,
                          LineSolid, CapRound, JoinRound);

      w2 = st->w * 0.8 * ss / r2;
      h2 = st->h * 0.8 * ss;
      x = (st->w - w2) / 2;
      y = (st->h - h2) / 2;
      XDrawArc (dpy, st->pix, st->gc, x, y, w2, h2, 0, 360*64);

      w2 = w2 * 0.8;
      h2 = h2 * 0.8;
      x = (st->w - w2) / 2;
      y = (st->h - h2) / 2;
      XDrawArc (dpy, st->pix, st->gc, x, y, w2, h2, 0, 360*64);
    }

 DONE:

  img = XGetImage (dpy, st->pix, 0, 0, st->w, st->h, ~0L, ZPixmap);

  analogtv_load_ximage (st->tv, st->rec.input, img, 0, 0, 0, 0, 0);
  analogtv_reception_update (&st->rec);
  analogtv_draw (st->tv, st->noise, &rec, 1);

  XDestroyImage (img);

  now = double_time();
  timedelta = (1 / 29.97) - (now - then);

  if (! st->button_down_p)
    {
      if (st->last_time == 0)
        st->start = then;
      else
        st->value -= then - st->last_time;

      if (st->value <= 0 ||
          (r > 0.9 && st->value <= st->stop))
        {
          st->value = (random() % 20) ? 8.9 : 15;
          st->stop = ((random() % 50) ? 2 : 1) + (random() % 5);

          if (st->value > 9)	/* Spin the knobs again */
            {
              st->rec.level = pow(frand(1.0), 3.0) * 2.0 + 0.05;
              st->rec.ofs = random() % ANALOGTV_SIGNAL_LEN;
              st->tv->color_control += frand(0.3) - 0.15;
            }
        }
    }

  st->tv->powerup = then - st->start;
  st->last_time = then;

  return timedelta > 0 ? timedelta * 1000000 : 0;
}


static void
filmleader_reshape (Display *dpy, Window window, void *closure, 
                    unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  analogtv_reconfigure (st->tv);
  XGetWindowAttributes (dpy, window, &st->xgwa);

  if ((st->w > st->h) != (st->xgwa.width > st->xgwa.height))
    {
      int swap = st->w;
      st->w = st->h;
      st->h = swap;
    }
}


static Bool
filmleader_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (event->xany.type == ButtonPress)
    {
      st->button_down_p = True;
      return True;
    }
  else if (event->xany.type == ButtonRelease)
    {
      st->button_down_p = False;
      return True;
    }
  else if (screenhack_event_helper (dpy, window, event))
    {
      st->value = 15;
      st->rec.level = pow(frand(1.0), 3.0) * 2.0 + 0.05;
      st->rec.ofs = random() % ANALOGTV_SIGNAL_LEN;
      st->tv->color_control += frand(0.3) - 0.15;
      return True;
    }
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c >= '2' && c <= '8')
        {
          st->value = (c - '0') + (st->value - (int) st->value);
          return True;
        }
    }

  return False;
}


static void
filmleader_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  analogtv_release (st->tv);
  free (st->inp);
  XftDrawDestroy (st->xftdraw);
  XftColorFree(dpy, st->xgwa.visual, st->xgwa.colormap, &st->xft_text_color_1);
  XftColorFree(dpy, st->xgwa.visual, st->xgwa.colormap, &st->xft_text_color_2);
  XFreePixmap (dpy, st->pix);
  XFreeGC (dpy, st->gc);
  free (st);
}


static const char *filmleader_defaults [] = {

  ".background:  #000000",

# ifdef HAVE_MOBILE

  "*textBackground: #444488",  /* Need much higher contrast for some reason */
  "*textColor:      #000033",
  "*ringColor:      #DDDDFF",
  "*traceColor:     #222244",

# else /* X11 or Cocoa */

  "*textBackground: #9999DD",
  "*textColor:      #000015",
  "*ringColor:      #DDDDFF",
  "*traceColor:     #555577",

# endif

  /* Note: these font sizes aren't relative to screen pixels, but to the
     712 x Y or X x 712 canvas that we draw in, which is then scaled to
     the size of the screen by analogtv. */

# if defined(HAVE_IPHONE) || defined(HAVE_COCOA)
  "*numberFont:  Helvetica Bold 120",
  "*numberFont2: Helvetica 36",
  "*numberFont3: Helvetica 28",
# else /* X11 or Android */
  "*numberFont:  Helvetica Bold 170",
  "*numberFont2: Helvetica 50",
  "*numberFont3: Helvetica 36",
# endif

  "*noise:       0.04",
  ANALOGTV_DEFAULTS
  ".lowrez: false",  /* Required on macOS, or font size fucks up */
  "*geometry: 1280x720",
  0
};

static XrmOptionDescRec filmleader_options [] = {
  { "-noise",           ".noise",     XrmoptionSepArg, 0 },
  ANALOGTV_OPTIONS
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("FilmLeader", filmleader)
