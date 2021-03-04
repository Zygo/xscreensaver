/* fluidballs, Copyright (c) 2000 by Peter Birtles <peter@bqdesign.com.au>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Ported to X11 and xscreensaver by jwz, 27-Feb-2002.
 *
 * http://astronomy.swin.edu.au/~pbourke/modelling/fluid/
 *
 * Some physics improvements by Steven Barker <steve@blckknght.org>
 */

/* Future ideas:
 * Specifying a distribution in the ball sizes (with a gamma curve, possibly).
 * Brownian motion, for that extra touch of realism.
 *
 * It would be nice to detect when there are more balls than fit in
 * the window, and scale the number of balls back.  Useful for the
 * xscreensaver-settings preview, which is often too tight by default.
 */

#include <math.h>
#include "screenhack.h"
#include <stdio.h>

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
#include "xdbe.h"
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  int delay;

  Pixmap b, ba;	/* double-buffer to reduce flicker */
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  XdbeBackBuffer backb;
  Bool dbeclear_p;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

  GC draw_gc;		/* most of the balls */
  GC draw_gc2;		/* the ball being dragged with the mouse */
  GC erase_gc;
  XColor fg;
  XColor fg2;

  int count;		/* number of balls */
  float xmin, ymin;	/* rectangle of window, relative to root */
  float xmax, ymax;

  int mouse_ball;	/* index of ball being dragged, or 0 if none. */

  float tc;		/* time constant (time-warp multiplier) */
  float accx;		/* horizontal acceleration (wind) */
  float accy;		/* vertical acceleration (gravity) */

  float *vx,  *vy;	/* current ball velocities */
  float *px,  *py;	/* current ball positions */
  float *opx, *opy;	/* previous ball positions */
  float *r;		/* ball radiuses */

  float *m;		/* ball mass, precalculated */
  float e;		/* coeficient of elasticity */
  float max_radius;	/* largest radius of any ball */

  Bool random_sizes_p;  /* Whether balls should be various sizes up to max. */
  Bool shake_p;		/* Whether to mess with gravity when things settle. */
  Bool dbuf;            /* Whether we're using double buffering. */
  float shake_threshold;
  int time_since_shake;

  Bool fps_p;		/* Whether to draw some text at the bottom. */
  XftColor xft_fg;
  XftDraw *xftdraw;
  XftFont *font;
  int font_height;
  int font_baseline;
  int frame_count;
  int collision_count;
  char fps_str[1024];
  
  int time_tick;
  struct timeval last_time;

} b_state;


/* Draws the frames per second string */
static void 
draw_fps_string (b_state *state)
{  
  XFillRectangle (state->dpy, state->b, state->erase_gc,
		  0, state->xgwa.height - state->font_height*3 - 20,
		  state->xgwa.width, state->font_height*3 + 20);
  XftDrawStringUtf8 (state->xftdraw, &state->xft_fg, state->font,
		    10, state->xgwa.height - state->font_height*2 -
                    state->font_baseline - 10,
		    (FcChar8 *) state->fps_str, strlen(state->fps_str));
}

/* Finds the origin of the window relative to the root window, by
   walking up the window tree until it reaches the top.
 */
static void
window_origin (Display *dpy, Window window, int *x, int *y)
{
  XTranslateCoordinates (dpy, window, RootWindow (dpy, DefaultScreen (dpy)),
                         0, 0, x, y, &window);
}


/* Queries the window position to see if the window has moved or resized.
   We poll this instead of waiting for ConfigureNotify events, because
   when the window manager moves the window, only one ConfigureNotify
   comes in: at the end of the motion.  If we poll, we can react to the
   new position while the window is still being moved.  (Assuming the WM
   does OpaqueMove, of course.)
 */
static void
check_window_moved (b_state *state)
{
  float oxmin = state->xmin;
  float oxmax = state->xmax;
  float oymin = state->ymin;
  float oymax = state->ymax;
  int wx, wy;
  XGetWindowAttributes (state->dpy, state->window, &state->xgwa);
  window_origin (state->dpy, state->window, &wx, &wy);
  state->xmin = wx;
  state->ymin = wy;
  state->xmax = state->xmin + state->xgwa.width;
  state->ymax = state->ymin + state->xgwa.height - (state->font_height*3) -
    (state->font_height ? 22 : 0);

  if (state->dbuf && (state->ba))
    {
      if (oxmax != state->xmax || oymax != state->ymax)
	{
	  XFreePixmap (state->dpy, state->ba);
	  state->ba = XCreatePixmap (state->dpy, state->window, 
				     state->xgwa.width, state->xgwa.height,
				     state->xgwa.depth);
	  XFillRectangle (state->dpy, state->ba, state->erase_gc, 0, 0, 
			  state->xgwa.width, state->xgwa.height);
	  state->b = state->ba;
	}
    }
  else 
    {
      /* Only need to erase the window if the origin moved */
      if (oxmin != state->xmin || oymin != state->ymin)
	XClearWindow (state->dpy, state->window);
      else if (state->fps_p && oymax != state->ymax)
	XFillRectangle (state->dpy, state->b, state->erase_gc,
			0, state->xgwa.height - state->font_height*3,
			state->xgwa.width, state->font_height*3);
    }
}


/* Returns the position of the mouse relative to the root window.
 */
static void
query_mouse (b_state *state, int *x, int *y)
{
  Window root1, child1;
  int mouse_x, mouse_y, root_x, root_y;
  unsigned int mask;
  if (XQueryPointer (state->dpy, state->window, &root1, &child1,
                     &root_x, &root_y, &mouse_x, &mouse_y, &mask))
    {
      *x = root_x;
      *y = root_y;
    }
  else
    {
      *x = -9999;
      *y = -9999;
    }
}

/* Re-pick the colors of the balls, and the mouse-ball.
 */
static void
recolor (b_state *state)
{
  if (state->fg.flags)
    XFreeColors (state->dpy, state->xgwa.colormap, &state->fg.pixel, 1, 0);
  if (state->fg2.flags)
    XFreeColors (state->dpy, state->xgwa.colormap, &state->fg2.pixel, 1, 0);

  state->fg.flags  = DoRed|DoGreen|DoBlue;
  state->fg.red    = 0x8888 + (random() % 0x8888);
  state->fg.green  = 0x8888 + (random() % 0x8888);
  state->fg.blue   = 0x8888 + (random() % 0x8888);

  state->fg2.flags = DoRed|DoGreen|DoBlue;
  state->fg2.red   = 0x8888 + (random() % 0x8888);
  state->fg2.green = 0x8888 + (random() % 0x8888);
  state->fg2.blue  = 0x8888 + (random() % 0x8888);

  if (XAllocColor (state->dpy, state->xgwa.colormap, &state->fg))
    XSetForeground (state->dpy, state->draw_gc,  state->fg.pixel);

  if (XAllocColor (state->dpy, state->xgwa.colormap, &state->fg2))
    XSetForeground (state->dpy, state->draw_gc2, state->fg2.pixel);
}

/* Initialize the state structure and various X data.
 */
static void *
fluidballs_init (Display *dpy, Window window)
{
  int i;
  float extx, exty;
  b_state *state = (b_state *) calloc (1, sizeof(*state));
  XGCValues gcv;

  state->dpy = dpy;
  state->window = window;
  state->delay = get_integer_resource (dpy, "delay", "Integer");

  check_window_moved (state);

  state->dbuf = get_boolean_resource (dpy, "doubleBuffer", "Boolean");

# ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
  state->dbuf = False;
# endif

  if (state->dbuf)
    {
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
      state->dbeclear_p = get_boolean_resource (dpy, "useDBEClear", "Boolean");
      if (state->dbeclear_p)
        state->b = xdbe_get_backbuffer (dpy, window, XdbeBackground);
      else
        state->b = xdbe_get_backbuffer (dpy, window, XdbeUndefined);
      state->backb = state->b;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

      if (!state->b)
        {
          state->ba = XCreatePixmap (state->dpy, state->window, 
				     state->xgwa.width, state->xgwa.height,
				     state->xgwa.depth);
          state->b = state->ba;
        }
    }
  else
    {
      state->b = state->window;
    }

  /* Select ButtonRelease events on the external window, if no other app has
     already selected it (only one app can select it at a time: BadAccess. */
#if 0
  if (! (state->xgwa.all_event_masks & ButtonReleaseMask))
    XSelectInput (state->dpy, state->window,
                  state->xgwa.your_event_mask | ButtonReleaseMask);
#endif

  gcv.foreground = get_pixel_resource(state->dpy, state->xgwa.colormap,
                                      "foreground", "Foreground");
  gcv.background = get_pixel_resource(state->dpy, state->xgwa.colormap,
                                      "background", "Background");
  state->draw_gc = XCreateGC (state->dpy, state->b,
                              GCForeground|GCBackground, &gcv);

  gcv.foreground = get_pixel_resource(state->dpy, state->xgwa.colormap,
                                      "mouseForeground", "MouseForeground");
  state->draw_gc2 = XCreateGC (state->dpy, state->b,
                               GCForeground|GCBackground, &gcv);

  gcv.foreground = gcv.background;
  state->erase_gc = XCreateGC (state->dpy, state->b,
                               GCForeground|GCBackground, &gcv);


  if (state->ba) 
    XFillRectangle (state->dpy, state->ba, state->erase_gc, 0, 0, 
		    state->xgwa.width, state->xgwa.height);

  recolor (state);

  extx = state->xmax - state->xmin;
  exty = state->ymax - state->ymin;

  state->count = get_integer_resource (dpy, "count", "Count");
  if (state->count < 1) state->count = 20;

  state->max_radius = get_float_resource (dpy, "size", "Size") / 2;
  if (state->max_radius < 1.0) state->max_radius = 1.0;

  if (state->xgwa.width > 2560) state->max_radius *= 2;  /* Retina displays */

  if (state->xgwa.width < 100 || state->xgwa.height < 100) /* tiny window */
    {
      if (state->max_radius > 5)
        state->max_radius = 5;
    }

  state->random_sizes_p = get_boolean_resource (dpy, "random", "Random");

  /* If the initial window size is too small to hold all these balls,
     make fewer of them...
   */
  {
    float r = (state->random_sizes_p
               ? state->max_radius * 0.7
               : state->max_radius);
    float ball_area = M_PI * r * r;
    float balls_area = state->count * ball_area;
    float window_area = state->xgwa.width * state->xgwa.height;
    window_area *= 0.75;  /* don't pack it completely full */
    if (balls_area > window_area)
      state->count = window_area / ball_area;
  }

  state->accx = get_float_resource (dpy, "wind", "Wind");
  if (state->accx < -1.0 || state->accx > 1.0) state->accx = 0;

  state->accy = get_float_resource (dpy, "gravity", "Gravity");
  if (state->accy < -1.0 || state->accy > 1.0) state->accy = 0.01;

  state->e = get_float_resource (dpy, "elasticity", "Elacitcity");
  if (state->e < 0.2 || state->e > 1.0) state->e = 0.97;

  state->tc = get_float_resource (dpy, "timeScale", "TimeScale");
  if (state->tc <= 0 || state->tc > 10) state->tc = 1.0;

  state->shake_p = get_boolean_resource (dpy, "shake", "Shake");
  state->shake_threshold = get_float_resource (dpy, "shakeThreshold",
                                               "ShakeThreshold");
  state->time_tick = 999999;

# ifdef HAVE_MOBILE	/* Always obey real-world gravity */
  state->shake_p = False;
# endif


  state->fps_p = get_boolean_resource (dpy, "doFPS", "DoFPS");
  if (state->fps_p)
    {
      char *fontname = get_string_resource (dpy, "fpsFont", "Font");
      char *s;
      if (!fontname) fontname = "Courier bold 18";
      state->font =
        load_xft_font_retry (dpy, screen_number (state->xgwa.screen),
                             fontname);
      if (!state->font) abort();
      s = get_string_resource (state->dpy, "textColor", "Foreground");
      if (!s) s = strdup ("white");
      XftColorAllocName (state->dpy, state->xgwa.visual, state->xgwa.colormap,
                         s, &state->xft_fg);
      free (s);
      state->xftdraw = XftDrawCreate (state->dpy, state->window,
                                      state->xgwa.visual,
                                      state->xgwa.colormap);
      state->font_height = state->font->ascent + state->font->descent;
      state->font_baseline = state->font->descent;
    }

  state->m   = (float *) malloc (sizeof (*state->m)   * (state->count + 1));
  state->r   = (float *) malloc (sizeof (*state->r)   * (state->count + 1));
  state->vx  = (float *) malloc (sizeof (*state->vx)  * (state->count + 1));
  state->vy  = (float *) malloc (sizeof (*state->vy)  * (state->count + 1));
  state->px  = (float *) malloc (sizeof (*state->px)  * (state->count + 1));
  state->py  = (float *) malloc (sizeof (*state->py)  * (state->count + 1));
  state->opx = (float *) malloc (sizeof (*state->opx) * (state->count + 1));
  state->opy = (float *) malloc (sizeof (*state->opy) * (state->count + 1));

  for (i=1; i<=state->count; i++)
    {
      state->px[i] = frand(extx) + state->xmin;
      state->py[i] = frand(exty) + state->ymin;
      state->vx[i] = frand(0.2) - 0.1;
      state->vy[i] = frand(0.2) - 0.1;

      state->r[i] = (state->random_sizes_p
                     ? ((0.2 + frand(0.8)) * state->max_radius)
                     : state->max_radius);
      /*state->r[i] = pow(frand(1.0), state->sizegamma) * state->max_radius;*/

      /* state->m[i] = pow(state->r[i],2) * M_PI; */
      state->m[i] = pow(state->r[i],3) * M_PI * 1.3333;
    }

  memcpy (state->opx, state->px, sizeof (*state->opx) * (state->count + 1));
  memcpy (state->opy, state->py, sizeof (*state->opx) * (state->count + 1));

  return state;
}


/* Messes with gravity: permute "down" to be in a random direction.
 */
static void
shake (b_state *state)
{
  float a = state->accx;
  float b = state->accy;
  int i = random() % 4;

  switch (i)
    {
    case 0:
      state->accx = a;
      state->accy = b;
      break;
    case 1:
      state->accx = -a;
      state->accy = -b;
      break;
    case 2:
      state->accx = b;
      state->accy = a;
      break;
    case 3:
      state->accx = -b;
      state->accy = -a;
      break;
    default:
      abort();
      break;
    }

  state->time_since_shake = 0;
  recolor (state);
}


/* Look at the current time, and update state->time_since_shake.
   Draw the FPS display if desired.
 */
static void
check_wall_clock (b_state *state, float max_d)
{
  state->frame_count++;
  
#if 0
  if (state->time_tick++ > 20)  /* don't call gettimeofday() too often -- it's slow. */
#endif
    {
      struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
      struct timezone tzp;
      gettimeofday(&now, &tzp);
# else
      gettimeofday(&now);
# endif

      if (state->last_time.tv_sec == 0)
        state->last_time = now;

      state->time_tick = 0;
#if 0
      if (now.tv_sec == state->last_time.tv_sec)
        return;
#endif

      state->time_since_shake += (now.tv_sec - state->last_time.tv_sec);

# ifdef HAVE_MOBILE	/* Always obey real-world gravity */
      {
        float a = fabs (fabs(state->accx) > fabs(state->accy)
                        ? state->accx : state->accy);
        int rot = current_device_rotation();
        switch (rot) {
        case    0: case  360: state->accx =  0; state->accy =  a; break;
        case  -90:            state->accx = -a; state->accy =  0; break;
        case   90:            state->accx =  a; state->accy =  0; break;
        case  180: case -180: state->accx =  0; state->accy = -a; break;
        default: break;
        }
      }
# endif /* HAVE_MOBILE */

      if (state->fps_p) 
	{
	  float elapsed = ((now.tv_sec  + (now.tv_usec  / 1000000.0)) -
			   (state->last_time.tv_sec + (state->last_time.tv_usec / 1000000.0)));
	  float fps = state->frame_count / elapsed;
	  float cps = state->collision_count / elapsed;
	  
	  sprintf (state->fps_str,
                   "Collisions: %6.3f/frame  Max motion: %6.3f",
		   cps/fps, max_d);
	  
	  draw_fps_string(state);
	}

      state->frame_count = 0;
      state->collision_count = 0;
      state->last_time = now;
    }
}

/* Erases the balls at their previous positions, and draws the new ones.
 */
static void
repaint_balls (b_state *state)
{
  int a;
# ifndef HAVE_JWXYZ
  int x1a, x2a, y1a, y2a;
# endif
  int x1b, x2b, y1b, y2b;
  float max_d = 0;

#ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
  XClearWindow (state->dpy, state->b);
#endif

  for (a=1; a <= state->count; a++)
    {
      GC gc;
# ifndef HAVE_JWXYZ
      x1a = (state->opx[a] - state->r[a] - state->xmin);
      y1a = (state->opy[a] - state->r[a] - state->ymin);
      x2a = (state->opx[a] + state->r[a] - state->xmin);
      y2a = (state->opy[a] + state->r[a] - state->ymin);
# endif

      x1b = (state->px[a] - state->r[a] - state->xmin);
      y1b = (state->py[a] - state->r[a] - state->ymin);
      x2b = (state->px[a] + state->r[a] - state->xmin);
      y2b = (state->py[a] + state->r[a] - state->ymin);

#ifndef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
      if (!state->dbeclear_p || !state->backb)
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
	{
/*	  if (x1a != x1b || y1a != y1b)   -- leaves turds if we optimize this */
	    {
	      gc = state->erase_gc;
	      XFillArc (state->dpy, state->b, gc,
			x1a, y1a, x2a-x1a, y2a-y1a,
			0, 360*64);
	    }
	}
#endif /* !HAVE_JWXYZ */

      if (state->mouse_ball == a)
        gc = state->draw_gc2;
      else
        gc = state->draw_gc;

      XFillArc (state->dpy, state->b, gc,
                x1b, y1b, x2b-x1b, y2b-y1b,
                0, 360*64);

      if (state->shake_p)
        {
          /* distance this ball moved this frame */
          float d = ((state->px[a] - state->opx[a]) *
                     (state->px[a] - state->opx[a]) +
                     (state->py[a] - state->opy[a]) *
                     (state->py[a] - state->opy[a]));
          if (d > max_d) max_d = d;
        }

      state->opx[a] = state->px[a];
      state->opy[a] = state->py[a];
    }

  if (state->fps_p
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
      && (state->backb ? state->dbeclear_p : 1)
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
      )
    draw_fps_string(state);

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  if (state->backb)
    {
      XdbeSwapInfo info[1];
      info[0].swap_window = state->window;
      info[0].swap_action = (state->dbeclear_p ? XdbeBackground : XdbeUndefined);
      XdbeSwapBuffers (state->dpy, info, 1);
    }
  else
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
  if (state->dbuf)
    {
      XCopyArea (state->dpy, state->b, state->window, state->erase_gc,
		 0, 0, state->xgwa.width, state->xgwa.height, 0, 0);
    }

  if (state->shake_p && state->time_since_shake > 5)
    {
      max_d /= state->max_radius;
      if (max_d < state->shake_threshold ||  /* when its stable */
          state->time_since_shake > 30)      /* or when 30 secs has passed */
        {
          shake (state);
        }
    }

  check_wall_clock (state, max_d);
}


/* Implements the laws of physics: move balls to their new positions.
 */
static void
update_balls (b_state *state)
{
  int a, b;
  float d, vxa, vya, vxb, vyb, dd, cdx, cdy;
  float ma, mb, vca, vcb, dva, dvb;
  float dee2;

  check_window_moved (state);

  /* If we're currently tracking the mouse, update that ball first.
   */
  if (state->mouse_ball != 0)
    {
      int mouse_x, mouse_y;
      query_mouse (state, &mouse_x, &mouse_y);
      state->px[state->mouse_ball] = mouse_x;
      state->py[state->mouse_ball] = mouse_y;
      state->vx[state->mouse_ball] =
        (0.1 *
         (state->px[state->mouse_ball] - state->opx[state->mouse_ball]) *
         state->tc);
      state->vy[state->mouse_ball] =
        (0.1 *
         (state->py[state->mouse_ball] - state->opy[state->mouse_ball]) *
         state->tc);
    }

  /* For each ball, compute the influence of every other ball. */
  for (a=1; a <= state->count -  1; a++)
    for (b=a + 1; b <= state->count; b++)
      {
         d = ((state->px[a] - state->px[b]) *
              (state->px[a] - state->px[b]) +
              (state->py[a] - state->py[b]) *
              (state->py[a] - state->py[b]));
         dee2 = (state->r[a] + state->r[b]) *
                (state->r[a] + state->r[b]);
         if (d < dee2)
         {
            state->collision_count++;
            d = sqrt(d);
            dd = state->r[a] + state->r[b] - d;

            cdx = (state->px[b] - state->px[a]) / d;
            cdy = (state->py[b] - state->py[a]) / d;

            /* Move each ball apart from the other by half the
             * 'collision' distance.
             */
            state->px[a] -= 0.5 * dd * cdx;
            state->py[a] -= 0.5 * dd * cdy;
            state->px[b] += 0.5 * dd * cdx;
            state->py[b] += 0.5 * dd * cdy;

            ma = state->m[a];
            mb = state->m[b];

            vxa = state->vx[a];
            vya = state->vy[a];
            vxb = state->vx[b];
            vyb = state->vy[b];

            vca = vxa * cdx + vya * cdy; /* the component of each velocity */
            vcb = vxb * cdx + vyb * cdy; /* along the axis of the collision */

            /* elastic collison */
            dva = (vca * (ma - mb) + vcb * 2 * mb) / (ma + mb) - vca;
            dvb = (vcb * (mb - ma) + vca * 2 * ma) / (ma + mb) - vcb;

            dva *= state->e; /* some energy lost to inelasticity */
            dvb *= state->e;

#if 0
            dva += (frand (50) - 25) / ma;   /* q: why are elves so chaotic? */
            dvb += (frand (50) - 25) / mb;   /* a: brownian motion. */
#endif

            vxa += dva * cdx;
            vya += dva * cdy;
            vxb += dvb * cdx;
            vyb += dvb * cdy;

            state->vx[a] = vxa;
            state->vy[a] = vya;
            state->vx[b] = vxb;
            state->vy[b] = vyb;
         }
      }

   /* Force all balls to be on screen.
    */
  for (a=1; a <= state->count; a++)
    {
      if (state->px[a] <= (state->xmin + state->r[a]))
        {
          state->px[a] = state->xmin + state->r[a];
          state->vx[a] = -state->vx[a] * state->e;
        }
      if (state->px[a] >= (state->xmax - state->r[a]))
        {
          state->px[a] = state->xmax - state->r[a];
          state->vx[a] = -state->vx[a] * state->e;
        }
      if (state->py[a] <= (state->ymin + state->r[a]))
        {
          state->py[a] = state->ymin + state->r[a];
          state->vy[a] = -state->vy[a] * state->e;
        }
      if (state->py[a] >= (state->ymax - state->r[a]))
        {
          state->py[a] = state->ymax - state->r[a];
          state->vy[a] = -state->vy[a] * state->e;
        }
    }

  /* Apply gravity to all balls.
   */
  for (a=1; a <= state->count; a++)
    if (a != state->mouse_ball)
      {
        state->vx[a] += state->accx * state->tc;
        state->vy[a] += state->accy * state->tc;
        state->px[a] += state->vx[a] * state->tc;
        state->py[a] += state->vy[a] * state->tc;
      }
}


/* Handle X events, specifically, allow a ball to be picked up with the mouse.
 */
static Bool
fluidballs_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  b_state *state = (b_state *) closure;

  if (event->xany.type == ButtonPress)
    {
      int i, rx, ry;
      XTranslateCoordinates (dpy, window, RootWindow (dpy, DefaultScreen(dpy)),
                             event->xbutton.x, event->xbutton.y, &rx, &ry,
                             &window);

      if (state->mouse_ball != 0)  /* second down-click?  drop the ball. */
        {
          state->mouse_ball = 0;
          return True;
        }
      else
        {
          /* When trying to pick up a ball, first look for a click directly
             inside the ball; but if we don't find it, expand the radius
             outward until we find something nearby.
           */
          float max = state->max_radius * 4;
          float step = max / 10;
          float r2;
          for (r2 = step; r2 < max; r2 += step) {
            for (i = 1; i <= state->count; i++)
              {
                float d = ((state->px[i] - rx) * (state->px[i] - rx) +
                           (state->py[i] - ry) * (state->py[i] - ry));
                float r = state->r[i];
                if (r2 > r) r = r2;
                if (d < r*r)
                  {
                    state->mouse_ball = i;
                    return True;
                  }
              }
          }
        }
      return True;
    }
  else if (event->xany.type == ButtonRelease)   /* drop the ball */
    {
      state->mouse_ball = 0;
      return True;
    }

  return False;
}

static unsigned long
fluidballs_draw (Display *dpy, Window window, void *closure)
{
  b_state *state = (b_state *) closure;
  repaint_balls(state);
  update_balls(state);
  return state->delay;
}

static void
fluidballs_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static void
fluidballs_free (Display *dpy, Window window, void *closure)
{
  b_state *state = (b_state *) closure;
  XFreeGC (dpy, state->draw_gc);
  XFreeGC (dpy, state->draw_gc2);
  XFreeGC (dpy, state->erase_gc);
  XftFontClose (state->dpy, state->font);
  XftDrawDestroy (state->xftdraw);
  free (state->m);
  free (state->r);
  free (state->vx);
  free (state->vy);
  free (state->px);
  free (state->py);
  free (state->opx);
  free (state->opy);
  free (state);
}


static const char *fluidballs_defaults [] = {
  ".background:		black",
  ".foreground:		yellow",
  ".textColor:		yellow",
  "*mouseForeground:	white",
  "*delay:		10000",
  "*count:		300",
  "*size:		25",
  "*random:		True",
  "*gravity:		0.01",
  "*wind:		0.00",
  "*elasticity:		0.97",
  "*timeScale:		1.0",
  "*shake:		True",
  "*shakeThreshold:	0.015",
  "*doubleBuffer:	True",
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  "*useDBE:		True",
  "*useDBEClear:	True",
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
#ifdef HAVE_MOBILE
  "*ignoreRotation:	True",
#endif
  0
};

static XrmOptionDescRec fluidballs_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-size",		".size",	XrmoptionSepArg, 0 },
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-gravity",		".gravity",	XrmoptionSepArg, 0 },
  { "-wind",		".wind",	XrmoptionSepArg, 0 },
  { "-elasticity",	".elasticity",	XrmoptionSepArg, 0 },
  { "-shake",		".shake",	XrmoptionNoArg, "True" },
  { "-no-shake",	".shake",	XrmoptionNoArg, "False" },
  { "-random",		".random",	XrmoptionNoArg, "True" },
  { "-no-random",	".random",	XrmoptionNoArg, "False" },
  { "-nonrandom",	".random",	XrmoptionNoArg, "False" },
  { "-db",		".doubleBuffer", XrmoptionNoArg,  "True" },
  { "-no-db",		".doubleBuffer", XrmoptionNoArg,  "False" },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("FluidBalls", fluidballs)
