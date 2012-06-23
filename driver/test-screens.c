/* test-screens.c --- some test cases for the "monitor sanity" checks.
 * xscreensaver, Copyright (c) 2008 Jamie Zawinski <jwz@jwz.org>
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

#include <X11/Xlib.h>

/* This file doesn't need the Xt headers, so stub these types out... */
#undef XtPointer
#define XtAppContext void*
#define XrmDatabase  void*
#define XtIntervalId void*
#define XtPointer    void*
#define Widget       void*

#include "xscreensaver.h"
#include "visual.h"

#undef WidthOfScreen
#undef HeightOfScreen
#define WidthOfScreen(s) 10240
#define HeightOfScreen(s) 10240

#undef screen_number
#define screen_number(s) ((int) s)

#include "screens.c"   /* to get at static void check_monitor_sanity() */

char *progname = 0;
char *progclass = "XScreenSaver";

const char *blurb(void) { return progname; }

Bool safe_XF86VidModeGetViewPort(Display *d, int i, int *x, int *y) { abort(); }
void initialize_screen_root_widget(saver_screen_info *ssi) { abort(); }
Visual *get_best_gl_visual (saver_info *si, Screen *sc) { abort(); }


static const char *
failstr (monitor_sanity san)
{
  switch (san) {
  case S_SANE:      return "OK";
  case S_ENCLOSED:  return "ENC";
  case S_DUPLICATE: return "DUP";
  case S_OVERLAP:   return "OVR";
  case S_OFFSCREEN: return "OFF";
  case S_DISABLED:  return "DIS";
  default: abort(); break;
  }
}


static void
test (int testnum, const char *screens, const char *desired)
{
  monitor *monitors[100];
  char result[2048];
  char *out = result;
  int i, nscreens = 0;
  char *token = strtok (strdup(screens), ",");
  while (token)
    {
      monitor *m = calloc (1, sizeof (monitor));
      char c;
      m->id = (testnum * 1000) + nscreens;
      if (5 == sscanf (token, "%dx%d+%d+%d@%d%c", 
                       &m->width, &m->height, &m->x, &m->y, 
                       (int *) &m->screen, &c))
        ;
      else if (4 != sscanf (token, "%dx%d+%d+%d%c", 
                            &m->width, &m->height, &m->x, &m->y, &c))
        {
          fprintf (stderr, "%s: unparsable geometry: %s\n", blurb(), token);
          exit (1);
        }
      monitors[nscreens] = m;
      nscreens++;
      token = strtok (0, ",");
    }
  monitors[nscreens] = 0;

  check_monitor_sanity (monitors);

  *out = 0;
  for (i = 0; i < nscreens; i++)
    {
      monitor *m = monitors[i];
      if (out != result) *out++ = ',';
      if (m->sanity == S_SANE)
        {
          sprintf (out, "%dx%d+%d+%d", m->width, m->height, m->x, m->y);
          if (m->screen)
            sprintf (out + strlen(out), "@%d", (int) m->screen);
        }
      else
        strcpy (out, failstr (m->sanity));
      out += strlen(out);
    }
  *out = 0;

  if (!strcmp (result, desired))
    fprintf (stderr, "%s: test %2d OK\n", blurb(), testnum);
  else
    fprintf (stderr, "%s: test %2d FAILED:\n"
             "%s:   given: %s\n"
             "%s:  wanted: %s\n"
             "%s:     got: %s\n",
             blurb(), testnum,
             blurb(), screens, 
             blurb(), desired, 
             blurb(), result);

# if 0
  {
    saver_info SI;
    SI.monitor_layout = monitors;
    describe_monitor_layout (&SI);
  }
# endif

}

static void
run_tests(void)
{
  int i = 1;
# define A(a)   test (i++, a, a);
# define B(a,b) test (i++, a, b)

  A("");
  A("1024x768+0+0");
  A("1024x768+0+0,1024x768+1024+0");
  A("1024x768+0+0,1024x768+0+768");
  A("1024x768+0+0,1024x768+0+768,1024x768+1024+0");
  A("800x600+0+0,800x600+0+0@1,800x600+10+0@2");

  B("1024x768+999999+0",
    "OFF");
  B("1024x768+-999999+-999999",
    "OFF");
  B("1024x768+0+0,1024x768+0+0",
    "1024x768+0+0,DUP");
  B("1024x768+0+0,1024x768+0+0,1024x768+0+0",
    "1024x768+0+0,DUP,DUP");
  B("1024x768+0+0,1024x768+1024+0,1024x768+0+0",
    "1024x768+0+0,1024x768+1024+0,DUP");
  B("1280x1024+0+0,1024x768+0+64,800x600+0+0,640x480+0+0,720x400+0+0",
    "1280x1024+0+0,ENC,ENC,ENC,ENC");
  B("1024x768+0+64,1280x1024+0+0,800x600+0+0,640x480+0+0,800x600+0+0,720x400+0+0",
    "ENC,1280x1024+0+0,ENC,ENC,ENC,ENC");
  B("1024x768+0+64,1280x1024+0+0,800x600+0+0,640x480+0+0,1280x1024+0+0,720x400+0+0",
    "ENC,1280x1024+0+0,ENC,ENC,DUP,ENC");
  B("720x400+0+0,640x480+0+0,800x600+0+0,1024x768+0+64,1280x1024+0+0",
    "ENC,ENC,ENC,ENC,1280x1024+0+0");
  B("1280x1024+0+0,800x600+1280+0,800x600+1300+0",
    "1280x1024+0+0,800x600+1280+0,OVR");
  B("1280x1024+0+0,800x600+1280+0,800x600+1300+0,1280x1024+0+0,800x600+1280+0",
    "1280x1024+0+0,800x600+1280+0,OVR,DUP,DUP");

  /*  +-------------+----+  +------+---+   1: 1440x900,  widescreen display
      |             :    |  | 3+4  :   |   2: 1280x1024, conventional display
      |     1+2     : 1  |  +......+   |   3: 1024x768,  laptop
      |             :    |  |  3       |   4: 800x600,   external projector
      +.............+----+  +----------+
      |      2      |
      |             |
      +-------------+
   */
  B("1440x900+0+0,1280x1024+0+0,1024x768+1440+0,800x600+1440+0",
    "1440x900+0+0,OVR,1024x768+1440+0,ENC");
  B("800x600+0+0,800x600+0+0,800x600+800+0",
    "800x600+0+0,DUP,800x600+800+0");
  B("1600x1200+0+0,1360x768+0+0",
    "1600x1200+0+0,ENC");
  B("1600x1200+0+0,1360x768+0+0,1600x1200+0+0@1,1360x768+0+0@1",
    "1600x1200+0+0,ENC,1600x1200+0+0@1,ENC");
}


int
main (int argc, char **argv)
{
  char *s;
  progname = argv[0];
  s = strrchr(progname, '/');
  if (s) progname = s+1;
  if (argc != 1)
    {
      fprintf (stderr, "usage: %s\n", argv[0]);
      exit (1);
    }

  run_tests();

  exit (0);
}
