/* xlockmore.c --- xscreensaver compatibility layer for xlockmore modules.
 * xscreensaver, Copyright (c) 1997-2018 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * This file, along with xlockmore.h, make it possible to compile an xlockmore
 * module into a standalone program, and thus use it with xscreensaver.
 * By Jamie Zawinski <jwz@jwz.org> on 10-May-97; based on the ideas
 * in the older xlock.h by Charles Hannum <mycroft@ai.mit.edu>.  (I had
 * to redo it, since xlockmore has diverged so far from xlock...)
 */

#include "xlockmoreI.h"
#include "screenhack.h"

#ifndef HAVE_JWXYZ
# include <X11/Intrinsic.h>
#endif /* !HAVE_JWXYZ */

#include <assert.h>
#include <float.h>

#define countof(x) (sizeof((x))/sizeof(*(x)))

#define MAX_COLORS (1L<<13)

extern struct xscreensaver_function_table *xscreensaver_function_table;

extern const char *progclass;

extern struct xlockmore_function_table xlockmore_function_table;

static void *xlockmore_init (Display *, Window, 
                             struct xlockmore_function_table *);
static unsigned long xlockmore_draw (Display *, Window, void *);
static void xlockmore_reshape (Display *, Window, void *, 
                               unsigned int w, unsigned int h);
static Bool xlockmore_event (Display *, Window, void *, XEvent *);
static void xlockmore_free (Display *, Window, void *);


void
xlockmore_setup (struct xscreensaver_function_table *xsft, void *arg)
{
  struct xlockmore_function_table *xlmft = 
    (struct xlockmore_function_table *) arg;
  int i, j;
  char *s;
  XrmOptionDescRec *new_options;
  char **new_defaults;
  const char *xlockmore_defaults;
  ModeSpecOpt *xlockmore_opts = xlmft->opts;

# undef ya_rand_init
  ya_rand_init (0);

  xsft->init_cb    = (void *(*) (Display *, Window)) xlockmore_init;
  xsft->draw_cb    = xlockmore_draw;
  xsft->reshape_cb = xlockmore_reshape;
  xsft->event_cb   = xlockmore_event;
  xsft->free_cb    = xlockmore_free;

  progclass = xlmft->progclass;
  xlockmore_defaults = xlmft->defaults;

  /* Translate the xlockmore `opts[]' argument to a form that
     screenhack.c expects.

     Some of the strings in here are leaked at exit, but since this code
     only runs on X11, that doesn't matter.
   */
  new_options = (XrmOptionDescRec *) 
    calloc (xlockmore_opts->numopts*3 + 100, sizeof (*new_options));

  for (i = 0; i < xlockmore_opts->numopts; i++)
    {
      XrmOptionDescRec *old = &xlockmore_opts->opts[i];
      XrmOptionDescRec *new = &new_options[i];

      if (old->option[0] == '-')
	new->option = old->option;
      else
	{
	  /* Convert "+foo" to "-no-foo". */
	  new->option = (char *) malloc (strlen(old->option) + 5);
	  strcpy (new->option, "-no-");
	  strcat (new->option, old->option + 1);
	}

      new->specifier = strrchr (old->specifier, '.');
      if (!new->specifier) abort();

      new->argKind = old->argKind;
      new->value = old->value;
    }

  /* Add extra args, if they're mentioned in the defaults... */
  {
    char *args[] = { "-count", "-cycles", "-delay", "-ncolors",
		     "-size", "-font", "-wireframe", "-use3d", "-useSHM" };
    for (j = 0; j < countof(args); j++)
      if (strstr(xlockmore_defaults, args[j]+1))
	{
	  XrmOptionDescRec *new = &new_options[i++];
	  new->option = args[j];
	  new->specifier = strdup(args[j]);
	  new->specifier[0] = '.';
	  if (!strcmp(new->option, "-wireframe"))
	    {
	      new->argKind = XrmoptionNoArg;
	      new->value = "True";
	      new = &new_options[i++];
	      new->option = "-no-wireframe";
	      new->specifier = new_options[i-2].specifier;
	      new->argKind = XrmoptionNoArg;
	      new->value = "False";
	    }
	  else if (!strcmp(new->option, "-use3d"))
	    {
	      new->option = "-3d";
	      new->argKind = XrmoptionNoArg;
	      new->value = "True";
	      new = &new_options[i++];
	      new->option = "-no-3d";
	      new->specifier = new_options[i-2].specifier;
	      new->argKind = XrmoptionNoArg;
	      new->value = "False";
	    }
	  else if (!strcmp(new->option, "-useSHM"))
	    {
	      new->option = "-shm";
	      new->argKind = XrmoptionNoArg;
	      new->value = "True";
	      new = &new_options[i++];
	      new->option = "-no-shm";
	      new->specifier = new_options[i-2].specifier;
	      new->argKind = XrmoptionNoArg;
	      new->value = "False";
	    }
	  else
	    {
	      new->argKind = XrmoptionSepArg;
	      new->value = 0;
	    }
	}
  }



  /* Construct the kind of `defaults' that screenhack.c expects from
     the xlockmore `vars[]' argument.
   */
  i = 0;

  new_defaults = (char **) calloc (1, xlockmore_opts->numvarsdesc * 10 + 1000);

  /* Put on the PROGCLASS.background/foreground resources. */
  s = (char *) malloc(50);
  *s = 0;
# ifndef HAVE_JWXYZ
  strcpy (s, progclass);
# endif
  strcat (s, ".background: black");
  new_defaults [i++] = s;

  s = (char *) malloc(50);
  *s = 0;
# ifndef HAVE_JWXYZ
  strcpy (s, progclass);
# endif
  strcat (s, ".foreground: white");
  new_defaults [i++] = s;

  /* Copy the lines out of the `xlockmore_defaults' var and into this array. */
  s = strdup (xlockmore_defaults);
  while (s && *s)
    {
      new_defaults [i++] = s;
      s = strchr(s, '\n');
      if (s)
	*s++ = 0;
    }

  /* Copy the defaults out of the `xlockmore_opts->' variable. */
  for (j = 0; j < xlockmore_opts->numvarsdesc; j++)
    {
      const char *def = xlockmore_opts->vars[j].def;

      if (!def) abort();
      if (!*def) abort();
      if (strlen(def) > 1000) abort();

      s = (char *) malloc (strlen (xlockmore_opts->vars[j].name) +
			   strlen (def) + 10);
      strcpy (s, "*");
      strcat (s, xlockmore_opts->vars[j].name);
      strcat (s, ": ");
      strcat (s, def);
      new_defaults [i++] = s;

      /* Go through the list of resources and print a warning if there
         are any duplicates.
       */
      {
        char *onew = strdup (xlockmore_opts->vars[j].name);
        const char *new = onew;
        int k;
        if ((s = strrchr (new, '.'))) new = s+1;
        if ((s = strrchr (new, '*'))) new = s+1;
        for (k = 0; k < i-1; k++)
          {
            char *oold = strdup (new_defaults[k]);
            const char *old = oold;
            if ((s = strchr (oold, ':'))) *s = 0;
            if ((s = strrchr (old, '.'))) old = s+1;
            if ((s = strrchr (old, '*'))) old = s+1;
            if (!strcasecmp (old, new))
              {
                fprintf (stderr,
                         "%s: duplicate resource \"%s\": "
                         "set in both DEFAULTS and vars[]\n",
                         progname, old);
              }
            free (oold);
          }
        free (onew);
      }
    }

  new_defaults [i] = 0;

  xsft->progclass = progclass;
  xsft->options   = new_options;
  xsft->defaults  = (const char * const *) new_defaults;
}


static void
xlockmore_release_screens (ModeInfo *mi)
{
  struct xlockmore_function_table *xlmft = mi->xlmft;

  /* 2. Call release_##, if it exists. */
  if (xlmft->hack_release)
    xlmft->hack_release (mi);

  /* 3. Free the state array. */
  if (xlmft->state_array) {
    free(*xlmft->state_array);
    *xlmft->state_array = NULL;
    xlmft->state_array = NULL;
  }

  /* 4. Pretend FreeAllGL(mi) gets called here. */

  mi->xlmft->got_init = 0;
}


static void
xlockmore_free_screens (ModeInfo *mi)
{
  struct xlockmore_function_table *xlmft = mi->xlmft;

  /* Optimization: xlockmore_read_resources calls this lots on first start. */
  if (!xlmft->got_init)
    return;

  /* Order is important here: */

  /* 1. Call free_## for all screens. */
  if (xlmft->hack_free) {
    int old_screen = mi->screen_number;
    for (mi->screen_number = 0; mi->screen_number < XLOCKMORE_NUM_SCREENS;
         ++mi->screen_number) {
      xlmft->hack_free (mi);
    }
    mi->screen_number = old_screen;
  }

  xlockmore_release_screens (mi);
}


static void
xlockmore_read_resources (ModeInfo *mi)
{
  Display *dpy = mi->dpy;
  ModeSpecOpt *xlockmore_opts = mi->xlmft->opts;
  int i;
  for (i = 0; i < xlockmore_opts->numvarsdesc; i++)
    {
      void  *var   = xlockmore_opts->vars[i].var;
      Bool  *var_b = (Bool *)  var;
      char **var_c = (char **) var;
      int   *var_i = (int *) var;
      float *var_f = (float *) var;

      /* If any of the options changed, stop this hack's other instances. */
      switch (xlockmore_opts->vars[i].type)
        {
        case t_String:
          {
            char *c = get_string_resource (dpy, xlockmore_opts->vars[i].name,
                                           xlockmore_opts->vars[i].classname);
            if ((!c && !*var_c) || (c && *var_c && !strcmp(c, *var_c))) {
              free (c);
            } else {
              xlockmore_free_screens (mi);
              if (*var_c) free (*var_c);
              *var_c = c;
            }
          }
          break;
        case t_Float:
          {
            float f = get_float_resource (dpy, xlockmore_opts->vars[i].name,
                                          xlockmore_opts->vars[i].classname);
            float frac = fabsf(*var_f) * (1.0f / (1l << (FLT_MANT_DIG - 4)));
            if (f < *var_f - frac || f > *var_f + frac) {
              xlockmore_free_screens (mi);
              *var_f = f;
            }
          }
          break;
        case t_Int:
          {
            int ii = get_integer_resource (dpy, xlockmore_opts->vars[i].name,
                                          xlockmore_opts->vars[i].classname);
            if (ii != *var_i) {
              xlockmore_free_screens (mi);
              *var_i = ii;
            }
          }
          break;
        case t_Bool:
          {
            Bool b = get_boolean_resource (dpy, xlockmore_opts->vars[i].name,
                                           xlockmore_opts->vars[i].classname);
            if (b != *var_b) {
              xlockmore_free_screens (mi);
              *var_b = b;
            }
          }
          break;
        default:
          abort ();
        }
    }
}


/* We keep a list of all of the screen numbers that are in use and not
   yet freed so that they can have sensible screen numbers.  If three
   displays are created (0, 1, 2) and then #1 is closed, then the fourth
   display will be given the now-unused display number 1. (Everything in
   here assumes a 1:1 Display/Screen mapping.)

   XLOCKMORE_NUM_SCREENS is the most number of live displays at one time. So
   if it's 64, then we'll blow up if the system has 64 monitors and also has
   System Preferences open (the small preview window).

   Note that xlockmore-style savers tend to allocate big structures, so
   setting XLOCKMORE_NUM_SCREENS to 1000 will waste a few megabytes.  Also
   most (all?) of them assume that the number of screens never changes, so
   dynamically expanding this array won't work.
 */


static void *
xlockmore_init (Display *dpy, Window window, 
                struct xlockmore_function_table *xlmft)
{
  ModeInfo *mi = (ModeInfo *) calloc (1, sizeof(*mi));
  XGCValues gcv;
  XColor color;
  int i;
  Bool root_p;

  if (! xlmft)
    abort();
  mi->xlmft = xlmft;

  mi->dpy = dpy;
  mi->window = window;
  XGetWindowAttributes (dpy, window, &mi->xgwa);
  
  /* In Cocoa and Android-based xscreensaver, as well as with DEBUG_PAIR,
     hacks run in the same address space, so each one needs to get its own
     screen number.

     Find the first empty slot in live_displays and plug us in.
   */
  {
    const int size = XLOCKMORE_NUM_SCREENS;
    int i;
    for (i = 0; i < size; i++) {
      if (! (xlmft->live_displays & (1 << i)))
        break;
    }
    if (i >= size) abort();
    xlmft->live_displays |= 1ul << i;
    xlmft->got_init &= ~(1ul << i);
    mi->screen_number = i;
  }

  root_p = (window == RootWindowOfScreen (mi->xgwa.screen));

#ifndef HAVE_JWXYZ

  /* Everybody gets motion events, just in case. */
  XSelectInput (dpy, window, (mi->xgwa.your_event_mask | PointerMotionMask));

#endif /* !HAVE_JWXYZ */

  if (mi->xlmft->hack_release) {
/*
    fprintf (
      stderr,
      "%s: WARNING: hack_release is not usually recommended; see MI_INIT.\n",
      progname);
*/
  }

  color.flags = DoRed|DoGreen|DoBlue;
  color.red = color.green = color.blue = 0;
  if (!XAllocColor(dpy, mi->xgwa.colormap, &color))
    abort();
  mi->black = color.pixel;
  color.red = color.green = color.blue = 0xFFFF;
  if (!XAllocColor(dpy, mi->xgwa.colormap, &color))
    abort();
  mi->white = color.pixel;

  if (mono_p)
    {
      static XColor colors[2];
    MONO:
      mi->npixels = 2;
      if (! mi->pixels)
        mi->pixels = (unsigned long *) 
          calloc (mi->npixels, sizeof (*mi->pixels));
      if (!mi->colors)
        mi->colors = (XColor *) 
          calloc (mi->npixels, sizeof (*mi->colors));
      colors[0].flags = DoRed|DoGreen|DoBlue;
      colors[1].flags = DoRed|DoGreen|DoBlue;
      colors[0].red = colors[0].green = colors[0].blue = 0;
      colors[1].red = colors[1].green = colors[1].blue = 0xFFFF;
      mi->writable_p = False;
    }
  else
    {
      mi->npixels = get_integer_resource (dpy, "ncolors", "Integer");
      if (mi->npixels <= 0)
	mi->npixels = 64;
      else if (mi->npixels > MAX_COLORS)
	mi->npixels = MAX_COLORS;

      mi->colors = (XColor *) calloc (mi->npixels, sizeof (*mi->colors));

      mi->writable_p = mi->xlmft->want_writable_colors;

      switch (mi->xlmft->desired_color_scheme)
        {
        case color_scheme_uniform:
          make_uniform_colormap (mi->xgwa.screen, mi->xgwa.visual,
                                 mi->xgwa.colormap,
                                 mi->colors, &mi->npixels,
                                 True, &mi->writable_p, True);
          break;
        case color_scheme_smooth:
          make_smooth_colormap (mi->xgwa.screen, mi->xgwa.visual,
                                mi->xgwa.colormap,
                                mi->colors, &mi->npixels,
                                True, &mi->writable_p, True);
          break;
        case color_scheme_bright:
        case color_scheme_default:
          make_random_colormap (mi->xgwa.screen, mi->xgwa.visual,
                                mi->xgwa.colormap,
                                mi->colors, &mi->npixels,
                                (mi->xlmft->desired_color_scheme ==
                                 color_scheme_bright),
                                True, &mi->writable_p, True);
          break;
        default:
          abort();
        }

      if (mi->npixels <= 2)
	goto MONO;
      else
	{
	  mi->pixels = (unsigned long *)
	    calloc (mi->npixels, sizeof (*mi->pixels));
	  for (i = 0; i < mi->npixels; i++)
	    mi->pixels[i] = mi->colors[i].pixel;
	}
    }

  gcv.foreground = mi->white;
  gcv.background = mi->black;
  mi->gc = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);

  mi->fullrandom = True;

  mi->pause      = get_integer_resource (dpy, "delay", "Usecs");

  mi->cycles     = get_integer_resource (dpy, "cycles", "Int");
  mi->batchcount = get_integer_resource (dpy, "count", "Int");
  mi->size	 = get_integer_resource (dpy, "size", "Int");

  mi->threed = get_boolean_resource (dpy, "use3d", "Boolean");
  mi->threed_delta = get_float_resource (dpy, "delta3d", "Float");
  mi->threed_right_color = get_pixel_resource (dpy,
					       mi->xgwa.colormap, "right3d", "Color");
  mi->threed_left_color = get_pixel_resource (dpy,
				 	      mi->xgwa.colormap, "left3d", "Color");
  mi->threed_both_color = get_pixel_resource (dpy,
					      mi->xgwa.colormap, "both3d", "Color");
  mi->threed_none_color = get_pixel_resource (dpy,
					      mi->xgwa.colormap, "none3d", "Color");

  mi->wireframe_p = get_boolean_resource (dpy, "wireframe", "Boolean");
  mi->root_p = root_p;
  mi->fps_p = get_boolean_resource (dpy, "doFPS", "DoFPS");
  mi->recursion_depth = -1;  /* see fps.c */

  if (mi->pause < 0)
    mi->pause = 0;
  else if (mi->pause > 100000000)
    mi->pause = 100000000;

  /* If this hack uses fonts (meaning, mentioned "font" in DEFAULTS)
     then load it. */
  {
    char *name = get_string_resource (dpy, "font", "Font");
    if (name)
      {
        XFontStruct *f = load_font_retry (dpy, name);
        if (!f) abort();
        XSetFont (dpy, mi->gc, f->fid);
        XFreeFont (dpy, f);
        free (name);
      }
  }
  
  xlockmore_read_resources (mi);

  return mi;
}


static void
xlockmore_clear (ModeInfo *mi)
{
# ifndef HAVE_ANDROID
  /* TODO: Clear the window for Xlib hacks on Android. */
  XClearWindow (mi->dpy, mi->window);
# endif
}


static void
xlockmore_do_init (ModeInfo *mi)
{
  mi->xlmft->got_init |= 1 << mi->screen_number;
  xlockmore_clear (mi);
  mi->xlmft->hack_init (mi);
}


static Bool
xlockmore_got_init (ModeInfo *mi)
{
  return mi->xlmft->got_init & (1 << mi->screen_number);
}


static void
xlockmore_abort_erase (ModeInfo *mi)
{
  if (mi->eraser) {
    eraser_free (mi->eraser);
    mi->eraser = NULL;
  }
  mi->needs_clear = False;
}


static void
xlockmore_check_init (ModeInfo *mi)
{
  if (! xlockmore_got_init (mi)) {
    xlockmore_abort_erase (mi);
    xlockmore_do_init (mi);
  }
}


static unsigned long
xlockmore_draw (Display *dpy, Window window, void *closure)
{
  ModeInfo *mi = (ModeInfo *) closure;
  unsigned long orig_pause = mi->pause;
  unsigned long this_pause;

  if (mi->needs_clear) {
    /* OpenGL hacks never get here. */
    if (!mi->is_drawn) {
      xlockmore_clear (mi);
    } else {
      mi->eraser = erase_window (dpy, window, mi->eraser);
      /* Delay calls to xlockmore hooks while the erase animation is running. */
      if (mi->eraser)
        return 33333;
    }
    mi->needs_clear = False;
  }

  xlockmore_check_init (mi);
  if (mi->needs_clear)
    return 0;
  mi->xlmft->hack_draw (mi);

  this_pause = mi->pause;
  mi->pause  = orig_pause;
  return mi->needs_clear ? 0 : this_pause;
}


static void
xlockmore_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  ModeInfo *mi = (ModeInfo *) closure;
  if (mi) {
    /* Ignore spurious resize events, because xlockmore_do_init usually clears
       the screen, and there's no reason to do that if we don't have to.
     */
# ifndef HAVE_MOBILE
    /* These are not spurious on mobile: they are rotations. */
    if (mi->xgwa.width == w && mi->xgwa.height == h)
      return;
# endif
    mi->xgwa.width = w;
    mi->xgwa.height = h;

    /* Finish any erase operations. */
    if (mi->needs_clear) {
      xlockmore_abort_erase (mi);
      xlockmore_clear (mi);
    }

    /* If there hasn't been an init yet, init now, but don't call reshape_##.
     */
    if (xlockmore_got_init (mi) && mi->xlmft->hack_reshape) {
      mi->xlmft->hack_reshape (mi, mi->xgwa.width, mi->xgwa.height);
    } else {
      mi->is_drawn = False;
      xlockmore_do_init (mi);
    }
  }
}

static Bool
xlockmore_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  ModeInfo *mi = (ModeInfo *) closure;
  if (mi) {
    if (mi->xlmft->hack_handle_events) {
      xlockmore_check_init (mi);
      return mi->xlmft->hack_handle_events (mi, event);
    }

    if (screenhack_event_helper (mi->dpy, mi->window, event)) {
      /* If a clear is in progress, don't interrupt or restart it. */
      if (mi->needs_clear)
        mi->xlmft->got_init &= ~(1ul << mi->screen_number);
      else
        mi->xlmft->hack_init (mi);
      return True;
    }
  }
  return False;
}

void
xlockmore_do_fps (Display *dpy, Window w, fps_state *fpst, void *closure)
{
  ModeInfo *mi = (ModeInfo *) closure;
  fps_compute (fpst, 0, mi ? mi->recursion_depth : -1);
  fps_draw (fpst);
}


static void
xlockmore_free (Display *dpy, Window window, void *closure)
{
  ModeInfo *mi = (ModeInfo *) closure;

  if (mi->eraser)
    eraser_free (mi->eraser);

  /* Some hacks may need to do things with their Display * on cleanup. And
     under JWXYZ, the Display * for this hack gets cleaned up right after
     xlockmore_free returns. Thus, hack_free has to happen now, rather than
     after the final screen has been released.
   */
  if (mi->xlmft->hack_free)
    mi->xlmft->hack_free (mi);

  /* Find us in live_displays and clear that slot. */
  assert (mi->xlmft->live_displays & (1ul << mi->screen_number));
  mi->xlmft->live_displays &= ~(1ul << mi->screen_number);
  if (!mi->xlmft->live_displays)
    xlockmore_release_screens (mi);

  XFreeGC (dpy, mi->gc);
  free_colors (mi->xgwa.screen, mi->xgwa.colormap, mi->colors, mi->npixels);
  free (mi->colors);
  free (mi->pixels);

  free (mi);
}


void
xlockmore_mi_init (ModeInfo *mi, size_t state_size, void **state_array)
{
  struct xlockmore_function_table *xlmft = mi->xlmft;

  /* Steal the state_array for safe keeping.
     Only necessary when the screenhack isn't a once per process deal.
     (i.e. macOS, iOS, Android)
   */
  assert ((!xlmft->state_array && !*state_array) ||
          xlmft->state_array == state_array);
  xlmft->state_array = state_array;

  if (!*xlmft->state_array) {
    *xlmft->state_array = calloc (XLOCKMORE_NUM_SCREENS, state_size);

    if (!*xlmft->state_array) {
#ifdef HAVE_JWXYZ
      /* Throws an exception instead of exiting the process. */
      jwxyz_abort ("%s: out of memory", progname);
#else
      fprintf (stderr, "%s: out of memory\n", progname);
      exit (1);
#endif
    }
  }

  /* Find the appropriate state object, clear it, and we're done. */
  {
    if (xlmft->hack_free)
      xlmft->hack_free (mi);
    memset ((char *)(*xlmft->state_array) + mi->screen_number * state_size, 0,
            state_size);
  }
}


Bool
xlockmore_no_events (ModeInfo *mi, XEvent *event)
{
  return False;
}
