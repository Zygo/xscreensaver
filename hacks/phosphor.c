/* xscreensaver, Copyright © 1999-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Phosphor -- simulate a glass tty with long-sustain phosphor.
 * Written by Jamie Zawinski <jwz@jwz.org>
 */

#include "screenhack.h"
#include "textclient.h"
#include "ansi-tty.h"
#include "ximage-loader.h"
#include "utf8wc.h"

#ifndef HAVE_JWXYZ
# include <X11/Intrinsic.h>
#endif


#define FUZZY_BORDER

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

#define BLANK  0
#define FLARE  1
#define NORMAL 2
#define FADE   3
#define STATE_MAX FADE

#define NPAR 16

#undef USE_XFT_BITMAP  /* Not working reliably */
#define BUILTIN_FONT

#ifdef BUILTIN_FONT
# include "images/gen/6x10font_png.h"
#endif /* BUILTIN_FONT */


typedef struct {
  unsigned long name;	/* Unicode character */
  int width, height;
  Pixmap pixmap;
#ifdef FUZZY_BORDER
  Pixmap pixmap2;
  Pixmap cache;
#endif /* FUZZY_BORDER */
  Bool blank_p;
} p_char;

typedef struct {
  unsigned char c;
  int state;
  Bool changed;
  Bool invert_p;
  Bool symbol_p;
} p_cell;

typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  XftFont *font;
  XftDraw *xftdraw;
  const char *program;
  int grid_width, grid_height;
  int char_width, char_height;
  int xmargin, ymargin;
  int saved_x, saved_y;
  int scale;
  int ticks;
  int mode;

  p_char   *chars[256];
  p_char  *ichars[256];
  p_char  *schars[256];
  p_char *sichars[256];

  p_cell *cells;
  XGCValues gcv;
  GC gc0;
  GC gc1;
#ifdef FUZZY_BORDER
  GC gc2;
#endif /* FUZZY_BORDER */
  GC *gcs;
  XImage *font_bits, *sym_font_bits;

  int cursor_x, cursor_y;
  Bool cursor_on;
  XtIntervalId cursor_timer;
  Time cursor_blink;
  int delay;

  text_data *tc;
  ansi_tty *tty;

} p_state;


static void capture_font_bits (p_state *state, Bool symbol_p);
static void char_to_pixmap (p_state *state, p_char *pc, unsigned long c,
                            Bool invert_p, Bool symbol_p);


/* About font metrics:

   "lbearing" is the distance from the leftmost pixel of a character to
   the logical origin of that character.  That is, it is the number of
   pixels of the character which are to the left of its logical origin.

   "rbearing" is the distance from the logical origin of a character to
   the rightmost pixel of a character.  That is, it is the number of
   pixels of the character to the right of its logical origin.

   "descent" is the distance from the bottommost pixel of a character to
   the logical baseline.  That is, it is the number of pixels of the
   character which are below the baseline.

   "ascent" is the distance from the logical baseline to the topmost pixel.
   That is, it is the number of pixels of the character above the baseline.

   Therefore, the bounding box of the "ink" of a character is
   lbearing + rbearing by ascent + descent;

   "width" is the distance from the logical origin of this character to
   the position where the logical orgin of the next character should be
   placed.

   For our purposes, we're only interested in the part of the character
   lying inside the "width" box.  If the characters have ink outside of
   that box (the "charcell" box) then we're going to lose it.  Alas.
 */


static void set_cursor (p_state *, Bool on);

static unsigned short scale_color_channel (unsigned short ch1, unsigned short ch2)
{
  return (ch1 * 100 + ch2 * 156) >> 8;
}

#define FONT6x10_WIDTH (256*7)
#define FONT6x10_HEIGHT 10

static void phosphor_tty_send (void *, const char *);

static void *
phosphor_init (Display *dpy, Window window)
{
  int i;
  unsigned long flags;
  p_state *state = (p_state *) calloc (sizeof(*state), 1);
  char *fontname = get_string_resource (dpy, "font", "Font");
  XftFont *font;

  state->dpy = dpy;
  state->window = window;

  XGetWindowAttributes (dpy, window, &state->xgwa);
/*  XSelectInput (dpy, window, state->xgwa.your_event_mask | ExposureMask);*/

  state->delay = get_integer_resource (dpy, "delay", "Integer");

# ifndef HAVE_XFT		/* Custom fonts only work under real Xft. */
  free (fontname);
  fontname = strdup ("builtin");
#  ifndef BUILTIN_FONT
#   error BUILTIN_FONT is required unless HAVE_XFT.
#  endif
# endif /* HAVE_XFT */

  if (!strcasecmp (fontname, "builtin") ||
      !strcasecmp (fontname, "(builtin)"))
    {
# ifndef BUILTIN_FONT
      fprintf (stderr, "%s: no builtin font\n", progname);
      state->font = load_xft_font_retry (dpy,
                                         screen_number (state->xgwa.screen),
                                         "fixed");
# endif /* !BUILTIN_FONT */
    }
  else
    {
      state->font = load_xft_font_retry (dpy,
                                         screen_number (state->xgwa.screen),
                                         fontname);
      if (!state->font) abort();
    }

  if (fontname) free (fontname);

  font = state->font;
  state->xftdraw = XftDrawCreate (dpy, window, state->xgwa.visual,
                               state->xgwa.colormap);

  /* Xft uses 'scale' */
  state->scale = get_integer_resource (dpy, "phosphorScale", "Integer");
  state->ticks = STATE_MAX + get_integer_resource (dpy, "ticks", "Integer");

  if (state->scale <= 0) state->scale = 1;
  if (state->ticks <= 0) state->ticks = 1;

  if (state->xgwa.width > 2560 || state->xgwa.height > 2560)
    state->scale *= 2;  /* Retina displays */

  state->cursor_blink = get_integer_resource (dpy, "cursor", "Time");

# ifdef BUILTIN_FONT
  if (! font)
    {
      state->char_width  = (FONT6x10_WIDTH / 256) - 1;
      state->char_height = FONT6x10_HEIGHT;
    }
  else
# endif /* BUILTIN_FONT */
    {
      XGlyphInfo overall;
      XftTextExtentsUtf8 (dpy, state->font, (FcChar8 *) "N", 1, &overall);
      state->char_width  = overall.xOff;
      state->char_height = font->ascent + font->descent;
    }

# ifdef HAVE_IPHONE
  /* Stupid iPhone X bezel.
     #### This is the worst of all possible ways to do this!  But how else?
   */
  if (state->xgwa.width == 2436 || state->xgwa.height == 2436) {
    state->xmargin = 96;
    state->ymargin = state->xmargin;
  }
# endif /* HAVE_IPHONE */

  state->grid_width = ((state->xgwa.width - state->xmargin * 2) /
                       (state->char_width * state->scale));
  state->grid_height = ((state->xgwa.height - state->ymargin * 2) /
                        (state->char_height * state->scale));
  state->cells = (p_cell *) calloc (sizeof(p_cell),
                                    state->grid_width * state->grid_height);

  state->gcs = (GC *) calloc (sizeof(GC), state->ticks + 1);

  {
    int ncolors = MAX (1, state->ticks - 3);
    XColor *colors = (XColor *) calloc (ncolors, sizeof(XColor));
    int h1, h2;
    double s1, s2, v1, v2;

    unsigned long fg = get_pixel_resource (state->dpy, state->xgwa.colormap,
                                           "foreground", "Foreground");
    unsigned long bg = get_pixel_resource (state->dpy, state->xgwa.colormap,
                                           "background", "Background");
    unsigned long flare = fg;

    XColor fg_color, bg_color;

    fg_color.pixel = fg;
    XQueryColor (state->dpy, state->xgwa.colormap, &fg_color);

    bg_color.pixel = bg;
    XQueryColor (state->dpy, state->xgwa.colormap, &bg_color);

    /* Now allocate a ramp of colors from the main color to the background. */
    rgb_to_hsv (scale_color_channel(fg_color.red, bg_color.red),
                scale_color_channel(fg_color.green, bg_color.green),
                scale_color_channel(fg_color.blue, bg_color.blue),
                &h1, &s1, &v1);
    rgb_to_hsv (bg_color.red, bg_color.green, bg_color.blue, &h2, &s2, &v2);

    /* Avoid rainbow effects when fading to black/grey/white. */
    if (s2 < 0.003)
      h2 = h1;
    if (s1 < 0.003)
      h1 = h2;

    make_color_ramp (state->xgwa.screen, state->xgwa.visual,
                     state->xgwa.colormap,
                     h1, s1, v1,
                     h2, s2, v2,
                     colors, &ncolors,
                     False, True, False);

    /* Adjust to the number of colors we actually got. */
    state->ticks = ncolors + STATE_MAX;

    /* If the foreground is brighter than the background, the flare is white.
     * Otherwise, the flare is left at the foreground color (i.e. no flare). */
    rgb_to_hsv (fg_color.red, fg_color.green, fg_color.blue, &h1, &s1, &v1);
    if (v2 <= v1)
      {
        XColor white;
        /* WhitePixel is only for the default visual, which can be overridden
         * on the command line. */
        white.red = 0xffff;
        white.green = 0xffff;
        white.blue = 0xffff;
        if (XAllocColor(state->dpy, state->xgwa.colormap, &white))
          flare = white.pixel;
      }

    /* Now, GCs all around.
     */
    state->gcv.cap_style = CapRound;
# ifdef FUZZY_BORDER
    state->gcv.line_width = (int) (((long) state->scale) * 1.3);
    if (state->gcv.line_width == state->scale)
      state->gcv.line_width++;
# else /* !FUZZY_BORDER */
    state->gcv.line_width = (int) (((long) state->scale) * 0.9);
    if (state->gcv.line_width >= state->scale)
      state->gcv.line_width = state->scale - 1;
    if (state->gcv.line_width < 1)
      state->gcv.line_width = 1;
# endif /* !FUZZY_BORDER */

    flags = (GCForeground | GCBackground | GCCapStyle | GCLineWidth);

    state->gcv.background = bg;
    state->gcv.foreground = bg;
    state->gcs[BLANK] = XCreateGC (state->dpy, state->window, flags,
                                   &state->gcv);

    state->gcv.foreground = flare;
    state->gcs[FLARE] = XCreateGC (state->dpy, state->window, flags,
                                   &state->gcv);

    state->gcv.foreground = fg;
    state->gcs[NORMAL] = XCreateGC (state->dpy, state->window, flags,
                                    &state->gcv);

    for (i = 0; i < ncolors; i++)
      {
        state->gcv.foreground = colors[i].pixel;
        state->gcs[STATE_MAX + i] = XCreateGC (state->dpy, state->window,
                                               flags, &state->gcv);
      }

    free (colors);
  }

  capture_font_bits (state, False);
  capture_font_bits (state, True);

  state->tty = ansi_tty_init (state->grid_width, state->grid_height);
  state->tty->closure  = state;
  state->tty->tty_send = phosphor_tty_send;

  set_cursor (state, True);

  state->tc = textclient_open (dpy);
  textclient_reshape (state->tc,
                      state->xgwa.width  - state->xmargin * 2,
                      state->xgwa.height - state->ymargin * 2,
                      state->grid_width,
                      state->grid_height,
                      0);

  return state;
}


/* Re-query the window size and update the internal character grid if changed.
 */
static Bool
resize_grid (p_state *state)
{
  int ow = state->grid_width;
  int oh = state->grid_height;
  p_cell *ocells = state->cells;
  int x, y;

  XGetWindowAttributes (state->dpy, state->window, &state->xgwa);

  /* Would like to ensure here that
     state->char_height * state->scale <= state->xgwa.height
     but changing scale requires regenerating the bitmaps. */

  state->grid_width  = ((state->xgwa.width - state->xmargin * 2) /
                        (state->char_width  * state->scale));
  state->grid_height = ((state->xgwa.height - state->ymargin * 2) /
                        (state->char_height * state->scale));

  if (state->grid_width  < 2) state->grid_width  = 2;
  if (state->grid_height < 2) state->grid_height = 2;

  if (ow == state->grid_width &&
      oh == state->grid_height)
    return False;

  state->cells = (p_cell *) calloc (sizeof(p_cell),
                                    state->grid_width * state->grid_height);

  for (y = 0; y < state->grid_height; y++)
    {
      for (x = 0; x < state->grid_width; x++)
        {
          p_cell *ncell = &state->cells [state->grid_width * y + x];
          if (x < ow && y < oh)
            *ncell = ocells [ow * y + x];
          ncell->changed = True;
        }
    }

  if (state->cursor_x >= state->grid_width)
    state->cursor_x = state->grid_width-1;
  if (state->cursor_y >= state->grid_height)
    state->cursor_y = state->grid_height-1;

  free (ocells);
  return True;
}


static p_char *
make_character (p_state *state, unsigned char c, Bool invert_p, Bool symbol_p)
{
  p_char *pc = (p_char *) malloc (sizeof (*pc));
  pc->name = (c + (symbol_p ? 256 : 0)) * (invert_p ? -1 : 1);
  pc->width =  state->scale * state->char_width;
  pc->height = state->scale * state->char_height;
  char_to_pixmap (state, pc, c, invert_p, symbol_p);
  return pc;
}


static void
capture_font_bits (p_state *state, Bool symbol_p)
{
  XftFont *font = state->font;
  int safe_width, height;
  unsigned char string[257];
  int i;
  Pixmap p;

# ifdef BUILTIN_FONT
  Pixmap p2 = 0;

  if (!font)
    {
      int pix_w, pix_h;
      XWindowAttributes xgwa;
      Pixmap m = 0;
      Pixmap p = image_data_to_pixmap (state->dpy, state->window,
                                       _6x10font_png, sizeof(_6x10font_png),
                                       &pix_w, &pix_h, &m);
      XImage *im = XGetImage (state->dpy, p, 0, 0, pix_w, pix_h, ~0L, ZPixmap);
      XImage *mm = XGetImage (state->dpy, m, 0, 0, pix_w, pix_h, 1, XYPixmap);
      XImage *im2;
      int x, y;
      XGCValues gcv;
      GC gc;
      unsigned long black =
        BlackPixelOfScreen (DefaultScreenOfDisplay (state->dpy));

      safe_width = state->char_width + 1;
      height = state->char_height;

      XFreePixmap (state->dpy, p);
      XFreePixmap (state->dpy, m);
      if (pix_w != 256*7) abort();
      if (pix_h != 10) abort();
      if (pix_w != FONT6x10_WIDTH) abort();
      if (pix_h != FONT6x10_HEIGHT) abort();

      if (symbol_p)
        {
          /* "6x10font.png" has the DEC Special Graphics characters down in
             the control area, so we just need to shift them into place. */
          int cw    = FONT6x10_WIDTH / 256;
          int from  = cw * 0x01;
          int to    = cw * 0x60;
          int len   = cw * 31;
          for (y = 0; y < pix_h; y++)
            for (x = to; x < to + len; x++)
              XPutPixel (mm, x, y, XGetPixel (mm, x - (to - from), y));
        }

      XGetWindowAttributes (state->dpy, state->window, &xgwa);
      im2 = XCreateImage (state->dpy, xgwa.visual, 1, XYBitmap, 0, 0,
                          pix_w, pix_h, 8, 0);
      im2->data = malloc (im2->bytes_per_line * im2->height);

      /* Convert deep image to 1 bit */
      for (y = 0; y < pix_h; y++)
        for (x = 0; x < pix_w; x++)
          XPutPixel (im2, x, y,
                     (XGetPixel (mm, x, y)
                      ? (XGetPixel (im, x, y) == black)
                      : 0));

      XDestroyImage (im);
      XDestroyImage (mm);
      im = 0;

      p2 = XCreatePixmap (state->dpy, state->window, 
                          im2->width, im2->height, im2->depth);
      gcv.foreground = 1;
      gcv.background = 0;
      gc = XCreateGC (state->dpy, p2, GCForeground|GCBackground, &gcv);
      XPutImage (state->dpy, p2, gc, im2, 0, 0, 0, 0, im2->width, im2->height);
      XFreeGC (state->dpy, gc);
      XDestroyImage (im2);
    }
  else
# endif /* BUILTIN_FONT */
    {
      XGlyphInfo overall;
      XftTextExtentsUtf8 (state->dpy, state->font,
                          (FcChar8 *) "N", 1, &overall);
      /* #### maybe safe_width should take lbearing into account */
      safe_width  = overall.xOff + 1;
      state->char_height = state->font->ascent + state->font->descent;
      height = state->char_height;
    }

  p = XCreatePixmap (state->dpy, state->window,
                     (safe_width * 256), height, 1);

  for (i = 0; i < 256; i++)
    string[i] = (unsigned char) i;
  string[256] = 0;

  state->gcv.foreground = 0;
  state->gcv.background = 0;
  state->gc0 = XCreateGC (state->dpy, p,
                          (GCForeground | GCBackground),
                          &state->gcv);

  state->gcv.foreground = 1;
  state->gc1 = XCreateGC (state->dpy, p,
                          (GCForeground | GCBackground |
                           GCCapStyle | GCLineWidth),
                          &state->gcv);

# ifdef HAVE_JWXYZ
  jwxyz_XSetAntiAliasing (state->dpy, state->gc0, False);
  jwxyz_XSetAntiAliasing (state->dpy, state->gc1, False);
# endif

# ifdef FUZZY_BORDER
  {
    state->gcv.line_width = (int) (((long) state->scale) * 0.8);
    if (state->gcv.line_width >= state->scale)
      state->gcv.line_width = state->scale - 1;
    if (state->gcv.line_width < 1)
      state->gcv.line_width = 1;
    state->gc2 = XCreateGC (state->dpy, p,
                            (GCForeground | GCBackground |
                             GCCapStyle | GCLineWidth),
                            &state->gcv);
  }
# endif /* FUZZY_BORDER */

  XFillRectangle (state->dpy, p, state->gc0, 0, 0, (safe_width * 256), height);

# ifdef BUILTIN_FONT
  if (p2)
    {
      XCopyPlane (state->dpy, p2, p, state->gc1,
                  0, 0, FONT6x10_WIDTH, FONT6x10_HEIGHT, 
                  0, 0, 1);
      XFreePixmap (state->dpy, p2);
    }
  else
# endif /* BUILTIN_FONT */
    {
# ifdef USE_XFT_BITMAP
      Pixmap pm_mono = XCreatePixmap (state->dpy, state->window,
                                      (safe_width * 256), height,
                                      1);
# else  /* !USE_XFT_BITMAP */
      Pixmap pm_color = XCreatePixmap (state->dpy, state->window,
                                       (safe_width * 256), height,
                                       state->xgwa.depth);
      XImage *xim_color;
      int x, y;
# endif /* !USE_XFT_BITMAP */
      XImage *xim_mono;
      GC text_gc;
      XGCValues gcv;
      XftDraw *xftdraw;
      XftColor xft_fg;

# ifdef USE_XFT_BITMAP
      /* Create a 1-bit pixmap and draw the text into it, fg/bg 1/0. */

      /* When using XftDrawCreateBitmap, XftDrawStringUtf8 always draws
         fg = 0 regardless of what is in the XftColor, so the background
         must be 1. */

      /* This is working (and looks better) on macOS X11, but no characters
         are showing up at all on "real" X11 on a Raspberry Pi.  Sigh.
         It also doesn't work under Cocoa with fake-Xft. */

      gcv.foreground = 1;
      xft_fg.pixel   = 0;
      xft_fg.color.red = xft_fg.color.green = xft_fg.color.blue = ~0L;

      text_gc = XCreateGC (state->dpy, pm_mono, GCForeground, &gcv);
      XFillRectangle (state->dpy, pm_mono, text_gc, 0, 0,
                      (safe_width * 256), height);
      xftdraw = XftDrawCreateBitmap (state->dpy, pm_mono);

# else  /* !USE_XFT_BITMAP */

      /* Create a full-depth pixmap and draw the text into it, fg/bg ~0/0. */

      gcv.foreground =
        BlackPixelOfScreen (DefaultScreenOfDisplay (state->dpy));
      text_gc = XCreateGC (state->dpy, pm_color, GCForeground, &gcv);
      XFillRectangle (state->dpy, pm_color, text_gc, 0, 0,
                      (safe_width * 256), height);
      xftdraw = XftDrawCreate (state->dpy, pm_color,
                               state->xgwa.visual, state->xgwa.colormap);
      xft_fg.pixel = ~0L;
      xft_fg.color.red = xft_fg.color.green = xft_fg.color.blue = ~0L;

# endif /* !USE_XFT_BITMAP */

      for (i = 0; i < 256; i++)
        if (!symbol_p)
          XftDrawStringUtf8 (xftdraw, &xft_fg, state->font,
                             i * safe_width, font->ascent,
                             (FcChar8 *) (string + i), 1);
        else
          {
            unsigned long uc = ansi_graphics_unicode[i];
            char ss[10];
            int L = utf8_encode (uc, ss, sizeof(ss)-1);
            XftDrawStringUtf8 (xftdraw, &xft_fg, state->font,
                               i * safe_width, font->ascent,
                               (FcChar8 *) ss, L);
          }

# ifdef USE_XFT_BITMAP
      /* Retrieve a 1-bit XImage. */
      xim_mono = XGetImage (state->dpy, pm_mono, 0, 0, 
                            i * safe_width, font->ascent,
                            1, XYPixmap);
      if (! xim_mono) abort();

      {
        int i;	/* Invert */
        for (i = 0; i < xim_mono->width * xim_mono->height / 8; i++)
          xim_mono->data[i] = ~xim_mono->data[i];
      }

# else  /* !USE_XFT_BITMAP */
      /* Retrieve a full-depth XImage. */
      xim_color = XGetImage (state->dpy, pm_color, 0, 0, 
                             i * safe_width, font->ascent,
                             ~0L, ZPixmap);

      /* Convert it to a mono XImage. */
      xim_mono = XCreateImage (state->dpy, state->xgwa.visual,
                               1, XYPixmap, 0, 0,
                               i * safe_width, font->ascent,
                               8, 0);
      xim_mono->data = (char *)
        calloc(xim_mono->height, xim_mono->bytes_per_line);
      for (y = 0; y < xim_color->height; y++)
        for (x = 0; x < xim_color->width; x++)
          XPutPixel (xim_mono, x, y, XGetPixel (xim_color, x, y) ? 1 : 0);
      XDestroyImage (xim_color);
      xim_color = 0;
# endif /* !USE_XFT_BITMAP */

      /* Copy mono ximage to mono pixmap */
      XFreeGC (state->dpy, text_gc);
      gcv.foreground = 1;
      gcv.background = 0;
      text_gc = XCreateGC (state->dpy, p, GCForeground|GCBackground, &gcv);
      XPutImage (state->dpy, p, text_gc, xim_mono, 0, 0, 0, 0,
                 (safe_width * 256), height);
      XFreeGC (state->dpy, text_gc);
      XDestroyImage (xim_mono);
    }

# if 0
  XWriteBitmapFile(state->dpy,
                   (symbol_p ? "/tmp/tvfont2.xbm" : "/tmp/tvfont.xbm"),
                   p, 
                   (safe_width * 256), height,
                   -1, -1);
# endif

  {
    XImage *im = XGetImage (state->dpy, p, 0, 0,
                            (safe_width * 256), height, ~0L, XYPixmap);
    if (symbol_p)
      state->sym_font_bits = im;
    else
      state->font_bits = im;
  }

  XFreePixmap (state->dpy, p);

  for (i = 0; i < countof(state->chars); i++)
    {
      p_char *c1 = make_character (state, i, False, symbol_p);
      p_char *c2 = make_character (state, i, True,  symbol_p);
      if (symbol_p)
        {
          state->schars [i] = c1;
          state->sichars[i] = c2;
        }
      else
        {
          state->chars [i] = c1;
          state->ichars[i] = c2;
        }
    }
}


static void
char_to_pixmap (p_state *state, p_char *pc, unsigned long c,
                Bool invert_p, Bool symbol_p)
{
  Pixmap p = 0;
  GC gc;
# ifdef FUZZY_BORDER
  Pixmap p2 = 0;
  GC gc2;
# endif /* FUZZY_BORDER */
  int from, to;
  int x1, y;
  XImage *font_bits = (symbol_p ? state->sym_font_bits : state->font_bits);

  int safe_width = state->char_width + 1;

  int width  = state->scale * state->char_width;
  int height = state->scale * state->char_height;

  gc = state->gc1;
  p = XCreatePixmap (state->dpy, state->window, width, height, 1);
  XFillRectangle (state->dpy, p, state->gc0, 0, 0, width, height);
# ifdef FUZZY_BORDER
  gc2 = state->gc2;
  p2 = XCreatePixmap (state->dpy, state->window, width, height, 1);
  XFillRectangle (state->dpy, p2, state->gc0, 0, 0, width, height);
# endif /* FUZZY_BORDER */

  from = safe_width * c;
  to =   safe_width * (c + 1);

# if 0
  if (c > 75 && c < 150)
    {
      printf ("\n=========== %lu (%c)\n", c, (char) c);
      for (y = 0; y < state->char_height; y++)
        {
          for (x1 = from; x1 < to; x1++)
            printf (XGetPixel (font_bits, x1, y) ? "* " : ". ");
          printf ("\n");
        }
    }
# endif

  pc->blank_p = !invert_p;
  for (y = 0; y < state->char_height; y++)
    for (x1 = from; x1 < to; x1++)
      {
        unsigned long pix = XGetPixel (font_bits, x1, y);
        if (invert_p) pix = !pix;
        if (pix)
          {
            int xoff = state->scale / 2;
            int x2;
            for (x2 = x1; x2 < to; x2++)
              {
                pix = XGetPixel (font_bits, x2, y);
                if (invert_p) pix = !pix;
                if (!pix)
                  break;
              }
            x2--;
            XDrawLine (state->dpy, p, gc,
                       (x1 - from) * state->scale + xoff, y * state->scale,
                       (x2 - from) * state->scale + xoff, y * state->scale);
# ifdef FUZZY_BORDER
            XDrawLine (state->dpy, p2, gc2,
                       (x1 - from) * state->scale + xoff, y * state->scale,
                       (x2 - from) * state->scale + xoff, y * state->scale);
# endif /* FUZZY_BORDER */
            x1 = x2;
            pc->blank_p = False;
          }
      }

  pc->pixmap = p;
# ifdef FUZZY_BORDER
  pc->pixmap2 = p2;
# endif /* FUZZY_BORDER */

# ifdef FUZZY_BORDER
  pc->cache = 0;
  if (get_boolean_resource (state->dpy, "cache", "Cache"))
    {
      /* Cache every possible frame of this character's fade in a pixmap,
         because XSetClipMask has become *incredibly* fucking slow on
         "modern" X11 systems (e.g. Raspbian 11.)  I blame compositors. */

      int st;
      pc->cache = XCreatePixmap (state->dpy, state->window,
                                 width * state->ticks, height,
                                 state->xgwa.depth);
      for (st = 0; st < state->ticks; st++)
        {
          int tx = st * width;
          int ty = 0;
          GC gc1 = state->gcs[st];
          GC gc2 = ((st + 2) < state->ticks
                    ? state->gcs[st + 2]
                    : 0);
          GC gc3 = (gc2 ? gc2 : gc1);
          if (gc3)
            XCopyPlane (state->dpy, pc->pixmap, pc->cache, gc3,
                        0, 0, width, height, tx, ty, 1L);
          if (gc2)
            {
              XSetClipMask (state->dpy, gc1, pc->pixmap2);
              XSetClipOrigin (state->dpy, gc1, tx, ty);
              XFillRectangle (state->dpy, pc->cache, gc1,
                              tx, ty, width, height);
              XSetClipMask (state->dpy, gc1, None);
            }
        }
    }
# endif /* FUZZY_BORDER */
}


/* Managing the display. 
 */

static void cursor_on_timer (XtPointer closure, XtIntervalId *id);
static void cursor_off_timer (XtPointer closure, XtIntervalId *id);

static Bool
set_cursor_1 (p_state *state, Bool on)
{
  p_cell *cell = &state->cells[state->grid_width * state->cursor_y +
                               state->cursor_x];
  if (state->cursor_on == on)
    return False;
  state->cursor_on = on;
  cell->changed = True;
  return True;
}

static void
set_cursor (p_state *state, Bool on)
{
  if (set_cursor_1 (state, on))
    {
      if (state->cursor_timer)
        XtRemoveTimeOut (state->cursor_timer);
      state->cursor_timer = 0;
      cursor_on_timer (state, 0);
    }
}


static void
cursor_off_timer (XtPointer closure, XtIntervalId *id)
{
  p_state *state = (p_state *) closure;
  XtAppContext app = XtDisplayToApplicationContext (state->dpy);
  set_cursor_1 (state, False);
  state->cursor_timer = XtAppAddTimeOut (app, state->cursor_blink,
                                         cursor_on_timer, closure);
}

static void
cursor_on_timer (XtPointer closure, XtIntervalId *id)
{
  p_state *state = (p_state *) closure;
  XtAppContext app = XtDisplayToApplicationContext (state->dpy);
  set_cursor_1 (state, True);
  state->cursor_timer = XtAppAddTimeOut (app, 2 * state->cursor_blink,
                                         cursor_off_timer, closure);
}


static void
decay (p_state *state)
{
  int x, y;
  for (y = 0; y < state->grid_height; y++)
    for (x = 0; x < state->grid_width; x++)
      {
        p_cell *cell = &state->cells[state->grid_width * y + x];
        if (cell->state == FLARE)
          {
            cell->state = NORMAL;
            cell->changed = True;
          }
        else if (cell->state >= FADE)
          {
            cell->state++;
            if (cell->state >= state->ticks)
              {
                cell->state = BLANK;
                cell->c = ' ';
              }
            cell->changed = True;
          }
      }
}


static void
print_char (p_state *state, int c)
{
  int x, y;
  ansi_tty *tty = state->tty;
  ansi_tty_print (tty, c);

  for (y = 0; y < tty->height; y++)
    for (x = 0; x < tty->width; x++)
      {
        tty_char *tcell = &tty->grid [tty->width * y + x];
        p_cell   *cell  = &state->cells [state->grid_width * y + x];
        Bool inv_p = tcell->flags & (TTY_BOLD | TTY_ITALIC | TTY_INVERSE);
        Bool sym_p = tcell->flags & (TTY_SYMBOLS);
        unsigned char latin1 = 0;
        p_char *pc;

        if (tty->inverse_p) inv_p = !inv_p;

        if ( cell->c == 0)  cell->c = ' ';
        if (tcell->c == 0) tcell->c = ' ';

        if (tcell->c <= 128)
          latin1 = tcell->c;
        else
          {
            /* Convert non-ASCII Unicode to closest Latin1. */
            char utf8[10];
            if (utf8_encode (tcell->c, utf8, sizeof(utf8)-1))
              {
                char *s = utf8_to_latin1 (utf8, FALSE);
                if (s)
                  {
                    latin1 = s[0];
                    free (s);
                  }
              }
          }

        if (!latin1) latin1 = ' ';

        pc = (cell->symbol_p
              ? (inv_p ? state->sichars[cell->c] : state->schars[cell->c])
              : (inv_p ?  state->ichars[cell->c] :  state->chars[cell->c]));

        if (!pc->blank_p &&
            latin1 == ' ' &&
            !inv_p)
          {
            /* Replacing existing character with a blank:
               fade out the previous character. */
            if (cell->state == FLARE || cell->state == NORMAL)
              cell->state = FADE;
            cell->changed = True;
          }
        else if (cell->c != tcell->c ||
                 cell->state >= FADE ||
                 cell->invert_p != inv_p ||
                 cell->symbol_p != sym_p)
          {
            /* Adding a new character: flare it in. */
            cell->invert_p = inv_p;
            cell->symbol_p = sym_p;
            /* cell->state = FLARE;  -- Looks bad when scrolling */
            cell->state = NORMAL;
            cell->changed = True;
            cell->c = latin1;
          }
      }

  /* If the cursor has moved, turn it on and flare it. */
  if (state->cursor_x != tty->x ||
      state->cursor_y != tty->y)
    {
      p_cell *oc = &state->cells [state->grid_width * state->cursor_y +
                                  state->cursor_x];
      p_cell *nc = &state->cells [state->grid_width * tty->y +
                                  tty->x];
      oc->changed = True;
      nc->changed = True;

      /* If the new cell is already fading out, do not bring its old
         character back underneath this new cursor. */
      if (nc->state >= FADE)
        nc->c = ' ';
      nc->state = FLARE;
      state->cursor_x = tty->x;
      state->cursor_y = tty->y;
      set_cursor (state, True);
    }
}


static void
update_display (p_state *state, Bool changed_only)
{
  int x, y;

  for (y = 0; y < state->grid_height; y++)
    for (x = 0; x < state->grid_width; x++)
      {
        p_cell *cell = &state->cells[state->grid_width * y + x];
        int st = cell->state;
        Bool inv_p = cell->invert_p;
        Bool sym_p = cell->symbol_p;
        Bool cursor_p = (x == state->cursor_x && y == state->cursor_y);
        unsigned char c = cell->c;
        p_char *pc;
        int width, height, tx, ty;

        if (changed_only && !cell->changed)
          continue;

        if (cursor_p)
          {
            if (state->cursor_on)
              inv_p = !inv_p;
            if (st >= FADE)
              c = ' ';
            if (st == BLANK || st >= FADE)
              st = NORMAL;
          }

        if (!c) c = ' ';
        pc = (sym_p
              ? (inv_p ? state->sichars[c] : state->schars[c])
              : (inv_p ?  state->ichars[c] :  state->chars[c]));

        width  = state->char_width  * state->scale;
        height = state->char_height * state->scale;
        tx = x * width  + state->xmargin;
        ty = y * height + state->ymargin;

        if (pc->blank_p || (cell->state == BLANK && !cursor_p))
          {
            XFillRectangle (state->dpy, state->window, state->gcs[BLANK],
                            tx, ty, width, height);
          }
# ifdef FUZZY_BORDER
        else if (pc->cache)
          {
            int src_x = st * width;
            int src_y = 0;
            XCopyArea (state->dpy, pc->cache, state->window, state->gcs[BLANK],
                       src_x, src_y, width, height, tx, ty);
          }
# endif /* FUZZY_BORDER */
        else
          {
# ifdef FUZZY_BORDER
            GC gc1 = state->gcs[st];
            GC gc2 = ((st + 2) < state->ticks
                      ? state->gcs[st + 2]
                      : 0);
            GC gc3 = (gc2 ? gc2 : gc1);
            if (gc3)
              XCopyPlane (state->dpy, pc->pixmap, state->window, gc3,
                          0, 0, width, height, tx, ty, 1L);
            if (gc2)
              {
                XSetClipMask (state->dpy, gc1, pc->pixmap2);
                XSetClipOrigin (state->dpy, gc1, tx, ty);
                XFillRectangle (state->dpy, state->window, gc1,
                                tx, ty, width, height);
                XSetClipMask (state->dpy, gc1, None);
              }
# else /* !FUZZY_BORDER */
            XCopyPlane (state->dpy,
                        pc->pixmap, state->window,
                        state->gcs[st],
                        0, 0, width, height, tx, ty, 1L);
# endif /* !FUZZY_BORDER */
          }

        cell->changed = False;
      }
}


static unsigned long
phosphor_draw (Display *dpy, Window window, void *closure)
{
  p_state *state = (p_state *) closure;
  int c;
  update_display (state, True);
  decay (state);

  c = textclient_getc (state->tc);
  if (c > 0) 
    print_char (state, c);

  return state->delay;
}


static void
phosphor_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  p_state *state = (p_state *) closure;
  Bool changed_p = resize_grid (state);

  if (! changed_p) return;

  ansi_tty_resize (state->tty, state->grid_width, state->grid_height);

  textclient_reshape (state->tc,
                      w - state->xmargin * 2,
                      h - state->ymargin * 2,
                      state->grid_width,
                      state->grid_height,
                      0);
}


static void
phosphor_tty_send (void *closure, const char *text)
{
  p_state *state = (p_state *) closure;
  textclient_puts (state->tc, text);
}


static Bool
phosphor_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  p_state *state = (p_state *) closure;

  if (event->xany.type == Expose)
    update_display (state, False);
  else if (event->xany.type == KeyPress)
    return textclient_putc_event (state->tc, &event->xkey);
  return False;
}

static void
phosphor_free (Display *dpy, Window window, void *closure)
{
  p_state *state = (p_state *) closure;
  int i;

  textclient_close (state->tc);
  if (state->cursor_timer)
    XtRemoveTimeOut (state->cursor_timer);

  ansi_tty_free (state->tty);

  if (state->gc0) XFreeGC (dpy, state->gc0);
  if (state->gc1) XFreeGC (dpy, state->gc1);
# ifdef FUZZY_BORDER
  if (state->gc2) XFreeGC (dpy, state->gc2);
# endif /* FUZZY_BORDER */
  for (i = 0; i < state->ticks; i++)
    if (state->gcs[i]) XFreeGC (dpy, state->gcs[i]);
  free (state->gcs);
  for (i = 0; i < countof(state->chars); i++) {
    XFreePixmap (dpy, state->chars[i]->pixmap);
    XFreePixmap (dpy, state->ichars[i]->pixmap);
# ifdef FUZZY_BORDER
    XFreePixmap (dpy, state->chars[i]->pixmap2);
    XFreePixmap (dpy, state->ichars[i]->pixmap2);
    if (state->chars[i]->cache)
      XFreePixmap (dpy, state->chars[i]->cache);
    if (state->ichars[i]->cache)
      XFreePixmap (dpy, state->ichars[i]->cache);
# endif /* FUZZY_BORDER */
    free (state->chars[i]);
  }
  XDestroyImage (state->font_bits);
  XDestroyImage (state->sym_font_bits);
  free (state->cells);
  free (state);
}



static const char *phosphor_defaults [] = {
/*".lowrez:                true", */
  ".background:		   Black",
  ".foreground:		   #00FF00",
  "*fpsSolid:		   true",
  "*phosphorScale:	   6",
  "*ticks:		   20",
  "*delay:		   50000",
  "*cursor:		   333",
  "*program:		   xscreensaver-text",
  "*relaunch:		   5",
  "*metaSendsESC:	   True",
  "*swapBSDEL:		   True",

# if defined(BUILTIN_FONT)
  "*font:		   (builtin)",
# elif defined(HAVE_COCOA)
  "*font:		   Monaco 15",
# else
  "*font:		   fixed",
# endif

# ifdef HAVE_JWXYZ
  "*cache:		   False",
# else
  "*cache:		   True",
# endif

# ifdef HAVE_FORKPTY
  "*usePty:                True",
# else
  "*usePty:                False",
# endif

  /* For debugging vt100 at 80x24: */
  /*
  "*delay: 0",
  "*phosphorScale: 3",
  ".geometry: =1470x760",
  "*program: echo $COLUMNS x $ROWS ; /usr/local/bin/vttest",
  */

  0
};

static XrmOptionDescRec phosphor_options [] = {
  { "-font",		".font",		XrmoptionSepArg, 0 },
  { "-scale",		".phosphorScale",	XrmoptionSepArg, 0 },
  { "-ticks",		".ticks",		XrmoptionSepArg, 0 },
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-program",		".program",		XrmoptionSepArg, 0 },
  { "-pipe",		".usePty",		XrmoptionNoArg, "False" },
  { "-pty",		".usePty",		XrmoptionNoArg, "True"  },
  { "-meta",		".metaSendsESC",	XrmoptionNoArg, "False" },
  { "-esc",		".metaSendsESC",	XrmoptionNoArg, "True"  },
  { "-bs",		".swapBSDEL",		XrmoptionNoArg, "False" },
  { "-del",		".swapBSDEL",		XrmoptionNoArg, "True"  },
  { "-cache",		".cache",		XrmoptionNoArg, "True"  },
  { "-no-cache",	".cache",		XrmoptionNoArg, "False" },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Phosphor", phosphor)
