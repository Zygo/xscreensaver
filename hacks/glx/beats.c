/* beats, Copyright (c) 2020 David Eccles (gringer) <hacking@gringene.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation.  No representations are made
 * about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

/* Beats changes the position of objects in time with a
 * synchronisation signal (or more correctly, based on the time
 * elapsed since the last synchronisation point). By default, the
 * system clock is used for this signal, with synchronisation
 * happening every minute. The location of objects is entirely
 * dependant on this synchronisation signal; there is no multi-object
 * state that needs to be stored, although there may be some styling
 * state required.
 */

#define DEFAULTS	"*count:	30          \n" \
			"*delay:	30000       \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \

# define release_beats 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "sphere.h"
#include "hsv.h"
#include <ctype.h>
#include <sys/time.h>

#ifdef USE_GL /* whole file */

#define DEF_CYCLE       "-1"
#define DEF_TICK        "True"
#define DEF_BLUR        "True"

#define SPHERE_SLICES 16  /* how densely to render spheres */
#define SPHERE_STACKS 16


typedef struct {
  GLXContext *glx_context;
  Bool button_down_p;

  GLuint beats_list;

  GLfloat pos;

  int ball_count; /* Number of balls */
  int preset_cycle; /* Cycle to show (-1 for random) */
  Bool use_tick; /* Add tick for clockwise / galaxy */
  Bool use_blur; /* Motion blur */
  int ncolors;
  XColor *colors;
  int ccolor;
  int color_shift;

} beats_configuration;

static beats_configuration *bps = NULL;

static int cycle_arg;
static Bool tick_arg;
static Bool blur_arg;

static XrmOptionDescRec opts[] = {
  { "-cycle",   ".cycle",   XrmoptionSepArg, 0 },
  { "-count",   ".count",   XrmoptionSepArg, 0 },
  { "-tick",    ".tick",     XrmoptionNoArg, "on" },
  { "+tick",    ".tick",     XrmoptionNoArg, "off" },
  { "-blur",    ".blur",     XrmoptionNoArg, "on" },
  { "+blur",    ".blur",     XrmoptionNoArg, "off" }
};

static argtype vars[] = {
  {&cycle_arg,   "cycle",   "Cycle",   DEF_CYCLE,   t_Int},
  {&tick_arg,   "tick",     "Tick",   DEF_TICK,   t_Bool},
  {&blur_arg,   "blur",     "Blur",   DEF_BLUR,   t_Bool}
};

static OptionStruct desc[] = {
  {"-count num", "number of balls"},
  {"-cycle num", "cycle / pattern type"},
  {"-/+tick", "enable/disable tick for clockwise and galaxy"},
  {"-/+blur", "enable/disable motion blur"}
};

ENTRYPOINT ModeSpecOpt beats_opts =
  {countof(opts), opts, countof(vars), vars, desc};

/* Window management, etc
 */
ENTRYPOINT void
reshape_beats (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width * 9/16;
    y = -height/2;
    h = height / (GLfloat) width;
  }

  glViewport (0, y, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (1/h, 1/h, 1/h);
  }
# endif

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
beats_handle_event (ModeInfo *mi, XEvent *event)
{
  return True;
}

static Bool getFracColour (GLfloat* retVal, float posFrac, float s){
  /* top: red, right: yellow, bottom: [dark] green, left: blue */
  /* note: fixed point; align to 0.1 degree increments */
  int theta, h, v;
  unsigned short r,g,b;
  theta = ((int)(posFrac * 3600) % 3600 + 3600) % 3600;
  v = 100;
  if        ((theta >=   0) && (theta <  900)) {
    h = (theta * 600) / 900;
  } else if ((theta >=  900) && (theta < 1800)) {
    h = ((theta - 900) * 600) / 900 + 600;
    v = 100 - ((theta - 900) / 18);
  } else if ((theta >= 1800) && (theta < 2700)) {
    h = ((theta - 1800) * 1200) / 900 + 1200;
    v = ((theta - 1800) / 18) + 50;
  } else /* if ((theta >= 2700) && (theta < 3600))*/ {
    h = ((theta - 2700) * 1200) / 900 + 2400;
  }
  hsv_to_rgb((int)h / 10.0, s, v / 100.0, &r, &g, &b);
  retVal[0] = r / 65535.0;
  retVal[1] = g / 65535.0;
  retVal[2] = b / 65535.0;
  return True;
}


ENTRYPOINT void 
init_beats (ModeInfo *mi)
{
  beats_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_beats (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  if (!wire)
    {
      GLfloat pos[4] = {1.0, 1.0, 1.0, 0.0};
      GLfloat amb[4] = {0.02, 0.02, 0.02, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {0.2, 0.2, 0.2, 0.2};

      glEnable(GL_LIGHTING);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);

      glEnable(GL_LIGHT0);
    }

  if (cycle_arg > 3) cycle_arg = -1;

  bp->ball_count = MI_COUNT(mi);
  if (bp->ball_count < 2) bp->ball_count = 2;

  bp->preset_cycle = cycle_arg;
  bp->use_tick = tick_arg;
  bp->use_blur = blur_arg;
  
# ifdef HAVE_ANDROID
  bp->use_blur = False; /* Works on iOS but not Android */
# endif

  bp->ncolors = 128;
  bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);

  bp->beats_list = glGenLists(1);

  glNewList (bp->beats_list, GL_COMPILE);
  glScalef(0.71, 0.71, 0.71);
  unit_sphere (SPHERE_STACKS, SPHERE_SLICES, wire);
  glEndList ();

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


ENTRYPOINT void
draw_beats (ModeInfo *mi)
{
  beats_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  unsigned num_objects = bp->ball_count, oi;
  struct timeval tv, tvOrig;
  struct tm *now;
  Bool sineWaveTick = bp->use_tick;
  Bool motionBlur = bp->use_blur;
  size_t cycle, dist;
  int tmS, tmM, tmH, tmD;
  uint64_t timeSeed;
  int64_t timeDelta = 0;
  size_t blurOffset = 10; /* offset per blur frame, in milliseconds */
  size_t framesPerBlur = 20;  /* number of sub-frames to blur */
  size_t deltaLimit = (motionBlur) ? (blurOffset * framesPerBlur) : 1;
  float ballAlpha;
  float secFrac, minFrac, minProp, hourProp, halfDayProp,
    z, op, mp, m2m,
    theta, delta, blurFrac, oFP, pathLength;

  static const GLfloat bspec[4]  = {1.0, 1.0, 1.0, 1.0};
  static const GLfloat bshiny    = 92.0;

  GLfloat bcolor[4] = {0.85, 0.75, 0.75, 1.0};

  if (!bp->glx_context)
    return;
  gettimeofday (&tvOrig, NULL);
  
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  
  glShadeModel(GL_SMOOTH);
  
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_ALPHA_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glPushMatrix ();
  glRotatef(current_device_rotation(), 0, 0, 1);

# ifdef HAVE_MOBILE
  {
    GLfloat s = (MI_WIDTH(mi) > MI_HEIGHT(mi) ? 0.5 : 0.3);
    glScalef (s, s, s);
  }
# endif

  /* timeDelta is in milliseconds */
  for(timeDelta = 0; timeDelta <= deltaLimit; timeDelta += blurOffset){
    if(timeDelta < blurOffset){
      /* glEnable(GL_DEPTH_TEST); */
      ballAlpha = 1.0;
    } else {
      /* glDisable(GL_DEPTH_TEST); */
      ballAlpha = 1.0 / framesPerBlur;
    }
    blurFrac = sin((1 - (float) timeDelta / deltaLimit) * M_PI_2) * ballAlpha;
    tv = tvOrig;
    now = localtime (&tv.tv_sec); /* This seems to be needed for seconds */
    tmS = now->tm_sec;
    tmM = now->tm_min;
    tmH = now->tm_hour;
    tmD = now->tm_yday;
    secFrac = ((tv.tv_usec % 1000000) - (timeDelta * 1000)) / (1e6);
    if(secFrac < 0){
      secFrac += 1;
      tmS--;
      if(tmS < 0){
	tmS += 60;
	tmM--;
      }
      if(tmM < 0){
	tmM += 60;
	tmH--;
      }
      if(tmH < 0){
	tmH += 24;
	tmD--;
      }
      if(tmD < 0){
	/* note: this won't be accurate for leap years, but the rare
	   event logic is complex enough */
	tmD += 365;
      }
    }
    /* pseudo-random generator based on current minute */
    timeSeed = (((tmM+1) * (tmM+1) * ((tmH+1) * 37) *
		 ((tmD+1) * 1151) * 1233599) % 653);
    cycle = timeSeed % 4;
    if(bp->preset_cycle != -1){
      cycle = bp->preset_cycle;
    }
    if(sineWaveTick && (cycle == 0 || cycle == 3)){
      Bool doTick = (timeSeed % 2 == 0);
      if(doTick){ /* choose to tick randomly */
	/* sine-wave 'tick' motion, converts linear 0..1 to 
	   pause/fast/pause 0..1 */
	secFrac = (1.0 - sin((0.5-secFrac) * M_PI))/2.0;
      }
    }
    minFrac = tmS / 60.0;
    /* now we have enough information to calculate our goal statistic,
       minProp: the position in the synchronisation cycle of one
       minute */
    minProp = (minFrac - trunc(minFrac)) + (secFrac / 60);
    m2m = minProp * 2 * M_PI;

    /* change colour based on the minute and hour */
    hourProp = tmM / 60.0 + minProp / 60.0;
    hourProp = hourProp - trunc(hourProp);

    halfDayProp = tmH / 12.0 + hourProp / 12.0;
    halfDayProp = halfDayProp - trunc(halfDayProp);

    mi->polygon_count = 0;

    for(oi = 0; oi < num_objects; oi++){
      glPushMatrix ();
      glScalef(1.1, 1.1, 1.1);

      /* Object Fraction Position - 0..1 depending on native Z order */
      oFP = oi * 1.0 / (num_objects - 1);

      /* set Z distance between [-3.5 .. 0.5] (common to all cycles) */
      z = (oFP) * 4.0 - 3.5;

      /* set colour (common to all cycles) */
      if(oFP < (1 / 3.0)){ /* "second" objects */
	getFracColour(bcolor, minProp, 1.0);
      } else if(oFP < (2 / 3.0)) { /* "minute" objects */
	getFracColour(bcolor, hourProp, 1.0);
      } else { /* "hour" objects */
	getFracColour(bcolor, halfDayProp, 1.0);
      }
    
      /* set x/y location */
      if(cycle == 0){
	/* clockwise */
	glRotatef(-minProp * 360 * (oi + 1), 0, 0, 1);
	glTranslatef(0, 5, 0);
      } else if(cycle == 1){
	/* rain dance */
	float y = 10 * cos(m2m * (oi + 1.0))/2;
	/* rotate around Y axis */
	glTranslatef(0, 0, -20);
	glRotatef(minProp * 360, 0, 1, 0);
	glTranslatef(0, y, 20);
      } else if(cycle == 2){
	/* metronome */
	theta = sin(-m2m * (oi + 1.0)) * 90;
	/* rotate around z axis at (-5, 0, 0) */
	glTranslatef(0, -5, 0);
	glRotatef(theta, 0, 0, 1);
	glTranslatef(0, 10, 0);
      } else if (cycle == 3){
	/* galaxy */
	mp = (num_objects - 1.0) / 2;
	op = mp - oi;
	dist = (int)(fabs(op)+0.5); /* dist from centre */
	/* make sure each object travels an integer number of loops in
	   a path through one cycle */
	pathLength = (int)((60.0 / dist) + 0.5) * 720.0;
	delta = pathLength / 2;
	theta = -minProp * delta - 180;
	/* rotate around X axis after translating (0,-5,0) */
	glTranslatef(0, 0, -20);
	glRotatef(minProp * 360 - 180, 1, 0, 0);
	glTranslatef(0, 0, 20);
	glTranslatef(0, -5, 0);
	/* rotate around Y axis */
	glTranslatef(0, 0, -20);
	glRotatef(theta, 0, 1, 0);
	glTranslatef(0, 0, 20);
      }

      /* spread out based on Z position */
      glTranslatef(0, 0, (z - 0.5) * 10);

      /* set up colours */
      glMaterialfv (GL_FRONT, GL_SPECULAR,            bspec);
      glMateriali  (GL_FRONT, GL_SHININESS,           bshiny);
      if(motionBlur){
	bcolor[3] = (timeDelta == 0) ? 1.0 : blurFrac; /* was ballAlpha */
      }
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, bcolor);
      glCallList (bp->beats_list); /* draw sphere */
      mi->polygon_count += (SPHERE_SLICES * SPHERE_STACKS);

      glPopMatrix();
    }
  }
  glPopMatrix();
  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_beats (ModeInfo *mi)
{
  beats_configuration *bp = &bps[MI_SCREEN(mi)];
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->colors) free (bp->colors);
  if (glIsList(bp->beats_list)) glDeleteLists(bp->beats_list, 1);
}

XSCREENSAVER_MODULE ("Beats", beats)

#endif /* USE_GL */
