/* splitflap, Copyright (c) 2015-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Draws a split-flap text display.
 */

#define FLAP_FONT "Helvetica Bold 144"

#define DEFAULTS	"*delay:	20000       \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*flapFont:   " FLAP_FONT  "\n" \
			"*frameColor:   #444444"   "\n" \
			"*caseColor:    #666666"   "\n" \
			"*discColor:    #888888"   "\n" \
			"*finColor:     #222222"   "\n" \
			"*textColor:    #FFFF00"   "\n" \
			"*multiSample:  True        \n" \
			"*program:      xscreensaver-text\n" \
			"*usePty:       False\n"

# define release_splitflap 0

#define DEF_SPEED       "1.0"
#define DEF_WIDTH       "22"
#define DEF_HEIGHT      "8"
#define DEF_SPIN        "XYZ"
#define DEF_WANDER      "True"
#define DEF_FACE_FRONT  "True"
#define DEF_MODE        "Text"

#include "xlockmore.h"

#include <ctype.h>

#ifdef USE_GL /* whole file */

#include "gltrackball.h"
#include "rotator.h"
#include "ximage-loader.h"
#include "utf8wc.h"
#include "textclient.h"
#include "texfont.h"
#include "gllist.h"

extern const struct gllist
  *splitflap_obj_box_quarter_frame, *splitflap_obj_disc_quarter,
  *splitflap_obj_fin_edge_half, *splitflap_obj_fin_face_half;
static struct gllist *splitflap_obj_outer_frame = 0;

static const struct gllist * const *all_objs[] = {
  &splitflap_obj_box_quarter_frame, &splitflap_obj_disc_quarter, 
  &splitflap_obj_fin_edge_half, &splitflap_obj_fin_face_half,
  (const struct gllist * const *) &splitflap_obj_outer_frame
};

#define SPLITFLAP_QUARTER_FRAME	0
#define SPLITFLAP_DISC_QUARTER	1
#define SPLITFLAP_FIN_EDGE_HALF	2
#define SPLITFLAP_FIN_FACE_HALF	3
#define SPLITFLAP_OUTER_FRAME	4

#define COLON_WIDTH 0.5

typedef struct {
  int target_index;		/* desired character */
  double current_index;		/* currently displayed, fractional */
  GLfloat sticky;		/* bottom fin doesn't fall all the way */
  int missing;			/* which fin has snapped off, or -1 */
  const char * const *spool;	/* chars available for display */
  int spool_size;		/* how many fins on the spool */
} flapper;

typedef struct {
  const char *text;
  GLuint texid;
  XCharStruct metrics;
  int tex_width, tex_height;
} texinfo;

typedef struct {
  GLXContext *glx_context;
  rotator *rot, *rot2;
  trackball_state *trackball;
  Bool button_down_p;
  Bool spinx, spiny, spinz;

  texinfo *texinfo;
  int texinfo_size;

  GLuint *dlists;
  GLfloat component_colors[countof(all_objs)][4];
  GLfloat text_color[4];

  flapper *flappers;  /* grid_width * grid_height */

  texture_font_data *font_data;
  int ascent, descent;

  text_data *tc;
  unsigned char text[5];
  int linger;
  int clock_p;
  Bool first_time_p;

} splitflap_configuration;

static const char * const digit_s1_spool[] = { " ", "1" };
static const char * const digit_01_spool[] = { "0", "1" };
static const char * const ap_spool[]       = { "A", "P" };
static const char * const m_spool[]        = { "M" };
static const char * const digit_05_spool[] = { "0", "1", "2", "3", "4", "5" };
static const char * const digit_spool[] = {
  "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

static const char * const ascii_spool[] = {
  " ", "!", "\"", "#", "$", "%", "&", "'",
  "(", ")", "*", "+", ",", "-", ".", "/",
  "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
  ":", ";", "<", "=", ">", "?", "@",
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
  "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
  "[", "\\", "]", "^", "_", "`", "{", "|", "}", "~", 
};


/* If we include these, the flappers just take too long. It's boring. */
static const char * const latin1_spool[] = {
  " ", "!", "\"", "#", "$", "%", "&", "'",
  "(", ")", "*", "+", ",", "-", ".", "/",
  "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
  ":", ";", "<", "=", ">", "?", "@",
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
  "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
  "[", "\\", "]", "^", "_", "`", "{", "|", "}", "~", 

  "\302\241", "\302\242", "\302\243", "\302\245",
  "\302\247", "\302\251", "\302\265", "\302\266",

  "\303\200", "\303\201", "\303\202", "\303\203",
  "\303\204", "\303\205", "\303\206", "\303\207",
  "\303\210", "\303\211", "\303\212", "\303\213",
  "\303\214", "\303\215", "\303\216", "\303\217",
  "\303\220", "\303\221", "\303\222", "\303\223",
  "\303\224", "\303\225", "\303\226", "\303\230",
  "\303\231", "\303\232", "\303\233", "\303\234",
  "\303\235", "\303\236", "\303\237", "\303\267",
};


static splitflap_configuration *bps = NULL;

static GLfloat speed;
static int grid_width, grid_height;
static char *do_spin;
static Bool do_wander;
static Bool face_front_p;
static char *mode_str;

static XrmOptionDescRec opts[] = {
  { "-speed",   ".speed",     XrmoptionSepArg, 0 },
  { "-width",   ".width",     XrmoptionSepArg, 0 },
  { "-height",  ".height",    XrmoptionSepArg, 0 },
  { "-spin",    ".spin",      XrmoptionSepArg, 0 },
  { "+spin",    ".spin",      XrmoptionNoArg, "" },
  { "-wander",  ".wander",    XrmoptionNoArg, "True" },
  { "+wander",  ".wander",    XrmoptionNoArg, "False" },
  { "-front",   ".faceFront", XrmoptionNoArg, "True" },
  { "+front",   ".faceFront", XrmoptionNoArg, "False" },
  { "-mode",    ".mode",      XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&speed,       "speed",      "Speed",     DEF_SPEED,      t_Float},
  {&grid_width,  "width",      "Width",     DEF_WIDTH,      t_Int},
  {&grid_height, "height",     "Height",    DEF_HEIGHT,     t_Int},
  {&do_spin,      "spin",      "Spin",      DEF_SPIN,       t_String},
  {&do_wander,    "wander",    "Wander",    DEF_WANDER,     t_Bool},
  {&face_front_p, "faceFront", "FaceFront", DEF_FACE_FRONT, t_Bool},
  {&mode_str,     "mode",       "Mode",     DEF_MODE,       t_String},
};

ENTRYPOINT ModeSpecOpt splitflap_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
ENTRYPOINT void
reshape_splitflap (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (40.0, 1/h, 0.5, 25);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0, 0, 3,  /* 10x lower than traditional, for better depth rez */
             0, 0, 0,
             0, 1, 0);

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (h, h, h);
  }
# endif

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
splitflap_handle_event (ModeInfo *mi, XEvent *event)
{
  splitflap_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;

  return False;
}


static void
init_textures (ModeInfo *mi)
{
  splitflap_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  const char * const *spool = latin1_spool;
  int max = countof(latin1_spool);

  bp->texinfo = (texinfo *) calloc (max+1, sizeof(*bp->texinfo));
  texture_string_metrics (bp->font_data, "", 0, &bp->ascent, &bp->descent);

  for (i = 0; i < max; i++)
    {
      texinfo *ti = &bp->texinfo[i];
      glGenTextures (1, &ti->texid);
      glBindTexture (GL_TEXTURE_2D, ti->texid);

      ti->text = spool[i];

      /* fprintf(stderr, "%d \\%03o\\%03o %s\n", i,
              (unsigned char) ti->text[0],
              (unsigned char) ti->text[1],
              ti->text); */

      string_to_texture (bp->font_data, ti->text, &ti->metrics,
                         &ti->tex_width, &ti->tex_height);
    }
  bp->texinfo_size = i;

  glBindTexture (GL_TEXTURE_2D, 0);
}


static void
parse_color (ModeInfo *mi, char *key, GLfloat color[4])
{
  XColor xcolor;
  char *string = get_string_resource (mi->dpy, key, "Color");
  if (!XParseColor (mi->dpy, mi->xgwa.colormap, string, &xcolor))
    {
      fprintf (stderr, "%s: unparsable color in %s: %s\n", progname,
               key, string);
      exit (1);
    }
  free (string);

  color[0] = xcolor.red   / 65536.0;
  color[1] = xcolor.green / 65536.0;
  color[2] = xcolor.blue  / 65536.0;
  color[3] = 1;
}


static int draw_outer_frame (ModeInfo *mi);

ENTRYPOINT void 
init_splitflap (ModeInfo *mi)
{
  splitflap_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;
  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];
  bp->glx_context = init_GL(mi);
  reshape_splitflap (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  bp->first_time_p = True;

  if (!mode_str || !*mode_str || !strcasecmp(mode_str, "text"))
    {
      bp->clock_p = 0;
    }
  else if (!strcasecmp (mode_str, "clock") ||
           !strcasecmp (mode_str, "clock12"))
    {
      bp->clock_p = 12;
      grid_width  = 8;
      grid_height = 1;
    }
  else if (!strcasecmp (mode_str, "clock24"))
    {
      bp->clock_p = 24;
      grid_width  = 6;
      grid_height = 1;
    }
  else
    {
      fprintf (stderr,
           "%s: `mode' must be text, clock12 or clock24: not `%s'\n",
               progname, mode_str);
      exit (1);
    }

  if (! bp->clock_p)
    {
      bp->tc = textclient_open (MI_DISPLAY (mi));
      bp->text[0] = 0;

      if (grid_width > 10)
        textclient_reshape (bp->tc, 
                            grid_width, grid_height,
                            grid_width, grid_height,
                            0);
    }

  if (bp->clock_p)
    speed /= 4;

  {
    double spin_speed   = 0.5;
    double wander_speed = 0.005;
    double tilt_speed   = 0.001;
    double spin_accel   = 0.5;

    char *s = do_spin;
    while (*s)
      {
        if      (*s == 'x' || *s == 'X') bp->spinx = True;
        else if (*s == 'y' || *s == 'Y') bp->spiny = True;
        else if (*s == 'z' || *s == 'Z') bp->spinz = True;
        else if (*s == '0') ;
        else
          {
            fprintf (stderr,
         "%s: spin must contain only the characters X, Y, or Z (not \"%s\")\n",
                     progname, do_spin);
            exit (1);
          }
        s++;
      }

    bp->rot = make_rotator (bp->spinx ? spin_speed : 0,
                            bp->spiny ? spin_speed : 0,
                            bp->spinz ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            False);
    bp->rot2 = (face_front_p
                ? make_rotator (0, 0, 0, 0, tilt_speed, True)
                : 0);
    bp->trackball = gltrackball_init (False);
  }

  bp->dlists = (GLuint *) calloc (countof(all_objs)+1, sizeof(GLuint));
  for (i = 0; i < countof(all_objs); i++)
    bp->dlists[i] = glGenLists (1);

  parse_color (mi, "textColor", bp->text_color);
  for (i = 0; i < countof(all_objs); i++)
    {
      const struct gllist *gll = *all_objs[i];
      char *key = 0;
      GLfloat spec[4] = {0.4, 0.4, 0.4, 1.0};
      GLfloat shiny = 80; /* 0-128 */

      glNewList (bp->dlists[i], GL_COMPILE);

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glMatrixMode(GL_TEXTURE);
      glPushMatrix();
      glMatrixMode(GL_MODELVIEW);

      glRotatef (-90, 1, 0, 0);

      glBindTexture (GL_TEXTURE_2D, 0);

      switch (i) {
      case SPLITFLAP_QUARTER_FRAME:
        key = "frameColor";
        break;
      case SPLITFLAP_OUTER_FRAME:
        key = "caseColor";
        break;
      case SPLITFLAP_DISC_QUARTER:
        key = (wire ? "frameColor" : "discColor");
        break;
      case SPLITFLAP_FIN_EDGE_HALF:
      case SPLITFLAP_FIN_FACE_HALF:
        key = "finColor";
        break;
      default:
        abort();
      }

      parse_color (mi, key, bp->component_colors[i]);

      if (wire && i == SPLITFLAP_FIN_EDGE_HALF)
        bp->component_colors[i][0] = 
        bp->component_colors[i][1] = 
        bp->component_colors[i][2] = 0.7;

      glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  spec);
      glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, shiny);

      switch (i) {
      case SPLITFLAP_OUTER_FRAME:
        if (! splitflap_obj_outer_frame)
          splitflap_obj_outer_frame =
            (struct gllist *) calloc (1, sizeof(*splitflap_obj_outer_frame));
        splitflap_obj_outer_frame->points = draw_outer_frame(mi);
        break;
      default:
        renderList (gll, wire);
        break;
      }

      glMatrixMode(GL_TEXTURE);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();

      glEndList ();
    }

  if (grid_width < 1)  grid_width  = 1;
  if (grid_height < 1) grid_height = 1;
  bp->flappers = (flapper *) calloc (grid_width * grid_height,
                                     sizeof (flapper));

  for (i = 0; i < grid_width * grid_height; i++)
    {
      flapper *f = &bp->flappers[i];

      if (!bp->clock_p)
        {
          f->spool = ascii_spool;
          f->spool_size = countof (ascii_spool);
        }
      else
        {
          switch (i) {
          case 0:
            if (bp->clock_p == 12)
              {
                f->spool = digit_s1_spool;
                f->spool_size = countof (digit_s1_spool);
              }
            else
              {
                f->spool = digit_01_spool;
                f->spool_size = countof (digit_01_spool);
              }
            break;
          case 1: case 3: case 5:
            f->spool = digit_spool;
            f->spool_size = countof (digit_spool);
            break;
          case 2: case 4:
            f->spool = digit_05_spool;
            f->spool_size = countof (digit_05_spool);
            break;
          case 6:
            f->spool = ap_spool;
            f->spool_size = countof (ap_spool);
            break;
          case 7:
            f->spool = m_spool;
            f->spool_size = countof (m_spool);
            break;
          default:
            abort();
          }
        }

      f->target_index = random() % f->spool_size;
      /* f->target_index = 0; */
      f->current_index = f->target_index;
      f->missing = (((random() % 10) == 0)
                    ? (random() % f->spool_size)
                    : -1);
    }

  bp->font_data = load_texture_font (mi->dpy, "flapFont");
  init_textures (mi);

  reshape_splitflap (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}


static int
draw_component (ModeInfo *mi, int i)
{
  splitflap_configuration *bp = &bps[MI_SCREEN(mi)];
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE,
                bp->component_colors[i]);
  glCallList (bp->dlists[i]);
  return (*all_objs[i])->points / 3;
}


static int
draw_frame_quarter (ModeInfo *mi, flapper *f)
{
  int count = 0;
  glPushMatrix();
  count += draw_component (mi, SPLITFLAP_QUARTER_FRAME);
  glPopMatrix();
  return count;
}

static int
draw_disc_quarter (ModeInfo *mi, flapper *f)
{
  int count = 0;
  glPushMatrix();
  count += draw_component (mi, SPLITFLAP_DISC_QUARTER);
  glPopMatrix();
  return count;
}

static int
draw_fin_edge_half (ModeInfo *mi, flapper *f)
{
  int count = 0;
  glPushMatrix();
  count += draw_component (mi, SPLITFLAP_FIN_EDGE_HALF);
  glPopMatrix();
  return count;
}

static int
draw_fin_face_half (ModeInfo *mi, flapper *f)
{
  int count = 0;
  if (MI_IS_WIREFRAME(mi)) return 0;
  glPushMatrix();
  count += draw_component (mi, SPLITFLAP_FIN_FACE_HALF);
  glPopMatrix();
  return count;
}


static int
draw_frame (ModeInfo *mi, flapper *f)
{
  int count = 0;

  glPushMatrix();

  glFrontFace (GL_CCW);
  count += draw_frame_quarter (mi, f);
  count += draw_disc_quarter (mi, f);

  glScalef (-1, 1, 1);
  glFrontFace (GL_CW);
  count += draw_frame_quarter (mi, f);
  count += draw_disc_quarter (mi, f);

  glScalef ( 1, -1, 1);
  glFrontFace (GL_CCW);
  count += draw_frame_quarter (mi, f);
  count += draw_disc_quarter (mi, f);

  glScalef (-1, 1, 1);
  glFrontFace (GL_CW);
  count += draw_frame_quarter (mi, f);
  count += draw_disc_quarter (mi, f);

  glPopMatrix();
  return count;
}


static void
draw_fin_text_quad (ModeInfo *mi, flapper *f, int index, Bool top_p)
{
  int wire = MI_IS_WIREFRAME(mi);
  splitflap_configuration *bp = &bps[MI_SCREEN(mi)];

  /* 15  / is weird
     27  ; descends too far
     32  @ is too wide
     59  [ descends too far
     79  A^ is taller than the font
     89  I` is weird
   */

  GLfloat z = 0.035;	/* Lifted off the surface by this distance */
  GLfloat bot = 0.013;	/* Distance away from the mid gutter */
  GLfloat scale = 1.8;  /* Scale to fill the panel */

  int lh = bp->ascent + bp->descent;
  texinfo *ti;
  GLfloat qx0, qy0, qx1, qy1;
  GLfloat tx0, ty0, tx1, ty1;
  XCharStruct overall;
  int tex_width, tex_height;
  int i;

  if (bp->texinfo_size <= 0) abort();
  for (i = 0; i < bp->texinfo_size; i++)
    {
      ti = &bp->texinfo[i];
      if (!strcmp (f->spool[index], ti->text))
        break;
    }
  if (i >= bp->texinfo_size) abort();

  overall = ti->metrics;
  tex_width  = ti->tex_width;
  tex_height = ti->tex_height;

  if (bp->ascent < overall.ascent)
    /* WTF! &Aacute; has a higher ascent than the font itself!
       Scale it down so that it doesn't overlap the fin. */
    scale *= bp->ascent / (GLfloat) overall.ascent * 0.98;

  glPushMatrix();

  glNormal3f (0, 0, 1);
  glFrontFace (top_p ? GL_CCW : GL_CW);

  if (! wire)
    {
      glBindTexture (GL_TEXTURE_2D, ti->texid);
      enable_texture_string_parameters (bp->font_data);
    }

  glTranslatef (0, 0, z);		/* Move to just above the surface */
  glScalef (1.0 / lh, 1.0 / lh, 1);	/* Scale to font pixel coordinates */
  glScalef (scale, scale, 1);		/* Fill the panel with the font */

  if (!top_p)
    {
      glRotatef (180, 0, 0, 1);
    }

  /* Position the XCharStruct origin at 0,0 in the scene. */
  qx0 = -overall.lbearing;
  qy0 = -overall.descent;
  qx1 =  overall.rbearing;
  qy1 =  overall.ascent;

  /* Center horizontally. */
  qx0 -= (overall.rbearing - overall.lbearing) / 2.0;
  qx1 -= (overall.rbearing - overall.lbearing) / 2.0;


  /* Move origin to below font descenders. */
  qy0 += bp->descent;
  qy1 += bp->descent;

  /* Center vertically. */
  qy0 -= (bp->ascent + bp->descent) / 2.0;
  qy1 -= (bp->ascent + bp->descent) / 2.0;

  /* Move the descenders down a bit, if there's room.
     This means that weirdos like [ and $ might not be on the baseline.
     #### This looks good with X11 fonts but bad with MacOS fonts.  WTF?
   */
#ifndef HAVE_COCOA
  {
    GLfloat off = bp->descent / 3.0;
    GLfloat max = bp->descent - off;
    if (overall.descent > max)
      off = max - overall.descent;
    if (off < 0)
      off = 0;
    qy0 -= off;
    qy1 -= off;
  }
# endif /* !HAVE_COCOA */

  /* Attach the texture to the quad. */
  tx0 = 0;
  ty1 = 0;
  tx1 = (overall.rbearing - overall.lbearing) / (GLfloat) tex_width;
  ty0 = (overall.ascent   + overall.descent)  / (GLfloat) tex_height;

  /* Convert from font ascent/descent to character ascent/descent. */

  /* Flip texture horizontally on bottom panel. */
  if (!top_p)
    {
      GLfloat s = tx0;
      tx0 = tx1;
      tx1 = s;
    }


  /* Cut the character in half, truncating just above the split line. */
  {
    GLfloat oqy0 = qy0;
    GLfloat oqy1 = qy1;
    GLfloat r0, r1;

    bot *= lh * scale;

    if (top_p)
      {
        if (qy0 < bot)
          qy0 = bot;
      }
    else
      {
        if (qy1 > -bot)
          qy1 = -bot;
      }

    r0 = (qy0 - oqy0) / (oqy1 - oqy0);
    r1 = (qy1 - oqy1) / (oqy1 - oqy0);
    ty0 -= r0 * (ty0 - ty1);
    ty1 -= r1 * (ty0 - ty1);
  }

  glColor4fv (bp->text_color);
  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
  glTexCoord2f (tx0, ty0); glVertex3f (qx0, qy0, 0);
  glTexCoord2f (tx1, ty0); glVertex3f (qx1, qy0, 0);
  glTexCoord2f (tx1, ty1); glVertex3f (qx1, qy1, 0);
  glTexCoord2f (tx0, ty1); glVertex3f (qx0, qy1, 0);
  glEnd();

  glPopMatrix();

  if (! wire)
    {
      glDisable (GL_BLEND);
      glEnable (GL_LIGHTING);
      glDisable (GL_TEXTURE_2D);
    }
}


static int
draw_fin (ModeInfo *mi, flapper *f, int front_index, int back_index,
          Bool text_p)
{
  int count = 0;

  glPushMatrix();

  glFrontFace (GL_CCW);

  if (! text_p)
    count += draw_fin_edge_half (mi, f);

  if (front_index >= 0)
    {
      if (text_p)
        {
          draw_fin_text_quad (mi, f, front_index, True);
          count++;
        }
      else
        count += draw_fin_face_half (mi, f);
    }

  glScalef (-1, 1, 1);
  if (! text_p)
    {
      glFrontFace (GL_CW);
      count += draw_fin_edge_half (mi, f);
      if (front_index >= 0)
        count += draw_fin_face_half (mi, f);
    }

  if (back_index >= 0)
    {
      glRotatef (180, 0, 1, 0);
      if (text_p)
        {
          draw_fin_text_quad (mi, f, back_index, False);
          count++;
        }
      else
        {
          count += draw_fin_face_half (mi, f);
          glScalef (-1, 1, 1);
          glFrontFace (GL_CCW);
          count += draw_fin_face_half (mi, f);
        }
    }

  glPopMatrix();
  return count;
}


/* The case holding the grid of flappers.
 */
static int
draw_outer_frame (ModeInfo *mi)
{
  splitflap_configuration *bp = &bps[MI_SCREEN(mi)];
  int count = 0;
  GLfloat w = grid_width;
  GLfloat h = grid_height;
  GLfloat d = 1;

  if (bp->clock_p == 12)
    w += COLON_WIDTH * 3;
  else if (bp->clock_p == 24)
    w += COLON_WIDTH * 2;

  w += 0.2;
  h += 0.2;

  if (bp->clock_p) w += 0.25;
  if (w > 3) w += 0.5;
  if (h > 3) h += 0.5;

  if (MI_IS_WIREFRAME(mi))
    return 0;

  glFrontFace (GL_CCW);
  glPushMatrix();
  glTranslatef (0, 1.03, 0);

  glBegin (GL_QUADS);

  glNormal3f ( 0,  1,  0);	/* back */
  glVertex3f (-w,  d,  h);
  glVertex3f ( w,  d,  h);
  glVertex3f ( w,  d, -h);
  glVertex3f (-w,  d, -h);
  count++;

  glNormal3f ( 0, -1,  0);	/* front */
  glVertex3f (-w, -d, -h);
  glVertex3f ( w, -d, -h);
  glVertex3f ( w, -d,  h);
  glVertex3f (-w, -d,  h);
  count++;

  glNormal3f ( 0, 0,   1);	/* top */
  glVertex3f (-w, -d,  h);
  glVertex3f ( w, -d,  h);
  glVertex3f ( w,  d,  h);
  glVertex3f (-w,  d,  h);
  count++;

  glNormal3f ( 0,  0, -1);	/* bottom */
  glVertex3f (-w,  d, -h);
  glVertex3f ( w,  d, -h);
  glVertex3f ( w, -d, -h);
  glVertex3f (-w, -d, -h);
  count++;

  glNormal3f ( 1,  0,  0);	/* left */
  glVertex3f ( w, -d,  h);
  glVertex3f ( w, -d, -h);
  glVertex3f ( w,  d, -h);
  glVertex3f ( w,  d,  h);
  count++;

  glNormal3f (-1,  0,  0);	/* right */
  glVertex3f (-w, -d, -h);
  glVertex3f (-w, -d,  h);
  glVertex3f (-w,  d,  h);
  glVertex3f (-w,  d, -h);
  count++;

  glEnd();
  glPopMatrix();

  return count;
}


static void
tick_flapper (ModeInfo *mi, flapper *f)
{
  splitflap_configuration *bp = &bps[MI_SCREEN(mi)];
  double prev = f->current_index;
  Bool wrapped_p = False;

  if (bp->button_down_p) return;
  if (f->current_index == f->target_index)
    return;

  f->current_index += speed * 0.35;		/* turn the crank */

  while (f->current_index > f->spool_size)
    {
      f->current_index -= f->spool_size;
      wrapped_p = True;
    }

  if (f->current_index < 0) abort();

  if ((prev < f->target_index || wrapped_p) &&
      f->current_index > f->target_index)	/* just overshot */
    f->current_index = f->target_index;
}


#define MOD(M,N) (((M)+(N)) % (N))  /* Works with negatives */

static int
draw_flapper (ModeInfo *mi, flapper *f, Bool text_p)
{
  int prev_index = floor (f->current_index);
  int next_index = MOD (prev_index+1, f->spool_size);
  int count = 0;
  GLfloat epsilon = 0.02;
  GLfloat r = f->current_index - prev_index;
  Bool moving_p = (r > 0 && r < 1);
  GLfloat sticky = f->sticky;

  if (f->missing >= 0)
    sticky = 0;

  if (f->missing >= 0 &&
      MOD (prev_index, f->spool_size) == f->missing)
    {
      moving_p = False;
      sticky = 0;
    }

  if (!moving_p)
    next_index = prev_index;

  if (! text_p)
    count += draw_frame (mi, f);

  /* Top flap, flat: top half of target char */
  if (!moving_p || !text_p || r > epsilon)
    {
      int p2 = next_index;

      if (p2 == f->missing)
        p2 = MOD (p2+1, f->spool_size);

      count += draw_fin (mi, f, p2, -1, text_p);
    }

  /* Bottom flap, flat: bottom half of prev char */
  if (!moving_p || !text_p || r < 1 - epsilon)
    {
      int p2 = prev_index;

      if (!moving_p && sticky)
        p2 = MOD (p2-1, f->spool_size);

      if (f->missing >= 0 &&
          p2 == MOD (f->missing+1, f->spool_size))
        p2 = MOD (p2-1, f->spool_size);

      glPushMatrix();
      glRotatef (180, 1, 0, 0);
      count += draw_fin (mi, f, -1, p2, text_p);
      glPopMatrix();
    }

  /* Moving flap, front: top half of prev char */
  /* Moving flap, back: bottom half of target char */
  if (moving_p || sticky)
    {
      if (!moving_p)
        r = 1.0;
      if (sticky && r > 1 - sticky)
        r = 1 - sticky;
      glPushMatrix();
      glRotatef (r * 180, 1, 0, 0);
      count += draw_fin (mi, f, prev_index, next_index, text_p);
      glPopMatrix();
    }

  return count;
}


static int
draw_colon (ModeInfo *mi)
{
  splitflap_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat s = 1.0 / (bp->ascent + bp->descent);
  GLfloat z = 0.01;
  int count = 0;
  XCharStruct m;

  texture_string_metrics (bp->font_data, ":", &m, 0, 0);

  s *= 2;

  glPushMatrix();

  glTranslatef (-(1 + COLON_WIDTH), 0, 0);
  glScalef (s, s, 1);

  glTranslatef (-m.lbearing - (m.rbearing - m.lbearing)/2,
                -(m.ascent + m.descent) / 2,
                0);

  glEnable (GL_TEXTURE_2D);

  /* draw the text five times, to give it a border. */
  {
    const XPoint offsets[] = {{ -1, -1 },
                              { -1,  1 },
                              {  1,  1 },
                              {  1, -1 },
                              {  0,  0 }};
    int i;
    GLfloat n = 1.5;

    glColor3f (0, 0, 0);
    for (i = 0; i < countof(offsets); i++)
      {
        glPushMatrix();
        if (offsets[i].x == 0)
          {
            glColor4fv (bp->text_color);
            glTranslatef (0, 0, z * 2);
          }
        glTranslatef (n * offsets[i].x, n * offsets[i].y, 0);
        print_texture_string (bp->font_data, ":");
        count++;
        glPopMatrix();
      }
  }

  glPopMatrix();

  return count;
}


/* Reads and returns a single Unicode character from the text client.
 */
static unsigned long
read_unicode (ModeInfo *mi)
{
  splitflap_configuration *bp = &bps[MI_SCREEN(mi)];
  const unsigned char *end = bp->text + sizeof(bp->text) - 1;  /* 4 bytes */
  unsigned long uc = 0;
  long L;
  int i;

  if (bp->clock_p || !bp->tc) abort();

  /* Fill the buffer with available input.
   */
  i = strlen ((char *) bp->text);
  while (i < (end - bp->text))
    {
      int c = textclient_getc (bp->tc);
      if (c <= 0) break;
      bp->text[i++] = (char) c;
      bp->text[i] = 0;
    }

  /* Pop 1-4 bytes from the front of the buffer and extract a UTF8 character.
   */
  L = utf8_decode (bp->text, i, &uc);
  if (L)
    {
      int j = end - bp->text - L;
      memmove (bp->text, bp->text + L, j);
      bp->text[j] = 0;
    }
  else
    uc = 0;

  return uc;
}


/* Given a Unicode character, finds the corresponding index on the spool,
   if any. Returns 0 if not found.
 */
static int
find_index (ModeInfo *mi, flapper *f, long uc)
{
  char string[5];
  int L = utf8_encode (uc, string, sizeof(string) - 1);
  int i;
  if (L <= 0) return 0;
  string[L] = 0;
  for (i = 0; i < f->spool_size; i++)
    {
      if (!strcmp (string, f->spool[i]))
        return i;
    }
  return 0;
}


/* Read input from the text client and populate the spool with it.
 */
static void
fill_targets (ModeInfo *mi)
{
  splitflap_configuration *bp = &bps[MI_SCREEN(mi)];
  int x, y;
  Bool cls_p = False;

  if (bp->clock_p)
    {
      char buf[80];
      time_t now = time ((time_t *) 0);
        struct tm *tm = localtime (&now);
      const char *fmt = (bp->clock_p == 24
                         ? "%H%M%S"
                         : "%I%M%S%p");
      int i;
      strftime (buf, sizeof(buf)-1, fmt, tm);
      if (bp->clock_p == 12 && buf[0] == '0')
        buf[0] = ' ';

      for (i = 0; i < strlen(buf); i++)
        {
          flapper *f = &bp->flappers[i];
          f->target_index = find_index (mi, f, buf[i]);
        }
      for (; i < grid_width * grid_height; i++)
        {
          flapper *f = &bp->flappers[i];
          f->target_index = find_index (mi, f, ' ');
        }
      return;
    }

  for (y = 0; y < grid_height; y++)
    {
      Bool nl_p = False;
      for (x = 0; x < grid_width; x++)
        {
          int i = y * grid_width + x;
          flapper *f = &bp->flappers[i];
          unsigned long uc = ((nl_p || cls_p) ? ' ' : read_unicode (mi));
          if (uc == '\r' || uc == '\n')
            nl_p = True;
          else if (uc == 12)  /* ^L */
            cls_p = True;

          /* Convert Unicode to the closest Latin1 equivalent. */
          if (uc > 127)
            {
              Bool ascii_p = (f->spool != latin1_spool);
              unsigned char s[5], *s2;
              int L = utf8_encode (uc, (char *) s, sizeof(s));
              s[L] = 0;
              s2 = (unsigned char *) utf8_to_latin1 ((char *) s, ascii_p);
              
              if (s2[0] < 128)	/* ASCII */
                uc = s2[0];
              else		/* Latin1 -> UTF8 -> Unicode */
                {
                  s[0] = (s2[0] > 0xBF ? 0xC3 : 0xC2);
                  s[1] = s2[0] & (s2[0] > 0xBF ? 0xBF : 0xFF);
                  s[2] = 0;
                  utf8_decode (s, 2, &uc);
                }

              free (s2);
            }

          /* Upcase ASCII. Upcasing Unicrud would be rocket surgery. */
          if (uc >= 'a' && uc <= 'z') uc += ('A'-'a');

          f->target_index = find_index (mi, f, uc);

          f->sticky = (((random() % 20) == 0)
                       ? 0.05 + frand(0.1) + frand(0.1)
                       : 0);
        }
    }

# if 0
  for (y = 0; y < grid_height; y++)
    {
      fprintf (stderr, "# ");
      for (x = 0; x < grid_width; x++)
        {
          int i = y * grid_width + x;
          flapper *f = &bp->flappers[i];
          fprintf(stderr, "%s", bp->spool[f->target_index]);
        }
      fprintf (stderr, " #\n");
    }
  fprintf (stderr, "\n");
# endif
}


static void
draw_flappers (ModeInfo *mi, Bool text_p)
{
  splitflap_configuration *bp = &bps[MI_SCREEN(mi)];
  int x, y;
  int running = 0;

  for (y = 0; y < grid_height; y++)
    for (x = 0; x < grid_width; x++)
      {
        int i = (grid_height - y - 1) * grid_width + x;
        flapper *f = &bp->flappers[i];
        GLfloat xx = x;
        GLfloat yy = y;

        if (bp->clock_p)
          {
            if (x >= 2) xx += COLON_WIDTH;
            if (x >= 4) xx += COLON_WIDTH;
            if (x >= 6) xx += COLON_WIDTH;
          }

        xx *= 2.01;
        yy *= 1.98;

        glPushMatrix();
        glTranslatef (xx, yy, 0);
        mi->polygon_count += draw_flapper (mi, f, text_p);

        if (text_p && bp->clock_p && (x == 2 || x == 4))
          mi->polygon_count += draw_colon (mi);

        glPopMatrix();

        if (text_p)
          {
            tick_flapper (mi, f);
            if (f->current_index != f->target_index)
              running++;
          }
      }

  if (text_p && !running)
    {
      if (bp->clock_p)
        fill_targets (mi);
      else if (bp->linger)
        {
          bp->linger--;
          if (!bp->linger)
            fill_targets (mi);
        }
      else
        {
          /* Base of 1 second, plus 1 second for every 25 characters.
             Also multiply by speed? */
          bp->linger = 30;
          if (!bp->first_time_p)
            bp->linger += (grid_width * grid_height * 1.2);
          bp->first_time_p = False;
        }
    }
}


ENTRYPOINT void
draw_splitflap (ModeInfo *mi)
{
  splitflap_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  if (!wire)
    {
      GLfloat pos[4] = {0.4, 0.2, 0.4, 0.0};
/*      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};*/
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

      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }


  glPushMatrix ();
  glRotatef(current_device_rotation(), 0, 0, 1);

  glScalef (0.1, 0.1, 0.1);  /* because of gluLookAt */

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 8,
                 (z - 0.5) * 8);

    gltrackball_rotate (bp->trackball);

    if (face_front_p)
      {
        double maxx = 120;
        double maxy = 60;
        double maxz = 45;
        get_position (bp->rot2, &x, &y, &z, !bp->button_down_p);
        if (bp->spinx) glRotatef (maxy/2 - x*maxy, 1, 0, 0);
        if (bp->spiny) glRotatef (maxx/2 - y*maxx, 0, 1, 0);
        if (bp->spinz) glRotatef (maxz/2 - z*maxz, 0, 0, 1);
      }
    else
      {
        get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
        glRotatef (x * 360, 1, 0, 0);
        glRotatef (y * 360, 0, 1, 0);
        glRotatef (z * 360, 0, 0, 1);
      }
  }

  /* Fit the whole grid on the screen */
  {
    GLfloat r = MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi);
    int cells = (grid_width > grid_height
                 ? grid_width * r
                 : grid_height);
    GLfloat s = 8;
# ifdef HAVE_MOBILE
    s *= 2; /* #### What. Why is this necessary? */
#endif
    s /= cells;
    glScalef (s, s, s);
  }

  mi->polygon_count = 0;
  mi->polygon_count += draw_component (mi, SPLITFLAP_OUTER_FRAME);

  {
    GLfloat xoff = (bp->clock_p == 12 ? COLON_WIDTH * 3 :
                    bp->clock_p == 24 ? COLON_WIDTH * 2 :
                    0);
    glTranslatef (1 - (grid_width + xoff), 1 - grid_height, 0);
  }

  /* We must render all text after all polygons, or alpha blending
     doesn't work right. */
  draw_flappers (mi, False);
  draw_flappers (mi, True);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

ENTRYPOINT void
free_splitflap (ModeInfo *mi)
{
  splitflap_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;

  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  if (bp->flappers) free (bp->flappers);
  if (bp->tc) textclient_close (bp->tc);
  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->rot2) free_rotator (bp->rot2);
  if (bp->font_data) free_texture_font (bp->font_data);
  if (bp->dlists) {
    for (i = 0; i < countof(all_objs); i++)
      if (glIsList(bp->dlists[i])) glDeleteLists(bp->dlists[i], 1);
    free (bp->dlists);
  }
  if (bp->texinfo) {
    for (i = 0; i < bp->texinfo_size; i++)
      if (bp->texinfo[i].texid) glDeleteTextures (1, &bp->texinfo[i].texid);
    free (bp->texinfo);
  }
}

XSCREENSAVER_MODULE ("SplitFlap", splitflap)

#endif /* USE_GL */
