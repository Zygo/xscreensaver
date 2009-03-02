/* font-ximage.c --- renders text to an XImage for use with OpenGL.
 * xscreensaver, Copyright (c) 2001, 2003 Jamie Zawinski <jwz@jwz.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_COCOA
# include "jwxyz.h"
# include <OpenGL/gl.h>
#else  /* !HAVE_COCOA */
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <GL/gl.h>	/* only for GLfloat */
#endif /* !HAVE_COCOA */

extern char *progname;

#include "font-ximage.h"

#undef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#if 0
static Bool
bigendian (void)
{
  union { int i; char c[sizeof(int)]; } u;
  u.i = 1;
  return !u.c[0];
}
#endif

/* return the next larger power of 2. */
static int
to_pow2 (int i)
{
  static const unsigned int pow2[] = { 
    1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024,
    2048, 4096, 8192, 16384, 32768, 65536 };
  int j;
  for (j = 0; j < countof(pow2); j++)
    if (pow2[j] >= i) return pow2[j];
  abort();  /* too big! */
}


/* Returns an XImage structure containing the string rendered in the font.
   This XImage will be 32 bits per pixel, 8 each per R, G, and B, with the
   extra byte set to 0xFF.

   Foregroune and background are GL-style color specifiers: 4 floats from
   0.0-1.0.
 */
XImage *
text_to_ximage (Screen *screen, Visual *visual,
                const char *font,
                const char *text_lines,
                GLfloat *texture_fg,
                GLfloat *texture_bg)
{
  Display *dpy = DisplayOfScreen (screen);
  int width, height;
  XFontStruct *f;
  Pixmap bitmap;

  f = XLoadQueryFont(dpy, font);
  if (!f)
    {
      f = XLoadQueryFont(dpy, "fixed");
      if (f)
        fprintf (stderr, "%s: unable to load font \"%s\"; using \"fixed\".\n",
                 progname, font);
      else
        {
          fprintf (stderr, "%s: unable to load fonts \"%s\" or \"fixed\"!\n",
                   progname, font);
          exit (1);
        }
    }

  /* Parse the text, and render it to `bitmap'
   */
  {
    char *text, *text2, *line, *token;
    int lines;
    XCharStruct overall;
    XGCValues gcv;
    GC gc;

    int margin = 2;
    int fg = 1;
    int bg = 0;
    int xoff, yoff;

    text  = strdup (text_lines);
    while (*text &&
           (text[strlen(text)-1] == '\r' ||
            text[strlen(text)-1] == '\n'))
      text[strlen(text)-1] = 0;

    text2 = strdup (text);

    memset(&overall, 0, sizeof(overall));
    token = text;
    lines = 0;
    while ((line = strtok (token, "\r\n")))
      {
        XCharStruct o2;
        int ascent, descent, direction;
        token = 0;
        XTextExtents (f, line, strlen(line),
                      &direction, &ascent, &descent, &o2);
        overall.lbearing = MAX(overall.lbearing, o2.lbearing);
        overall.rbearing = MAX(overall.rbearing, o2.rbearing);
        lines++;
      }

    width = overall.lbearing + overall.rbearing + margin + margin + 1;
    height = ((f->ascent + f->descent) * lines) + margin + margin;

    /* GL texture sizes must be powers of two. */
    {
      int w2 = to_pow2(width);
      int h2 = to_pow2(height);
      xoff = (w2 - width)  / 2;
      yoff = (h2 - height) / 2;
      width  = w2;
      height = h2;
    }

    bitmap = XCreatePixmap(dpy, RootWindowOfScreen (screen), width, height, 1);

    gcv.font = f->fid;
    gcv.foreground = bg;
    gc = XCreateGC (dpy, bitmap, (GCFont | GCForeground), &gcv);
    XFillRectangle(dpy, bitmap, gc, 0, 0, width, height);
    XSetForeground(dpy, gc, fg);

    token = text2;
    lines = 0;
    while ((line = strtok(token, "\r\n")))
      {
        XCharStruct o2;
        int ascent, descent, direction, xoff2;
        token = 0;

        XTextExtents(f, line, strlen(line),
                     &direction, &ascent, &descent, &o2);
        xoff2 = (xoff +
                 ((overall.lbearing + overall.rbearing) -
                  (o2.lbearing + o2.rbearing)) / 2);

        XDrawString(dpy, bitmap, gc,
                    overall.lbearing + margin + xoff,
                    ((f->ascent * (lines + 1)) +
                     (f->descent * lines) +
                     margin +
                     yoff),
                    line, strlen(line));
        lines++;
      }
    free(text2);

    XUnloadFont(dpy, f->fid);
    XFree((XPointer) f);
    XFreeGC(dpy, gc);
  }

  /* Convert the server-side Pixmap to a client-side GL-ordered XImage.
   */
  {
    XImage *ximage1, *ximage2;
    unsigned long fg, bg;
    int x, y;

    ximage1 = XGetImage (dpy, bitmap, 0, 0, width, height, ~0L, ZPixmap);
    XFreePixmap(dpy, bitmap);
    ximage2 = XCreateImage (dpy, visual, 32, ZPixmap, 0, 0,
                            width, height, 32, 0);

    ximage2->data = (char *) malloc (height * ximage2->bytes_per_line);

    /* Translate the 1-bit image to a deep image:
       first figure out what the colors are.
     */
    {
      int rpos, gpos, bpos, apos;  /* bitfield positions */

      /* Note that unlike X, which is endianness-agnostic (since any XImage
         can have its own specific bit ordering, with the server reversing
         things as necessary) OpenGL pretends everything is client-side, so
         we need to pack things in the right order for the client machine.
       */
#if 0
    /* #### Cherub says that the little-endian case must be taken on MacOSX,
            or else the colors/alpha are the wrong way around.  How can
            that be the case?
     */
      if (bigendian())
        rpos = 24, gpos = 16, bpos =  8, apos =  0;
      else
#endif
        rpos =  0, gpos =  8, bpos = 16, apos = 24;

      fg = (((unsigned long) (texture_fg[0] * 255.0) << rpos) |
            ((unsigned long) (texture_fg[1] * 255.0) << gpos) |
            ((unsigned long) (texture_fg[2] * 255.0) << bpos) |
            ((unsigned long) (texture_fg[3] * 255.0) << apos));
      bg = (((unsigned long) (texture_bg[0] * 255.0) << rpos) |
            ((unsigned long) (texture_bg[1] * 255.0) << gpos) |
            ((unsigned long) (texture_bg[2] * 255.0) << bpos) |
            ((unsigned long) (texture_bg[3] * 255.0) << apos));
    }

    for (y = 0; y < height; y++)
      {
	int y2 = (height-1-y); /* Texture maps are upside down. */
	for (x = 0; x < width; x++)
          XPutPixel (ximage2, x, y, 
                     XGetPixel (ximage1, x, y2) ? fg : bg);
      }

    XDestroyImage (ximage1);

#if 0
    for (y = 0; y < height; y++)
      {
	int y2 = (height-1-y); /* Texture maps are upside down. */
	for (x = 0; x < width; x++)
          fputc ((XGetPixel (ximage2, x, y2) == fg ? '#' : ' '), stdout);
        fputc ('\n', stdout);
      }
    fputc ('\n', stdout);
#endif /* 0 */

    return ximage2;
  }
}
