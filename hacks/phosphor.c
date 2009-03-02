/* xscreensaver, Copyright (c) 1999 Jamie Zawinski <jwz@jwz.org>
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
 */

#include "screenhack.h"
#include <stdio.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>

extern XtAppContext app;

#define FUZZY_BORDER

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

#define BLANK  0
#define FLARE  1
#define NORMAL 2
#define FADE   3

#define CURSOR_INDEX 128

typedef struct {
  unsigned char name;
  int width, height;
  Pixmap pixmap;
#ifdef FUZZY_BORDER
  Pixmap pixmap2;
#endif /* FUZZY_BORDER */
  Bool blank_p;
} p_char;

typedef struct {
  p_char *p_char;
  int state;
  Bool changed;
} p_cell;

typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  XFontStruct *font;
  int grid_width, grid_height;
  int char_width, char_height;
  int scale;
  int ticks;
  p_char **chars;
  p_cell *cells;
  XGCValues gcv;
  GC gc0;
  GC gc1;
#ifdef FUZZY_BORDER
  GC gc2;
#endif /* FUZZY_BORDER */
  GC *gcs;
  XImage *font_bits;

  int cursor_x, cursor_y;
  XtIntervalId cursor_timer;
  Time cursor_blink;

  FILE *pipe;
  XtInputId pipe_id;
  Bool input_available_p;
  Time subproc_relaunch_delay;

} p_state;


static void capture_font_bits (p_state *state);
static p_char *make_character (p_state *state, int c);
static void drain_input (p_state *state);
static void char_to_pixmap (p_state *state, p_char *pc, int c);
static void launch_text_generator (p_state *state);


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

static p_state *
init_phosphor (Display *dpy, Window window)
{
  int i;
  unsigned long flags;
  p_state *state = (p_state *) calloc (sizeof(*state), 1);
  char *fontname = get_string_resource ("font", "Font");
  XFontStruct *font;

  state->dpy = dpy;
  state->window = window;

  XGetWindowAttributes (dpy, window, &state->xgwa);

  state->font = XLoadQueryFont (dpy, fontname);

  if (!state->font)
    {
      fprintf(stderr, "couldn't load font \"%s\"\n", fontname);
      state->font = XLoadQueryFont (dpy, "fixed");
    }
  if (!state->font)
    {
      fprintf(stderr, "couldn't load font \"fixed\"");
      exit(1);
    }

  font = state->font;
  state->scale = get_integer_resource ("scale", "Integer");
  state->ticks = 3 + get_integer_resource ("ticks", "Integer");

#if 0
  for (i = 0; i < font->n_properties; i++)
    if (font->properties[i].name == XA_FONT)
      printf ("font: %s\n", XGetAtomName(dpy, font->properties[i].card32));
#endif /* 0 */

  state->cursor_blink = get_integer_resource ("cursor", "Time");
  state->subproc_relaunch_delay =
    (1000 * get_integer_resource ("relaunch", "Time"));

  state->char_width  = font->max_bounds.width;
  state->char_height = font->max_bounds.ascent + font->max_bounds.descent;

  state->grid_width = state->xgwa.width / (state->char_width * state->scale);
  state->grid_height = state->xgwa.height /(state->char_height * state->scale);
  state->cells = (p_cell *) calloc (sizeof(p_cell),
                                    state->grid_width * state->grid_height);
  state->chars = (p_char **) calloc (sizeof(p_char *), 256);

  state->gcs = (GC *) calloc (sizeof(GC), state->ticks + 1);

  {
    int ncolors = MAX (0, state->ticks - 3);
    XColor *colors = (XColor *) calloc (ncolors, sizeof(XColor));
    int h1, h2;
    double s1, s2, v1, v2;

    unsigned long fg = get_pixel_resource ("foreground", "Foreground",
                                           state->dpy, state->xgwa.colormap);
    unsigned long bg = get_pixel_resource ("background", "Background",
                                           state->dpy, state->xgwa.colormap);
    unsigned long flare = get_pixel_resource ("flareForeground", "Foreground",
                                              state->dpy,state->xgwa.colormap);
    unsigned long fade = get_pixel_resource ("fadeForeground", "Foreground",
                                             state->dpy,state->xgwa.colormap);

    XColor start, end;

    start.pixel = fade;
    XQueryColor (state->dpy, state->xgwa.colormap, &start);

    end.pixel = bg;
    XQueryColor (state->dpy, state->xgwa.colormap, &end);

    /* Now allocate a ramp of colors from the main color to the background. */
    rgb_to_hsv (start.red, start.green, start.blue, &h1, &s1, &v1);
    rgb_to_hsv (end.red, end.green, end.blue, &h2, &s2, &v2);
    make_color_ramp (state->dpy, state->xgwa.colormap,
                     h1, s1, v1,
                     h2, s2, v2,
                     colors, &ncolors,
                     False, True, False);

    /* Now, GCs all around.
     */
    state->gcv.font = font->fid;
    state->gcv.cap_style = CapRound;
#ifdef FUZZY_BORDER
    state->gcv.line_width = (int) (((long) state->scale) * 1.3);
    if (state->gcv.line_width == state->scale)
      state->gcv.line_width++;
#else /* !FUZZY_BORDER */
    state->gcv.line_width = (int) (((long) state->scale) * 0.9);
    if (state->gcv.line_width >= state->scale)
      state->gcv.line_width = state->scale - 1;
    if (state->gcv.line_width < 1)
      state->gcv.line_width = 1;
#endif /* !FUZZY_BORDER */

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
        state->gcs[FADE + i] = XCreateGC (state->dpy, state->window,
                                          flags, &state->gcv);
      }
  }

  capture_font_bits (state);

  launch_text_generator (state);

  return state;
}


static void
capture_font_bits (p_state *state)
{
  XFontStruct *font = state->font;
  int safe_width = font->max_bounds.rbearing - font->min_bounds.lbearing;
  int height = state->char_height;
  unsigned char string[257];
  int i;
  Pixmap p = XCreatePixmap (state->dpy, state->window,
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
                          (GCFont | GCForeground | GCBackground |
                           GCCapStyle | GCLineWidth),
                          &state->gcv);

#ifdef FUZZY_BORDER
  {
    state->gcv.line_width = (int) (((long) state->scale) * 0.8);
    if (state->gcv.line_width >= state->scale)
      state->gcv.line_width = state->scale - 1;
    if (state->gcv.line_width < 1)
      state->gcv.line_width = 1;
    state->gc2 = XCreateGC (state->dpy, p,
                            (GCFont | GCForeground | GCBackground |
                             GCCapStyle | GCLineWidth),
                            &state->gcv);
  }
#endif /* FUZZY_BORDER */

  XFillRectangle (state->dpy, p, state->gc0, 0, 0, (safe_width * 256), height);

  for (i = 0; i < 256; i++)
    {
      if (string[i] < font->min_char_or_byte2 ||
          string[i] > font->max_char_or_byte2)
        continue;
      XDrawString (state->dpy, p, state->gc1,
                   i * safe_width, font->ascent,
                   string + i, 1);
    }

  /* Draw the cursor. */
  XFillRectangle (state->dpy, p, state->gc1,
                  (CURSOR_INDEX * safe_width), 1,
                  (font->per_char
                   ? font->per_char['n'-font->min_char_or_byte2].width
                   : font->max_bounds.width),
                  font->ascent - 1);

#if 0
  XCopyPlane (state->dpy, p, state->window, state->gcs[FLARE],
              0, 0, (safe_width * 256), height, 0, 0, 1L);
  XSync(state->dpy, False);
#endif

  XSync (state->dpy, False);
  state->font_bits = XGetImage (state->dpy, p, 0, 0,
                                (safe_width * 256), height, ~0L, XYPixmap);
  XFreePixmap (state->dpy, p);

  for (i = 0; i < 256; i++)
    state->chars[i] = make_character (state, i);
  state->chars[CURSOR_INDEX] = make_character (state, CURSOR_INDEX);
}


static p_char *
make_character (p_state *state, int c)
{
  p_char *pc = (p_char *) malloc (sizeof (*pc));
  pc->name = (unsigned char) c;
  pc->width =  state->scale * state->char_width;
  pc->height = state->scale * state->char_height;
  char_to_pixmap (state, pc, c);
  return pc;
}


static void
char_to_pixmap (p_state *state, p_char *pc, int c)
{
  Pixmap p = 0;
  GC gc;
#ifdef FUZZY_BORDER
  Pixmap p2 = 0;
  GC gc2;
#endif /* FUZZY_BORDER */
  int from, to;
  int x1, y;

  XFontStruct *font = state->font;
  int safe_width = font->max_bounds.rbearing - font->min_bounds.lbearing;

  int width  = state->scale * state->char_width;
  int height = state->scale * state->char_height;

  if (c < font->min_char_or_byte2 ||
      c > font->max_char_or_byte2)
    goto DONE;

  gc = state->gc1;
  p = XCreatePixmap (state->dpy, state->window, width, height, 1);
  XFillRectangle (state->dpy, p, state->gc0, 0, 0, width, height);
#ifdef FUZZY_BORDER
  gc2 = state->gc2;
  p2 = XCreatePixmap (state->dpy, state->window, width, height, 1);
  XFillRectangle (state->dpy, p2, state->gc0, 0, 0, width, height);
#endif /* FUZZY_BORDER */

  from = safe_width * c;
  to =   safe_width * (c + 1);

#if 0
  if (c > 75 && c < 150)
    {
      printf ("\n=========== %d (%c)\n", c, c);
      for (y = 0; y < state->char_height; y++)
        {
          for (x1 = from; x1 < to; x1++)
            printf (XGetPixel (state->font_bits, x1, y) ? "* " : ". ");
          printf ("\n");
        }
    }
#endif

  pc->blank_p = True;
  for (y = 0; y < state->char_height; y++)
    for (x1 = from; x1 < to; x1++)
      if (XGetPixel (state->font_bits, x1, y))
        {
          int xoff = state->scale / 2;
          int x2;
          for (x2 = x1; x2 < to; x2++)
            if (!XGetPixel (state->font_bits, x2, y))
              break;
          x2--;
          XDrawLine (state->dpy, p, gc,
                     (x1 - from) * state->scale + xoff, y * state->scale,
                     (x2 - from) * state->scale + xoff, y * state->scale);
#ifdef FUZZY_BORDER
          XDrawLine (state->dpy, p2, gc2,
                     (x1 - from) * state->scale + xoff, y * state->scale,
                     (x2 - from) * state->scale + xoff, y * state->scale);
#endif /* FUZZY_BORDER */
          x1 = x2;
          pc->blank_p = False;
        }

  /*  if (pc->blank_p && c == CURSOR_INDEX)
    abort();*/

 DONE:
  pc->pixmap = p;
#ifdef FUZZY_BORDER
  pc->pixmap2 = p2;
#endif /* FUZZY_BORDER */
}


/* Managing the display. 
 */

static void cursor_on_timer (XtPointer closure, XtIntervalId *id);
static void cursor_off_timer (XtPointer closure, XtIntervalId *id);

static Bool
set_cursor_1 (p_state *state, Bool on)
{
  p_cell *cell = &state->cells[state->grid_width * state->cursor_y
                              + state->cursor_x];
  p_char *cursor = state->chars[CURSOR_INDEX];
  int new_state = (on ? NORMAL : FADE);

  if (cell->p_char != cursor)
    cell->changed = True;

  if (cell->state != new_state)
    cell->changed = True;

  cell->p_char = cursor;
  cell->state = new_state;
  return cell->changed;
}

static void
set_cursor (p_state *state, Bool on)
{
  if (set_cursor_1 (state, on))
;
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
  set_cursor_1 (state, False);
  state->cursor_timer = XtAppAddTimeOut (app, state->cursor_blink,
                                         cursor_on_timer, closure);
}

static void
cursor_on_timer (XtPointer closure, XtIntervalId *id)
{
  p_state *state = (p_state *) closure;
  set_cursor_1 (state, True);
  state->cursor_timer = XtAppAddTimeOut (app, 2 * state->cursor_blink,
                                         cursor_off_timer, closure);
}


static void
clear (p_state *state)
{
  int x, y;
  state->cursor_x = 0;
  state->cursor_y = 0;
  for (y = 0; y < state->grid_height; y++)
    for (x = 0; x < state->grid_width; x++)
      {
        p_cell *cell = &state->cells[state->grid_width * y + x];
        if (cell->state == FLARE || cell->state == NORMAL)
          {
            cell->state = FADE;
            cell->changed = True;
          }
      }
  set_cursor (state, True);
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
              cell->state = BLANK;
            cell->changed = True;
          }
      }
}


static void
scroll (p_state *state)
{
  int x, y;
  for (x = 0; x < state->grid_width; x++)
    {
      p_cell *from, *to;
      for (y = 1; y < state->grid_height; y++)
        {
          from = &state->cells[state->grid_width * y     + x];
          to   = &state->cells[state->grid_width * (y-1) + x];

          if ((from->state == FLARE || from->state == NORMAL) &&
              !from->p_char->blank_p)
            {
              *to = *from;
              to->state = NORMAL;  /* should be FLARE?  Looks bad... */
            }
          else
            {
              if (to->state == FLARE || to->state == NORMAL)
                to->state = FADE;
            }

          to->changed = True;
        }

      to = from;
      if (to->state == FLARE || to->state == NORMAL)
        {
          to->state = FADE;
          to->changed = True;
        }
    }
  set_cursor (state, True);
}


static void
print_char (p_state *state, int c)
{
  p_cell *cell = &state->cells[state->grid_width * state->cursor_y
                              + state->cursor_x];

  /* Start the cursor fading (in case we don't end up overwriting it.) */
  if (cell->state == FLARE || cell->state == NORMAL)
    {
      cell->state = FADE;
      cell->changed = True;
    }

  if (c == '\t') c = ' ';   /* blah. */

  if (c == '\r' || c == '\n')
    {
      state->cursor_x = 0;
      if (state->cursor_y == state->grid_height - 1)
        scroll (state);
      else
        state->cursor_y++;
    }
  else if (c == '\014')
    {
      clear (state);
    }
  else
    {
      cell->state = FLARE;
      cell->p_char = state->chars[c];
      cell->changed = True;
      state->cursor_x++;

      if (c != ' ' && cell->p_char->blank_p)
        cell->p_char = state->chars[CURSOR_INDEX];

      if (state->cursor_x >= state->grid_width - 1)
        {
          state->cursor_x = 0;
          if (state->cursor_y >= state->grid_height - 1)
            scroll (state);
          else
            state->cursor_y++;
        }
    }
  set_cursor (state, True);
}


static void
update_display (p_state *state, Bool changed_only)
{
  int x, y;

  for (y = 0; y < state->grid_height; y++)
    for (x = 0; x < state->grid_width; x++)
      {
        p_cell *cell = &state->cells[state->grid_width * y + x];
        int width, height, tx, ty;

        if (changed_only && !cell->changed)
          continue;

        width = state->char_width * state->scale;
        height = state->char_height * state->scale;
        tx = x * width;
        ty = y * height;

        if (cell->state == BLANK || cell->p_char->blank_p)
          {
            XFillRectangle (state->dpy, state->window, state->gcs[BLANK],
                            tx, ty, width, height);
          }
        else
          {
#ifdef FUZZY_BORDER
            GC gc1 = state->gcs[cell->state];
            GC gc2 = ((cell->state + 2) < state->ticks
                      ? state->gcs[cell->state + 2]
                      : 0);
            XCopyPlane (state->dpy, cell->p_char->pixmap, state->window,
                        (gc2 ? gc2 : gc1),
                        0, 0, width, height, tx, ty, 1L);
            if (gc2)
              {
                XSetClipMask (state->dpy, gc1, cell->p_char->pixmap2);
                XSetClipOrigin (state->dpy, gc1, tx, ty);
                XFillRectangle (state->dpy, state->window, gc1,
                                tx, ty, width, height);
                XSetClipMask (state->dpy, gc1, None);
              }
#else /* !FUZZY_BORDER */

            XCopyPlane (state->dpy,
                        cell->p_char->pixmap, state->window,
                        state->gcs[cell->state],
                        0, 0, width, height, tx, ty, 1L);

#endif /* !FUZZY_BORDER */
          }

        cell->changed = False;
      }
}


static void
run_phosphor (p_state *state)
{
  update_display (state, True);
  decay (state);
  drain_input (state);
}


/* Subprocess.
 */

static void
subproc_cb (XtPointer closure, int *source, XtInputId *id)
{
  p_state *state = (p_state *) closure;
  state->input_available_p = True;
}


static void
launch_text_generator (p_state *state)
{
  char *oprogram = get_string_resource ("program", "Program");
  char *program = (char *) malloc (strlen (oprogram) + 10);

  strcpy (program, "( ");
  strcat (program, oprogram);
  strcat (program, " ) 2>&1");

  if ((state->pipe = popen (program, "r")))
    {
      state->pipe_id =
        XtAppAddInput (app, fileno (state->pipe),
                       (XtPointer) (XtInputReadMask | XtInputExceptMask),
                       subproc_cb, (XtPointer) state);
    }
  else
    {
      perror (program);
    }
}


static void
relaunch_generator_timer (XtPointer closure, XtIntervalId *id)
{
  p_state *state = (p_state *) closure;
  launch_text_generator (state);
}


static void
drain_input (p_state *state)
{
  if (state->input_available_p)
    {
      char s[2];
      int n = read (fileno (state->pipe), (void *) s, 1);
      if (n == 1)
        {
          print_char (state, s[0]);
        }
      else
        {
          XtRemoveInput (state->pipe_id);
          state->pipe_id = 0;
          pclose (state->pipe);
          state->pipe = 0;

          if (state->cursor_x != 0)	/* break line if unbroken */
            print_char (state, '\n');	/* blank line */
          print_char (state, '\n');

          /* Set up a timer to re-launch the subproc in a bit. */
          XtAppAddTimeOut (app, state->subproc_relaunch_delay,
                           relaunch_generator_timer,
                           (XtPointer) state);
        }
        
      state->input_available_p = False;
    }
}



char *progclass = "Phosphor";

char *defaults [] = {
  ".background:		   Black",
  ".foreground:		   Green",
  "*fadeForeground:	   DarkGreen",
  "*flareForeground:	   White",
  "*font:		   fixed",
  "*scale:		   6",
  "*ticks:		   20",
  "*delay:		   50000",
  "*cursor:		   333",
  "*program:		 " ZIPPY_PROGRAM,
  "*relaunch:		   5",
  0
};

XrmOptionDescRec options [] = {
  { "-font",		".font",		XrmoptionSepArg, 0 },
  { "-scale",		".scale",		XrmoptionSepArg, 0 },
  { "-ticks",		".ticks",		XrmoptionSepArg, 0 },
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-program",		".program",		XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


void
screenhack (Display *dpy, Window window)
{
  int delay = get_integer_resource ("delay", "Integer");
  p_state *state = init_phosphor (dpy, window);

  clear (state);

  while (1)
    {
      run_phosphor (state);
      XSync (dpy, False);
      screenhack_handle_events (dpy);

      if (XtAppPending (app) & (XtIMTimer|XtIMAlternateInput))
        XtAppProcessEvent (app, XtIMTimer|XtIMAlternateInput);

      if (delay) usleep (delay);
    }
}
