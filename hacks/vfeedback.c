/* vfeedback, Copyright (c) 2018 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Simulates video feedback: pointing a video camera at an NTSC television.
 *
 * Created: 4-Aug-2018.
 *
 * TODO:
 *
 * - Figure out better UI gestures on mobile to pan, zoom and rotate.
 *
 * - When zoomed in really far, grab_rectangle should decompose pixels
 *   into RGB phosphor dots.
 *
 * - Maybe load an image and chroma-key it, letting transparency bleed,
 *   for that Amiga Genlock, Cabaret Voltaire look.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include "screenhack.h"
#include "analogtv.h"

#include <time.h>

#undef DEBUG
#undef DARKEN

#ifdef DEBUG
# include "ximage-loader.h"
# include "images/gen/testcard_bbcf_png.h"
#endif

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define RANDSIGN() ((random() & 1) ? 1 : -1)

struct state {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  int w, h;
  Pixmap pix;
  GC gc;
  double start, last_time;
  double noise;
  double zoom, rot;
  double value, svalue, speed, dx, dy, ds, dth;

  struct { double x, y, w, h, th; } rect, orect;
  struct { int x, y, s; } specular;

  enum { POWERUP, IDLE, MOVE } state;
  analogtv *tv;
  analogtv_reception rec;
  Bool button_down_p;
  int mouse_x, mouse_y;
  double mouse_th;
  Bool dragmode;

# ifdef DEBUG
  XImage *tcimg;
# endif

};


static void
twiddle_knobs (struct state *st)
{
  st->rec.ofs = random() % ANALOGTV_SIGNAL_LEN;		/* roll picture once */
  st->rec.level = 0.8 + frand(1.0);  /* signal strength (low = dark, static) */
  st->tv->color_control    = frand(1.0) * RANDSIGN();
  st->tv->contrast_control = 0.4 + frand(1.0);
  st->tv->tint_control     = frand(360);
}


static void
twiddle_camera (struct state *st)
{
# if 0
  st->rect.x  = 0;
  st->rect.y  = 0;
  st->rect.w  = 1;
  st->rect.h  = 1;
  st->rect.th = 0;
# else
  st->rect.x = frand(0.1) * RANDSIGN();
  st->rect.y = frand(0.1) * RANDSIGN();
  st->rect.w = st->rect.h = 1 + frand(0.4) * RANDSIGN();
  st->rect.th = 0.2 + frand(1.0) * RANDSIGN();
# endif
}


static void *
vfeedback_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;

  st->dpy = dpy;
  st->window = window;
  st->tv = analogtv_allocate (st->dpy, st->window);
  analogtv_set_defaults (st->tv, "");
  st->tv->need_clear = 1;
  st->rec.input = analogtv_input_allocate();
  analogtv_setup_sync (st->rec.input, 1, 0);
  st->tv->use_color = 1;
  st->tv->powerup = 0;
  st->rec.multipath = 0;
  twiddle_camera (st);
  twiddle_knobs (st);
  st->noise = get_float_resource (st->dpy, "noise", "Float");
  st->speed = get_float_resource (st->dpy, "speed", "Float");

  XGetWindowAttributes (dpy, window, &st->xgwa);

  st->state = POWERUP;
  st->value = 0;

  st->w = 640;
  st->h = 480;
  gcv.foreground = get_pixel_resource (st->dpy, st->xgwa.colormap,
                                       "foreground", "Foreground");
  st->gc = XCreateGC (dpy, st->window, GCForeground, &gcv);

  st->orect = st->rect;

# ifdef DEBUG
  {
    int w, h;
    Pixmap p;
    p = image_data_to_pixmap (dpy, window,
                              testcard_bbcf_png, sizeof(testcard_bbcf_png),
                              &w, &h, 0);
    st->tcimg = XGetImage (dpy, p, 0, 0, w, h, ~0L, ZPixmap);
    XFreePixmap (dpy, p);
  }
# endif

# ifndef HAVE_JWXYZ
  XSelectInput (dpy, window,
                PointerMotionMask | st->xgwa.your_event_mask);
# endif

  return st;
}


static double
ease_fn (double r)
{
  return cos ((r/2 + 1) * M_PI) + 1; /* Smooth curve up, end at slope 1. */
}


static double
ease_ratio (double r)
{
  double ease = 0.5;
  if      (r <= 0)     return 0;
  else if (r >= 1)     return 1;
  else if (r <= ease)  return     ease * ease_fn (r / ease);
  else if (r > 1-ease) return 1 - ease * ease_fn ((1 - r) / ease);
  else                 return r;
}


static XImage *
grab_rectangle (struct state *st)
{
  XImage *in, *out;

  /* Under XQuartz we can't just do XGetImage on the Window, we have to
     go through an intermediate Pixmap first.  I don't understand why.
   */
  if (! st->pix)
    st->pix = XCreatePixmap (st->dpy, st->window, 
                             st->xgwa.width, st->xgwa.height, st->xgwa.depth);

  XCopyArea (st->dpy, st->window, st->pix, st->gc, 0, 0,
             st->xgwa.width, st->xgwa.height, 0, 0);

  if (st->specular.s)
    {
      double p = 0.2;
      double r = (st->svalue <    p ? st->svalue/p :
                  st->svalue >= 1-p ? (1-st->svalue)/p :
                  1);
      double s = st->specular.s * ease_ratio (r * 2);
      XFillArc (st->dpy, st->pix, st->gc,
                st->specular.x - s/2,
                st->specular.y - s/2,
                s, s, 0, 360*64);
    }

# ifdef DEBUG
  in = st->tcimg;
# else
  in = XGetImage (st->dpy, st->pix,
                  0, 0, st->xgwa.width, st->xgwa.height,
                  ~0L, ZPixmap);
  /* Could actually use st->tv->image here, except we don't have the
     subrectangle being used (overall_top, usewidth, etc.) */
# endif

  out = XCreateImage (st->dpy, st->xgwa.visual, st->xgwa.depth,
                      ZPixmap, 0, NULL,
                      st->w, st->h, 8, 0);

  if (! in) abort();
  if (! out) abort();
  out->data = (char *) calloc (out->height, out->bytes_per_line);
  if (! out->data) abort();

  {
    double C = cos (st->rect.th);
    double S = sin (st->rect.th);
    unsigned long black = BlackPixelOfScreen (st->xgwa.screen);
    int ox, oy;
    for (oy = 0; oy < out->height; oy++)
      {
        double doy = (double) oy / out->height;
        double diy = st->rect.h * doy + st->rect.y - 0.5;

        float dix_mul = (float) st->rect.w / out->width;
        float dix_add = (-0.5 + st->rect.x) * st->rect.w;
        float ix_add = (-diy * S + 0.5) * in->width;
        float iy_add = ( diy * C + 0.5) * in->height;
        float ix_mul = C * in->width;
        float iy_mul = S * in->height;

        ix_add += dix_add * ix_mul;
        iy_add += dix_add * iy_mul;
        ix_mul *= dix_mul;
        iy_mul *= dix_mul;

        if (in->bits_per_pixel == 32 &&
            out->bits_per_pixel == 32)
          {
            /* Unwrapping XGetPixel and XPutPixel gains us several FPS here */
            uint32_t *out_line =
              (uint32_t *) (out->data + out->bytes_per_line * oy);
            for (ox = 0; ox < out->width; ox++)
              {
                float dix = ox;
                int ix = dix * ix_mul + ix_add;
                int iy = dix * iy_mul + iy_add;
                unsigned long p = (ix >= 0 && ix < in->width &&
                                   iy >= 0 && iy < in->height
                                   ? ((uint32_t *)
                                      (in->data + in->bytes_per_line * iy))[ix]
                                   : black);
# ifdef HAVE_JWXYZ
                p |= black;   /* We get 0 instead of BlackPixel... */
# endif
                out_line[ox] = p;
              }
          }
        else
          for (ox = 0; ox < out->width; ox++)
            {
              float dix = ox;
              int ix = dix * ix_mul + ix_add;
              int iy = dix * iy_mul + iy_add;
              unsigned long p = (ix >= 0 && ix < in->width &&
                                 iy >= 0 && iy < in->height
                                 ? XGetPixel (in, ix, iy)
                                 : black);
# ifdef HAVE_JWXYZ
              p |= black;   /* We get 0 instead of BlackPixel... */
# endif
              XPutPixel (out, ox, oy, p);
            }
      }
  }

# ifndef DEBUG
  XDestroyImage (in);
# endif

  return out;
}


static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}


static unsigned long
vfeedback_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  const analogtv_reception *rec = &st->rec;
  double then = double_time(), now, timedelta;
  XImage *img = 0;

  switch (st->state) {
  case POWERUP: case IDLE: break;
  case MOVE:
    st->rect.x  = st->orect.x  + st->dx  * ease_ratio (st->value);
    st->rect.y  = st->orect.y  + st->dy  * ease_ratio (st->value);
    st->rect.th = st->orect.th + st->dth * ease_ratio (st->value);
    st->rect.w  = st->orect.w * (1 + (st->ds * ease_ratio (st->value)));
    st->rect.h  = st->orect.h * (1 + (st->ds * ease_ratio (st->value)));
    break;
  default:
    abort();
    break;
  }

  if (! st->button_down_p)
    {
      st->value  += 0.03 * st->speed;
      if (st->value > 1 || st->state == POWERUP)
        {
          st->orect = st->rect;
          st->value = 0;
          st->dx = st->dy = st->ds = st->dth = 0;

          switch (st->state) {
          case POWERUP:
            /* Wait until the monitor has warmed up before turning on
               the camcorder? */
            /* if (st->tv->powerup > 4.0) */
              st->state = IDLE;
            break;
          case IDLE:
            st->state = MOVE;
            if (! (random() % 5))
              st->ds = frand(0.2) * RANDSIGN();		/* zoom */
            if (! (random() % 3))
              st->dth = frand(0.2) * RANDSIGN();	/* rotate */
            if (! (random() % 8))
              st->dx = frand(0.05) * RANDSIGN(),	/* pan */
              st->dy = frand(0.05) * RANDSIGN();
            if (! (random() % 2000))
              {
                twiddle_knobs (st);
                if (! (random() % 10))
                  twiddle_camera (st);
              }
            break;
          case MOVE:
            st->state = IDLE;
            st->value = 0.3;
            break;
          default:
            abort();
            break;
          }
        }

      /* Draw a specular reflection somewhere on the screen, to mix it up
         with a little noise from environmental light.
       */
      if (st->specular.s)
        {
          st->svalue += 0.01 * st->speed;
          if (st->svalue > 1)
            {
              st->svalue = 0;
              st->specular.s = 0;
            }
        }
      else if (! (random() % 300))
        {
# if 1
          /* Center on the monitor's screen, depth 1 */
          int cx = st->xgwa.width / 2;
          int cy = st->xgwa.height / 2;
# else
          /* Center on the monitor's screen, depth 0 -- but this clips. */
          int cx = (st->rect.x + st->rect.w / 2) * st->xgwa.width;
          int cy = (st->rect.y + st->rect.h / 2) * st->xgwa.height;
# endif
          int ww = 4 + (st->rect.h * st->xgwa.height) / 12;
          st->specular.x = cx + (random() % ww) * RANDSIGN();
          st->specular.y = cy + (random() % ww) * RANDSIGN();
          st->specular.s = ww * (0.8 + frand(0.4));
          st->svalue = 0;
        }
    }

  if (st->last_time == 0)
    st->start = then;

  if (st->state != POWERUP)
    {
      img = grab_rectangle (st);
      analogtv_load_ximage (st->tv, st->rec.input, img, 0, 0, 0, 0, 0);
    }

  analogtv_reception_update (&st->rec);
  analogtv_draw (st->tv, st->noise, &rec, 1);
  if (img)
    XDestroyImage (img);

  now = double_time();
  timedelta = (1 / 29.97) - (now - then);

  st->tv->powerup = then - st->start;
  st->last_time = then;

  return timedelta > 0 ? timedelta * 1000000 : 0;
}


static void
vfeedback_reshape (Display *dpy, Window window, void *closure, 
                    unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  analogtv_reconfigure (st->tv);
  XGetWindowAttributes (dpy, window, &st->xgwa);

  if (st->pix)
    {
      XFreePixmap (dpy, st->pix);
      st->pix = 0;
    }
}


static Bool
vfeedback_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  double i = 0.02;

  /* Pan with left button and no modifier keys.
     Rotate with other buttons, or left-with-modifiers.
   */
  if (event->xany.type == ButtonPress &&
      (event->xbutton.button == Button1 ||
       event->xbutton.button == Button2 ||
       event->xbutton.button == Button3))
    {
      st->button_down_p = True;
      st->mouse_x = event->xbutton.x;
      st->mouse_y = event->xbutton.y;
      st->mouse_th = st->rect.th;
      st->dragmode = (event->xbutton.button == Button1 && 
                      !event->xbutton.state);
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           (event->xbutton.button == Button1 ||
            event->xbutton.button == Button2 ||
            event->xbutton.button == Button3))
    {
      st->button_down_p = False;
      return True;
    }
  else if (event->xany.type == MotionNotify && st->button_down_p)
    {
      if (st->dragmode)
        {
          double dx = st->mouse_x - event->xmotion.x;
          double dy = st->mouse_y - event->xmotion.y;
          st->rect.x += dx / st->xgwa.width  * st->rect.w;
          st->rect.y += dy / st->xgwa.height * st->rect.h;
          st->mouse_x = event->xmotion.x;
          st->mouse_y = event->xmotion.y;
        }
      else
        {
          /* Angle between center and initial click */
          double a1 = -atan2 (st->mouse_y - st->xgwa.height / 2,
                              st->mouse_x - st->xgwa.width  / 2);
          /* Angle between center and drag position */
          double a2 = -atan2 (event->xmotion.y - st->xgwa.height / 2,
                              event->xmotion.x - st->xgwa.width  / 2);
          /* Cumulatively rotate by difference between them */
          st->rect.th = a2 - a1 + st->mouse_th;
        }
      goto OK;
    }

  /* Zoom with mouse wheel */

  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button6))
    {
      i = 1-i;
      goto ZZ;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button5 ||
            event->xbutton.button == Button7))
    {
      i = 1+i;
      goto ZZ;
    }
  else if (event->type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      switch (keysym) {
        /* pan with arrow keys */
      case XK_Up:    st->rect.y += i; goto OK; break;
      case XK_Down:  st->rect.y -= i; goto OK; break;
      case XK_Left:  st->rect.x += i; goto OK; break;
      case XK_Right: st->rect.x -= i; goto OK; break;
      default: break;
      }
      switch (c) {

        /* rotate with <> */
      case '<': case ',': st->rect.th += i; goto OK; break;
      case '>': case '.': st->rect.th -= i; goto OK; break;

        /* zoom with += */
      case '-': case '_':
        i = 1+i;
        goto ZZ;
      case '=': case '+':
        i = 1-i;
      ZZ:
        st->orect = st->rect;
        st->rect.w *= i;
        st->rect.h *= i;
        st->rect.x += (st->orect.w - st->rect.w) / 2;
        st->rect.y += (st->orect.h - st->rect.h) / 2;
        goto OK;
        break;

        /* tv controls with T, C, B, O */
      case 't': st->tv->tint_control       += 5;    goto OK; break;
      case 'T': st->tv->tint_control       -= 5;    goto OK; break;
      case 'c': st->tv->color_control      += 0.1;  goto OK; break;
      case 'C': st->tv->color_control      -= 0.1;  goto OK; break;
      case 'b': st->tv->brightness_control += 0.01; goto OK; break;
      case 'B': st->tv->brightness_control -= 0.01; goto OK; break;
      case 'o': st->tv->contrast_control   += 0.1;  goto OK; break;
      case 'O': st->tv->contrast_control   -= 0.1;  goto OK; break;
      case 'r': st->rec.level              += 0.01; goto OK; break;
      case 'R': st->rec.level              -= 0.01; goto OK; break;
      default: break;
      }
      goto NOPE;
    OK:
# if 0
      fprintf (stderr, " %.6f x %.6f @ %.6f, %.6f %.6f\t",
               st->rect.w, st->rect.h,
               st->rect.x, st->rect.y, st->rect.th);
      fprintf (stderr," T=%.2f C=%.2f B=%.2f O=%.2f R=%.3f\n",
               st->tv->tint_control,
               st->tv->color_control/* * 100*/,
               st->tv->brightness_control/* * 100*/,
               st->tv->contrast_control/* * 100*/,
               st->rec.level);
# endif
      st->value = 0;
      st->state = IDLE;
      st->orect = st->rect;
      return True;
    }

 NOPE:
  if (screenhack_event_helper (dpy, window, event))
    {
      /* SPC or RET re-randomize the TV controls. */
      twiddle_knobs (st);
      goto OK;
    }

  return False;
}


static void
vfeedback_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  analogtv_release (st->tv);
  if (st->pix)
    XFreePixmap (dpy, st->pix);
  XFreeGC (dpy, st->gc);
  free (st);
}


static const char *vfeedback_defaults [] = {

  ".foreground:  #CCCC44",
  ".background:  #000000",
  "*noise:       0.02",
  "*speed:       1.0",
  ANALOGTV_DEFAULTS
  "*TVBrightness: 1.5",
  "*TVContrast:   150",
  0
};

static XrmOptionDescRec vfeedback_options [] = {
  { "-noise",           ".noise",     XrmoptionSepArg, 0 },
  { "-speed",           ".speed",     XrmoptionSepArg, 0 },
  ANALOGTV_OPTIONS
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("VFeedback", vfeedback)
