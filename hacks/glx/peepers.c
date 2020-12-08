/* peepers, Copyright (c) 2018-2019 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Created: 14 Feb 2018, jwz.
 *
 * Floating eyeballs!
 *
 * Inspired by @PaintYourDragon's Adafruit Snake Eyes Raspberry Pi Bonnet
 * https://learn.adafruit.com/animated-snake-eyes-bonnet-for-raspberry-pi/
 * which is excellent.
 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*count:        0           \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \

# define release_peepers 0

#define DEF_SPEED "1.0"
#define DEF_MODE  "random"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)
#undef RANDSIGN
#define RANDSIGN() ((random() & 1) ? 1 : -1)

#include "xlockmore.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include "ximage-loader.h"
#include <ctype.h>

#ifndef HAVE_JWXYZ
# include <X11/Xatom.h>
#endif

#include "images/gen/sclera_png.h"
#include "images/gen/iris_png.h"

#ifdef USE_GL /* whole file */

typedef struct { double a, o; } LL;	/* latitude + longitude */

typedef struct {
  int idx;
  GLfloat x, y, z;
  GLfloat dx, dy, dz;
  GLfloat ddx, ddy, ddz;
  rotator *rot;
  struct { GLfloat from, to, current, tick; } dilation;
  enum { ROTATE, SPIN, TRACK } focus;
  XYZ track;
  GLfloat tilt, roll;
  GLfloat scale;
  GLfloat color[4];
  int jaundice;
} floater;

typedef enum { RETINA, IRIS, SCLERA, LENS, TICK } component;

typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;

  Bool button_down_p;
  XYZ mouse, last_mouse, fake_mouse;
  time_t last_mouse_time;
  int mouse_dx, mouse_dy;

  GLuint retina_list, sclera_list, lens_list, iris_list;
  GLuint sclera_texture, iris_texture;
  int eye_polys;

  int nfloaters;
  floater *floaters;
  enum { BOUNCE, SCROLL_LEFT, SCROLL_RIGHT, XEYES, BEHOLDER } mode;

} peepers_configuration;

static peepers_configuration *bps = NULL;

static GLfloat speed;
const char *mode_opt;

static XrmOptionDescRec opts[] = {
  { "-speed", ".speed", XrmoptionSepArg, 0 },
  { "-mode",  ".mode",  XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&speed,    "speed", "Speed", DEF_SPEED, t_Float},
  {&mode_opt, "mode",  "Mode",  DEF_MODE,  t_String},
};

ENTRYPOINT ModeSpecOpt peepers_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Bottom edge of screen is -0.5; left and right scale by aspect. */
#define BOTTOM (-1.6)
#define LEFT   (BOTTOM * MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))

static void
reset_floater (ModeInfo *mi, floater *f)
{
  peepers_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat r = ((bp->mode == BOUNCE ? LEFT : BOTTOM) *
               (bp->nfloaters < 10 ? 0.3: 0.6));
  GLfloat x, y;

  if (bp->nfloaters <= 2)
    {
      x = frand(LEFT) * RANDSIGN() * 0.3;
      y = 0;
    }
  else
    {
      /* Position them off screen in a circle */
      GLfloat th = f->idx * (M_PI + (M_PI/6)) * 2 / bp->nfloaters;
      x = r * cos (th);
      y = r * sin (th) * 1.5;   /* Oval */
    }

  switch (bp->mode) {
  case BOUNCE:
    f->x = x;
    f->y = BOTTOM;
    f->z = y;

    /* Yes, I know I'm varying the force of gravity instead of varying the
       launch velocity.  That's intentional: empirical studies indicate
       that it's way, way funnier that way. */

    f->dy = 0.1;
    f->dx = 0;
    f->dz = 0;

    {
      GLfloat min = -0.004;
      GLfloat max = -0.0019;
      f->ddy = min + frand (max - min);
      f->ddx = 0;
      f->ddz = 0;
    }

    if (! (random() % (10 * bp->nfloaters)))
      {
        f->dx = BELLRAND(0.03) * RANDSIGN();
        f->dz = BELLRAND(0.03) * RANDSIGN();
      }
    break;

  case SCROLL_LEFT:
  case SCROLL_RIGHT:

    f->x = (bp->mode == SCROLL_LEFT ? -LEFT : LEFT);
    f->y = x;
    f->z = y;

    f->dx = (1.0 + frand(2.0)) * 0.020 * (bp->mode == SCROLL_LEFT ? -1 : 1);
    f->dy = (1.0 + frand(2.0)) * 0.002 * RANDSIGN();
    f->dz = (1.0 + frand(2.0)) * 0.002 * RANDSIGN();
    f->ddy = 0;
    f->ddz = 0;
    break;

  case XEYES: /* This happens in layout_grid() */
    break;
  case BEHOLDER: /* This happens in layout_geodesic() */
    break;

  default:
    abort();
  }

  f->focus = ((random() % 8) ? ROTATE :
              (random() % 4) ? TRACK : SPIN);
  f->track.x = 8 - frand(16);
  f->track.y = 8 - frand(16);
  f->track.z = 8 + frand(16);

  f->tilt = 45 - BELLRAND(90);
  f->roll = frand(180);
  f->dilation.to = f->dilation.from = f->dilation.current = frand(1.0);
  f->dilation.tick = 1;

  f->scale = 0.8 + BELLRAND(0.2);

  if      (bp->nfloaters == 1)  f->scale *= 0.5;
  else if (bp->nfloaters <= 3)  f->scale *= 0.4;
  else if (bp->nfloaters <= 9)  f->scale *= 0.3;
  else if (bp->nfloaters <= 15) f->scale *= 0.2;
  else if (bp->nfloaters <= 25) f->scale *= 0.15;
  else if (bp->nfloaters <= 90) f->scale *= 0.12;
  else                          f->scale *= 0.07;

  if (MI_WIDTH(mi) < MI_HEIGHT(mi))
    {
      f->scale /= MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi) * 1.2;
    }

  {
    static const struct { GLfloat pct; unsigned long c; } c[] = {
      /* All of the articles that I found with percentages in them only
         added up to around 70%, so who knows what that means. */
# if 0
      { 55,   0x985A07 },  /* brown -- supposedly real global percentage */
# else
      { 20,   0x985A07 },  /* brown -- but that's a lot of brown... */
# endif
      {  8,   0xD5AD68 },  /* hazel */
      {  8,   0x777F92 },  /* blue  */
      {  2,   0x6B7249 },  /* green */
      {  1,   0x7F7775 },  /* gray  */
      {  0.5, 0x9E8042 },  /* amber */
      {  0.1, 0xFFAA88 },  /* red   */
    };
    GLfloat p = 0, t = 0;
    GLfloat s = 1 - frand(0.3);
    int i;
    for (i = 0; i < countof(c); i++)
      p += c[i].pct;
    p = frand(p);

    for (i = 0; i < countof(c) - 1; i++)
      {
        t += c[i].pct;
        if (t > p) break;
      }

    if (c[i].c == 0xFFAA88)    f->jaundice = 2;
    else if (!(random() % 20)) f->jaundice = 1;

    f->color[0] = ((c[i].c >> 16) & 0xFF) / 255.0 * s;
    f->color[1] = ((c[i].c >>  8) & 0xFF) / 255.0 * s;
    f->color[2] = ((c[i].c >>  0) & 0xFF) / 255.0 * s;
    f->color[3] = 1;
  }
}


/* Place a grid of eyeballs on the screen, maximizing use of space.
 */
static void
layout_grid (ModeInfo *mi)
{
  peepers_configuration *bp = &bps[MI_SCREEN(mi)];

  /* Distribute the eyes into a rectangular grid that fills the window.
     There may be some empty cells.  N items in a W x H rectangle:
     N = W * H
     N = W * W * R
     N/R = W*W
     W = sqrt(N/R)
  */
  GLfloat aspect = MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi);
  int nlines = sqrt (bp->nfloaters / aspect) + 0.5;
  int *cols = (int *) calloc (nlines, sizeof(*cols));
  int i, x, y, max = 0;
  GLfloat scale, spacing;

  for (i = 0; i < bp->nfloaters; i++)
    {
      cols[i % nlines]++;
      if (cols[i % nlines] > max) max = cols[i % nlines];
    }

  /* That gave us, e.g. 7777666. Redistribute to 6767767. */ 
  for (i = 0; i < nlines / 2; i += 2)
    {
      int j = nlines-i-1;
      int swap = cols[i];
      cols[i] = cols[j];
      cols[j] = swap;
    }

  scale = 1.0 / nlines;       /* Scale for height */
  if (scale * max > aspect)   /* Shrink if overshot width */
    scale *= aspect / (scale * max);

  scale *= 0.9;		  /* Add padding */
  spacing = scale * 2.2;

  if (bp->nfloaters == 1) spacing = 0;

  i = 0;
  for (y = 0; y < nlines; y++)
    for (x = 0; x < cols[y]; x++)
      {
        floater *f = &bp->floaters[i];
        f->scale = scale;
        f->x = spacing * (x - cols[y] / 2.0) + spacing/2;
        f->y = spacing * (y - nlines  / 2.0) + spacing/2;
        f->z = 0;
        i++;
      }
  free (cols);
}


/* Computes the midpoint of a line between two polar coords.
 */
static void
midpoint2 (LL v1, LL v2, LL *vm_ret,
           XYZ *p1_ret, XYZ *p2_ret, XYZ *pm_ret)
{
  XYZ p1, p2, pm;
  LL vm;
  GLfloat hyp;

  p1.x = cos (v1.a) * cos (v1.o);
  p1.y = cos (v1.a) * sin (v1.o);
  p1.z = sin (v1.a);

  p2.x = cos (v2.a) * cos (v2.o);
  p2.y = cos (v2.a) * sin (v2.o);
  p2.z = sin (v2.a);

  pm.x = (p1.x + p2.x) / 2;
  pm.y = (p1.y + p2.y) / 2;
  pm.z = (p1.z + p2.z) / 2;

  vm.o = atan2 (pm.y, pm.x);
  hyp = sqrt (pm.x * pm.x + pm.y * pm.y);
  vm.a = atan2 (pm.z, hyp);

  *p1_ret = p1;
  *p2_ret = p2;
  *pm_ret = pm;
  *vm_ret = vm;
}


/* Computes the midpoint of a triangle specified in polar coords.
 */
static void
midpoint3 (LL v1, LL v2, LL v3, LL *vm_ret,
           XYZ *p1_ret, XYZ *p2_ret, XYZ *p3_ret, XYZ *pm_ret)
{
  XYZ p1, p2, p3, pm;
  LL vm;
  GLfloat hyp;

  p1.x = cos (v1.a) * cos (v1.o);
  p1.y = cos (v1.a) * sin (v1.o);
  p1.z = sin (v1.a);

  p2.x = cos (v2.a) * cos (v2.o);
  p2.y = cos (v2.a) * sin (v2.o);
  p2.z = sin (v2.a);

  p3.x = cos (v3.a) * cos (v3.o);
  p3.y = cos (v3.a) * sin (v3.o);
  p3.z = sin (v3.a);

  pm.x = (p1.x + p2.x + p3.x) / 3;
  pm.y = (p1.y + p2.y + p3.y) / 3;
  pm.z = (p1.z + p2.z + p3.z) / 3;

  vm.o = atan2 (pm.y, pm.x);
  hyp = sqrt (pm.x * pm.x + pm.y * pm.y);
  vm.a = atan2 (pm.z, hyp);

  *p1_ret = p1;
  *p2_ret = p2;
  *p3_ret = p3;
  *pm_ret = pm;
  *vm_ret = vm;
}


/* Place the eyeballs on a sphere (geodesic)
 */
static void
layout_geodesic_triangle (ModeInfo *mi, LL v1, LL v2, LL v3, int depth,
                          int *i)
{
  peepers_configuration *bp = &bps[MI_SCREEN(mi)];

  if (depth <= 0)
    {
      floater *f = &bp->floaters[*i];
      GLfloat s2 = 0.7;
      LL vc;
      XYZ p1, p2, p3, pc;
      if (*i >= bp->nfloaters) abort();

      midpoint3 (v1, v2, v3, &vc, &p1, &p2, &p3, &pc);

      switch (bp->nfloaters) {     /* This is lame. */
      case 20:   f->scale = 0.26;   break;
      case 80:   f->scale = 0.13;   break;
      case 320:  f->scale = 0.065;  break;
      case 1280: f->scale = 0.0325; break;
      default: abort();
      }

      f->z = s2 * cos (vc.a) * cos (vc.o);
      f->x = s2 * cos (vc.a) * sin (vc.o);
      f->y = s2 * sin (vc.a);
      (*i)++;
    }
  else
    {
      LL v12, v23, v13;
      XYZ p1, p2, p3, p12, p23, p13;

      midpoint2 (v1, v2, &v12, &p1, &p2, &p12);
      midpoint2 (v2, v3, &v23, &p2, &p3, &p23);
      midpoint2 (v1, v3, &v13, &p1, &p3, &p13);
      depth--;

      layout_geodesic_triangle (mi, v1,  v12, v13, depth, i);
      layout_geodesic_triangle (mi, v12, v2,  v23, depth, i);
      layout_geodesic_triangle (mi, v13, v23, v3,  depth, i);
      layout_geodesic_triangle (mi, v12, v23, v13, depth, i);
    }
}


/* Creates triangles of a geodesic to the given depth (frequency).
 */
static void
layout_geodesic (ModeInfo *mi)
{
  peepers_configuration *bp = &bps[MI_SCREEN(mi)];
  int depth;
  GLfloat th0 = atan (0.5);  /* lat division: 26.57 deg */
  GLfloat s = M_PI / 5;	     /* lon division: 72 deg    */
  int i;
  int ii = 0;

  switch (bp->nfloaters) {     /* This is lame. */
  case 20:   depth = 0; break;
  case 80:   depth = 1; break;
  case 320:  depth = 2; break;
  case 1280: depth = 3; break;
  default: abort();
  }

  for (i = 0; i < 10; i++)
    {
      GLfloat th1 = s * i;
      GLfloat th2 = s * (i+1);
      GLfloat th3 = s * (i+2);
      LL v1, v2, v3, vc;
      v1.a = th0;    v1.o = th1;
      v2.a = th0;    v2.o = th3;
      v3.a = -th0;   v3.o = th2;
      vc.a = M_PI/2; vc.o = th2;

      if (i & 1)			/* north */
        {
          layout_geodesic_triangle (mi, v1, v2, vc, depth, &ii);
          layout_geodesic_triangle (mi, v2, v1, v3, depth, &ii);
        }
      else				/* south */
        {
          v1.a = -v1.a;
          v2.a = -v2.a;
          v3.a = -v3.a;
          vc.a = -vc.a;
          layout_geodesic_triangle (mi, v2, v1, vc, depth, &ii);
          layout_geodesic_triangle (mi, v1, v2, v3, depth, &ii);
        }
    }

  bp->floaters[0].dx = BELLRAND(0.01) * RANDSIGN();
}


/* Advance the animation by one step.
 */
static void
tick_floater (ModeInfo *mi, floater *f)
{
  peepers_configuration *bp = &bps[MI_SCREEN(mi)];

  /* if (bp->button_down_p) return;*/

  f->dx += f->ddx * speed * 0.5;
  f->dy += f->ddy * speed * 0.5;
  f->dz += f->ddz * speed * 0.5;

  if (bp->mode != BEHOLDER)
    {
      f->x += f->dx * speed * 0.5;
      f->y += f->dy * speed * 0.5;
      f->z += f->dz * speed * 0.5;
    }

  f->dilation.tick += 0.1 * speed;
  if (f->dilation.tick > 1) f->dilation.tick = 1;
  if (f->dilation.tick < 0) f->dilation.tick = 0;

  f->dilation.current = (f->dilation.from +
                         ((f->dilation.to - f->dilation.from) *
                          f->dilation.tick));

  if (f->dilation.tick == 1 && !(random() % 20))
    {
      f->dilation.from = f->dilation.to;
      f->dilation.to = frand(1.0);
      f->dilation.tick = 0;
    }

  switch (bp->mode) {
  case BOUNCE:
    if (f->y < BOTTOM ||
        f->x < LEFT || f->x > -LEFT)
      reset_floater (mi, f);
    break;
  case SCROLL_LEFT:
    if (f->x < LEFT)
      reset_floater (mi, f);
    break;
  case SCROLL_RIGHT:
    if (f->x > -LEFT)
      reset_floater (mi, f);
    break;
  case XEYES:
    break;
  case BEHOLDER:
    {
      GLfloat x = f->x;
      GLfloat y = f->z;
      GLfloat th = atan2 (y, x);
      GLfloat r = sqrt(x*x + y*y);
      th += bp->floaters[0].dx;
      f->x = r*cos(th);
      f->z = r*sin(th);

      if (! (random() % 100))
        bp->floaters[0].dx += frand(0.0001) * RANDSIGN();
    }
    break;
  default:
    abort();
  }
}


/* Make sure none of the eyeballs overlap.
 */
static void
de_collide (ModeInfo *mi)
{
  peepers_configuration *bp = &bps[MI_SCREEN(mi)];
  int i, j;
  for (i = 0; i < bp->nfloaters; i++)
    {
      floater *f0 = &bp->floaters[i];
      for (j = i+1; j < bp->nfloaters; j++)
      {
        floater *f1 = &bp->floaters[j];
        GLfloat X = f1->x - f0->x;
        GLfloat Y = f1->y - f0->y;
        GLfloat Z = f1->z - f0->z;
        GLfloat min = (f0->scale + f1->scale);
        GLfloat d2 = X*X + Y*Y + Z*Z;
        if (d2 < min*min)
          {
            GLfloat d   = sqrt (d2);
            GLfloat dd = 0.5 * (min - d) / 2;
            GLfloat dx = X * dd;
            GLfloat dy = Y * dd;
            GLfloat dz = Z * dd;
            f0->x -= dx; f0->y -= dy; f0->z -= dz;
            f1->x += dx; f1->y += dy; f1->z += dz;
          }
      }
    }
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_peepers (ModeInfo *mi, int width, int height)
{
  peepers_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  glViewport (0, y, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT);

  if (bp->mode == XEYES)
    layout_grid (mi);
}


/* Find the mouse pointer on the screen and note its position in the scene.
 */
static void
track_mouse (ModeInfo *mi)
{
  peepers_configuration *bp = &bps[MI_SCREEN(mi)];
  Window r, c;
  int x, y, rx, ry;
  unsigned int m;
  int w = MI_WIDTH(mi);
  int h = MI_HEIGHT(mi);
  int rot = (int) current_device_rotation();
  int swap;
  GLfloat ys = 2.0;
  GLfloat xs = ys * w / h;
  time_t now = time ((time_t *) 0);

  XQueryPointer (MI_DISPLAY (mi), MI_WINDOW (mi),
                 &r, &c, &rx, &ry, &x, &y, &m);

  if (x != bp->last_mouse.x && y != bp->last_mouse.y)
    {
      bp->last_mouse_time = now;
      bp->fake_mouse.x = x;
      bp->fake_mouse.y = y;
      bp->mouse_dx = 0;
      bp->mouse_dy = 0;
      bp->last_mouse.x = x;
      bp->last_mouse.y = y;
    }
  else if (now > bp->last_mouse_time + 10)
    {
      /* Mouse isn't moving. Bored now. */
      if (! (random() % 20)) bp->mouse_dx += (random() % 2) * RANDSIGN();
      if (! (random() % 20)) bp->mouse_dy += (random() % 2) * RANDSIGN();
      bp->fake_mouse.x += bp->mouse_dx;
      bp->fake_mouse.y += bp->mouse_dy;
      x = bp->fake_mouse.x;
      y = bp->fake_mouse.y;
    }

  while (rot <= -180) rot += 360;
  while (rot >   180) rot -= 360;

  if (rot > 135 || rot < -135)		/* 180 */
    {
      x = w - x;
      y = h - y;
    }
  else if (rot > 45)			/* 90 */
    {
      swap = x; x = y; y = swap;
      swap = w; w = h; h = swap;
      xs = ys;
      ys = xs * w / h;
      x = w - x;
    }
  else if (rot < -45)			/* 270 */
    {
      swap = x; x = y; y = swap;
      swap = w; w = h; h = swap;
      xs = ys;
      ys = xs * w / h;
      y = h - y;
    }

  /* Put the mouse directly on the glass. */
  x = x - w / 2;
  y = h / 2 - y;
  bp->mouse.x = xs * x / w;
  bp->mouse.y = ys * y / h;
  bp->mouse.z = 0;

# if 0
  glPushMatrix();
  glTranslatef (bp->mouse.x, bp->mouse.y, bp->mouse.z);
  if (!MI_IS_WIREFRAME(mi)) glDisable(GL_LIGHTING);
  glColor3f(1,1,1);
  glBegin(GL_LINES);
  glVertex3f(-1,0,0); glVertex3f(1,0,0);
  glVertex3f(0,-1,0); glVertex3f(0,1,0);
  glVertex3f(0,0,-1); glVertex3f(0,0,1);
  glEnd();
  glPopMatrix();
  if (!MI_IS_WIREFRAME(mi)) glEnable(GL_LIGHTING);
# endif

  /* Move it farther into the scene: on the glass is too far away.
     But keep it farther away the farther outside the window the
     mouse is, so the eyes don''t turn 90 degrees sideways.
   */
  bp->mouse.x *= 0.8;
  bp->mouse.y *= 0.8;
  bp->mouse.z += 0.7;

  bp->mouse.z = MAX (0.7, 
                     sqrt (bp->mouse.x * bp->mouse.x +
                           bp->mouse.y * bp->mouse.y));

  if (bp->mode == BEHOLDER)
    bp->mouse.z += 0.25;


# if 0
  glPushMatrix();
  glTranslatef (bp->mouse.x, bp->mouse.y, bp->mouse.z);
  if (!MI_IS_WIREFRAME(mi)) glDisable(GL_LIGHTING);
  glColor3f(1,0,1);
  glBegin(GL_LINES);
  glVertex3f(-1,0,0); glVertex3f(1,0,0);
  glVertex3f(0,-1,0); glVertex3f(0,1,0);
  glVertex3f(0,0,-1); glVertex3f(0,0,1);
  glEnd();
  glPopMatrix();
  if (!MI_IS_WIREFRAME(mi)) glEnable(GL_LIGHTING);
# endif
}


ENTRYPOINT Bool
peepers_handle_event (ModeInfo *mi, XEvent *event)
{
  peepers_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    {
      if (bp->button_down_p)  /* Aim each eyeball at the mouse. */
        {
          int i;
          track_mouse (mi);
          for (i = 0; i < bp->nfloaters; i++)
            {
              floater *f = &bp->floaters[i];
              f->track = bp->mouse;
              f->focus = TRACK;
            }
        }

      return True;
    }

  return False;
}


/* Generate the polygons for the display lists.
   This routine generates the various styles of sphere-oid we use.
 */
static int
draw_ball (ModeInfo *mi, component which)
{
  peepers_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;

  GLfloat iris_ratio = 0.42;  /* Size of the iris. */
  /* The lens bulges out, but the iris bulges in, sorta. */
  GLfloat lens_bulge = (which == IRIS ? -0.50 : 0.32);

  GLfloat xstep = 32;   /* Facets on the sphere */
  GLfloat ystep = 32;
  XYZ *stacks, *normals;
  GLfloat x, y, z;
  int i, j;
  int xstart, xstop;

  if (bp->nfloaters > 16 || wire)
    xstep = ystep = 16;

  if (bp->nfloaters > 96 && which == LENS)
    return 0;

  switch (which) {
  case LENS:   xstart = 0; xstop = xstep; break;
  case SCLERA: xstart = 0; xstop = xstep * (1 - iris_ratio/2); break;
  case IRIS:   xstart = xstep * (1 - iris_ratio/2 * 1.2); xstop = xstep; break;
  case RETINA: xstart = xstep * (1 - iris_ratio/2 * 1.2); xstop = 0; break;
  default: abort(); break;
  }

  stacks  = (XYZ *) calloc (sizeof(*stacks), xstep + 1);
  normals = (XYZ *) calloc (sizeof(*stacks), xstep + 1);

  if (which == RETINA)
    {
      GLfloat c1[4] = { 0,    0, 0, 1 };
      GLfloat c2[4] = { 0.15, 0, 0, 1 };
      GLfloat th = M_PI * (1.0 - iris_ratio/2);
      GLfloat z1 = cos(th);
      GLfloat z2 = 0.9;
      GLfloat r1 = sin(th);
      GLfloat r2 = r1 * 0.3;

      if (!wire)
        {
          glColor4fv (c1);
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c1);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, c1);
        }
      
      /* Draw a black cone to occlude the interior of the eye. */

      glBegin (wire ? GL_LINES : GL_QUAD_STRIP);
      for (i = 0; i <= xstep; i++)
        {
          GLfloat th2 = i * M_PI * 2 / xstep;
          GLfloat x = cos(th2);
          GLfloat y = sin(th2);
          glNormal3f (0, 0, 1);
          glVertex3f (z1, r1 * x, r1 * y);
          glNormal3f (0, 0, 1);
          glVertex3f (z2, r2 * x, r2 * y);
          polys++;
        }
      glEnd();

      if (!wire)
        {
          glColor4fv (c2);
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c2);
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, c2);
        }

      /* Draw a small red circle at the base of the cone. */

      glBegin (wire ? GL_LINES : GL_TRIANGLE_FAN);
      glVertex3f (z2, 0, 0);
      glNormal3f (0, 0, 1);
      for (i = xstep; i >= 0; i--)
        {
          GLfloat th2 = i * M_PI * 2 / xstep;
          GLfloat x = cos(th2);
          GLfloat y = sin(th2);
          glVertex3f (z2, r2 * x, r2 * y);
          polys++;
        }
      glEnd();
      goto DONE;
    }

  for (i = xstart; i <= xstop; i++)
    {
      GLfloat th = i * M_PI / xstep;
      GLfloat x = cos(th);
      GLfloat y = sin(th);

      /* Bulge the lens, or dimple the iris. */
      if (th > M_PI * (1.0 - iris_ratio/2) &&
          th < M_PI * (1.0 + iris_ratio/2))
        {
          GLfloat r = (1 - th / M_PI) / iris_ratio * 2;
          r = cos (M_PI * r / 2);
          r *= lens_bulge;
          r = r * r * (lens_bulge < 0 ? -1 : 1);
          x *= 1+r;
          y *= 1+r;
        }

      stacks[i].x = x;
      stacks[i].y = y;
      stacks[i].z = 0;
    }

  /* Fill normals with the normal at the center of each face. */
  for (i = xstart; i < xstop; i++)
    {
      GLfloat dx = stacks[i+1].x - stacks[i].x;
      GLfloat dy = stacks[i+1].y - stacks[i].y;
      y = dy/dx;
      z = sqrt (1 + y*y);
      normals[i].x = -y/z;
      normals[i].y =  1/z;
      normals[i].z = 0;

      if (lens_bulge < 0 && i > xstep * (1 - iris_ratio/2) + 1)
        {
          normals[i].x *= -1;
          normals[i].y *= -1;
        }
    }

  if (!wire)
    glBegin (GL_QUADS);

  for (i = xstart; i < xstop; i++)
    {
      GLfloat x0 = stacks[i].x;
      GLfloat x1 = stacks[i+1].x;
      GLfloat r0 = stacks[i].y;
      GLfloat r1 = stacks[i+1].y;

      for (j = 0; j < ystep*2; j++)
        {
          GLfloat tha = j     * M_PI / ystep;
          GLfloat thb = (j+1) * M_PI / ystep;
          GLfloat xa = cos (tha);
          GLfloat ya = sin (tha);
          GLfloat xb = cos (thb);
          GLfloat yb = sin (thb);
          
          /* Each vertex normal is average of adjacent face normals. */

          XYZ p1, p2, p3, p4;
          XYZ n1, n2, n3, n4;
          p1.x = x0; p1.y = r0 * ya; p1.z = r0 * xa;
          p2.x = x1; p2.y = r1 * ya; p2.z = r1 * xa;
          p3.x = x1; p3.y = r1 * yb; p3.z = r1 * xb;
          p4.x = x0; p4.y = r0 * yb; p4.z = r0 * xb;

          if (i == 0)
            {
              n1.x = 1; n1.y = 0; n1.z = 0;
              n4.x = 1; n4.y = 0; n4.z = 0;
            }
          else
            {
              x = (normals[i-1].x + normals[i].x) / 2;
              y = (normals[i-1].y + normals[i].y) / 2;
              n1.x = x; n1.z = y * xa; n1.y = y * ya;
              n4.x = x; n4.z = y * xb; n4.y = y * yb;
            }

          if (i == xstep-1)
            {
              n2.x = -1; n2.y = 0; n2.z = 0;
              n3.x = -1; n3.y = 0; n3.z = 0;
            }
          else
            {
              x = (normals[i+1].x + normals[i].x) / 2;
              y = (normals[i+1].y + normals[i].y) / 2;
              n2.x = x; n2.z = y * xa; n2.y = y * ya;
              n3.x = x; n3.z = y * xb; n3.y = y * yb;
            }

#if 0
          /* Render normals as lines for debugging */
          glBegin(GL_LINES);
          glVertex3f(p1.x, p1.y, p1.z);
          glVertex3f(p1.x + n1.x * 0.3, p1.y + n1.y * 0.3, p1.z + n1.z * 0.3);
          glEnd();

          glBegin(GL_LINES);
          glVertex3f(p2.x, p2.y, p2.z);
          glVertex3f(p2.x + n2.x * 0.3, p2.y + n2.y * 0.3, p2.z + n2.z * 0.3);
          glEnd();

          glBegin(GL_LINES);
          glVertex3f(p3.x, p3.y, p3.z);
          glVertex3f(p3.x + n3.x * 0.3, p3.y + n3.y * 0.3, p3.z + n3.z * 0.3);
          glEnd();

          glBegin(GL_LINES);
          glVertex3f(p4.x, p4.y, p4.z);
          glVertex3f(p4.x + n4.x * 0.3, p4.y + n4.y * 0.3, p4.z + n4.z * 0.3);
          glEnd();
#endif

          if (wire)
            glBegin (GL_LINE_LOOP);

          glTexCoord2f ((j+1) / (GLfloat) ystep / 2,
                        (i - xstart) / (GLfloat) (xstop - xstart));

          glNormal3f (n4.x, n4.y, n4.z);
          glVertex3f (p4.x, p4.y, p4.z);

          glTexCoord2f ((j+1) / (GLfloat) ystep / 2,
                        ((i+1) - xstart) / (GLfloat) (xstop - xstart));

          glNormal3f (n3.x, n3.y, n3.z);
          glVertex3f (p3.x, p3.y, p3.z);

          glTexCoord2f (j / (GLfloat) ystep / 2,
                        ((i+1) - xstart) / (GLfloat) (xstop - xstart));

          glNormal3f (n2.x, n2.y, n2.z);
          glVertex3f (p2.x, p2.y, p2.z);

          glTexCoord2f (j / (GLfloat) ystep / 2,
                        (i - xstart) / (GLfloat) (xstop - xstart));

          glNormal3f (n1.x, n1.y, n1.z);
          glVertex3f (p1.x, p1.y, p1.z);

          polys++;

          if (wire)
            glEnd();
        }
    }

  if (!wire)
    glEnd();

 DONE:
  free (stacks);
  free (normals);

  return polys;
}


ENTRYPOINT void
init_peepers (ModeInfo *mi)
{
  peepers_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_peepers (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);

  if (!wire)
    {
      XImage *xi;
      GLfloat pos[4] = {0.4, 0.2, 0.4, 0.0};
      GLfloat amb[4] = {0.1, 0.1, 0.1, 1.0};

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);

      glEnable (GL_LIGHTING);
      glEnable (GL_LIGHT0);
      glEnable (GL_DEPTH_TEST);
      glEnable (GL_CULL_FACE);
      glEnable (GL_BLEND);

      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);

      glPixelStorei (GL_UNPACK_ALIGNMENT, 1);

      xi = image_data_to_ximage (mi->dpy, mi->xgwa.visual, 
                                 sclera_png, sizeof(sclera_png));
      glGenTextures (1, &bp->sclera_texture);
      glBindTexture (GL_TEXTURE_2D, bp->sclera_texture);

      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

      glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
                    xi->width, xi->height, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, xi->data);
      check_gl_error("texture");
      XDestroyImage (xi);

      xi = image_data_to_ximage (mi->dpy, mi->xgwa.visual, 
                                 iris_png, sizeof(iris_png));

      glGenTextures (1, &bp->iris_texture);
      glBindTexture (GL_TEXTURE_2D, bp->iris_texture);

      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

      glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
                    xi->width, xi->height, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, xi->data);
      check_gl_error("texture");
      XDestroyImage (xi);


    }

  bp->lens_list = glGenLists (1);
  glNewList (bp->lens_list, GL_COMPILE);
  bp->eye_polys += draw_ball (mi, LENS);
  glEndList ();

  bp->sclera_list = glGenLists (1);
  glNewList (bp->sclera_list, GL_COMPILE);
  bp->eye_polys += draw_ball (mi, SCLERA);
  glEndList ();

  bp->iris_list = glGenLists (1);
  glNewList (bp->iris_list, GL_COMPILE);
  bp->eye_polys += draw_ball (mi, IRIS);
  glEndList ();

  bp->retina_list = glGenLists (1);
  glNewList (bp->retina_list, GL_COMPILE);
  bp->eye_polys += draw_ball (mi, RETINA);
  glEndList ();

  bp->trackball = gltrackball_init (False);

  if (!mode_opt || !*mode_opt || !strcasecmp (mode_opt, "random"))
    bp->mode = ((random() & 1) ? BOUNCE :
                ((random() & 1) ? SCROLL_LEFT : SCROLL_RIGHT));
  else if (!strcasecmp (mode_opt, "bounce"))
    bp->mode = BOUNCE;
  else if (!strcasecmp (mode_opt, "scroll"))
    bp->mode = (random() & 1) ? SCROLL_LEFT : SCROLL_RIGHT;
  else if (!strcasecmp (mode_opt, "xeyes"))
    bp->mode = XEYES;
  else if (!strcasecmp (mode_opt, "beholder") ||
           !strcasecmp (mode_opt, "ball"))
    bp->mode = BEHOLDER;
  else
    {
      fprintf (stderr,
               "%s: mode must be bounce, scroll, random, xeyes or beholder,"
               " not \"%s\"\n", 
               progname, mode_opt);
      exit (1);
    }

  bp->nfloaters = MI_COUNT (mi);

  if (bp->nfloaters <= 0)
    {
      if (bp->mode == XEYES)
        bp->nfloaters = 2 + (random() % 30);
      else if (bp->mode == BEHOLDER)
        bp->nfloaters = 20 * pow (4, (random() % 4));
      else
        bp->nfloaters = 2 + (random() % 6);
    }

  if (bp->mode == BEHOLDER)
    {
      if      (bp->nfloaters <= 20)  bp->nfloaters = 20;  /* This is lame */
      else if (bp->nfloaters <= 80)  bp->nfloaters = 80;
      else if (bp->nfloaters <= 320) bp->nfloaters = 320;
      else bp->nfloaters = 1280;
    }

  bp->floaters = (floater *) calloc (bp->nfloaters, sizeof (floater));

  for (i = 0; i < bp->nfloaters; i++)
    {
      floater *f = &bp->floaters[i];
      f->idx = i;
      f->rot = make_rotator (10.0, 0, 0,
                             4, 0.05 * speed,
                             True);
      if (bp->nfloaters == 2)
        {
          f->x = 10 * (i ? 1 : -1);
        }
      else if (i != 0)
        {
          double th = (i - 1) * M_PI*2 / (bp->nfloaters-1);
          double r = LEFT * 0.3;
          f->x = r * cos(th);
          f->z = r * sin(th);
        }

      if (bp->mode == SCROLL_LEFT || bp->mode == SCROLL_RIGHT)
        {
          f->y = f->x;
          f->x = 0;
        }

      reset_floater (mi, f);
    }

  if (bp->mode == XEYES)
    layout_grid (mi);
  else if (bp->mode == BEHOLDER)
    layout_geodesic (mi);

# ifndef HAVE_JWXYZ /* Real X11 */
#  if 0 /* I wonder if this works? */
  if (bp->mode == XEYES && MI_WIN_IS_INWINDOW (mi))
    {
      uint32_t ca = 0;
      glClearColor (0, 0, 0, 0);
      XChangeProperty (MI_DISPLAY(mi), MI_WINDOW(mi),
                       XInternAtom (MI_DISPLAY(mi),
                                    "_NET_WM_WINDOW_OPACITY", 0),
                       XA_CARDINAL, 32, PropModeReplace,
                       (uint8_t *) &ca, 1);
    }
#  endif
# endif
}


static void
draw_floater (ModeInfo *mi, floater *f, component which)
{
  peepers_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  double x, y, z;

  GLfloat spc[4] = { 1.0, 1.0, 1.0, 1.0 };
  GLfloat c2[4]  = { 1.0, 1.0, 1.0, 1.0 };
  GLfloat c2b[4] = { 1.0, 0.6, 0.6, 1.0 };
  GLfloat c2c[4] = { 1.0, 1.0, 0.65, 1.0 };
  GLfloat c3[4]  = { 0.6, 0.6, 0.6, 0.25 };

  get_position (f->rot, &x, &y, &z, 
                which == LENS && !bp->button_down_p);

  if (bp->nfloaters == 2 && 
      f != &bp->floaters[0] &&
      (bp->mode == BOUNCE || bp->mode == XEYES))
    {
      /* When there are exactly two eyes, track them together. */
      floater *f0 = &bp->floaters[0];
      double x0, y0, z0;
      get_position (f0->rot, &x0, &y0, &z0, 0);
      x = x0;
      y = 1-y0;  /* This is rotation: what the eye is looking at */
      z = z0;
      if (bp->mode != XEYES)
        {
          f->x = f0->x + f0->scale * 3;
          f->y = f0->y;
          f->z = f0->z;
        }
      f->dilation = f0->dilation;
      f->focus = f0->focus;
      f->track = f0->track;
      f->tilt = f0->tilt;
      f->scale = f0->scale;
      f->jaundice = f0->jaundice;
      if (f->focus == ROTATE)
        f->focus = f0->focus = TRACK;
      memcpy (f->color, f0->color, sizeof(f0->color));
    }

  glPushMatrix();
  glTranslatef (f->x, f->y, f->z);

  /* gltrackball_rotate (bp->trackball); */

  switch (f->focus) {
  case ROTATE:
    glRotatef (y * 180, 0, 1, 0);
    glRotatef (f->tilt, 0, 0, 1);
    break;
  case SPIN:
    glRotatef (y * 360 + 90, 0, 1, 0);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
    break;
  case TRACK:
    {
      GLfloat X, Y, Z;
      X = f->track.x - f->x;
      Y = f->track.z - f->z;
      Z = f->track.y - f->y;
      if (X != 0 || Y != 0)
        {
          GLfloat facing = atan2 (X, Y) * (180 / M_PI);
          GLfloat pitch  = atan2 (Z, sqrt(X*X + Y*Y)) * (180 / M_PI);
          glRotatef (90,     0, 1, 0);
          glRotatef (facing, 0, 1, 0);
          glRotatef (-pitch, 0, 0, 1);
        }
    }

    break;
  default:
    abort();
  }

  glRotatef (f->roll, 1, 0, 0);
  glScalef (f->scale, f->scale, f->scale);

  if (! wire)
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  switch (which) {
    case RETINA:
      if (!wire)
        {
          glScalef (0.96, 0.96, 0.96);
          glCallList (bp->retina_list);
        }
      break;

    case IRIS:
      glColor4fv (f->color);
      if (! wire)
        {
          glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  spc);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, 10);

          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, f->color);
          glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, 20);

          glEnable (GL_TEXTURE_2D);
          glBindTexture (GL_TEXTURE_2D, bp->iris_texture);
          glMatrixMode (GL_TEXTURE);
          glLoadIdentity();
          glScalef (1, 1.25 + f->dilation.current * 0.3, 1);
          glMatrixMode (GL_MODELVIEW);
        }
      glScalef (0.96, 0.96, 0.96);
      glCallList (bp->iris_list);

      if (! wire)
        {
          glMatrixMode (GL_TEXTURE);
          glLoadIdentity();
          glMatrixMode (GL_MODELVIEW);
        }
      break;

    case SCLERA:
      if (! wire)
        {
          GLfloat *c = (f->jaundice == 2 ? c2b : f->jaundice == 1 ? c2c : c2);
          glColor4fv (c);
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c);
          glBindTexture (GL_TEXTURE_2D, bp->sclera_texture);

          glScalef (0.98, 0.98, 0.98);
          glCallList (bp->sclera_list);
        }
      break;

    case LENS:
      glColor4fv (c3);
      if (! wire)
        {
          glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, c3);
          glDisable (GL_TEXTURE_2D);
        }
      glCallList (bp->lens_list);
      break;

  default:
    abort();
    break;
  }

  glPopMatrix();
}


ENTRYPOINT void
draw_peepers (ModeInfo *mi)
{
  peepers_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glRotatef (current_device_rotation(), 0, 0, 1);


  /* Scale so that screen is 1 high and w/h wide. */
  glScalef (8, 8, 8);

  mi->polygon_count = 0;

  if (bp->mode == XEYES || bp->mode == BEHOLDER)
    {
      int i;
      track_mouse (mi);
      for (i = 0; i < bp->nfloaters; i++)
        {
          floater *f = &bp->floaters[i];
          f->track = bp->mouse;
          f->focus = TRACK;
        }
    }

# if 0
  {
    /* Draw just one */
    component j;
    floater F;
    reset_floater(mi, &F);
    F.x = F.y = F.z = 0;
    F.dx = F.dy = F.dz = 0;
    F.ddx = F.ddy = F.ddz = 0;
    F.scale = 1;
    F.focus = TRACK;
    F.dilation.current = 0;
    F.track.x = F.track.y = F.track.z = 0;
    F.rot = make_rotator (0, 0, 0, 1, 0, False);
    glRotatef(180,0,1,0);
    glRotatef(15,1,0,0);
    for (j = RETINA; j <= LENS; j++)
      draw_floater (mi, &F, j);
    mi->polygon_count += bp->eye_polys;
  }
# else
  {
    component j;
    int i;
    for (j = RETINA; j <= TICK; j++)
      for (i = 0; i < bp->nfloaters; i++)
        {
          floater *f = &bp->floaters[i];
          if (j == TICK)
            tick_floater (mi, f);
          else
            draw_floater (mi, f, j);
        }

    if (bp->mode != BEHOLDER)
      de_collide (mi);

    mi->polygon_count += bp->eye_polys * bp->nfloaters;
  }
# endif

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_peepers (ModeInfo *mi)
{
  peepers_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  for (i = 0; i < bp->nfloaters; i++)
    free_rotator (bp->floaters[i].rot);
  if (bp->floaters) free (bp->floaters);
  if (glIsList(bp->lens_list)) glDeleteLists(bp->lens_list, 1);
  if (glIsList(bp->sclera_list)) glDeleteLists(bp->sclera_list, 1);
  if (glIsList(bp->iris_list)) glDeleteLists(bp->iris_list, 1);
  if (glIsList(bp->retina_list)) glDeleteLists(bp->retina_list, 1);
  if (bp->sclera_texture) glDeleteTextures (1, &bp->sclera_texture);
  if (bp->iris_texture) glDeleteTextures (1, &bp->iris_texture);
}

XSCREENSAVER_MODULE ("Peepers", peepers)

#endif /* USE_GL */
