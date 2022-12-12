/* test-xinput.c --- playing with the XInput2 extension.
 * xscreensaver, Copyright © 2021-2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * This verbosely prints out all events received from Xlib and the XInput2
 * extension.
 *
 *     --grab, --grab-kbd, --grab-mouse
 *     Read events while grabbed.  The grab is released after 15 seconds.
 *
 *     --sync, --async
 *     Async grabs queue up events and deliver them all at release.
 *
 * Real X11 multi-head ("Zaphod Heads") has different behaviors than 
 * single-screen displays with multiple monitors (Xinerama, XRANDR).
 * To test X11 multi-head or different visual depths on Raspberry Pi
 * you have to do it inside a nested server:
 *
 *    apt install xserver-xephyr
 *    sudo rm /tmp/.X1-lock /tmp/.X11-unix/X1
 *    export DISPLAY=:0
 *    Xephyr :1 -ac -screen 1280x720 -screen 640x480x8 &
 *    export DISPLAY=:1
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/Xproto.h>
#include <X11/extensions/XInput2.h>

/* #include "blurb.h" */
extern const char *progname;
extern int verbose_p;

#include "xinput.h"

char *progclass = "XScreenSaver";
Bool debug_p = True;

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define RST "\x1B[0m"
#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YEL "\x1B[33m"
#define BLU "\x1B[34m"
#define MAG "\x1B[35m"
#define CYN "\x1B[36m"
#define WHT "\x1B[37m"
#define BLD "\x1B[1m"
#define UL  "\x1B[4m"

static const char *
blurb(void)
{
  static char buf[255] = { 0 };
  struct tm *tm;
  struct timeval tv;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday (&tv, &tzp);
# else
  gettimeofday (&tv);
# endif
  tm = localtime (&tv.tv_sec);
  sprintf (buf, "%s: %02d:%02d:%02d.%03lu", progname,
           tm->tm_hour, tm->tm_min, tm->tm_sec, 
           (unsigned long) (tv.tv_usec / 1000));
  return buf;
}


static void
ungrab_timer (XtPointer closure, XtIntervalId *id)
{
  Display *dpy = (Display *) closure;
  fprintf (stdout, "\n%s: ungrabbing\n\n", blurb());
  XUngrabKeyboard (dpy, CurrentTime);
  XUngrabPointer (dpy, CurrentTime);
}


static const char *
grab_string (int status)
{
  switch (status) {
  case GrabSuccess:     return "GrabSuccess";
  case AlreadyGrabbed:  return "AlreadyGrabbed";
  case GrabInvalidTime: return "GrabInvalidTime";
  case GrabNotViewable: return "GrabNotViewable";
  case GrabFrozen:      return "GrabFrozen";
  default:
    {
      static char buf[255];
      sprintf(buf, "unknown status: %d", status);
      return buf;
    }
  }
}


typedef enum { 
  ETYPE, ETIME, ESERIAL, EROOT, EWIN, ESUB, EX, EY, EXR, EYR,
  EFLAGS, ESTATE, EKEYCODE, EKEY, EHINT, ESSCR,
  EEND
} coltype;

typedef struct {
  const char *name;
  int width;
  enum { TSTR, TSTRL, TINT, TUINT, THEX, TTIME } type;
} columns;


static const columns cols[] = {
  /* ETYPE	*/ { "Type", 	  17, TSTRL },
  /* ETIME	*/ { "Timestamp", 11, TTIME },
  /* ESERIAL	*/ { "Serial",     9, TUINT },
  /* EROOT	*/ { "Root", 	   8, THEX },
  /* EWIN	*/ { "Window",	   8, THEX },
  /* ESUB	*/ { "Subwin", 	   8, THEX },
  /* EX		*/ { "X", 	  11, TINT },
  /* EY		*/ { "Y", 	  11, TINT },
  /* EXR	*/ { "X Root", 	  11, TINT },
  /* EYR	*/ { "Y Root", 	  11, TINT },
  /* EFLAGS	*/ { "Flags", 	   8, THEX },
  /* ESTATE	*/ { "State", 	   8, THEX },
  /* EKEYCODE	*/ { "Code", 	   5, THEX },
  /* EKEY	*/ { "Key", 	   3, TSTR },
  /* EHINT	*/ { "Hint", 	   9, TINT },
  /* ESSCR	*/ { "Same", 	   4, TINT },
};

static struct { Time t; XEvent e; } history[100] = { 0, };

static void
print_header (void)
{
  char buf[1024] = { 0 };
  char *s = buf;
  coltype t;

  if (countof(cols) != EEND) abort();
  for (t = 0; t < EEND; t++)
    {
      if (t > 0) *s++ = ' ';
      if (cols[t].type == TSTRL)
        sprintf (s, "%-*s", cols[t].width, cols[t].name);
      else
        sprintf (s, "%*s",  cols[t].width, cols[t].name);
      s += strlen(s);
    }
  *s++ = '\n';
  for (t = 0; t < EEND; t++)
    {
      int i;
      if (t > 0) *s++ = ' ';
      for (i = 0; i < cols[t].width; i++)
        *s++ = '=';
    }
  fprintf (stdout, "\n%s\n", buf);
}


static void
print_field (char *out, coltype t, void *val)
{
  const columns *col = &cols[t];
  if (! val)
    {
      sprintf (out, "%*s", col->width, "-");
      return;
    }

  switch (col->type) {
  case TSTRL: sprintf (out, "%-*s", col->width, (char *) val); break;
  case TSTR:  sprintf (out, "%*s", col->width, (char *) val); break;
  case TINT:  sprintf (out, "%*d", col->width, *((int *) val)); break;
  case TUINT: sprintf (out, "%*u", col->width, *((unsigned int *) val)); break;
  case THEX:  sprintf (out, "%*X", col->width, *((unsigned int *) val)); break;
  case TTIME: sprintf (out, "%*.3f", col->width, 
                       (double) (*((unsigned int *) val)) / 1000.0); break;
  default: abort(); break;
  }
}


static void
push_history (Time t, XEvent *xev)
{
  memmove (history + 1, history, sizeof(history) - sizeof(*history));
  history[0].t = t;
  history[0].e = *xev;
}


static void
asciify (char *c, int L)
{
  if      (!strcmp(c,"\n")) strcpy(c, "\\n");
  else if (!strcmp(c,"\r")) strcpy(c, "\\r");
  else if (!strcmp(c,"\t")) strcpy(c, "\\t");
  else if (!strcmp(c," "))  strcpy(c, "SPC");
  else if (L == 1 && c[0] < ' ')
    sprintf (c, "^%c", c[0] + '@');
}


static Bool error_handler_hit_p = False;

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  return 0;
}


/* Whether the field's value is sane. */
static Bool
validate_field (Display *dpy, coltype t, void *val)
{
  const char *err = 0;
  if (!val) return True;

  switch (t) {
  case ETIME:
    {
      Time t = *(Time *) val;
      int i;
      for (i = 0; i < countof(history); i++)
        {
          if (t && t == history[i].t)
            {
              err = "DUP TIME";
              break;
            }
          else if (t && t < history[i].t)
            {
              err = "TIME TRAVEL";
              break;
            }
        }
    }
    break;

  case ESERIAL:
    {
      unsigned long s = *(unsigned long *) val;
      int i;
      for (i = 0; i < countof(history); i++)
        {
          if (s && s == history[i].e.xany.serial)
            {
              err = "DUP SERIAL";
              break;
            }
          else if (s && s < history[i].e.xany.serial)
            {
              err = "TIME TRAVEL";
              break;
            }
        }
    }
    break;

  case EROOT: case EWIN: case ESUB:
    {
      Window w = *(Window *) val;
      if (w)
        {
          XWindowAttributes xgwa;
          error_handler_hit_p = False;
          XSync (dpy, False);
          XSetErrorHandler (ignore_all_errors_ehandler);
          XSync (dpy, False);
          XGetWindowAttributes (dpy, w, &xgwa);
          XSync (dpy, False);
          if (error_handler_hit_p)
            {
              err = "BAD WINDOW";
              break;
            }
        }
    }

  case EX: case EY: case EXR: case EYR:
    {
      int i = *(int *) val;
      if (i < 0 || i > 0xFFFF)
        {
          err = "BAD COORD";
          break;
        }
    }
    break;

  case EFLAGS:
    {
      /* "The only defined flag is XIKeyRepeat for XI_KeyPress events." */
      int i = *(int *) val;
      if (i != 0 && 1 != XIKeyRepeat)
        {
          err = "BAD BOOL";
          break;
        }
    }
    break;

  case ESTATE:
    {
      unsigned int i = *(int *) val;

      if (i & ~(ShiftMask | LockMask | ControlMask | Mod1Mask |
                Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask |
                Button1Mask | Button2Mask | Button3Mask |
                Button4Mask | Button5Mask))
        {
          err = "BAD MODS";
          break;
        }
    }
    break;

  case EKEYCODE:
    {
      int i = *(int *) val;
      if (i < 0 || i > 0xFFFF)
        {
          err = "BAD KEYCODE";
          break;
        }
    }
    break;

  case EHINT: case ESSCR:
    {
      int i = *(int *) val;
      if (i != 0 && 1 != 1)
        {
          err = "BAD BOOL";
          break;
        }
    }
    break;

  default:
    break;
  }

  if (err) return False;
  return True;
}


static void
print_event (Display *dpy, XEvent *xev, int xi_opcode)
{
  XIRawEvent *re = 0;
  XIDeviceEvent *de = 0;
  void *fields[EEND] = { 0, };

  switch (xev->xany.type) {
  case KeyPress:
  case KeyRelease:
    {
      static XComposeStatus compose = { 0, };
      KeySym keysym = 0;
      static char c[100];
      int n;

      fields[ETYPE] = (void *)
        (xev->xany.type == KeyPress ? "KeyPress" : "KeyRelease");
      fields[ETIME]    = &xev->xkey.time;
      fields[ESERIAL]  = &xev->xkey.serial;
      fields[EWIN]     = &xev->xkey.window;
      fields[EROOT]    = &xev->xkey.root;
      fields[ESUB]     = &xev->xkey.subwindow;
      fields[EX]       = &xev->xkey.x;
      fields[EY]       = &xev->xkey.y;
      fields[EXR]      = &xev->xkey.x_root;
      fields[EYR]      = &xev->xkey.y_root;
      fields[ESTATE]   = &xev->xkey.state;
      fields[EKEYCODE] = &xev->xkey.keycode;

      n = XLookupString (&xev->xkey, c, sizeof(c)-1, &keysym, &compose);
      c[n] = 0;
      asciify (c, n);
      fields[EKEY] = c;
    }
    break;
  case ButtonPress:
  case ButtonRelease:
    {
      fields[ETYPE] = (void *)
        (xev->xany.type == ButtonPress ? "ButtonPress" : "ButtonRelease");
      fields[ETIME]    = &xev->xbutton.time;
      fields[ESERIAL]  = &xev->xbutton.serial;
      fields[EWIN]     = &xev->xbutton.window;
      fields[EROOT]    = &xev->xbutton.root;
      fields[ESUB]     = &xev->xbutton.subwindow;
      fields[EX]       = &xev->xbutton.x;
      fields[EY]       = &xev->xbutton.y;
      fields[EXR]      = &xev->xbutton.x_root;
      fields[EYR]      = &xev->xbutton.y_root;
      fields[ESTATE]   = &xev->xbutton.state;
      fields[EKEYCODE] = &xev->xbutton.button;
    }
    break;
  case MotionNotify:
    {
      fields[ETYPE] = (void *) "MotionNotify";
      fields[ETIME]    = &xev->xmotion.time;
      fields[ESERIAL]  = &xev->xmotion.serial;
      fields[EWIN]     = &xev->xmotion.window;
      fields[EROOT]    = &xev->xmotion.root;
      fields[ESUB]     = &xev->xmotion.subwindow;
      fields[EX]       = &xev->xmotion.x;
      fields[EY]       = &xev->xmotion.y;
      fields[EXR]      = &xev->xmotion.x_root;
      fields[EYR]      = &xev->xmotion.y_root;
      fields[ESTATE]   = &xev->xmotion.state;
      fields[EHINT]    = &xev->xmotion.is_hint;
    }
    break;
  case GenericEvent:
    break;
  case EnterNotify:
  case LeaveNotify:
    break;
  default:
    {
      static char ee[100];
      sprintf (ee, "EVENT %2d", xev->xany.type);
      fields[ETYPE]   = &ee;
      fields[ESERIAL] = &xev->xany.serial;
      fields[EWIN]    = &xev->xany.window;
    }
    break;
  }

  if (xev->xcookie.type != GenericEvent ||
      xev->xcookie.extension != xi_opcode)
    goto DONE;  /* not an XInput event */
  if (!xev->xcookie.data)
    XGetEventData (dpy, &xev->xcookie);
  if (!xev->xcookie.data)
    {
      fields[ETYPE] = "BAD XIINPUT";
      goto DONE;
    }

  re = xev->xcookie.data;

  if (xev->xany.serial != re->serial) abort();

  switch (xev->xcookie.evtype) {
  case XI_RawKeyPress:      fields[ETYPE] = "XI_RawKeyPress";   break;
  case XI_RawKeyRelease:    fields[ETYPE] = "XI_RawKeyRelease"; break;
  case XI_RawButtonPress:   fields[ETYPE] = "XI_RawBtnPress";   break;
  case XI_RawButtonRelease: fields[ETYPE] = "XI_RawBtnRelease"; break;
  case XI_RawMotion:        fields[ETYPE] = "XI_RawMotion";     break;
  case XI_RawTouchBegin:    fields[ETYPE] = "XI_RawTouchBegin"; break;
  case XI_RawTouchEnd:      fields[ETYPE] = "XI_RawTouchEnd";   break;
  case XI_RawTouchUpdate:   fields[ETYPE] = "XI_RawTouchUpd";   break;
  case XI_KeyPress:         fields[ETYPE] = "XI_KeyPress";      break;
  case XI_KeyRelease:       fields[ETYPE] = "XI_KeyRelease";    break;
  case XI_ButtonPress:      fields[ETYPE] = "XI_BtnPress";      break;
  case XI_ButtonRelease:    fields[ETYPE] = "XI_BtnRelease";    break;
  case XI_Motion:           fields[ETYPE] = "XI_Motion";        break;
  case XI_TouchBegin:       fields[ETYPE] = "XI_TouchBegin";    break;
  case XI_TouchEnd:         fields[ETYPE] = "XI_TouchEnd";      break;
  case XI_TouchUpdate:      fields[ETYPE] = "XI_TouchUpd";      break;
  default:
    {
      static char ee[100];
      sprintf (ee, "XI EVENT %2d", xev->xany.type);
      fields[ETYPE]   = &ee;
    }
    break;
  }

  fields[ESERIAL]  = &xev->xany.serial;
  fields[ETIME]    = &re->time;
  fields[EFLAGS]   = &re->flags;
  fields[EKEYCODE] = &re->detail;

  /* Only these events are XIDeviceEvents. The "XI_Raw" variants are not.
   */
  switch (xev->xcookie.evtype) {
  case XI_KeyPress:
  case XI_KeyRelease:
  case XI_ButtonPress:
  case XI_ButtonRelease:
  case XI_Motion:
  case XI_TouchBegin:
  case XI_TouchEnd:
  case XI_TouchUpdate:
    de = (XIDeviceEvent *) re;
    fields[EROOT]  = &de->root;
    fields[EWIN]   = &de->event;
    fields[ESUB]   = &de->child;
    fields[EX]     = &de->event_x;
    fields[EY]     = &de->event_y;
    fields[EXR]    = &de->root_x;
    fields[EYR]    = &de->root_y;
    fields[ESTATE] = &de->mods.effective;
    break;
  default: break;
  }

  switch (xev->xcookie.evtype) {
  case XI_RawKeyPress:
  case XI_RawKeyRelease:
  case XI_KeyPress:
  case XI_KeyRelease:
    {
      XKeyEvent xkey = { 0, };
      static XComposeStatus compose = { 0, };
      KeySym keysym = 0;
      static char c[100];
      int n;
      xkey.type    = ((xev->xcookie.evtype == XI_RawKeyPress ||
                       xev->xcookie.evtype == XI_KeyPress)
                      ? KeyPress : KeyRelease);
      xkey.serial  = xev->xany.serial;
      xkey.display = xev->xany.display;
      xkey.window  = 0; /* xev->xany.window; */
      xkey.keycode = re->detail;

      if (de)  /* Available for non-raw events only */
        {
          xkey.root      = de->root;
          xkey.subwindow = 0; /* de->child; */
          xkey.time      = de->time;
          xkey.state     = de->mods.effective;
        }

      n = XLookupString (&xkey, c, sizeof(c)-1, &keysym, &compose);
      c[n] = 0;
      asciify (c, n);
      fields[EKEY] = c;
    }
    break;
  case XI_RawButtonPress:
  case XI_RawButtonRelease:
  case XI_ButtonPress:
  case XI_ButtonRelease:
    {
      static char c[10];
      sprintf (c, "b%d", re->detail);
      fields[EKEY] = &c;
    }
    break;
  default:
    break;
  }

 DONE:

  if (fields[ETYPE])
    {
      char buf[10240];
      char *s = buf;
      coltype t;
      *s = 0;
      for (t = 0; t < EEND; t++)
        {
          Bool ok = validate_field (dpy, t, fields[t]);

          if (t > 0) { *s++ = ' '; *s = 0; }

          if (!ok)
            {
              strcat (s, BLD);
              strcat (s, RED);
              s += strlen(s);
            }

          print_field (s, t, fields[t]);
          s += strlen(s);

          if (!ok)
            {
              strcat (s, RST);
              s += strlen(s);
            }
        }
      fprintf (stdout, "%s\n", buf);
    }

  {
    Time t = (fields[ETIME] ? * (Time *) fields[ETIME] : 0);
    push_history (t, xev);
  }
}


int
main (int argc, char **argv)
{
  XtAppContext app;
  Widget toplevel_shell;
  Display *dpy;
  int xi_opcode;
  Bool grab_kbd_p   = False;
  Bool grab_mouse_p = False;
  Bool mouse_sync_p = False;
  Bool kbd_sync_p   = False;
  int i;
  char *s;

  progname = argv[0];
  s = strrchr (progname, '/');
  if (s) progname = s+1;

  for (i = 1; i < argc; i++)
    {
      const char *oa = argv[i];
      if (argv[i][0] == '-' && argv[i][1] == '-')
        argv[i]++;
      if (!strcmp (argv[i], "-grab"))
        grab_kbd_p = grab_mouse_p = True;
      else if (!strcmp (argv[i], "-grab-kbd") ||
               !strcmp (argv[i], "-grab-keyboard"))
        grab_kbd_p = True;
      else if (!strcmp (argv[i], "-grab-mouse") ||
               !strcmp (argv[i], "-grab-pointer"))
        grab_mouse_p = True;
      else if (!strcmp (argv[i], "-kbd-sync") ||
               !strcmp (argv[i], "-keyboard-sync"))
        kbd_sync_p = True;
      else if (!strcmp (argv[i], "-kbd-async") ||
               !strcmp (argv[i], "-keyboard-async"))
        kbd_sync_p = False;
      else if (!strcmp (argv[i], "-mouse-sync") ||
               !strcmp (argv[i], "-pointer-sync"))
        mouse_sync_p = True;
      else if (!strcmp (argv[i], "-mouse-async") ||
               !strcmp (argv[i], "-pointer-async"))
        mouse_sync_p = False;
      else if (!strcmp (argv[i], "-sync"))
        kbd_sync_p = mouse_sync_p = True;
      else if (!strcmp (argv[i], "-async"))
        kbd_sync_p = mouse_sync_p = False;
      else
        {
          fprintf (stderr, "%s: unknown option: %s\n", blurb(), oa);
          fprintf (stderr, "usage: %s "
                            "[--grab]       [--sync       | --async]"
                   "\n\t\t   [--grab-kbd]   [--kbd-sync   | --kbd-async]"
                   "\n\t\t   [--grab-mouse] [--mouse-sync | --mouse-async]\n",
                   progname);
          exit (1);
        }
    }

  toplevel_shell = XtAppInitialize (&app, progclass, 0, 0, 
                                    &argc, argv, 0, 0, 0);
  dpy = XtDisplay (toplevel_shell);
  if (!dpy) exit(1);

  if (! init_xinput (dpy, &xi_opcode))
    exit (1);

  fprintf (stdout, "\n%s: Make your window wide. "
           "Bogus values are " BLD RED "RED" RST ".\n",
           blurb());

  if (grab_kbd_p || grab_mouse_p)
    {
      int timeout = 15;
      Window w = RootWindow (dpy, 0);
      int status;
      XColor black = { 0, };
      Pixmap bit = XCreateBitmapFromData (dpy, w, "\000", 1, 1);
      Cursor cursor = XCreatePixmapCursor (dpy, bit, bit, &black, &black, 0, 0);

      if (grab_kbd_p)
        {
          status = XGrabKeyboard (dpy, w, True,
                                  (mouse_sync_p ? GrabModeSync : GrabModeAsync),
                                  (kbd_sync_p   ? GrabModeSync : GrabModeAsync),
                                  CurrentTime);
          if (status == GrabSuccess)
            fprintf (stdout, "%s: grabbed keyboard (%s, %s)\n", blurb(),
                     (mouse_sync_p ? "sync" : "async"),
                     (kbd_sync_p   ? "sync" : "async"));
          else
            {
              fprintf (stdout, "%s: failed to grab keyboard (%s, %s): %s\n",
                       blurb(),
                       (mouse_sync_p ? "sync" : "async"),
                       (kbd_sync_p   ? "sync" : "async"),
                       grab_string (status));
              exit(1);
            }
        }

      if (grab_mouse_p)
        {
          status = XGrabPointer (dpy, w, True, 
                                 (ButtonPressMask   | ButtonReleaseMask |
                                  EnterWindowMask   | LeaveWindowMask |
                                  PointerMotionMask | PointerMotionHintMask |
                                  Button1MotionMask | Button2MotionMask |
                                  Button3MotionMask | Button4MotionMask |
                                  Button5MotionMask | ButtonMotionMask),
                                 (mouse_sync_p ? GrabModeSync : GrabModeAsync),
                                 (kbd_sync_p   ? GrabModeSync : GrabModeAsync),
                                 w, cursor, CurrentTime);
          if (status == GrabSuccess)
            fprintf (stdout, "%s: grabbed mouse (%s, %s)\n", blurb(),
                     (mouse_sync_p ? "sync" : "async"),
                     (kbd_sync_p   ? "sync" : "async"));
          else
            {
              fprintf (stdout, "%s: failed to grab mouse (%s, %s): %s\n",
                       blurb(),
                       (mouse_sync_p ? "sync" : "async"),
                       (kbd_sync_p   ? "sync" : "async"),
                       grab_string (status));
              exit(1);
            }
        }

      fprintf (stdout, "%s: ungrabbing in %d seconds\n", blurb(), timeout);
      XtAppAddTimeOut (app, 1000 * timeout, ungrab_timer, (XtPointer) dpy);
    }

  print_header();
  while (1)
    {
      XEvent xev = { 0, };
      XtAppNextEvent (app, &xev);
      XtDispatchEvent (&xev);
      print_event (dpy, &xev, xi_opcode);
      XFreeEventData (dpy, &xev.xcookie);
    }

  exit (0);
}
