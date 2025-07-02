/* xscreensaver, Copyright © 1998-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Blue Screen of Death: the finest in personal computer emulation.
 * Concept cribbed from Stephen Martin <smartin@mks.com>;
 * this version written by jwz, 4-Jun-98.
 * Mostly rewritten by jwz, 20-Feb-2006.
 */


/* To add a new mode:

    - Define a function `new_os(dpy,win)' that returns a `bsod_state' struct.
    - Draw on the window to set up its frame-zero state.  This must be fast:
      no sleeping or long loops!
    - Populate the bsod_state structure with additional actions to take by
      using the various BSOD_ macros.  Note that you can control the delays
      when printing text on a per-character or per-line basis.
    - Insert your function in the `all_modes' table.
    - Add a `doXXX' entry to `bsod_defaults'.
    - Add fonts or colors to `bsod_defaults' if necessary.

   Look at windows_31() for a simple example.

   Look at linux_fsck() for a more complicated example with random behavior.

   Or, you can bypass all that: look at nvidia() for a really hairy example.
 */


#include "screenhack.h"
#include "xft.h"
#include "xftwrap.h"
#include "ximage-loader.h"
#include "apple2.h"

#include <ctype.h>
#include <time.h>

#ifdef HAVE_XSHM_EXTENSION
#include "xshm.h"
#endif

#ifdef HAVE_UNAME
# include <sys/utsname.h>
#endif /* HAVE_UNAME */

#include "images/gen/amiga_png.h"
#include "images/gen/hmac_png.h"
#include "images/gen/osx_10_2_png.h"
#include "images/gen/osx_10_3_png.h"
#include "images/gen/android_png.h"
#include "images/gen/ransomware_png.h"
#include "images/gen/atari_png.h"
#include "images/gen/mac_png.h"
#include "images/gen/macbomb_png.h"
#include "images/gen/apple_png.h"
#include "images/gen/atm_png.h"
#include "images/gen/sun_png.h"
#include "images/gen/dvd_png.h"
#include "images/gen/gnome1_png.h"
#include "images/gen/gnome2_png.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

# undef MIN
# undef MAX
# define MIN(A,B) ((A)<(B)?(A):(B))
# define MAX(A,B) ((A)>(B)?(A):(B))

#undef EOF
typedef enum { EOF=0,
               LEFT, CENTER, RIGHT,
               LEFT_FULL, CENTER_FULL, RIGHT_FULL,
               COLOR, INVERT, MOVETO, MARGINS,
               CURSOR_BLOCK, CURSOR_LINE, RECT, LINE, COPY, PIXMAP, IMG, FONT,
               PAUSE, CHAR_DELAY, LINE_DELAY,
               LOOP, RESET, VERT_MARGINS, CROP,
               WRAP, WORD_WRAP, TRUNCATE
} bsod_event_type;

struct bsod_event {
  bsod_event_type type;
  void *arg1, *arg2, *arg3, *arg4, *arg5, *arg6;
};

struct bsod_state {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  XftFont *font, *fontA, *fontB, *fontC;
  XftDraw *xftdraw;
  XftColor xft_fg, xft_bg;
  unsigned long fg, bg;
  GC gc;
  int left_margin, right_margin;	/* for text wrapping */
  int top_margin, bottom_margin;	/* for text scrolling and cropping */
  int xoff, yoff;
  Bool wrap_p, word_wrap_p;
  char word_buf[80];
  Bool scroll_p;
  Bool crop_p;                  /* If True, chops off extra text vertically */

  Pixmap pixmap, mask;		/* Source image used by BSOD_PIXMAP */

  int x, y;			/* current text-drawing position */
  int current_left;		/* don't use this */
  int last_nonwhite;

  int pos;			/* position in queue */
  int queue_size;
  struct bsod_event *queue;

  unsigned long char_delay;	/* delay between printing characters */
  unsigned long line_delay;	/* delay between printing lines */

  Bool macx_eol_kludge;

  void *closure;
  int  (*draw_cb) (struct bsod_state *);
  void (*free_cb) (struct bsod_state *);

  async_load_state *img_loader;
};


# if !defined(__GNUC__) && !defined(__extension__)
  /* don't warn about "string length is greater than the length ISO C89
     compilers are required to support" in these string constants... */
#  define  __extension__ /**/
# endif


/* Draw text at the current position; align is LEFT, CENTER, RIGHT, or *_FULL
   (meaning to clear the whole line margin to margin).
 */
#define BSOD_TEXT(bst,align,string) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = (align); \
  (bst)->queue[(bst)->pos].arg1 = (void *) strdup (__extension__ (string)); \
  (bst)->queue[(bst)->pos].arg2 = (bst)->queue[(bst)->pos].arg1; \
  (bst)->queue[(bst)->pos].arg3 = (void *) 0; \
  (bst)->pos++; \
  } while (0)

/* Flip the foreground and background colors
 */
#define BSOD_INVERT(bst) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = INVERT; \
  (bst)->pos++; \
  } while (0)

/* Set the foreground and background colors to the given pixels
 */
#define BSOD_COLOR(bst,fg,bg) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = COLOR; \
  (bst)->queue[(bst)->pos].arg1 = (void *) fg; \
  (bst)->queue[(bst)->pos].arg2 = (void *) bg; \
  (bst)->pos++; \
  } while (0)

/* Set the position of the next text.
   Note that this is the baseline: lower left corner of next char printed.
 */
#define BSOD_MOVETO(bst,x,y) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = MOVETO; \
  (bst)->queue[(bst)->pos].arg1 = (void *) ((long) (x)); \
  (bst)->queue[(bst)->pos].arg2 = (void *) ((long) (y)); \
  (bst)->pos++; \
  } while (0)

/* Delay for at least the given number of microseconds.
 */
#define BSOD_PAUSE(bst,usec) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = PAUSE; \
  (bst)->queue[(bst)->pos].arg1 = (void *) ((long) (usec)); \
  (bst)->pos++; \
  } while (0)

/* Set the delay after each character is printed.
 */
#define BSOD_CHAR_DELAY(bst,usec) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = CHAR_DELAY; \
  (bst)->queue[(bst)->pos].arg1 = (void *) ((long) (usec)); \
  (bst)->pos++; \
  } while (0)

/* Set the delay after each newline.
 */
#define BSOD_LINE_DELAY(bst,usec) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = LINE_DELAY; \
  (bst)->queue[(bst)->pos].arg1 = (void *) (usec); \
  (bst)->pos++; \
  } while (0)

/* Set the prevailing left/right margins (used when strings have
   embedded newlines, and when centering or right-justifying text.)
 */
#define BSOD_MARGINS(bst,left,right) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = MARGINS; \
  (bst)->queue[(bst)->pos].arg1 = (void *) ((long) (left)); \
  (bst)->queue[(bst)->pos].arg2 = (void *) ((long) (right)); \
  (bst)->pos++; \
  } while (0)

/* Set the prevailing top/bottom margins (used when scrolling/cropping text)
 */
#define BSOD_VERT_MARGINS(bst,top,bottom) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = VERT_MARGINS; \
  (bst)->queue[(bst)->pos].arg1 = (void *) ((long) (top)); \
  (bst)->queue[(bst)->pos].arg2 = (void *) ((long) (bottom)); \
  (bst)->pos++; \
  } while (0)

/* Draw a blinking cursor; type is CURSOR_BLOCK or CURSOR_LINE.
   usec is how long 1/2 of a cycle is.  count is how many times to blink.
   (You can pass a gigantic number if this is the last thing in your mode.)
 */
#define BSOD_CURSOR(bst,ctype,usec,count) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = (ctype); \
  (bst)->queue[(bst)->pos].arg1 = (void *) (usec); \
  (bst)->queue[(bst)->pos].arg2 = (void *) (count); \
  (bst)->pos++; \
  } while (0)

/* Draw or fill a rectangle.  You can set line-width in the GC.
 */
#define BSOD_RECT(bst,fill,x,y,w,h) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = RECT; \
  (bst)->queue[(bst)->pos].arg1 = (void *) ((long) (fill)); \
  (bst)->queue[(bst)->pos].arg2 = (void *) ((long) (x)); \
  (bst)->queue[(bst)->pos].arg3 = (void *) ((long) (y)); \
  (bst)->queue[(bst)->pos].arg4 = (void *) ((long) (w)); \
  (bst)->queue[(bst)->pos].arg5 = (void *) ((long) (h)); \
  (bst)->pos++; \
  } while (0)

/* Draw a line.
 */
#define BSOD_LINE(bst,x,y,x2,y2,thick) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = LINE; \
  (bst)->queue[(bst)->pos].arg1 = (void *) ((long) (x)); \
  (bst)->queue[(bst)->pos].arg2 = (void *) ((long) (y)); \
  (bst)->queue[(bst)->pos].arg3 = (void *) ((long) (x2)); \
  (bst)->queue[(bst)->pos].arg4 = (void *) ((long) (y2)); \
  (bst)->queue[(bst)->pos].arg5 = (void *) ((long) (thick)); \
  (bst)->pos++; \
  } while (0)

/* Copy a rect from the window, to the window.
 */
#define BSOD_COPY(bst,srcx,srcy,w,h,tox,toy) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = COPY; \
  (bst)->queue[(bst)->pos].arg1 = (void *) ((long) (srcx)); \
  (bst)->queue[(bst)->pos].arg2 = (void *) ((long) (srcy)); \
  (bst)->queue[(bst)->pos].arg3 = (void *) ((long) (w)); \
  (bst)->queue[(bst)->pos].arg4 = (void *) ((long) (h)); \
  (bst)->queue[(bst)->pos].arg5 = (void *) ((long) (tox)); \
  (bst)->queue[(bst)->pos].arg6 = (void *) ((long) (toy)); \
  (bst)->pos++; \
  } while (0)

/* Copy a rect from bst->pixmap to the window.
 */
#define BSOD_PIXMAP(bst,srcx,srcy,w,h,tox,toy) do { \
  BSOD_COPY(bst,srcx,srcy,w,h,tox,toy); \
  (bst)->queue[(bst)->pos-1].type = PIXMAP; \
  } while (0)

/* Load a random image (or the desktop) onto the window.
 */
#define BSOD_IMG(bst) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = IMG; \
  (bst)->pos++; \
  } while (0)

/* Switch between fonts A, B and C.
 */
#define BSOD_FONT(bst,n) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = FONT; \
  (bst)->queue[(bst)->pos].arg1 = (void *) ((long) (n)); \
  (bst)->pos++; \
  } while (0)

/* Jump around in the state table.  You can use this as the last thing
   in your state table to repeat the last N elements forever.
 */
#define BSOD_LOOP(bst,off) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = LOOP; \
  (bst)->queue[(bst)->pos].arg1 = (void *) (off); \
  (bst)->pos++; \
  } while (0)

/* Restart the whole thing from the beginning.
 */
#define BSOD_RESET(bst) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = RESET; \
  (bst)->pos++; \
  } while (0)

/* Sets the crop state, if True, will not write text below the bottom_margin
 */
#define BSOD_CROP(bst, state) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = CROP; \
  (bst)->queue[(bst)->pos].arg1 = (void *) ((long) (state)); \
  (bst)->pos++; \
  } while (0)

#define BSOD_WRAP(bst) do { \
  ensure_queue (bst); \
 (bst)->queue[(bst)->pos].type = WRAP; \
  (bst)->pos++; \
  } while (0)

#define BSOD_WORD_WRAP(bst) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = WORD_WRAP; \
  (bst)->pos++; \
  } while (0)

#define BSOD_TRUNCATE(bst) do { \
  ensure_queue (bst); \
  (bst)->queue[(bst)->pos].type = TRUNCATE; \
  (bst)->pos++; \
  } while (0)


static void
ensure_queue (struct bsod_state *bst)
{
  int n;
  if (bst->pos + 1 < bst->queue_size)
    return;

  n = bst->queue_size + 10;
  if (n < 100) n *= 2;
  n *= 1.2;

  bst->queue = (struct bsod_event *)
    realloc (bst->queue, n * sizeof(*bst->queue));
  if (!bst->queue) abort();
  memset (bst->queue + bst->queue_size, 0,
          (n - bst->queue_size) * sizeof(*bst->queue));
  bst->queue_size = n;
}


static void
position_for_text (struct bsod_state *bst, const char *line)
{
  int max_width = 0;

  const char *start = line;

  if (bst->queue[bst->pos].type != LEFT &&
      bst->queue[bst->pos].type != LEFT_FULL)
    while (*start)
      {
        XGlyphInfo ov;
        const char *end = start;
        while (*end && *end != '\r' && *end != '\n')
          end++;

        XftTextExtentsUtf8 (bst->dpy, bst->font,
                            (FcChar8 *) start, end-start, &ov);
        if (ov.xOff > max_width)
          max_width = ov.xOff;
        if (!*end) break;
        start = end+1;
      }

  switch (bst->queue[bst->pos].type) {
  case LEFT:
  case LEFT_FULL:
    bst->current_left = bst->left_margin + bst->xoff;
    break;
  case RIGHT:
  case RIGHT_FULL:
    bst->x = max_width - bst->right_margin - bst->xoff;
    bst->current_left = bst->x;
    break;
  case CENTER:
  case CENTER_FULL:
    {
      int w = (bst->xgwa.width - bst->left_margin - bst->right_margin -
               max_width);
      if (w < 0) w = 0;
      bst->x = bst->left_margin + bst->xoff + (w / 2);
      bst->current_left = bst->x;
      break;
    }
  default:
    abort();
  }
}


static void
bst_crlf (struct bsod_state *bst)
{
  int lh = bst->font->ascent + bst->font->descent;
  bst->x = bst->current_left;
  if (!bst->scroll_p ||
      bst->y + lh < bst->xgwa.height - bst->bottom_margin - bst->yoff)
    bst->y += lh;
  else
    {
      int w = bst->xgwa.width  - bst->right_margin - bst->left_margin;
      int h = bst->xgwa.height - bst->top_margin - bst->bottom_margin;
      XCopyArea (bst->dpy, bst->window, bst->window, bst->gc,
                 bst->left_margin + bst->xoff,
                 bst->top_margin + bst->yoff + lh,
                 w, h - lh,
                 bst->left_margin + bst->xoff,
                 bst->top_margin + bst->yoff);
      XClearArea (bst->dpy, bst->window,
                  bst->left_margin + bst->xoff,
                  bst->top_margin + bst->yoff + h - lh, w, lh, False);
    }
}


/* This is slow, use it sparingly. */
static void
set_xft_color (Display *dpy, Colormap cmap, XftColor *c, unsigned long pixel)
{
  XColor xc;
  if (c->pixel == pixel && c->color.alpha)
    return;  /* Using alpha as "is initialized" */
  xc.pixel = pixel;
  XQueryColor (dpy, cmap, &xc);
  c->pixel = pixel;
  c->color.red   = xc.red;
  c->color.green = xc.green;
  c->color.blue  = xc.blue;
  c->color.alpha = 0xFFFF;
}


static void
draw_char (struct bsod_state *bst, char c)
{
  if (!c)
    abort();
  else if (bst->crop_p && bst->y >=
           (bst->xgwa.height - bst->bottom_margin - bst->yoff))
    {
      /* reached the bottom of the drawing area, and crop_p = True */
      return;
    }
  else if (c == '\r')
    {
      bst->x = bst->current_left;
      bst->last_nonwhite = bst->x;
    }
  else if (c == '\n')
    {
      if (bst->macx_eol_kludge)
        {
          /* Special case for the weird way OSX crashes print newlines... */
          XGlyphInfo ov;
          XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) "n", 1, &ov);
          XSetForeground (bst->dpy, bst->gc, bst->bg);
          XFillRectangle (bst->dpy, bst->window, bst->gc,
                          bst->x, bst->y - bst->font->ascent,
                          ov.width,
                          (bst->font->ascent + bst->font->descent) * 2);
          XSetForeground (bst->dpy, bst->gc, bst->fg);
        }
      bst_crlf (bst);
    }
  else if (c == '\b')	/* backspace */
    {
      XGlyphInfo ov;
      XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) "n", 1, &ov);
      bst->x -= ov.xOff;
      if (bst->x < bst->left_margin + bst->xoff)
        bst->x = bst->left_margin + bst->xoff;
    }
  else
    {
      XGlyphInfo ov;
      Bool cursorp = (c == '\033');  /* apparently \e is non-standard now! */

      if (cursorp) c = ' ';
      XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) &c, 1, &ov);

      if (bst->x < bst->left_margin)
        bst->x = bst->left_margin;

      if ((bst->wrap_p || bst->word_wrap_p) &&
          bst->x + ov.xOff > bst->xgwa.width - bst->right_margin - bst->xoff)
        {
          XGlyphInfo ov2;
          int L = 0;

          if (bst->word_wrap_p && *bst->word_buf)
            {
              L = strlen(bst->word_buf);
              XftTextExtentsUtf8 (bst->dpy, bst->font,
                                  (FcChar8 *) bst->word_buf, L, &ov2);
            }

          if (L)  /* Erase the truncated wrapped word */
            {
              XSetForeground (bst->dpy, bst->gc, bst->bg);
              XFillRectangle (bst->dpy, bst->window, bst->gc,
                              bst->last_nonwhite,
                              bst->y - bst->font->ascent,
                              ov2.width,
                              bst->font->ascent + bst->font->descent);
              XSetForeground (bst->dpy, bst->gc, bst->fg);
            }

          bst_crlf (bst);

          if (L)  /* Draw wrapped partial word on the next line, no delay */
            {
              XftTextExtentsUtf8 (bst->dpy, bst->font,
                                  (FcChar8 *) bst->word_buf, L, &ov);
              XSetForeground (bst->dpy, bst->gc, bst->bg);
              XFillRectangle (bst->dpy, bst->window, bst->gc,
                              bst->x,
                              bst->y - bst->font->ascent,
                              ov.xOff,
                              bst->font->ascent + bst->font->descent);
              XftDrawStringUtf8 (bst->xftdraw, &bst->xft_fg, bst->font,
                                 bst->x, bst->y, 
                                 (FcChar8 *) bst->word_buf, L);
              bst->x += ov2.width;
              bst->last_nonwhite = bst->x;
            }
        }

      /* We render the character ESC as an inverted space (block cursor). */

      if (cursorp)
        {
          XftColor cswap = bst->xft_fg;
          unsigned long swap = bst->fg;
          bst->fg = bst->bg;
          bst->xft_fg = bst->xft_bg;
          bst->bg = swap;
          bst->xft_bg = cswap;
          XSetForeground (bst->dpy, bst->gc, bst->fg);
          XSetBackground (bst->dpy, bst->gc, bst->bg);
        }

      XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) &c, 1, &ov);
      XSetForeground (bst->dpy, bst->gc, bst->bg);
      XFillRectangle (bst->dpy, bst->window, bst->gc,
                      bst->x,
                      bst->y - bst->font->ascent,
                      ov.xOff,
                      bst->font->ascent + bst->font->descent);
      XSetForeground (bst->dpy, bst->gc, bst->fg);
      XftDrawStringUtf8 (bst->xftdraw, &bst->xft_fg, bst->font,
                         bst->x, bst->y, (FcChar8 *) &c, 1);

      if (cursorp)
        {
          XftColor cswap = bst->xft_fg;
          unsigned long swap = bst->fg;
          bst->fg = bst->bg;
          bst->xft_fg = bst->xft_bg;
          bst->bg = swap;
          bst->xft_bg = cswap;
          XSetForeground (bst->dpy, bst->gc, bst->fg);
          XSetBackground (bst->dpy, bst->gc, bst->bg);
          c = ' ';
        }

      bst->x += ov.xOff;

      if (bst->word_wrap_p)
        {
          if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            {
              bst->word_buf[0] = 0;
              bst->last_nonwhite = bst->x;
            }
          else
            {
              int L = strlen (bst->word_buf);
              if (L >= sizeof(bst->word_buf)-1) abort();
              bst->word_buf[L] = c;
              bst->word_buf[L+1] = 0;
            }
        }
    }
}


static long
bsod_pop (struct bsod_state *bst)
{
  bsod_event_type type = bst->queue[bst->pos].type;

  if (bst->draw_cb)
    return bst->draw_cb (bst);

  if (bst->pos < 0)   /* already done */
    abort();

  switch (type) {

  case LEFT:   case LEFT_FULL:
  case CENTER: case CENTER_FULL:
  case RIGHT:  case RIGHT_FULL:
    {
      const char *s = (const char *) bst->queue[bst->pos].arg2;
      char c;

      if (! *s)
        {
          long delay = bst->line_delay;
          /* Reset the string back to the beginning, in case we loop. */
          bst->queue[bst->pos].arg2 = bst->queue[bst->pos].arg1;
          bst->queue[bst->pos].arg3 = 0;
          /* I have no idea what this means, but the extra cast seems to
             silence it: "warning: cast to smaller integer type
             'bsod_event_type' from 'void *' [-Wvoid-pointer-to-enum-cast]" */
          bst->queue[bst->pos].type = (bsod_event_type) (size_t)
            bst->queue[bst->pos].arg4;
          bst->pos++;
          bst->current_left = bst->left_margin + bst->xoff;
          return delay;
        }

      if (! bst->queue[bst->pos].arg3)    /* "done once" */
        {
          position_for_text (bst, s);
          bst->queue[bst->pos].arg4 = (void *) bst->queue[bst->pos].type;
          bst->queue[bst->pos].type = LEFT;
          bst->word_buf[0] = 0;
          bst->last_nonwhite = bst->x;

          if (type == CENTER_FULL ||
              type == LEFT_FULL ||
              type == RIGHT_FULL)
            {
              XSetForeground (bst->dpy, bst->gc, bst->bg);
              XFillRectangle (bst->dpy, bst->window, bst->gc,
                              0,
                              bst->y - bst->font->ascent,
                              bst->xgwa.width,
                              bst->font->ascent + bst->font->descent);
              XSetForeground (bst->dpy, bst->gc, bst->fg);
            }
        }

      c = *s++;
      draw_char (bst, c);
      bst->queue[bst->pos].arg2 = (void *) s;
      bst->queue[bst->pos].arg3 = (void *) 1;  /* "done once" */

      return (c == '\r' || c == '\n'
              ? bst->line_delay
              : bst->char_delay);
    }
  case INVERT:
    {
      XftColor cswap = bst->xft_fg;
      unsigned long swap = bst->fg;
      bst->fg = bst->bg;
      bst->xft_fg = bst->xft_bg;
      bst->bg = swap;
      bst->xft_bg = cswap;
      XSetForeground (bst->dpy, bst->gc, bst->fg);
      XSetBackground (bst->dpy, bst->gc, bst->bg);
      bst->pos++;
      return 0;
    }
  case COLOR:
    {
      bst->fg = (unsigned long) bst->queue[bst->pos].arg1;
      bst->bg = (unsigned long) bst->queue[bst->pos].arg2;
      XSetForeground (bst->dpy, bst->gc, bst->fg);
      XSetBackground (bst->dpy, bst->gc, bst->bg);
      set_xft_color (bst->dpy, bst->xgwa.colormap, &bst->xft_fg, bst->fg);
      bst->pos++;
      return 0;
    }
  case MOVETO:
    {
      bst->x = (long) bst->queue[bst->pos].arg1;
      bst->y = (long) bst->queue[bst->pos].arg2;
      bst->word_buf[0] = 0;
      bst->last_nonwhite = bst->x;
      bst->pos++;
      return 0;
    }
  case RECT:
    {
      int f = (long) bst->queue[bst->pos].arg1;
      int x = (long) bst->queue[bst->pos].arg2;
      int y = (long) bst->queue[bst->pos].arg3;
      int w = (long) bst->queue[bst->pos].arg4;
      int h = (long) bst->queue[bst->pos].arg5;
      if (f)
        XFillRectangle (bst->dpy, bst->window, bst->gc, x, y, w, h);
      else
        XDrawRectangle (bst->dpy, bst->window, bst->gc, x, y, w, h);
      bst->pos++;
      return 0;
    }
  case LINE:
    {
      int x1 = (long) bst->queue[bst->pos].arg1;
      int y1 = (long) bst->queue[bst->pos].arg2;
      int x2 = (long) bst->queue[bst->pos].arg3;
      int y2 = (long) bst->queue[bst->pos].arg4;
      int t  = (long) bst->queue[bst->pos].arg5;
      XGCValues gcv;
      gcv.line_width = t;
      XChangeGC (bst->dpy, bst->gc, GCLineWidth, &gcv);
      XDrawLine (bst->dpy, bst->window, bst->gc, x1, y1, x2, y2);
      bst->pos++;
      return 0;
    }
  case COPY:
  case PIXMAP:
    {
      int srcx = (long) bst->queue[bst->pos].arg1;
      int srcy = (long) bst->queue[bst->pos].arg2;
      int w    = (long) bst->queue[bst->pos].arg3;
      int h    = (long) bst->queue[bst->pos].arg4;
      int tox  = (long) bst->queue[bst->pos].arg5;
      int toy  = (long) bst->queue[bst->pos].arg6;

      /* If ~0, place pixmap at current text pos */
      if (tox == ~0) tox = bst->x;
      if (toy == ~0) toy = bst->y - bst->font->ascent;

      if (type == PIXMAP && bst->mask)
        {
          XSetClipMask (bst->dpy, bst->gc, bst->mask);
          XSetClipOrigin (bst->dpy, bst->gc, tox, toy);
        }
      XCopyArea (bst->dpy,
                 (type == PIXMAP ? bst->pixmap : bst->window),
                 bst->window, bst->gc,
                 srcx, srcy, w, h, tox, toy);
      if (type == PIXMAP && bst->mask)
        XSetClipMask (bst->dpy, bst->gc, None);
      bst->pos++;
      return 0;
    }
  case IMG:
    {
      if (bst->img_loader) abort();
      bst->img_loader = load_image_async_simple (0, bst->xgwa.screen,
                                                 bst->window, bst->window,
                                                 0, 0);
      bst->pos++;
      return 0;
    }
  case FONT:
    {
      switch ((long) bst->queue[bst->pos].arg1) {
      case 0: bst->font = bst->fontA; break;
      case 1: bst->font = bst->fontB; break;
      case 2: bst->font = bst->fontC; break;
      default: abort(); break;
      }
      bst->pos++;
      return 0;
    }
  case PAUSE:
    {
      long delay = (long) bst->queue[bst->pos].arg1;
      bst->pos++;
      return delay;
    }
  case CHAR_DELAY:
    {
      bst->char_delay = (long) bst->queue[bst->pos].arg1;
      bst->pos++;
      return 0;
    }
  case LINE_DELAY:
    {
      bst->line_delay = (long) bst->queue[bst->pos].arg1;
      bst->pos++;
      return 0;
    }
  case MARGINS:
    {
      bst->left_margin  = (long) bst->queue[bst->pos].arg1;
      bst->right_margin = (long) bst->queue[bst->pos].arg2;
      bst->pos++;
      return 0;
    }
  case VERT_MARGINS:
    {
      bst->top_margin    = (long) bst->queue[bst->pos].arg1;
      bst->bottom_margin = (long) bst->queue[bst->pos].arg2;
      bst->pos++;
      return 0;
    }
  case CURSOR_BLOCK:
  case CURSOR_LINE:
    {
      long delay = (long) bst->queue[bst->pos].arg1;
      long count = (long) bst->queue[bst->pos].arg2;
      int ox = bst->x;

      if (type == CURSOR_BLOCK)
        {
          XGlyphInfo ov;
          XftColor cswap = bst->xft_fg;
          unsigned long swap = bst->fg;
          bst->fg = bst->bg;
          bst->xft_fg = bst->xft_bg;
          bst->bg = swap;
          bst->xft_bg = cswap;
          XSetForeground (bst->dpy, bst->gc, bst->fg);
          XSetBackground (bst->dpy, bst->gc, bst->bg);
          XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) "n", 1, &ov);
          XFillRectangle (bst->dpy, bst->window, bst->gc,
                          bst->x, bst->y - bst->font->ascent,
                          ov.width,
                          bst->font->ascent + bst->font->descent);
          draw_char (bst, ' ');
        }
      else
        {
          draw_char (bst, (count & 1 ? ' ' : '_'));
          draw_char (bst, ' ');
        }

      bst->x = ox;

      count--;
      bst->queue[bst->pos].arg2 = (void *) count;
      if (count <= 0)
        bst->pos++;

      return delay;
    }
  case WRAP:
    bst->wrap_p = 1;
    bst->word_wrap_p = 0;
    bst->pos++;
    return 0;

  case WORD_WRAP:
    bst->wrap_p = 0;
    bst->word_wrap_p = 1;
    bst->pos++;
    return 0;

  case TRUNCATE:
    bst->wrap_p = 0;
    bst->word_wrap_p = 0;
    bst->pos++;
    return 0;

  case LOOP:
    {
      long off = (long) bst->queue[bst->pos].arg1;
      bst->pos += off;
      if (bst->pos < 0 || bst->pos >= bst->queue_size)
        abort();
      return 0;
    }
  case RESET:
    {
      int i;
      for (i = 0; i < bst->queue_size; i++)
        switch (bst->queue[i].type) {
        case LEFT:   case LEFT_FULL:
        case CENTER: case CENTER_FULL:
        case RIGHT:  case RIGHT_FULL:
          bst->queue[i].arg2 = bst->queue[i].arg1;
          bst->queue[i].arg3 = 0;
          /* I have no idea what this means, but the extra cast seems to
             silence it: "warning: cast to smaller integer type
             'bsod_event_type' from 'void *' [-Wvoid-pointer-to-enum-cast]" */
          bst->queue[i].type = (bsod_event_type) (size_t)
            bst->queue[i].arg4;
          break;
        default: break;
        }
      bst->pos = 0;
      return 0;
    }
  case CROP:
    {
      bst->crop_p = (long) bst->queue[bst->pos].arg1;
      bst->pos++;
      return 0;
    }
  case EOF:
    {
      bst->pos = -1;
      return -1;
    }
  default:
    break;
  }
  abort();
}


static struct bsod_state *
make_bsod_state (Display *dpy, Window window,
                 const char *name, const char *class)
{
  XGCValues gcv;
  struct bsod_state *bst;
  char buf1[1024], buf2[1024];
  char buf5[1024], buf6[1024];
  char buf7[1024], buf8[1024];
  char *font1, *font3, *font4;
  int screen_no;

  bst = (struct bsod_state *) calloc (1, sizeof (*bst));
  bst->queue_size = 10;
  bst->queue = (struct bsod_event *) calloc (bst->queue_size,
                                             sizeof (*bst->queue));
  bst->dpy = dpy;
  bst->window = window;
  XGetWindowAttributes (dpy, window, &bst->xgwa);
  screen_no = screen_number (bst->xgwa.screen);

  /* If the window is small, use ".font"; if big, ".bigFont". */
  if (
# ifdef HAVE_MOBILE
      1
# else
      bst->xgwa.height < 640
# endif
      )
    {
      sprintf (buf1, "%.100s.font", name);
      sprintf (buf2, "%.100s.font", class);
    }
  else
    {
      sprintf (buf1, "%.100s.bigFont", name);
      sprintf (buf2, "%.100s.bigFont", class);
    }
  sprintf (buf5, "%.100s.fontB", name);
  sprintf (buf6, "%.100s.fontB", class);
  sprintf (buf7, "%.100s.fontC", name);
  sprintf (buf8, "%.100s.fontC", class);

  font1 = get_string_resource (dpy, buf1, buf2);
  font3 = get_string_resource (dpy, buf5, buf6);
  font4 = get_string_resource (dpy, buf7, buf8);

  /* If there was no ".mode.font" resource also look for ".font".
     Under real X11, the wildcard does this, so this is redundant,
     but jwxyz needs it because it doesn't implement wildcards.
   */
# define RES2(VAR, BUF1, BUF2) do {                          \
    if (! VAR) {                                             \
      VAR = get_string_resource (dpy,                        \
				 strchr (BUF1, '.') + 1,     \
				 strchr (BUF2, '.') + 1);    \
    }} while(0)
  RES2 (font1, buf1, buf2);
  RES2 (font3, buf5, buf6);
  RES2 (font4, buf7, buf8);
#undef RES2

  if (font1 && *font1)
    bst->font = load_xft_font_retry (dpy, screen_no, font1);

  if (! bst->font)
    abort();

  if (font3 && *font3)
    bst->fontB = load_xft_font_retry (dpy, screen_no, font3);
  if (font4 && *font4)
    bst->fontC = load_xft_font_retry (dpy, screen_no, font4);

  if (! bst->fontB) bst->fontB = bst->font;
  if (! bst->fontC) bst->fontC = bst->font;

  bst->fontA = bst->font;

  bst->xftdraw = XftDrawCreate (dpy, bst->window, bst->xgwa.visual,
                                bst->xgwa.colormap);

  sprintf (buf1, "%.100s.foreground", name);
  sprintf (buf2, "%.100s.Foreground", class);
  bst->fg = gcv.foreground = get_pixel_resource (dpy, bst->xgwa.colormap,
                                                 buf1, buf2);
  set_xft_color (bst->dpy, bst->xgwa.colormap, &bst->xft_fg, bst->fg);

  sprintf (buf1, "%.100s.background", name);
  sprintf (buf2, "%.100s.Background", class);
  bst->bg = gcv.background = get_pixel_resource (dpy, bst->xgwa.colormap,
                                                 buf1, buf2);

  set_xft_color (bst->dpy, bst->xgwa.colormap, &bst->xft_fg, bst->fg);
  set_xft_color (bst->dpy, bst->xgwa.colormap, &bst->xft_bg, bst->bg);

  bst->gc = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);

#ifdef HAVE_JWXYZ
  jwxyz_XSetAntiAliasing (dpy, bst->gc, True);
#endif

# ifdef HAVE_IPHONE
  /* Stupid iPhone X bezel.
     #### This is the worst of all possible ways to do this!  But how else?
     This magic number should catch iPhone X and larger, but unfortunately
     also catches iPads which do not have the stupid bezel.
   */
  if (bst->xgwa.width >= 1218 || bst->xgwa.height >= 1218) {
    if (bst->xgwa.width > bst->xgwa.height)
      bst->xoff = 96;
    else
      bst->yoff = 96;
  }
# endif

  bst->left_margin = bst->right_margin = 10;
  bst->x = bst->left_margin + bst->xoff;
  bst->y = bst->font->ascent + bst->left_margin + bst->yoff;

  XSetWindowBackground (dpy, window, gcv.background);

  if (font1) free (font1);
  if (font3) free (font3);
  if (font4) free (font4);

  return bst;
}


static void
free_bsod_state (struct bsod_state *bst)
{
  int i;

  if (bst->free_cb)
    bst->free_cb (bst);
  if (bst->pixmap)
    XFreePixmap(bst->dpy, bst->pixmap);
  if (bst->mask)
    XFreePixmap(bst->dpy, bst->mask);

  XftFontClose (bst->dpy, bst->font);
  if (bst->fontB && bst->fontB != bst->font)
    XftFontClose (bst->dpy, bst->fontB);
  if (bst->fontC && bst->fontC != bst->font)
    XftFontClose (bst->dpy, bst->fontC);
  XFreeGC (bst->dpy, bst->gc);

  XftDrawDestroy (bst->xftdraw);

  for (i = 0; i < bst->queue_size; i++)
    switch (bst->queue[i].type) {
    case LEFT:   case LEFT_FULL:
    case RIGHT:  case RIGHT_FULL:
    case CENTER: case CENTER_FULL:
      free ((char *) bst->queue[i].arg1);
      break;
    default:
      break;
    }

  free (bst->queue);
  free (bst);
}


static Pixmap
double_pixmap (Display *dpy, Visual *visual, int depth, Pixmap pixmap,
               int pix_w, int pix_h)
{
  int x, y;
  Pixmap p2 = XCreatePixmap(dpy, pixmap, pix_w*2, pix_h*2, depth);
  XImage *i1 = XGetImage (dpy, pixmap, 0, 0, pix_w, pix_h, ~0L, 
                          (depth == 1 ? XYPixmap : ZPixmap));
  XImage *i2 = XCreateImage (dpy, visual, depth, 
                             (depth == 1 ? XYPixmap : ZPixmap), 0, 0,
                             pix_w*2, pix_h*2, 8, 0);
  XGCValues gcv;
  GC gc = XCreateGC (dpy, p2, 0, &gcv);
  i2->data = (char *) calloc(i2->height, i2->bytes_per_line);
  for (y = 0; y < pix_h; y++)
    for (x = 0; x < pix_w; x++)
      {
	unsigned long p = XGetPixel(i1, x, y);
	XPutPixel(i2, x*2,   y*2,   p);
	XPutPixel(i2, x*2+1, y*2,   p);
	XPutPixel(i2, x*2,   y*2+1, p);
	XPutPixel(i2, x*2+1, y*2+1, p);
      }
  free(i1->data); i1->data = 0;
  XDestroyImage(i1);
  XPutImage(dpy, p2, gc, i2, 0, 0, 0, 0, i2->width, i2->height);
  XFreeGC (dpy, gc);
  free(i2->data); i2->data = 0;
  XDestroyImage(i2);
  XFreePixmap(dpy, pixmap);
  return p2;
}


/*****************************************************************************
 *****************************************************************************/

static struct bsod_state *
windows_31 (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "windows", "Windows");
  int lines = 9;

  bst->xoff = bst->left_margin = bst->right_margin = 0;

  switch (random() % 8) {

  case 0:  /* Windows 3.1 */
  case 1:
  case 2:
    BSOD_INVERT (bst);
    BSOD_TEXT   (bst, CENTER, "Windows\n");
    BSOD_INVERT (bst);
    BSOD_TEXT (bst, CENTER,
      "A fatal exception 0E has occurred at F0AD:42494C4C\n"
      "the current application will be terminated.\n"
      "\n"
      "* Press any key to terminate the current application.\n"
      "* Press CTRL+ALT+DELETE again to restart your computer.\n"
      "  You will lose any unsaved information in all applications.\n"
      "\n"
      "\n");
    BSOD_TEXT (bst, CENTER, "Press any key to continue ");
    BSOD_CURSOR (bst, CURSOR_LINE, 120000, 999999);
    break;

  case 3:  /* Windows 3.1 */
  case 4:
    BSOD_INVERT (bst);
    BSOD_TEXT   (bst, CENTER, "NETSCAPE.EXE\n");
    BSOD_INVERT (bst);
    BSOD_TEXT (bst, CENTER,
      "   This windows application has stopped responding to the system.\n"
      "\n"
      "*  Press ESC to cancel and return to Windows.\n"
      "*  Press ENTER to close this application that is not responding.\n"
      "   You will lose any unsaved information in this application.\n"
      "*  Press CTRL+ALT+DEL again to restart your computer. You will\n"
      "   lose any unsaved information in all applications.\n"
      "\n");
    BSOD_TEXT (bst, CENTER, "Press ENTER for OK or ESC to Cancel: OK\b\b");
    BSOD_CURSOR (bst, CURSOR_LINE, 120000, 999999);
    break;

  case 5:  /* Windows 95 */
    BSOD_INVERT (bst);
    BSOD_TEXT   (bst, CENTER, "Windows\n");
    BSOD_INVERT (bst);
    BSOD_TEXT (bst, CENTER,
      "An exception 00 has occurred at 0028:C18580AE in VxD HSFLOP(03) +\n"
      "0000156E.  This was called from 0028:C1858AED in VxD HSFLOP(03) +\n"
      "0000F0AD.  It may be possible to continue normally.\n"
      "\n"
      "*  Press any key to attempt to continue.\n"
      "*  Press CTRL+ALT+DEL to restart your computer.  You will\n"
      "   lose any unsaved information in all applications.\n"
      "\n");
    BSOD_TEXT (bst, CENTER, "Press any key to continue ");
    BSOD_CURSOR (bst, CURSOR_LINE, 120000, 999999);
    break;

  case 6:  /* Windows 95 */
    BSOD_INVERT (bst);
    BSOD_TEXT   (bst, CENTER, "Windows\n");
    BSOD_INVERT (bst);
    BSOD_TEXT (bst, CENTER,
      "A fatal exception 0E has occurred at F0AD:011747F3.  The current\n"
      "application will be terminated.\n"
      "\n"
      "*  Press any key to terminate the current application.\n"
      "*  Press CTRL+ALT+DEL again to restart your computer. You will\n"
      "   lose any unsaved information in all applications.\n"
      "\n");
    BSOD_TEXT (bst, CENTER, "Press any key to continue ");
    BSOD_CURSOR (bst, CURSOR_LINE, 120000, 999999);
    break;

  case 7:  /* Windows 95 */
    BSOD_INVERT (bst);
    BSOD_TEXT   (bst, CENTER, "WARNING!\n");
    BSOD_INVERT (bst);
    BSOD_TEXT (bst, CENTER,
      "The system is either busy or has become unstable. You can wait and\n"
      "see if it becomes available again, or you can restart your computer.\n"
      "\n"
      "*  Press any key to return to Windows and wait.\n"
      "*  Press CTRL+ALT+DEL again to restart your computer. You will\n"
      "   lose unsaved information in any programs that are running.\n"
      "\n");
    BSOD_TEXT (bst, CENTER, "Press any key to continue ");
    BSOD_CURSOR (bst, CURSOR_LINE, 120000, 999999);
    break;

  default:
    abort();
    break;
  }

  bst->y = ((bst->xgwa.height - bst->yoff -
             ((bst->font->ascent + bst->font->descent) * lines))
            / 2);

  XClearWindow (dpy, window);
  return bst;
}

static struct bsod_state *
vmware (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "vmware", "VMware");

  unsigned long fg = bst->fg;
  unsigned long bg = bst->bg;
  unsigned long fg2 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                          "vmware.foreground2",
                                          "vmware.foreground");
  BSOD_COLOR (bst, fg2, bg);
  BSOD_TEXT   (bst, LEFT,
		"VMware ESX Server [Releasebuild-98103]\n");
  BSOD_COLOR (bst, fg, bg);
  BSOD_TEXT   (bst, LEFT,
    "PCPU 1 locked up. Failed to ack TLB invalidate.\n"
    "frame=0x3a37d98 ip=0x625e94 cr2=0x0 cr3=0x40c66000 cr4=0x16c\n"
    "es=0xffffffff ds=0xffffffff fs=0xffffffff gs=0xffffffff\n"
    "eax=0xffffffff ebx=0xffffffff ecx=0xffffffff edx=0xffffffff\n"
    "ebp=0x3a37ef4 esi=0xffffffff edi=0xffffffff err=-1 eflags=0xffffffff\n"
    "*0:1037/helper1-4 1:1107/vmm0:Fagi 2:1121/vmware-vm 3:1122/mks:Franc\n"
    "0x3a37ef4:[0x625e94]Panic+0x17 stack: 0x833ab4, 0x3a37f10, 0x3a37f48\n"
    "0x3a37f04:[0x625e94]Panic+0x17 stack: 0x833ab4, 0x1, 0x14a03a0\n"
    "0x3a37f48:[0x64bfa4]TLBDoInvalidate+0x38f stack: 0x3a37f54, 0x40, 0x2\n"
    "0x3a37f70:[0x66da4d]XMapForceFlush+0x64 stack: 0x0, 0x4d3a, 0x0\n"
    "0x3a37fac:[0x652b8b]helpFunc+0x2d2 stack: 0x1, 0x14a4580, 0x0\n"
    "0x3a37ffc:[0x750902]CpuSched_StartWorld+0x109 stack: 0x0, 0x0, 0x0\n"
    "0x3a38000:[0x0]blk_dev+0xfd76461f stack: 0x0, 0x0, 0x0\n"
    "VMK uptime: 7:05:43:45.014 TSC: 1751259712918392\n"
    "Starting coredump to disk\n");
  BSOD_CHAR_DELAY (bst, 10000);		
  BSOD_TEXT (bst, LEFT,	"using slot 1 of 1... ");
  BSOD_CHAR_DELAY (bst, 300000);
  BSOD_TEXT (bst, LEFT, "9876");
  BSOD_CHAR_DELAY (bst, 3000000);
  BSOD_TEXT (bst, LEFT, "66665");
  BSOD_CHAR_DELAY (bst, 100000);
  BSOD_TEXT (bst, LEFT, "4321");
  BSOD_CHAR_DELAY (bst, 0);
  BSOD_TEXT (bst, LEFT, "Disk dump successful.\n"
		"Waiting for Debugger (world 1037)\n"
		"Debugger is listening on serial port ...\n");
  BSOD_CHAR_DELAY (bst, 10000);
  BSOD_TEXT (bst, LEFT, "Press Escape to enter local debugger\n");
  BSOD_CHAR_DELAY (bst, 10000);
  BSOD_TEXT (bst, LEFT,
    "Remote debugger activated. Local debugger no longer available.\n");

/*  BSOD_CURSOR (bst, CURSOR_LINE, 240000, 999999);*/
		
/*  bst->y = ((bst->xgwa.height - bst->yoff -
             ((bst->font->ascent + bst->font->descent) * 9))
            / 2);*/

  XClearWindow (dpy, window);
  return bst;
}

/* VMware ESXi 7.0 on a 64-bit Arm host (actually ESXi-Arm Fling)
   by Andrei E. Warkentin <andreiw@mm.st>.
 */
static struct bsod_state *
vmware_arm (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window,
                                            "vmware-arm", "VMwareArm");

  unsigned i = 2;
  unsigned msg_row;
  unsigned long fg = bst->fg;
  char t_string[25];
  time_t t_now = time(NULL);
  unsigned long font_height = bst->font->ascent + bst->font->descent;

  unsigned long psod_bg = get_pixel_resource(dpy, bst->xgwa.colormap,
					    "vmware-arm-psod.background",
					    "vmware-arm-psod.background");
  unsigned long term_bg = get_pixel_resource(dpy, bst->xgwa.colormap,
					    "vmware-arm-term.background",
					    "vmware-arm-term.background");
  unsigned long term_fg2 = get_pixel_resource(dpy, bst->xgwa.colormap,
					     "vmware-arm-term.foreground2",
					     "vmware-arm-term.foreground2");
  unsigned long term_fg3 = get_pixel_resource(dpy, bst->xgwa.colormap,
					     "vmware-arm-term.foreground3",
					     "vmware-arm-term.foreground3");
  unsigned long dbg_bg = get_pixel_resource(dpy, bst->xgwa.colormap,
					    "vmware-arm-dbg.background",
					    "vmware-arm-dbg.background");
  unsigned long dbg_fg = get_pixel_resource(dpy, bst->xgwa.colormap,
					    "vmware-arm-dbg.foreground",
					    "vmware-arm-dbg.foreground");
  unsigned long fg2 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                          "vmware-arm.foreground2",
                                          "vmware-arm.foreground2");

  BSOD_WRAP (bst);
  BSOD_MARGINS(bst, 0, 0);
  BSOD_VERT_MARGINS(bst, 0, 0);

  /*
   * statusterm.
   */
  BSOD_TRUNCATE(bst);
  BSOD_COLOR (bst, term_bg, fg);
  BSOD_RECT (bst, True, 0, 0, bst->xgwa.width, bst->xgwa.height);
  BSOD_MOVETO(bst, 0, font_height * 6);
  BSOD_COLOR (bst, fg, term_bg);
  BSOD_TEXT (bst, LEFT,
    "                VMware ESXi 7.0.0 (VMKernel Release Build 19076756)");
  BSOD_MOVETO(bst, 0, font_height * 8);
  BSOD_COLOR (bst, fg, term_bg);
  BSOD_TEXT (bst, LEFT, "                PINE64 Quartz64 Model A");
  BSOD_MOVETO(bst, 0, font_height * 10);
  BSOD_COLOR (bst, term_fg2, term_bg);
  BSOD_TEXT (bst, LEFT, "                ARM Limited Cortex-A55 r2p0");
  BSOD_MOVETO(bst, 0, font_height * 11);
  BSOD_COLOR (bst, term_fg2, term_bg);
  BSOD_TEXT (bst, LEFT, "                7.7 GiB Memory");

  msg_row = 1 + (bst->xgwa.height / (font_height * 2));
  if (msg_row > 11) {
    BSOD_WRAP(bst);
    BSOD_MOVETO(bst, 0, font_height * msg_row);
    BSOD_COLOR (bst, term_fg3, term_bg);
    strftime(t_string, sizeof(t_string), "%Y-%m-%dT%H:%M:%S.000Z",
             localtime(&t_now));
    BSOD_TEXT (bst, LEFT, t_string);
    BSOD_TEXT (bst, LEFT,
      " cpu0:65802)Failed to verify signatures of the following vib(s):"
      " [bnxtnet bnxtroce brcmfcoe brcmnvmefc elx-esx-libelxima.so"
      " eslxiscsi elxnet ena esx base esx-dvfilter-generic-fastpath"
      " esx-ui esx-update i40en i40iwn iavmd igbn iser$");
  }

  BSOD_PAUSE (bst, 10000000);

  /*
   * Now the PSOD.
   */
  BSOD_TRUNCATE(bst);
  BSOD_COLOR (bst, psod_bg, fg);
  BSOD_RECT (bst, True, 0, 0, bst->xgwa.width, bst->xgwa.height);
  BSOD_MOVETO(bst, 0, bst->font->ascent);
  BSOD_COLOR (bst, fg2, psod_bg);
  BSOD_TEXT   (bst, LEFT,
		"VMware ESX 7.0.0 [Releasebuild-19076756 aarch64]\n");
  BSOD_COLOR (bst, fg, psod_bg);
  BSOD_LINE_DELAY (bst, 1000);
  BSOD_TEXT   (bst, LEFT,
    "EXCVEC_CUREL_SP_EL0_SYNCH Exception 0 in world 131126:HELPER_UPLIN"
    " (ec 0x25 il 1 iss 0x47 far_el1 0x315d3541b8 far_el2 0x4501843b1000)"
    "\nTTB=0x12ade8000"
    "\nCurrentEL=2 SP_EL0 DAIF"
    "\nSCTLR_EL2=0x30c0180d sa0 SA C a M"
    "\n[ 0]     4501843b0000 [ 1]                0 [ 2]"
    "             1000 [ 3]     4501843b1000"
    "\n[ 4]     451a01b1be20 [ 5]                0 [ 6]"
    "     4305be607a5a [ 7]     4200400001c0"
    "\n[ 8]                0 [ 9]     451a01b1be20 [10]"
    "                1 [11]                1"
    "\n[12]         ffffed40 [13]     420040000080 [14]"
    "     41fffa5d7000 [15]     41fffa5d7c20"
    "\n[16]                1 [17]                4 [18]"
    "     41fffa5d7c08 [19]     451a01b1be70"
    "\n[20]     43024240b680 [21]     4305be601900 [22]"
    "     4305be6012c0 [23]     41ffd0c00000"
    "\n[24]     4305be601220 [25]          bad0001 [26]"
    "                0 [27]     43006fc01220"
    "\n[28]     4303d5001220 [29]                0 [30]     42003b3ceb6c"
    "\n[pc]     42003a344d54 [sp]     451a01b1bdd0 [psr]        20000248"
    "\n*PCPU0:131126/HELPER_UPLINK_ASYNC_CALL_QUEUE"
    "\nPCPU  0: SUUU"
    "\nCode start: 0x42003a200000 VMK uptime: 0:00:05:35.425"
    "\n0x451a01b1bdd0:[0x42003a344d54]vmk_Memset@vmkernel#nover+0x28"
    " stack: 0x42003b3cfa48"
    "\n0x451a01b1bdd0:[0x42003b3ceb68]EQOSEnable@(eqos)#<None>+0xe0"
    " stack: 0x42003b3cfa48"
    "\n0x451a01b1be20:[0x42003b3c9220]SETHUplAssociate@(eqos)#<None>+0x88"
    " stack: 0x43024240b680"
    "\n0x451a01b1be80:[0x42003a48a078]UplinkDeviceAssociateAsyncCB"
    "@vmkernel#nover+0x50 stack: 0x43024240bb08"
    "\n0x451a01b1bed0:[0x42003a555a6c]UplinkAsyncProcessCallsHelperCB"
    "@vmkernel#nover+0x12c stack: 0x451a01b21000"
    "\n0x451a01b1bf20:[0x42003a2fd510]HelperQueueFunc@vmkernel#nover+0x174"
    " stack: 0x451a01b21100"
    "\n0x451a01b1bfe0:[0x42003a59e4fc]CpuSched_StartWorld@vmkernel#nover+0x70"
    " stack: 0x0"
    "\n0x451a01b1c000:[0x42003a5ec610]CpuSched_UseMwaitCallback"
    "@vmkernel#nover+0x8 stack: 0x0"
    "\nNo place on disk to dump data."
    "\nCoredump to file: /vmfs/volumes/3a4fcb25-5f6ca096-c940-70886b86100c"
    "/vmkdump/00000000-0000-0000-0000-000000000000.dumpfile."
    "\nFaulting world regs (01/15)");
  BSOD_PAUSE (bst, 300000);
  BSOD_TEXT (bst, LEFT, "\nVmm code/data (02/15)");
  BSOD_PAUSE (bst, 300000);
  BSOD_TEXT (bst, LEFT,  "\nVmk code/rodata/stack (03/15)");
  BSOD_PAUSE (bst, 300000);
  BSOD_TEXT (bst, LEFT, "\nVmk data/heap (04/15)");
  BSOD_PAUSE (bst, 300000);
  BSOD_TEXT (bst, LEFT, "\nPCPU (05/15)");
  BSOD_PAUSE (bst, 300000);
  BSOD_TEXT (bst, LEFT, "\nWorld-specific data (06/15)");
  BSOD_PAUSE (bst, 300000);
  BSOD_TEXT (bst, LEFT, "\nVASpace (08/15)");
  BSOD_PAUSE (bst, 1000000);
  BSOD_TEXT (bst, LEFT,	"\nPFrame (09/15)");
  BSOD_PAUSE (bst, 1000000);
  BSOD_TEXT (bst, LEFT, "\nMemXferFs (11/15)");
  BSOD_PAUSE (bst, 300000);
  BSOD_TEXT (bst, LEFT, "\nDump Files (13/15)");
  BSOD_PAUSE (bst, 600000);
  BSOD_TEXT (bst, LEFT, "\nCollecting userworld dumps (14/15)");
  BSOD_PAUSE (bst, 600000);
  BSOD_TEXT (bst, LEFT,
    "\nFinalized dump header (15/15) FileDump: Successful.");
  BSOD_PAUSE (bst, 10000);
  BSOD_TEXT (bst, LEFT,
    "\nNo port for remote debugger. Press \"Escape\" for local debugger.");
  BSOD_PAUSE (bst, 1000000);

  /*
   * Local debugger.
   */
  BSOD_COLOR (bst, dbg_bg, dbg_fg);
  BSOD_RECT (bst, True, 0, 0, bst->xgwa.width, bst->xgwa.height);
  BSOD_MOVETO(bst, 0, bst->font->ascent);
  BSOD_TEXT (bst, LEFT, "vmkernel debugger (h for help)");
  BSOD_RECT (bst, True, 0, bst->font->ascent + bst->font->descent / 2,
	     bst->xgwa.width, bst->font->ascent);
  BSOD_COLOR (bst, dbg_fg, dbg_bg);
  BSOD_TEXT (bst, LEFT, "\n[PCPU2] VMKDBG> _");
  while (i--) {
    BSOD_PAUSE (bst, 1000000);
    BSOD_TEXT (bst, LEFT, "\bh_");
    BSOD_PAUSE (bst, 10000);
    BSOD_TEXT (bst, LEFT, "\b \nh       : help"
	       "\nreboot  : reboot"
	       "\nlivedump: Create live coredump without crashing system"
	       "\nl       : display vmkernel log"
	       "\np       : display content of symbol"
	       "\ns       : display storage dump+boot info"
	       "\nx       : display 32-bit content of address"
	       "\nx/N     : display N bytes of content at address"
	       "\ng port  : bind remote debugger to port (com1 or com2)"
	       "\nbt N    : show backtrace for CPU N"
	       "\nq       : quit debug terminal"
	       "\n[PCPU2] VMKDBG> _");
  }
  BSOD_PAUSE (bst, 100000);
  BSOD_TEXT (bst, LEFT, "\bre_");
  BSOD_PAUSE (bst, 500000);
  BSOD_TEXT (bst, LEFT, "\bb_");
  BSOD_PAUSE (bst, 50000);
  BSOD_TEXT (bst, LEFT, "\bo_");
  BSOD_PAUSE (bst, 50000);
  BSOD_TEXT (bst, LEFT, "\bo_");
  BSOD_PAUSE (bst, 50000);
  BSOD_TEXT (bst, LEFT, "\bt_");
  BSOD_PAUSE (bst, 2000000);
  BSOD_COLOR (bst, dbg_bg, dbg_fg);
  BSOD_RECT (bst, True, 0, 0, bst->xgwa.width, bst->xgwa.height);
  BSOD_PAUSE (bst, 1000000);

  XClearWindow (dpy, window);
  return bst;
}


/* Windows NT 3.1 - 4.0
 */
static struct bsod_state *
windows_nt (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "nt", "NT");

  switch (random() % 4) {
  case 0: case 1: case 2:
    BSOD_TEXT (bst, LEFT,
   "*** STOP: 0x0000001E (0x80000003,0x80106fc0,0x8025ea21,0xfd6829e8)\n"
   "Unhandled Kernel exception c0000047 from fa8418b4 (8025ea21,fd6829e8)\n"
   "\n"
   "Dll Base Date Stamp - Name             Dll Base Date Stamp - Name\n"
   "80100000 2be154c9 - ntoskrnl.exe       80400000 2bc153b0 - hal.dll\n"
   "80258000 2bd49628 - ncrc710.sys        8025c000 2bd49688 - SCSIPORT.SYS \n"
   "80267000 2bd49683 - scsidisk.sys       802a6000 2bd496b9 - Fastfat.sys\n"
   "fa800000 2bd49666 - Floppy.SYS         fa810000 2bd496db - Hpfs_Rec.SYS\n"
   "fa820000 2bd49676 - Null.SYS           fa830000 2bd4965a - Beep.SYS\n"
   "fa840000 2bdaab00 - i8042prt.SYS       fa850000 2bd5a020 - SERMOUSE.SYS\n"
   "fa860000 2bd4966f - kbdclass.SYS       fa870000 2bd49671 - MOUCLASS.SYS\n"
   "fa880000 2bd9c0be - Videoprt.SYS       fa890000 2bd49638 - NCC1701E.SYS\n"
   "fa8a0000 2bd4a4ce - Vga.SYS            fa8b0000 2bd496d0 - Msfs.SYS\n"
   "fa8c0000 2bd496c3 - Npfs.SYS           fa8e0000 2bd496c9 - Ntfs.SYS\n"
   "fa940000 2bd496df - NDIS.SYS           fa930000 2bd49707 - wdlan.sys\n"
   "fa970000 2bd49712 - TDI.SYS            fa950000 2bd5a7fb - nbf.sys\n"
   "fa980000 2bd72406 - streams.sys        fa9b0000 2bd4975f - ubnb.sys\n"
   "fa9c0000 2bd5bfd7 - usbser.sys         fa9d0000 2bd4971d - netbios.sys\n"
   "fa9e0000 2bd49678 - Parallel.sys       fa9f0000 2bd4969f - serial.SYS\n"
   "faa00000 2bd49739 - mup.sys            faa40000 2bd4971f - SMBTRSUP.SYS\n"
   "faa10000 2bd6f2a2 - srv.sys            faa50000 2bd4971a - afd.sys\n"
   "faa60000 2bd6fd80 - rdr.sys            faaa0000 2bd49735 - bowser.sys\n"
   "\n"
   "Address dword dump Dll Base                                      - Name\n"
   "801afc20 80106fc0 80106fc0 00000000 00000000 80149905 : "
     "fa840000 - i8042prt.SYS\n"
   "801afc24 80149905 80149905 ff8e6b8c 80129c2c ff8e6b94 : "
     "8025c000 - SCSIPORT.SYS\n"
   "801afc2c 80129c2c 80129c2c ff8e6b94 00000000 ff8e6b94 : "
     "80100000 - ntoskrnl.exe\n"
   "801afc34 801240f2 80124f02 ff8e6df4 ff8e6f60 ff8e6c58 : "
     "80100000 - ntoskrnl.exe\n"
   "801afc54 80124f16 80124f16 ff8e6f60 ff8e6c3c 8015ac7e : "
     "80100000 - ntoskrnl.exe\n"
   "801afc64 8015ac7e 8015ac7e ff8e6df4 ff8e6f60 ff8e6c58 : "
     "80100000 - ntoskrnl.exe\n"
   "801afc70 80129bda 80129bda 00000000 80088000 80106fc0 : "
     "80100000 - ntoskrnl.exe\n"
   "\n"
   "Kernel Debugger Using: COM2 (Port 0x2f8, Baud Rate 19200)\n"
   "Restart and set the recovery options in the system control panel\n"
   "or the /CRASHDEBUG system start option. If this message reappears,\n"
   "contact your system administrator or technical support group."
   );

  bst->line_delay = 750;
  break;

  case 3:
    BSOD_TEXT (bst, CENTER,
     "Microsoft (R) Windows NT (R) Version 5.0 (Build 1796)\n"
     "1 System Processor [128 MB Memory] MultiProcessor Kernel\n"
     "\n"
     "*** STOP: 0x0000006B (0xC000003A, 0x00000002,0x00000000,0x00000000)\n"
     "PROCESS1_INITIALIZATION_FAILED\n"
     "\n"
     "If this is the first time you[ve seen this Stop error screen,\n"
     "restart your computer. If this screen appears again, follow\n"
     "these steps:\n"
     "\n"
     "Check to make sure any new hardware or software is properly installed.\n"
     "If this is a new installation, ask your hardware or software"
     " manufacturer\n"
     "for any Windows NT updates you might need.\n"
     "\n"
     "If problems continue, disable or remove any newly installed hardware\n"
     "or software. Disable BIOS memory options such as caching or shadowing.\n"
     "If you need to use Safe Mode to remove or disable components, restart\n"
     "your computer, press F8 to select Advanced Startup Options, and then\n"
     "select Safe Mode.\n"
     "\n"
     "Refer to your Getting Started manual for more information on\n"
     "troubleshooting Stop errors.\n"
     "\n");
    break;
  default:
    abort();
  }

  XClearWindow (dpy, window);
  return bst;
}


static struct bsod_state *
windows_2k (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "windows", "Windows");

  switch (random() % 4) {
  case 0: case 1: case 2:
    BSOD_TEXT (bst, LEFT,
    "*** STOP: 0x000000D1 (0xE1D38000,0x0000001C,0x00000000,0xF09D42DA)\n"
    "DRIVER_IRQL_NOT_LESS_OR_EQUAL \n"
    "\n"
    "*** Address F09D42DA base at F09D4000, DateStamp 39f459ff - CRASHDD.SYS\n"
    "\n"
    "Beginning dump of physical memory\n");
    BSOD_PAUSE (bst, 4000000);
    BSOD_TEXT (bst, LEFT,
      "Physical memory dump complete. Contact your system administrator or\n"
      "technical support group.\n");

    bst->left_margin = 40;
    bst->y = (bst->font->ascent + bst->font->descent) * 10;
    bst->line_delay = 750;
    break;

  case 3:
    BSOD_TEXT (bst, CENTER,
      "\n\n\n"
      "*** STOP: 0x0000007B (0xF641F84C,0xC00000034,0x00000000,0x00000000)\n"
      "INACCESSIBLE_BOOT_DEVICE\n"
      "\n"
      "If this is the first time you[ve seen this Stop error screen,\n"
      "restart your computer. If this screen appears again, follow\n"
      "these steps:\n"
      "\n"
      "Check for viruses on your computer. Remove any newly installed\n"
      "hard drives or hard drive controllers. Chcek your hard drive\n"
      "to make sure it is properly configured and terminated.\n"
      "Run CHKDSK /F to check for hard drive corruption, and then\n"
      "restart your computer.\n"
      "\n"
      "Refer to your Getting Started manual for more information on\n"
      "troubleshooting Stop errors.\n"
      "\n"
      "\n");
    break;

  default:
    abort();
  }

  XClearWindow (dpy, window);
  return bst;
}


static struct bsod_state *
windows_me (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "windows", "Windows");

  switch (random() % 3) {
  case 0: case 1:
    BSOD_TEXT (bst, LEFT,
      "Windows protection error.  You need to restart your computer.\n\n"
      "System halted.");
    BSOD_CURSOR (bst, CURSOR_LINE, 120000, 999999);

    bst->left_margin = 40;
    bst->y = ((bst->xgwa.height - bst->yoff -
               ((bst->font->ascent + bst->font->descent) * 3))
              / 2);
    break;
  case 2:
    BSOD_INVERT (bst);
    BSOD_TEXT   (bst, CENTER, "Windows\n");
    BSOD_INVERT (bst);
    BSOD_TEXT (bst, CENTER,
      "\n"
      "An error has occurred. To continue:\n"
      "\n"
      "Press Enter to return to Windows, or\n"
      "\n"
      "Press CTRL+ALT+DEL to restart your computer. If you do this,\n"
      "you will lose any unsaved information in all open applications.\n"
      "\n"
      "Error: 0E : 015F : FOAD0D0D\n"
      "\n");
    BSOD_TEXT (bst, CENTER, "Press any key to continue ");
    BSOD_CURSOR (bst, CURSOR_LINE, 120000, 999999);
    bst->y = ((bst->xgwa.height - bst->yoff -
               ((bst->font->ascent + bst->font->descent) * 11))
              / 2);
    break;
  default:
    abort();
  }

  XClearWindow (dpy, window);
  return bst;
}


static struct bsod_state *
windows_xp (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "windows", "Windows");

  switch (random() % 6) {
  case 0: case 1: case 2:
    BSOD_TEXT (bst, LEFT,  /* From Wm. Rhodes <xscreensaver@27.org> */
      "A problem has been detected and windows has been shut down to prevent "
      "damage\n"
      "to your computer.\n"
      "\n"
      "If this is the first time you've seen this Stop error screen,\n"
      "restart your computer. If this screen appears again, follow\n"
      "these steps:\n"
      "\n"
      "Check to be sure you have adequate disk space. If a driver is\n"
      "identified in the Stop message, disable the driver or check\n"
      "with the manufacturer for driver updates. Try changing video\n"
      "adapters.\n"
      "\n"
      "Check with your hardware vendor for any BIOS updates. Disable\n"
      "BIOS memory options such as caching or shadowing. If you need\n"
      "to use Safe Mode to remove or disable components, restart your\n"
      "computer, press F8 to select Advanced Startup Options, and then\n"
      "select Safe Mode.\n"
      "\n"
      "Technical information:\n"
      "\n"
      "*** STOP: 0x0000007E (0xC0000005,0xF88FF190,0x0xF8975BA0,0xF89758A0)\n"
      "\n"
      "\n"
      "***  EPUSBDSK.sys - Address F88FF190 base at FF88FE000, datestamp "
      "3b9f3248\n"
      "\n"
      "Beginning dump of physical memory\n");
    BSOD_PAUSE (bst, 4000000);
    BSOD_TEXT (bst, LEFT,
      "Physical memory dump complete.\n"
      "Contact your system administrator or technical support group for "
      "further\n"
      "assistance.\n");
    break;
  case 3:  /* Windows XP/Vista/7 */
    BSOD_TEXT (bst, LEFT,
      "STOP: C0000021a {Fatal System Error}\n"
      "The Session Manager Initialization system process terminated"
      " unexpectedly\n"
      "with a status of 0x00000001 (0x00000000 0x00000000).\n"
      "The system has been shut down.\n");
    break;
  case 4:  /* Windows CE */
    BSOD_TEXT (bst, LEFT,
      "A error has occurred and Windows CE has been shut down to prevent\n"
      "damage to your computer.\n"
      "If you will try to restart your computer, press Ctrl+Alt+Delete.\n"
      "\n"
      "Technical information:\n"
      "\n"
      "*** STOP: 0x0004c2 (inaccessible embedded device)\n"
      "\n"
      "\n"
      "The computer will restart automatically\n"
      "after 23 seconds.\n");
    break;
  case 5:  /* Windows 8 */
    BSOD_TEXT (bst, LEFT,
     "A problem has been detected and windows has been shut down to prevent\n"
     "damage to your computer.\n"
     "\n"
     "SYSTEM_SERVICE_EXCEPTION\n"
     "\n"
     "If this is the first time you[ve seen this Stop error screen,\n"
     "restart your computer. If this screen appears again, follow\n"
     "these steps:\n"
     "\n"
     "Check to make sure any new hardware or software is properly installed.\n"
     "If this is a new installation, ask your hardware or software"
     " manufacturer\n"
     "for any Windows NT updates you might need.\n"
     "\n"
     "If problems continue, disable or remove any newly installed hardware\n"
     "or software. Disable BIOS memory options such as caching or shadowing.\n"
     "If you need to use Safe Mode to remove or disable components, restart\n"
     "your computer, press F8 to select Advanced Startup Options, and then\n"
     "select Safe Mode.\n"
     "\n"
     "Technical information:\n"
     "\n"
     "*** STOP: 0x0000003B (0x00000000c000005,0xFFFFF880041C9062,"
              "0xFFFFF88002E22F60,0x0000000000000000(\n"
     "\n"
     "***   dxgmms1.sys - Address FFFFF880041C9062 base at FFFFF8800418F000,"
              " DateStamp 4cdb7409\n"
     "\n"
     "Collecting data for crash dump ...\n");
    BSOD_PAUSE (bst, 4000000);
    BSOD_TEXT (bst, LEFT,
      "Initializing disk for for crash dump ...\n");
    break;
  default:
    abort();
  }

  XClearWindow (dpy, window);
  return bst;
}


static struct bsod_state *
windows_lh (Display *dpy, Window window)
{
  struct bsod_state *bst =
    make_bsod_state (dpy, window, "windowslh", "WindowsLH");

  unsigned long fg = bst->fg;
  unsigned long bg = bst->bg;
  unsigned long bg2 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                          "windowslh.background2",
                                          "WindowsLH.Background");

  /* The "RSOD" that appeared with "Windows Longhorn 5048.050401-0536_x86fre"
     As reported by http://joi.ito.com/RedScreen.jpg
   */
  BSOD_COLOR (bst, bg, bg2);
  BSOD_TEXT (bst, CENTER_FULL, "Windows Boot Error\n");
  BSOD_COLOR (bst, fg, bg);
  BSOD_TEXT (bst, CENTER,
     "\n"
     "Windows Boot Manager has experienced a problem.\n"
     "\n"
     "\n"
     "    Status: 0xc000000f\n"
     "\n"
     "\n"
     "\n"
     "    Info: An error occurred transferring exectuion."  /* (sic) */
     "\n"
     "\n"
     "\n"
     "You can try to recover the system with the Microsoft Windows "
     "System Recovery\n"
     "Tools. (You might need to restart the system manually.)\n"
     "\n"
     "If the problem continues, please contact your system administrator "
     "or computer\n"
     "manufacturer.\n"
     );
  BSOD_MOVETO (bst, bst->left_margin + bst->xoff,
               bst->xgwa.height - bst->yoff - bst->font->descent);
  BSOD_COLOR (bst, bg, bg2);
  BSOD_TEXT (bst, LEFT_FULL, " SPACE=Continue\n");

  bst->y = bst->font->ascent;

  XClearWindow (dpy, window);
  return bst;
}


static struct bsod_state *
windows_10_update (Display *dpy, Window window)
{
  struct bsod_state *bst =
    make_bsod_state (dpy, window, "win10", "Win10");
  const char *fmt  = "Working on updates  %d%%";
  char        line1[255];
  const char *line2 = "Don't turn off your PC. This will take a while.";
  const char *line3 = "Your PC will restart several times.";
  int line_height = bst->fontA->ascent + bst->fontA->descent;
  int y1 = bst->xgwa.height / 2 - line_height * 4;
  int y2 = bst->xgwa.height - bst->yoff - line_height * 3;
  int pct;

  /* TODO: win10_spinner.gif goes at (y - line_height * 2 - 64). */

  for (pct = 0; pct < 98; pct++)
    {
      BSOD_MOVETO (bst, 0, y1);
      sprintf (line1, fmt, pct);
      BSOD_TEXT (bst, CENTER, line1);
      BSOD_MOVETO (bst, 0, y1 + line_height);
      BSOD_TEXT (bst, CENTER, line2);
      BSOD_MOVETO (bst, 0, y2);
      BSOD_TEXT (bst, CENTER, line3);
      BSOD_PAUSE (bst, 200000 + (random() % 3000000) + (random() % 3000000));
    }

  XClearWindow (dpy, window);
  return bst;
}


static struct bsod_state *
windows_10_recovery (Display *dpy, Window window)
{
  struct bsod_state *bst =
    make_bsod_state (dpy, window, "win10r", "Win10r");
  const char *line1 = "Recovery";
  const char *line2 = "It looks like Windows didn't load correctly";
  const char *line3 = "If you'd like to restart and try again, choose "
		      "\"Restart my PC\" below. Otherwise, choose "
		      "\"See advanced repair options\" for troubleshooting "
		      "tools and advanced options. If you don't know which "
		      "option is right for you, contact someone you "
		      "trust to help with this.";
  const char *line4 = "  See advanced repair options  ";
  const char *line5 = "  Restart my PC  ";
  int line_height  = bst->fontA->ascent + bst->fontA->descent;
  int line_heightB = bst->fontB->ascent + bst->fontB->descent;
  int line_heightC = bst->fontC->ascent + bst->fontC->descent;
  int y = line_heightC * 2;
  int x, x2, x3;
  XGlyphInfo ov;

  XftTextExtentsUtf8 (bst->dpy, bst->fontB, (FcChar8 *) line2, strlen(line2),
                      &ov);
  x = (bst->xgwa.width - (ov.xOff * 1.7)) / 2;
  if (x < 10) x = 10;
  BSOD_MARGINS (bst, x, x);

  BSOD_WORD_WRAP (bst);
  BSOD_FONT (bst, 2);
  BSOD_MOVETO (bst, 0, y);
  BSOD_TEXT (bst, LEFT, line1);
  BSOD_TEXT (bst, LEFT, "\n");

  BSOD_FONT (bst, 1);
  BSOD_TEXT (bst, LEFT, line2);
  BSOD_TEXT (bst, LEFT, "\n");

  BSOD_FONT (bst, 0);
  BSOD_TEXT (bst, LEFT, line3);

  XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) line5, strlen(line5),
                      &ov);
  x2 = bst->xgwa.width - x - ov.xOff;

  if ( bst->xgwa.width >  bst->xgwa.height)
    y += line_heightB * 2 + line_height * 7;
  else
    y += line_heightB * 3 + line_height * 9;

  BSOD_TRUNCATE (bst);
  BSOD_MOVETO (bst, x2, y);
  BSOD_TEXT (bst, LEFT, line5);
  BSOD_RECT (bst, False, x2, y - line_height, ov.xOff,
             line_height + bst->fontA->descent * 2);

  XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) line4, strlen(line4),
                      &ov);
  x3 = x2 - ov.xOff - line_height;

  BSOD_TRUNCATE (bst);
  BSOD_MOVETO (bst, x3, y);
  BSOD_TEXT (bst, LEFT, line4);
  BSOD_RECT (bst, False, x3, y - line_height, ov.xOff,
             line_height + bst->fontA->descent * 2);


  XClearWindow (dpy, window);
  return bst;
}


static struct bsod_state *
windows_10 (Display *dpy, Window window)
{
  struct bsod_state *bst =
    make_bsod_state (dpy, window, "win10", "Win10");

  int qr_width  = 41;
  int qr_height = 41;
  static const unsigned char qr_bits[] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0x01,0xFF,0xFF,0xFF,0xFF,0xFF,0x01,
    0x03,0x9A,0x70,0xEE,0x80,0x01,0xFB,0x22,0xAA,0xA6,0xBE,0x01,
    0x8B,0x8E,0x74,0xE7,0xA2,0x01,0x8B,0xEE,0x42,0xC4,0xA2,0x01,
    0x8B,0x42,0x6E,0xED,0xA2,0x01,0xFB,0xDA,0x63,0xA6,0xBE,0x01,
    0x03,0xAA,0xAA,0xAA,0x80,0x01,0xFF,0x8B,0xD8,0x9D,0xFF,0x01,
    0x63,0x62,0xDA,0x1B,0x98,0x01,0x6F,0x67,0x98,0x9F,0xBC,0x01,
    0x4F,0xCC,0x55,0x81,0x83,0x01,0xB7,0x6D,0xFF,0x68,0xB2,0x01,
    0xC3,0x10,0x87,0x8B,0x96,0x01,0x6F,0xB1,0x91,0x58,0x94,0x01,
    0xE3,0x36,0x88,0x84,0xB8,0x01,0x83,0x9B,0xFE,0x59,0xD7,0x01,
    0x3B,0x74,0x98,0x5C,0xB4,0x01,0x37,0x75,0xDC,0x91,0xA6,0x01,
    0x77,0xDE,0x01,0x54,0xBA,0x01,0xBB,0x6D,0x8B,0xB9,0xB5,0x01,
    0x1F,0x06,0xBD,0x9B,0xB4,0x01,0xD3,0xBD,0x91,0x19,0x84,0x01,
    0x0B,0x20,0xD8,0x91,0xB4,0x01,0x33,0x95,0xBC,0x0A,0xD5,0x01,
    0xB3,0x60,0xDC,0xD9,0xB6,0x01,0xEF,0x77,0x18,0x09,0xA4,0x01,
    0xA3,0xC2,0x95,0x51,0xB2,0x01,0xDF,0x63,0xDB,0xBE,0xB3,0x01,
    0x03,0x08,0xC9,0x09,0xF0,0x01,0xFF,0xA3,0x19,0xBD,0xFB,0x01,
    0x03,0x2E,0x84,0xA5,0xAA,0x01,0xFB,0x9A,0xFC,0x9B,0xBB,0x01,
    0x8B,0x7E,0x9C,0x1D,0xB0,0x01,0x8B,0x6E,0x58,0xA1,0xDB,0x01,
    0x8B,0xDA,0xD5,0x65,0xA2,0x01,0xFB,0x72,0xFB,0xE9,0xF0,0x01,
    0x03,0x02,0x99,0x3B,0xB3,0x01,0xFF,0xFF,0xFF,0xFF,0xFF,0x01,
    0xFF,0xFF,0xFF,0xFF,0xFF,0x01};
  Pixmap pixmap;

  const char * const lines1[] = {
    ":(",

    "\n"
    "Your PC ran into a problem and needs to restart. We're just"
    " collecting some error info, and then we'll restart for you."
    "\n\n\n",

    "\n\n",

    "For more information about this issue and possible fixes, visit\n"
    "http://youtu.be/-RjmN9RZyr4\n\n"
    "If you call a support person, give them this info:\n"
    "Stop code CRITICAL_PROCESS_DIED",
  };
  const char * const lines2[] = {
    "-\\_(:/)_/-",   /* ¯\\_(ツ)_/¯   BSOD_TEXT() does not support UTF-8. */

    "\n"
    "Your PC ran into a ClownStrike and needs to restart 15 or more times."
    "\n\n\n",

    "\n\n",

    "For more information about this issue and possible fixes, visit\n"
    "http://youtu.be/-RjmN9RZyr4\n\n"
    "If you call a support person, give them this info:\n"
    "Stop code COMPLIANCE_LINE_ITEM_OK",
  };
  const char * const lines3[] = {
    "x__x",

    "\n"
    "This place is a message... "
      "and part of a system of messages... "
      "pay attention to it! "
      "\n\n"
      "Sending this message was important to us. "
      "We considered ourselves to be a powerful culture."
      "\n\n",

    "\nThis place is not a place of honor... "
      "no highly esteemed deed is commemorated here... "
      "nothing valued is here. "
      "\n\n"
      "What is here was dangerous and repulsive to us. "
      "This message is a warning about danger. "
      "\n",

      "The danger is in a particular location... "
      "it increases towards a center... "
      "the center of danger is here... "
      "of a particular size and shape, and below us. "
      "\n\n"
      "The danger is still present, in your time, as it was in ours. "
      "The danger is to the body, and it can kill. "
      "\n\n"
      "This place is best shunned and left uninhabited."
  };
  int i, y = 0;
  int top, left0, left, right, y1 = 0;
  Bool clownp = !(random() % 10);
  Bool honorp = !clownp && !(random() % 20);
  const char * const * lines = (clownp ? lines2 : honorp ? lines3 : lines1);
  int stop = 60 + (random() % 39) + (clownp ? 1300 : 0);

  if (!(random() % 4))
    return windows_10_recovery (dpy, window);

  if (!(random() % 14))
    return windows_10_update (dpy, window);

  top = (bst->xgwa.height > 800 ? bst->fontB->ascent : 0);

  {
    XGlyphInfo ov;
    const char *s = lines1[1];
    XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) s, strlen(s), &ov);
    left = (bst->xgwa.width - (ov.xOff * 0.55)) / 2;
    if (left < 10) left  = 10;
    left0 = right = left;
    BSOD_MARGINS (bst, left, right);
  }

  pixmap = XCreatePixmapFromBitmapData (dpy, window, (char *) qr_bits,
                                        qr_width, qr_height,
                                        bst->fg, bst->bg, bst->xgwa.depth);
  {
    int n = 2;
    if (bst->xgwa.width > 2560 || bst->xgwa.height > 2560)
      n++;  /* Retina displays */
    for (i = 0; i < n; i++)
      {
        pixmap = double_pixmap (dpy, bst->xgwa.visual, bst->xgwa.depth,
                                pixmap, qr_width, qr_height);
        qr_width *= 2;
        qr_height *= 2;
      }
  }
  bst->pixmap = pixmap;

  y = top;
  BSOD_FONT (bst, 1);
  for (i = 0; i < countof(lines1); i++)
    {
      int oy = y;
      int fid = (i == 0 ? 1 : i >= 3 ? 2 : 0);
      XftFont *font = (fid == 0 ? bst->font :
                       fid == 1 ? bst->fontB : bst->fontC);
      char *line;
      XGlyphInfo ov;

      line = xft_word_wrap (dpy, font, lines[i],
                            bst->xgwa.width - left - right);
      XftTextExtentsUtf8_multi (dpy, font, (FcChar8 *) line, strlen(line),
                                &ov);

      BSOD_MOVETO (bst, left, y + font->ascent);
      BSOD_FONT (bst, fid);
      BSOD_TEXT (bst, LEFT, line);

      free (line);
      line = 0;
      y += ov.height + font->descent;

      if (i == 0)
        {
          BSOD_MOVETO (bst, left, y);
        }
      else if (i == 1)
        {
          /* y += font->ascent + font->descent; */
          y1 = y;
          /* y += font->ascent + font->descent; */
        }
      else if (i == 2)
        {
          left  += qr_width + font->ascent / 2;
          if (bst->xgwa.width > bst->xgwa.height)
            right += qr_width * 1.8;
          BSOD_MARGINS (bst, left, right);
        }
      else if (i == 3)
        {
          BSOD_PIXMAP (bst, 0, 0, qr_width, qr_height, left0, oy);
        }
    }

  BSOD_MARGINS (bst, left0, 0);
  BSOD_FONT (bst, 0);
  for (i = 0; i <= stop; i++)
    {
      char buf[100];
      sprintf (buf, "%d%% complete", i);
      BSOD_MOVETO (bst, left0, y1);
      BSOD_TEXT (bst, LEFT, buf);
      BSOD_PAUSE (bst, 85000);
    }
  BSOD_PAUSE (bst, 3000000);

  XClearWindow (dpy, window);
  return bst;
}


static struct bsod_state *
windows_safe (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "winSafe", "WinSafe");

  unsigned long fg = bst->fg;
  unsigned long bg = bst->bg;
  const char * const lines[] = {
    "We apologize for the inconvenience, but Windows did not start"
    " successfully.  A\n",
    "recent hardware or software change might have caused this.\n",
    "\n",
    "If your computer stopped responding, restarted unexpectedly, or was\n",
    "automatically shut down to protect your files and folders, choose Last"
    " Known\n",
    "Good Configuration to refert to the most recent settings that worked.\n",
    "\n",
    "If a previous startup attempt was interrupted due to a power failure or"
    " because\n",
    "the Power or Reset button was pressed, or if you aren't sure what caused"
    " the\n",
    "problem, choose Start Windows Normally.\n",
    "\n",
    "    Safe Mode\n",
    "    Safe Mode with Networking\n",
    "    Safe Mode with Command Prompt\n",
    "\n",
    "    Last Known Good Configuration (your most recent settings that"
    " worked)\n",
    "\n",
    "*    Start Windows Normally\n",
    "\n",
    "Use the up and down arrow keys to move the highlight to your choice.\n",
    "Seconds until Windows starts:  ",
  };
  int i, j;
  int bit = random() % 8;  /* Dead bus line */

  if (bit && (random() % 4))
    bit = 3;

  /* 1: Stapt Windous Nmrmally
     2: Start Windoss Nkrmahly
     3: Start Wandows Ngrmadly
     4: Stabt Windogs Normally
     5: StaRt WIndoWs NOrmaLly
     6: Sta2t W)ndo7s N/rma,ly
   */
  BSOD_COLOR (bst, fg, bg);
  for (j = 0; j < countof(lines); j++)
    {
      int col;
      int inv = (lines[j][0] == '*');
      char *L = strdup (lines[j] + inv);

      for (i = 0, col = 1; i < strlen(L); i++, col++)
        {
          if (L[i] > ' ' && !(col % 4))
            {
              L[i] &= ~(1 << bit);
              if (!L[i]) L[i] = ' ';
            }
        }
      if (inv) BSOD_INVERT (bst);
      BSOD_TEXT (bst, LEFT, L);
      if (inv) BSOD_INVERT (bst);
      free (L);
    }

  for (i = 9; i >= 0; i--)
    {
      char c[10];
      sprintf (c, "\b%d", i);
      BSOD_TEXT (bst, LEFT, c);
      BSOD_PAUSE (bst, 1000000);
    }

  XClearWindow (dpy, window);
  return bst;
}


static struct bsod_state *
windows_sb (Display *dpy, Window window)
{
  struct bsod_state *bst =
    make_bsod_state (dpy, window, "windowssb", "WindowsSB");

  unsigned long fg = bst->fg;
  unsigned long bg = bst->bg;
  unsigned long bg2 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                          "windowssb.background2",
                                          "WindowsSB.Background");
  int line_height = bst->fontA->ascent + bst->fontA->descent;
  int top = (bst->xgwa.height - line_height * 8) / 2;
  XGlyphInfo em;
  int left;
  XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) "M", 1, &em);
  left = (bst->xgwa.width - em.width * 46) / 2;

  BSOD_MOVETO (bst, left, top);
  BSOD_COLOR (bst, fg, bg2);
  BSOD_MARGINS (bst, left, bst->xgwa.width);

  /* Really these should be lines from Unicode Box Drawing Block 2500-257F,
     but BSOD_TEXT doesn't support UTF-8 in strings.  Also the font might
     not have those. Close enough. */
  BSOD_TEXT (bst, LEFT,
             "|----------- Secure Boot Violation ----------|\n"
             "|                                            |\n"
             "|  Invalid signature detected. Check Secure  |\n"
             "|            Boot Policy in Setup            |\n"
             "|                                            |\n"
             "|--------------------------------------------|\n"
             "|                     ");
  BSOD_COLOR (bst, fg, bg);
  BSOD_TEXT (bst, LEFT,            "Ok");
  BSOD_COLOR (bst, fg, bg2);
  BSOD_TEXT (bst, LEFT,              "                     |\n"
             "|--------------------------------------------|\n"
    );
  XClearWindow (dpy, window);
  return bst;
}


static struct bsod_state *
windows_other (Display *dpy, Window window)
{
  /* Lump all of the 2K-ish crashes together and select them randomly...
   */
  int which = (random() % 6);
  switch (which) {
  case 0: return windows_2k (dpy, window); break;
  case 1: return windows_me (dpy, window); break;
  case 2: return windows_xp (dpy, window); break;
  case 3: return windows_lh (dpy, window); break;
  case 4: return windows_safe (dpy, window); break;
  case 5: return windows_sb (dpy, window); break;
  default: abort(); break;
  }
}

/* Windows and Apple2 ransomware.
 */
static struct bsod_state *apple2ransomware (Display *, Window);

static struct bsod_state *
windows_ransomware (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window,
                                            "ransomware", "Ransomware");
  char buf[1024];

  int pix_w = 0, pix_h = 0;
  Pixmap mask = 0;
  Pixmap pixmap = image_data_to_pixmap (dpy, window,
                                        ransomware_png, sizeof(ransomware_png),
                                        &pix_w, &pix_h, &mask);
  int i, n = 0;

  /* Don't start the countdown from the start, advance the deadline by 3 - 30
     hours */
  int advance_deadline = (random() % 97200) + 10800;

  time_t now = time(NULL);
  const time_t stage1_deadline = now + 259200 - advance_deadline; /* 3 days */
  const time_t stage2_deadline = now + 604800 - advance_deadline; /* 7 days */
  char stage1_deadline_str[25], stage2_deadline_str[25];
  char countdown_str[20];
  int countdown_d, countdown_h, countdown_m, countdown_s, countdown_r;
  int line_height  = bst->font->ascent + bst->font->descent;
  int line_height1 = bst->fontA->ascent + bst->fontA->descent;

  const char * const currencies[] = {
    "Blitcoin",
    "Bitcorn",
    "Buttcorn",
    "clicks",
    "clicks",
    "Ass Pennies",
    "Ass Pennies",
    "Dollary-doos",
    "Dunning-Krugerrands",
    "Dunning-Krugerrands",
    "Dunning-Krugerrands",
    "Dunning-Krugerrands",
    "Dunning-Krugerrands",
    "Dunning-Krugerrands",
    "gift certificates",
    "Creepto-Currency",
    "secret sauce",
    "Tribbles",
  };

  const char *currency = currencies[random() % countof(currencies)];

  const char * const header_quips[] = {
    "Oops, your screens have been encrypted!",
    "Oops, your screens have been encrypted!",
    "Oops, your screens have been encrypted!",
    "Oops, your screens have been encrypted!",
    "Oops, your screen have encrypted!",
    "Oops, you're screens have been encrypted!",
    "Oops, your screens have been encrupted!",
    "Oops, your screens have been encrumpet!",
    "Oops, your screens have been encrusted!",
    "If you don't pay this ransom, then you are a theif!",
    "Your screen was subject to the laws of mathomatics!",
    "Oops, your screen was shaved by Occam's Razor!",
    "Oops, your screen was perturbated by Langford's Basilisk!",
    "Your screen is now stored as Snapchat messages!",
    "Oops, your screen is now stored on Betamax!",
    "Oops, your screen is now in the clown!",
    "Oops, your screen has been deprecated!",
    "Oops, you're screen was seized by the FBI!",
    "All your screen was shared with your coworkers!",
    "All your screen are belong to us.",
    "Well actually, your screen isn't needed anymore.",
    "u just got popped with some 0day shit!!",
    "M'lady,",
    "ALL UR APES ARE GONE!!1",
    "Oops, all your apes are gone!",
    "Oops, all your apes are gone!!",
    "Oops, all ur tokens have been funged!",
    "Oops, all your turkens have been funged!",
    "Oops, all your tokens have been funged!",
    "YOUR TOKENS ARE FUNGED. PRAY I DO NOT FUNGE THEM FURTHER.",
  };

  const char *header_quip = header_quips[random() % countof(header_quips)];

  /* You got this because... */
  const char * const excuse_quips[] = {
    "all human actions are equivalent and all are on principle doomed "
    "to failure",
    "you hold a diverse portfolio of cryptocurrencies",
    "you need to get in on ransomware futures at the ground floor",
    "your flight was overbooked",
    "you did not apply the security update for bugs NSA keys secret from "
    "Microsoft in your Windows(R) operating system",
    "you are bad and you should feel bad",
    "you used the wifi at defcon",
    "you lack official Clown Strike[TM] threaty threat technology",
    "Capitalism is a death cult",
    "Web3 is in full effect",
    "paperclip maximizers gonna paperclip maximize",
    "the line is pleased",
    "line goes up",
    "you didn't HODL",
    "you didn't click hard enough and now Tinkerbelle is dead",
    "of your tesla stonks",
    "MAMMON HUNGERS",
  };

  const char *excuse_quip   = excuse_quips[random() % countof(excuse_quips)];
  const char *excuse_quip_2 = excuse_quips[random() % countof(excuse_quips)];

  /* WELL ACTUALLY, screensavers aren't really necessary anymore because... */
  const char * const screensaver_quips[] = {
    "I read it on hacker news",
    "that's official Debian policy now",
    "that is the official policy of United Airlines",
    "they cause global warming",
    "they lack an eternal struggle",
    "they lack a vapid dichotomy",
    "those electrons could be used for gold farming instead",
    "you can make more money in art exhibitions",
  };

  const char *screensaver_quip =
    screensaver_quips[random() % countof(screensaver_quips)];

  const char * const lines[] = {
    "*What Happened To My Computer?\n",
    "Your important pixels are paintcrypted. All of your documents, photos, ",
    "videos, databases, icons, dick pics are not accessible because they ",
    "have been bitblted. Maybe you are looking for a way to get them back, ",
    "but don't waste your time. Nobody can recover your pixels without our ",
    "pointer motion clicker services.\n",
    "\n",
    "*Can I Recover My Important Dick Pix?\n",
    "Yes. We guarantee that you can recover them safely and easily. But you ",
    "not have much time.\n",
    "You can expose some files for free. Try it now by pressing <The Any ",
    "Key>.\n",
    "But if you want to unsave all your screens, then you need to pay. ",
    "You have only 3 days to click. After that the clicks will double. ",
    "After 7 days your pixels will be gone forever.\n",
    "We will have free events for cheapskates who can't pay in 6 months, ",
    "long after all the pixels are xored.\n",
    "\n",
    "*How do I pay?\n",
    "Payment is accepted in ", "[C]",
    " only. For more information, press <About ", "[C]", ">.",
    " Please check the current price of ", "[C]", " and buy some ", "[C]",
    ". For more information, press <How to buy ", "[C]", ">.\n",
    "And send the correct amount to the address specified below. After your ",
    "payment, press <Check Payment>. Best time to check: 4-6am, Mon-Fri.\n",
    "\n",
    "*Why Did I Get This?\n",
    "You got this because ", "[Q]", ". Also ", "[Q2]", ".\n",
    "\n",
    "*But Aren't Screensavers Are Necessary?\n",
    "WELL ACTUALLY, screensavers aren't really necessary anymore because ",
    "[S]", ".\n",
    "\n",
    "Please file complaints to @POTUS on Twitter.\n",
    "\n"
    "\n"
    "\n"
    "\n",
    "*GREETZ TO CRASH OVERRIDE AND ALSO JOEY\n",
  };

  /* Positions of UI elements. Layout:

   +---------+-+---------------------------------------+
   |   LOGO  | |               HEADER                  |
   |         | |---------------------------------------|
   |         | | NOTE TEXT                             |
   |   DEAD  | |                                       |
   |   LINE  | |                                       |
   |  TIMERS | |                                       |
   |         | |                                       |
   |         | |                                       |
   |         | |                                       |
   |         | |                                       |
   |         | |                                       |
   +---------+ |                                       |
   | LINKS   | +---------------------------------------+
   | LINKS   | | FOOTER                                |
   +---------+-+---------------------------------------+

  The right side of the UI maximises to available width.
  The note text maximises to available height.
  The logo, header and timers are anchored to the top left of the window.
  The links and footer are anchored to the bottom left of the window.
  The entire window is a fixed 4:3 scale, with a minimum margin around it.
  */

  /* main window text */
  unsigned long fg = bst->fg;
  unsigned long bg = bst->bg;
  /* ransom note */
  unsigned long fg2 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                          "ransomware.foreground2",
                                          "Ransomware.Foreground");
  unsigned long bg2 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                          "ransomware.background2",
                                          "Ransomware.Background");
  /* buttons */
  unsigned long fg3 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                          "ransomware.foreground3",
                                          "Ransomware.Foreground");
  unsigned long bg3 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                          "ransomware.background3",
                                          "Ransomware.Background");
  /* links */
  unsigned long link = get_pixel_resource (dpy, bst->xgwa.colormap,
                                           "ransomware.link",
                                           "Ransomware.Foreground");
  /* headers */
  unsigned long theader = get_pixel_resource (dpy, bst->xgwa.colormap,
                                              "ransomware.timerheader",
                                              "Ransomware.Foreground");
  int left_column_width;
  int right_column_width;
  int right_column_height;
  int stage1_countdown_y, stage2_countdown_y;
  int margin;
  int top_height, bottom_height;
  int x, y;

  while (excuse_quip == excuse_quip_2)
    excuse_quip_2 = excuse_quips[random() % countof(excuse_quips)];

  if (bst->xgwa.width > 2560 || bst->xgwa.height > 2560)
    n++;  /* Retina displays */
  for (i = 0; i < n; i++)
    {
      pixmap = double_pixmap (dpy, bst->xgwa.visual, bst->xgwa.depth,
                              pixmap, pix_w, pix_h);
      mask = double_pixmap (dpy, bst->xgwa.visual, 1,
                            mask, pix_w, pix_h);
      pix_w *= 2;
      pix_h *= 2;
    }

  margin = line_height;
  left_column_width  = MAX (pix_w, line_height1 * 8);
  right_column_width = MIN (line_height * 40,
                            MAX (line_height * 8,
                                 bst->xgwa.width - left_column_width
                                 - margin*2));
  top_height = line_height * 2.5;
  bottom_height = line_height * 6;
  right_column_height = MIN (line_height * 36,
                             bst->xgwa.height - bottom_height - top_height
                             - line_height);

  if ((bst->xgwa.width / 4) * 3 > bst->xgwa.height)
    /* Wide screen: keep the big text box at 4:3, centered. */
    right_column_height = MIN (right_column_height,
                               right_column_width * 4 / 3);
  else if (right_column_width < line_height * 30)
    /* Tall but narrow screen: make the text box be full height. */
    right_column_height = (bst->xgwa.height - bottom_height - top_height
                           - line_height);

  x = (bst->xgwa.width - left_column_width - right_column_width - margin) / 2;
  y = (bst->xgwa.height - right_column_height - bottom_height) / 2;

  bst->xoff = bst->left_margin = bst->right_margin = 0;

  if (!(random() % 8))
    return apple2ransomware (dpy, window);

  /* Draw the main red window */
  BSOD_INVERT (bst);
  BSOD_RECT (bst, True, 0, 0, bst->xgwa.width, bst->xgwa.height);

  if (pixmap) {
    bst->pixmap = pixmap;
    bst->mask = mask;
    BSOD_PIXMAP (bst, 0, 0, pix_w, pix_h,
                 x + (left_column_width - pix_w) / 2,
                 y);
  }

  /* Setup deadlines */
  strftime (stage1_deadline_str, sizeof(stage1_deadline_str),
            "%m/%d/%Y %H:%M:%S", localtime(&stage1_deadline));
  strftime (stage2_deadline_str, sizeof(stage1_deadline_str),
            "%m/%d/%Y %H:%M:%S", localtime(&stage2_deadline));

  BSOD_INVERT (bst);
  /* Draw header pane */
  BSOD_FONT (bst, 0);

  BSOD_MARGINS (bst,
                x + left_column_width + margin,
                bst->xgwa.width -
                (x + left_column_width + margin + right_column_width));
  BSOD_MOVETO (bst, x + left_column_width + margin,
               y + bst->fontA->ascent);
  BSOD_COLOR (bst, fg, bg);
  BSOD_WORD_WRAP (bst);
  BSOD_TEXT (bst, CENTER, header_quip);
  BSOD_TRUNCATE (bst);

  /* Draw left-side timers */
  BSOD_MARGINS (bst, x, bst->xgwa.width - (x + left_column_width));
  BSOD_MOVETO (bst, x, y + pix_h + line_height);
  BSOD_FONT (bst, 1);

  BSOD_COLOR (bst, theader, bg);
  BSOD_TEXT (bst, CENTER, "Payment will be raised on\n");
  BSOD_COLOR (bst, fg, bg);
  BSOD_TEXT (bst, CENTER, stage1_deadline_str);

  stage1_countdown_y = y + pix_h + line_height + line_height1 * 3;
  BSOD_MOVETO (bst, x, stage1_countdown_y - line_height);
  BSOD_TEXT (bst, CENTER, "Time Left");

  BSOD_COLOR (bst, theader, bg);
  BSOD_WORD_WRAP (bst);
  BSOD_TEXT (bst, CENTER, "\n\n\n\nYour pixels will be lost on\n");
  BSOD_TRUNCATE (bst);
  BSOD_COLOR (bst, fg, bg);
  BSOD_TEXT (bst, CENTER, stage2_deadline_str);

  stage2_countdown_y = stage1_countdown_y + line_height1 * 5;
  BSOD_MOVETO (bst, x, stage2_countdown_y - line_height);
  BSOD_TEXT (bst, CENTER, "Time Left");

  BSOD_FONT (bst, 1);

  /* Draw links, but skip on small screens */
  if (right_column_height > 425) {
    BSOD_MOVETO (bst, x, 
                 y + right_column_height + top_height + bottom_height
                 - line_height1 * 5);
    BSOD_COLOR (bst, link, bg);
    BSOD_TEXT (bst, LEFT, "\n");
    BSOD_TEXT (bst, LEFT, "About ");
    BSOD_TEXT (bst, LEFT, currency);
    BSOD_TEXT (bst, LEFT, "\n\n"
                          "How to buy ");
    BSOD_TEXT (bst, LEFT, currency);
    BSOD_TEXT (bst, LEFT, "\n\n"
                          "Contact us\n");
  }

  /* Ransom note text area */
  BSOD_COLOR (bst, bg2, fg2);
  BSOD_RECT (bst, True, 
             x + left_column_width + margin,
             y + top_height,
             right_column_width,
             right_column_height);
  BSOD_MOVETO (bst,
               x + left_column_width + margin + line_height / 2,
               y + top_height + line_height + line_height / 2);
  BSOD_MARGINS (bst,
                x + left_column_width + margin + line_height / 2,
                bst->xgwa.width - 
                (x + left_column_width + margin + right_column_width));
  BSOD_VERT_MARGINS (bst, 
                     y + top_height + line_height / 2,
                     bottom_height - line_height);
  BSOD_INVERT (bst);

  /* Write out the ransom note itself */
  BSOD_CROP (bst, True);
  BSOD_WORD_WRAP (bst);
  for (i = 0; i < countof(lines); i++)
    {
      const char *s = lines[i];
      if (!strcmp(s, "[C]")) s = currency;
      else if  (!strcmp(s, "[Q]"))  s = excuse_quip;
      else if  (!strcmp(s, "[Q2]")) s = excuse_quip_2;
      else if  (!strcmp(s, "[S]")) s = screensaver_quip;

      if (*s == '*')
        {
          s++;
          BSOD_FONT (bst, 2);
        }
      else
        BSOD_FONT (bst, 0);

      BSOD_TEXT (bst, LEFT, s);
    }
  BSOD_TRUNCATE (bst);
  BSOD_CROP (bst, False);
  BSOD_FONT (bst, 0);

  /* Draw over any overflowing ransom text. */
  BSOD_COLOR (bst, bg, fg);
  BSOD_RECT (bst, True,
             x + left_column_width + margin,
             y + top_height + right_column_height,
             bst->xgwa.width, bst->xgwa.height);
  BSOD_RECT (bst, True,
             x + left_column_width + margin + right_column_width,
             y + top_height,
             bst->xgwa.width, bst->xgwa.height);

  /* Draw the footer */
  BSOD_COLOR (bst, theader, bg);
  BSOD_MOVETO (bst,
               x + left_column_width + margin,
               y + top_height + right_column_height + line_height * 2);

  sprintf(buf, "Send $%.2f of %s to this address:\n", 101+frand(888),
          currency);
  BSOD_TEXT (bst, LEFT, buf);
  BSOD_COLOR (bst, fg2, bg2);

  /* address, has some extra slashes in there because it's a fake address */
  for (i = 0; i < 40; i++) {
    const char *s =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123459789";
    buf[i] = s[random() % strlen(s)];
  }
  memcpy (buf, " //", 3);
  buf[10] = '/';
  buf[17] = '/';
  buf[24] = '/';
  strcpy (buf+33, " ");
  BSOD_TEXT (bst, LEFT, buf);

  BSOD_COLOR (bst, fg, bg);
  BSOD_TEXT (bst, LEFT, "   ");
  BSOD_COLOR (bst, fg3, bg3);
  BSOD_TEXT (bst, LEFT, "  Copy  ");
  BSOD_COLOR (bst, fg, bg);
  BSOD_TEXT (bst, LEFT, "\n\n");

  BSOD_COLOR (bst, fg3, bg3);
  BSOD_TEXT (bst, LEFT, "  Demogrify Screen  ");
  BSOD_COLOR (bst, fg, bg);
  BSOD_TEXT (bst, LEFT, "            ");
  BSOD_COLOR (bst, fg3, bg3);
  BSOD_TEXT (bst, LEFT, "  Check Payment  ");


  /* Draw countdown timers */
  BSOD_COLOR (bst, fg, bg);
  BSOD_FONT (bst, 0);
  do {
    /* First timer */
    BSOD_MOVETO (bst, x, stage1_countdown_y);
    BSOD_MARGINS (bst, x, bst->xgwa.width - (x + left_column_width));

    countdown_r = stage1_deadline - now;
    countdown_s = countdown_r % 60;
    countdown_m = (countdown_r / 60) % 60;
    countdown_h = (countdown_r / 3600) % 24;
    countdown_d = (countdown_r / 86400);

    sprintf (countdown_str, "%02d:%02d:%02d:%02d\n",
             countdown_d, countdown_h, countdown_m, countdown_s);

    BSOD_TEXT (bst, CENTER, countdown_str);

    /* Second timer */
    BSOD_MOVETO (bst, x, stage2_countdown_y);

    countdown_r = stage2_deadline - now;
    countdown_s = countdown_r % 60;
    countdown_m = (countdown_r / 60) % 60;
    countdown_h = (countdown_r / 3600) % 24;
    countdown_d = (countdown_r / 86400);

    sprintf (countdown_str, "%02d:%02d:%02d:%02d\n",
             countdown_d, countdown_h, countdown_m, countdown_s);

    BSOD_TEXT (bst, CENTER, countdown_str);

    BSOD_PAUSE (bst, 1000000);
    now++;

    /* While the "correct" thing to do is create enough of a script to fill the
     * stage2_deadline, this would be 7 days of "frames", which is quite a bit
     * of memory. Instead, only fill the buffer with 1 hour of frames, which is
     * enough to make the point before xscreensaver cycles us.
     */
  } while (stage1_deadline - now > 3600);

  XClearWindow (dpy, window);
  return bst;
}


static struct bsod_state *
bitlocker (Display *dpy, Window window)
{
  struct bsod_state *bst =
    make_bsod_state (dpy, window, "bitlocker", "Bitlocker");
  int top = bst->fontB->ascent + bst->fontB->descent;
  int left = top * 2;
  int right = left + bst->fontB->ascent * 22;
  const char *bottom = ("Press Enter to reboot and try again\n"
                        "Press ESC for BitLocker recovery");
  int which = random() % 5;

  if (right > bst->xgwa.width) right = bst->xgwa.width;
  BSOD_MARGINS (bst, left, bst->xgwa.width - right);
  BSOD_WORD_WRAP (bst);
  BSOD_FONT (bst, 1);
  BSOD_MOVETO (bst, 0, top * 2);

  switch (which) {
    case 0:
      BSOD_TEXT (bst, LEFT, "BitLocker\n\n");
      BSOD_FONT (bst, 0);
      BSOD_TEXT (bst, LEFT,
                 "Plug in the USB drive that has the BitLocker key\n");
      break;

    case 1:
      BSOD_TEXT (bst, LEFT, "BitLocker recovery\n\n");
      BSOD_FONT (bst, 0);
      BSOD_TEXT (bst, LEFT,
                 "To recover this drive, plug in the USB drive that has "
                 "the BitLocker recovery key\n\n");
      BSOD_FONT (bst, 2);
      BSOD_TEXT (bst, LEFT,
                 "Bitlocker needs your recovery key to unlock your drive "
                 "because Secure Boot policy has unexpectedly changed.\n"
                 "For more information on how to retrieve this key, go to\n"
                 "https://www.jwz.org/xscreensaver/ from another PC "
                 "or mobile device.");
      bottom = ("Press Enter to reboot and try again\n"
                "Press Esc or the Windows key for more recovery options");
      break;
    case 2:
      BSOD_TEXT (bst, LEFT, "BitLocker\n\n");
      BSOD_FONT (bst, 0);
      BSOD_TEXT (bst, LEFT,
                 "Enter the PIN to unlock this drive\n");
      BSOD_INVERT (bst);
      BSOD_TRUNCATE (bst);
      BSOD_TEXT (bst, LEFT,
                 "                                                "
                 "                                                ");
      BSOD_INVERT (bst);
      BSOD_WORD_WRAP (bst);
      BSOD_TEXT (bst, LEFT,
                 "\n\n\n"
                 "Use the number keys or function keys F1-F10 "
                 "(use F10 for 0)."
                 "\n\n\n"
                 "Press the Insert key to see the PIN as you type.");
      break;

    case 3:
      BSOD_TEXT (bst, LEFT, "BitLocker recovery\n\n");
      BSOD_FONT (bst, 0);
      BSOD_TEXT (bst, LEFT,
                 "Enter the recovery key for this drive\n");
      BSOD_INVERT (bst);
      BSOD_TRUNCATE (bst);
      BSOD_TEXT (bst, LEFT,
                 "                                                "
                 "                                                ");
      BSOD_INVERT (bst);
      BSOD_WORD_WRAP (bst);

      BSOD_FONT (bst, 2);
      BSOD_TEXT (bst, LEFT,
                 "\n\n\n"
                 "Use the number keys or function keys F1-F10 "
                 "(use F10 for 0).\n");
      {
        int which2;
        char k[200];
        sprintf (k, "Recovery key ID (to identify your key): "
                 "%08X-%04X-%04X-%04X%08X\n\n",
                 random() & 0xFFFFFFFF,
                 random() & 0xFFFF,
                 random() & 0xFFFF,
                 random() & 0xFFFF,
                 random() & 0xFFFFFFFF);
        BSOD_TEXT (bst, LEFT, k);
      
        which2 = random() % 4;
        switch (which2) {
        case 0:
          BSOD_TEXT (bst, LEFT,
                     "Bitlocker needs your recovery key to unlock your "
                     "drive because Secure Boot policy has unexpectedly "
                     "changed."
                     "\n\n");
          break;
        case 1:
          BSOD_TEXT (bst, LEFT,
                     "Bitlocker needs your recovery key to unlock your "
                     "PC's configuration has changed. This may have happened "
                     "because a disc or USB device ws inserted. Removing it "
                     "and restarting your PC may fix this problem."
                     "\n\n");
          break;
        case 3:
          BSOD_TEXT (bst, LEFT,
                     "Bitlocker needs your recovery key to unlock your drive "
                     "because Secure Boot has been disabled. Either Secure "
                     "Boot must be re-enabled, or BitLocker must be suspended "
                     "for Windows to start normally."
                     "\n\n");
          break;
        default:
          break;
        }
      }

      BSOD_TEXT (bst, LEFT,
                 "Here's how to find your key:\n"
                 "- Sign in on another device and go to: "
                 "https://www.jwz.org/\n"
                 "- For more information go to: "
                 "https://www.jwz.org/xscreensaver/");
      break;

    case 4:
      BSOD_TEXT (bst, LEFT, "Recovery\n\n");
      BSOD_FONT (bst, 0);
      BSOD_TEXT (bst, LEFT,
                 "There are no more BitLocker recovery options on your PC"
                 "\n\n");
      BSOD_FONT (bst, 2);
      BSOD_TEXT (bst, LEFT,
                 "You'll need to use the recovery tools on your installation "
                 "media. If you don't have any installation media (like a "
                 "disc or USB device), contact your system administrator or "
                 "PC manufacturer.");
      bottom = ("Press Enter to try again\n"
                "Press F8 for Startup Settings\n"
                "Press Esc for UEFI Firmware Settings");
      break;

  default:
    abort();
  }

  BSOD_FONT (bst, 0);
  BSOD_MOVETO (bst, 0, bst->xgwa.height - top -
               (bst->fontB->ascent + bst->fontB->descent) * 1);
  BSOD_TEXT (bst, LEFT, bottom);

  XClearWindow (dpy, window);
  return bst;
}


/* As seen in Portal 2.  By jwz.
 */
static struct bsod_state *
glados (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "glaDOS", "GlaDOS");
  const char * const panicstr[] = {
    "\n",
    "MOLTEN CORE WARNING\n",
    "\n",
    "An operator error exception has occurred at FISSREAC0020093:09\n",
    "FISSREAC0020077:14 FISSREAC0020023:17 FISSREAC0020088:22\n",
    "neutron multiplication rate at spikevalue 99999999\n",
    "\n",
    "* Press any key to vent radiological emissions into atmosphere.\n",
    "* Consult reactor core manual for instructions on proper reactor core\n",
    "maintenance and repair.\n",
    "\n",
    "Press any key to continue\n",
  };

  int i;

  bst->xoff = bst->left_margin = bst->right_margin = 0;

  bst->y = ((bst->xgwa.height - bst->yoff -
             ((bst->font->ascent + bst->font->descent) * countof(panicstr)))
            / 2);

  BSOD_MOVETO (bst, 0, bst->y);
  BSOD_INVERT (bst);
  BSOD_TEXT   (bst,  CENTER, "OPERATOR ERROR\n");
  BSOD_INVERT (bst);
  for (i = 0; i < countof(panicstr); i++)
    BSOD_TEXT (bst, CENTER, panicstr[i]);
  BSOD_PAUSE (bst, 1000000);
  BSOD_INVERT (bst);
  BSOD_RECT (bst, True, 0, 0, bst->xgwa.width, bst->xgwa.height);
  BSOD_INVERT (bst);
  BSOD_PAUSE (bst, 250000);
  BSOD_RESET (bst);

  XClearWindow (dpy, window);
  return bst;
}



/* SCO OpenServer 5 panic, by Tom Kelly <tom@ancilla.toronto.on.ca>
 */
static struct bsod_state *
sco (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "sco", "SCO");

  BSOD_TEXT (bst, LEFT,
     "Unexpected trap in kernel mode:\n"
     "\n"
     "cr0 0x80010013     cr2  0x00000014     cr3 0x00000000  tlb  0x00000000\n"
     "ss  0x00071054    uesp  0x00012055     efl 0x00080888  ipl  0x00000005\n"
     "cs  0x00092585     eip  0x00544a4b     err 0x004d4a47  trap 0x0000000E\n"
     "eax 0x0045474b     ecx  0x0042544b     edx 0x57687920  ebx  0x61726520\n"
     "esp 0x796f7520     ebp  0x72656164     esi 0x696e6720  edi  0x74686973\n"
     "ds  0x3f000000     es   0x43494c48     fs  0x43525343  gs   0x4f4d4b53\n"
     "\n"
     "PANIC: k_trap - kernel mode trap type 0x0000000E\n"
     "Trying to dump 5023 pages to dumpdev hd (1/41), 63 pages per '.'\n"
    );
  BSOD_CHAR_DELAY (bst, 100000);
  BSOD_TEXT (bst, LEFT,
    "................................................................."
    "..............\n"
    );
  BSOD_CHAR_DELAY (bst, 0);
  BSOD_TEXT (bst, LEFT,
     "5023 pages dumped\n"
     "\n"
     "\n"
     );
  BSOD_PAUSE (bst, 2000000);
  BSOD_TEXT (bst, LEFT,
     "**   Safe to Power Off   **\n"
     "           - or -\n"
     "** Press Any Key to Reboot **\n"
    );

  bst->y = ((bst->xgwa.height - bst->yoff -
             ((bst->font->ascent + bst->font->descent) * 18)));

  XClearWindow (dpy, window);
  return bst;
}


/* Linux (sparc) panic, by Tom Kelly <tom@ancilla.toronto.on.ca>
 */
static struct bsod_state *
sparc_linux (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window,
                                            "sparclinux", "SparcLinux");
  bst->scroll_p = True;
  bst->y = bst->xgwa.height - bst->yoff
    - bst->font->ascent - bst->font->descent;

  BSOD_TEXT (bst, LEFT,
        "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
	"Unable to handle kernel paging request at virtual address f0d4a000\n"
	"tsk->mm->context = 00000014\n"
	"tsk->mm->pgd = f26b0000\n"
	"              \\|/ ____ \\|/\n"
	"              \"@'/ ,. \\`@\"\n"
	"              /_| \\__/ |_\\\n"
	"                 \\__U_/\n"
	"gawk(22827): Oops\n"
	"PSR: 044010c1 PC: f001c2cc NPC: f001c2d0 Y: 00000000\n"
	"g0: 00001000 g1: fffffff7 g2: 04401086 g3: 0001eaa0\n"
	"g4: 000207dc g5: f0130400 g6: f0d4a018 g7: 00000001\n"
	"o0: 00000000 o1: f0d4a298 o2: 00000040 o3: f1380718\n"
	"o4: f1380718 o5: 00000200 sp: f1b13f08 ret_pc: f001c2a0\n"
	"l0: efffd880 l1: 00000001 l2: f0d4a230 l3: 00000014\n"
	"l4: 0000ffff l5: f0131550 l6: f012c000 l7: f0130400\n"
	"i0: f1b13fb0 i1: 00000001 i2: 00000002 i3: 0007c000\n"
	"i4: f01457c0 i5: 00000004 i6: f1b13f70 i7: f0015360\n"
	"Instruction DUMP:\n"
    );

  XClearWindow (dpy, window);
  return bst;
}


/* BSD Panic by greywolf@starwolf.com - modeled after the Linux panic above.
   By Grey Wolf <greywolf@siteROCK.com>
 */
static struct bsod_state *
bsd (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "bsd", "BSD");

  const char * const panicstr[] = {
    "panic: ifree: freeing free inode\n",
    "panic: blkfree: freeing free block\n",
    "panic: improbability coefficient below zero\n",
    "panic: cgsixmmap\n",
    "panic: crazy interrupts\n",
    "panic: nmi\n",
    "panic: attempted windows install\n",
    "panic: don't\n",
    "panic: free inode isn't\n",
    "panic: cpu_fork: curproc\n",
    "panic: malloc: out of space in kmem_map\n",
    "panic: vogon starship detected\n",
    "panic: teleport chamber: out of order\n",
    "panic: Brain fried - core dumped\n"
   };
  int i, n, b;
  char syncing[80], bbuf[50];

  for (i = 0; i < sizeof(syncing); i++)
    syncing[i] = 0;

  i = (random() % (sizeof(panicstr) / sizeof(*panicstr)));
  BSOD_TEXT (bst, LEFT, panicstr[i]);
  BSOD_TEXT (bst, LEFT, "Syncing disks: ");

  b = (random() % 40);
  for (n = 0; (n < 20) && (b > 0); n++)
    {
      if (i)
        {
          i = (random() & 0x7);
          b -= (random() & 0xff) % 20;
          if (b < 0)
            b = 0;
        }
      sprintf (bbuf, "%d ", b);
      BSOD_TEXT (bst, LEFT, bbuf);
      BSOD_PAUSE (bst, 1000000);
    }

  BSOD_TEXT (bst, LEFT, "\n");
  BSOD_TEXT (bst, LEFT, (b ? "damn!" : "sunk!"));
  BSOD_TEXT (bst, LEFT, "\nRebooting\n");

  bst->y = ((bst->xgwa.height - bst->yoff -
             ((bst->font->ascent + bst->font->descent) * 4)));

  XClearWindow (dpy, window);
  return bst;
}


static struct bsod_state *
amiga (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "amiga", "Amiga");

  const char *guru1 ="Software failure.  Press left mouse button to continue.";
  const char *guru2 ="Guru Meditation #00000003.00C01570";
  Pixmap pixmap = 0;
  Pixmap mask = 0;
  int pix_w = 0, pix_h = 0;
  int height;
  int lw = bst->font->ascent + bst->font->descent;
  unsigned long fg = bst->fg;
  unsigned long bg = bst->bg;

  unsigned long bg2 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                          "amiga.background2",
                                          "Amiga.Background");

  bst->yoff = 0;
  bst->top_margin = bst->bottom_margin = 0;

  pixmap = image_data_to_pixmap (dpy, window,
                                 amiga_png, sizeof(amiga_png),
                                 &pix_w, &pix_h, &mask);

  if (pixmap)
    {
      int i, n = 0;
      if (MIN (bst->xgwa.width, bst->xgwa.height) > 600) n++;
      if (bst->xgwa.width > 2560 || bst->xgwa.height > 2560)
        n++;  /* Retina displays */
      for (i = 0; i < n; i++)
        {
          pixmap = double_pixmap (dpy, bst->xgwa.visual, bst->xgwa.depth,
                                  pixmap, pix_w, pix_h);
          mask = double_pixmap (dpy, bst->xgwa.visual, 1, mask, pix_w, pix_h);
          pix_w *= 2;
          pix_h *= 2;
        }
    }

  XSetLineAttributes (dpy, bst->gc, lw, LineSolid, CapButt, JoinMiter);

  height = lw * 5;

  bst->char_delay = bst->line_delay = 0;

  BSOD_PAUSE (bst, 2000000);
  BSOD_COPY (bst, 0, 0, bst->xgwa.width, bst->xgwa.height - height, 0, height);

  BSOD_COLOR (bst, fg, bg);
  BSOD_RECT (bst, True, 0, 0, bst->xgwa.width, height); /* red */
  BSOD_COLOR (bst, bg, fg);
  BSOD_RECT (bst, True, lw/2, lw/2, bst->xgwa.width-lw, height-lw); /* black */
  BSOD_COLOR (bst, fg, bg);
  BSOD_MOVETO (bst, 0, lw*2);
  BSOD_TEXT (bst, CENTER, guru1);
  BSOD_MOVETO (bst, 0, lw*3.5);
  BSOD_TEXT (bst, CENTER, guru2);
  BSOD_PAUSE (bst, 1000000);

  BSOD_COLOR (bst, bg, fg);
  BSOD_RECT (bst, True, 0, 0, bst->xgwa.width, height); /* black */
  BSOD_COLOR (bst, fg, bg);
  BSOD_MOVETO (bst, 0, lw*2);
  BSOD_TEXT (bst, CENTER, guru1);
  BSOD_MOVETO (bst, 0, lw*3.5);
  BSOD_TEXT (bst, CENTER, guru2);
  BSOD_PAUSE (bst, 1000000);

  BSOD_LOOP (bst, -17);

  XSetWindowBackground (dpy, window, bg2);
  XClearWindow (dpy, window);
  XSetWindowBackground (dpy, window, bst->bg);

  if (pixmap)
    {
      int x = (bst->xgwa.width - pix_w) / 2;
      int y = ((bst->xgwa.height - pix_h) / 2);
      XSetClipMask (dpy, bst->gc, mask);
      XSetClipOrigin (dpy, bst->gc, x, y);
      XCopyArea (dpy, pixmap, bst->window, bst->gc, 0, 0, pix_w, pix_h, x, y);
      XSetClipMask (dpy, bst->gc, None);
      XFreePixmap (dpy, pixmap);
      XFreePixmap (dpy, mask);
    }

  bst->y += lw;

  return bst;
}



/* Atari ST, by Marcus Herbert <rhoenie@nobiscum.de>
   Marcus had this to say:

	Though I still have my Atari somewhere, I hardly remember
	the meaning of the bombs. I think 9 bombs was "bus error" or
	something like that.  And you often had a few bombs displayed
	quickly and then the next few ones coming up step by step.
	Perhaps somebody else can tell you more about it..  its just
	a quick hack :-}
 */
static struct bsod_state *
atari (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "atari", "Atari");

  int pix_w, pix_h;
  int offset;
  int i, x, y;
  Pixmap mask = 0;
  Pixmap pixmap = image_data_to_pixmap (dpy, window,
                                        atari_png, sizeof(atari_png),
                                        &pix_w, &pix_h, &mask);

  for (i = 0; i < 3; i++)
    {
      pixmap = double_pixmap (dpy, bst->xgwa.visual, bst->xgwa.depth,
                              pixmap, pix_w, pix_h);
      mask = double_pixmap (dpy, bst->xgwa.visual, 1, mask, pix_w, pix_h);
      pix_w *= 2; pix_h *= 2;
    }

  offset = pix_w;
  x = 0;
  y = bst->xgwa.height/2;
  if (y < 0) y = 0;

  for (i = 1; i< 7; i++)
    BSOD_COPY (bst, x, y, pix_w, pix_h, (x + (i*offset)), y);

  for (; i< 10; i++)
    {
      BSOD_PAUSE (bst, 1000000);
      BSOD_COPY (bst, x, y, pix_w, pix_h, (x + (i*offset)), y);
    }

  XClearWindow (dpy, window);
  XSetClipMask (dpy, bst->gc, mask);
  XSetClipOrigin (dpy, bst->gc, x, y);
  XCopyArea (dpy, pixmap, window, bst->gc, 0, 0, pix_w, pix_h, x, y);
  XSetClipMask (dpy, bst->gc, None);
  XFreePixmap (dpy, pixmap);
  XFreePixmap (dpy, mask);

  return bst;
}


static struct bsod_state *
mac (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "mac", "Mac");

  int pix_w, pix_h;
  int i;

  const char *string = ("0 0 0 0 0 0 0 F\n"
			"0 0 0 0 0 0 0 3");

  Pixmap mask = 0;
  Pixmap pixmap = image_data_to_pixmap (dpy, window,
                                        mac_png, sizeof(mac_png),
                                        &pix_w, &pix_h, &mask);
  int offset = pix_h * 4;

  bst->xoff = bst->left_margin = bst->right_margin = 0;

  for (i = 0; i < 2; i++)
    {
      pixmap = double_pixmap (dpy, bst->xgwa.visual, bst->xgwa.depth,
                              pixmap, pix_w, pix_h);
      mask = double_pixmap (dpy, bst->xgwa.visual, 1, mask, pix_w, pix_h);
      pix_w *= 2; pix_h *= 2;
    }

  bst->x = (bst->xgwa.width - pix_w) / 2;
  bst->y = (((bst->xgwa.height + offset) / 2) -
            pix_h -
            (bst->font->ascent + bst->font->descent) * 2);
  if (bst->y < 0) bst->y = 0;

  XClearWindow (dpy, window);
  XSetClipMask (dpy, bst->gc, mask);
  XSetClipOrigin (dpy, bst->gc, bst->x, bst->y);
  XCopyArea (dpy, pixmap, window, bst->gc, 0, 0, pix_w, pix_h, bst->x, bst->y);
  XSetClipMask (dpy, bst->gc, None);
  XFreePixmap (dpy, pixmap);
  XFreePixmap (dpy, mask);

  bst->y += offset + bst->font->ascent + bst->font->descent;
  BSOD_TEXT (bst, CENTER, string);

  return bst;
}


static struct bsod_state *
macsbug (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "macsbug", "MacsBug");

  __extension__
  const char *left = ("    SP     \n"
		      " 04EB0A58  \n"
		      "58 00010000\n"
		      "5C 00010000\n"
		      "   ........\n"
		      "60 00000000\n"
		      "64 000004EB\n"
		      "   ........\n"
		      "68 0000027F\n"
		      "6C 2D980035\n"
		      "   ....-..5\n"
		      "70 00000054\n"
		      "74 0173003E\n"
		      "   ...T.s.>\n"
		      "78 04EBDA76\n"
		      "7C 04EBDA8E\n"
		      "   .S.L.a.U\n"
		      "80 00000000\n"
		      "84 000004EB\n"
		      "   ........\n"
		      "88 00010000\n"
		      "8C 00010000\n"
		      "   ...{3..S\n"
		      "\n"
		      "\n"
		      " CurApName \n"
		      "  Finder   \n"
		      "\n"
		      " 32-bit VM \n"
		      "SR Smxnzvc0\n"
		      "D0 04EC0062\n"
		      "D1 00000053\n"
		      "D2 FFFF0100\n"
		      "D3 00010000\n"
		      "D4 00010000\n"
		      "D5 04EBDA76\n"
		      "D6 04EBDA8E\n"
		      "D7 00000001\n"
		      "\n"
		      "A0 04EBDA76\n"
		      "A1 04EBDA8E\n"
		      "A2 A0A00060\n"
		      "A3 027F2D98\n"
		      "A4 027F2E58\n"
		      "A5 04EC04F0\n"
		      "A6 04EB0A86\n"
		      "A7 04EB0A58");
  const char *bottom = ("  _A09D\n"
			"     +00884    40843714     #$0700,SR         "
			"                  ; A973        | A973\n"
			"     +00886    40843765     *+$0400           "
			"                                | 4A1F\n"
			"     +00888    40843718     $0004(A7),([0,A7[)"
			"                  ; 04E8D0AE    | 66B8");
  __extension__
  const char * body = ("PowerPC unmapped memory exception at 003AFDAC "
						"BowelsOfTheMemoryMgr+04F9C\n"
		      " Calling chain using A6/R1 links\n"
		      "  Back chain  ISA  Caller\n"
		      "  00000000    PPC  28C5353C  __start+00054\n"
		      "  24DB03C0    PPC  28B9258C  main+0039C\n"
		      "  24DB0350    PPC  28B9210C  MainEvent+00494\n"
		      "  24DB02B0    PPC  28B91B40  HandleEvent+00278\n"
		      "  24DB0250    PPC  28B83DAC  DoAppleEvent+00020\n"
		      "  24DB0210    PPC  FFD3E5D0  "
						"AEProcessAppleEvent+00020\n"
		      "  24DB0132    68K  00589468\n"
		      "  24DAFF8C    68K  00589582\n"
		      "  24DAFF26    68K  00588F70\n"
		      "  24DAFEB3    PPC  00307098  "
						"EmToNatEndMoveParams+00014\n"
		      "  24DAFE40    PPC  28B9D0B0  DoScript+001C4\n"
		      "  24DAFDD0    PPC  28B9C35C  RunScript+00390\n"
		      "  24DAFC60    PPC  28BA36D4  run_perl+000E0\n"
		      "  24DAFC10    PPC  28BC2904  perl_run+002CC\n"
		      "  24DAFA80    PPC  28C18490  Perl_runops+00068\n"
		      "  24DAFA30    PPC  28BE6CC0  Perl_pp_backtick+000FC\n"
		      "  24DAF9D0    PPC  28BA48B8  Perl_my_popen+00158\n"
		      "  24DAF980    PPC  28C5395C  sfclose+00378\n"
		      "  24DAF930    PPC  28BA568C  free+0000C\n"
		      "  24DAF8F0    PPC  28BA6254  pool_free+001D0\n"
		      "  24DAF8A0    PPC  FFD48F14  DisposePtr+00028\n"
		      "  24DAF7C9    PPC  00307098  "
						"EmToNatEndMoveParams+00014\n"
		      "  24DAF780    PPC  003AA180  __DisposePtr+00010");

  XGlyphInfo ov;
  const char *s;
  int body_lines = 1;

  int char_width, line_height;
  int col_right, row_top, row_bottom, page_right, page_bottom, body_top;
  int xoff, yoff;

  unsigned long fg = bst->fg;
  unsigned long bg = bst->bg;
  unsigned long bc = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "macsbug.borderColor",
                                         "MacsBug.BorderColor");

  bst->xoff = bst->left_margin = bst->right_margin = 0;

  for (s = body; *s; s++) if (*s == '\n') body_lines++;

  XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) "n", 1, &ov);
  char_width = ov.xOff;
  line_height = bst->font->ascent + bst->font->descent;

  col_right   = char_width  * 12;  /* number of columns in `left' */
  page_bottom = line_height * 47;  /* number of lines in `left'   */

  if (page_bottom > bst->xgwa.height - bst->yoff)
    page_bottom = bst->xgwa.height - bst->yoff;

  row_bottom = page_bottom - line_height;
  row_top    = row_bottom - (line_height * 4);
  page_right = col_right + (char_width * 88);
  body_top   = row_top - (line_height * body_lines);

  page_bottom += 2;
  row_bottom += 2;
  body_top -= 4;

  if (body_top > 4)
    body_top = 4;

  xoff = (bst->xgwa.width  - page_right)  / 2;
  yoff = (bst->xgwa.height - page_bottom) / 2;

  if (xoff < 0) xoff = 0;
  if (yoff < 0) yoff = 0;

  BSOD_MARGINS (bst, xoff, xoff);

  BSOD_COLOR (bst, bc, bg);
  BSOD_RECT (bst, True, 0, 0, bst->xgwa.width, bst->xgwa.height);
  BSOD_COLOR (bst, bg, bg);
  BSOD_RECT (bst, True, xoff-2, yoff, page_right+4, page_bottom);
  BSOD_COLOR (bst, fg, bg);

  BSOD_MOVETO (bst, xoff, yoff + line_height);
  BSOD_TEXT (bst, LEFT, left);
  BSOD_MOVETO (bst, xoff+col_right, yoff + row_top + line_height);
  BSOD_TEXT (bst, LEFT, bottom);

  BSOD_RECT (bst, True, xoff + col_right, yoff, 2, page_bottom);
  BSOD_RECT (bst, True, xoff + col_right, yoff + row_top,
             page_right - col_right, 1);
  BSOD_RECT (bst, True, xoff + col_right, yoff + row_bottom,
             page_right - col_right, 1);
  BSOD_RECT (bst, False, xoff-2, yoff, page_right+4, page_bottom);

  BSOD_LINE_DELAY (bst, 500);
  BSOD_MOVETO (bst,
               xoff + col_right + char_width,
               yoff + body_top + line_height);
  BSOD_MARGINS (bst,
                xoff + col_right + char_width,
                xoff + col_right + char_width);
  BSOD_TEXT (bst, LEFT, body);

  BSOD_RECT (bst, False, xoff-2, yoff, page_right+4, page_bottom); /* again */

  BSOD_RECT (bst, False,
             xoff + col_right + (char_width/2)+2,
             yoff + row_bottom + 2,
             0,
             page_bottom - row_bottom - 4);

  BSOD_PAUSE (bst, 666666);
  BSOD_INVERT (bst);
  BSOD_LOOP (bst, -3);

  XClearWindow (dpy, window);
  return bst;
}


static struct bsod_state *
mac1 (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "mac1", "Mac1");

  int pix_w, pix_h;
  int x, y;
  Pixmap mask = 0;
  Pixmap pixmap = image_data_to_pixmap (dpy, window,
                                        macbomb_png, sizeof(macbomb_png),
                                        &pix_w, &pix_h, &mask);

  if (pixmap && 
      pix_w < bst->xgwa.width / 2 &&
      pix_h < bst->xgwa.height / 2)
    {
      int i, n = 1;
      if (bst->xgwa.width > 2560 || bst->xgwa.height > 2560)
        n++;  /* Retina displays */
      for (i = 0; i < n; i++)
        {
          pixmap = double_pixmap (dpy, bst->xgwa.visual,
                                  bst->xgwa.depth, pixmap, pix_w, pix_h);
          mask = double_pixmap (dpy, bst->xgwa.visual, 1, mask, pix_w, pix_h);
          pix_w *= 2;
          pix_h *= 2;
        }
    }

  x = (bst->xgwa.width - pix_w) / 2;
  y = (bst->xgwa.height - pix_h) / 2;
  if (y < 0) y = 0;

  XClearWindow (dpy, window);
  XSetClipMask (dpy, bst->gc, mask);
  XSetClipOrigin (dpy, bst->gc, x, y);
  XCopyArea (dpy, pixmap, window, bst->gc, 0, 0, pix_w, pix_h, x, y);
  XSetClipMask (dpy, bst->gc, None);
  XFreePixmap (dpy, mask);

  return bst;
}


/* This is what kernel panics looked like on MacOS X 10.0 through 10.1.5.
   In later releases, it's a graphic of a power button with text in
   English, French, German, and Japanese overlayed transparently.
 */
static struct bsod_state *
macx_10_0 (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "macx", "MacX");
  unsigned long fg2 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                          "macx.textForeground",
                                          "MacX.TextForeground");
  unsigned long bg2 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                          "macx.textBackground",
                                          "MacX.TextBackground");
  XClearWindow (dpy, window);
  {
    Pixmap pixmap = 0;
    Pixmap mask = 0;
    int x, y, pix_w, pix_h;
    pixmap = image_data_to_pixmap (dpy, window,
                                   hmac_png, sizeof(hmac_png),
                                   &pix_w, &pix_h, &mask);

/*# ifdef HAVE_MOBILE*/
    if (pixmap)
      {
        pixmap = double_pixmap (dpy, bst->xgwa.visual,
                                bst->xgwa.depth, pixmap, pix_w, pix_h);
        mask = double_pixmap (dpy, bst->xgwa.visual,
                              1, mask, pix_w, pix_h);
        pix_w *= 2;
        pix_h *= 2;
      }
/*# endif*/

    x = (bst->xgwa.width - pix_w) / 2;
    y = (bst->xgwa.height - pix_h) / 2;
    if (y < 0) y = 0;
    XSetClipMask (dpy, bst->gc, mask);
    XSetClipOrigin (dpy, bst->gc, x, y);
    XCopyArea (dpy, pixmap, window, bst->gc, 0, 0, pix_w, pix_h, x, y);
    XSetClipMask (dpy, bst->gc, None);
    XFreePixmap (dpy, pixmap);
    XFreePixmap (dpy, mask);
  }

  bst->left_margin = 0;
  bst->right_margin = 0;
  bst->y = bst->font->ascent;
  bst->macx_eol_kludge = True;
  bst->wrap_p = True;

  BSOD_PAUSE (bst, 3000000);
  BSOD_COLOR(bst, fg2, bg2);
  BSOD_TEXT (bst, LEFT,
    "panic(cpu 0): Unable to find driver for this platform: "
    "\"PowerMac 3,5\".\n"
    "\n"
    "backtrace: 0x0008c2f4 0x0002a7a0 0x001f0204 0x001d4e4c 0x001d4c5c "
    "0x001a56cc 0x01d5dbc 0x001c621c 0x00037430 0x00037364\n"
    "\n"
    "\n"
    "\n"
    "No debugger configured - dumping debug information\n"
    "\n"
    "version string : Darwin Kernel Version 1.3:\n"
    "Thu Mar  1 06:56:40 PST 2001; root:xnu/xnu-123.5.obj~1/RELEASE_PPC\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "DBAT0: 00000000 00000000\n"
    "DBAT1: 00000000 00000000\n"
    "DBAT2: 80001FFE 8000003A\n"
    "DBAT3: 90001FFE 9000003A\n"
    "MSR=00001030\n"
    "backtrace: 0x0008c2f4 0x0002a7a0 0x001f0204 0x001d4e4c 0x001d4c5c "
    "0x001a56cc 0x01d5dbc 0x001c621c 0x00037430 0x00037364\n"
    "\n"
    "panic: We are hanging here...\n");

  return bst;
}


static struct bsod_state *
macx_10_2 (Display *dpy, Window window, Bool v10_3_p)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "macx", "MacX");

  Pixmap mask = 0;
  Pixmap pixmap = 0;
  int pix_w = 0, pix_h = 0;
  int x, y;

  if (v10_3_p)
    pixmap = image_data_to_pixmap (dpy, window,
                                   osx_10_3_png, sizeof(osx_10_3_png),
                                   &pix_w, &pix_h, &mask);
  else
    pixmap = image_data_to_pixmap (dpy, window,
                                   osx_10_2_png, sizeof(osx_10_2_png),
                                   &pix_w, &pix_h, &mask);
  if (! pixmap) abort();
  if (! mask) abort();

#if 1
  if (bst->xgwa.height > 600)	/* scale up the bitmap */
    {
      pixmap = double_pixmap (dpy, bst->xgwa.visual, bst->xgwa.depth,
                              pixmap, pix_w, pix_h);
      mask = double_pixmap (dpy, bst->xgwa.visual, 1, mask, pix_w, pix_h);
      if (! pixmap) abort();
      if (! mask) abort();
      pix_w *= 2;
      pix_h *= 2;
    }
#endif

  BSOD_IMG (bst);
  BSOD_PAUSE (bst, 2000000);

  bst->pixmap = pixmap;
  bst->mask = mask;

  x = (bst->xgwa.width - pix_w) / 2;
  y = ((bst->xgwa.height - pix_h) / 2);
  BSOD_PIXMAP (bst, 0, 0, pix_w, pix_h, x, y);

  return bst;
}


/* 2006 Mac Mini with MacOS 10.6 failing with a bad boot drive. By jwz.
 */
static struct bsod_state *
mac_diskfail (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "macdisk", "Mac");
  XGlyphInfo ov;
  int cw, h, L, T;
  unsigned long fg, bg, bg2;

  XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) "n", 1, &ov);
  cw = ov.xOff;
  h = bst->font->ascent + bst->font->descent;
  L = (bst->xgwa.width - (cw * 80)) / 2;
  T = (bst->xgwa.height - (h  * 10)) / 2;

  fg = bst->fg;
  bg = bst->bg;
  bg2 = get_pixel_resource (dpy, bst->xgwa.colormap,
                            "macx.background", "Mac.Background");
  if (L < 0) L = 0;
  if (T < 0) T = 0;

  bst->wrap_p = True;
  bst->scroll_p = True;

  BSOD_COLOR(bst, bg2, bg);
  BSOD_RECT (bst, True, 0, 0, bst->xgwa.width, bst->xgwa.height);
  BSOD_PAUSE (bst, 3000000);

  BSOD_COLOR(bst, bg, fg);
  BSOD_RECT (bst, True, 0, 0, bst->xgwa.width, bst->xgwa.height);
  BSOD_COLOR(bst, fg, bg);

  BSOD_MARGINS (bst, L, L);
  BSOD_MOVETO (bst, L, T);

  BSOD_TEXT (bst, LEFT,
             "efiboot loaded from device: Acpi(PNP0A03,0)/Pci*1F|2)/Ata"
             "(Primary,Slave)/HD(Part\n"
             "2,Sig8997E427-064E-4FE7-8CB9-F27A784B232C)\n"
             "boot file path: \\System\\Library\\CoreServices\\boot.efi\n"
             ".Loading kernel cache file 'System\\Library\\Caches\\"
             "com.apple.kext.caches\\Startup\\\n"
             "kernelcache_i386.2A14EC2C'\n"
             "Loading 'mach_kernel'...\n"
             );
  BSOD_CHAR_DELAY (bst, 7000);
  BSOD_TEXT (bst, LEFT,
             ".....................\n"
             );
  BSOD_CHAR_DELAY (bst, 0);
  BSOD_TEXT (bst, LEFT,
             "root device uuid is 'B62181B4-6755-3C27-BFA1-49A0E053DBD6\n"
             "Loading drivers...\n"
             "Loading System\\Library\\Caches\\com.apple.kext.caches\\"
             "Startup\\Extensions.mkext....\n"
             );
  BSOD_CHAR_DELAY (bst, 7000);
  BSOD_TEXT (bst, LEFT,
             "..............................................................."
             ".................\n"
             "..............................................................."
             ".................\n"
             "..............\n"
             );
  BSOD_INVERT (bst);
  BSOD_RECT (bst, True, 0, 0, bst->xgwa.width, bst->xgwa.height);
  BSOD_INVERT (bst);

  BSOD_MARGINS (bst, 0, 0);
  BSOD_MOVETO (bst, 0, h);

  BSOD_CHAR_DELAY (bst, 0);
  BSOD_LINE_DELAY (bst, 5000);
  BSOD_TEXT (bst, LEFT,
             "npvhash=4095\n"
             "PRE enabled\n"
             "Darwin Kernel Version 10.8.9: Tue Jun  7 16:33:36 PDT 2011;"
             " root:xnu-1504.15.3~1/RELEASE_I386\n"
             "vm_page_bootstrap: 508036 free pages and 16252 wired pages\n"
             "standard timeslicing quantum is 10000 us\n"
             "mig_table_max_displ = 73\n"
             "AppleACPICPU: ProcessorId=0 LocalApicId=0 Enabled\n"
             "AppleACPICPU: ProcessorId=1 LocalApicId=1 Enabled\n"
             "calling npo_policy_init for Quarantine\n"
             "Security policy loaded: Quaantine policy (Quarantine)\n"
             "calling npo_policy_init for Sandbox\n"
             "Security policy loaded: Seatbelt sandbox policy (Sandbox)\n"
             "calling npo_policy_init for TMSafetyNet\n"
             "Security policy loaded: Safety net for Time Machine "
             "(TMSafetyNet)\n"
             "Copyright (c) 1982, 1986, 1989, 1991, 1993\n"
             "The Regents of the University of California. All rights "
             "reserved.\n"
             "\n"
             "MAC Framework successfully initialized\n"
             "using 10485 buffer headers and 4096 cluster IO buffer headers\n"
             "IOAPIC: Version 0x20 Vectors 64:87\n"
             "ACPI: System State [S0 S3 S4 S5] (S3)\n"
             "PFM64 0x10000000, 0xf0000000\n"
             "[ PCI configuration begin ]\n"
             "PCI configuration changed (bridge=1 device=1 cardbus=0)\n"
             "[ PCI configuration end, bridges 4 devices 17 ]\n"
             "nbinit: done (64 MB memory set for nbuf pool)\n"
             "rooting via boot-uuid from /chosen: "
             "B62181B4-6755-3C27-BFA1-49A0E053DBD6\n"
             "Waiting on <dict ID=\"0\"><key>IOProviderClass</key>"
             "<string ID=\"1\">IOResources</string><key>IOResourceMatch</key>"
             "<string ID=\"2\">boot-uuid-nedia</string></dict>\n"
             "com.apple.AppleFSCCompressionTypeZlib kmod start\n"
             "com.apple.AppleFSCCompressionTypeZlib kmod succeeded\n"
             "AppleIntelCPUPowerManagementClient: ready\n"
             "FireWire (OHCI) Lucent ID 5811  built-in now active, GUID "
             "0019e3fffe97f8b4; max speed s400.\n"
             "Got boot device = IOService:/AppleACPIPlatformExpert/PCI000/"
             "AppleACPIPCI/SATA@1F,2/AppleAHCI/PRI202/IOAHCIDevice@0/"
             "AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/"
             "IOBlockStorageDriver/ST96812AS Media/IOGUIDPartitionScheme/"
             "Customer02\n"
             );
  BSOD_PAUSE (bst, 1000000);
  BSOD_TEXT (bst, LEFT,
             "BSD root: Disk0s, major 14, minor 2\n"
             "[Bluetooth::CSRHIDTransition] switchtoHCIMode (legacy)\n"
             "[Bluetooth::CSRHIDTransition] transition complete.\n"
             "CSRUSBBluetoothHCIController::setupHardware super returned 0\n"
             );
  BSOD_PAUSE (bst, 3000000);
  BSOD_TEXT (bst, LEFT,
             "disk0s2: I/O error.\n"
             "0 [Level 3] [ReadUID 0] [Facility com.apple.system.fs] "
             "[ErrType IO] [ErrNo 5] [IOType Read] [PBlkNum 48424] "
             "[LBlkNum 1362] [FSLogMsgID 2009724291] [FSLogMsgOrder First]\n"
             "0 [Level 3] [ReadUID 0] [Facility com.apple.system.fs] "
             "[DevNode root_device] [MountPt /] [FSLogMsgID 2009724291] "
             "[FSLogMsgOrder Last]\n"
             "panic(cpu 0 caller 0x47f5ad): \"Process 1 exec of /sbin/launchd"
             " failed, errno 5\\n\"0/SourceCache/xnu/xnu-1504.15.3/bsd/kern/"
             "kern_exec.c:3145\n"
             "Debugger called: <panic>\n"
             "Backtrace (CPU 0), Frame : Return Address (4 potential args "
             "on stack)\n"
             "0x34bf3e48 : 0x21b837 (0x5dd7fc 0x34bf3e7c 0x223ce1 0x0)\n"
             "0x34bf3e98 : 0x47f5ad (0x5cf950 0x831c08 0x5 0x0)\n"
             "0x34bf3ef8 : 0x4696d2 (0x4800d20 0x1fe 0x45a69a0 0x80000001)\n"
             "0x34bf3f38 : 0x48fee5 (0x46077a8 0x84baa0 0x34bf3f88 "
             "0x34bf3f94)\n"
             "0x34bf3f68 : 0x219432 (0x46077a8 0xffffff7f 0x0 0x227c4b)\n"
             "0x34bf3fa8 : 0x2aacb4 (0xffffffff 0x1 0x22f8f5 0x227c4b)\n"
             "0x34bf3fc8 : 0x2a1976 (0x0 0x0 0x2a17ab 0x4023ef0)\n"
             "\n"
             "BSD process name corresponding to current thread: init\n"
             "\n"
             "Mac OS version:\n"
             "Not yet set\n"
             "\n"
             "Kernel version:\n"
             "Darwin Kernel version 10.8.0: Tue Jun  7 16:33:36 PDT 2011; "
             "root:xnu-1504.15-3~1/RELEASE_I386\n"
             "System model name: Macmini1,1 (Mac-F4208EC0)\n"
             "\n"
             "System uptime in nanoseconds: 13239332027\n"
             );
  BSOD_CURSOR (bst, CURSOR_BLOCK, 500000, 999999);

  XClearWindow (dpy, window);

  return bst;
}


/* 2017 MacOS 10.12 interminable software update, by jwz.
 */
static struct bsod_state *
macx_install (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "macinstall", "MacX");

  int pix_w, pix_h;
  int x, y;
  int bw1, bh1;
  int bw2, bh2;

  unsigned long fg = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "macinstall.foreground",
                                         "Mac.Foreground");
  unsigned long bg = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "macinstall.background",
                                         "Mac.Background");
  unsigned long fg2 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "macinstall.barForeground",
                                         "Mac.Foreground");
  unsigned long bg2 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "macinstall.barBackground",
                                         "Mac.Background");
  char buf[1024];
  int lh = bst->font->ascent + bst->font->descent;
  int i, min;
  double pct;

  Pixmap mask = 0;
  Pixmap pixmap = image_data_to_pixmap (dpy, window,
                                        apple_png, sizeof(apple_png),
                                        &pix_w, &pix_h, &mask);

  bst->xoff = bst->left_margin = bst->right_margin = 0;

  if (pixmap)
    {
      int i, n = 0;
      if (bst->xgwa.width > 2560 || bst->xgwa.height > 2560)
        n++;  /* Retina displays */
      for (i = 0; i < n; i++)
        {
          pixmap = double_pixmap (dpy, bst->xgwa.visual, bst->xgwa.depth,
                                  pixmap, pix_w, pix_h);
          mask = double_pixmap (dpy, bst->xgwa.visual, 1, mask, pix_w, pix_h);
          pix_w *= 2;
          pix_h *= 2;
        }
    }

  bst->pixmap = pixmap;
  bst->mask = mask;

  x = (bst->xgwa.width - pix_w) / 2;
  y = (bst->xgwa.height) / 2  - pix_h;
  if (y < 0) y = 0;

  XSetLineAttributes (dpy, bst->gc, 1, LineSolid, CapRound, JoinMiter);

  BSOD_COLOR(bst, bg, bg);
  BSOD_RECT (bst, True, 0, 0, bst->xgwa.width, bst->xgwa.height);
  BSOD_COLOR(bst, fg, bg);
  BSOD_PIXMAP (bst, 0, 0, pix_w, pix_h, x, y);
  y += pix_h * 2 - lh;

  /* progress bar */
  bw1 = pix_w * 2.5;
  bh1 = lh * 0.66;
  if (bh1 < 8) bh1 = 8;

  x = (bst->xgwa.width - bw1) / 2;
  BSOD_COLOR(bst, fg2, bg);
  BSOD_LINE (bst, x,   y, x + bw1, y, bh1);

  bw2 = bw1 - 1;
  bh2 = bh1 - 4;
  BSOD_COLOR(bst, bg2, bg);
  BSOD_LINE (bst, x+1, y, x + bw2, y, bh2);

  BSOD_COLOR(bst, fg, bg);
  BSOD_LINE (bst, x,   y, x + 1, y, bh1);

  pct = 5 + (random() % 40);
  min = 5 + (random() % 40);

  for (i = 0; i < 100; i++) {
    pct += frand(0.3);
    min += (random() % 3) - 1;  /* sometimes down, mostly up */

    if (pct > 90) pct = 90;
    BSOD_RECT (bst, True, x, y - bh1/2, bw1 * pct / 100, bh1);

    sprintf (buf, "  Installing Software Update: about %d minutes.  ", min);
    bst->y = y + lh * 3;
    BSOD_TEXT (bst, CENTER, buf);
    BSOD_PAUSE (bst, 1000000);
  }

  return bst;
}


static struct bsod_state *
macx (Display *dpy, Window window)
{
  switch (random() % 5) {
  case 0: return macx_10_0 (dpy, window);        break;
  case 1: return macx_10_2 (dpy, window, False); break;
  case 2: return macx_10_2 (dpy, window, True);  break;
  case 3: return mac_diskfail (dpy, window);     break;
  case 4: return macx_install (dpy, window);     break;
  default: abort();
  }
}


#ifndef HAVE_JWXYZ /* #### I have no idea how to implement this without
                           real plane-masks.  I don't think it would look
                           right if done with alpha-transparency... */
/* blit damage
 *
 * by Martin Pool <mbp@samba.org>, Feb 2000.
 *
 * This is meant to look like the preferred failure mode of NCD
 * Xterms.  The parameters for choosing what to copy where might not
 * be quite right, but it looks about ugly enough.
 */
static struct bsod_state *
blitdamage (Display *dpy, Window window)
{
  struct bsod_state *bst =
    make_bsod_state (dpy, window, "blitdamage", "BlitDamage");

  int i;
  int delta_x = 0, delta_y = 0;
  int w, h;
  int chunk_h, chunk_w;
  int steps;
  int src_x, src_y;
  int x, y;

  w = bst->xgwa.width;
  h = bst->xgwa.height;

  XSetPlaneMask (dpy, bst->gc, random());

  steps = 50;
  chunk_w = w / (random() % 1 + 1);
  chunk_h = h / (random() % 1 + 1);
  if (random() & 0x1000)
    delta_y = random() % 600;
  if (!delta_y || (random() & 0x2000))
    delta_x = random() % 600;
  src_x = 0;
  src_y = 0;
  x = 0;
  y = 0;

  BSOD_IMG (bst);
  for (i = 0; i < steps; i++) {
    if (x + chunk_w > w)
      x -= w;
    else
      x += delta_x;

    if (y + chunk_h > h)
      y -= h;
    else
      y += delta_y;

    BSOD_COPY (bst, src_x, src_y, chunk_w, chunk_h, x, y);
    BSOD_PAUSE (bst, 1000);
  }

  return bst;
}
#endif /* !HAVE_JWXYZ */


/*
 * OS/2 panics, by Knut St. Osmundsen <bird-xscreensaver@anduin.net>
 *
 * All but one messages are real ones, some are from my test machines
 * and system dumps, others are reconstructed from google results.
 * Please, don't be to hard if the formatting of the earlier systems
 * aren't 100% correct.
 */
static struct bsod_state *
os2 (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "os2", "OS2");

  __extension__
  static const char * const os2_panics[] =
    { /* OS/2 2.0 trap - details are bogus (CR0++). */
      "TRAP 0002       ERRCD=0000  ERACC=****  ERLIM=********\n"
      "EAX=7d240a58  EBX=ff202fdc  ECX=00064423  EDX=00003624\n"
      "ESI=fff3272c  EDI=7d240004  EBP=00004a44  FLG=00003202\n"
      "CS:EIP=0160:fff702a6  CSACC=c09d  CSLIM=ffffffff\n"
      "SS:ESP=0030:00004a38  SSACC=1097  SSLIM=00003fff\n"
      "DS=0158  DSACC=c0f3  DSLIM=ffffffff  CR0=fffffffb\n"
      "ES=0158  ESACC=c0f3  ESLIM=ffffffff  CR2=1a060014\n"
      "FS=0000  FSACC=****  FSLIM=********\n"
      "GS=0000  GSACC=****  GSLIM=********\n"
      "\n"
      "The system detected an internal processing error\n"
      "at location ##0160:fff6453f - 000d:a53f\n"
      "60000, 9084\n"
      "\n"
      "038600d1\n"
      "Internal revision 6.307, 92/03/01\n"
      "\n",

      /* warp 3 (early) */
      "TRAP 000e       ERRCD=0000  ERACC=****  ERLIM=********\n"
      "EAX=ff050c20  EBX=000000bb  ECX=ffff00c1  EDx=fff379b8\n"
      "ESI=ffe55a3c  EDI=00000000  EBP=00004eb8  FLG=00013282\n"
      "CS:EIP=0160:fff8dbb8  CSACC=c09b  CSLIM=ffffffff\n"
      "SS:EIP=0030:00004eb4  SSACC=1097  SSLIM=00003fff\n"
      "DS=0158  DSACC=c0f3  DSLIM=ffffffff  CR0=8001001b\n"
      "ES=0158  DSACC=c0f3  DSLIM=ffffffff  CR2=000000c7\n"
      "FS=0000  FSACC=****  FSLIM=********\n"
      "GS=0000  GSACC=****  GSLIM=********\n"
      "\n"
      "The system detected an internal processing error\n"
      "at location ##0160:fff66bf0 - 000d:9bf0.\n"
      "60000, 9084\n"
      "\n"
      "048600b4\n"
      "Internal revision 8.125, 94/02/16\n"
      "\n"
      "The system is stopped.  Record the location number of the error\n"
      "and contact your service representative.\n",

      /* warp 3 */
      "TRAP 000e       ERRCD=0002  ERACC=****  ERLIM=********\n"
      "EAX=00000000  EBX=fdef1e0c  ECX=00003824  EDX=0000edf9\n"
      "ESI=fdf30e80  EDI=fc8b0000  EBP=00005658  FLG=00012246\n"
      "CS:EIP=0160:fff8ada3  CSACC=c09b  CSLIM=ffffffff\n"
      "SS:ESP=0030:000055d4  SSACC=1097  SSLIM=0000480f\n"
      "DS=0158  DSACC=c093  DSLIM=ffffffff  CR0=8001001b\n"
      "ES=0158  ESACC=c093  ESLIM=ffffffff  CR2=fc8b0000\n"
      "FS=03b8  FSACC=0093  FSLIM=00000023\n"
      "GS=0000  GSACC=****  GSLIM=********\n"
      "\n"
      "The system detected an internal processing error\n"
      "at location ##0160:fff5c364 - 000d:a364.\n"
      "60000, 9084\n"
      "\n"
      "05860526\n"
      "Internal revision 8200,94/11/07\n"
      "\n"
      "The system is stopped. Record all of the above information and\n"
      "contact your service representative.\n",

      /* warp 3 (late) */
      "TRAP 000d       ERRCD=2200  ERACC=1092  ERLIM=00010fff\n"
      "EAX=0000802e  EBX=fff001c8  ECX=9bd80000  EDX=00000000\n"
      "ESI=fff09bd8  EDI=fdeb001b  EBP=00000000  FLG=00012012\n"
      "CS:EIP=0168:fff480a2  CSACC=c09b  CSLIM=ffffffff\n"
      "SS:ESP=00e8:00001f32  SSACC=0093  SSLIM=00001fff\n"
      "DS=0940  DSACC=0093  DSLIM=00000397  CR0=8001001b\n"
      "ES=00e8  ESACC=0093  ESLIM=00001fff  CR2=15760008\n"
      "FS=0000  FSACC=****  FSLIM=****\n"
      "GS=0000  GSACC=****  GSLIM=****\n"
      "\n"
      "The system detected an internal processing error\n"
      "at location ##0168:fff4b06e - 000e:c06e\n"
      "60000, 9084\n"
      "\n"
      "06860652\n"
      "Internal revision 8.259_uni,98/01/07\n"
      "\n"
      "The system is stopped. Record all of the above information and\n"
      "contact your service representative.\n",

      /* Warp 4.52+ - the official r0trap.exe from the debugging classes */
      "Exception in module: OS2KRNL\n"
      "TRAP 000e       ERRCD=0002  ERACC=****  ERLIM=********\n"
      "EAX=00000001  EBX=80010002  ECX=ffed4638  EDX=0003f17b\n"
      "ESI=00000001  EDI=00000002  EBP=00005408  FLG=00012202\n"
      "CS:EIP=0168:fff3cd2e  CSACC=c09b  CSLIM=ffffffff\n"
      "SS:ESP=0030:000053ec  SSACC=1097  SSLIM=000044ff\n"
      "DS=0160  DSACC=c093  DSLIM=ffffffff  CR0=8001001b\n"
      "ES=0160  ESACC=c093  ESLIM=ffffffff  CR2=00000001\n"
      "FS=0000  FSACC=****  FSLIM=********\n"
      "GS=0000  GSACC=****  GSLIM=********\n"
      "\n"
      "The system detected an internal processing error at\n"
      "location ##0168:fff1e3f3 - 000e:c3f3.\n"
      "60000, 9084\n"
      "\n"
      "068606a0\n"
      "Internal revision 14.097_UNI\n"
      "\n"
      "The system is stopped. Record all of the above information and\n"
      "contact your service representative.\n",

      /* Warp 4.52+, typical JFS problem. */
      "Exception in module: JFS\n"
      "TRAP 0003       ERRCD=0000  ERACC=****  ERLIM=********\n"
      "EAX=00000000  EBX=ffffff05  ECX=00000001  EDX=f5cd8010\n"
      "ESI=000000e6  EDI=000000e7  EBP=f9c7378e  FLG=00002296\n"
      "CS:EIP=0168:f8df3250  CSACC=c09b  CSLIM=ffffffff\n"
      "SS:ESP=1550:fdc73778  SSACC=c093  SSLIM=ffffffff\n"
      "DS=0160  DSACC=c093  DSLIM=ffffffff  CR0=80010016\n"
      "ES=0160  ESACC=c093  DSLIM=ffffffff  CR2=05318000\n"
      "FS=03c0  FSACC=0093  DSLIM=00000023\n"
      "GS=0160  GSACC=c093  DSLIM=ffffffff\n"
      "\n"
      "The system detected an internal processing error\n"
      "at location ##0168:fff1e2ab - 000e:c2ab.\n"
      "60000, 9084\n"
      "\n"
      "07860695\n"
      "\n"
      "Internal revision 14.100c_UNI\n"
      "\n"
      "The system is stopped. Record all of the above information and\n"
      "contact your service representative.\n"
    };

  BSOD_TEXT (bst, LEFT, os2_panics[random() % countof(os2_panics)]);
  BSOD_CURSOR (bst, CURSOR_LINE, 240000, 999999);

  XClearWindow (dpy, window);
  return bst;
}


/* SPARC Solaris panic. Should look pretty authentic on Solaris boxes.
 * Anton Solovyev <solovam@earthlink.net>
 */
static struct bsod_state *
sparc_solaris (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "solaris", "Solaris");
  int i;
  int pix_w = 0, pix_h = 0;
  int char_width;
  Pixmap mask = 0;
  Pixmap pixmap = image_data_to_pixmap (dpy, window,
                                        sun_png, sizeof(sun_png),
                                        &pix_w, &pix_h, &mask);
  XGlyphInfo ov;
  XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) "n", 1, &ov);
  char_width = ov.xOff;

  if (pixmap)
    while (pix_w < char_width * 4)
      {
        pixmap = double_pixmap (dpy, bst->xgwa.visual,
                                bst->xgwa.depth, pixmap, pix_w, pix_h);
        mask = double_pixmap (dpy, bst->xgwa.visual, 1, mask, pix_w, pix_h);
        pix_w *= 2;
        pix_h *= 2;
      }

  bst->pixmap = pixmap;
  bst->mask = mask;

  bst->scroll_p = True;
  bst->wrap_p = True;
  bst->left_margin = bst->right_margin = bst->xgwa.width  * 0.07;
  bst->top_margin = bst->bottom_margin = bst->xgwa.height * 0.07;
  bst->y = bst->top_margin + bst->yoff + bst->font->ascent;

# if 0
  /* This looks correct if we have a desktop image behind it, but doesn't
     make sense if it's a photo.  So let's skip it. */
  BSOD_IMG (bst);
  BSOD_PAUSE (bst, 3000000);
# endif

  BSOD_LINE_DELAY (bst, 20000);

  BSOD_INVERT(bst);
  BSOD_RECT (bst, True,
             bst->left_margin, bst->top_margin,
             bst->xgwa.width - bst->left_margin - bst->right_margin,
             bst->xgwa.height - bst->top_margin - bst->bottom_margin);
  BSOD_INVERT(bst);

  BSOD_TEXT (bst, LEFT,
    "BAD TRAP: cpu=0 type=0x31 rp=0x2a10043b5e0 addr=0xf3880 mmu_fsr=0x0\n"
    "BAD TRAP occurred in module \"unix\" due to an illegal access to a"
    " user address.\n"
    "adb: trap type = 0x31\n"
    "addr=0xf3880\n"
    "pid=307, pc=0x100306e4, sp=0x2a10043ae81, tstate=0x4480001602,"
    " context=0x87f\n"
    "g1-g7: 1045b000, 32f, 10079440, 180, 300000ebde8, 0, 30000953a20\n"
    "Begin traceback... sp = 2a10043ae81\n"
    "Called from 100bd060, fp=2a10043af31, args=f3700 300008cc988 f3880 0"
    " 1 300000ebde0.\n"
    "Called from 101fe1bc, fp=2a10043b011, args=3000045a240 104465a0"
    " 300008e47d0 300008e48fa 300008ae350 300008ae410\n"
    "Called from 1007c520, fp=2a10043b0c1, args=300008e4878 300003596e8 0"
    " 3000045a320 0 3000045a220\n"
    "Called from 1007c498, fp=2a10043b171, args=1045a000 300007847f0 20"
    " 3000045a240 1 0\n"
    "Called from 1007972c, fp=2a10043b221, args=1 300009517c0 30000951e58 1"
    " 300007847f0 0\n"
    "Called from 10031e10, fp=2a10043b2d1, args=3000095b0c8 0 300009396a8"
    " 30000953a20 0 1\n"
    "Called from 10000bdd8, fp=ffffffff7ffff1c1, args=0 57 100131480"
    " 100131480 10012a6e0 0\n"
    "End traceback...\n"
    "panic[cpu0]/thread=30000953a20: trap\n"
    "syncing file systems...\033");

  BSOD_PAUSE (bst, 3000000);

  BSOD_TEXT (bst, LEFT, "\b 1 done\n");
  BSOD_TEXT (bst, LEFT, "dumping to /dev/dsk/c0t0d0s3, offset 26935296\n\033");
  BSOD_PAUSE (bst, 2000000);
  BSOD_TEXT (bst, LEFT, "\b");

  for (i = 1; i <= 100; ++i)
    {
      char buf[100];
      sprintf (buf, "\r %3d%% done\033", i);
      BSOD_TEXT (bst, LEFT, buf);
      BSOD_PAUSE (bst, 100000);
    }

  BSOD_TEXT (bst, LEFT,
    "\b: 2803 pages dumped, compression ratio 2.88, dump succeeded\n\033");
  BSOD_PAUSE (bst, 2000000);

  BSOD_TEXT (bst, LEFT,
    "\brebooting...\n"
    "Resetting ...\n\033");

  BSOD_PAUSE (bst, 3000000);

  BSOD_INVERT(bst);
  BSOD_RECT (bst, True,
             bst->left_margin, bst->top_margin,
             bst->xgwa.width - bst->left_margin - bst->right_margin,
             bst->xgwa.height - bst->top_margin - bst->bottom_margin);
  BSOD_INVERT(bst);
  BSOD_MOVETO (bst, bst->left_margin, bst->top_margin + bst->font->ascent);
  BSOD_PAUSE (bst, 1000000);

  BSOD_TEXT (bst, LEFT,
    "Starting real time clock...\n"
    "Probing /sbus@1,f8000000 at 0,0  dma esp sd st le\n"
    "Probing /sbus@1,f8000000 at 1,0  Invalid FCode start byte at ffe70000\n"
    "Probing /sbus@1,f8000000 at 2,0  Invalid FCode start byte at ffe70000\n"
    "Probing /sbus@1,f8000000 at 3,0  bwtwo\n"
    "\n");

  BSOD_PIXMAP (bst, 0, 0, pix_w, pix_h, ~0, ~0);
  BSOD_MARGINS (bst,
                bst->left_margin + pix_w + char_width * 2,
                bst->left_margin + pix_w + char_width * 2);
  BSOD_TEXT (bst, LEFT,
             "SPARCstation IPC, Keyboard Present\n"
             "ROM Rev. 2.9, 16 MB memory installed, Serial #12648190.\n"
             "Ethernet address 8:0:20:37:1:87, Host ID: 52c0fefe.\n");
  BSOD_MARGINS (bst, bst->left_margin, bst->left_margin);

  BSOD_TEXT (bst, LEFT, "\n\n\033");
  BSOD_PAUSE (bst, 3000000);
  BSOD_TEXT (bst, LEFT, "\r");

  BSOD_TEXT (bst, LEFT, "Testing 16 megs of memory. Still to go \033 16");

  for (i = 16; i > 0; i--)
    {
      char buf[100];
      sprintf (buf, "\b\b%2d", i);
      BSOD_TEXT (bst, LEFT, buf);
      BSOD_PAUSE (bst, 100000);
    }
  BSOD_TEXT (bst, LEFT, "\b\b\b\b   0\n"
             "Initializing  16 megs of memory at addr         0\033 16");
  for (i = 16; i >= 0; i--)
    {
      char buf[100];
      sprintf (buf, "\b\b%2d", i);
      BSOD_TEXT (bst, LEFT, buf);
      BSOD_PAUSE (bst, 30000);
    }
  BSOD_TEXT (bst, LEFT, "\r"
             "                                                          \r"
             "Boot device: /sbus/le@0,c00000   File and args:\n\033");

  {
    int n = (random() % 10);
    for (i = 0; i < n; i++)
      {
        BSOD_PAUSE (bst, 3000000);
        BSOD_TEXT (bst, LEFT, "\rTimeout waiting for ARP/RARP packet\n\033");
      }
  }
  BSOD_TEXT (bst, LEFT, "\r");

  BSOD_TEXT (bst, LEFT,
    "Internal loopback test -- Wrong packet length;"
      " expected 36, observed 1600\n"
    "Can't open boot device\n"
    "\n"
    "Type b (boot), c (continue), or n (new command mode)\n"
    ">");
  BSOD_CURSOR (bst, CURSOR_BLOCK, 500000, 10);
  BSOD_TEXT (bst, LEFT, "n\n");
  BSOD_PAUSE (bst, 500000);
  BSOD_TEXT (bst, LEFT, "Type  help  for more information\n"
             "ok ");

  BSOD_CURSOR (bst, CURSOR_BLOCK, 500000, 4);
  BSOD_CHAR_DELAY (bst, 80000);
  BSOD_TEXT (bst, LEFT, /* "test net" */
             "t\033\be\033\bs\033\bt\033\b \033\bn\033\be\033\bt\033\b");
  BSOD_CURSOR (bst, CURSOR_BLOCK, 500000, 2);
  BSOD_TEXT (bst, LEFT, "\n\033");
  BSOD_CHAR_DELAY (bst, 0);

  BSOD_PAUSE (bst, 1000000);
  BSOD_TEXT (bst, LEFT, "\r Lance register test -- succeeded.\n\033");
  BSOD_PAUSE (bst, 1000000);
  BSOD_TEXT (bst, LEFT, "\r Internal loopback test -- succeeded.\n\033");
  BSOD_PAUSE (bst, 1000000);
  BSOD_TEXT (bst, LEFT, "\r External loopback test -- succeeded.\n"
             "ok ");
  BSOD_CURSOR (bst, CURSOR_BLOCK, 500000, 8);

  BSOD_TEXT (bst, LEFT, "\rok ");
  BSOD_CURSOR (bst, CURSOR_BLOCK, 500000, 4);
  BSOD_CHAR_DELAY (bst, 80000);
  BSOD_TEXT (bst, LEFT, /* "boot cdrom" */
     "b\033\bo\033\bo\033\bt\033\b \033\bc\033\bd\033\br\033\bo\033\bm\033\b");
  BSOD_TEXT (bst, LEFT, "  \n");
  BSOD_CHAR_DELAY (bst, 0);
  BSOD_TEXT (bst, LEFT,
    "\rBoot device: /sbus/esp@0,800000/sd@6,0:c    File and args:\n\033");
  BSOD_PAUSE (bst, 1000000);
  BSOD_TEXT (bst, LEFT,
    "\rroot on   fstype 4.3\n"
    "Boot: vmunix\n\033");

  BSOD_TEXT (bst, LEFT, "\rSize: 696320+");
  BSOD_CHAR_DELAY (bst, 5000);
  BSOD_INVERT(bst);
  BSOD_TEXT (bst, LEFT,  /* spinner */
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/"
             "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/" "\b-\b\\\b|\b/");
  BSOD_INVERT(bst);
  BSOD_TEXT (bst, LEFT, "\b+");
  BSOD_CHAR_DELAY (bst, 0);
  BSOD_TEXT (bst, LEFT,
             "2218504+28056 bytes\n\033");
  BSOD_PAUSE (bst, 1000000);

  i = random() % 3;

  if (i == 0)
    {
      BSOD_TEXT (bst, LEFT,
        "\rSunOS Release 4.1.1 (MUNIX) #1: Thu Oct 11 11:22:48 PDT 1990\n"
        "Copyright (c) 1983-1990, Sun Microsystems, Inc.\n"
        "mem = 16384K (0x1000000)\n"
        "avail mem = 12865536\n"
        "Ethernet address = 8:0:20:37:1:87\n"
        "No FPU in configuration\n"
        "cpu = SUNW,Sun 4/40\n"
        "zs0 at obio 0xf1000000 pri 12\n"
        "zs1 at obio 0xf0000000 pri 12\n"
        "sbus0 at SBus slot 0 0x0\n"
        "dma0 at SBus slot 0 0x400000\n"
        "esp0 at SBus slot 0 0x800000 pri 3\n"
        "\033");
    }
  else if (i == 1)
    {
      BSOD_TEXT (bst, LEFT,
        "\rSunOS Release 5.6 Version Generic [UNIX(R) System V Release 4.0]\n"
        "Copyright (c) 1983-1997, Sun Microsystems, Inc.\n"
        "No FPU in configuration\n"
        "WARNING: kbd: Unknown keyboard type, Type 3 assumed.\n"
        "WARNING: kbd: Unknown keyboard type, Type 3 assumed.\n"
        "Configuring devices...\n"
        "\033");
      BSOD_PAUSE (bst, 1000000);
      BSOD_TEXT (bst, LEFT,
        "\rWARNING: /sbus@1,f8000000/esp@0,800000/sd@6,0 (sd6):\n"
        "        incomplete read- retrying\n"
        "\n"
        "loadkeys: ioctl(KIOCLAYOUT): Invalid argument\n"
        "\033");
    }
  else
    {
      BSOD_TEXT (bst, LEFT,
        "\rSunOS Release 5.3 Version Generic [UNIX(R) System V Release 4.0]\n"
        "Copyright (c) 1983-1993, Sun Microsystems, Inc.\n"
        "No FPU in configuration\n"
        "WARNING: /sbus@1,f8000000/esp@0,800000/sd@3,0 (sd3):\n"
        "        corrupt label - wrong magic number\n"
        "\n"
        "\\       unexpected data phase\n\033");
      for (i = 0; i < 3; i++)
        {
          BSOD_PAUSE (bst, 3000000);
          if (i > 0)
            BSOD_TEXT (bst, LEFT,
                       "\r        polled command timeout\n");
          BSOD_TEXT (bst, LEFT,
           "\resp:         State=DATA Last State=UNKNOWN\n"
           "esp:         Latched stat=0x11<XZERO,IO> intr=0x10<BUS,FCMO>"
                   " fifo 0x0\n"
           "esp:         lst msg out: IDENTIFY; lst msg in: COMMAND COMPLETE\n"
           "esp:         DMA csr=0x10<INTEN>\n"
           "esp:         addr=fff0100f dmacnt=8000 last=fff01008 last_cnt=7\n"
           "esp:         Cmd dump for Target 6 Lun 0:\n"
           "esp:         cdblen=6, cdb=[ 0x0 0x0 0x0 0x0 0x1 0x0 ]\n"
           "esp:         pkt_state 0x3<SEL,ARB> pkt_flags 0x10000"
                   " pkt_statistics 0x0\n"
           "esp:         cmd_flags=0x10022 cmd_timeout=120\n"
           "\033");
        }
      BSOD_PAUSE (bst, 3000000);
      BSOD_TEXT (bst, LEFT,
        "\rWARNING: /sbus@1,f8000000/esp@0,800000/sd@6,0 (sd6):\n"
        "        incomplete read- retrying\n"
        "\n"
        "loadkeys: ioctl(KIOCLAYOUT): Invalid argument\n"
        "\033");
  }

  BSOD_PAUSE (bst, 1000000);
  BSOD_TEXT (bst, LEFT,
    "\rsd1 at esp0 target 1 lun 0\n"
    "sd1:    corrupt label - wrong magic number\n"
    "sd1: Vendor ' SEAGAT', product ' ', 786432 512 byte blocks\n"
    "sr0: Unrecongized vendor 'Sony    ',"
      " product 'CDU-76S        'sr0 at esp0 target 6 lun 0\n"
    "le0 at SBus slot 0 0xc00000 pri 5\n"
    "fd0 at obio 0xf7200000 pri 11\n\033");
  BSOD_PAUSE (bst, 1000000);
  BSOD_TEXT (bst, LEFT,
    "\rrd0: using preloaded munixfs\n"
    "WARNING: TOD clock not initialized -- CHECK AND RESET THE DATE!\n"
    "root on rd0a fstype 4.2\n"
    "swap on ns0b fstype spec size 12480K\n"
    "dump on ns0b fstype spec size 12468K\n"
    "\033");
  BSOD_PAUSE (bst, 4000000);

  i = random() % 3;

  if (i == 0)
    {
      BSOD_TEXT (bst, LEFT,
        "\rStarting OpenWindows...\n"
        "\n"
        "\n"
        "waiting for X server to begin accepting connections \033");
      BSOD_PAUSE (bst, 2500000);
      BSOD_TEXT (bst, LEFT, "\b.\033");
      BSOD_PAUSE (bst, 2500000);
      BSOD_TEXT (bst, LEFT, "\b \n.\033");
      for (i = 0; i < 10; i++)
        {
          BSOD_PAUSE (bst, 2500000);
          BSOD_TEXT (bst, LEFT, "\b.\033");
          BSOD_PAUSE (bst, 2500000);
          BSOD_TEXT (bst, LEFT, "\b \n.\033");
        }
    }
  else if (i == 1)
    {
      BSOD_TEXT (bst, LEFT,
        "\rCan't invoke /sbin/init, error 13\n"
        "Can't invoke /etc/init, error 13\n"
        "Can't invoke /bin/init, error 13\n"
        "Can't invoke /usr/etc/init, error 13\n"
        "Can't invoke /usr/bin/init, error 13\n"
        "panic: icode\n"
        "syncing file systems...\033");
      BSOD_PAUSE (bst, 2000000);
      BSOD_TEXT (bst, LEFT, "\b done\n");
      BSOD_TEXT (bst, LEFT,
        "00000 low-memory static kernel pages\n"
        "00888 additional static and sysmap kernel pages\n"
        "00000 dynamic kernel data pages\n"
        "00008 additional user structure pages\n"
        "00000 segmap kernel pages\n"
        "00000 segvn kernel pages\n"
        "00000 current user process pages\n"
        "00000 user stack pages\n"
        "00896 total pages (896 chunks)\n"
        "\n"
        "dumping to vp ff007dd4, offset 17768\n"
        "0 total pages, dump failed: error 19\n"
        "rebooting...\n");
    }
  else
    {
      BSOD_TEXT (bst, LEFT,
        "\r  \n"
        "What would you like to do?\n"
        "  1 - install SunOS mini-root\n"
        "  2 - exit to single user shell\n"
        "Enter a 1 or 2: ");
      BSOD_CURSOR (bst, CURSOR_BLOCK, 500000, 4);
      BSOD_TEXT (bst, LEFT,
                 "2\n"
                 "you may restart this script by typing <cntl-D>\n# ");
      BSOD_CURSOR (bst, CURSOR_BLOCK, 500000, 4);
      BSOD_CHAR_DELAY (bst, 80000);
      BSOD_TEXT (bst, LEFT, "l\033\bs\033\b");
      BSOD_TEXT (bst, LEFT, "  \n");
      BSOD_CHAR_DELAY (bst, 0);
      BSOD_TEXT (bst, LEFT,
        ".MUNIXFS        bin             extract         stand\n"
        ".profile        dev             lib             tmp\n"
        "README          etc             sbin            usr\n"
        "# ");
      BSOD_CURSOR (bst, CURSOR_BLOCK, 500000, 4);
      BSOD_CHAR_DELAY (bst, 80000);
      BSOD_TEXT (bst, LEFT, /* ". extract" */
        ".\033\b \033\be\033\bx\033\bt\033\br\033\ba\033\bc\033\bt\033\b");
      BSOD_TEXT (bst, LEFT, "  \n");
      BSOD_CHAR_DELAY (bst, 0);
      BSOD_TEXT (bst, LEFT,
         "using cdrom partition number 2\n"
         "esp0:    data transfer overrun\n"
         "         State=DATA Last State=DATA_DNE\n"
         "         Latched stat=0x11<XZERO,IO> intr=0x10<BUS> fifo 0x0\n"
         "         lst msg out: EXTENDED; lst msg in: COMMAND COMPLETE\n"
         "         DMA csr=0x10<INTEN>\n"
         "         addr=fff026d0 last=fff024d0 last_count=200\n"
         "         Cmd dump for Target 6 Lun 0:\n"
         "         cdb=[ 0x8 0x0 0x0 0x0 0x1 0x0 ]\n"
         "         pkt_state 0xf<XFER,CMD,SEL,ARB> pkt_flags 0x0"
                 " pkt_statistics 0x0\n"
         "         Mapped Dma Space:\n"
         "                 Base = 0x24d0 Count = 0x200\n"
         "         Transfer History:\n"
         "                 Base = 0x24d0 Count = 0x200\n");
    }

  BSOD_CURSOR (bst, CURSOR_BLOCK, 500000, 8);

  XClearWindow(dpy, window);

  return bst;
}


/* Linux panic and fsck, by jwz
 */
static struct bsod_state *
linux_fsck (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "linux", "Linux");

  int i;
  const char *sysname;
  char buf[1024];
# ifdef HAVE_UNAME
  struct utsname uts;
#endif /* UNAME */

  const char * const linux_panic[] = {
   " kernel: Unable to handle kernel paging request at virtual "
     "address 0000f0ad\n",
   " kernel:  printing eip:\n",
   " kernel: c01becd7\n",
   " kernel: *pde = 00000000\n",
   " kernel: Oops: 0000\n",
   " kernel: CPU:    0\n",
   " kernel: EIP:    0010:[<c01becd7>]    Tainted: P \n",
   " kernel: EFLAGS: 00010286\n",
   " kernel: eax: 0000ff00   ebx: ca6b7e00   ecx: ce1d7a60   edx: ce1d7a60\n",
   " kernel: esi: ca6b7ebc   edi: 00030000   ebp: d3655ca0   esp: ca6b7e5c\n",
   " kernel: ds: 0018   es: 0018   ss: 0018\n",
   " kernel: Process crond (pid: 1189, stackpage=ca6b7000)\n",
   " kernel: Stack: d3655ca0 ca6b7ebc 00030054 ca6b7e7c c01c1e5b "
       "00000287 00000020 c01c1fbf \n",
   "",
   " kernel:        00005a36 000000dc 000001f4 00000000 00000000 "
       "ce046d40 00000001 00000000 \n",
   "", "", "",
   " kernel:        ffffffff d3655ca0 d3655b80 00030054 c01bef93 "
       "d3655ca0 ca6b7ebc 00030054 \n",
   "", "", "",
   " kernel: Call Trace:    [<c01c1e5b>] [<c01c1fbf>] [<c01bef93>] "
       "[<c01bf02b>] [<c0134c4f>]\n",
   "", "", "",
   " kernel:   [<c0142562>] [<c0114f8c>] [<c0134de3>] [<c010891b>]\n",
   " kernel: \n",
   " kernel: Code: 2a 00 75 08 8b 44 24 2c 85 c0 74 0c 8b 44 24 58 83 48 18 "
      "08 \n",
   0
  };

  bst->scroll_p = True;
  bst->wrap_p = True;
  bst->left_margin = bst->right_margin = 10;
  bst->top_margin = bst->bottom_margin = 10;

  sysname = "linux";
# ifdef HAVE_UNAME
  {
    char *s;
    if (uname (&uts) >= 0)
      sysname = uts.nodename;
    s = strchr (sysname, '.');
    if (s) *s = 0;
  }
# endif	/* !HAVE_UNAME */


  BSOD_TEXT (bst, LEFT, "waiting for X server to shut down ");
  BSOD_PAUSE (bst, 100000);
  BSOD_TEXT (bst, LEFT,
             "XIO:  fatal IO error 2 (broken pipe) on X server \":0.0\"\n"
             "        after 339471 requests (339471 known processed) "
             "with 0 events remaining\n");
  BSOD_CHAR_DELAY (bst, 300000);
  BSOD_TEXT (bst, LEFT, ".........\n");
  BSOD_CHAR_DELAY (bst, 0);
  BSOD_TEXT (bst, LEFT,
             "xinit:  X server slow to shut down, sending KILL signal.\n"
             "waiting for server to die ");
  BSOD_CHAR_DELAY (bst, 300000);
  BSOD_TEXT (bst, LEFT, "...\n");
  BSOD_CHAR_DELAY (bst, 0);
  BSOD_TEXT (bst, LEFT, "xinit:  Can't kill server\n");
  BSOD_PAUSE (bst, 2000000);

  sprintf (buf, "\n%s Login: ", sysname);
  BSOD_TEXT (bst, LEFT, buf);
  BSOD_PAUSE (bst, 1000000);
  BSOD_TEXT (bst, LEFT,
    "\n\n"
    "Parallelizing fsck version 1.22 (22-Jun-2001)\n"
    "e2fsck 1.22, 22-Jun-2001 for EXT2 FS 0.5b, 95/08/09\n"
    "Warning!  /dev/hda1 is mounted.\n"
    "/dev/hda1 contains a file system with errors, check forced.\n");
  BSOD_PAUSE (bst, 1000000);

  if (0 == random() % 2)
    BSOD_TEXT (bst, LEFT,
     "Couldn't find ext2 superblock, trying backup blocks...\n"
     "The filesystem size (according to the superblock) is 3644739 blocks\n"
     "The physical size of the device is 3636706 blocks\n"
     "Either the superblock or the partition table is likely to be corrupt!\n"
     "Abort<y>? no\n");
  BSOD_PAUSE (bst, 1000000);

 AGAIN:

  BSOD_TEXT (bst, LEFT, "Pass 1: Checking inodes, blocks, and sizes\n");
  BSOD_PAUSE (bst, 2000000);

  i = (random() % 60) - 20;
  while (--i > 0)
    {
      int b = random() % 0xFFFF;
      sprintf (buf, "Deleted inode %d has zero dtime.  Fix<y>? yes\n\n", b);
      BSOD_TEXT (bst, LEFT, buf);
      BSOD_PAUSE (bst, 1000);
    }

  i = (random() % 40) - 10;
  if (i > 0)
    {
      int g = random() % 0xFFFF;
      int b = random() % 0xFFFFFFF;

      BSOD_PAUSE (bst, 1000000);

      sprintf (buf, "Warning: Group %d's copy of the group descriptors "
               "has a bad block (%d).\n", g, b);
      BSOD_TEXT (bst, LEFT, buf);

      b = random() % 0x3FFFFF;
      while (--i > 0)
        {
          b += random() % 0xFFFF;
          sprintf (buf,
                   "Error reading block %d (Attempt to read block "
                   "from filesystem resulted in short read) while doing "
                   "inode scan.  Ignore error<y>?",
                   b);
          BSOD_TEXT (bst, LEFT, buf);
          BSOD_PAUSE (bst, 10000);
          BSOD_TEXT (bst, LEFT, " yes\n\n");
        }
    }

  if (0 == (random() % 10))
    {
      BSOD_PAUSE (bst, 1000000);

      i = 3 + (random() % 10);
      while (--i > 0)
        {
          BSOD_TEXT (bst, LEFT,
                     "Could not allocate 256 block(s) for inode table: "
                     "No space left on device\n");
          BSOD_PAUSE (bst, 1000);
        }
      BSOD_TEXT (bst, LEFT, "Restarting e2fsck from the beginning...\n");
      BSOD_PAUSE (bst, 2000000);

      goto AGAIN;
    }

  i = (random() % 20) - 5;

  if (i > 0)
    BSOD_PAUSE (bst, 1000000);

  while (--i > 0)
    {
      int j = 5 + (random() % 10);
      int w = random() % 4;

      while (--j > 0)
        {
          int b = random() % 0xFFFFF;
          int g = random() % 0xFFF;

          if (0 == (random() % 10))
            b = 0;
          else if (0 == (random() % 10))
            b = -1;

          if (w == 0)
            sprintf (buf,
                     "Inode table for group %d not in group.  (block %d)\n"
                     "WARNING: SEVERE DATA LOSS POSSIBLE.\n"
                     "Relocate<y>?",
                     g, b);
          else if (w == 1)
            sprintf (buf,
                     "Block bitmap for group %d not in group.  (block %d)\n"
                     "Relocate<y>?",
                     g, b);
          else if (w == 2)
            sprintf (buf,
                     "Inode bitmap %d for group %d not in group.\n"
                     "Continue<y>?",
                     b, g);
          else /* if (w == 3) */
            sprintf (buf,
                     "Bad block %d in group %d's inode table.\n"
                     "WARNING: SEVERE DATA LOSS POSSIBLE.\n"
                     "Relocate<y>?",
                     b, g);

          BSOD_TEXT (bst, LEFT, buf);
          BSOD_TEXT (bst, LEFT, " yes\n\n");
          BSOD_PAUSE (bst, 1000);
        }
    }


  if (0 == random() % 10) goto PANIC;
  BSOD_TEXT (bst, LEFT, "Pass 2: Checking directory structure\n");
  BSOD_PAUSE (bst, 2000000);

  i = (random() % 20) - 5;
  while (--i > 0)
    {
      int n = random() % 0xFFFFF;
      int o = random() % 0xFFF;
      sprintf (buf, "Directory inode %d, block 0, offset %d: "
               "directory corrupted\n"
               "Salvage<y>? ",
               n, o);
      BSOD_TEXT (bst, LEFT, buf);
      BSOD_PAUSE (bst, 1000);
      BSOD_TEXT (bst, LEFT, " yes\n\n");

      if (0 == (random() % 100))
        {
          sprintf (buf, "Missing '.' in directory inode %d.\nFix<y>?", n);
          BSOD_TEXT (bst, LEFT, buf);
          BSOD_PAUSE (bst, 1000);
          BSOD_TEXT (bst, LEFT, " yes\n\n");
        }
    }

  if (0 == random() % 10)
    goto PANIC;

  BSOD_TEXT (bst, LEFT,
             "Pass 3: Checking directory connectivity\n"
             "/lost+found not found.  Create? yes\n");
  BSOD_PAUSE (bst, 2000000);

  /* Unconnected directory inode 4949 (/var/spool/squid/06/???)
     Connect to /lost+found<y>? yes

     '..' in /var/spool/squid/06/08 (20351) is <The NULL inode> (0), should be
     /var/spool/squid/06 (20350).
     Fix<y>? yes

     Unconnected directory inode 128337 (/var/spool/squid/06/???)
     Connect to /lost+found<y>? yes
   */


  if (0 == random() % 10) goto PANIC;
  BSOD_TEXT (bst, LEFT,  "Pass 4: Checking reference counts\n");
  BSOD_PAUSE (bst, 2000000);

  /* Inode 2 ref count is 19, should be 20.  Fix<y>? yes

     Inode 4949 ref count is 3, should be 2.  Fix<y>? yes

        ...

     Inode 128336 ref count is 3, should be 2.  Fix<y>? yes

     Inode 128337 ref count is 3, should be 2.  Fix<y>? yes

   */


  if (0 == random() % 10) goto PANIC;
  BSOD_TEXT (bst, LEFT,  "Pass 5: Checking group summary information\n");
  BSOD_PAUSE (bst, 2000000);

  i = (random() % 200) - 50;
  if (i > 0)
    {
      BSOD_TEXT (bst, LEFT,  "Block bitmap differences: ");
      while (--i > 0)
        {
          sprintf (buf, " %d", -(random() % 0xFFF));
          BSOD_TEXT (bst, LEFT, buf);
          BSOD_PAUSE (bst, 1000);
        }
      BSOD_TEXT (bst, LEFT, "\nFix? yes\n\n");
    }


  i = (random() % 100) - 50;
  if (i > 0)
    {
      BSOD_TEXT (bst, LEFT,  "Inode bitmap differences: ");
      while (--i > 0)
        {
          sprintf (buf, " %d", -(random() % 0xFFF));
          BSOD_TEXT (bst, LEFT, buf);
          BSOD_PAUSE (bst, 1000);
        }
      BSOD_TEXT (bst, LEFT,  "\nFix? yes\n\n");
    }

  i = (random() % 20) - 5;
  while (--i > 0)
    {
      int g = random() % 0xFFFF;
      int c = random() % 0xFFFF;
      sprintf (buf,
               "Free blocks count wrong for group #0 (%d, counted=%d).\nFix? ",
               g, c);
      BSOD_TEXT (bst, LEFT, buf);
      BSOD_PAUSE (bst, 1000);
      BSOD_TEXT (bst, LEFT,  " yes\n\n");
    }

 PANIC:

  i = 0;
  BSOD_TEXT (bst, LEFT,  "\n\n");
  while (linux_panic[i])
    {
      time_t t = time ((time_t *) 0);
      struct tm *tm = localtime (&t);
      char prefix[100];

      if (*linux_panic[i])
        {
          strftime (prefix, sizeof(prefix)-1, "%b %d %H:%M:%S ", tm);
          BSOD_TEXT (bst, LEFT,  prefix);
          BSOD_TEXT (bst, LEFT,  sysname);
          BSOD_TEXT (bst, LEFT,  linux_panic[i]);
          BSOD_PAUSE (bst, 1000);
        }
      else
        BSOD_PAUSE (bst, 300000);

      i++;
    }
  BSOD_PAUSE (bst, 4000000);

  XClearWindow(dpy, window);
  return bst;
}


/*
 * Linux (hppa) panic, by Stuart Brady <sdbrady@ntlworld.com>
 * Output courtesy of M. Grabert
 */
static struct bsod_state *
hppa_linux (Display *dpy, Window window)
{
  struct bsod_state *bst =
    make_bsod_state (dpy, window, "hppalinux", "HPPALinux");

  int i = 0;
  const char *release, *sysname, *gccversion, *version;
  long int linedelay = 0;
  char ss[1024];
# ifdef HAVE_UNAME
  struct utsname uts;
# endif /* UNAME */

  __extension__
  struct { long int delay; const char *string; } linux_panic[] =
    {{ 0, "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
          "\n\n\n\n\n\n\n\n\n\n\n\n\n" },
     { 0, "Linux version %s (root@%s) (gcc version %s) %s\n" },
     { 4000, "FP[0] enabled: Rev 1 Model 16\n" },
     { 10, "The 32-bit Kernel has started...\n" },
     { -1, "Determining PDC firmware type: System Map.\n" },
     { -1, "model 00005bb0 00000481 00000000 00000002 7778df9f 100000f0 "
           "00000008 000000b2 000000b2\n" },
     { -1, "vers  00000203\n" },
     { -1, "CPUID vers 17 rev 7 (0x00000227)\n" },
     { -1, "capabilities 0x3\n" },
     { -1, "model 9000/785/C3000\n" },
     { -1, "Total Memory: 1024 Mb\n" },
     { -1, "On node 0 totalpages: 262144\n" },
     { -1, "  DMA zone: 262144 pages, LIFO batch:16\n" },
     { -1, "  Normal zone: 0 pages, LIFO batch:1\n" },
     { -1, "  HighMem zone: 0 pages, LIFO batch:1\n" },
     { -1, "LCD display at f05d0008,f05d0000 registered\n" },
     { -1, "Building zonelist for node : 0\n" },
     { -1, "Kernel command line: ide=nodma root=/dev/sda3 HOME=/ ip=off "
           "console=ttyS0 TERM=vt102 palo_kernel=2/vmlinux-2.6\n" },
     { -1, "ide_setup: ide=nodmaIDE: Prevented DMA\n" },
     { -1, "PID hash table entries: 16 (order 4: 128 bytes)\n" },
     {500, "Console: colour dummy device 160x64\n" },
     { 10, "Memory: 1034036k available\n" },
     { -1, "Calibrating delay loop... 796.67 BogoMIPS\n" },
     { -1, "Dentry cache hash table entries: 131072 (order: 7, 524288 "
           "bytes)\n" },
     { -1, "Inode-cache hash table entries: 65536 (order: 6, 262144 "
           "bytes)\n" },
     { -1, "Mount-cache hash table entries: 512 (order: 0, 4096 bytes)\n" },
     { -1, "POSIX conformance testing by UNIFIX\n" },
     { -1, "NET: Registered protocol family 16\n" },
     { 100, "Searching for devices...\n" },
     { 25, "Found devices:\n" },
     { 10, "1. Astro BC Runway Port at 0xfed00000 [10] "
           "{ 12, 0x0, 0x582, 0x0000b }\n" },
     { -1, "2. Elroy PCI Bridge at 0xfed30000 [10/0] "
           "{ 13, 0x0, 0x782, 0x0000a }\n" },
     { -1, "3. Elroy PCI Bridge at 0xfed32000 [10/1] "
           "{ 13, 0x0, 0x782, 0x0000a }\n" },
     { -1, "4. Elroy PCI Bridge at 0xfed38000 [10/4] "
           "{ 13, 0x0, 0x782, 0x0000a }\n" },
     { -1, "5. Elroy PCI Bridge at 0xfed3c000 [10/6] "
           "{ 13, 0x0, 0x782, 0x0000a }\n" },
     { -1, "6. AllegroHigh W at 0xfffa0000 [32] "
           "{ 0, 0x0, 0x5bb, 0x00004 }\n" },
     { -1, "7. Memory at 0xfed10200 [49] { 1, 0x0, 0x086, 0x00009 }\n" },
     { -1, "CPU(s): 1 x PA8500 (PCX-W) at 400.000000 MHz\n" },
     { -1, "SBA found Astro 2.1 at 0xfed00000\n" },
     { -1, "lba version TR2.1 (0x2) found at 0xfed30000\n" },
     { -1, "lba version TR2.1 (0x2) found at 0xfed32000\n" },
     { -1, "lba version TR2.1 (0x2) found at 0xfed38000\n" },
     { -1, "lba version TR2.1 (0x2) found at 0xfed3c000\n" },
     { 100, "SCSI subsystem initialized\n" },
     { 10, "drivers/usb/core/usb.c: registered new driver usbfs\n" },
     { -1, "drivers/usb/core/usb.c: registered new driver hub\n" },
     { -1, "ikconfig 0.7 with /proc/config*\n" },
     { -1, "Initializing Cryptographic API\n" },
     { 250, "SuperIO: probe of 0000:00:0e.0 failed with error -1\n" },
     { 20, "SuperIO: Found NS87560 Legacy I/O device at 0000:00:0e.1 "
           "(IRQ 64)\n" },
     { -1, "SuperIO: Serial port 1 at 0x3f8\n" },
     { -1, "SuperIO: Serial port 2 at 0x2f8\n" },
     { -1, "SuperIO: Parallel port at 0x378\n" },
     { -1, "SuperIO: Floppy controller at 0x3f0\n" },
     { -1, "SuperIO: ACPI at 0x7e0\n" },
     { -1, "SuperIO: USB regulator enabled\n" },
     { -1, "SuperIO: probe of 0000:00:0e.2 failed with error -1\n" },
     { -1, "Soft power switch enabled, polling @ 0xf0400804.\n" },
     { -1, "pty: 256 Unix98 ptys configured\n" },
     { -1, "Generic RTC Driver v1.07\n" },
     { -1, "Serial: 8250/16550 driver $" "Revision: 1.100 $ 13 ports, "
           "IRQ sharing disabled\n" },
     { -1, "ttyS0 at I/O 0x3f8 (irq = 0) is a 16550A\n" },
     { -1, "ttyS1 at I/O 0x2f8 (irq = 0) is a 16550A\n" },
     { -1, "Linux Tulip driver version 1.1.13 (May 11, 2002)\n" },
     { 150, "tulip0: no phy info, aborting mtable build\n" },
     { 10, "tulip0:  MII transceiver #1 config 1000 status 782d "
           "advertising 01e1.\n" },
     { -1, "eth0: Digital DS21143 Tulip rev 65 at 0xf4008000, "
           "00:10:83:F9:B4:34, IRQ 66.\n" },
     { -1, "Uniform Multi-Platform E-IDE driver Revision: 7.00alpha2\n" },
     { -1, "ide: Assuming 33MHz system bus speed for PIO modes; "
           "override with idebus=xx\n" },
     { 100, "SiI680: IDE controller at PCI slot 0000:01:06.0\n" },
     { 10, "SiI680: chipset revision 2\n" },
     { -1, "SiI680: BASE CLOCK == 133\n" },
     { -1, "SiI680: 100% native mode on irq 128\n" },
     { -1, "    ide0: MMIO-DMA at 0xf4800000-0xf4800007 -- "
           "Error, MMIO ports already in use.\n" },
     { -1, "    ide1: MMIO-DMA at 0xf4800008-0xf480000f -- "
           "Error, MMIO ports already in use.\n" },
     { 5, "hda: TS130220A2, ATA DISK drive\n" },
     { -1, "      _______________________________\n" },
     { -1, "     < Your System ate a SPARC! Gah! >\n" },
     { -1, "      -------------------------------\n" },
     { -1, "             \\   ^__^\n" },
     { -1, "              \\  (xx)\\_______\n" },
     { -1, "                 (__)\\       )\\/\\\n" },
     { -1, "                  U  ||----w |\n" },
     { -1, "                     ||     ||\n" },
     { -1, "swapper (pid 1): Breakpoint (code 0)\n" },
     { -1, "\n" },
     { -1, "     YZrvWESTHLNXBCVMcbcbcbcbOGFRQPDI\n" },
     { -1, "PSW: 00000000000001001111111100001111 Not tainted\n" },
     { -1, "r00-03  4d6f6f21 1032f010 10208f34 103fc2e0\n" },
     { -1, "r04-07  103fc230 00000001 00000001 0000000f\n" },
     { -1, "r08-11  103454f8 000f41fa 372d3980 103ee404\n" },
     { -1, "r12-15  3ccbf700 10344810 103ee010 f0400004\n" },
     { -1, "r16-19  f00008c4 f000017c f0000174 00000000\n" },
     { -1, "r20-23  fed32840 fed32800 00000000 0000000a\n" },
     { -1, "r24-27  0000ffa0 000000ff 103fc2e0 10326010\n" },
     { -1, "r28-31  00000000 00061a80 4ff98340 10208f34\n" },
     { -1, "sr0-3   00000000 00000000 00000000 00000000\n" },
     { -1, "sr4-7   00000000 00000000 00000000 00000000\n" },
     { -1, "\n" },
     { -1, "IASQ: 00000000 00000000 IAOQ: 00000000 00000004\n" },
     { -1, " IIR: 00000000    ISR: 00000000  IOR: 00000000\n" },
     { -1, " CPU:        0   CR30: 4ff98000 CR31: 1037c000\n" },
     { -1, " ORIG_R28: 55555555\n" },
     { -1, " IAOQ[0]: 0x0\n" },
     { -1, " IAOQ[1]: 0x4\n" },
     { -1, " RP(r2): probe_hwif+0x218/0x44c\n" },
     { -1, "Kernel panic: Attempted to kill init!\n" },
     { 0, NULL }};

  bst->scroll_p = True;
  bst->wrap_p = True;
  bst->left_margin = bst->right_margin = 10;
  bst->top_margin = bst->bottom_margin = 10;

  release = "2.6.0-test11-pa2";
  sysname = "hppa";
  version = "#2 Mon Dec 8 06:09:27 GMT 2003";
# ifdef HAVE_UNAME
  {
    char *s;
    if (uname (&uts) >= 0)
      {
	sysname = uts.nodename;
	if (!strcmp (uts.sysname, "Linux"))
	  {
	    release = uts.release;
	    version = uts.version;
	  }
      }
    s = strchr (sysname, '.');
    if (s) *s = 0;
  }
# endif	/* !HAVE_UNAME */

# if (defined (__GNUC__) && defined (__VERSION__))
  gccversion = __VERSION__;
# else /* !(defined (__GNUC__) && defined (__VERSION__)) */
  gccversion = "3.3.2 (Debian)";
# endif /* !(defined (__GNUC__) && defined (__VERSION__)) */

  /* Insert current host name into banner on line 2 */
  {
    snprintf (ss, 1024, linux_panic[1].string,
	      release, sysname, gccversion, version);
    linux_panic[1].string = ss;
  }

  BSOD_PAUSE (bst, 100000);
  while (linux_panic[i].string)
    {
      if (linux_panic[i].delay != -1)
        linedelay = linux_panic[i].delay * 1000;
      BSOD_PAUSE (bst, linedelay);
      BSOD_TEXT (bst, LEFT, linux_panic[i].string);
      i++;
    }

  bst->y = bst->xgwa.height - bst->yoff
    - bst->font->ascent - bst->font->descent;

  XClearWindow(dpy, window);
  return bst;
}


/* VMS by jwz (text sent by Roland Barmettler <roli@barmettler.net>)
 */
static struct bsod_state *
vms (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "vms", "VMS");

  const char *sysname;
  int char_delay = 0;
  int dot_delay = 40000;
  int chunk_delay = 500000;
  char *s, *s1;
  int i;
  int arg_count;
# ifdef HAVE_UNAME
  struct utsname uts;
# endif /* UNAME */

  __extension__

  const char * const lines[] = {
    "%CNXMAN,  Lost connection to system #\n"
    "%SHADOW-I-VOLPROC, DSA0: shadow master has changed.  "
    "Dump file WILL be written if system crashes.\n"
    "\n",
    "",

    "%CNXMAN,  Quorum lost, blocking activity\n"
    "%CNXMAN,  Timed-out lost connection to system #\n"
    "%CNXMAN,  Timed-out lost connection to system #\n"
    "%CNXMAN,  Timed-out lost connection to system #\n"
    "%CNXMAN,  Proposing reconfiguration of the VMScluster\n",
    "",

    "%CNXMAN,  Removed from VMScluster system #\n"
    "%CNXMAN,  Removed from VMScluster system #\n"
    "%CNXMAN,  Removed from VMScluster system #\n"
    "%CNXMAN,  Completing VMScluster state transition\n",

    "\n"
    "**** OpenVMS (TM) Alpha Operating system V7.3-1   - BUGCHECK ****\n"
    "\n"
    "** Bugcheck code = 000005DC: CLUEXIT, Node voluntarily exiting "
    "VMScluster\n"
    "** Crash CPU: 00    Primary CPU: 00    Active CPUs: 00000001\n"
    "** Current Process = NULL\n"
    "** Current PSB ID = 00000001\n"
    "** Image Name =\n"
    "\n"
    "** Dumping error log buffers to HBVS unit 0\n"
    "**** Unable to dump error log buffers to remaining shadow set members\n"
    "** Error log buffers not dumped to HBVS unit 200\n"
    "\n"
    "** Dumping memory to HBVS unit 0\n"
    "**** Starting compressed selective memory dump at #...\n",

    "...",

    "\n"
    "**** Memory dump complete - not all processes or global pages saved\n",

    "\n"
    "halted CPU 0\n",
    "",

    "\n"
    "halt code = 5\n"
    "HALT instruction executed\n"
    "PC = ffffffff800c3884\n",

    "\n"
    "CPU 0 booting\n",

    "\n"
    "resetting all I/O buses\n"
    "\n"
    "\n"
    };
  char *args[8];
  int ids[3];

  bst->scroll_p = True;
  bst->wrap_p = True;
  bst->left_margin = bst->right_margin = 10;
  bst->top_margin = bst->bottom_margin = 10;

  sysname = "VMS001";
# ifdef HAVE_UNAME
  {
    if (uname (&uts) >= 0)
      sysname = uts.nodename;
    s = strchr (sysname, '.');
    if (s) *s = 0;
  }
# endif	/* !HAVE_UNAME */

  args[0] = malloc (strlen(sysname) + 7);
  strcpy (args[0], sysname);
  args[0][5] = 0;

  /* Pick three numbers, 1-9, no overlaps. */
  ids[0] = 1 + (random() % 9);
  do { ids[1] = 1 + (random() % 9); } while (ids[1]==ids[0]);
  do { ids[2] = 1 + (random() % 9); } while (ids[2]==ids[0] || ids[2]==ids[1]);

  i = strlen(args[0])-1;
  if (i < 6) i++;
  args[0][i] = '0' + ids[0];
  args[0][i+1] = 0;

  for (s = args[0]; *s; s++)
    if (isalpha(*s)) *s = toupper (*s);

  args[1] = strdup (args[0]);
  args[2] = strdup (args[0]); args[2][i] = '0' + ids[1];
  args[3] = strdup (args[0]); args[3][i] = '0' + ids[2];

  args[4] = strdup (args[1]);
  args[5] = strdup (args[2]);
  args[6] = strdup (args[3]);

  {
    time_t t = time ((time_t *) 0);
    struct tm *tm = localtime (&t);
    args[7] = malloc (30);
    strftime (args[7], 29, "%d-%b-%Y %H:%M", tm);
    for (s = args[7]; *s; s++)
      if (isalpha(*s)) *s = toupper (*s);
  }

  arg_count = 0;
  for (i = 0; i < countof(lines); i++)
    {
      const char *fmt = lines[i];
      if (! strcmp (fmt, "..."))
        {
          int steps = 180 + (random() % 60);
          while (--steps >= 0)
            {
              BSOD_TEXT (bst, LEFT, ".");
              BSOD_PAUSE (bst, dot_delay);
            }
        }
      else
        {
          char *fmt2 = malloc (strlen (fmt) * 2 + 1);
          for (s = (char *) fmt, s1 = fmt2; *s; s++)
            {
              if (*s == '#')
                {
                  strcpy (s1, args[arg_count++]);
                  s1 += strlen(s1);
                }
              else
                *s1++ = *s;
            }
          *s1 = 0;
          BSOD_CHAR_DELAY (bst, char_delay);
          BSOD_TEXT (bst, LEFT, fmt2);
          free (fmt2);
          BSOD_CHAR_DELAY (bst, 0);
          BSOD_PAUSE (bst, chunk_delay);
        }
    }

  for (i = 0; i < countof (args); i++)
    free (args[i]);

  XClearWindow(dpy, window);
  return bst;
}


/* HVX (formerly GCOS6) and TPS6 crash
   by Brian Garratt <brian-m.garratt@bull.co.uk>

   GCOS6 is a Unix-like operating system developed by Honeywell in the
   1970s in collaboration with MIT and AT&T (who called their version
   UNIX).  Both are very much like MULTICS which Honeywell got from GE.

   HVX ("High-performance Virtual System on Unix") is an AIX application
   which emulates GCOS6 hardware on RS6000-like machines.
 */
static struct bsod_state *
hvx (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "hvx", "HVX");

  bst->scroll_p = True;
  bst->wrap_p = True;
  bst->y = bst->xgwa.height - bst->bottom_margin - bst->yoff
    - bst->font->ascent;

  BSOD_CHAR_DELAY (bst, 10000);
  BSOD_TEXT (bst, LEFT,
     "(TP) Trap no E   Effective address 00000000   Instruction D7DE\n"
     "(TP)  Registers :\n"
     "(TP)  B1 -> B7  03801B02  00000000  03880D45  038BABDB  0388AFFD"
     "  0389B3F8  03972317\n"
     "(TP)  R1 -> R7  0001  0007  F10F  090F  0020  0106  0272\n"
     "(TP)  P I Z M1  0388A18B  3232  0000 FF00\n"
     "(TP) Program counter is at offset 0028 from string YTPAD\n"
     "(TP) User id of task which trapped is LT 626\n"
     "(TP)?\n"
     );
  BSOD_PAUSE (bst, 1000000);

  BSOD_CHAR_DELAY (bst, 100000);
  BSOD_TEXT (bst, LEFT, " TP CLOSE ALL");

  BSOD_CHAR_DELAY (bst, 10000);
  BSOD_TEXT (bst, LEFT, "\n(TP)?\n");
  BSOD_PAUSE (bst, 1000000);

  BSOD_CHAR_DELAY (bst, 100000);
  BSOD_TEXT (bst, LEFT, " TP ABORT -LT ALL");

  BSOD_CHAR_DELAY (bst, 10000);
  BSOD_TEXT (bst, LEFT, "\n(TP)?\n");
  BSOD_PAUSE (bst, 1000000);

  BSOD_CHAR_DELAY (bst, 100000);
  BSOD_TEXT (bst, LEFT, "  TP STOP KILL");

  BSOD_CHAR_DELAY (bst, 10000);
  BSOD_TEXT (bst, LEFT,
     "\n"
     "(TP)?\n"
     "Core dumps initiated for selected HVX processes ...\n"
     "Core dumps complete.\n"
     "Fri Jul 19 15:53:09 2002\n"
     "Live registers for cp 0:\n"
     " P    =     7de3  IW=0000     I=32    CI=30000000   S=80006013"
     "   IV=aa0      Level=13\n"
     " R1-7 =       1f      913       13        4        8        0        0\n"
     " B1-7 =   64e71b      a93      50e   64e73c     6c2c     7000      b54\n"
     "Memory dump starting to file /var/hvx/dp01/diag/Level2 ...\n"
     "Memory dump complete.\n"
    );

  XClearWindow(dpy, window);
  return bst;
}


/* HPUX panic, by Tobias Klausmann <klausman@schwarzvogel.de>
 */
static struct bsod_state *
hpux (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "hpux", "HPUX");
  const char *sysname;
  char buf[2048];
# ifdef HAVE_UNAME
  struct utsname uts;
# endif /* UNAME */

  bst->scroll_p = True;
  bst->y = bst->xgwa.height - bst->bottom_margin - bst->yoff
    - bst->font->ascent;

  sysname = "HPUX";
# ifdef HAVE_UNAME
  {
    char *s;
    if (uname (&uts) >= 0)
      sysname = uts.nodename;
    s = strchr (sysname, '.');
    if (s) *s = 0;
  }
# endif	/* !HAVE_UNAME */

  BSOD_TEXT (bst, LEFT,
             "                                                       "
             "                                                       "
             "                                                       \n");
  sprintf (buf, "%.100s [HP Release B.11.00] (see /etc/issue)\n", sysname);
  BSOD_TEXT (bst, LEFT, buf);
  BSOD_PAUSE (bst, 1000000);
  BSOD_TEXT (bst, LEFT,
   "Console Login:\n"
   "\n"
   "     ******* Unexpected HPMC/TOC. Processor HPA FFFFFFFF'"
   "FFFA0000 *******\n"
   "                              GENERAL REGISTERS:\n"
   "r00/03 00000000'00000000 00000000'00000000 00000000'00000000 00000000'"
   "006C76C0\n"
   "r04/07 00000000'00000001 00000000'0126E328 00000000'00000000 00000000'"
   "0122B640\n"
   "r08/11 00000000'00000000 00000000'0198CFC0 00000000'000476FE 00000000'"
   "00000001\n"
   "r12/15 00000000'40013EE8 00000000'08000080 00000000'4002530C 00000000'"
   "4002530C\n"
   "r16/19 00000000'7F7F2A00 00000000'00000001 00000000'00000000 00000000'"
   "00000000\n"
   "r20/23 00000000'006C8048 00000000'00000001 00000000'00000000 00000000'"
   "00000000\n"
   "r24/27 00000000'00000000 00000000'00000000 00000000'00000000 00000000'"
   "00744378\n"
   "r28/31 00000000'00000000 00000000'007DD628 00000000'0199F2B0 00000000'"
   "00000000\n"
   "                              CONTROL REGISTERS:\n"
   "sr0/3  00000000'0F3B4000 00000000'0C2A2000 00000000'016FF800 00000000'"
   "00000000\n"
   "sr4/7  00000000'00000000 00000000'016FF800 00000000'0DBF1400 00000000'"
   "00000000\n"
   "pcq =  00000000'00000000.00000000'00104950 00000000'00000000.00000000'"
   "00104A14\n"
   "isr =  00000000'10240006 ior = 00000000'67D9E220 iir = 08000240 rctr = "
   "7FF10BB6\n"
   "\n"
   "pid reg cr8/cr9    00007700'0000B3A9 00000000'0000C5D8\n"
   "pid reg cr12/cr13  00000000'00000000 00000000'00000000\n"
   "ipsw = 000000FF'080CFF1F iva = 00000000'0002C000 sar = 3A ccr = C0\n"
   "tr0/3  00000000'006C76C0 00000000'00000001 00000000'00000000 00000000'"
   "7F7CE000\n"
   "tr4/7  00000000'03790000 0000000C'4FB68340 00000000'C07EE13F 00000000'"
   "0199F2B0\n"
   "eiem = FFFFFFF0'FFFFFFFF eirr = 80000000'00000000 itmr = 0000000C'"
   "4FD8EDE1\n"
   "cr1/4  00000000'00000000 00000000'00000000 00000000'00000000 00000000'"
   "00000000\n"
   "cr5/7  00000000'00000000 00000000'00000000 00000000'"
   "00000000\n"
   "                           MACHINE CHECK PARAMETERS:\n"
   "Check Type = 00000000 CPU STATE = 9E000001 Cache Check = 00000000\n"
   "TLB Check = 00000000 Bus Check = 00000000 PIM State = ? SIU "
   "Status = ????????\n"
   "Assists = 00000000 Processor = 00000000\n"
   "Slave Addr = 00000000'00000000 Master Addr = 00000000'00000000\n"
   "\n"
   "\n"
   "TOC,    pcsq.pcoq = 0'0.0'104950   , isr.ior = 0'10240006.0'67d9e220\n"
   "@(#)B2352B/9245XB HP-UX (B.11.00) #1: Wed Nov  5 22:38:19 PST 1997\n"
   "Transfer of control: (display==0xd904, flags==0x0)\n"
   "\n"
   "\n"
   "\n"
   "*** A system crash has occurred.  (See the above messages for details.)\n"
   "*** The system is now preparing to dump physical memory to disk, for use\n"
   "*** in debugging the crash.\n"
   "\n"
   "*** The dump will be a SELECTIVE dump:  40 of 256 megabytes.\n"
   "*** To change this dump type, press any key within 10 seconds.\n"
   "*** Proceeding with selective dump.\n"
   "\n"
   "*** The dump may be aborted at any time by pressing ESC.\n");

  {
    int i;
    int steps = 11;
    int size = 40;
    for (i = 0; i <= steps; i++)
      {
        sprintf (buf,
               "*** Dumping: %3d%% complete (%d of 40 MB) (device 64:0x2)\r",
                 i * 100 / steps,
                 i * size / steps);
        BSOD_TEXT (bst, LEFT, buf);
        BSOD_PAUSE (bst, 1500000);
      }
  }

  BSOD_TEXT (bst, LEFT, "\n*** System rebooting.\n");

  XClearWindow(dpy, window);
  return bst;
}


/* IBM OS/390 aka MVS aka z/OS.
   Text from Dan Espen <dane@mk.telcordia.com>.
   Apparently this isn't actually a crash, just a random session...
   But who can tell.
 */
static struct bsod_state *
os390 (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "os390", "OS390");

  bst->scroll_p = True;
  bst->y = bst->xgwa.height - bst->bottom_margin - bst->yoff
    - bst->font->ascent;

  BSOD_LINE_DELAY (bst, 100000);
  BSOD_TEXT (bst, LEFT,
   "\n*** System rebooting.\n"
   "* ISPF Subtask abend *\n"
   "SPF      ENDED DUE TO ERROR+\n"
   "READY\n"
   "\n"
   "IEA995I SYMPTOM DUMP OUTPUT\n"
   "  USER COMPLETION CODE=0222\n"
   " TIME=23.00.51  SEQ=03210  CPU=0000  ASID=00AE\n"
   " PSW AT TIME OF ERROR  078D1000   859DAF18  ILC 2  INTC 0D\n"
   "   NO ACTIVE MODULE FOUND\n"
   "   NAME=UNKNOWN\n"
   "   DATA AT PSW  059DAF12 - 00181610  0A0D9180  70644710\n"
   "   AR/GR 0: 00000000/80000000   1: 00000000/800000DE\n"
   "         2: 00000000/196504DC   3: 00000000/00037A78\n"
   "         4: 00000000/00037B78   5: 00000000/0003351C\n"
   "         6: 00000000/0000F0AD   7: 00000000/00012000\n"
   "         8: 00000000/059DAF10   9: 00000000/0002D098\n"
   "         A: 00000000/059D9F10   B: 00000000/059D8F10\n"
   "         C: 00000000/859D7F10   D: 00000000/00032D60\n"
   "         E: 00000000/00033005   F: 01000002/00000041\n"
   " END OF SYMPTOM DUMP\n"
   "ISPS014 - ** Logical screen request failed - abend 0000DE **\n"
   "ISPS015 - ** Contact your system programmer or dialog developer.**\n"
   "*** ISPF Main task abend ***\n"
   "IEA995I SYMPTOM DUMP OUTPUT\n"
   "  USER COMPLETION CODE=0222\n"
   " TIME=23.00.52  SEQ=03211  CPU=0000  ASID=00AE\n"
   " PSW AT TIME OF ERROR  078D1000   8585713C  ILC 2  INTC 0D\n"
   "   ACTIVE LOAD MODULE           ADDRESS=05855000  OFFSET=0000213C\n"
   "   NAME=ISPMAIN\n"
   "   DATA AT PSW  05857136 - 00181610  0A0D9180  D3304770\n"
   "   GR 0: 80000000   1: 800000DE\n"
   "      2: 00015260   3: 00000038\n"
   "      4: 00012508   5: 00000000\n"
   "      6: 000173AC   7: FFFFFFF8\n"
   "      8: 05858000   9: 00012CA0\n"
   "      A: 05857000   B: 05856000\n"
   "      C: 85855000   D: 00017020\n"
   "      E: 85857104   F: 00000000\n"
   " END OF SYMPTOM DUMP\n"
   "READY\n"
   "***");
  BSOD_CURSOR (bst, CURSOR_LINE, 240000, 999999);

  XClearWindow(dpy, window);
  return bst;
}


/* Compaq Tru64 Unix panic, by jwz as described by
   Tobias Klausmann <klausman@schwarzvogel.de>
 */
static struct bsod_state *
tru64 (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "tru64", "Tru64");
  const char *sysname;
  char buf[2048];
# ifdef HAVE_UNAME
  struct utsname uts;
#endif /* UNAME */

  bst->scroll_p = True;
  bst->y = bst->xgwa.height - bst->bottom_margin - bst->yoff
    - bst->font->ascent;

  sysname = "127.0.0.1";
# ifdef HAVE_UNAME
  {
    if (uname (&uts) >= 0)
      sysname = uts.nodename;
  }
# endif	/* !HAVE_UNAME */

  sprintf (buf,
           "Compaq Tru64 UNIX V5.1B (Rev. 2650) (%.100s) console\n"
           "\n"
           "login: ",
           sysname);
  BSOD_TEXT (bst, LEFT, buf);
  BSOD_PAUSE (bst, 6000000);

  BSOD_TEXT (bst, LEFT,
    "panic (cpu 0): trap: illegal instruction\n"
    "kernel inst fault=gentrap, ps=0x5, pc=0xfffffc0000593878, inst=0xaa\n"
    "kernel inst fault=gentrap, ps=0x5, pc=0xfffffc0000593878, inst=0xaa\n"
    "                                                                   \n"
    "DUMP: blocks available:  1571600\n"
    "DUMP: blocks wanted:      100802 (partial compressed dump) [OKAY]\n"
    "DUMP: Device     Disk Blocks Available\n"
    "DUMP: ------     ---------------------\n"
    "DUMP: 0x1300023  1182795 - 1571597 (of 1571598) [primary swap]\n"
    "DUMP.prom: Open: dev 0x5100041, block 2102016: SCSI 0 11 0 2 200 0 0\n"
    "DUMP: Writing header... [1024 bytes at dev 0x1300023, block 1571598]\n"
    "DUMP: Writing data");

  {
    int i;
    int steps = 4 + (random() % 8);
    BSOD_CHAR_DELAY (bst, 1000000);
    for (i = 0; i < steps; i++)
      BSOD_TEXT (bst, LEFT, ".");
    BSOD_CHAR_DELAY (bst, 0);
    sprintf (buf, "[%dMB]\n", steps);
    BSOD_TEXT (bst, LEFT, buf);
  }

  BSOD_TEXT (bst, LEFT,
    "DUMP: Writing header... [1024 bytes at dev 0x1300023, block 1571598]\n"
    "DUMP: crash dump complete.\n"
    "kernel inst fault=gentrap, ps=0x5, pc=0xfffffc0000593878, inst=0xaa\n"
    "                                                                   \n"
    "DUMP: second crash dump skipped: 'dump_savecnt' enforced.\n");
  BSOD_PAUSE (bst, 4000000);

  BSOD_TEXT (bst, LEFT,
    "\n"
    "halted CPU 0\n"
    "\n"
    "halt code = 5\n"
    "HALT instruction executed\n"
    "PC = fffffc00005863b0\n");
  BSOD_PAUSE (bst, 3000000);

  BSOD_TEXT (bst, LEFT,
    "\n"
    "CPU 0 booting\n"
    "\n"
    "\n"
    "\n");

  XClearWindow(dpy, window);
  return bst;
}


/* MS-DOS, by jwz
 */
static struct bsod_state *
msdos (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "msdos", "MSDOS");

  BSOD_CHAR_DELAY (bst, 10000);
  BSOD_TEXT (bst, LEFT, "C:\\WINDOWS>");
  BSOD_CURSOR (bst, CURSOR_LINE, 200000, 8);

  BSOD_CHAR_DELAY (bst, 200000);
  BSOD_TEXT (bst, LEFT, "dir a:");
  BSOD_PAUSE (bst, 1000000);

  BSOD_CHAR_DELAY (bst, 10000);
  BSOD_TEXT (bst, LEFT, "\nNot ready reading drive A\nAbort, Retry, Fail?");

  BSOD_CURSOR (bst, CURSOR_LINE, 200000, 10);
  BSOD_CHAR_DELAY (bst, 200000);
  BSOD_TEXT (bst, LEFT, "f");
  BSOD_PAUSE (bst, 1000000);

  BSOD_CHAR_DELAY (bst, 10000);
  BSOD_TEXT (bst, LEFT,
             "\n\n\nNot ready reading drive A\nAbort, Retry, Fail?");

  BSOD_CURSOR (bst, CURSOR_LINE, 200000, 10);
  BSOD_CHAR_DELAY (bst, 200000);
  BSOD_TEXT (bst, LEFT, "f");
  BSOD_PAUSE (bst, 1000000);

  BSOD_CHAR_DELAY (bst, 10000);
  BSOD_TEXT (bst, LEFT, "\nVolume in drive A has no label\n\n"
                  "Not ready reading drive A\nAbort, Retry, Fail?");

  BSOD_CURSOR (bst, CURSOR_LINE, 200000, 12);
  BSOD_CHAR_DELAY (bst, 200000);
  BSOD_TEXT (bst, LEFT, "a");
  BSOD_PAUSE (bst, 1000000);

  BSOD_CHAR_DELAY (bst, 10000);
  BSOD_TEXT (bst, LEFT, "\n\nC:\\WINDOWS>");

  BSOD_CURSOR (bst, CURSOR_LINE, 200000, 999999);

  XClearWindow(dpy, window);
  return bst;
}


/* nvidia, by jwz.
 *
 * This is what happens if an Nvidia card goes into some crazy text mode.
 * Most often seen on the second screen of a dual-head system when the
 * proper driver isn't loaded.
 */
typedef struct { int fg; int bg; int bit; Bool blink; } nvcell;

static void
nvspatter (nvcell *grid, int rows, int cols, int ncolors, int nbits,
           Bool fill_p)
{
  int max = rows * cols;
  int from = fill_p ?   0 : random() % (max - 1);
  int len  = fill_p ? max : random() % (cols * 4);
  int to = from + len;
  int i;
  Bool noisy = ((random() % 4) == 0);
  Bool diag = (noisy || fill_p) ? 0 : ((random() % 4) == 0);

  int fg = random() % ncolors;
  int bg = random() % ncolors;
  int blink = ((random() % 4) == 0);
  int bit = (random() % nbits);

  if (to > max) to = max;

  if (diag)
    {
      int src = random() % (rows * cols);
      int len2 = (cols / 2) - (random() % 5);
      int j = src;
      for (i = from; i < to; i++, j++)
        {
          if (j > src + len2 || j >= max)
            j = src;
          if (i >= max) abort();
          if (j >= max) abort();
          grid[j] = grid[i];
        }
    }
  else
    for (i = from; i < to; i++)
      {
        nvcell *cell = &grid[i];
        cell->fg = fg;
        cell->bg = bg;
        cell->bit = bit;
        cell->blink = blink;

        if (noisy)
          {
            fg = random() % ncolors;
            bg = random() % ncolors;
            blink = ((random() % 8) == 0);
          }
      }
}

typedef struct {
  struct bsod_state *bst;
  GC gc1;
  Pixmap bits[5];
  int rows, cols;
  int cellw, cellh;
  nvcell *grid;
  int ncolors;
  unsigned long colors[256];
  int tick;
} nvstate;


static void
nvidia_free (struct bsod_state *bst)
{
  nvstate *nvs = (nvstate *) bst->closure;
  int i;
  XFreeColors (bst->dpy, bst->xgwa.colormap, nvs->colors, nvs->ncolors, 0);
  for (i = 0; i < countof(nvs->bits); i++)
    XFreePixmap (bst->dpy, nvs->bits[i]);
  XFreeGC (bst->dpy, nvs->gc1);
  free (nvs->grid);
  free (nvs);
}

static int
nvidia_draw (struct bsod_state *bst)
{
  nvstate *nvs = (nvstate *) bst->closure;
  int x, y;

  for (y = 0; y < nvs->rows; y++)
    for (x = 0; x < nvs->cols; x++)
      {
        nvcell *cell = &nvs->grid[y * nvs->cols + x];
        unsigned long fg = nvs->colors[cell->fg];
        unsigned long bg = nvs->colors[cell->bg];
        Bool flip = cell->blink && (nvs->tick & 1);
        XSetForeground (bst->dpy, bst->gc, flip ? fg : bg);
        XSetBackground (bst->dpy, bst->gc, flip ? bg : fg);
        XCopyPlane (bst->dpy, nvs->bits[cell->bit], bst->window, bst->gc,
                    0, 0, nvs->cellw, nvs->cellh,
                    x * nvs->cellw, y * nvs->cellh, 1L);
      }

  nvs->tick++;
  if ((random() % 5) == 0)    /* change the display */
    nvspatter (nvs->grid, nvs->rows, nvs->cols, nvs->ncolors,
               countof(nvs->bits), False);

  return 250000;
}


static struct bsod_state *
nvidia (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "nvidia", "nVidia");
  nvstate *nvs = (nvstate *) calloc (1, sizeof (*nvs));

  XGCValues gcv;
  int i;

  nvs->bst = bst;
  bst->closure = nvs;
  bst->draw_cb = nvidia_draw;
  bst->free_cb = nvidia_free;

  nvs->cols = 80;
  nvs->rows = 25;
  nvs->cellw = bst->xgwa.width  / nvs->cols;
  nvs->cellh = bst->xgwa.height / nvs->rows;
  if (nvs->cellw < 8 || nvs->cellh < 18)
    nvs->cellw = 8, nvs->cellh = 18;
  nvs->cols = (bst->xgwa.width  / nvs->cellw) + 1;
  nvs->rows = (bst->xgwa.height / nvs->cellh) + 1;

  nvs->grid = (nvcell *) calloc (sizeof(*nvs->grid), nvs->rows * nvs->cols);

  /* Allocate colors
   */
  nvs->ncolors = 16;
  for (i = 0; i < nvs->ncolors; i++)
    {
      XColor c;
      c.red   = random() & 0xFFFF;
      c.green = random() & 0xFFFF;
      c.blue  = random() & 0xFFFF;
      c.flags = DoRed|DoGreen|DoBlue;
      XAllocColor (dpy, bst->xgwa.colormap, &c);
      nvs->colors[i] = c.pixel;
    }

  /* Construct corrupted character bitmaps
   */
  for (i = 0; i < countof(nvs->bits); i++)
    {
      int j;

      nvs->bits[i] = XCreatePixmap (dpy, window, nvs->cellw, nvs->cellh, 1);
      if (!nvs->gc1) nvs->gc1 = XCreateGC (dpy, nvs->bits[i], 0, &gcv);

      XSetForeground (dpy, nvs->gc1, 0);
      XFillRectangle (dpy, nvs->bits[i], nvs->gc1, 0, 0,
                      nvs->cellw, nvs->cellh);
      XSetForeground (dpy, nvs->gc1, 1);

      if ((random() % 40) != 0)
        for (j = 0; j < ((nvs->cellw * nvs->cellh) / 16); j++)
          XFillRectangle (dpy, nvs->bits[i], nvs->gc1,
                          (random() % (nvs->cellw-2)) & ~1,
                          (random() % (nvs->cellh-2)) & ~1,
                          2, 2);
    }

  /* Randomize the grid
   */
  nvspatter (nvs->grid, nvs->rows, nvs->cols, nvs->ncolors,
             countof(nvs->bits), True);
  for (i = 0; i < 20; i++)
    nvspatter (nvs->grid, nvs->rows, nvs->cols, nvs->ncolors,
               countof(nvs->bits), False);

  return bst;
}


/*
 * Simulate various Apple ][ crashes. The memory map encouraged many programs
 * to use the primary hi-res video page for various storage, and the secondary
 * hi-res page for active display. When it crashed into Applesoft or the
 * monitor, it would revert to the primary page and you'd see memory garbage on
 * the screen. Also, it was common for copy-protected games to use the primary
 * text page for important code, because that made it really hard to
 * reverse-engineer them. The result often looked like what this generates.
 *
 * The Apple ][ logic and video hardware is in apple2.c. The TV is emulated by
 * analogtv.c for maximum realism
 *
 * Trevor Blackwell <tlb@tlb.org>
 */

static const char * const apple2_basic_errors[]={
  "BREAK",
  "NEXT WITHOUT FOR",
  "SYNTAX ERROR",
  "RETURN WITHOUT GOSUB",
  "ILLEGAL QUANTITY",
  "OVERFLOW",
  "OUT OF MEMORY",
  "BAD SUBSCRIPT ERROR",
  "DIVISION BY ZERO",
  "STRING TOO LONG",
  "FORMULA TOO COMPLEX",
  "UNDEF'D FUNCTION",
  "OUT OF DATA"
#if 0
  ,
  "DEFAULT ARGUMENTS ARE NOT ALLOWED IN DECLARATION OF FRIEND "
  "TEMPLATE SPECIALIZATION"
#endif

};
static const char * const apple2_dos_errors[]={
  "VOLUME MISMATCH",
  "I/O ERROR",
  "DISK FULL",
  "NO BUFFERS AVAILABLE",
  "PROGRAM TOO LARGE",
};

static void a2controller_crash(apple2_sim_t *sim, int *stepno,
                               double *next_actiontime)
{
  apple2_state_t *st=sim->st;
  char *s;
  int i;

  struct mydata {
    int fillptr;
    int fillbyte;
  } *mine;

  if (!sim->controller_data)
    sim->controller_data = calloc(sizeof(struct mydata),1);
  mine=(struct mydata *) sim->controller_data;

  switch(*stepno) {
  case 0:

    a2_init_memory_active(sim);
    sim->dec->powerup = 1000.0;

    if (random()%3==0) {
      st->gr_mode=0;
      *next_actiontime+=0.4;
      *stepno=100;
    }
    else if (random()%4==0) {
      st->gr_mode=A2_GR_LORES;
      if (random()%3==0) st->gr_mode |= A2_GR_FULL;
      *next_actiontime+=0.4;
      *stepno=100;
    }
    else if (random()%2==0) {
      st->gr_mode=A2_GR_HIRES;
      *stepno=300;
    }
    else {
      st->gr_mode=A2_GR_HIRES;
      *next_actiontime+=0.4;
      *stepno=100;
    }
    break;

  case 100:
    /* An illegal instruction or a reset caused it to drop into the
       assembly language monitor, where you could disassemble code & view
       data in hex. */
    if (random()%3==0) {
      char ibytes[128];
      char itext[128];
      int addr=0xd000+random()%0x3000;
      sprintf(ibytes,
              "%02X",random()%0xff);
      sprintf(itext,
              "???");
      sprintf(sim->printing_buf,
              "\n\n"
              "%04X: %-15s %s\n"
              " A=%02X X=%02X Y=%02X S=%02X F=%02X\n"
              "*",
              addr,ibytes,itext,
              random()%0xff, random()%0xff,
              random()%0xff, random()%0xff,
              random()%0xff);
      sim->printing=sim->printing_buf;
      a2_goto(st,23,1);
      if (st->gr_mode) {
        *stepno=180;
      } else {
        *stepno=200;
      }
      sim->prompt='*';
      *next_actiontime += 2.0 + (random()%1000)*0.0002;
    }
    else {
      /* Lots of programs had at least their main functionality in
         Applesoft Basic, which had a lot of limits (memory, string
         length, etc) and would sometimes crash unexpectedly. */
      sprintf(sim->printing_buf,
              "\n"
              "\n"
              "\n"
              "?%s IN %d\n"
              "\001]",
              apple2_basic_errors[random() %
                                  (sizeof(apple2_basic_errors)
                                   /sizeof(char *))],
              (1000*(random()%(random()%59+1)) +
               100*(random()%(random()%9+1)) +
               5*(random()%(random()%199+1)) +
               1*(random()%(random()%(random()%2+1)+1))));
      sim->printing=sim->printing_buf;
      a2_goto(st,23,1);
      *stepno=110;
      sim->prompt=']';
      *next_actiontime += 2.0 + (random()%1000)*0.0002;
    }
    break;

  case 110:
    if (random()%3==0) {
      /* This was how you reset the Basic interpreter. The sort of
         incantation you'd have on a little piece of paper taped to the
         side of your machine */
      sim->typing="CALL -1370";
      *stepno=120;
    }
    else if (random()%2==0) {
      sim->typing="CATALOG\n";
      *stepno=170;
    }
    else {
      *next_actiontime+=1.0;
      *stepno=999;
    }
    break;

  case 120:
    *stepno=130;
    *next_actiontime += 0.5;
    break;

  case 130:
    st->gr_mode=0;
    a2_cls(st);
    a2_goto(st,0,16);
    for (s="APPLE ]["; *s; s++) a2_printc(st,*s);
    a2_goto(st,23,0);
    a2_printc(st,']');
    *next_actiontime+=1.0;
    *stepno=999;
    break;

  case 170:
    if (random()%50==0) {
      sprintf(sim->printing_buf,
              "\nDISK VOLUME 254\n\n"
              " A 002 HELLO\n"
              "\n"
              "]");
      sim->printing=sim->printing_buf;
    }
    else {
      sprintf(sim->printing_buf,"\n?%s\n]",
              apple2_dos_errors[random()%
                                (sizeof(apple2_dos_errors) /
                                 sizeof(char *))]);
      sim->printing=sim->printing_buf;
    }
    *stepno=999;
    *next_actiontime+=1.0;
    break;

  case 180:
    if (random()%2==0) {
      /* This was how you went back to text mode in the monitor */
      sim->typing="FB4BG";
      *stepno=190;
    } else {
      *next_actiontime+=1.0;
      *stepno=999;
    }
    break;

  case 190:
    st->gr_mode=0;
    a2_invalidate(st);
    a2_printc(st,'\n');
    a2_printc(st,'*');
    *stepno=200;
    *next_actiontime+=2.0;
    break;

  case 200:
    /* This reset things into Basic */
    if (random()%2==0) {
      sim->typing="FAA6G";
      *stepno=120;
    }
    else {
      *stepno=999;
      *next_actiontime+=sim->delay;
    }
    break;

  case 300:
    for (i=0; i<1500; i++) {
      a2_poke(st, mine->fillptr, mine->fillbyte);
      mine->fillptr++;
      mine->fillbyte = (mine->fillbyte+1)&0xff;
    }
    *next_actiontime += 0.08;
    /* When you hit c000, it changed video settings */
    if (mine->fillptr>=0xc000) {
      a2_invalidate(st);
      st->gr_mode=0;
    }
    /* And it seemed to reset around here, I dunno why */
    if (mine->fillptr>=0xcf00) *stepno=130;
    break;

  case 999:
    break;

  case A2CONTROLLER_FREE:
    free(mine);
    mine = 0;
    break;
  }
}

static int
a2_draw (struct bsod_state *bst)
{
  apple2_sim_t *sim = (apple2_sim_t *) bst->closure;
  if (! sim) {
    sim = apple2_start (bst->dpy, bst->window, 9999999, a2controller_crash);
    bst->closure = sim;
  }

  if (! apple2_one_frame (sim)) {
    bst->closure = 0;
  }

  return 10000;
}

static void
a2_free (struct bsod_state *bst)
{
  apple2_sim_t *sim = (apple2_sim_t *) bst->closure;
  if (sim) {
    sim->stepno = A2CONTROLLER_DONE;
    a2_draw (bst);		/* finish up */
    if (bst->closure) abort();  /* should have been freed by now */
  }
}


static struct bsod_state *
apple2crash (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "apple2", "Apple2");
  bst->draw_cb = a2_draw;
  bst->free_cb = a2_free;
  return bst;
}


static void
a2controller_ransomware(apple2_sim_t *sim, int *stepno,
                        double *next_actiontime)
{
  apple2_state_t *st=sim->st;

  struct mydata {
    const char *str;
    Bool bold_p;
  } *mine;

  if (!sim->controller_data)
    sim->controller_data = calloc(sizeof(struct mydata),1);
  mine=(struct mydata *) sim->controller_data;

  switch(*stepno) {
  case 0:

    st->gr_mode |= A2_GR_FULL;
    a2_cls(st);
    a2_goto(st,0,16);
    a2_prints(st, "APPLE ][");
    a2_goto(st,2,0);
    *stepno = 10;
    *next_actiontime += 2;
    break;

  case 10:
    a2_prints(st, "READY\n\n");
    *stepno = 11;
    *next_actiontime += 1;
    break;

  case 11:
    a2_goto(st, 1, 0);
    *stepno = 12;
    mine->str =
      ("\n"
       " _____________________________________\n"
       "/                                     \\\n"
       "! OOPS YOUR FILES HAVE BEEN ENCRYPTED !\n"
       "!          ________________________   !\n"
       "!         !                        !  !\n"
       "!  [/--\\]   !  [ WHAT HAPPENED TO MY ] !  !\n"
       "!  [!]  [!]   !  [ COMPUTER? ]           !  !\n"
       "!  [!]  [!]   !                        !  !\n"
       "! [######]  !  [ CAN I RECOVER MY ]    !  !\n"
       "! [######]  !  [ FILES? ]              !  !\n"
       "! [######]  !                        !  !\n"
       "! [######]  !  [ HOW DO I PAY? ]       !  !\n"
       "!         !                        !  !\n"
       "!         !________________________!  !\n"
       "!                                     !\n"
       "!         BITCOIN ACCEPTED HERE       !\n"
       "\\_____________________________________/\n"
       "\n"
       "\n"
       "WAITING FOR BLOCKCHAIN..@\n"
       "\n"
       "PLEASE INSERT NEXT FLOPPY: "
       );
    break;

  case 12:
    {
      char c = *mine->str;
      mine->str++;

      if (c == 0)
        {
          *next_actiontime += 30;
          *stepno = 0;
        }
      else if (c == '[')
        mine->bold_p++;
      else if (c == ']')
        mine->bold_p--;
      else
        {
          if (c == '@')
            {
              c = '.';
              *next_actiontime += 2;
            }

          if (mine->bold_p)
            c |= 0xC0;
          a2_printc_noscroll(st, c);
          if (c == '.')
            *next_actiontime += 1;
        }
    }
    break;

  case A2CONTROLLER_FREE:
    return;
  }
}


static int
a2_ransomware_draw (struct bsod_state *bst)
{
  apple2_sim_t *sim = (apple2_sim_t *) bst->closure;
  if (! sim) {
    sim = apple2_start (bst->dpy, bst->window, 9999999,
                        a2controller_ransomware);
    bst->closure = sim;
  }

  if (! apple2_one_frame (sim)) {
    bst->closure = 0;
  }

  return 10000;
}

static struct bsod_state *
apple2ransomware (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "apple2", "Apple2");
  bst->draw_cb = a2_ransomware_draw;
  bst->free_cb = a2_free;
  return bst;
}



static void
a2controller_encom (apple2_sim_t *sim, int *stepno,
                    double *next_actiontime)
{
  apple2_state_t *st=sim->st;

  struct mydata {
    const char *str;
    Bool bold_p;
  } *mine;

  if (!sim->controller_data)
    sim->controller_data = calloc(sizeof(struct mydata),1);
  mine=(struct mydata *) sim->controller_data;

  switch(*stepno) {
  case 0:

    st->gr_mode |= A2_GR_FULL;
    a2_cls(st);
    a2_goto(st,0,35);
    a2_prints(st, "ENCOM");
    a2_goto(st,23,0);
    *stepno = 10;
    *next_actiontime += 6;
    break;

  case 10:
    a2_cls(st);
    a2_goto(st,0,0);
    *stepno = 11;
    *next_actiontime += 1;
    break;

  case 11:
    a2_goto(st, 1, 0);
    *stepno = 12;
    mine->str = ("\n"
                 "\r\n"
                 "\r\n"
                 "\r\n"
                 "\r\n"
                 "\r\n"
                 "\r\n"
                 "\r\n"
                 "\r\n"
                 "SEPT 22, 18:32:21 PM\n"
                 "\n"
                 "         YOUR ACCESS SUSPENDED\n"
                 "         PLEASE REPORT TO DILLINGER\n"
                 "         IMMEDIATELY\n"
                 "         AUTHORIZATION: MASTER CONTROL\n"
                 "         PROGRAM\n"
                 "\n"
                 "\r\r\r\r\r"
                 "         END OF LINE\n"
                 "\n"
                 "\n"
                 "\n"
                 "\n");
    break;

  case 12:
    {
      char c = *mine->str;
      mine->str++;

      if (c == 0)
        {
          *next_actiontime += 30;
          *stepno = 0;
        }
      else
        {
          if (c == '\r')
            *next_actiontime += 0.2;
          if (mine->bold_p)
            c |= 0xC0;
          a2_printc_noscroll(st, c);
        }
    }
    break;

  case A2CONTROLLER_FREE:
    return;
  }
}


static int
a2_encom_draw (struct bsod_state *bst)
{
  apple2_sim_t *sim = (apple2_sim_t *) bst->closure;
  if (! sim) {
    sim = apple2_start (bst->dpy, bst->window, 9999999,
                        a2controller_encom);
    bst->closure = sim;
  }

  if (! apple2_one_frame (sim)) {
    bst->closure = 0;
  }

  return 10000;
}

static struct bsod_state *
encom (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "encom", "Encom");
  bst->draw_cb = a2_encom_draw;
  bst->free_cb = a2_free;
  return bst;
}


/* A crash spotted on a cash machine circa 2006, by jwz.  I didn't note
   what model it was; probably a Tranax Mini-Bank 1000 or similar vintage.
 */
static struct bsod_state *
atm (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "atm", "ATM");

  int pix_w, pix_h;
  int x, y, i = 0;
  float scale = 0.48;
  Pixmap mask = 0;
  Pixmap pixmap = image_data_to_pixmap (dpy, window,
                                        atm_png, sizeof(atm_png),
                                        &pix_w, &pix_h, &mask);

  XClearWindow (dpy, window);

  while (pix_w <= bst->xgwa.width  * scale &&
         pix_h <= bst->xgwa.height * scale)
    {
      pixmap = double_pixmap (dpy, bst->xgwa.visual, bst->xgwa.depth,
                              pixmap, pix_w, pix_h);
      mask = double_pixmap (dpy, bst->xgwa.visual, 1, mask, pix_w, pix_h);
      pix_w *= 2;
      pix_h *= 2;
      i++;
    }

  x = (bst->xgwa.width  - pix_w) / 2;
  y = (bst->xgwa.height - pix_h) / 2;
  if (y < 0) y = 0;

  if (i > 0)
    {
      int j;
      XSetForeground (dpy, bst->gc,
                      get_pixel_resource (dpy, bst->xgwa.colormap,
                                          "atm.background",
                                          "ATM.Background"));
      for (j = -1; j < pix_w; j += i+1)
        XDrawLine (bst->dpy, pixmap, bst->gc, j, 0, j, pix_h);
      for (j = -1; j < pix_h; j += i+1)
        XDrawLine (bst->dpy, pixmap, bst->gc, 0, j, pix_w, j);
    }

  XSetClipMask (dpy, bst->gc, mask);
  XSetClipOrigin (dpy, bst->gc, x, y);
  XCopyArea (dpy, pixmap, window, bst->gc, 0, 0, pix_w, pix_h, x, y);

  XFreePixmap (dpy, pixmap);
  XFreePixmap (dpy, mask);

  return bst;
}


static struct bsod_state *
dvd (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "dvd", "DVD");
  int pix_w, pix_h;
  float scale = 0.15;
  Pixmap mask = 0;
  Pixmap pixmap = image_data_to_pixmap (dpy, window,
                                        dvd_png, sizeof(dvd_png),
                                        &pix_w, &pix_h, &mask);
  int i = 0;
  int x, y, dx, dy;
  int steps = 10000;
  int speed = 1;

  XClearWindow (dpy, window);

  while (pix_w <= bst->xgwa.width  * scale &&
         pix_h <= bst->xgwa.height * scale)
    {
      pixmap = double_pixmap (dpy, bst->xgwa.visual, bst->xgwa.depth,
                              pixmap, pix_w, pix_h);
      mask = double_pixmap (dpy, bst->xgwa.visual, 1, mask, pix_w, pix_h);
      pix_w *= 2;
      pix_h *= 2;
      speed *= 2;
      i++;
    }

  bst->pixmap = pixmap;
  bst->mask = mask;
  x = random() % (bst->xgwa.width  - pix_w);
  y = random() % (bst->xgwa.height - pix_h);
  dx = speed * (random() & 1 ? 1 : -1);
  dy = speed * (random() & 1 ? 1 : -1);

  BSOD_INVERT(bst);
  for (i = 0; i < steps; i++)
    {
      BSOD_RECT (bst, True, x, y, pix_w, pix_h);
      if (x + dx < 0 || x + dx + pix_w > bst->xgwa.width)  dx = -dx;
      if (y + dy < 0 || y + dy + pix_h > bst->xgwa.height) dy = -dy;
      x += dx;
      y += dy;
      BSOD_PIXMAP (bst, 0, 0, pix_w, pix_h, x, y);
      BSOD_PAUSE (bst, 1000000 / 30.0);
    }

  return bst;
}


static struct bsod_state *
tivo (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "tivo", "Tivo");
  XGlyphInfo ov;
  int line_height = bst->font->ascent + bst->font->descent;
  int char_width, left, top;
  XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) "n", 1, &ov);
  char_width = ov.xOff;

  left = (bst->xgwa.width - char_width * 44) / 2;
  top = (bst->xgwa.height - line_height * 15) / 2;
  if (left < 0) left = 0;
  if (top < 0) top = 0;

  XClearWindow (dpy, window);

  BSOD_MARGINS (bst, left, left);
  BSOD_MOVETO (bst, left, top);

  BSOD_FONT (bst, 1);
  BSOD_TEXT (bst, LEFT, "\nA severe error has occurred.\n\n");
  BSOD_FONT (bst, 0);
  BSOD_TEXT (bst, LEFT,
             "Please leave the Receiver plugged in and connected\n"
             "to the phone line for the next three hours while the\n"
             "Receiver attempts to repair itself.");
  BSOD_FONT (bst, 1);
  BSOD_TEXT (bst, LEFT,
             "\n\n"
             "DO NOT UNPLUG OR RESTART\nTHE RECEIVER.\n\n");
  BSOD_FONT (bst, 0);
  BSOD_TEXT (bst, LEFT,
             "If, after three hours, the Receiver does not restart\n"
             "itself, call Customer Care.");

  BSOD_PAUSE (bst, 1000000 * 60);
  return bst;
}


/* Error message for corrupted (and therefore presumed bootleg) cartridges.
 */
static struct bsod_state *
nintendo (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window,
                                            "nintendo", "Nintendo");
  unsigned long bg = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "nintendo.background",
                                         "Nintendo.Background");
  unsigned long bg2 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                          "nintendo.background2",
                                          "Nintendo.Background");
  unsigned long fg = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "nintendo.foreground",
                                         "Nintendo.Foreground");
  int line_height = bst->font->ascent + bst->font->descent;
  XGlyphInfo ov;
  int char_width;
  int left, top, left2, top2;

  XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) "n", 1, &ov);
  char_width = ov.xOff;
  left = (bst->xgwa.width - char_width * 30) / 2;
  top = (bst->xgwa.height - line_height * 9) / 2;
  left2 = left - char_width * 4;
  top2  = top - line_height;
  if (left < 0) left = 0;
  if (top < 0) top = 0;
  if (left2 < 0) left2 = 0;
  if (top2 < 0) top2 = 0;
  if (left2 > char_width * 8) left2 = char_width * 8;
  if (top2  > line_height * 10) top2 = line_height * 10;

  XClearWindow (dpy, window);

  BSOD_COLOR (bst, bg2, bg);
  BSOD_RECT (bst, True, left2, top2 - line_height*2,
             bst->xgwa.width - left2*2, 
             bst->xgwa.height - top2*2 + line_height*2);

  BSOD_MARGINS (bst, left, left);
  BSOD_MOVETO (bst, left, top - line_height/2);

  BSOD_FONT (bst, 1);
  BSOD_COLOR (bst, bg, bg2);

  /* a variant crash has a second box above the English text that says:

                        警告
         ビデオゲームのコピーは法律で禁じられています。
         詳しくは取扱説明書をご覧になってください。

     but "PxPlus IBM VGA8" doesn't contain Japanese characters.
   */

  BSOD_TEXT (bst, CENTER, "WARNING");
  BSOD_FONT (bst, 0);
  BSOD_COLOR (bst, fg, bg2);
  BSOD_TEXT (bst, LEFT,
             "\n\n"
             "IT IS A SERIOUS CRIME\n"
             "TO COPY VIDEO GAMES\n"
             "ACCORDING TO COPYRIGHT LAW.\n"
             "PLEASE REFER TO\n"
             "YOUR NINTENDO GAME\n"
             "INSTRUCTION BOOKLET\n"
             "FOR FURTHER INFORMATION.");

  BSOD_PAUSE (bst, 1000000 * 60);
  return bst;
}


/* A 2013 Android phone boot loader, by jwz.
 */
static struct bsod_state *
android (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "android", "Android");

  unsigned long bg = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "android.background",
                                         "Android.Background");
  unsigned long fg = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "android.foreground",
                                         "Android.Foreground");
  unsigned long c1 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "android.color1",
                                         "Android.Foreground");
  unsigned long c2 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "android.color2",
                                         "Android.Foreground");
  unsigned long c3 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "android.color3",
                                         "Android.Foreground");
  unsigned long c4 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "android.color4",
                                         "Android.Foreground");
  unsigned long c5 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "android.color5",
                                         "Android.Foreground");
  unsigned long c6 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "android.color6",
                                         "Android.Foreground");
  unsigned long c7 = get_pixel_resource (dpy, bst->xgwa.colormap,
                                         "android.color7",
                                         "Android.Foreground");

  const char * const lines0[] = {
    "Calculating... please wait\n",
    "osbl:     0x499DF907\n",
    "amss:     0x73162409\n",
    "hboot:    0xE46C3327\n",
    "boot:     0xBA570E7A\n",
    "recovery: 0xC8BBA213\n",
    "system:   0x87C3B1F0\n",
    "\n",
    "Press power key to go back.\n",
  };

  const char * const lines1[] = {
    "Checking SD card update...\n",
    "",
    "  SD Checking...\n",
    "  Failed to open zipfile\n",
    "  loading preload_content...\n",
    "  [Caution] Preload Content Not Found\n",
    "  loading HTCUpdateZipName image...\n",
    "",
    "  Checking...[PG46IMG.zip]\n",
    "Please plug off USB\n",
  };

  const char * const lines2[] = {
    "  SD Checking...\n",
    "  Loading...[PK76DIAG.zip]\n",
    "  No image!\n",
    "  Loading...[PK76DIAG.nbh]\n",
    "  No image or wrong image!\n",
    "  Loading...[PK76IMG.zip]\n",
    "  No image!\n",
    "  Loading...[PK76IMG.nbh]\n",
    "  No image or wrong image!\n",
    "  Loading...[PK76IMG.tar]\n",
    "  No image!\n",
    "  Loading...[PK76IMG.aes]\n",
    "  No image!\n",
    "  Loading...[PK76IMG.enc]\n",
    "  No image!\n",
  };

  int line_height = bst->font->ascent + bst->font->descent;
  int cw;
  XGlyphInfo ov;
  int state = 0;
  Pixmap pixmap = 0, mask = 0;
  int pix_w = 0, pix_h = 0;

  XftTextExtentsUtf8 (bst->dpy, bst->font, (FcChar8 *) "n", 1, &ov);
  cw = ov.xOff;

  pixmap = image_data_to_pixmap (dpy, window, 
                                 android_png, sizeof(android_png),
                                 &pix_w, &pix_h, &mask);
  if (! pixmap) abort();
  {
    int i, n = 0;
    if (bst->xgwa.width > 2560 || bst->xgwa.height > 2560)
      n++;  /* Retina displays */
    for (i = 0; i < n; i++)
      {
        pixmap = double_pixmap (dpy, bst->xgwa.visual, bst->xgwa.depth,
                                pixmap, pix_w, pix_h);
        mask = double_pixmap (dpy, bst->xgwa.visual, 1,
                              mask, pix_w, pix_h);
        pix_w *= 2;
        pix_h *= 2;
      }
  }
  bst->pixmap = pixmap;
  bst->mask = mask;

  bst->left_margin = (bst->xgwa.width - (cw * 40)) / 2;
  if (bst->left_margin < 0) bst->left_margin = 0;

  while (1) {
    unsigned long delay =
      ((state == 0 ||
        state == countof(lines0) ||
        state == countof(lines0) + countof(lines1) ||
        state == countof(lines0) + countof(lines1) + countof(lines2))
                           ? 10000 : 0);
    BSOD_LINE_DELAY (bst, delay);

    if (state <= countof(lines0) + countof(lines1) + countof(lines2))
      {
        BSOD_COLOR (bst, bg, bg);
        BSOD_RECT (bst, True, 0, 0, bst->xgwa.width, bst->xgwa.height);
        BSOD_COLOR (bst, bg, c1);
        BSOD_MOVETO (bst, bst->left_margin + bst->xoff,
                     bst->top_margin + bst->yoff + line_height);
        BSOD_TEXT (bst, LEFT, "*** UNLOCKED ***\n");
        BSOD_COLOR (bst, c2, bg);
        BSOD_TEXT (bst, LEFT,
                   "PRIMOU PVT SHIP S-OFF RL\n"
                   "HBOOT-1.17.0000\n"
                   "CPLD-None\n"
                   "MICROP-None\n"
                   "RADIO-3831.17.00.23_2\n"
                   "eMMC-bootmode: disabled\n"
                   "CPU-bootmode : disabled\n"
                   "HW Secure boot: enabled\n"
                   "MODEM PATH : OFF\n"
                   "May 15 2012, 10:28:15\n"
                   "\n");
        BSOD_COLOR (bst, bg, c3);

        if (pixmap)
          {
            int x = (bst->xgwa.width - pix_w) / 2;
            int y = bst->xgwa.height - bst->yoff - pix_h;
            BSOD_PIXMAP (bst, 0, 0, pix_w, pix_h, x, y);
          }
      }

    if (state == countof(lines0) ||
        state == countof(lines0) + countof(lines1) ||
        state == countof(lines0) + countof(lines1) + countof(lines2))
      {
        BSOD_TEXT (bst, LEFT, "HBOOT USB\n");
        BSOD_COLOR (bst, c4, bg);
        BSOD_TEXT (bst, LEFT,
                   "\n"
                   "<VOL UP> to previous item\n"
                   "<VOL DOWN> to next item\n"
                   "<POWER> to select item\n"
                   "\n");
        BSOD_COLOR (bst, c5, bg); BSOD_TEXT (bst, LEFT, "FASTBOOT\n");
        BSOD_COLOR (bst, c6, bg); BSOD_TEXT (bst, LEFT, "RECOVERY\n");
        BSOD_COLOR (bst, c7, bg); BSOD_TEXT (bst, LEFT, "FACTORY RESET\n");
        BSOD_COLOR (bst, c3, bg); BSOD_TEXT (bst, LEFT, "SIMLOCK\n");
        BSOD_COLOR (bst, bg, c3); BSOD_TEXT (bst, LEFT, "HBOOT USB\n");
        BSOD_COLOR (bst, fg, bg); BSOD_TEXT (bst, LEFT, "IMAGE CRC\n");
        BSOD_COLOR (bst, c3, bg); BSOD_TEXT (bst, LEFT, "SHOW BARCODE\n");
        BSOD_PAUSE (bst, 3000000);
      }
    else if (state < countof(lines0))
      {
        BSOD_TEXT (bst, LEFT, "IMAGE CRC\n\n");
        BSOD_COLOR (bst, c5, bg);
        {
          int i;
          for (i = 0; i <= state; i++) {
            const char *s = lines0[i];
            BSOD_COLOR (bst, (strchr(s, ':') ? c7 : c3), bg);
            BSOD_TEXT (bst, LEFT, s);
          }
        }
        BSOD_PAUSE (bst, 500000);
        if (state == countof(lines0)-1)
          BSOD_PAUSE (bst, 2000000);
      }
    else if (state < countof(lines0) + countof(lines1))
      {
        BSOD_TEXT (bst, LEFT, "HBOOT\n\n");
        BSOD_COLOR (bst, c5, bg);
        {
          int i;
          for (i = countof(lines0); i <= state; i++) {
            const char *s = lines1[i - countof(lines0)];
            BSOD_COLOR (bst, (*s == ' ' ? c6 : c3), bg);
            BSOD_TEXT (bst, LEFT, s);
          }
        }
        BSOD_PAUSE (bst, 500000);
        if (state == countof(lines0) + countof(lines1) - 1)
          BSOD_PAUSE (bst, 2000000);
      }
    else if (state < countof(lines0) + countof(lines1) + countof(lines2))
      {
        BSOD_TEXT (bst, LEFT, "HBOOT USB\n\n");
        BSOD_COLOR (bst, c5, bg);
        {
          int i;
          for (i = countof(lines0) + countof(lines1); i <= state; i++) {
            const char *s = lines2[i - countof(lines0) - countof(lines1)];
            BSOD_COLOR (bst, (*s == ' ' ? c6 : c3), bg);
            BSOD_TEXT (bst, LEFT, s);
          }
        }
        BSOD_PAUSE (bst, 500000);
        if (state == countof(lines0) + countof(lines1) + countof(lines2)-1)
          BSOD_PAUSE (bst, 2000000);
      }
    else
      break;

    state++;
  }

  XClearWindow (dpy, window);

  return bst;
}


/* Gnome SOD. Truly 2020 will be the year of the Linux Desktop. */
static struct bsod_state *
gnome (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "gnome", "Gnome");

  int pix_w, pix_h;
  int x, y;
  int lh = bst->font->ascent + bst->font->descent;
  Pixmap mask = 0;
  Pixmap pixmap;
  unsigned long fg, bg;
  Bool which = random() & 1;

  if (which)
    {
      pixmap = image_data_to_pixmap (dpy, window,
                                     gnome2_png, sizeof(gnome2_png),
                                     &pix_w, &pix_h, &mask);
      fg = get_pixel_resource (dpy, bst->xgwa.colormap,
                               "gnome.foreground2", "Gnome.Foreground");
      bg = get_pixel_resource (dpy, bst->xgwa.colormap,
                               "gnome.background2", "Gnome.Background");
    }
  else
    {
      pixmap = image_data_to_pixmap (dpy, window,
                                     gnome1_png, sizeof(gnome1_png),
                                     &pix_w, &pix_h, &mask);
      fg = get_pixel_resource (dpy, bst->xgwa.colormap,
                               "gnome.foreground", "Gnome.Foreground");
      bg = get_pixel_resource (dpy, bst->xgwa.colormap,
                               "gnome.background", "Gnome.Background");
    }

  x = (bst->xgwa.width - pix_w) / 2;
  y = (bst->xgwa.height - pix_h) / 2;
  if (y < 0) y = 0;

  XSetWindowBackground (dpy, window, bg);
  XClearWindow (dpy, window);
  XSetClipMask (dpy, bst->gc, mask);
  XSetClipOrigin (dpy, bst->gc, x, y);
  XCopyArea (dpy, pixmap, window, bst->gc, 0, 0, pix_w, pix_h, x, y);
  XSetClipMask (dpy, bst->gc, None);
  XFreePixmap (dpy, mask);

  BSOD_MOVETO (bst, 0, y + pix_h + lh * 2);
  BSOD_COLOR (bst, fg, bg);
  BSOD_FONT (bst, 0);
  BSOD_TEXT (bst, CENTER, "Oh no!  Something has gone wrong!\n\n");
  BSOD_FONT (bst, 1);
  BSOD_TEXT (bst, CENTER,
             "A problem has occurred and the system can't recover.\n");
  BSOD_TEXT (bst, CENTER, "Please log out and try again.");
  BSOD_PAUSE (bst, 60 * 1000000);

  return bst;
}


/* Linux kernel panic obscured by systemd, 2024.
   https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/gpu/drm/drm_panic.c
 */
static struct bsod_state *
systemd (Display *dpy, Window window)
{
  struct bsod_state *bst = make_bsod_state (dpy, window, "systemd", "Systemd");
  int lh = (bst->font->ascent + bst->font->descent);
  BSOD_MOVETO (bst, 0, lh);
  BSOD_TEXT (bst, LEFT,
             "     .--.        _\n"
             "    |o_o |      | |\n"
             "    |:_/ |      | |\n"
             "   //   \\ \\     |_|\n"
             "  (|     | )     _\n"
             " /'\\_   _/`\\    (_)\n"
             " \\___)=(___/\n");
  BSOD_MOVETO (bst, 0, (bst->xgwa.height / 2) - lh);
  BSOD_TEXT (bst, CENTER,
             "KERNEL PANIC !\n"
             "\n"
             "Please reboot your computer.");
  XClearWindow (dpy, window);
  return bst;
}


/*****************************************************************************
 *****************************************************************************/


static const struct {
  const char *name;
  struct bsod_state * (*fn) (Display *, Window);
} all_modes[] = {
  { "Windows",		windows_31 },
  { "NT",		windows_nt },
  { "Win2K",		windows_other },
  { "Win10",		windows_10 },
  { "Bitlocker",	bitlocker },
  { "Ransomware",	windows_ransomware },
  { "Amiga",		amiga },
  { "Mac",		mac },
  { "MacsBug",		macsbug },
  { "Mac1",		mac1 },
  { "MacX",		macx },
  { "SCO",		sco },
  { "HVX",		hvx },
  { "HPPALinux",	hppa_linux },
  { "SparcLinux",	sparc_linux },
  { "BSD",		bsd },
  { "Atari",		atari },
#ifndef HAVE_JWXYZ
  { "BlitDamage",	blitdamage },
#endif
  { "Solaris",		sparc_solaris },
  { "Linux",		linux_fsck },
  { "HPUX",		hpux },
  { "OS390",		os390 },
  { "Tru64",		tru64 },
  { "VMS",		vms },
  { "OS2",		os2 },
  { "MSDOS",		msdos },
  { "Nvidia",		nvidia },
  { "Apple2",		apple2crash },
  { "ATM",		atm },
  { "GLaDOS",		glados },
  { "Android",		android },
  { "VMware",		vmware },
  { "VMwareArm",	vmware_arm },
  { "Encom",		encom },
  { "DVD",		dvd },
  { "Tivo",		tivo },
  { "Nintendo",		nintendo },
  { "Gnome",		gnome },
  { "Systemd",		systemd },
};


struct driver_state {
  const char *name;
  int only, which, next_one;
  int mode_duration;
  int delay_remaining;
  time_t start;
  Bool debug_p, cycle_p;
  struct bsod_state *bst;
};


static void
hack_title (struct driver_state *dst)
{
# ifndef HAVE_JWXYZ
  char *oname = 0;
  XFetchName (dst->bst->dpy, dst->bst->window, &oname);
  if (oname && !strncmp (oname, "BSOD: ", 6)) {
    char *tail = oname + 4;
    char *s = strchr (tail+1, ':');
    char *nname;
    if (s) tail = s;
    nname = malloc (strlen (tail) + strlen (dst->name) + 20);
    sprintf (nname, "BSOD: %s%s", dst->name, tail);
    XStoreName (dst->bst->dpy, dst->bst->window, nname);
    free (nname);
  }
  if (oname) free (oname);
# endif /* !HAVE_JWXYZ */
}

static void *
bsod_init (Display *dpy, Window window)
{
  struct driver_state *dst = (struct driver_state *) calloc (1, sizeof(*dst));
  char *s;

  dst->mode_duration = get_integer_resource (dpy, "delay", "Integer");
  if (dst->mode_duration < 3) dst->mode_duration = 3;

  dst->debug_p = get_boolean_resource (dpy, "debug", "Boolean");

  dst->only = -1;
  dst->next_one = -1;
  s = get_string_resource(dpy, "doOnly", "DoOnly");
  if (s && !strcasecmp (s, "cycle"))
    {
      dst->which = -1;
      dst->cycle_p = True;
    }
  else if (s && *s)
    {
      int count = countof(all_modes);
      for (dst->only = 0; dst->only < count; dst->only++)
        if (!strcasecmp (s, all_modes[dst->only].name))
          break;
      if (dst->only >= count)
        {
          fprintf (stderr, "%s: unknown -only mode: \"%s\"\n", progname, s);
          dst->only = -1;
        }
    }
  if (s) free (s);

  dst->name = "none";
  dst->which = -1;
  return dst;
}


static unsigned long
bsod_draw (Display *dpy, Window window, void *closure)
{
  struct driver_state *dst = (struct driver_state *) closure;
  time_t now;
  int time_left;

 AGAIN:
  now = time ((time_t *) 0);
  time_left = dst->start + dst->mode_duration - now;

  if (dst->bst && dst->bst->img_loader)   /* still loading */
    {
      dst->bst->img_loader =
        load_image_async_simple (dst->bst->img_loader, 0, 0, 0, 0, 0);
      return 100000;
    }

 DELAY_NOW:
  /* Rather than returning a multi-second delay from the draw() routine,
     meaning "don't call us again for N seconds", we quantize that down
     to 1/10th second intervals so that it's more responsive to
     rotate/reshape events.
   */
  if (dst->delay_remaining)
    {
      int inc = 10000;
      int this_delay = MIN (dst->delay_remaining, inc);
      dst->delay_remaining = MAX (0, dst->delay_remaining - inc);
      if (time_left < 0 && dst->start > 0)
        dst->delay_remaining = 0;
      return this_delay;
    }

  if (! dst->bst && time_left > 0)	/* run completed; wait out the delay */
    {
      if (dst->debug_p)
        fprintf (stderr, "%s: %s: %d left\n", progname, dst->name, time_left);
      dst->start = 0;
      if (time_left > 5) time_left = 5;  /* Boooored now */
      dst->delay_remaining = 1000000 * time_left;
    }

  else if (dst->bst)			/* sub-mode currently running */
    {
      int this_delay = -1;

      if (time_left > 0)
        this_delay = bsod_pop (dst->bst);

      /* XSync (dpy, False);  slows down char drawing too much on HAVE_JWXYZ */

      if (this_delay == 0){
        goto AGAIN;			/* no delay, not expired: stay here */
}
      else if (this_delay >= 0)
        {
          dst->delay_remaining = this_delay;	/* return; time to sleep */
          goto DELAY_NOW;
        }
      else
        {				/* sub-mode run completed or expired */
          if (dst->debug_p)
            fprintf (stderr, "%s: %s: done\n", progname, dst->name);
          free_bsod_state (dst->bst);
          dst->bst = 0;
          return 0;
        }
    }
  else					/* launch a new sub-mode */
    {
      if (dst->next_one >= 0)
        dst->which = dst->next_one, dst->next_one = -1;
      else if (dst->cycle_p)
        dst->which = (dst->which + 1) % countof(all_modes);
      else if (dst->only >= 0)
        dst->which = dst->only;
      else
        {
          int count = countof(all_modes);
          int *enabled = (int *) calloc (sizeof(*enabled), count + 1);
          int nenabled = 0;
          int i;

          for (i = 0; i < count; i++)
            {
              char name[100], class[100];
              sprintf (name,  "do%s", all_modes[i].name);
              sprintf (class, "Do%s", all_modes[i].name);
              if (get_boolean_resource (dpy, name, class))
                enabled[nenabled++] = i;
            }

          if (nenabled == 0)
            {
              fprintf (stderr, "%s: no display modes enabled?\n", progname);
              /* exit (-1); */
              dst->which = dst->only = 0;
            }
          else if (nenabled == 1)
            dst->which = enabled[0];
          else
            {
              i = dst->which;
              while (i == dst->which)
                i = enabled[random() % nenabled];
              dst->which = i;
            }
          free (enabled);
        }

      if (dst->debug_p)
        fprintf (stderr, "%s: %s: launch\n", progname,
                 all_modes[dst->which].name);

      /* Run the mode setup routine...
       */
      if (dst->bst) abort();
      dst->name  = all_modes[dst->which].name;
      dst->bst   = all_modes[dst->which].fn (dpy, window);
      dst->start = (dst->bst ? time ((time_t *) 0) : 0);

      /* Reset the structure run state to the beginning,
         and do some sanitization of the cursor position
         before the first run.
       */
      if (dst->bst)
        {
          if (dst->debug_p)
            fprintf (stderr, "%s: %s: queue size: %d (%d)\n", progname,
                     dst->name, dst->bst->pos, dst->bst->queue_size);

          hack_title (dst);
          dst->bst->pos = 0;
          dst->bst->x = dst->bst->current_left =
            dst->bst->left_margin + dst->bst->xoff;

          if (dst->bst->y < 
              dst->bst->top_margin + dst->bst->yoff + dst->bst->font->ascent)
            dst->bst->y =
              dst->bst->top_margin + dst->bst->yoff + dst->bst->font->ascent;
        }
    }

  return 0;
}


static void
bsod_reshape (Display *dpy, Window window, void *closure,
              unsigned int w, unsigned int h)
{
  struct driver_state *dst = (struct driver_state *) closure;

  if (dst->bst &&
      w == dst->bst->xgwa.width &&
      h == dst->bst->xgwa.height)
    return;

  if (dst->debug_p)
    fprintf (stderr, "%s: %s: reshape reset\n", progname, dst->name);

  /* just restart this mode and restart when the window is resized. */
  if (dst->bst)
    free_bsod_state (dst->bst);
  dst->bst = 0;
  dst->start = 0;
  dst->delay_remaining = 0;
  dst->next_one = dst->which;
  dst->name = "none";
  XClearWindow (dpy, window);
}


static Bool
bsod_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct driver_state *dst = (struct driver_state *) closure;
  Bool reset_p = False;

  /* pick a new mode and restart when mouse clicked, or certain keys typed. */

  if (screenhack_event_helper (dpy, window, event))
    reset_p = True;

  if (reset_p)
    {
      if (dst->debug_p)
        fprintf (stderr, "%s: %s: manual reset\n", progname, dst->name);
      if (dst->bst)
        free_bsod_state (dst->bst);
      dst->bst = 0;
      dst->start = 0;
      dst->delay_remaining = 0;
      dst->name = "none";
      XClearWindow (dpy, window);
      return True;
    }
  else
    return False;
}


static void
bsod_free (Display *dpy, Window window, void *closure)
{
  struct driver_state *dst = (struct driver_state *) closure;
  if (dst->bst)
    free_bsod_state (dst->bst);
  free (dst);
}


static const char *bsod_defaults [] = {
  "*delay:		   45",
  "*debug:		   False",

  "*doOnly:		   ",
  "*doWindows:		   True",
  "*doNT:		   True",
  "*doWin2K:		   True",
  "*doWin10:		   True",
  "*doBitlocker:	   True",
  "*doRansomware:	   True",
  "*doAmiga:		   True",
  "*doMac:		   True",
  "*doMacsBug:		   True",
  "*doMac1:		   True",
  "*doMacX:		   True",
  "*doSCO:		   True",
  "*doAtari:		   False",	/* boring */
  "*doBSD:		   False",	/* boring */
  "*doLinux:		   True",
  "*doSparcLinux:	   False",	/* boring */
  "*doHPPALinux:	   True",
  "*doBlitDamage:          True",
  "*doSolaris:             True",
  "*doHPUX:                True",
  "*doTru64:               True",
  "*doApple2:              True",
  "*doOS390:               True",
  "*doVMS:		   True",
  "*doHVX:		   True",
  "*doMSDOS:		   True",
  "*doOS2:		   True",
  "*doNvidia:		   True",
  "*doATM:		   True",
  "*doGLaDOS:		   True",
  "*doAndroid:		   False",
  "*doVMware:		   True",
  "*doVMwareArm:	   True",
  "*doEncom:		   True",
  "*doDVD:		   True",
  "*doTivo:		   True",
  "*doNintendo:		   True",
  "*doGnome:		   True",
  "*doSystemd:		   True",

  ".foreground:		   White",
  ".background:		   Black",

  ".windows.foreground:	   White",
  ".windows.background:	   #0000AA",    /* EGA color 0x01. */

  ".nt.foreground:	   White",
  ".nt.background:	   #0000AA",    /* EGA color 0x01. */

  ".windowslh.foreground:  White",
  ".windowslh.background:  #AA0000",    /* EGA color 0x04. */
  ".windowslh.background2: #AAAAAA",    /* EGA color 0x07. */

  ".windowssb.foreground:  White",
  ".windowssb.background:  #000000",
  ".windowssb.background2: #FF0000",

  ".win10.foreground:      White",
  ".win10.background:      #1070AA",

  ".win10r.foreground:     White",
  ".win10r.background:     #1070AA",

  ".bitlocker.foreground:  White",
  ".bitlocker.background:  #1070AA",

  ".ransomware.foreground:   White",
  ".ransomware.background:   #841212",
  ".ransomware.foreground2:  Black",      /* ransom note */
  ".ransomware.background2:  White",
  ".ransomware.foreground3:  Black",      /* buttons */
  ".ransomware.background3:  #AAAAAA",
  ".ransomware.link:         #7BF9F6",
  ".ransomware.timerheader:  #BDBE02",


  ".glaDOS.foreground:	   White",
  ".glaDOS.background:	   #0000AA",    /* EGA color 0x01. */

  ".amiga.foreground:	   #FF0000",
  ".amiga.background:	   Black",
  ".amiga.background2:	   White",

  ".mac.foreground:	   #FFFFFF",
  ".mac.background:	   Black",

  ".atari.foreground:	   Black",
  ".atari.background:	   White",

  ".macsbug.foreground:	   Black",
  ".macsbug.background:	   White",
  ".macsbug.borderColor:   #AAAAAA",

  ".mac1.foreground:	   Black",
  ".mac1.background:	   White",

  ".macx.foreground:       White",
  ".macx.textForeground:   White",
  ".macx.textBackground:   Black",
  ".macx.background:	   #888888",

  ".macinstall.barForeground: #C0C0C0",
  ".macinstall.barBackground: #888888",

  ".sco.foreground:	   White",
  ".sco.background:	   Black",

  ".hvx.foreground:	   White",
  ".hvx.background:	   Black",

  ".linux.foreground:      White",
  ".linux.background:      Black",

  ".hppalinux.foreground:  White",
  ".hppalinux.background:  Black",

  ".sparclinux.foreground: White",
  ".sparclinux.background: Black",

  ".bsd.foreground:	   #c0c0c0",
  ".bsd.background:	   Black",

  ".solaris.foreground:    Black",
  ".solaris.background:    White",

  ".hpux.foreground:	   White",
  ".hpux.background:	   Black",

  ".os390.background:	   Black",
  ".os390.foreground:	   Red",

  ".tru64.foreground:	   White",
  ".tru64.background:	   #0000AA",    /* EGA color 0x01. */

  ".vms.foreground:	   White",
  ".vms.background:	   Black",

  ".msdos.foreground:	   White",
  ".msdos.background:	   Black",

  ".os2.foreground:	   White",
  ".os2.background:	   Black",

  ".atm.foreground:	   Black",
  ".atm.background:	   #FF6600",

  ".android.foreground:	   Black",
  ".android.background:	   White",
  ".android.color1:	   #AA00AA", /* violet */
  ".android.color2:	   #336633", /* green1 */
  ".android.color3:	   #0000FF", /* blue */
  ".android.color4:	   #CC7744", /* orange */
  ".android.color5:	   #99AA55", /* green2 */
  ".android.color6:	   #66AA33", /* green3 */
  ".android.color7:	   #FF0000", /* red */

  ".vmware.foreground:	   White",
  ".vmware.foreground2:	   Yellow",
  ".vmware.background:	   #a700a8",    /* purple */

  ".vmware-arm.foreground:       #FFFFFF",
  ".vmware-arm.foreground2:      #FFFF00",
  ".vmware-arm.background:       #555555",
  ".vmware-arm-psod.background:  #a700a8",
  ".vmware-arm-dbg.background:   #000000",
  ".vmware-arm-dbg.foreground:   #ABABAB",
  ".vmware-arm-term.background:  #555555",
  ".vmware-arm-term.foreground2: #AAAAAA",
  ".vmware-arm-term.foreground3: #FF5757",

  ".tivo.background:	   #339020",
  ".tivo.foreground:	   #B8E6BA",

  ".nintendo.background:   #F76D0A",
  ".nintendo.background2:  #085C89",
  ".nintendo.foreground:   #EEAACF",

  ".gnome.background:      #000000",
  ".gnome.foreground:      #E2E2E2",
  ".gnome.background2:     #F0F0F0",
  ".gnome.foreground2:     #2E3436",

  ".systemd.background:    #0000AA",
  ".systemd.foreground:    White",

  "*dontClearRoot:         True",

  ANALOGTV_DEFAULTS

#ifdef HAVE_XSHM_EXTENSION
  "*useSHM:                True",
#endif

  ".lowrez: false",  /* This is required on macOS */


  /* "bigFont" replaces "font" on desktop if window height >= 640. */


  /* Some of the following fonts can only display ASCII, but that's
     ok because this program only displays static ASCII text. */


  /* "Classic Console" is the MS-DOS 8x16 VGA font. */
  "*font:		Classic Console 12, Courier Bold 12",
  "*bigFont:		Classic Console 24, Courier Bold 24",
  "*fontB:		",
  "*fontC:		",

  ".win10.font:		Arial 24, Helvetica 24",
  ".win10.bigFont:	Arial 24, Helvetica 24",
  ".win10.fontB:	Arial 90, Helvetica 36",
  ".win10.fontC:	Arial 16, Helvetica 16",

  ".win10r.font:	Arial 16, Helvetica 24",
  ".win10r.bigFont:	Arial 16, Helvetica 24",
  ".win10r.fontB:	Arial 28, Helvetica 36",
  ".win10r.fontC:	Arial 50, Helvetica 16",

  ".bitlocker.font:	Arial 24, Helvetica 24",
  ".bitlocker.bigFont:	Arial 24, Helvetica 24",
  ".bitlocker.fontB:	Arial 36, Helvetica 36",
  ".bitlocker.fontC:	Arial 18, Helvetica 18",

  /* "Arial" loads "ArialMT" but "Arial Bold" does not load "Arial-BoldMT"? */
  ".ransomware.font:    Arial 12, Helvetica 12",
  ".ransomware.bigFont: Arial 12, Helvetica 12",
  ".ransomware.fontB:   Arial 8, Helvetica 8",
  ".ransomware.fontC:   Arial Bold 16, Arial-BoldMT 16, Helvetica Bold 16",

  ".macsbug.font:	Monaco 8, Courier Bold 8",
  ".macsbug.bigFont:	Monaco 14, Courier Bold 14",

  ".macx.font:		Courier Bold 10",
  ".macx.bigFont:	Courier Bold 14",

  ".macdisk.font:	Courier Bold 14",
  ".macdisk.bigFont:	Courier Bold 14",

  ".macinstall.font:	Helvetica 12, Arial 12",
  ".macinstall.bigFont:	Helvetica 24, Arial 24",

  /* "Gallant" was the original Solaris 2.x console font. */
  ".solaris.font:	Gallant12x22 12, Luxi Mono 12, Courier Bold 12",
  ".solaris.bigFont:	Gallant12x22 24, Luxi Mono 24, Courier Bold 24",

  /* "Luxi Mono" was the Red Hat console font. */
  ".linux.font:		Luxi Mono 12, Gallant12x22 12, Courier Bold 12",
  ".linux.bigFont:	Luxi Mono 24, Gallant12x22 24, Courier Bold 24",

  ".android.font:	Courier Bold 12",
  ".android.bigFont:	Courier Bold 24",

  ".tivo.font:		Helvetica Bold 16",
  ".tivo.bigFont:	Helvetica Bold 28",

  ".nintendo.font:	Classic Console 18, Courier Bold 18",
  ".nintendo.bigFont:	Classic Console 40, Courier Bold 40",

  ".gnome.font:		Helvetica Bold 13",
  ".gnome.bigFont:	Helvetica Bold 13",
  ".gnome.fontB:	Helvetica 13",

  ".systemd.font:	Classic Console 14",
  ".systemd.bigFont:	Classic Console 14",
  0
};

static const XrmOptionDescRec bsod_options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-only",		".doOnly",		XrmoptionSepArg, 0 },
  { "-debug",		".debug",		XrmoptionNoArg,  "True"  },
  { "-windows",		".doWindows",		XrmoptionNoArg,  "True"  },
  { "-no-windows",	".doWindows",		XrmoptionNoArg,  "False" },
  { "-nt",		".doNT",		XrmoptionNoArg,  "True"  },
  { "-no-nt",		".doNT",		XrmoptionNoArg,  "False" },
  { "-2k",		".doWin2K",		XrmoptionNoArg,  "True"  },
  { "-no-2k",		".doWin2K",		XrmoptionNoArg,  "False" },
  { "-win10",		".doWin10",		XrmoptionNoArg,  "True"  },
  { "-no-win10",	".doWin10",		XrmoptionNoArg,  "False" },
  { "-bitlocker",	".doBitlocker",		XrmoptionNoArg,  "True"  },
  { "-no-bitlocker",	".doBitlocker",		XrmoptionNoArg,  "False" },
  { "-ransomware",	".doRansomware",	XrmoptionNoArg,  "True"  },
  { "-no-ransomware",	".doRansomware",	XrmoptionNoArg,  "False" },
  { "-amiga",		".doAmiga",		XrmoptionNoArg,  "True"  },
  { "-no-amiga",	".doAmiga",		XrmoptionNoArg,  "False" },
  { "-mac",		".doMac",		XrmoptionNoArg,  "True"  },
  { "-no-mac",		".doMac",		XrmoptionNoArg,  "False" },
  { "-mac1",		".doMac1",		XrmoptionNoArg,  "True"  },
  { "-no-mac1",		".doMac1",		XrmoptionNoArg,  "False" },
  { "-macx",		".doMacX",		XrmoptionNoArg,  "True"  },
  { "-no-macx",		".doMacX",		XrmoptionNoArg,  "False" },
  { "-atari",		".doAtari",		XrmoptionNoArg,  "True"  },
  { "-no-atari",	".doAtari",		XrmoptionNoArg,  "False" },
  { "-macsbug",		".doMacsBug",		XrmoptionNoArg,  "True"  },
  { "-no-macsbug",	".doMacsBug",		XrmoptionNoArg,  "False" },
  { "-apple2",		".doApple2",		XrmoptionNoArg,  "True"  },
  { "-no-apple2",	".doApple2",		XrmoptionNoArg,  "False" },
  { "-sco",		".doSCO",		XrmoptionNoArg,  "True"  },
  { "-no-sco",		".doSCO",		XrmoptionNoArg,  "False" },
  { "-hvx",		".doHVX",		XrmoptionNoArg,  "True"  },
  { "-no-hvx",		".doHVX",		XrmoptionNoArg,  "False" },
  { "-bsd",		".doBSD",		XrmoptionNoArg,  "True"  },
  { "-no-bsd",		".doBSD",		XrmoptionNoArg,  "False" },
  { "-linux",		".doLinux",		XrmoptionNoArg,  "True"  },
  { "-no-linux",	".doLinux",		XrmoptionNoArg,  "False" },
  { "-hppalinux",	".doHPPALinux",		XrmoptionNoArg,  "True"  },
  { "-no-hppalinux",	".doHPPALinux",		XrmoptionNoArg,  "False" },
  { "-sparclinux",	".doSparcLinux",	XrmoptionNoArg,  "True"  },
  { "-no-sparclinux",	".doSparcLinux",	XrmoptionNoArg,  "False" },
  { "-blitdamage",	".doBlitDamage",	XrmoptionNoArg,  "True"  },
  { "-no-blitdamage",	".doBlitDamage",	XrmoptionNoArg,  "False" },
  { "-nvidia",		".doNvidia",		XrmoptionNoArg,  "True"  },
  { "-no-nvidia",	".doNvidia",		XrmoptionNoArg,  "False" },
  { "-solaris",		".doSolaris",		XrmoptionNoArg,  "True"  },
  { "-no-solaris",	".doSolaris",		XrmoptionNoArg,  "False" },
  { "-hpux",		".doHPUX",		XrmoptionNoArg,  "True"  },
  { "-no-hpux",		".doHPUX",		XrmoptionNoArg,  "False" },
  { "-os390",		".doOS390",		XrmoptionNoArg,  "True"  },
  { "-no-os390",	".doOS390",		XrmoptionNoArg,  "False" },
  { "-tru64",		".doHPUX",		XrmoptionNoArg,  "True"  },
  { "-no-tru64",	".doTru64",		XrmoptionNoArg,  "False" },
  { "-vms",		".doVMS",		XrmoptionNoArg,  "True"  },
  { "-no-vms",		".doVMS",		XrmoptionNoArg,  "False" },
  { "-msdos",		".doMSDOS",		XrmoptionNoArg,  "True"  },
  { "-no-msdos",	".doMSDOS",		XrmoptionNoArg,  "False" },
  { "-os2",		".doOS2",		XrmoptionNoArg,  "True"  },
  { "-no-os2",		".doOS2",		XrmoptionNoArg,  "False" },
  { "-atm",		".doATM",		XrmoptionNoArg,  "True"  },
  { "-no-atm",		".doATM",		XrmoptionNoArg,  "False" },
  { "-glados",		".doGLaDOS",		XrmoptionNoArg,  "True"  },
  { "-no-glados",	".doGLaDOS",		XrmoptionNoArg,  "False" },
  { "-android",		".doAndroid",		XrmoptionNoArg,  "True"  },
  { "-no-android",	".doAndroid",		XrmoptionNoArg,  "False" },
  { "-vmware",		".doVMware",		XrmoptionNoArg,  "True"  },
  { "-no-vmware",	".doVMware",		XrmoptionNoArg,  "False" },
  { "-vmwarearm",	".doVMwareArm",		XrmoptionNoArg,  "True"  },
  { "-no-vmwarearm",	".doVMwareArm",		XrmoptionNoArg,  "False" },
  { "-encom",		".doEncom",		XrmoptionNoArg,  "True"  },
  { "-no-encom",	".doEncom",		XrmoptionNoArg,  "False" },
  { "-dvd",		".doDVD",		XrmoptionNoArg,  "True"  },
  { "-no-dvd",		".doDVD",		XrmoptionNoArg,  "False" },
  { "-tivo",		".doTivo",		XrmoptionNoArg,  "True"  },
  { "-no-tivo",		".doTivo",		XrmoptionNoArg,  "False" },
  { "-nintendo",	".doNintendo",		XrmoptionNoArg,  "True"  },
  { "-no-nintendo",	".doNintendo",		XrmoptionNoArg,  "False" },
  { "-gnome",		".doGnome",		XrmoptionNoArg,  "True"  },
  { "-no-gnome",	".doGnome",		XrmoptionNoArg,  "False" },
  { "-systemd",		".doSystemd",		XrmoptionNoArg,  "True"  },
  { "-no-systemd",	".doSystemd",		XrmoptionNoArg,  "False" },
  ANALOGTV_OPTIONS
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("BSOD", bsod)
