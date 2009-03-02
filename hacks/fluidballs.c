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
 */

/* cjb notes
 *
 * Future ideas:
 * Specifying a distribution in the ball sizes (with a gamma curve, possibly).
 * Brownian motion, for that extra touch of realism.
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

  Pixmap b, ba;	/* double-buffer to reduce flicker */
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  XdbeBackBuffer backb;
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
  float e;		/* coefficient of friction, I think? */
  float max_radius;	/* largest radius of any ball */

  Bool random_sizes_p;  /* Whether balls should be various sizes up to max. */
  Bool shake_p;		/* Whether to mess with gravity when things settle. */
  Bool dbuf;            /* Whether we're using double buffering. */
  Bool dbeclear_p;      /* ? */
  float shake_threshold;
  int time_since_shake;

  Bool fps_p;		/* Whether to draw some text at the bottom. */
  GC font_gc;
  int font_height;
  int font_baseline;
  int frame_count;
  int collision_count;
  char fps_str[1024];
  
} b_state;


/* Draws the frames per second string */
static void 
draw_fps_string (b_state *state)
{  
  XFillRectangle (state->dpy, state->b, state->erase_gc,
		  0, state->xgwa.height - state->font_height,
		  state->xgwa.width, state->font_height);
  XDrawImageString (state->dpy, state->b, state->font_gc,
		    0, state->xgwa.height - state->font_baseline,
		    state->fps_str, strlen(state->fps_str));
}

/* Finds the origin of the window relative to the root window, by
   walking up the window tree until it reaches the top.
 */
static void
window_origin (Display *dpy, Window window, int *x, int *y)
{
  Window root, parent, *kids;
  unsigned int nkids;
  XWindowAttributes xgwa;
  int wx, wy;
  XGetWindowAttributes (dpy, window, &xgwa);

  wx = xgwa.x;
  wy = xgwa.y;

  kids = 0;
  *x = 0;
  *y = 0;

  if (XQueryTree (dpy, window, &root, &parent, &kids, &nkids))
    {
      if (parent && parent != root)
        {
          int px, py;
          window_origin (dpy, parent, &px, &py);
          wx += px;
          wy += py;
        }
    }
  if (kids) XFree (kids);
  *x = wx;
  *y = wy;
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
  state->ymax = state->ymin + state->xgwa.height - state->font_height;

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
			0, state->xgwa.height - state->font_height,
			state->xgwa.width, state->font_height);
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
static b_state *
init_balls (Display *dpy, Window window)
{
  int i;
  float extx, exty;
  b_state *state = (b_state *) calloc (1, sizeof(*state));
  XGCValues gcv;

  state->dpy = dpy;

  state->window = window;

  check_window_moved (state);

  state->dbuf = get_boolean_resource ("doubleBuffer", "Boolean");
  state->dbeclear_p = get_boolean_resource ("useDBEClear", "Boolean");

  if (state->dbuf)
    {
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
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
  if (! (state->xgwa.all_event_masks & ButtonReleaseMask))
    XSelectInput (state->dpy, state->window,
                  state->xgwa.your_event_mask | ButtonReleaseMask);

  gcv.foreground = get_pixel_resource("foreground", "Foreground",
                                      state->dpy, state->xgwa.colormap);
  gcv.background = get_pixel_resource("background", "Background",
                                      state->dpy, state->xgwa.colormap);
  state->draw_gc = XCreateGC (state->dpy, state->b,
                              GCForeground|GCBackground, &gcv);

  gcv.foreground = get_pixel_resource("mouseForeground", "MouseForeground",
                                      state->dpy, state->xgwa.colormap);
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

  state->count = get_integer_resource ("count", "Count");
  if (state->count < 1) state->count = 20;

  state->max_radius = get_float_resource ("size", "Size") / 2;
  if (state->max_radius < 1.0) state->max_radius = 1.0;

  state->random_sizes_p = get_boolean_resource ("random", "Random");

  state->accx = get_float_resource ("wind", "Wind");
  if (state->accx < -1.0 || state->accx > 1.0) state->accx = 0;

  state->accy = get_float_resource ("gravity", "Gravity");
  if (state->accy < -1.0 || state->accy > 1.0) state->accy = 0.01;

  state->e = get_float_resource ("friction", "Friction");
  if (state->e < 0.2 || state->e > 1.0) state->e = 0.97;

  state->tc = get_float_resource ("timeScale", "TimeScale");
  if (state->tc <= 0 || state->tc > 10) state->tc = 1.0;

  state->shake_p = get_boolean_resource ("shake", "Shake");
  state->shake_threshold = get_float_resource ("shakeThreshold",
                                               "ShakeThreshold");

  state->fps_p = get_boolean_resource ("doFPS", "DoFPS");
  if (state->fps_p)
    {
      XFontStruct *font;
      char *fontname = get_string_resource ("font", "Font");
      const char *def_font = "fixed";
      if (!fontname || !*fontname) fontname = (char *)def_font;
      font = XLoadQueryFont (dpy, fontname);
      if (!font) font = XLoadQueryFont (dpy, def_font);
      if (!font) exit(-1);
      gcv.font = font->fid;
      gcv.foreground = get_pixel_resource("textColor", "Foreground",
                                          state->dpy, state->xgwa.colormap);
      state->font_gc = XCreateGC(dpy, state->b,
                                 GCFont|GCForeground|GCBackground, &gcv);
      state->font_height = font->ascent + font->descent;
      state->font_baseline = font->descent;
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
  static int tick = 0;
  state->frame_count++;
  
  if (tick++ > 20)  /* don't call gettimeofday() too often -- it's slow. */
    {
      struct timeval now;
      static struct timeval last = {0, };
# ifdef GETTIMEOFDAY_TWO_ARGS
      struct timezone tzp;
      gettimeofday(&now, &tzp);
# else
      gettimeofday(&now);
# endif

      if (last.tv_sec == 0)
        last = now;

      tick = 0;
      if (now.tv_sec == last.tv_sec)
        return;

      state->time_since_shake += (now.tv_sec - last.tv_sec);

      if (state->fps_p) 
	{
	  float elapsed = ((now.tv_sec  + (now.tv_usec  / 1000000.0)) -
			   (last.tv_sec + (last.tv_usec / 1000000.0)));
	  float fps = state->frame_count / elapsed;
	  float cps = state->collision_count / elapsed;
	  
	  sprintf (state->fps_str, 
		   " FPS: %.2f  Collisions: %.3f/frame  Max motion: %.3f",
		   fps, cps/fps, max_d);
	  
	  draw_fps_string(state);
	}

      state->frame_count = 0;
      state->collision_count = 0;
      last = now;
    }
}

/* Erases the balls at their previous positions, and draws the new ones.
 */
static void
repaint_balls (b_state *state)
{
  int a;
  int x1a, x2a, y1a, y2a;
  int x1b, x2b, y1b, y2b;
  float max_d = 0;

  for (a=1; a <= state->count; a++)
    {
      GC gc;
      x1a = (state->opx[a] - state->r[a] - state->xmin);
      y1a = (state->opy[a] - state->r[a] - state->ymin);
      x2a = (state->opx[a] + state->r[a] - state->xmin);
      y2a = (state->opy[a] + state->r[a] - state->ymin);

      x1b = (state->px[a] - state->r[a] - state->xmin);
      y1b = (state->py[a] - state->r[a] - state->ymin);
      x2b = (state->px[a] + state->r[a] - state->xmin);
      y2b = (state->py[a] + state->r[a] - state->ymin);

      if (!state->dbeclear_p ||
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
          !state->backb
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
	  )
	{
/*	  if (x1a != x1b || y1a != y1b)   -- leaves turds if we optimize this */
	    {
	      gc = state->erase_gc;
	      XFillArc (state->dpy, state->b, gc,
			x1a, y1a, x2a-x1a, y2a-y1a,
			0, 360*64);
	    }
	}
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

  if (state->fps_p && state->dbeclear_p) 
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
  float ma, mb, vela, velb, vela1, velb1;
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
  for (a=1; a <= state->count; a++)
    if (a != state->mouse_ball)
      for (b=1; b <= state->count; b++)
        if (a != b)
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
		/* A pair of balls that have already collided in this
		 * current frame (and therefore touching each other)
		 * should not have another collision calculated, hence
		 * the fallthru if "dd ~= 0.0".
		 */
		if ((dd < -0.01) || (dd > 0.01))
		  {
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

		    vela = sqrt((vxa * vxa) + (vya * vya));
		    velb = sqrt((vxb * vxb) + (vyb * vyb));

		    vela1 = vela * ((ma - mb) / (ma + mb)) +
		            velb * ((2 * mb) / (ma + mb));
		    velb1 = vela * ((2 * ma) / (ma + mb)) +
		            velb * ((mb - ma) / (ma + mb));

		    vela1 *= state->e; /* "air resistance" */
		    velb1 *= state->e;
#if 0
		    vela1 += (frand (50) - 25) / ma; /* brownian motion */
		    velb1 += (frand (50) - 25) / mb;
#endif
		    state->vx[a] = -cdx * vela1;
		    state->vy[a] = -cdy * vela1;
		    state->vx[b] = cdx * velb1;
		    state->vy[b] = cdy * velb1;
                  }
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
static void
handle_events (b_state *state)
{
  XSync (state->dpy, False);
  while (XPending (state->dpy))
    {
      XEvent event;
      XNextEvent (state->dpy, &event);
      if (event.xany.type == ButtonPress)
        {
          int i;
          float fmx = event.xbutton.x_root;
          float fmy = event.xbutton.y_root;
          if (state->mouse_ball != 0)  /* second down-click?  drop the ball. */
            {
              state->mouse_ball = 0;
              return;
            }
          else
            for (i=1; i <= state->count; i++)
              {
                float d = ((state->px[i] - fmx) * (state->px[i] - fmx) +
                           (state->py[i] - fmy) * (state->py[i] - fmy));
                float r = state->r[i];
                if (d < r*r)
                  {
                    state->mouse_ball = i;
                    return;
                  }
              }
        }
      else if (event.xany.type == ButtonRelease)   /* drop the ball */
        {
          state->mouse_ball = 0;
          return;
        }
      else if (event.xany.type == ConfigureNotify)
        {
          /* This is redundant, since we poll this every iteration. */
          check_window_moved (state);
        }

      screenhack_handle_event (state->dpy, &event);
    }
}


char *progclass = "FluidBalls";

char *defaults [] = {
  ".background:		black",
  ".textColor:		yellow",
  ".font:		-*-helvetica-*-r-*-*-*-180-*-*-p-*-*-*",
  "*delay:		10000",
  "*count:		300",
  "*size:		25",
  "*random:		True",
  "*gravity:		0.01",
  "*wind:		0.00",
  "*friction:		0.8",
  "*timeScale:		1.0",
  "*doFPS:		False",
  "*shake:		True",
  "*shakeThreshold:	0.015",
  "*doubleBuffer:	True",
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  "*useDBE:		True",
  "*useDBEClear:	True",
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-size",		".size",	XrmoptionSepArg, 0 },
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-gravity",		".gravity",	XrmoptionSepArg, 0 },
  { "-wind",		".wind",	XrmoptionSepArg, 0 },
  { "-friction",	".friction",	XrmoptionSepArg, 0 },
  { "-fps",		".doFPS",	XrmoptionNoArg, "True" },
  { "-no-fps",		".doFPS",	XrmoptionNoArg, "False" },
  { "-shake",		".shake",	XrmoptionNoArg, "True" },
  { "-no-shake",	".shake",	XrmoptionNoArg, "False" },
  { "-random",		".random",	XrmoptionNoArg, "True" },
  { "-nonrandom",	".random",	XrmoptionNoArg, "False" },
  { "-db",		".doubleBuffer", XrmoptionNoArg,  "True" },
  { "-no-db",		".doubleBuffer", XrmoptionNoArg,  "False" },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  b_state *state = init_balls(dpy, window);
  int delay = get_integer_resource ("delay", "Integer");

  while (1)
    {
      repaint_balls(state);
      update_balls(state);

      XSync (dpy, False);
      handle_events (state);
      if (delay) usleep (delay);
    }
}
