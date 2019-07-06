/* energystream, Copyright (c) 2016 Eugene Sandulenko <sev@scummvm.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Based on Public Domain code by konrad "yoghurt" zagorowicz
 * for Tesla demo by Sunflower (http://www.pouet.net/prod.php?which=33)
 */

#define DEFAULTS  "*delay:  30000       \n" \
      "*count:        30          \n" \
      "*showFPS:      False       \n" \
      "*wireframe:    False       \n" \

# define release_stream 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "gltrackball.h"
#include "rotator.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

static float change_time = 0;
static float change_time1 = 25;
static float change_time2 = 40;
static float change_time3 = 60;

#define DEF_STREAMS     "16"

#include <sys/time.h>
#include <time.h>

typedef struct timeval streamtime;

#define GETSECS(t) ((t).tv_sec)
#define GETMSECS(t) ((t).tv_usec/1000)

#define DEF_SPIN        "False"
#define DEF_WANDER      "False"
#define DEF_SPEED       "1.0"


typedef struct {
  float x, y, z;
} Vector;

typedef struct {
  Vector *flares;
  int num_flares;
  GLuint flare_tex;
  float speed;
} flare_stream;

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  time_t start_time;

  int num_streams;
  flare_stream *streams;

} stream_configuration;

static stream_configuration *ess = NULL;

static int num_streams;
static Bool do_spin;
static GLfloat global_speed;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-streams",  ".streams",  XrmoptionSepArg, 0 },
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" }
};

static argtype vars[] = {
  {&num_streams, "streams",  "Streams",  DEF_STREAMS,  t_Int},
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&global_speed, "speed",  "Speed",  DEF_SPEED,  t_Float},
};

ENTRYPOINT ModeSpecOpt stream_opts = {countof(opts), opts, countof(vars), vars, NULL};

static void gettime(streamtime *t)
{
#ifdef GETTIMEOFDAY_TWO_ARGS
        struct timezone tzp;
        gettimeofday(t, &tzp);
#else /* !GETTIMEOFDAY_TWO_ARGS */
        gettimeofday(t);
#endif /* !GETTIMEOFDAY_TWO_ARGS */
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_stream (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
  gluLookAt (0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glClear (GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
stream_handle_event (ModeInfo *mi, XEvent *event)
{
  stream_configuration *es = &ess[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, es->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &es->button_down_p))
    return True;

  return False;
}

#define TEX_WIDTH 256
#define TEX_HEIGHT 256
#define COEFF 0.2

static GLuint gen_texture (void)
{
    int x, y, i;
    float color;
    GLuint tex;

    unsigned char *texture = (unsigned char *)calloc (TEX_WIDTH * TEX_HEIGHT, 4);
    unsigned char *ptr = texture;

    float tint[3];
    for (i = 0; i < 3; i++)
      tint[i] = 1.0 * random() / RAND_MAX;

    for (y = 0; y < TEX_HEIGHT; y++) {
        for (x = 0; x < TEX_WIDTH; x++) {
            color = 255 - sqrt((x - TEX_WIDTH / 2) * (x - TEX_WIDTH / 2) / COEFF
                        + (y - TEX_HEIGHT / 2) * (y - TEX_HEIGHT / 2) / COEFF);

            if (color < 0)
                color = 0;

            for (i = 0; i < 3; i++)
                ptr[i] = (unsigned char)(color * tint[i]);
            ptr[3] = color ? 255 : 0;

            ptr += 4;
        }
    }

  glGenTextures (1, &tex);
#ifdef HAVE_GLBINDTEXTURE
  glBindTexture (GL_TEXTURE_2D, tex);
#endif /* HAVE_GLBINDTEXTURE */
  glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
  clear_gl_error ();
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, TEX_WIDTH, TEX_HEIGHT,
      0, GL_RGBA, GL_UNSIGNED_BYTE, texture);
  check_gl_error ("texture");

  /* Texture parameters, LINEAR scaling for better texture quality */
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  free (texture);

  return tex;
}

static inline void vector_copy (Vector *a, Vector *b)
{
  a->x = b->x;
  a->y = b->y;
  a->z = b->z;
}

/* a += b */
static inline void vector_add (Vector *a, Vector *b)
{
  a->x += b->x;
  a->y += b->y;
  a->z += b->z;
}

/* a -= b */
static inline void vector_sub (Vector *a, Vector *b)
{
  a->x -= b->x;
  a->y -= b->y;
  a->z -= b->z;
}

static void init_flare_stream (flare_stream *s, int num_flares, float bx, float by, float bz, float speed)
{
  int i;

  s->flares = (Vector *) calloc (num_flares, sizeof (Vector));
  s->num_flares = num_flares;
  s->flare_tex = gen_texture();
  s->speed = speed;

  for (i = 0; i != s->num_flares; i++)
  {
    s->flares[i].x = -800.0f * random() / RAND_MAX - 1150 + bx;
    s->flares[i].y =   10.0f * random() / RAND_MAX -   20 + by;
    s->flares[i].z =   10.0f * random() / RAND_MAX -   20 + bz;
  }
}

static void render_flare_stream (flare_stream *s, float cur_time, Vector *vx, Vector *vy, float alpha)
{
  float fMultipler = 1;
  int i;

  if (s->flare_tex == -1)
    return;
  if (!s->num_flares)
    return;

  if (cur_time < change_time)
    return;

  cur_time -= change_time;

  glColor4f (1.0, 1, 1, alpha);
#ifdef HAVE_GLBINDTEXTURE
  glBindTexture (GL_TEXTURE_2D, s->flare_tex);
#endif

  glBegin (GL_QUADS);

  if (cur_time + change_time > change_time1)
  {
    if (cur_time + change_time > change_time2)
    {
      fMultipler = 2.5;
    }
    else
      fMultipler = 2;
  }

  for (i = 0; i != s->num_flares; i++)
  {
    Vector flare_pos;
    Vector cc;

    flare_pos.x = fmod (s->flares[i].x + cur_time * s->speed * fMultipler, 800) - 400;
    flare_pos.y = s->flares[i].y + 2 * sin (cur_time * 7 + s->flares[i].x);
    flare_pos.z = s->flares[i].z + 2 * cos (cur_time * 7 + i * 3.14);

    glTexCoord2f (0, 0);
    vector_copy  (&cc, &flare_pos);
    vector_sub   (&cc, vx);
    vector_add   (&cc, vy);
    glVertex3fv  ((float *)&cc);

    glTexCoord2f ( 1, 0 );
    vector_copy  (&cc, &flare_pos);
    vector_add   (&cc, vx);
    vector_add   (&cc, vy);
    glVertex3fv  ((float *)&cc);

    glTexCoord2f ( 1, 1 );
    vector_copy  (&cc, &flare_pos);
    vector_add   (&cc, vx);
    vector_sub   (&cc, vy);
    glVertex3fv  ((float *)&cc);

    glTexCoord2f ( 0, 1 );
    vector_copy  (&cc, &flare_pos);
    vector_sub   (&cc, vx);
    vector_sub   (&cc, vy);
    glVertex3fv  ((float *)&cc);
  }

  glEnd ();
}

ENTRYPOINT void
init_stream (ModeInfo *mi)
{
  stream_configuration *es;
  streamtime current_time;

  MI_INIT (mi, ess);

  es = &ess[MI_SCREEN(mi)];

  es->glx_context = init_GL (mi);

  reshape_stream (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  gettime (&current_time);
  es->start_time = GETSECS(current_time) * 1000 + GETMSECS(current_time);

  es->num_streams = num_streams;

  es->streams = (flare_stream *) calloc (es->num_streams, sizeof(flare_stream));

  init_flare_stream (&es->streams[0], 150, 0, 50, 0, 300);
  init_flare_stream (&es->streams[1], 150, 0, 0, 0, 150);
  init_flare_stream (&es->streams[2], 150, 0, 90, 60, 250);
  init_flare_stream (&es->streams[3], 150, 0, -100, 30, 160);
  init_flare_stream (&es->streams[4], 150, 0, 50, -100, 340);
  init_flare_stream (&es->streams[5], 150, 0, -50, 50, 270 );
  init_flare_stream (&es->streams[6], 150, 0, 100, 50, 180);
  init_flare_stream (&es->streams[7], 150, 0, -30, 90, 130);

  init_flare_stream (&es->streams[8], 150, 0, 150, 10, 200);
  init_flare_stream (&es->streams[9], 150, 0, 100, -100, 210);
  init_flare_stream (&es->streams[10], 150, 0, 190, 160, 220);
  init_flare_stream (&es->streams[11], 150, 0, -200, 130, 230);
  init_flare_stream (&es->streams[12], 150, 0, 150, -200, 240);
  init_flare_stream (&es->streams[13], 150, 0, -150, 250, 160);
  init_flare_stream (&es->streams[14], 150, 0, 200, 150, 230);
  init_flare_stream (&es->streams[15], 150, 0, -130, 190, 250);

  {
    double spin_speed   = 0.5  * global_speed;
    double wander_speed = 0.02 * global_speed;
    double spin_accel   = 1.1;

    es->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            True);
    es->trackball = gltrackball_init (True);
  }
}

ENTRYPOINT void
free_stream (ModeInfo * mi)
{
  stream_configuration *es = &ess[MI_SCREEN(mi)];
  int i;

  if (!es->glx_context) return;
  glXMakeCurrent (MI_DISPLAY(mi), MI_WINDOW(mi), *es->glx_context);

  if (es->trackball) gltrackball_free (es->trackball);
  if (es->rot) free_rotator (es->rot);

  for (i = 0; i < es->num_streams; i++) {
    free (es->streams[i].flares);
    glDeleteTextures (1, &es->streams[i].flare_tex);
  }

  if (es->streams) free (es->streams);

}


static void inverse_matrix (float m[16]) {
  double a,b,c,d,e,f,g,h,i,j,k,l;
  register double dW;

  a = m[ 0]; b = m[ 1]; c = m[ 2];
  d = m[ 4]; e = m[ 5]; f = m[ 6];
  g = m[ 8]; h = m[ 9]; i = m[10];
  j = m[12]; k = m[13]; l = m[14];

  dW = 1.0 / (a * (e * i - f * h)
           - (b * (d * i - f * g)
           +  c * (e * g - d * h)));

  m[ 0]= (float)((e * i - f * h) * dW);
  m[ 1]= (float)((c * h - b * i) * dW);
  m[ 2]= (float)((b * f - c * e) * dW);

  m[ 4]= (float)((f * g - d * i) * dW);
  m[ 5]= (float)((a * i - c * g) * dW);
  m[ 6]= (float)((c * d - a * f) * dW);

  m[ 8]= (float)((d * h - e * g) * dW);
  m[ 9]= (float)((b * g - a * h) * dW);
  m[10]= (float)((a * e - b * d) * dW);

  m[12]= (float)((e * (g * l - i * j)
                + f * (h * j - g * k)
                - d * (h * l - i * k)) * dW);
  m[13]= (float)((a * (h * l - i * k)
                + b * (i * j - g * l)
                + c * (g * k - h * j)) * dW);
  m[14]= (float)((b * (d * l - f * j)
                + c * (e * j - d * k)
                - a * (e * l - f * k)) * dW);
}

ENTRYPOINT void
draw_stream (ModeInfo *mi)
{
  stream_configuration *es = &ess[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  streamtime current_time;
  float cur_time;
  int i;
  float alpha = 1.0;
  Vector vx;
  Vector vy;
  GLfloat m[4*4];

  if (!es->glx_context)
    return;

  gettime (&current_time);

  cur_time = (float)(GETSECS(current_time) * 1000 + GETMSECS(current_time) - es->start_time) / 1000.0;
  cur_time *= global_speed;

  glXMakeCurrent (MI_DISPLAY(mi), MI_WINDOW(mi), *es->glx_context);

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glFrustum (-.6f, .6f, -.45f, .45f, 1, 1000);

  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  glEnable (GL_LIGHTING);
  glEnable (GL_TEXTURE_2D);
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE);
  glDisable (GL_CULL_FACE);
  glDisable (GL_DEPTH_TEST);
  glDepthMask (0);

  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);

  glTranslatef (0, 0, -300);
  glRotatef (cur_time * 30, 1, 0, 0);
  glRotatef (30 * sin(cur_time / 3) + 10, 0, 0, 1);

  {
    double x, y, z;
    get_position (es->rot, &x, &y, &z, !es->button_down_p);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 8,
                 (z - 0.5) * 15);

    gltrackball_rotate (es->trackball);

    get_rotation (es->rot, &x, &y, &z, !es->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  if (cur_time > change_time1)
  {
    if (cur_time > change_time2)
    {
      glRotatef (90, 0, 1, 0);

      if (cur_time > change_time3)
        es->start_time = GETSECS(current_time) * 1000 + GETMSECS(current_time) - 5000;
    }
    else
    {
      glRotatef (180, 0, 1, 0);
    }
  }

  glEnable ( GL_FOG);
  glFogf (GL_FOG_START, 200);
  glFogf (GL_FOG_END, 500);
  glFogf (GL_FOG_MODE, GL_LINEAR);

  glGetFloatv (GL_MODELVIEW_MATRIX, m);

  inverse_matrix (m);

  vx.x = m[0] * 10;
  vx.y = m[1] * 10;
  vx.z = m[2] * 10;

  vy.x = m[4] * 10;
  vy.y = m[5] * 10;
  vy.z = m[6] * 10;

  mi->polygon_count = 0;

  for (i = 0; i != es->num_streams; i++)
  {
    mi->polygon_count += es->streams[i].num_flares;
    render_flare_stream (&es->streams[i], cur_time, &vx, &vy, alpha);
  }

  glDisable (GL_TEXTURE_2D);
  glDisable (GL_LIGHTING);
  glDisable (GL_FOG);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers (dpy, window);
}

XSCREENSAVER_MODULE_2("EnergyStream", energystream, stream)

#endif /* USE_GL */
