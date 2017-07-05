/* vigilance, Copyright (c) 2017 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Draws surveillance cameras, taking an interest in their surroundings.
 */

#define DEFAULTS	"*delay:	20000       \n" \
			"*count:        5           \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*bodyColor:    #666666"   "\n" \
			"*capColor:     #FFFFFF"   "\n" \
			"*hingeColor:   #444444"   "\n" \
			"*mountColor:   #444444"   "\n" \
			"*lensColor:    #000000"   "\n" \
			"*groundColor:  #004400"   "\n" \

# define refresh_camera 0
# define release_camera 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define DEF_SPEED       "1.0"
#define DEF_CAMERA_SIZE "1.0"

#include "xlockmore.h"
#include "gltrackball.h"
#include "xpm-ximage.h"
#include "normals.h"

#include <ctype.h>

#ifdef USE_GL /* whole file */

#undef ABS
#define ABS(x) ((x)<0?-(x):(x))
#undef MAX
#define MAX(A,B) ((A)>(B)?(A):(B))
#undef MIN
#define MIN(A,B) ((A)<(B)?(A):(B))
#undef BELLRAND
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

#include "gllist.h"

extern const struct gllist
  *seccam_body, *seccam_cap, *seccam_hinge, *seccam_pipe, *seccam_lens;
static struct gllist *ground = 0;

static const struct gllist * const *all_objs[] = {
  &seccam_body, &seccam_cap, &seccam_hinge, &seccam_pipe, &seccam_lens,
  (const struct gllist * const *) &ground
};

#define CAMERA_BODY	0
#define CAMERA_CAP	1
#define CAMERA_HINGE	2
#define CAMERA_MOUNT	3
#define CAMERA_LENS	4
#define GROUND		5

#define BEAM_ZOFF 0.185	/* distance from origin to lens in model */

typedef enum { IDLE, WARM_UP, ZOT, COOL_DOWN } camera_state;


typedef struct {
  XYZ pos;
  GLfloat facing;		/* rotation around vertical axis, degrees */
  GLfloat pitch;		/* front/back tilt, degrees */
  GLfloat velocity;		/* most recent angular velocity, degrees */
  long focus_id;		/* pedestrian or camera of interest */
  XYZ focus;			/* point of interest */
  camera_state state;
  GLfloat tick;
} camera;

typedef struct pedestrian pedestrian;
struct pedestrian {
  long id;
  XYZ pos;
  GLfloat length;
  GLfloat frequency, amplitude;
  GLfloat ratio;
  GLfloat speed;
  pedestrian *next;
};

typedef struct {
  GLXContext *glx_context;
  trackball_state *user_trackball;
  Bool button_down_p;

  GLuint *dlists;
  GLfloat component_colors[countof(all_objs)][4];

  int ncameras;
  camera *cameras;
  pedestrian *pedestrians;
} camera_configuration;

static camera_configuration *bps = NULL;

static GLfloat speed_arg;
#ifdef DEBUG
static int debug_p;
#endif

static XrmOptionDescRec opts[] = {
  { "-speed",      ".speed",     XrmoptionSepArg, 0 },
#ifdef DEBUG
  {"-debug",       ".debug",     XrmoptionNoArg, "True" },
  {"+debug",       ".debug",     XrmoptionNoArg, "False" },
#endif
};

static argtype vars[] = {
  {&speed_arg,   "speed",      "Speed",     DEF_SPEED,      t_Float},
#ifdef DEBUG
  {&debug_p,     "debug",      "Debug",     "False",        t_Bool},
#endif
};

ENTRYPOINT ModeSpecOpt camera_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


ENTRYPOINT void
reshape_camera (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  glViewport (0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 200);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0, 0, 30,
             0, 0, 0,
             0, 1, 0);

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
camera_handle_event (ModeInfo *mi, XEvent *event)
{
  camera_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->user_trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == ' ' || c == '\t')
        {
          int i;
          if (bp->cameras[0].state == IDLE && bp->pedestrians)
            for (i = 0; i < bp->ncameras; i++)
              {
                bp->cameras[i].state = WARM_UP;
                bp->cameras[i].tick  = 1.0;
              }
          return True;
        }
    }

  return False;
}


static int draw_ground (ModeInfo *, GLfloat color[4]);

static void
parse_color (ModeInfo *mi, char *key, GLfloat color[4])
{
  XColor xcolor;
  char *string = get_string_resource (mi->dpy, key, "CameraColor");
  if (!XParseColor (mi->dpy, mi->xgwa.colormap, string, &xcolor))
    {
      fprintf (stderr, "%s: unparsable color in %s: %s\n", progname,
               key, string);
      exit (1);
    }

  color[0] = xcolor.red   / 65536.0;
  color[1] = xcolor.green / 65536.0;
  color[2] = xcolor.blue  / 65536.0;
  color[3] = 1;
}


ENTRYPOINT void 
init_camera (ModeInfo *mi)
{
  camera_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;
  MI_INIT (mi, bps, 0);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_camera (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  if (!wire)
    {
      GLfloat pos[4] = {0.4, 0.2, 0.4, 0.0};
      GLfloat amb[4] = {0.2, 0.2, 0.2, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {1.0, 1.0, 1.0, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  bp->user_trackball = gltrackball_init (False);

  bp->dlists = (GLuint *) calloc (countof(all_objs)+1, sizeof(GLuint));
  for (i = 0; i < countof(all_objs); i++)
    bp->dlists[i] = glGenLists (1);

  for (i = 0; i < countof(all_objs); i++)
    {
      const struct gllist *gll = *all_objs[i];
      char *key = 0;
      GLfloat spec1[4] = {1.00, 1.00, 1.00, 1.0};
      GLfloat spec2[4] = {0.40, 0.40, 0.70, 1.0};
      GLfloat *spec = spec1;
      GLfloat shiny = 20;

      glNewList (bp->dlists[i], GL_COMPILE);

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glMatrixMode(GL_TEXTURE);
      glPushMatrix();
      glMatrixMode(GL_MODELVIEW);

      glRotatef (-90, 1, 0, 0);
      glRotatef (180, 0, 0, 1);
      glScalef (6, 6, 6);

      glBindTexture (GL_TEXTURE_2D, 0);

      switch (i) {
      case CAMERA_BODY:  key = "bodyColor";   break;
      case CAMERA_CAP:   key = "capColor";    break;
      case CAMERA_HINGE: key = "hingeColor";  break;
      case CAMERA_MOUNT: key = "mountColor";  break;
      case CAMERA_LENS:  key = "lensColor";   spec = spec2; break;
      case GROUND:       key = "groundColor"; spec = spec2; shiny = 128; break;
      default: abort(); break;
      }

      parse_color (mi, key, bp->component_colors[i]);

      glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  spec);
      glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, shiny);

      switch (i) {
      case GROUND:
        if (! ground)
          ground = (struct gllist *) calloc (1, sizeof(*ground));
        ground->points = draw_ground (mi, bp->component_colors[i]);
        break;
      default:
        renderList (gll, wire);
        /* glColor3f (1, 1, 1); renderListNormals (gll, 100, True); */
        /* glColor3f (1, 1, 0); renderListNormals (gll, 100, False); */
        break;
      }

      glMatrixMode(GL_TEXTURE);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();

      glEndList ();
    }

  bp->ncameras = MI_COUNT(mi);
  if (bp->ncameras <= 0) bp->ncameras = 1;
  bp->cameras = (camera *) calloc (bp->ncameras, sizeof (camera));

  {
    GLfloat range = (MI_COUNT(mi) <= 2) ? 4 : 5.5;
    GLfloat extent;
    GLfloat spacing = range / bp->ncameras;
    if (spacing < 0.7) spacing = 0.7;
    extent = spacing * (bp->ncameras - 1);
    for (i = 0; i < bp->ncameras; i++)
      {
        camera *c = &bp->cameras[i];
        c->state = IDLE;
        c->pos.x = i*spacing - extent/2;
        c->pos.z += 0.7;
        if (spacing < 1.6)
          c->pos.z = (i & 1 ? 1.1 : -0.3);
        c->focus.x = c->pos.x;
        c->focus.y = c->pos.y + 1;
        c->focus.z = c->pos.z + BEAM_ZOFF;
        c->pitch   = -50;
      }
  }


# ifdef DEBUG
  if (!debug_p)
# endif
    /* Let's tilt the floor a little. */
    gltrackball_reset (bp->user_trackball,
                       -0.70 + frand(1.58),
                       -0.30 + frand(0.40));
}


static XYZ
normalize (XYZ p)
{
  GLfloat d = sqrt(p.x*p.x + p.y*p.y * p.z*p.z);
  if (d < 0.0000001)
    p.x = p.y = p.z = 0;
  else
    {
      p.x /= d;
      p.y /= d;
      p.z /= d;
    }
    
  return p;
}


static GLfloat
vector_angle (XYZ a, XYZ b)
{
  double La = sqrt (a.x*a.x + a.y*a.y + a.z*a.z);
  double Lb = sqrt (b.x*b.x + b.y*b.y + b.z*b.z);
  double cc, angle;

  if (La == 0 || Lb == 0) return 0;
  if (a.x == b.x && a.y == b.y && a.z == b.z) return 0;

  /* dot product of two vectors is defined as:
       La * Lb * cos(angle between vectors)
     and is also defined as:
       ax*bx + ay*by + az*bz
     so:
      La * Lb * cos(angle) = ax*bx + ay*by + az*bz
      cos(angle)  = (ax*bx + ay*by + az*bz) / (La * Lb)
      angle = acos ((ax*bx + ay*by + az*bz) / (La * Lb));
  */
  cc = (a.x*b.x + a.y*b.y + a.z*b.z) / (La * Lb);
  if (cc > 1) cc = 1;  /* avoid fp rounding error (1.000001 => sqrt error) */
  angle = acos (cc);

  return (angle);
}


static int
draw_component (ModeInfo *mi, int i, GLfloat color[4])
{
  camera_configuration *bp = &bps[MI_SCREEN(mi)];
  if (! color)
    color = bp->component_colors[i];
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
  glCallList (bp->dlists[i]);
  return (*all_objs[i])->points / 3;
}


static int
draw_double_component (ModeInfo *mi, int i)
{
  int count = draw_component (mi, i, 0);

  glFrontFace(GL_CCW);
  glScalef (1, 1, -1);
  count += draw_component (mi, i, 0);
  glScalef (1, 1, -1);
  glFrontFace(GL_CW);
  return count;
}


static int
draw_ray (ModeInfo *mi, camera *c)
{
  int wire = MI_IS_WIREFRAME(mi);
  int count = 0;
  GLfloat length = 10000;
  GLfloat thickness = 0.08;
  int i;

  glPushMatrix();
  glTranslatef (c->pos.x, c->pos.y, c->pos.z + BEAM_ZOFF);
  glRotatef (-c->facing, 0, 0, 1);
  glRotatef ( c->pitch,  1, 0, 0);
  glRotatef (frand(90),  0, 1, 0);
  glScalef (thickness, length, thickness);
  glDisable (GL_LIGHTING);

  for (i = 0; i < 5; i++)
    {
      glColor4f (1, 0, 0, 0.1 + (i * 0.18));
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glVertex3f (0, 0, -0.5); glVertex3f (0, 0,  0.5);
      glVertex3f (0, 1,  0.5); glVertex3f (0, 1, -0.5);
      glEnd();
      count++;
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glVertex3f (-0.5, 0, 0); glVertex3f ( 0.5, 0, 0);
      glVertex3f ( 0.5, 1, 0); glVertex3f (-0.5, 1, 0);
      glEnd();
      count++;
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glVertex3f (0, 1, -0.5); glVertex3f (0, 1,  0.5);
      glVertex3f (0, 0,  0.5); glVertex3f (0, 0, -0.5);
      glEnd();
      count++;
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glVertex3f (-0.5, 1, 0); glVertex3f ( 0.5, 1, 0);
      glVertex3f ( 0.5, 0, 0); glVertex3f (-0.5, 0, 0);
      glEnd();
      count++;
      glScalef (0.7, 1, 0.7);
    }
  if (!MI_IS_WIREFRAME(mi))
    glEnable (GL_LIGHTING);
  glPopMatrix();
  return count;
}


static int
draw_camera_1 (ModeInfo *mi, camera *c)
{
  int count = 0;
  GLfloat scale = 0.01;
  GLfloat color[4] = { 1, 0, 0, 1 };
  glPushMatrix();

  glTranslatef (c->pos.x, c->pos.y, c->pos.z);
  glScalef (scale, scale, scale);

  glRotatef (90, 1, 0, 0);
  glRotatef (-90, 0, 1, 0);

  count += draw_double_component (mi, CAMERA_MOUNT);

  glRotatef (-c->facing, 0, 1, 0);
  glRotatef (-c->pitch,  0, 0, 1);

  count += draw_double_component (mi, CAMERA_HINGE);

  if (c->state == WARM_UP)
    {
      if (c->tick < 0.2)
        glTranslatef ((0.2 - c->tick) / (scale * 3), 0, 0);
    }

  if (c->state == ZOT)
    {
      glTranslatef ((0.005 - frand(0.01)) / scale,
                    (0.005 - frand(0.01)) / scale,
                    (0.005 - frand(0.01)) / scale);
    }

  count += draw_double_component (mi, CAMERA_BODY);

  if (c->state == ZOT)
    {
      glTranslatef ((0.005 - frand(0.01)) / scale,
                    (0.005 - frand(0.01)) / scale,
                    (0.005 - frand(0.01)) / scale);
    }

  count += draw_double_component (mi, CAMERA_CAP);

  switch (c->state) {
  case IDLE:      break;
  case WARM_UP:   color[0] = 1.0 - c->tick; break;
  case ZOT:       color[0] = 1.0;           break;
  case COOL_DOWN: color[0] = c->tick;       break;
  default: abort(); break;
  }

  count += draw_component (mi, CAMERA_LENS, 
                           (c->state == IDLE ? 0 : color));

# ifdef DEBUG
  if (debug_p && c->state != ZOT)
    {
      glDisable (GL_LIGHTING);
      glColor3f (1, 1, 0);
      glBegin (GL_LINES);
      glVertex3f (0, BEAM_ZOFF / scale, 0);
      glVertex3f (-100 / scale, BEAM_ZOFF / scale, 0);
      glEnd();
      if (!MI_IS_WIREFRAME(mi))
        glEnable (GL_LIGHTING);
    }
# endif

  glPopMatrix();

  return count;
}


/* The position this pedestrian would appear at during the given ratio
   through its life cycle.
 */
static XYZ
pedestrian_position (pedestrian *p, GLfloat ratio)
{
  XYZ pos = p->pos;
  if (p->speed < 0)
    ratio = 1-ratio;
  pos.x += p->length * ratio;
  pos.z += sin (M_PI * ratio * p->frequency * 2) * p->amplitude
    + p->amplitude/2;
  return pos;
}


static int
draw_pedestrian (ModeInfo *mi, pedestrian *p)
{
  int count = 0;
# ifdef DEBUG
  if (debug_p)
    {
      GLfloat r;
      GLfloat step = 0.001;
      glDisable (GL_LIGHTING);
      glColor3f (0, 0, 1);

      glBegin (GL_LINE_STRIP);
      glVertex3f (p->pos.x, p->pos.y, p->pos.z + p->amplitude);
      glVertex3f (p->pos.x, p->pos.y, 0);
      glVertex3f (p->pos.x + p->length, p->pos.y, 0);
      glVertex3f (p->pos.x + p->length, p->pos.y, p->pos.z + p->amplitude);
      glEnd();

      glBegin (GL_LINE_STRIP);
      for (r = 0; r <= 1; r += step)
        {
          XYZ pos = pedestrian_position (p, r);
          glVertex3f (pos.x, pos.y, pos.z);
          count++;
          if (p->ratio >= r && p->ratio < r + step)
            {
              glVertex3f (pos.x, pos.y, pos.z - p->amplitude);
              glVertex3f (pos.x, pos.y, pos.z + p->amplitude);
              glVertex3f (pos.x, pos.y, pos.z);
              count++;
            }
        }
      glEnd();
      if (!MI_IS_WIREFRAME(mi))
        glEnable (GL_LIGHTING);
    }
# endif
  return count;
}


/* Start someone walking through the scene.
 */
static void
add_pedestrian (ModeInfo *mi)
{
  camera_configuration *bp = &bps[MI_SCREEN(mi)];
  pedestrian *p = (pedestrian *) calloc (1, sizeof(*p));
  static int id = 100;
  p->id = id++;
  p->length = 35;
  p->ratio = 0;
  p->pos.x = -p->length/2;
  p->pos.y = 3 + frand(10);
  p->pos.z = -1.5 + frand(4) + ((random() % 10) ? 0 : frand(8));
  p->frequency = 4 + frand(4);
  p->amplitude = 0.1 + ((random() % 10) ? BELLRAND(0.45) : BELLRAND(1.5));
  p->ratio = 0;
  p->speed = ((4 + frand(4) + ((random() % 10) ? 0 : frand(10)))
              * (random() & 1 ? 1 : -1)
              * speed_arg);

  if (bp->pedestrians)
    {
      pedestrian *p2;	/* add it to the end */
      for (p2 = bp->pedestrians; p2->next; p2 = p2->next)
        ;
      p2->next = p;
    }
  else
    {
      p->next = bp->pedestrians;
      bp->pedestrians = p;
    }
}


static void
free_pedestrian (ModeInfo *mi, pedestrian *p)
{
  camera_configuration *bp = &bps[MI_SCREEN(mi)];
  Bool ok = False;
  if (!p) abort();
  if (p == bp->pedestrians)
    {
      bp->pedestrians = p->next;
      ok = True;
    }
  else
    {
      pedestrian *p0;
      for (p0 = bp->pedestrians; p0; p0 = p0->next)
        if (p0->next == p)
          {
            p0->next = p0->next ? p0->next->next : 0;
            ok = True;
            break;
          }
    }
  if (!ok) abort();
  p->next = 0;
  free (p);
}


static void
tick_pedestrian (ModeInfo *mi, pedestrian *p)
{
  p->ratio += 0.001 * (p->speed > 0 ? p->speed : -p->speed);
  if (p->ratio >= 1)
    free_pedestrian (mi, p);
}


/* Extract the position of the thing this camera would like to look at.
 */
static void
set_camera_focus (ModeInfo *mi, camera *c)
{
  camera_configuration *bp = &bps[MI_SCREEN(mi)];

  if (c->focus_id > 0)		/* Look at pedestrian with id N */
    {
      long id = c->focus_id;
      pedestrian *p;
      for (p = bp->pedestrians; p; p = p->next)
        if (p->id == id)
          break;
      if (p)
        c->focus = pedestrian_position (p, p->ratio);
      else
        c->focus_id = 0;  /* that pedestrian has escaped */
    }
  else if (c->focus_id < 0)	/* Look at camera -N-1 */
    {
      long n = -c->focus_id - 1;
      if (bp->ncameras > n)
        c->focus = bp->cameras[n].pos;
    }
}


static void
tick_camera (ModeInfo *mi, camera *c)
{
  camera_configuration *bp = &bps[MI_SCREEN(mi)];

  GLfloat X, Y, Z;
  set_camera_focus (mi, c);

  X = c->focus.x - c->pos.x;
  Y = c->focus.y - c->pos.y;
  Z = c->focus.z - c->pos.z - BEAM_ZOFF;

  if (X != 0 || Y != 0)
    {
      GLfloat ofacing = c->facing;
      GLfloat opitch  = c->pitch;

      GLfloat target_facing = atan2 (X, Y) * (180 / M_PI);
      GLfloat target_pitch  = atan2 (Z, sqrt(X*X + Y*Y)) * (180 / M_PI);

      /* Adjust the current velocity.
         If we are nearing the target, slow down (but don't stop).
         Otherwise, speed up (but don't break the speed limit).
       */
      {
        GLfloat accel = 0.5 * speed_arg;
        GLfloat decel_range = 20;
        GLfloat max_velocity = 5 * speed_arg;
        GLfloat close_enough = 0.5 * speed_arg;

        GLfloat dx = target_facing - c->facing;
        GLfloat dy = target_pitch  - c->pitch;
        GLfloat angular_distance = sqrt (dx*dx + dy*dy);

        /* Split the velocity into vx and vy components.  This isn't
           quite right since this treats them as Cartesian rather than
           polar, but it's close enough.
         */
        GLfloat r = (dx == 0) ? 1 : ABS(dy) / ABS(dx);
        GLfloat vx = 1-r;
        GLfloat vy = r;

        if (angular_distance < decel_range)  /* Nearing target, slow down */
          {
            c->velocity -= accel;
            if (c->velocity <= 0)
              c->velocity = accel;
          }
        else
          {
            c->velocity += accel;
            if (c->velocity > max_velocity)
              c->velocity = max_velocity;
          }

        /* Don't overshoot */
        if (vx > ABS(dx)) vx = ABS(dx);
        if (vy > ABS(dy)) vy = ABS(dy);


        /* Rotate toward target by current angular velocity. */
        c->facing += vx * c->velocity * (target_facing > c->facing ? 1 : -1);
        c->pitch  += vy * c->velocity * (target_pitch  > c->pitch  ? 1 : -1);

        /* If we are pointed *really close* to the target, just lock on,
           to avoid twitchy overshoot rounding errors.
         */
        if (angular_distance < close_enough)
          {
            c->facing = target_facing;
            c->pitch  = target_pitch;
          }

        /* Constrain to hinge's range of motion */
        c->facing = MAX (-90, MIN (90, c->facing));
        c->pitch  = MAX (-90, MIN (55, c->pitch));

        /* After this motion, our prevailing velocity (for next time)
           is the angular distance we actually moved.
         */
        dx = c->facing - ofacing;
        dy = c->pitch  - opitch;
        c->velocity = sqrt (dx*dx + dy*dy);
      }
    }

# ifdef DEBUG
  if (debug_p && 1)
    /* Mark the point on which this camera is focusing. */
    {
      glPushMatrix();
      glDisable (GL_LIGHTING);
      glColor3f(0.3, 0.3, 0.3);
      glBegin (GL_LINES);
      glVertex3f (c->pos.x,   c->pos.y,   c->pos.z + BEAM_ZOFF);
      glVertex3f (c->focus.x, c->focus.y, c->focus.z);
      glEnd();
      glColor3f(1,1,1);
      glBegin (GL_LINES);
      glVertex3f (c->focus.x-0.25, c->focus.y, c->focus.z);
      glVertex3f (c->focus.x+0.25, c->focus.y, c->focus.z);
      glVertex3f (c->focus.x, c->focus.y-0.25, c->focus.z);
      glVertex3f (c->focus.x, c->focus.y+0.25, c->focus.z);
      glVertex3f (c->focus.x, c->focus.y, c->focus.z-0.25);
      glVertex3f (c->focus.x, c->focus.y, c->focus.z+0.25);
      glEnd();
      if (!MI_IS_WIREFRAME(mi))
        glEnable (GL_LIGHTING);
      glPopMatrix();
    }
# endif

  /* If this camera is looking at another camera, get shy and look away
     if the target sees us looking.
   */
  if (c->focus_id < 0)
    {
      camera *c2 = &bp->cameras[-1 - c->focus_id];
      XYZ a, b;
      GLfloat aa = c->facing / (180 / M_PI);
      GLfloat bb = c->pitch  / (180 / M_PI);
      GLfloat angle;

      a.y = cos(aa)* cos(bb);
      a.x = sin(aa)* cos(bb);
      a.z = sin(bb);

      aa = c2->facing / (180 / M_PI);
      bb = c2->pitch  / (180 / M_PI);
      b.y = cos(aa)* cos(bb);
      b.x = sin(aa)* cos(bb);
      b.z = sin(bb);

      angle = vector_angle (normalize(a), normalize(b)) * (180 / M_PI);

      if (angle > 100)
        {
          c->focus_id = 0;
          /* Look "away" which means in the same direction. */
          c->focus.x = c->pos.x + (c2->focus.x - c2->pos.x);
          c->focus.y = c->pos.y + (c2->focus.y - c2->pos.y);
          c->focus.z = c->pos.z + (c2->focus.z - c2->pos.z);
          /* Look at either the sky or the ground.*/
          c->focus.z = c->focus.x * (random() & 1 ? 1 : -1) * 5;
          c->velocity = c2->velocity * 3;
        }
    }


  /* Randomly pick some other things to look at.
   */

  if (c->state == IDLE &&
      (c->focus_id <= 0
       ? !(random() % (int) MAX (1, (50 / speed_arg)))
       : !(random() % (int) MAX (1, (1000 / speed_arg)))))
    {

      /* Shiny! Start paying attention to something else. */

      if ((bp->ncameras > 1 && !(random() % 20)))	/* look at a camera */
        {
          int which = random() % 4;
          long i;
          for (i = 0; i < bp->ncameras; i++)
            if (c == &bp->cameras[i])
              break;

          /* Look at cameras 1 or 2 away in each direction, but not farther.
             Since we arrange them in 2 staggered lines, those are the only
             four that are in line of sight.
           */
          if (i >= 2 && which == 0)
            which = i-2;
          else if (i >= 1 && which == 1)
            which = i-1;
          else if (i < bp->ncameras-1 && which == 2)
            which = i+2;
          else /* (i < bp->ncameras-2 && which == 3) */
            which = i+1;

          c->focus_id = -1 - which;
        }
      else					/* look at a pedestrian */
        {
          int n = 0;
          pedestrian *p;
          for (p = bp->pedestrians; p; p = p->next)
            n++;
          if (n > 0)
            {
              n = random() % n;
              p = bp->pedestrians;
              while (n > 0 && p)
                p = p->next;
              if (p)
                c->focus_id = p->id;
            }
        }
    }

  /* Run the animation */

  if (c->state != IDLE)
    {
      pedestrian *p = bp->pedestrians; /* first one */
      if (p) c->focus_id = p->id;

      switch (c->state) {
      case WARM_UP:   c->tick -= 0.01 * speed_arg; break;
      case ZOT:       c->tick -= 0.006 * speed_arg; 
        if (p) p->speed *= 0.995;  /* target takes 1d6 HP damage */
        break;
      case COOL_DOWN: c->tick -= 0.02 * speed_arg; break;
      default: abort(); break;
      }

      if (c->tick <= 0)
        {
          c->tick = 1.0;
          switch (c->state) {
          case WARM_UP:   c->state = ZOT; break;
          case ZOT:       c->state = COOL_DOWN;
            c->focus_id = 0;
            if (p) p->ratio = 1.0;  /* threat eliminated */
            break;
          case COOL_DOWN: c->state = IDLE; break;
          default: abort(); break;
          }
        }
    }

  if (bp->cameras[0].state == IDLE &&
      bp->pedestrians &&
      bp->pedestrians[0].ratio < 0.3 &&
      !(random() % MAX (1, (int) (50000 / speed_arg))))
    {
      /* CASE NIGHTMARE RED detected, engage SCORPION STARE by authority
         of MAGINOT BLUE STARS. */
      int i;
      for (i = 0; i < bp->ncameras; i++)
        {
          bp->cameras[i].state = WARM_UP;
          bp->cameras[i].tick  = 1.0;
        }
    }
}


static int
draw_ground (ModeInfo *mi, GLfloat color[4])
{
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat i, j, k, h;

  /* When using fog, iOS apparently doesn't like lines or quads that are
     really long, and extend very far outside of the scene. Maybe?  If the
     length of the line (cells * cell_size) is greater than 25 or so, lines
     that are oriented steeply away from the viewer tend to disappear
     (whether implemented as GL_LINES or as GL_QUADS).

     So we do a bunch of smaller grids instead of one big one.
  */
  int cells = 20;
  GLfloat cell_size = 0.4;
  int points = 0;
  int gridsw = 10;
  int gridsh = 2;

  glPushMatrix();

  if (!wire)
    {
      GLfloat fog_color[4] = { 0, 0, 0, 1 };

      glLineWidth (2);
      glEnable (GL_LINE_SMOOTH);
      glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
      glEnable (GL_BLEND);

      glFogi (GL_FOG_MODE, GL_EXP2);
      glFogfv (GL_FOG_COLOR, fog_color);
      glFogf (GL_FOG_DENSITY, 0.017);
      glFogf (GL_FOG_START, -cells/2 * cell_size * gridsh);
      glEnable (GL_FOG);
    }

  glColor4fv (color);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);

  glTranslatef (-cells * gridsw * cell_size / 2, 0, 0);
  for (h = 0; h < 2; h++)
    {
      glPushMatrix();
      glTranslatef (0, cells * cell_size / 2, 0);
      for (j = 0; j < gridsh; j++)
        {
          glPushMatrix();
          for (k = 0; k < gridsw; k++)
            {
              glBegin (GL_LINES);
              for (i = -cells/2; i < cells/2; i++)
                {
                  GLfloat a = i * cell_size;
                  GLfloat b = cells/2 * cell_size;
                  glVertex3f (a, -b, 0); glVertex3f (a, b, 0); points++;
                  glVertex3f (-b, a, 0); glVertex3f (b, a, 0); points++;
                }
              glEnd();
              glTranslatef (cells * cell_size, 0, 0);
            }
          glPopMatrix();
          glTranslatef (0, cells * cell_size, 0);
        }
      glPopMatrix();
      glRotatef (90, 1, 0, 0);
    }

  if (!wire)
    glDisable (GL_LINE_SMOOTH);

  glPopMatrix();

  return points;
}


ENTRYPOINT void
draw_camera (ModeInfo *mi)
{
  camera_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  GLfloat camera_size;
  int i;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

# ifdef HAVE_MOBILE
  glRotatef (current_device_rotation(), 0, 0, 1);  /* right side up */
# endif

  gltrackball_rotate (bp->user_trackball);

# ifdef HAVE_MOBILE
  {
    GLfloat s = 0.6;
    glScalef (s, s, s);
  }
# endif

# ifdef DEBUG
  if (debug_p)
    {
      GLfloat s = 0.2;
      glScalef (s, s, s);
      glRotatef (30, 0, 1, 0);
      glRotatef (15, 1, 0, 0);
      glTranslatef (0, 0, -80);
    }
# endif

  mi->polygon_count = 0;

  camera_size = 5;

  if (MI_COUNT(mi) <= 2)    /* re-frame the scene a little bit */
    glTranslatef (0, -1, 7);
  if (MI_COUNT(mi) >= 20)
    glTranslatef (0, -1.5, -5);
  if (MI_COUNT(mi) >= 40)
    glTranslatef (0, 2, -15);

  glScalef (camera_size, camera_size, camera_size);

  /* +Z is toward sky; +X is toward the right along the back wall;
     +Y is toward the viewer. */
  glRotatef (-90, 1, 0, 0);
  glScalef (1, -1, 1);

  glPushMatrix();
  glScalef (1/camera_size, 1/camera_size, 1/camera_size);
  glTranslatef (0, -2.38, -8);  /* Move the ground down and back */
  glCallList (bp->dlists[GROUND]);
  mi->polygon_count += ground->points;

  glPopMatrix();

  {
    pedestrian *p, *p2;
    for (p = bp->pedestrians, p2 = p ? p->next : 0;
         p;
         p = p2, p2 = p2 ? p2->next : 0)
      {
        mi->polygon_count += draw_pedestrian (mi, p);
        tick_pedestrian (mi, p);  /* might free p */
      }

    if (!bp->pedestrians || !(random() % MAX (1, (int) (200 / speed_arg))))
      add_pedestrian (mi);
  }

  for (i = 0; i < bp->ncameras; i++)
    {
      camera *c = &bp->cameras[i];
      mi->polygon_count += draw_camera_1 (mi, c);
      tick_camera (mi, c);
    }

  for (i = 0; i < bp->ncameras; i++)
    {
      camera *c = &bp->cameras[i];
      if (c->state == ZOT)  /* Do this last, for alpha blending */
        mi->polygon_count += draw_ray (mi, c);
    }

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("Vigilance", vigilance, camera)

#endif /* USE_GL */
