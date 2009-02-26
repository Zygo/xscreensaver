/* xscreensaver, Copyright (c) 2003, 2005 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * fontglide -- reads text from a subprocess and puts it on the screen using
 * large characters that glide in from the edges, assemble, then disperse.
 * Requires a system with scalable fonts.  (X's font handing sucks.  A lot.)
 */

#include <math.h>
#include "screenhack.h"
#include <X11/Intrinsic.h>

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
#include "xdbe.h"
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

extern XtAppContext app;


typedef struct {
  char *text;
  int x, y, width, height;
  int ascent, lbearing, rbearing;

  int nticks, tick;
  int start_x,  start_y;
  int target_x, target_y;
  Pixmap pixmap, mask;
} word;


typedef struct {
  int id;
  XColor fg;
  XColor bg;
  Bool dark_p;
  Bool move_chars_p;
  int width;

  char *font_name;
  XFontStruct *font;

  GC fg_gc;

  int nwords;
  word **words;

  enum { IN, PAUSE, OUT } anim_state;
  enum { LEFT, CENTER, RIGHT } alignment;
  int pause_tick;

} sentence;


typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;

  Pixmap b, ba;	/* double-buffer to reduce flicker */
  GC bg_gc;

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  XdbeBackBuffer backb;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

  Bool dbuf;            /* Whether we're using double buffering. */
  Bool dbeclear_p;      /* ? */

  int border_width;     /* size of the font outline */
  char *charset;        /* registry and encoding for font lookups */
  double speed;		/* frame rate multiplier */
  double linger;	/* multiplier for how long to leave words on screen */
  Bool trails_p;
  Bool debug_p;
  enum { PAGE, SCROLL } mode;

  char *font_override;  /* if -font was specified on the cmd line */

  FILE *pipe;
  XtInputId pipe_id;
  Time subproc_relaunch_delay;
  Bool input_available_p;

  char buf [40];	/* this only needs to be as big as one "word". */
  int buf_tail;

  int nsentences;
  sentence **sentences;
  Bool spawn_p;		/* whether it is time to create a new sentence */
  int latest_sentence;

} state;


static void launch_text_generator (state *);
static void drain_input (state *s);


/* Finds the set of scalable fonts on the system; picks one;
   and loads that font in a random pixel size.
   Returns False if something went wrong.
 */
static Bool
pick_font_1 (state *s, sentence *se)
{
  char pattern[1024];
  char **names = 0;
  char **names2 = 0;
  XFontStruct *info = 0;
  int count = 0, count2 = 0;
  int i;
  Bool ok = False;

  if (se->font)
    {
      XFreeFont (s->dpy, se->font);
      free (se->font_name);
      se->font = 0;
      se->font_name = 0;
    }

  if (s->font_override)
    sprintf (pattern, "%.200s", s->font_override);
  else
    sprintf (pattern, "-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s",
             "*",         /* foundry */
             "*",         /* family */
             "*",         /* weight */
             "*",         /* slant */
             "*",         /* swidth */
             "*",         /* adstyle */
             "0",         /* pixel size */
             "0",         /* point size */
             "0",         /* resolution x */
             "0",         /* resolution y */
             "p",         /* spacing */
             "0",         /* avg width */
             s->charset); /* registry + encoding */

  names = XListFonts (s->dpy, pattern, 1000, &count);

  if (count <= 0)
    {
      if (s->font_override)
        fprintf (stderr, "%s: -font option bogus: %s\n", progname, pattern);
      else
        fprintf (stderr, "%s: no scalable fonts found!  (pattern: %s)\n",
                 progname, pattern);
      exit (1);
    }

  i = random() % count;

  names2 = XListFontsWithInfo (s->dpy, names[i], 1000, &count2, &info);
  if (count2 <= 0)
    {
      fprintf (stderr, "%s: pattern %s\n"
                "     gave unusable %s\n\n",
               progname, pattern, names[i]);
      goto FAIL;
    }

  {
    XFontStruct *font = &info[0];
    unsigned long value = 0;
    char *foundry=0, *family=0, *weight=0, *slant=0, *setwidth=0, *add_style=0;
    unsigned long pixel=0, point=0, res_x=0, res_y=0;
    char *spacing=0;
    unsigned long avg_width=0;
    char *registry=0, *encoding=0;
    Atom a;
    char *bogus = "\"?\"";

# define STR(ATOM,VAR)					\
  bogus = (ATOM);					\
  a = XInternAtom (s->dpy, (ATOM), False);		\
  if (XGetFontProperty (font, a, &value))		\
    VAR = XGetAtomName (s->dpy, value);			\
  else							\
    goto FAIL2

# define INT(ATOM,VAR)					\
  bogus = (ATOM);					\
  a = XInternAtom (s->dpy, (ATOM), False);		\
  if (!XGetFontProperty (font, a, &VAR) ||		\
      VAR > 9999)					\
    goto FAIL2

    STR ("FOUNDRY",          foundry);
    STR ("FAMILY_NAME",      family);
    STR ("WEIGHT_NAME",      weight);
    STR ("SLANT",            slant);
    STR ("SETWIDTH_NAME",    setwidth);
    STR ("ADD_STYLE_NAME",   add_style);
    INT ("PIXEL_SIZE",       pixel);
    INT ("POINT_SIZE",       point);
    INT ("RESOLUTION_X",     res_x);
    INT ("RESOLUTION_Y",     res_y);
    STR ("SPACING",          spacing);
    INT ("AVERAGE_WIDTH",    avg_width);
    STR ("CHARSET_REGISTRY", registry);
    STR ("CHARSET_ENCODING", encoding);

#undef INT
#undef STR

    {
      double scale = s->xgwa.height / 1024.0;  /* shrink for small windows */
      int min, max, r;

      min = scale * 24;
      max = scale * 260;

      if (min < 10) min = 10;
      if (max < 30) max = 30;

      r = ((max-min)/3)+1;

      pixel = min + ((random() % r) + (random() % r) + (random() % r));

      if (s->mode == SCROLL)  /* scroll mode likes bigger fonts */
        pixel *= 1.5;
    }

#if 0
    /* Occasionally change the aspect ratio of the font, by increasing
       either the X or Y resolution (while leaving the other alone.)

       #### Looks like this trick doesn't really work that well: the
            metrics of the individual characters are ok, but the
            overall font ascent comes out wrong (unscaled.)
     */
    if (! (random() % 8))
      {
        double n = 2.5 / 3;
        double scale = 1 + (frand(n) + frand(n) + frand(n));
        if (random() % 2)
          res_x *= scale;
        else
          res_y *= scale;
      }
# endif

    sprintf (pattern,
             "-%s-%s-%s-%s-%s-%s-%ld-%s-%ld-%ld-%s-%s-%s-%s",
             foundry, family, weight, slant, setwidth, add_style,
             pixel, "*", /* point, */
             res_x, res_y, spacing,
             "*", /* avg_width */
             registry, encoding);
    ok = True;

  FAIL2:
    if (!ok)
      fprintf (stderr, "%s: font has bogus %s property: %s\n",
               progname, bogus, names[i]);

    if (foundry)   XFree (foundry);
    if (family)    XFree (family);
    if (weight)    XFree (weight);
    if (slant)     XFree (slant);
    if (setwidth)  XFree (setwidth);
    if (add_style) XFree (add_style);
    if (spacing)   XFree (spacing);
    if (registry)  XFree (registry);
    if (encoding)  XFree (encoding);
  }

 FAIL: 

  XFreeFontInfo (names2, info, count2);
  XFreeFontNames (names);

  if (! ok) return False;

  se->font = XLoadQueryFont (s->dpy, pattern);
  if (! se->font)
    {
      if (s->debug_p)
        fprintf (stderr, "%s: unable to load font %s\n",
                 progname, pattern);
      return False;
    }

  if (se->font->min_bounds.width == se->font->max_bounds.width &&
      !s->font_override)
    {
      /* This is to weed out
         "-urw-nimbus mono l-medium-o-normal--*-*-*-*-p-*-iso8859-1" and
         "-urw-courier-medium-r-normal--*-*-*-*-p-*-iso8859-1".
         We asked for only proportional fonts, but this fixed-width font
         shows up anyway -- but it has goofy metrics (see below) so it
         looks terrible anyway.
       */
      if (s->debug_p)
        fprintf (stderr,
                 "%s: skipping bogus monospace non-charcell font: %s\n",
                 progname, pattern);
      return False;
    }

  if (s->debug_p) 
    fprintf(stderr, "%s: %s\n", progname, pattern);

  se->font_name = strdup (pattern);
  XSetFont (s->dpy, se->fg_gc, se->font->fid);
  return True;
}


/* Finds the set of scalable fonts on the system; picks one;
   and loads that font in a random pixel size.
 */
static void
pick_font (state *s, sentence *se)
{
  int i;
  for (i = 0; i < 20; i++)
    if (pick_font_1 (s, se))
      return;
  fprintf (stderr, "%s: too many font-loading failures: giving up!\n", progname);
  exit (1);
}


static char *unread_word_text = 0;

/* Returns a newly-allocated string with one word in it, or NULL if there
   is no complete word available.
 */
static char *
get_word_text (state *s)
{
  char *start = s->buf;
  char *end;
  char *result = 0;
  int lfs = 0;

  if (unread_word_text)
    {
      char *s = unread_word_text;
      unread_word_text = 0;
      return s;
    }

  /* Skip over whitespace at the beginning of the buffer,
     and count up how many linebreaks we see while doing so.
   */
  while (*start &&
         (*start == ' ' ||
          *start == '\t' ||
          *start == '\r' ||
          *start == '\n'))
    {
      if (*start == '\n' || (*start == '\r' && start[1] != '\n'))
        lfs++;
      start++;
    }

  end = start;

  /* If we saw a blank line, then return NULL (treat it as a temporary "eof",
     to trigger a sentence break here.) */
  if (lfs >= 2)
    goto DONE;

  /* Skip forward to the end of this word (find next whitespace.) */
  while (*end &&
         (! (*end == ' ' ||
             *end == '\t' ||
             *end == '\r' ||
             *end == '\n')))
    end++;

  /* If we have a word, allocate a string for it */
  if (end > start)
    {
      result = malloc ((end - start) + 1);
      strncpy (result, start, (end-start));
      result [end-start] = 0;
    }

 DONE:

  /* Make room in the buffer by compressing out any bytes we've processed.
   */
  if (end > s->buf)
    {
      int n = end - s->buf;
      memmove (s->buf, end, sizeof(s->buf) - n);
      s->buf_tail -= n;
    }

  /* See if there is more to be read, now that there's room in the buffer. */
  drain_input (s);

  return result;
}


/* Gets some random text, and creates a "word" object from it.
 */
static word *
new_word (state *s, sentence *se, char *txt, Bool alloc_p)
{
  word *w;
  XCharStruct overall;
  int dir, ascent, descent;
  int bw = s->border_width;

  if (!txt)
    return 0;

  w = (word *) calloc (1, sizeof(*w));
  XTextExtents (se->font, txt, strlen(txt), &dir, &ascent, &descent, &overall);

  w->width    = overall.rbearing - overall.lbearing + bw + bw;
  w->height   = overall.ascent   + overall.descent  + bw + bw;
  w->ascent   = overall.ascent   + bw;
  w->lbearing = overall.lbearing - bw;
  w->rbearing = overall.width    + bw;

# if 0
  /* The metrics on some fonts are strange -- e.g.,
     "-urw-nimbus mono l-medium-o-normal--*-*-*-*-p-*-iso8859-1" and
     "-urw-courier-medium-r-normal--*-*-*-*-p-*-iso8859-1" both have
     an rbearing so wide that it looks like there are two spaces after
     each letter.  If this character says it has an rbearing that is to
     the right of its ink, ignore that.

     #### Of course, this hack only helps when we're in `move_chars_p' mode
          and drawing a char at a time -- when we draw the whole word at once,
          XDrawString believes the bogus metrics and spaces the font out
          crazily anyway.

     Sigh, this causes some text to mis-render in, e.g.,
     "-adobe-utopia-medium-i-normal--114-*-100-100-p-*-iso8859-1"
     (in "ux", we need the rbearing on "r" or we get too much overlap.)
   */
  if (w->rbearing > w->width)
    w->rbearing = w->width;
# endif /* 0 */

  if (s->mode == SCROLL && !alloc_p) abort();

  if (alloc_p)
    {
      int i, j;
      XGCValues gcv;
      GC gc0, gc1;

      w->pixmap = XCreatePixmap (s->dpy, s->b, w->width, w->height, 1L);
      w->mask   = XCreatePixmap (s->dpy, s->b, w->width, w->height, 1L);

      gcv.font = se->font->fid;
      gcv.foreground = 0L;
      gcv.background = 1L;
      gc0 = XCreateGC (s->dpy, w->pixmap, GCFont|GCForeground|GCBackground,
                       &gcv);
      gcv.foreground = 1L;
      gcv.background = 0L;
      gc1 = XCreateGC (s->dpy, w->pixmap, GCFont|GCForeground|GCBackground,
                       &gcv);

      XFillRectangle (s->dpy, w->mask,   gc0, 0, 0, w->width, w->height);
      XFillRectangle (s->dpy, w->pixmap, gc0, 0, 0, w->width, w->height);

      if (s->debug_p)
        {
          /* bounding box (behind the characters) */
          XDrawRectangle (s->dpy, w->pixmap, (se->dark_p ? gc0 : gc1),
                          0, 0, w->width-1, w->height-1);
          XDrawRectangle (s->dpy, w->mask,   gc1,
                          0, 0, w->width-1, w->height-1);
        }

      /* Draw foreground text */
      XDrawString (s->dpy, w->pixmap, gc1, -w->lbearing, w->ascent,
                   txt, strlen(txt));

      /* Cheesy hack to draw a border */
      /* (I should be able to do this in i*2 time instead of i*i time,
         but I can't get it right, so fuck it.) */
      XSetFunction (s->dpy, gc1, GXor);
      for (i = -bw; i <= bw; i++)
        for (j = -bw; j <= bw; j++)
          XCopyArea (s->dpy, w->pixmap, w->mask, gc1,
                     0, 0, w->width, w->height,
                     i, j);

      if (s->debug_p)
        {
          XSetFunction (s->dpy, gc1, GXset);
          if (w->ascent != w->height)
            {
              /* baseline (on top of the characters) */
              XDrawLine (s->dpy, w->pixmap, (se->dark_p ? gc0 : gc1),
                         0, w->ascent, w->width-1, w->ascent);
              XDrawLine (s->dpy, w->mask,   gc1,
                         0, w->ascent, w->width-1, w->ascent);
            }

          if (w->lbearing != 0)
            {
              /* left edge of charcell */
              XDrawLine (s->dpy, w->pixmap, (se->dark_p ? gc0 : gc1),
                         w->lbearing, 0, w->lbearing, w->height-1);
              XDrawLine (s->dpy, w->mask,   gc1,
                         w->lbearing, 0, w->lbearing, w->height-1);
            }

          if (w->rbearing != w->width)
            {
              /* right edge of charcell */
              XDrawLine (s->dpy, w->pixmap, (se->dark_p ? gc0 : gc1),
                         w->rbearing, 0, w->rbearing, w->height-1);
              XDrawLine (s->dpy, w->mask,   gc1,
                         w->rbearing, 0, w->rbearing, w->height-1);
            }
        }

      XFreeGC (s->dpy, gc0);
      XFreeGC (s->dpy, gc1);
    }

  w->text = txt;
  return w;
}


static void
free_word (state *s, word *w)
{
  if (w->text)   free (w->text);
  if (w->pixmap) XFreePixmap (s->dpy, w->pixmap);
  if (w->mask)   XFreePixmap (s->dpy, w->mask);
}


static sentence *
new_sentence (state *s)
{
  static int id = 0;
  XGCValues gcv;
  sentence *se = (sentence *) calloc (1, sizeof (*se));
  se->fg_gc = XCreateGC (s->dpy, s->b, 0, &gcv);
  se->anim_state = IN;
  se->id = ++id;
  return se;
}


static void
free_sentence (state *s, sentence *se)
{
  int i;
  for (i = 0; i < se->nwords; i++)
    free_word (s, se->words[i]);
  if (se->words) free (se->words);

  if (se->fg.flags)
    XFreeColors (s->dpy, s->xgwa.colormap, &se->fg.pixel, 1, 0);
  if (se->bg.flags)
    XFreeColors (s->dpy, s->xgwa.colormap, &se->bg.pixel, 1, 0);

  if (se->font_name) free (se->font_name);
  if (se->font) XFreeFont (s->dpy, se->font);
  if (se->fg_gc) XFreeGC (s->dpy, se->fg_gc);

  free (se);
}


/* free the word, and put its text back at the front of the input queue,
   to be read next time. */
static void
unread_word (state *s, word *w)
{
  if (unread_word_text)
    abort();
  unread_word_text = w->text;
  w->text = 0;
  free_word (s, w);
}


/* Divide each of the words in the sentence into one character words,
   without changing the positions of those characters.
 */
static void
split_words (state *s, sentence *se)
{
  word **words2;
  int nwords2 = 0;
  int i, j;
  for (i = 0; i < se->nwords; i++)
    nwords2 += strlen (se->words[i]->text);

  words2 = (word **) calloc (nwords2, sizeof(*words2));

  for (i = 0, j = 0; i < se->nwords; i++)
    {
      word *ow = se->words[i];
      int L = strlen (ow->text);
      int k;

      int x  = ow->x;
      int y  = ow->y;
      int sx = ow->start_x;
      int sy = ow->start_y;
      int tx = ow->target_x;
      int ty = ow->target_y;

      for (k = 0; k < L; k++)
        {
          char *t2 = malloc (2);
          word *w2;
          int xoff, yoff;

          t2[0] = ow->text[k];
          t2[1] = 0;
          w2 = new_word (s, se, t2, True);
          words2[j++] = w2;

          xoff = (w2->lbearing - ow->lbearing);
          yoff = (ow->ascent - w2->ascent);

          w2->x        = x  + xoff;
          w2->y        = y  + yoff;
          w2->start_x  = sx + xoff;
          w2->start_y  = sy + yoff;
          w2->target_x = tx + xoff;
          w2->target_y = ty + yoff;

          x  += w2->rbearing;
          sx += w2->rbearing;
          tx += w2->rbearing;
        }

      free_word (s, ow);
      se->words[i] = 0;
    }
  free (se->words);

  se->words = words2;
  se->nwords = nwords2;
}


/* Set the source or destination position of the words to be somewhere
   off screen.
 */
static void
scatter_sentence (state *s, sentence *se)
{
  int i = 0;
  int off = 100;

  int flock_p = ((random() % 4) == 0);
  int mode = (flock_p ? (random() % 12) : 0);

  for (i = 0; i < se->nwords; i++)
    {
      word *w = se->words[i];
      int x, y;
      int r = (flock_p ? mode : (random() % 4));
      switch (r)
        {
          /* random positions on the edges */

        case 0:
          x = -off - w->width;
          y = random() % s->xgwa.height;
          break;
        case 1:
          x = off + s->xgwa.width;
          y = random() % s->xgwa.height;
          break;
        case 2:
          x = random() % s->xgwa.width;
          y = -off - w->height;
          break;
        case 3:
          x = random() % s->xgwa.width;
          y = off + s->xgwa.height;
          break;

          /* straight towards the edges */

        case 4:
          x = -off - w->width;
          y = w->target_y;
          break;
        case 5:
          x = off + s->xgwa.width;
          y = w->target_y;
          break;
        case 6:
          x = w->target_x;
          y = -off - w->height;
          break;
        case 7:
          x = w->target_x;
          y = off + s->xgwa.height;
          break;

          /* corners */

        case 8:
          x = -off - w->width;
          y = -off - w->height;
          break;
        case 9:
          x = -off - w->width;
          y =  off + s->xgwa.height;
          break;
        case 10:
          x =  off + s->xgwa.width;
          y =  off + s->xgwa.height;
          break;
        case 11:
          x =  off + s->xgwa.width;
          y = -off - w->height;
          break;

        default:
          abort();
          break;
        }

      if (se->anim_state == IN)
        {
          w->start_x = x;
          w->start_y = y;
        }
      else
        {
          w->start_x = w->x;
          w->start_y = w->y;
          w->target_x = x;
          w->target_y = y;
        }

      w->nticks = ((100 + ((random() % 140) +
                           (random() % 140) +
                           (random() % 140)))
                   / s->speed);
      if (w->nticks < 2)
        w->nticks = 2;
      w->tick = 0;
    }
}


/* Set the source position of the words to be off the right side,
   and the destination to be off the left side.
 */
static void
aim_sentence (state *s, sentence *se)
{
  int i = 0;
  int nticks;
  int yoff = 0;

  if (se->nwords <= 0) abort();

  /* Have the sentence shift up or down a little bit; not too far, and
     never let it fall off the top or bottom of the screen before its
     last character has reached the left edge.
   */
  for (i = 0; i < 10; i++)
    {
      int ty = random() % (s->xgwa.height - se->words[0]->ascent);
      yoff = ty - se->words[0]->target_y;
      if (yoff < s->xgwa.height/3)  /* this one is ok */
        break;
    }

  for (i = 0; i < se->nwords; i++)
    {
      word *w = se->words[i];
      w->start_x   = w->target_x + s->xgwa.width;
      w->target_x -= se->width;
      w->start_y   = w->target_y;
      w->target_y += yoff;
    }

  nticks = ((se->words[0]->start_x - se->words[0]->target_x)
            / (s->speed * 10));
  nticks *= (frand(0.9) + frand(0.9) + frand(0.9));

  if (nticks < 2)
    nticks = 2;

  for (i = 0; i < se->nwords; i++)
    {
      word *w = se->words[i];
      w->nticks = nticks;
      w->tick = 0;
    }
}


/* Randomize the order of the words in the list (since that changes
   which ones are "on top".)
 */
static void
shuffle_words (state *s, sentence *se)
{
  int i;
  for (i = 0; i < se->nwords-1; i++)
    {
      int j = i + (random() % (se->nwords - i));
      word *swap = se->words[i];
      se->words[i] = se->words[j];
      se->words[j] = swap;
    }
}


/* qsort comparitor */
static int
cmp_sentences (const void *aa, const void *bb)
{
  const sentence *a = *(sentence **) aa;
  const sentence *b = *(sentence **) bb;
  return ((a ? a->id : 999999) - (b ? b->id : 999999));
}


/* Sort the sentences by id, so that sentences added later are on top.
 */
static void
sort_sentences (state *s)
{
  qsort (s->sentences, s->nsentences, sizeof(*s->sentences), cmp_sentences);
}


/* Re-pick the colors of the text and border
 */
static void
recolor (state *s, sentence *se)
{
  if (se->fg.flags)
    XFreeColors (s->dpy, s->xgwa.colormap, &se->fg.pixel, 1, 0);
  if (se->bg.flags)
    XFreeColors (s->dpy, s->xgwa.colormap, &se->bg.pixel, 1, 0);

  se->fg.flags  = DoRed|DoGreen|DoBlue;
  se->bg.flags  = DoRed|DoGreen|DoBlue;

  switch (random() % 2)
    {
    case 0:   /* bright fg, dim bg */
      se->fg.red    = (random() % 0x8888) + 0x8888;
      se->fg.green  = (random() % 0x8888) + 0x8888;
      se->fg.blue   = (random() % 0x8888) + 0x8888;
      se->bg.red    = (random() % 0x5555);
      se->bg.green  = (random() % 0x5555);
      se->bg.blue   = (random() % 0x5555);
      break;

    case 1:   /* bright bg, dim fg */
      se->fg.red    = (random() % 0x4444);
      se->fg.green  = (random() % 0x4444);
      se->fg.blue   = (random() % 0x4444);
      se->bg.red    = (random() % 0x4444) + 0xCCCC;
      se->bg.green  = (random() % 0x4444) + 0xCCCC;
      se->bg.blue   = (random() % 0x4444) + 0xCCCC;
      break;

    default:
      abort();
      break;
    }

  if (s->debug_p)
    se->dark_p = (se->fg.red*2 + se->fg.green*3 + se->fg.blue <
                  se->bg.red*2 + se->bg.green*3 + se->bg.blue);

  if (XAllocColor (s->dpy, s->xgwa.colormap, &se->fg))
    XSetForeground (s->dpy, se->fg_gc, se->fg.pixel);
  if (XAllocColor (s->dpy, s->xgwa.colormap, &se->bg))
    XSetBackground (s->dpy, se->fg_gc, se->bg.pixel);
}


static void
align_line (state *s, sentence *se, int line_start, int x, int right)
{
  int off, j;
  switch (se->alignment)
    {
    case LEFT:   off = 0;               break;
    case CENTER: off = (right - x) / 2; break;
    case RIGHT:  off = (right - x);     break;
    default:     abort();               break;
    }

  if (off != 0)
    for (j = line_start; j < se->nwords; j++)
      se->words[j]->target_x += off;
}


/* Fill the sentence with new words: in "page" mode, fills the page
   with text; in "scroll" mode, just makes one long horizontal sentence.
   The sentence might have *no* words in it, if no text is currently
   available.
 */
static void
populate_sentence (state *s, sentence *se)
{
  int i = 0;
  int left, right, top, x, y;
  int space = 0;
  int line_start = 0;
  Bool done = False;

  int array_size = 100;

  se->move_chars_p = (s->mode == SCROLL ? False :
                      (random() % 3) ? False : True);
  se->alignment = (random() % 3);

  recolor (s, se);

  if (se->words)
    {
      for (i = 0; i < se->nwords; i++)
        free_word (s, se->words[i]);
      free (se->words);
    }

  se->words = (word **) calloc (array_size, sizeof(*se->words));
  se->nwords = 0;

  switch (s->mode)
    {
    case PAGE:
      left  = random() % (s->xgwa.width / 3);
      right = s->xgwa.width - (random() % (s->xgwa.width / 3));
      top = random() % (s->xgwa.height * 2 / 3);
      break;
    case SCROLL:
      left = 0;
      right = s->xgwa.width;
      top = random() % s->xgwa.height;
      break;
    default:
      abort();
      break;
    }

  x = left;
  y = top;

  while (!done)
    {
      char *txt = get_word_text (s);
      word *w;
      if (!txt)
        {
          if (se->nwords == 0)
            return;		/* If the stream is empty, bail. */
          else
            break;		/* If EOF after some words, end of sentence. */
        }

      if (! se->font)           /* Got a word: need a font now */
        {
          pick_font (s, se);
          if (y < se->font->ascent)
            y += se->font->ascent;
          space = XTextWidth (se->font, " ", 1);
        }

      w = new_word (s, se, txt, !se->move_chars_p);

      /* If we have a few words, let punctuation terminate the sentence:
         stop gathering more words if the last word ends in a period, etc. */
      if (se->nwords >= 4)
        {
          char c = w->text[strlen(w->text)-1];
          if (c == '.' || c == '?' || c == '!')
            done = True;
        }

      /* If the sentence is kind of long already, terminate at commas, etc. */
      if (se->nwords >= 12)
        {
          char c = w->text[strlen(w->text)-1];
          if (c == ',' || c == ';' || c == ':' || c == '-' ||
              c == ')' || c == ']' || c == '}')
            done = True;
        }

      if (se->nwords >= 25)  /* ok that's just about enough out of you */
        done = True;

      if (s->mode == PAGE &&
          x + w->rbearing > right)			/* wrap line */
        {
          align_line (s, se, line_start, x, right);
          line_start = se->nwords;

          x = left;
          y += se->font->ascent;

          /* If we're close to the bottom of the screen, stop, and 
             unread the current word.  (But not if this is the first
             word, otherwise we might just get stuck on it.)
           */
          if (se->nwords > 0 &&
              y + se->font->ascent > s->xgwa.height)
            {
              unread_word (s, w);
              done = True;
              break;
            }
        }

      w->target_x = x + w->lbearing;
      w->target_y = y - w->ascent;

      x += w->rbearing + space;
      se->width = x;

      if (se->nwords >= (array_size - 1))
        {
          array_size += 100;
          se->words = (word **) realloc (se->words,
                                         array_size * sizeof(*se->words));
          if (!se->words)
            {
              fprintf (stderr, "%s: out of memory (%d words)\n",
                       progname, array_size);
              exit (1);
            }
        }

      se->words[se->nwords++] = w;
    }

  se->width -= space;

  switch (s->mode)
    {
    case PAGE:
      align_line (s, se, line_start, x, right);
      if (se->move_chars_p)
        split_words (s, se);
      scatter_sentence (s, se);
      shuffle_words (s, se);
      break;
    case SCROLL:
      aim_sentence (s, se);
      break;
    default:
      abort();
      break;
    }

# ifdef DEBUG
  if (s->debug_p)
    {
      fprintf (stderr, "%s: sentence %d:", progname, se->id);
      for (i = 0; i < se->nwords; i++)
        fprintf (stderr, " %s", se->words[i]->text);
      fprintf (stderr, "\n");
    }
# endif
}


/* Render a single word object to the screen.
 */
static void
draw_word (state *s, sentence *se, word *w)
{
  if (! w->pixmap) return;

  if (w->x + w->width < 0 ||
      w->y + w->height < 0 ||
      w->x > s->xgwa.width ||
      w->y > s->xgwa.height)
    return;

  XSetClipMask (s->dpy, se->fg_gc, w->mask);
  XSetClipOrigin (s->dpy, se->fg_gc, w->x, w->y);
  XCopyPlane (s->dpy, w->pixmap, s->b, se->fg_gc,
              0, 0, w->width, w->height,
              w->x, w->y,
              1L);
}


/* If there is room for more sentences, add one.
 */
static void
more_sentences (state *s)
{
  int i;
  Bool any = False;
  for (i = 0; i < s->nsentences; i++)
    {
      sentence *se = s->sentences[i];
      if (! se)
        {
          se = new_sentence (s);
          populate_sentence (s, se);
          if (se->nwords > 0)
            s->spawn_p = False, any = True;
          else
            {
              free_sentence (s, se);
              se = 0;
            }
          s->sentences[i] = se;
          if (se)
            s->latest_sentence = se->id;
          break;
        }
    }

  if (any) sort_sentences (s);
}


/* Render all the words to the screen, and run the animation one step.
 */
static void
draw_sentence (state *s, sentence *se)
{
  int i;
  Bool moved = False;

  if (! se) return;

  for (i = 0; i < se->nwords; i++)
    {
      word *w = se->words[i];

      switch (s->mode)
        {
        case PAGE:
          if (se->anim_state != PAUSE &&
              w->tick <= w->nticks)
            {
              int dx = w->target_x - w->start_x;
              int dy = w->target_y - w->start_y;
              double r = sin (w->tick * M_PI / (2 * w->nticks));
              w->x = w->start_x + (dx * r);
              w->y = w->start_y + (dy * r);

              w->tick++;
              if (se->anim_state == OUT && s->mode == PAGE)
                w->tick++;  /* go out faster */
              moved = True;
            }
          break;
        case SCROLL:
          {
            int dx = w->target_x - w->start_x;
            int dy = w->target_y - w->start_y;
            double r = (double) w->tick / w->nticks;
            w->x = w->start_x + (dx * r);
            w->y = w->start_y + (dy * r);
            w->tick++;
            moved = (w->tick <= w->nticks);

            /* Launch a new sentence when:
               - the front of this sentence is almost off the left edge;
               - the end of this sentence is almost on screen.
               - or, randomly
             */
            if (se->anim_state != OUT &&
                i == 0 &&
                se->id == s->latest_sentence)
              {
                Bool new_p = (w->x < (s->xgwa.width * 0.4) &&
                              w->x + se->width < (s->xgwa.width * 2.1));
                Bool rand_p = (new_p ? 0 : !(random() % 2000));

                if (new_p || rand_p)
                  {
                    se->anim_state = OUT;
                    s->spawn_p = True;
# ifdef DEBUG
                    if (s->debug_p)
                      fprintf (stderr, "%s: OUT   %d (x2 = %d%s)\n",
                               progname, se->id,
                               se->words[0]->x + se->width,
                               rand_p ? " randomly" : "");
# endif
                  }
              }
          }
          break;
        default:
          abort();
          break;
        }

      draw_word (s, se, w);
    }

  if (moved && se->anim_state == PAUSE)
    abort();

  if (! moved)
    {
      switch (se->anim_state)
        {
        case IN:
          se->anim_state = PAUSE;
          se->pause_tick = (se->nwords * 7 * s->linger);
          if (se->move_chars_p)
            se->pause_tick /= 5;
          scatter_sentence (s, se);
          shuffle_words (s, se);
# ifdef DEBUG
          if (s->debug_p)
            fprintf (stderr, "%s: PAUSE %d\n", progname, se->id);
# endif
          break;
        case PAUSE:
          if (--se->pause_tick <= 0)
            {
              se->anim_state = OUT;
              s->spawn_p = True;
# ifdef DEBUG
              if (s->debug_p)
                fprintf (stderr, "%s: OUT   %d\n", progname, se->id);
# endif
            }
          break;
        case OUT:
# ifdef DEBUG
          if (s->debug_p)
            fprintf (stderr, "%s: DEAD  %d\n", progname, se->id);
# endif
          {
            int j;
            for (j = 0; j < s->nsentences; j++)
              if (s->sentences[j] == se)
                s->sentences[j] = 0;
            free_sentence (s, se);
          }
          break;
        default:
          abort();
          break;
        }
    }
}


/* Render all the words to the screen, and run the animation one step.
   Clear screen first, swap buffers after.
 */
static void
draw_fontglide (state *s)
{
  int i;

  if (s->spawn_p)
    more_sentences (s);

  if (!s->trails_p)
    XFillRectangle (s->dpy, s->b, s->bg_gc,
                    0, 0, s->xgwa.width, s->xgwa.height);

  for (i = 0; i < s->nsentences; i++)
    draw_sentence (s, s->sentences[i]);

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  if (s->backb)
    {
      XdbeSwapInfo info[1];
      info[0].swap_window = s->window;
      info[0].swap_action = (s->dbeclear_p ? XdbeBackground : XdbeUndefined);
      XdbeSwapBuffers (s->dpy, info, 1);
    }
  else
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
  if (s->dbuf)
    {
      XCopyArea (s->dpy, s->b, s->window, s->bg_gc,
		 0, 0, s->xgwa.width, s->xgwa.height, 0, 0);
    }
}


static void
handle_events (state *s)
{
  while (XPending (s->dpy))
    {
      XEvent event;
      XNextEvent (s->dpy, &event);

      if (event.xany.type == ConfigureNotify)
        {
          XGetWindowAttributes (s->dpy, s->window, &s->xgwa);

          if (s->dbuf && (s->ba))
            {
              XFreePixmap (s->dpy, s->ba);
              s->ba = XCreatePixmap (s->dpy, s->window, 
				     s->xgwa.width, s->xgwa.height,
				     s->xgwa.depth);
              XFillRectangle (s->dpy, s->ba, s->bg_gc, 0, 0, 
                              s->xgwa.width, s->xgwa.height);
              s->b = s->ba;
            }
        }

      screenhack_handle_event (s->dpy, &event);
    }
}



/* Subprocess.
   (This bit mostly cribbed from phosphor.c)
 */

static void
subproc_cb (XtPointer closure, int *source, XtInputId *id)
{
  state *s = (state *) closure;
  s->input_available_p = True;
}


static void
launch_text_generator (state *s)
{
  char *oprogram = get_string_resource ("program", "Program");
  char *program = (char *) malloc (strlen (oprogram) + 10);
  strcpy (program, "( ");
  strcat (program, oprogram);
  strcat (program, " ) 2>&1");

  if (s->debug_p)
    fprintf (stderr, "%s: forking: %s\n", progname, program);

  if ((s->pipe = popen (program, "r")))
    {
      s->pipe_id =
        XtAppAddInput (app, fileno (s->pipe),
                       (XtPointer) (XtInputReadMask | XtInputExceptMask),
                       subproc_cb, (XtPointer) s);
    }
  else
    {
      perror (program);
    }
}


static void
relaunch_generator_timer (XtPointer closure, XtIntervalId *id)
{
  state *s = (state *) closure;
  launch_text_generator (s);
}


/* When the subprocess has generated some output, this reads as much as it
   can into s->buf at s->buf_tail.
 */
static void
drain_input (state *s)
{
  /* allow subproc_cb() to run, if the select() down in Xt says that
     input is available.  This tells us whether we can read() without
     blocking. */
  if (! s->input_available_p)
    if (XtAppPending (app) & (XtIMTimer|XtIMAlternateInput))
      XtAppProcessEvent (app, XtIMTimer|XtIMAlternateInput);

  if (! s->pipe) return;
  if (! s->input_available_p) return;
  s->input_available_p = False;

  if (s->buf_tail < sizeof(s->buf) - 2)
    {
      int target = sizeof(s->buf) - s->buf_tail - 2;
      int n;
      n = read (fileno (s->pipe),
                (void *) (s->buf + s->buf_tail),
                target);
      if (n > 0)
        {
          s->buf_tail += n;
          s->buf[s->buf_tail] = 0;
        }
      else
        {
          XtRemoveInput (s->pipe_id);
          s->pipe_id = 0;
          pclose (s->pipe);
          s->pipe = 0;

          /* If the process didn't print a terminating newline, add one. */
          if (s->buf_tail > 1 &&
              s->buf[s->buf_tail-1] != '\n')
            {
              s->buf[s->buf_tail++] = '\n';
              s->buf[s->buf_tail] = 0;
            }

          /* Then add one more, to make sure there's a sentence break at EOF.
           */
          s->buf[s->buf_tail++] = '\n';
          s->buf[s->buf_tail] = 0;

          /* Set up a timer to re-launch the subproc in a bit. */
          XtAppAddTimeOut (app, s->subproc_relaunch_delay,
                           relaunch_generator_timer,
                           (XtPointer) s);
        }
    }
}


/* Window setup and resource loading */

static state *
init_fontglide (Display *dpy, Window window)
{
  XGCValues gcv;
  state *s = (state *) calloc (1, sizeof(*s));
  s->dpy = dpy;
  s->window = window;

  XGetWindowAttributes (s->dpy, s->window, &s->xgwa);

  s->font_override = get_string_resource ("font", "Font");
  if (s->font_override && (!*s->font_override || *s->font_override == '('))
    s->font_override = 0;

  s->charset = get_string_resource ("fontCharset", "FontCharset");
  s->border_width = get_integer_resource ("fontBorderWidth", "Integer");
  if (s->border_width < 0 || s->border_width > 20)
    s->border_width = 1;

  s->speed = get_float_resource ("speed", "Float");
  if (s->speed <= 0 || s->speed > 200)
    s->speed = 1;

  s->linger = get_float_resource ("linger", "Float");
  if (s->linger <= 0 || s->linger > 200)
    s->linger = 1;

  s->debug_p = get_boolean_resource ("debug", "Debug");
  s->trails_p = get_boolean_resource ("trails", "Trails");

  s->dbuf = get_boolean_resource ("doubleBuffer", "Boolean");
  s->dbeclear_p = get_boolean_resource ("useDBEClear", "Boolean");

  if (s->trails_p) s->dbuf = False;  /* don't need it in this case */

  {
    char *ss = get_string_resource ("mode", "Mode");
    if (!ss || !*ss || !strcasecmp (ss, "random"))
      s->mode = ((random() % 2) ? SCROLL : PAGE);
    else if (!strcasecmp (ss, "scroll"))
      s->mode = SCROLL;
    else if (!strcasecmp (ss, "page"))
      s->mode = PAGE;
    else
      {
        fprintf (stderr,
                "%s: `mode' must be `scroll', `page', or `random', not `%s'\n",
                 progname, ss);
      }
  }

  if (s->dbuf)
    {
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
      if (s->dbeclear_p)
        s->b = xdbe_get_backbuffer (dpy, window, XdbeBackground);
      else
        s->b = xdbe_get_backbuffer (dpy, window, XdbeUndefined);
      s->backb = s->b;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

      if (!s->b)
        {
          s->ba = XCreatePixmap (s->dpy, s->window, 
                                 s->xgwa.width, s->xgwa.height,
                                 s->xgwa.depth);
          s->b = s->ba;
        }
    }
  else
    {
      s->b = s->window;
    }

  gcv.foreground = get_pixel_resource ("background", "Background",
                                       s->dpy, s->xgwa.colormap);
  s->bg_gc = XCreateGC (s->dpy, s->b, GCForeground, &gcv);

  s->subproc_relaunch_delay = 2 * 1000;

  launch_text_generator (s);

  s->nsentences = 5; /* #### */
  s->sentences = (sentence **) calloc (s->nsentences, sizeof (sentence *));
  s->spawn_p = True;

  return s;
}



char *progclass = "FontGlide";

char *defaults [] = {
  ".background:		#000000",
  ".foreground:		#DDDDDD",
  ".borderColor:	#555555",
  "*delay:	        10000",
  "*program:	        xscreensaver-text",
  "*mode:               random",
  ".font:               (default)",
  "*fontCharset:        iso8859-1",
  "*fontBorderWidth:    2",
  "*speed:              1.0",
  "*linger:             1.0",
  "*trails:             False",
  "*debug:              False",
  "*doubleBuffer:	True",
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  "*useDBE:		True",
  "*useDBEClear:	True",
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
  0
};

XrmOptionDescRec options [] = {
  { "-scroll",		".mode",	    XrmoptionNoArg, "scroll" },
  { "-page",		".mode",	    XrmoptionNoArg, "page"   },
  { "-random",		".mode",	    XrmoptionNoArg, "random" },
  { "-delay",		".delay",	    XrmoptionSepArg, 0 },
  { "-speed",		".speed",	    XrmoptionSepArg, 0 },
  { "-linger",		".linger",	    XrmoptionSepArg, 0 },
  { "-program",		".program",	    XrmoptionSepArg, 0 },
  { "-font",		".font",	    XrmoptionSepArg, 0 },
  { "-fn",		".font",	    XrmoptionSepArg, 0 },
  { "-bw",		".fontBorderWidth", XrmoptionSepArg, 0 },
  { "-trails",		".trails",	    XrmoptionNoArg,  "True"  },
  { "-no-trails",	".trails",	    XrmoptionNoArg,  "False" },
  { "-db",		".doubleBuffer",    XrmoptionNoArg,  "True"  },
  { "-no-db",		".doubleBuffer",    XrmoptionNoArg,  "False" },
  { "-debug",		".debug",           XrmoptionNoArg,  "True"  },
  { 0, 0, 0, 0 }
};


void
screenhack (Display *dpy, Window window)
{
  state *s = init_fontglide (dpy, window);
  int delay = get_integer_resource ("delay", "Integer");

  while (1)
    {
      handle_events (s);
      draw_fontglide (s);
      XSync (dpy, False);
      if (delay) usleep (delay);
    }
}
