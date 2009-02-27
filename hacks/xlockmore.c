/* xlockmore.c --- xscreensaver compatibility layer for xlockmore modules.
 * xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@netscape.com>
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
 * By Jamie Zawinski <jwz@netscape.com> on 10-May-97; based on the ideas
 * in the older xlock.h by Charles Hannum <mycroft@ai.mit.edu>.  (I had
 * to redo it, since xlockmore has diverged so far from xlock...)
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "screenhack.h"
#include "xlockmoreI.h"

#define countof(x) (sizeof((x))/sizeof(*(x)))

extern ModeSpecOpt xlockmore_opts[];
extern const char *app_defaults;

void
pre_merge_options (void)
{
  int i, j;
  char *s;

  /* Translate the xlockmore `opts[]' argument to a form that
     screenhack.c expects.
   */
  for (i = 0; i < xlockmore_opts->numopts; i++)
    {
      XrmOptionDescRec *old = &xlockmore_opts->opts[i];
      XrmOptionDescRec *new = &options[i];

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
		     "-size", "-wireframe", "-use3d" };
    for (j = 0; j < countof(args); j++)
      if (strstr(app_defaults, args[j]+1))
	{
	  XrmOptionDescRec *new = &options[i++];
	  new->option = args[j];
	  new->specifier = strdup(args[j]);
	  new->specifier[0] = '.';
	  if (!strcmp(new->option, "-wireframe"))
	    {
	      new->argKind = XrmoptionNoArg;
	      new->value = "True";
	      new = &options[i++];
	      new->option = "-no-wireframe";
	      new->specifier = options[i-1].specifier;
	      new->argKind = XrmoptionNoArg;
	      new->value = "False";
	    }
	  else if (!strcmp(new->option, "-use3d"))
	    {
	      new->option = "-3d";
	      new->argKind = XrmoptionNoArg;
	      new->value = "True";
	      new = &options[i++];
	      new->option = "-no-3d";
	      new->specifier = options[i-1].specifier;
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

  /* Put on the PROGCLASS.background/foreground resources. */
  s = (char *) malloc(50);
  strcpy (s, progclass);
  strcat (s, ".background: black");
  defaults [i++] = s;

  s = (char *) malloc(50);
  strcpy (s, progclass);
  strcat (s, ".foreground: white");
  defaults [i++] = s;

  /* Copy the lines out of the `app_defaults' var and into this array. */
  s = strdup (app_defaults);
  while (s && *s)
    {
      defaults [i++] = s;
      s = strchr(s, '\n');
      if (s)
	*s++ = 0;
    }

  /* Copy the defaults out of the `xlockmore_opts->' variable. */
  for (j = 0; j < xlockmore_opts->numvarsdesc; j++)
    {
      const char *def = xlockmore_opts->vars[j].def;
      if (!def) def = "False";
      if (def == ((char*) 1)) def = "True";
      s = (char *) malloc (strlen (xlockmore_opts->vars[j].name) +
			   strlen (def) + 10);
      strcpy (s, "*");
      strcat (s, xlockmore_opts->vars[j].name);
      strcat (s, ": ");
      strcat (s, def);
      defaults [i++] = s;
    }

  defaults [i] = 0;
}


static void
xlockmore_read_resources (void)
{
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
	  *var_c = get_string_resource (xlockmore_opts->vars[i].name,
					xlockmore_opts->vars[i].classname);
	  break;
	case t_Float:
	  *var_f = get_float_resource (xlockmore_opts->vars[i].name,
				       xlockmore_opts->vars[i].classname);
	  break;
	case t_Int:
	  *var_i = get_integer_resource (xlockmore_opts->vars[i].name,
					 xlockmore_opts->vars[i].classname);
	  break;
	case t_Bool:
	  *var_b = get_boolean_resource (xlockmore_opts->vars[i].name,
					 xlockmore_opts->vars[i].classname);
	  break;
	default:
	  abort ();
	}
    }
}



void 
xlockmore_screenhack (Display *dpy, Window window,
		      Bool want_writable_colors,
		      Bool want_uniform_colors,
		      Bool want_smooth_colors,
		      Bool want_bright_colors,
		      void (*hack_init) (ModeInfo *),
		      void (*hack_draw) (ModeInfo *),
		      void (*hack_free) (ModeInfo *))
{
  ModeInfo mi;
  XGCValues gcv;
  XColor color;
  int i;
  time_t start, now;

  memset(&mi, 0, sizeof(mi));
  mi.dpy = dpy;
  mi.window = window;
  XGetWindowAttributes (dpy, window, &mi.xgwa);

  color.flags = DoRed|DoGreen|DoBlue;
  color.red = color.green = color.blue = 0;
  if (!XAllocColor(dpy, mi.xgwa.colormap, &color))
    abort();
  mi.black = color.pixel;
  color.red = color.green = color.blue = 0xFFFF;
  if (!XAllocColor(dpy, mi.xgwa.colormap, &color))
    abort();
  mi.white = color.pixel;

  if (mono_p)
    {
      static unsigned long pixels[2];
      static XColor colors[2];
    MONO:
      mi.npixels = 2;
      mi.pixels = pixels;
      mi.colors = colors;
      pixels[0] = mi.black;
      pixels[1] = mi.white;
      colors[0].flags = DoRed|DoGreen|DoBlue;
      colors[1].flags = DoRed|DoGreen|DoBlue;
      colors[0].red = colors[0].green = colors[0].blue = 0;
      colors[1].red = colors[1].green = colors[1].blue = 0xFFFF;
      mi.writable_p = False;
    }
  else
    {
      mi.npixels = get_integer_resource ("ncolors", "Integer");
      if (mi.npixels <= 0)
	mi.npixels = 64;
      else if (mi.npixels > 256)
	mi.npixels = 256;

      mi.colors = (XColor *) calloc (mi.npixels, sizeof (*mi.colors));

      mi.writable_p = want_writable_colors;

      if (want_uniform_colors)
	make_uniform_colormap (dpy, mi.xgwa.visual, mi.xgwa.colormap,
			       mi.colors, &mi.npixels,
			       True, &mi.writable_p, True);
      else if (want_smooth_colors)
	make_smooth_colormap (dpy, mi.xgwa.visual, mi.xgwa.colormap,
			      mi.colors, &mi.npixels,
			      True, &mi.writable_p, True);
      else
	make_random_colormap (dpy, mi.xgwa.visual, mi.xgwa.colormap,
			      mi.colors, &mi.npixels,
			      want_bright_colors,
			      True, &mi.writable_p, True);

      if (mi.npixels <= 2)
	goto MONO;
      else
	{
	  int i;
	  mi.pixels = (unsigned long *)
	    calloc (mi.npixels, sizeof (*mi.pixels));
	  for (i = 0; i < mi.npixels; i++)
	    mi.pixels[i] = mi.colors[i].pixel;
	}
    }

  gcv.foreground = mi.white;
  gcv.background = mi.black;
  mi.gc = XCreateGC(dpy, window, GCForeground|GCBackground, &gcv);

  mi.fullrandom = True;

  mi.pause      = get_integer_resource ("delay", "Usecs");

  mi.cycles     = get_integer_resource ("cycles", "Int");
  mi.batchcount = get_integer_resource ("count", "Int");
  mi.size	= get_integer_resource ("size", "Int");

#if 0
  decay = get_boolean_resource ("decay", "Boolean");
  if (decay) mi.fullrandom = False;

  trail = get_boolean_resource ("trail", "Boolean");
  if (trail) mi.fullrandom = False;

  grow = get_boolean_resource ("grow", "Boolean");
  if (grow) mi.fullrandom = False;

  liss = get_boolean_resource ("liss", "Boolean");
  if (liss) mi.fullrandom = False;

  ammann = get_boolean_resource ("ammann", "Boolean");
  if (ammann) mi.fullrandom = False;

  jong = get_boolean_resource ("jong", "Boolean");
  if (jong) mi.fullrandom = False;

  sine = get_boolean_resource ("sine", "Boolean");
  if (sine) mi.fullrandom = False;
#endif

  mi.threed = get_boolean_resource ("use3d", "Boolean");
  mi.threed_delta = get_float_resource ("delta3d", "Boolean");
  mi.threed_right_color = get_pixel_resource ("right3d", "Color", dpy,
					      mi.xgwa.colormap);
  mi.threed_left_color = get_pixel_resource ("left3d", "Color", dpy,
					     mi.xgwa.colormap);
  mi.threed_both_color = get_pixel_resource ("both3d", "Color", dpy,
					     mi.xgwa.colormap);
  mi.threed_none_color = get_pixel_resource ("none3d", "Color", dpy,
					     mi.xgwa.colormap);

  mi.wireframe_p = get_boolean_resource ("wireframe", "Boolean");
  mi.root_p = (window == RootWindowOfScreen (mi.xgwa.screen));


  if (mi.pause < 0)
    mi.pause = 0;
  else if (mi.pause > 100000000)
    mi.pause = 100000000;

  xlockmore_read_resources ();

  XClearWindow (dpy, window);

  i = 0;
  start = time((time_t) 0);

  hack_init (&mi);
  do {
    hack_draw (&mi);
    XSync(dpy, False);
    if (mi.pause)
      usleep(mi.pause);

    if (hack_free)
      {
	if (i++ > (mi.batchcount / 4) &&
	    (start + 5) < (now = time((time_t) 0)))
	  {
	    i = 0;
	    start = now;
	    hack_free (&mi);
	    hack_init (&mi);
	    XSync(dpy, False);
	  }
      }

  } while (1);
}
