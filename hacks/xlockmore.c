/* xlockmore.c --- xscreensaver compatibility layer for xlockmore modules.
 * xscreensaver, Copyright (c) 1997-2013 Jamie Zawinski <jwz@jwz.org>
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

#ifndef HAVE_COCOA
# include <X11/Intrinsic.h>
#endif /* !HAVE_COCOA */

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
# ifndef HAVE_COCOA
  strcpy (s, progclass);
# endif
  strcat (s, ".background: black");
  new_defaults [i++] = s;

  s = (char *) malloc(50);
  *s = 0;
# ifndef HAVE_COCOA
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

      switch (xlockmore_opts->vars[i].type)
	{
	case t_String:
	  *var_c = get_string_resource (dpy, xlockmore_opts->vars[i].name,
					xlockmore_opts->vars[i].classname);
	  break;
	case t_Float:
	  *var_f = get_float_resource (dpy, xlockmore_opts->vars[i].name,
				       xlockmore_opts->vars[i].classname);
	  break;
	case t_Int:
	  *var_i = get_integer_resource (dpy, xlockmore_opts->vars[i].name,
					 xlockmore_opts->vars[i].classname);
	  break;
	case t_Bool:
	  *var_b = get_boolean_resource (dpy, xlockmore_opts->vars[i].name,
					 xlockmore_opts->vars[i].classname);
	  break;
	default:
	  abort ();
	}
    }
}


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
  
#ifdef HAVE_COCOA
  
  /* In Cocoa-based xscreensaver, all hacks run in the same address space,
     so each one needs to get its own screen number.  We just use a global
     counter for that, instead of actually trying to figure out which
     monitor each window is on.  Also, the embedded "preview" view counts
     as a screen as well.
    
     Note that the screen number will increase each time the saver is
     restarted (e.g., each time preferences are changed!)  So we just
     keep pushing the num_screens number up as needed, and assume that
     no more than 10 simultanious copies will be running at once...
   */
  {
    static int screen_tick = 0;
    mi->num_screens = 10;
    mi->screen_number = screen_tick++;
    if (screen_tick >= mi->num_screens)
      mi->num_screens += 10;
  }
  
  root_p = True;
#else /* !HAVE_COCOA -- real Xlib */
  
  /* In Xlib-based xscreensaver, each hack runs in its own address space,
     so each one only needs to be aware of one screen.
   */
  mi->num_screens = 1;
  mi->screen_number = 0;
  
  {  /* kludge for DEBUG_PAIR */
    static int screen_tick = 0;
    mi->num_screens++;
    if (screen_tick)
      mi->screen_number++;
    screen_tick++;
  }

  root_p = (window == RootWindowOfScreen (mi->xgwa.screen));

  /* Everybody gets motion events, just in case. */
  XSelectInput (dpy, window, (mi->xgwa.your_event_mask | PointerMotionMask));

#endif /* !HAVE_COCOA */
  
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
      static unsigned long pixels[2];
      static XColor colors[2];
    MONO:
      mi->npixels = 2;
      if (! mi->pixels)
        mi->pixels = (unsigned long *) 
          calloc (mi->npixels, sizeof (*mi->pixels));
      if (!mi->colors)
        mi->colors = (XColor *) 
          calloc (mi->npixels, sizeof (*mi->colors));
      pixels[0] = mi->black;
      pixels[1] = mi->white;
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
#ifdef HAVE_XSHM_EXTENSION
  mi->use_shm = get_boolean_resource (dpy, "useSHM", "Boolean");
#endif /* !HAVE_XSHM_EXTENSION */
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
        XFontStruct *f = XLoadQueryFont (dpy, name);
        const char *def1 = "-*-helvetica-bold-r-normal-*-180-*";
        const char *def2 = "fixed";
        if (!f)
          {
            fprintf (stderr, "%s: font %s does not exist, using %s\n",
                     progname, name, def1);
            f = XLoadQueryFont (dpy, def1);
          }
        if (!f)
          {
            fprintf (stderr, "%s: font %s does not exist, using %s\n",
                     progname, def1, def2);
            f = XLoadQueryFont (dpy, def2);
          }
        if (f) XSetFont (dpy, mi->gc, f->fid);
        if (f) XFreeFont (dpy, f);
        free (name);
      }
  }

  xlockmore_read_resources (mi);

  XClearWindow (dpy, window);

  mi->xlmft->hack_init (mi);

  return mi;
}

static unsigned long
xlockmore_draw (Display *dpy, Window window, void *closure)
{
  ModeInfo *mi = (ModeInfo *) closure;

  unsigned long orig_pause = mi->pause;
  unsigned long this_pause;

  mi->xlmft->hack_draw (mi);

  this_pause = mi->pause;
  mi->pause  = orig_pause;
  return this_pause;
}


static void
xlockmore_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  ModeInfo *mi = (ModeInfo *) closure;
  if (mi && mi->xlmft->hack_reshape)
    {
      XGetWindowAttributes (dpy, window, &mi->xgwa);
      mi->xlmft->hack_reshape (mi, mi->xgwa.width, mi->xgwa.height);
    }
}

static Bool
xlockmore_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  ModeInfo *mi = (ModeInfo *) closure;
  if (mi && mi->xlmft->hack_handle_events)
    return mi->xlmft->hack_handle_events (mi, event);
  else
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
  /* Most of the xlockmore/GL hacks don't have `free' functions, and of
     those that do have them, they're incomplete or buggy.  So, fuck it.
     Under X11, we're about to exit anyway, and it doesn't matter.
     On OSX, we'll leak a little.  Beats crashing.
   */
#if 0
  ModeInfo *mi = (ModeInfo *) closure;
  if (mi->xlmft->hack_free)
    mi->xlmft->hack_free (mi);

  XFreeGC (dpy, mi->gc);
  free_colors (dpy, mi->xgwa.colormap, mi->colors, mi->npixels);
  free (mi->colors);
  free (mi->pixels);

  free (mi);
#endif
}

