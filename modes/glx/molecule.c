/* -*- Mode: C; tab-width: 4 -*- */
/* molecule --- 3D molecules */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)molecule.c	5.01 2001/04/12 xlockmore";
#endif

/* molecule, Copyright (c) 2001 Jamie Zawinski <jwz@jwz.org>
 * Draws molecules, based on coordinates from PDB (Protein Data Base) files.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 *
 * Revision History:
 * 19-Apr-01:  tschmidt@micron.com
 *                              Fixed most memory leaks
 * 19-Apr-01:  rolf@groppe.de   fixed some allocation problems
 *                              Startup message now only at first call
 * 12-Apr-01:  rolf@groppe.de   Ported this mode from xscreensaver to xlock.
 *                              Use of parameter -cycles instead of timeout, 
 *                              when used as xlock module.
 *                              Some minor changes for better display in
 *                              iconified state.
 */


/* Documentation on the PDB file format:
   http://www.rcsb.org/pdb/docs/format/pdbguide2.2/guide2.2_frame.html

   Good source of PDB files:
   http://www.sci.ouc.bc.ca/chem/molecule/molecule.html

   TO DO:

     - I'm not sure the text labels are being done in the best way;
       they are sometimes, but not always, occluded by spheres that
       pass in front of them. 
 */

#ifdef VMS
#include "vms_x_fix.h"
#endif
#include <X11/Intrinsic.h>

#define DEF_TIMEOUT     "20"
#define DEF_SPIN        "XYZ"
#define DEF_WANDER      "False"
#define DEF_LABELS      "True"
#define DEF_TITLES      "True"
#define DEF_ATOMS       "True"
#define DEF_BONDS       "True"
#define DEF_BBOX        "False"
#define DEF_MOLECULE    "(default)"


#ifdef STANDALONE

#define PROGCLASS	"Molecule"
#define HACK_INIT	init_molecule
#define HACK_DRAW	draw_molecule
#define HACK_RESHAPE	reshape_molecule
#define molecule_opts	xlockmore_opts

#define DEFAULTS	"*delay:	10000         \n" \
			"*timeout:    " DEF_TIMEOUT  "\n" \
			"*showFPS:      False         \n" \
			"*wireframe:    False         \n" \
			"*molecule:   " DEF_MOLECULE "\n" \
			"*spin:       " DEF_SPIN     "\n" \
			"*wander:     " DEF_WANDER   "\n" \
			"*labels:     " DEF_LABELS   "\n" \
			"*atoms:      " DEF_ATOMS    "\n" \
			"*bonds:      " DEF_BONDS    "\n" \
			"*bbox:       " DEF_BBOX     "\n" \
			"*atomFont:   -*-times-bold-r-normal-*-240-*\n" \
			"*titleFont:  -*-times-bold-r-normal-*-180-*\n" \
			"*noLabelThreshold:    30     \n" \
			"*wireframeThreshold:  150    \n" \

#include "xlockmore.h"
#include "colors.h"
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
# include "visgl.h"
#include "color.h"
#endif /* !STANDALONE */
#include "sphere.h"
#include "tube.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#ifdef MODE_molecule /* whole file */

#include <ctype.h>
#include <GL/glu.h>
#include <time.h>

#define SPHERE_SLICES 16  /* how densely to render spheres */
#define SPHERE_STACKS 10

#define SMOOTH_TUBE       /* whether to have smooth or faceted tubes */

#ifdef SMOOTH_TUBE
# define TUBE_FACES  12   /* how densely to render tubes */
#else
# define TUBE_FACES  8
#endif

#define FLOATRAND(a) (((double)LRAND() / (double)MAXRAND) * a)

static int scale_down;
#define SPHERE_SLICES_2  7
#define SPHERE_STACKS_2  4
#define TUBE_FACES_2     3


const char * const builtin_pdb_data[] = {
# include "molecules.h"
};


typedef struct {
  const char *name;
  GLfloat size, size2;
  const char *color;
  const char *text_color;
  GLfloat gl_color[8];
} atom_data;


/* These are the traditional colors used to render these atoms,
   and their approximate size in angstroms.
 */
static atom_data all_atom_data[] = {
  { "H",    1.17,  0,  "White",           "Grey60",        { 0, }},
  { "C",    1.75,  0,  "Grey50",          "White",         { 0, }},
  { "N",    1.55,  0,  "RoyalBlue1",      "NavyBlue",      { 0, }},
  { "O",    1.40,  0,  "Red",             "LightPink",     { 0, }},
  { "P",    1.28,  0,  "MediumPurple",    "PaleVioletRed", { 0, }},
  { "S",    1.80,  0,  "Yellow4",         "Yellow1",       { 0, }},
  { "bond", 0,     0,  "Grey70",          "Yellow1",       { 0, }},
  { "*",    1.40,  0,  "Green4",          "PaleGreen2",    { 0, }}
};


typedef struct {
  int id;		/* sequence number in the PDB file */
  const char *label;	/* The atom name */
  GLfloat x, y, z;	/* position in 3-space (angstroms) */
  atom_data *data;	/* computed: which style of atom this is */
} molecule_atom;

typedef struct {
  int from, to;		/* atom sequence numbers */
  int strength;		/* how many bonds are between these two atoms */
} molecule_bond;


typedef struct {
  const char *label;		/* description of this compound */
  int natoms, atoms_size;
  int nbonds, bonds_size;
  molecule_atom *atoms;
  molecule_bond *bonds;
} molecule;


typedef struct {
  GLXContext *glx_context;

  GLfloat rotx, roty, rotz;	   /* current object rotation */
  GLfloat dx, dy, dz;		   /* current rotational velocity */
  GLfloat ddx, ddy, ddz;	   /* current rotational acceleration */
  GLfloat d_max;		   /* max velocity */

  Bool spin_x, spin_y, spin_z;

  GLfloat molecule_size;	   /* max dimension of molecule bounding box */

  GLfloat no_label_threshold;	   /* Things happen when molecules are huge */
  GLfloat wireframe_threshold;

  int which;			   /* which of the molecules is being shown */
  int nmolecules;
  molecule *molecules;

  GLuint molecule_dlist;
  XFontStruct *xfont1, *xfont2;
  GLuint font1_dlist, font2_dlist;

} molecule_configuration;


static molecule_configuration *mcs = NULL;
static molecule *mols = NULL;

static int timeout;
static char *molecule_str;
static char *do_spin;
static Bool do_wander;
static Bool do_titles;
static Bool do_labels;
static Bool do_atoms;
static Bool do_bonds;
static Bool do_bbox;

static Bool orig_do_labels, orig_do_titles, orig_do_bonds, cur_wire, orig_wire; /* saved to reset */
static Bool not_first_gl = False;
static Bool firstcall = True;

static XrmOptionDescRec opts[] = {
  {(char *) "-molecule", (char *) ".molecule.molecule", XrmoptionSepArg, 0 },
#ifdef STANDALONE
  {(char *) "-timeout", (char *) ".molecule.timeout",XrmoptionSepArg, 0 },
#endif
  {(char *) "-spin",   (char *) ".molecule.spin",   XrmoptionSepArg, 0 },
  {(char *) "+spin",   (char *) ".molecule.spin",   XrmoptionNoArg, "" },
  {(char *) "-wander", (char *) ".molecule.wander", XrmoptionNoArg, "on" },
  {(char *) "+wander", (char *) ".molecule.wander", XrmoptionNoArg, "off" },
  {(char *) "-labels", (char *) ".molecule.labels", XrmoptionNoArg, "on" },
  {(char *) "+labels", (char *) ".molecule.labels", XrmoptionNoArg, "off" },
  {(char *) "-titles", (char *) ".molecule.titles", XrmoptionNoArg, "on" },
  {(char *) "+titles", (char *) ".molecule.titles", XrmoptionNoArg, "off" },
  {(char *) "-atoms",  (char *) ".molecule.atoms",  XrmoptionNoArg, "on" },
  {(char *) "+atoms",  (char *) ".molecule.atoms",  XrmoptionNoArg, "off" },
  {(char *) "-bonds",  (char *) ".molecule.bonds",  XrmoptionNoArg, "on" },
  {(char *) "+bonds",  (char *) ".molecule.bonds",  XrmoptionNoArg, "off" },
  {(char *) "-bbox",   (char *) ".molecule.bbox",  XrmoptionNoArg, "on" },
  {(char *) "+bbox",   (char *) ".molecule.bbox",  XrmoptionNoArg, "off" },
};

static argtype vars[] = {
  {(caddr_t *) &molecule_str, (char *) "molecule",   (char *) "Molecule", DEF_MOLECULE,t_String},
#ifdef STANDALONE
  {(caddr_t *) &timeout,   (char *) "timeout",(char *) "Seconds",DEF_TIMEOUT,t_Int},
#endif
  {(caddr_t *) &do_spin,   (char *) "spin",   (char *) "Spin",   DEF_SPIN,   t_String},
  {(caddr_t *) &do_wander, (char *) "wander", (char *) "Wander", DEF_WANDER, t_Bool},
  {(caddr_t *) &do_labels, (char *) "labels", (char *) "Labels", DEF_LABELS, t_Bool},
  {(caddr_t *) &do_titles, (char *) "titles", (char *) "Titles", DEF_TITLES, t_Bool},
  {(caddr_t *) &do_atoms,  (char *) "atoms",  (char *) "Atoms",  DEF_ATOMS,  t_Bool},
  {(caddr_t *) &do_bonds,  (char *) "bonds",  (char *) "Bonds",  DEF_BONDS,  t_Bool},
  {(caddr_t *) &do_bbox,   (char *) "bbox",   (char *) "BBox",   DEF_BBOX,   t_Bool},
};

static OptionStruct desc[] = {
  {(char *) "-molecule", (char *) "set molecule"},
#ifdef STANDALONE
  {(char *) "-timeout", (char *) "set timeout"},
#endif
  {(char *) "-/+spin", (char *) "turn on/off spin"},
  {(char *) "-/+wander", (char *) "turn on/off wander"},
  {(char *) "-/+labels", (char *) "turn on/off labels"},
  {(char *) "-/+titles", (char *) "turn on/off titles"},
  {(char *) "-/+atoms", (char *) "turn on/off atoms"},
  {(char *) "-/+bonds", (char *) "turn on/off bonds"},
  {(char *) "-/+bbox", (char *) "turn on/off bbox"},
};

ModeSpecOpt molecule_opts = {countof(opts), opts, countof(vars), vars, desc};

#ifdef USE_MODULES
ModStruct   molecule_description =
{"molecule", "init_molecule", "draw_molecule", "release_molecule",
 "reshape_molecule", "init_molecule", NULL, &molecule_opts,
 50000, 1, 20, 1, 64, 1.0, "",
 "Draws molecules", 0, NULL};
#endif

static time_t last = 0;


/* shapes */

static void
sphere (GLfloat x, GLfloat y, GLfloat z, GLfloat diameter, Bool wire)
{
  int stacks = (scale_down ? SPHERE_STACKS_2 : SPHERE_STACKS);
  int slices = (scale_down ? SPHERE_SLICES_2 : SPHERE_SLICES);

  glPushMatrix ();
  glTranslatef (x, y, z);
  glScalef (diameter, diameter, diameter);
  unit_sphere (stacks, slices, wire);
  glPopMatrix ();
}


static void
load_font (ModeInfo *mi, char *res, XFontStruct **fontP, GLuint *dlistP)
{
#ifdef STANDALONE
  const char *font = get_string_resource (res, "Font"); 
#else
  const char *font = res;
#endif
  XFontStruct *f;
  Font id;
  int first, last;

  if (!font) font = "-*-times-bold-r-normal-*-180-*";

  f = XLoadQueryFont(MI_DISPLAY(mi), font);
  if (!f) f = XLoadQueryFont(MI_DISPLAY(mi), "fixed");

  id = f->fid;
  first = f->min_char_or_byte2;
  last = f->max_char_or_byte2;
  
  clear_gl_error ();
  *dlistP = glGenLists ((GLuint) last+1);
  check_gl_error ("glGenLists");
  /*-
   * PURIFY reports a cumulative memory leak on the next line with Mesa 3.4.1
   * on Solaris 2.X and SunOS 4.1.X. This can be fixed with a patch to Mesa
   * 3.4.1. OpenGL on Solaris does not have the memory leak.
   */
  glXUseXFont(id, first, last-first+1, *dlistP + first);
  check_gl_error ("glXUseXFont");

  *fontP = f;
}


static void
load_fonts (ModeInfo *mi)
{
  molecule_configuration *mc = &mcs[MI_SCREEN(mi)];
#ifdef STANDALONE
  load_font (mi, "atomFont",  &mc->xfont1, &mc->font1_dlist);
  load_font (mi, "titleFont", &mc->xfont2, &mc->font2_dlist);
#else
  load_font (mi, "-*-times-bold-r-normal-*-240-*",  &mc->xfont1, &mc->font1_dlist);
  load_font (mi, "-*-times-bold-r-normal-*-180-*", &mc->xfont2, &mc->font2_dlist);
#endif
}


static int
string_width (XFontStruct *f, const char *c)
{
  int w = 0;
  while (*c)
    {
      int cc = *((unsigned char *) c);
      w += (f->per_char
            ? f->per_char[cc-f->min_char_or_byte2].rbearing
            : f->min_bounds.rbearing);
      c++;
    }
  return w;
}


static atom_data *
get_atom_data (const char *atom_name)
{
  int i;
  atom_data *d = 0;
  char *n = strdup (atom_name);
  char *n2 = n;
  int L;

  while (!isalpha(*n)) n++;
  L = strlen(n);
  while (L > 0 && !isalpha(n[L-1]))
    n[--L] = 0;

  for (i = 0; i < countof(all_atom_data); i++)
    {
      d = &all_atom_data[i];
      if (!strcmp (n, all_atom_data[i].name))
        break;
    }

  free (n2);
  return d;
}


static void
set_atom_color (ModeInfo *mi, molecule_atom *a, Bool font_p)
{
  atom_data *d;
  GLfloat *gl_color;

  if (a)
    d = a->data;
  else
    {
      static atom_data *def_data = 0;
      if (!def_data) def_data = get_atom_data ("bond");
      d = def_data;
    }

  gl_color = (!font_p ? d->gl_color : (d->gl_color + 4));

  if (gl_color[3] == 0)
    {
      const char *string = !font_p ? d->color : d->text_color;
      XColor xcolor;
      if (!XParseColor (MI_DISPLAY(mi), MI_COLORMAP(mi), string, &xcolor))
        {
          fprintf (stderr, "molecule: unparsable color in %s: %s\n",
                   (a ? a->label : d->name), string);
          /* exit (1); */
        }

      gl_color[0] = xcolor.red   / 65536.0;
      gl_color[1] = xcolor.green / 65536.0;
      gl_color[2] = xcolor.blue  / 65536.0;
      gl_color[3] = 1.0;
    }
  
  if (font_p)
    glColor3f (gl_color[0], gl_color[1], gl_color[2]);
  else
    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gl_color);
}


static GLfloat
atom_size (molecule_atom *a)
{
  if (do_bonds)
    {
      if (a->data->size2 == 0)
        {
          /* let the molecules have the same relative sizes, but scale
             them to a smaller range, so that the bond-tubes are
             actually visible...
           */
          GLfloat bot = 0.4;
          GLfloat top = 0.6;
          GLfloat min = 1.17;
          GLfloat max = 1.80;
          GLfloat ratio = (a->data->size - min) / (max - min);
          a->data->size2 = bot + (ratio * (top - bot));
        }
      return a->data->size2;
    }
  else
    return a->data->size;
}


static molecule_atom *
get_atom (molecule_atom *atoms, int natoms, int id)
{
  int i;

  /* quick short-circuit */
  if (id < natoms)
    {
      if (atoms[id].id == id)
        return &atoms[id];
      if (id > 0 && atoms[id-1].id == id)
        return &atoms[id-1];
      if (id < natoms-1 && atoms[id+1].id == id)
        return &atoms[id+1];
    }

  for (i = 0; i < natoms; i++)
    if (id == atoms[i].id)
      return &atoms[i];

  fprintf (stderr, "molecule: no atom %d\n", id);
  /* abort(); */
}


static void
molecule_bounding_box (ModeInfo *mi,
                       GLfloat *x1, GLfloat *y1, GLfloat *z1,
                       GLfloat *x2, GLfloat *y2, GLfloat *z2)
{
  molecule_configuration *mc = &mcs[MI_SCREEN(mi)];
  molecule *m = &mc->molecules[mc->which];
  int i;

  if (m->natoms == 0)
    {
      *x1 = *y1 = *z1 = *x2 = *y2 = *z2 = 0;
    }
  else
    {
      *x1 = *x2 = m->atoms[0].x;
      *y1 = *y2 = m->atoms[0].y;
      *z1 = *z2 = m->atoms[0].z;
    }

  for (i = 1; i < m->natoms; i++)
    {
      if (m->atoms[i].x < *x1) *x1 = m->atoms[i].x;
      if (m->atoms[i].y < *y1) *y1 = m->atoms[i].y;
      if (m->atoms[i].z < *z1) *z1 = m->atoms[i].z;

      if (m->atoms[i].x > *x2) *x2 = m->atoms[i].x;
      if (m->atoms[i].y > *y2) *y2 = m->atoms[i].y;
      if (m->atoms[i].z > *z2) *z2 = m->atoms[i].z;
    }

  *x1 -= 1;
  *y1 -= 1;
  *z1 -= 1;
  *x2 += 1;
  *y2 += 1;
  *z2 += 1;
}


static void
draw_bounding_box (ModeInfo *mi)
{
  static GLfloat c1[4] = { 0.2, 0.2, 0.6, 1.0 };
  static GLfloat c2[4] = { 1.0, 0.0, 0.0, 1.0 };
  int wire = cur_wire;
  GLfloat x1, y1, z1, x2, y2, z2;
  molecule_bounding_box (mi, &x1, &y1, &z1, &x2, &y2, &z2);

  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, c1);
  glFrontFace(GL_CCW);

  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glNormal3f(0, 1, 0);
  glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z2);
  glVertex3f(x2, y1, z2); glVertex3f(x2, y1, z1);
  glEnd();
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glNormal3f(0, -1, 0);
  glVertex3f(x2, y2, z1); glVertex3f(x2, y2, z2);
  glVertex3f(x1, y2, z2); glVertex3f(x1, y2, z1);
  glEnd();
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glNormal3f(0, 0, 1);
  glVertex3f(x1, y1, z1); glVertex3f(x2, y1, z1);
  glVertex3f(x2, y2, z1); glVertex3f(x1, y2, z1);
  glEnd();
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glNormal3f(0, 0, -1);
  glVertex3f(x1, y2, z2); glVertex3f(x2, y2, z2);
  glVertex3f(x2, y1, z2); glVertex3f(x1, y1, z2);
  glEnd();
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glNormal3f(1, 0, 0);
  glVertex3f(x1, y2, z1); glVertex3f(x1, y2, z2);
  glVertex3f(x1, y1, z2); glVertex3f(x1, y1, z1);
  glEnd();
  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
  glNormal3f(-1, 0, 0);
  glVertex3f(x2, y1, z1); glVertex3f(x2, y1, z2);
  glVertex3f(x2, y2, z2); glVertex3f(x2, y2, z1);
  glEnd();

  glPushAttrib (GL_LIGHTING);
  glDisable (GL_LIGHTING);

  glColor3f (c2[0], c2[1], c2[2]);
  glBegin(GL_LINES);
  if (x1 > 0) x1 = 0; if (x2 < 0) x2 = 0;
  if (y1 > 0) y1 = 0; if (y2 < 0) y2 = 0;
  if (z1 > 0) z1 = 0; if (z2 < 0) z2 = 0;
  glVertex3f(x1, 0,  0);  glVertex3f(x2, 0,  0); 
  glVertex3f(0 , y1, 0);  glVertex3f(0,  y2, 0); 
  glVertex3f(0,  0,  z1); glVertex3f(0,  0,  z2); 
  glEnd();

  glPopAttrib();
}


/* Since PDB files don't always have the molecule centered around the
   origin, and since some molecules are pretty large, scale and/or
   translate so that the whole molecule is visible in the window.
 */
static void
ensure_bounding_box_visible (ModeInfo *mi)
{
  molecule_configuration *mc = &mcs[MI_SCREEN(mi)];

  GLfloat x1, y1, z1, x2, y2, z2;
  GLfloat w, h, d;
  GLfloat size;
  GLfloat max_size = 10;  /* don't bother scaling down if the molecule
                             is already smaller than this */

  molecule_bounding_box (mi, &x1, &y1, &z1, &x2, &y2, &z2);
  w = x2-x1;
  h = y2-y1;
  d = z2-z1;

  size = (w > h ? w : h);
  size = (size > d ? size : d);

  mc->molecule_size = size;

  scale_down = 0;

  if (size > max_size)
    {
      GLfloat scale = max_size / size;
      glScalef (scale, scale, scale);

      scale_down = scale < 0.3;
    }

  glTranslatef (-(x1 + w/2),
                -(y1 + h/2),
                -(z1 + d/2));
}


static void
print_title_string (ModeInfo *mi, const char *string,
                    GLfloat x, GLfloat y, GLfloat line_height)
{
  molecule_configuration *mc = &mcs[MI_SCREEN(mi)];
  
  y -= line_height;

  glPushAttrib (GL_LIGHTING | GL_DEPTH_TEST);
  glDisable (GL_LIGHTING);
  glDisable (GL_DEPTH_TEST);
  {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    {
      glLoadIdentity();

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      {
        int i;
        glLoadIdentity();

        gluOrtho2D (0, MI_WIDTH(mi), 0, MI_HEIGHT(mi));

        set_atom_color (mi, 0, True);

        glRasterPos2f (x, y);
        for (i = 0; i < strlen(string); i++)
          {
            char c = string[i];
            if (c == '\n')
              glRasterPos2f (x, (y -= line_height));
            else
              glCallList (mc->font2_dlist + (int)(c));
          }
      }
      glPopMatrix();
    }
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
  }
  glPopAttrib();

  glMatrixMode(GL_MODELVIEW);
}


/* Constructs the GL shapes of the current molecule
 */
static void
build_molecule (ModeInfo *mi)
{
  molecule_configuration *mc = &mcs[MI_SCREEN(mi)];
  int wire = cur_wire;
  int i;

  molecule *m = &mc->molecules[mc->which];

  if (wire)
    {
      glDisable(GL_CULL_FACE);
      glDisable(GL_LIGHTING);
      glDisable(GL_LIGHT0);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_NORMALIZE);
      glDisable(GL_CULL_FACE);
    }
  else
    {
      glEnable(GL_CULL_FACE);
      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_NORMALIZE);
      glEnable(GL_CULL_FACE);
    }

  if (!wire)
    set_atom_color (mi, 0, False);

  if (do_bonds)
    for (i = 0; i < m->nbonds; i++)
      {
        molecule_bond *b = &m->bonds[i];
        molecule_atom *from = get_atom (m->atoms, m->natoms, b->from);
        molecule_atom *to   = get_atom (m->atoms, m->natoms, b->to);

        if (wire)
          {
            glBegin(GL_LINES);
            glVertex3f(from->x, from->y, from->z);
            glVertex3f(to->x,   to->y,   to->z);
            glEnd();
          }
        else
          {
            int faces = (scale_down ? TUBE_FACES_2 : TUBE_FACES);
# ifdef SMOOTH_TUBE
            int smooth = True;
# else
            int smooth = False;
# endif
            GLfloat thickness = 0.07 * b->strength;
            GLfloat cap_size = 0.03;
            if (thickness > 0.3)
              thickness = 0.3;

            tube (from->x, from->y, from->z,
                  to->x,   to->y,   to->z,
                  thickness, cap_size,
                  faces, smooth, wire);
          }
      }

  for (i = 0; i < m->natoms; i++)
    {
      molecule_atom *a = &m->atoms[i];
      int i;

      if (!wire && do_atoms)
        {
          GLfloat size = atom_size (a);
          set_atom_color (mi, a, False);
          sphere (a->x, a->y, a->z, size, wire);
        }

      if (do_labels)
        {
          glPushAttrib (GL_LIGHTING | GL_DEPTH_TEST);
          glDisable (GL_LIGHTING);
          glDisable (GL_DEPTH_TEST);

          if (!wire)
            set_atom_color (mi, a, True);

          glRasterPos3f (a->x, a->y, a->z);

          {
            GLdouble mm[17], pm[17];
            GLint vp[5];
            GLdouble wx=-1, wy=-1, wz=-1;
            glGetDoublev (GL_MODELVIEW_MATRIX, mm);
            glGetDoublev (GL_PROJECTION_MATRIX, pm);
            glGetIntegerv (GL_VIEWPORT, vp);

            /* Convert 3D coordinates to window coordinates */
            gluProject (a->x, a->y, a->z, mm, pm, vp, &wx, &wy, &wz);

            /* Fudge the window coordinates to center the string */
            wx -= string_width (mc->xfont1, a->label) / 2;
            wy -= mc->xfont1->descent;

            /* Convert new window coordinates back to 3D coordinates */
            gluUnProject (wx, wy, wz, mm, pm, vp, &wx, &wy, &wz);
            glRasterPos3f (wx, wy, wz);
          }

          for (i = 0; i < strlen(a->label); i++)
            glCallList (mc->font1_dlist + (int)(a->label[i]));

          glPopAttrib();
        }
    }

  if (do_bbox)
    draw_bounding_box (mi);

  if (do_titles && m->label && *m->label)
    print_title_string (mi, m->label,
                        10, MI_HEIGHT(mi) - 10,
                        mc->xfont2->ascent + mc->xfont2->descent);
}



/* loading */

static void
push_atom (molecule *m,
           int id, const char *label,
           GLfloat x, GLfloat y, GLfloat z)
{
  m->natoms++;
  if (m->atoms_size < m->natoms)
    {
      m->atoms_size += 20;
      m->atoms = (molecule_atom *) realloc (m->atoms,
                                            m->atoms_size * sizeof(*m->atoms));
    }
  m->atoms[m->natoms-1].id = id;
  m->atoms[m->natoms-1].label = label;
  m->atoms[m->natoms-1].x = x;
  m->atoms[m->natoms-1].y = y;
  m->atoms[m->natoms-1].z = z;
  m->atoms[m->natoms-1].data = get_atom_data (label);
}


static void
push_bond (molecule *m, int from, int to)
{
  int i;

  for (i = 0; i < m->nbonds; i++)
    if ((m->bonds[i].from == from && m->bonds[i].to   == to) ||
        (m->bonds[i].to   == from && m->bonds[i].from == to))
      {
        m->bonds[i].strength++;
        return;
      }

  m->nbonds++;
  if (m->bonds_size < m->nbonds)
    {
      m->bonds_size += 20;
      m->bonds = (molecule_bond *) realloc (m->bonds,
                                            m->bonds_size * sizeof(*m->bonds));
    }
  m->bonds[m->nbonds-1].from = from;
  m->bonds[m->nbonds-1].to = to;
  m->bonds[m->nbonds-1].strength = 1;
}



/* This function is crap.
 */
static void
parse_pdb_data (molecule *m, const char *data, const char *filename, int line)
{
  const char *s = data;
  char *ss;
  while (*s)
    {
      if ((!m->label || !*m->label) &&
          (!strncmp (s, "HEADER", 6) || !strncmp (s, "COMPND", 6)))
        {
          char *name = calloc (1, 100);
          char *n2 = name;
          int L = strlen(s);
          if (L > 99) L = 99;

          strncpy (n2, s, L);
          n2 += 7;
          while (isspace(*n2)) n2++;

          ss = strchr (n2, '\n');
          if (ss) *ss = 0;
          ss = strchr (n2, '\r');
          if (ss) *ss = 0;

          ss = n2+strlen(n2)-1;
          while (isspace(*ss) && ss > n2)
            *ss-- = 0;

          if (strlen (n2) > 4 &&
              !strcmp (n2 + strlen(n2) - 4, ".pdb"))
            n2[strlen(n2)-4] = 0;

          if (m->label) free ((char *) m->label);
          m->label = strdup (n2);
          free (name);
        }
      else if (!strncmp (s, "TITLE ", 6) ||
               !strncmp (s, "HEADER", 6) ||
               !strncmp (s, "COMPND", 6) ||
               !strncmp (s, "AUTHOR", 6) ||
               !strncmp (s, "REVDAT", 6) ||
               !strncmp (s, "SOURCE", 6) ||
               !strncmp (s, "EXPDTA", 6) ||
               !strncmp (s, "JRNL  ", 6) ||
               !strncmp (s, "REMARK", 6) ||
               !strncmp (s, "SEQRES", 6) ||
               !strncmp (s, "HET   ", 6) ||
               !strncmp (s, "FORMUL", 6) ||
               !strncmp (s, "CRYST1", 6) ||
               !strncmp (s, "ORIGX1", 6) ||
               !strncmp (s, "ORIGX2", 6) ||
               !strncmp (s, "ORIGX3", 6) ||
               !strncmp (s, "SCALE1", 6) ||
               !strncmp (s, "SCALE2", 6) ||
               !strncmp (s, "SCALE3", 6) ||
               !strncmp (s, "MASTER", 6) ||
               !strncmp (s, "KEYWDS", 6) ||
               !strncmp (s, "DBREF ", 6) ||
               !strncmp (s, "HETNAM", 6) ||
               !strncmp (s, "HETSYN", 6) ||
               !strncmp (s, "HELIX ", 6) ||
               !strncmp (s, "LINK  ", 6) ||
               !strncmp (s, "MTRIX1", 6) ||
               !strncmp (s, "MTRIX2", 6) ||
               !strncmp (s, "MTRIX3", 6) ||
               !strncmp (s, "SHEET ", 6) ||
               !strncmp (s, "CISPEP", 6) ||
               !strncmp (s, "GENERATED BY", 12) ||
               !strncmp (s, "TER ", 4) ||
               !strncmp (s, "END ", 4) ||
               !strncmp (s, "TER\n", 4) ||
               !strncmp (s, "END\n", 4) ||
               !strncmp (s, "\n", 1))
        /* ignored. */
        ;
      else if (!strncmp (s, "ATOM   ", 7))
        {
          int id;
	  /* PURIFY reports a cumulative potential memory leak on the next line */
          char *name = (char *) calloc (1, 4);
          GLfloat x = -999, y = -999, z = -999;

          sscanf (s+7, " %d ", &id);

          strncpy (name, s+12, 3);
          while (isspace(*name)) name++;
          ss = name + strlen(name)-1;
          while (isspace(*ss) && ss > name)
            *ss-- = 0;
          sscanf (s + 32, " %f %f %f ", &x, &y, &z);
/*
          fprintf (stderr, "molecule: %s: %d: atom: %d \"%s\" %9.4f %9.4f %9.4f\n",
                   filename, line,
                   id, name, x, y, z);
*/
          push_atom (m, id, name, x, y, z);
        }
      else if (!strncmp (s, "HETATM ", 7))
        {
          int id;
	  /* PURIFY reports a cumulative potential memory leak on the next line */
          char *name = (char *) calloc (1, 4);
          GLfloat x = -999, y = -999, z = -999;

          sscanf (s+7, " %d ", &id);

          strncpy (name, s+12, 3);
          while (isspace(*name)) name++;
          ss = name + strlen(name)-1;
          while (isspace(*ss) && ss > name)
            *ss-- = 0;
          sscanf (s + 30, " %f %f %f ", &x, &y, &z);
/*
          fprintf (stderr, "molecule: %s: %d: atom: %d \"%s\" %9.4f %9.4f %9.4f\n",
                   filename, line,
                   id, name, x, y, z);
*/
          push_atom (m, id, name, x, y, z);
        }
      else if (!strncmp (s, "CONECT ", 7))
        {
          int atoms[11];
          int i = sscanf (s + 8, " %d %d %d %d %d %d %d %d %d %d %d ",
                          &atoms[0], &atoms[1], &atoms[2], &atoms[3], 
                          &atoms[4], &atoms[5], &atoms[6], &atoms[7], 
                          &atoms[8], &atoms[9], &atoms[10], &atoms[11]);
          int j;
          for (j = 1; j < i; j++)
            if (atoms[j] > 0)
              {
/*
                fprintf (stderr, "molecule: %s: %d: bond: %d %d\n",
                         filename, line, atoms[0], atoms[j]);
*/
                push_bond (m, atoms[0], atoms[j]);
              }
        }
      else
        {
          char *s1 = strdup (s);
          for (ss = s1; *ss && *ss != '\n'; ss++)
            ;
          *ss = 0;
          fprintf (stderr, "molecule: %s: %d: unrecognised line: %s\n",
                   filename, line, s1);
        }

      while (*s && *s != '\n')
        s++;
      if (*s == '\n')
        s++;
      line++;
    }
}


static void
parse_pdb_file (molecule *m, const char *name)
{
  FILE *in;
  int buf_size = 40960;
  char *buf;
  int line = 1;

  in = fopen(name, "r");
  if (!in)
    {
      char *buf = (char *) malloc(1024 + strlen(name));
      sprintf(buf, "molecule: error reading \"%s\"", name);
      perror(buf);
      /* exit (1); */
    }

  buf = (char *) malloc (buf_size);

  while (fgets (buf, buf_size-1, in))
    {
      char *s;
      for (s = buf; *s; s++)
        if (*s == '\r') *s = '\n';
      parse_pdb_data (m, buf, name, line++);
    }

  free (buf);
  fclose (in);

  if (!m->natoms)
    {
      fprintf (stderr, "molecule: file %s contains no atomic coordinates!\n",
               name);
      /* exit (1); */
    }

  if (!m->nbonds && do_bonds)
    {
      fprintf (stderr, "molecule: warning: file %s contains no atomic bond info.\n",
               name);
      do_bonds = 0;
    }
}


static void
generate_molecule_formula (molecule *m)
{
  char *buf = (char *) malloc (m->natoms * 10);
  char *s = buf;
  int i;
  struct { char *atom; int count; } counts[200];
  memset (counts, 0, sizeof(counts));
  *s = 0;
  for (i = 0; i < m->natoms; i++)
    {
      int j = 0;
      char *a = (char *) m->atoms[i].label;
      char *e;
      while (!isalpha(*a)) a++;
      a = strdup (a);
      for (e = a; isalpha(*e); e++);
      *e = 0;
      while (counts[j].atom && !!strcmp(a, counts[j].atom))
        j++;
      if (counts[j].atom)
        free (a);
      else
        counts[j].atom = a;
      counts[j].count++;
    }

  i = 0;
  while (counts[i].atom)
    {
      strcat (s, counts[i].atom);
      free (counts[i].atom);
      s += strlen (s);
      if (counts[i].count > 1)
        sprintf (s, "(%d)", counts[i].count);
      s += strlen (s);
      i++;
    }

  if (!m->label) m->label = strdup("");
  s = (char *) malloc (strlen (m->label) + strlen (buf) + 2);
  strcpy (s, m->label);
  strcat (s, "\n");
  strcat (s, buf);
  free ((char *) m->label);
  free (buf);
  m->label = s;
}

static void
insert_vertical_whitespace (char *string)
{
  while (*string)
    {
      if ((string[0] == ',' ||
           string[0] == ';' ||
           string[0] == ':') &&
          string[1] == ' ')
        string[0] = ' ', string[1] = '\n';
      string++;
    }
}


/* Construct the molecule data from either: the builtins; or from
   the (one) .pdb file specified with -molecule.
 */
static void
load_molecules (ModeInfo *mi)
{
  molecule_configuration *mc = &mcs[MI_SCREEN(mi)];
  int wire = cur_wire;

  if (!molecule_str || !*molecule_str ||
      !strcmp(molecule_str, "(default)"))	/* do the builtins */
    {
      int i;
      mc->nmolecules = countof(builtin_pdb_data);
      if (!mols) {
        mols = (molecule *) calloc (sizeof (molecule), mc->nmolecules);
        for (i = 0; i < mc->nmolecules; i++)
          {
            char name[100];
            sprintf (name, "<builtin-%d>", i);
            parse_pdb_data (&mols[i], builtin_pdb_data[i], name, 1);
            generate_molecule_formula (&mols[i]);
            insert_vertical_whitespace ((char *) mols[i].label);
          }
      }
      mc->molecules = mols;
    }
  else						/* Load a file */
    {
      int i = 0;
      mc->nmolecules = 1;
      if (!mols) {
        mols = (molecule *) calloc (sizeof (molecule), mc->nmolecules);
        parse_pdb_file (&mols[i], molecule_str);
        generate_molecule_formula (&mols[i]);
        insert_vertical_whitespace ((char *) mols[i].label);

        if ((wire || !do_atoms) &&
            !do_labels &&
            mols[i].nbonds == 0)
          {
            /* If we're not drawing atoms (e.g., wireframe mode), and
               there is no bond info, then make sure labels are turned on,
               or we'll be looking at a black screen... */
            fprintf (stderr, "molecule: no bonds: turning -label on.\n");
            do_labels = 1;
          }
       }
       mc->molecules = mols;
    }
}



/* Window management, etc
 */
void
reshape_molecule (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  gluPerspective( 30.0, 1/h, 1.0, 100.0 );
  gluLookAt( 0.0, 0.0, 15.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -15.0);

  glClear(GL_COLOR_BUFFER_BIT);
}


static void
gl_init (ModeInfo *mi)
{
  static GLfloat pos[4] = {5.0, 5.0, 10.0, 1.0};
  glLightfv(GL_LIGHT0, GL_POSITION, pos);
  if (!not_first_gl)
   {
    orig_do_labels = do_labels;
    orig_do_bonds = do_bonds;
    orig_wire = cur_wire;
    orig_do_titles = do_titles;
    not_first_gl = True;
   }
}


/* lifted from lament.c */
#define RAND(n) ((long) ((LRAND() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((LRAND() & 1) ? 1 : -1)

static void
rotate(GLfloat *pos, GLfloat *v, GLfloat *dv, GLfloat max_v)
{
  double ppos = *pos;

  /* tick position */
  if (ppos < 0)
    ppos = -(ppos + *v);
  else
    ppos += *v;

  if (ppos > 1.0)
    ppos -= 1.0;
  else if (ppos < 0)
    ppos += 1.0;

  if (ppos < 0) {
    /* abort(); */
    fprintf (stderr, "ppos = %g\n", ppos);
  }
  if (ppos > 1.0) {
     /* abort(); */
    fprintf (stderr, "ppos = %g\n", ppos);
  }
  *pos = (*pos > 0 ? ppos : -ppos);

  /* accelerate */
  *v += *dv;

  /* clamp velocity */
  if (*v > max_v || *v < -max_v)
    {
      *dv = -*dv;
    }
  /* If it stops, start it going in the other direction. */
  else if (*v < 0)
    {
      if (random() % 4)
	{
	  *v = 0;

	  /* keep going in the same direction */
	  if (random() % 2)
	    *dv = 0;
	  else if (*dv < 0)
	    *dv = -*dv;
	}
      else
	{
	  /* reverse gears */
	  *v = -*v;
	  *dv = -*dv;
	  *pos = -*pos;
	}
    }

  /* Alter direction of rotational acceleration randomly. */
  if (! (random() % 120))
    *dv = -*dv;

  /* Change acceleration very occasionally. */
  if (! (random() % 200))
    {
      if (*dv == 0)
	*dv = 0.00001;
      else if (random() & 1)
	*dv *= 1.2;
      else
	*dv *= 0.8;
    }
}


static void
startup_blurb (ModeInfo *mi)
{
  molecule_configuration *mc = &mcs[MI_SCREEN(mi)];
  const char *s = "Constructing molecules...";
  print_title_string (mi, s,
                      MI_WIDTH(mi) - (string_width (mc->xfont2, s) + 40),
                      10 + mc->xfont2->ascent + mc->xfont2->descent,
                      mc->xfont2->ascent + mc->xfont2->descent);
  glFinish();
  glXSwapBuffers(MI_DISPLAY(mi), MI_WINDOW(mi));
  firstcall = False;
}

void 
init_molecule (ModeInfo *mi)
{
  molecule_configuration *mc;
  int wire;
  int screen;

#ifndef STANDALONE
  timeout = MI_CYCLES(mi);
#endif
  if (!mcs) {
    mcs = (molecule_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (molecule_configuration));
    if (!mcs) {
       return;
    }
  }

  mc = &mcs[MI_SCREEN(mi)];
  if (mc->glx_context) {
	/* Display lists MUST be freed while their glXContext is current. */
	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(mc->glx_context));
	if (glIsList(mc->font1_dlist)) {
	  glDeleteLists(mc->font1_dlist, 1);
	  mc->font1_dlist = 0;
	}
	if (glIsList(mc->font2_dlist)) {
	  glDeleteLists(mc->font2_dlist, 1);
	  mc->font2_dlist = 0;
	}
	if (glIsList(mc->molecule_dlist)) {
	  glDeleteLists(mc->molecule_dlist, 1);
	  mc->molecule_dlist = 0;
	}
  }

  if ((mc->glx_context = init_GL(mi)) != NULL) {
    gl_init(mi);
    last = 0;
    reshape_molecule (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  load_fonts (mi);
  if (firstcall)
  startup_blurb (mi);
  cur_wire = MI_IS_WIREFRAME(mi);
  wire = cur_wire;

  mc->rotx = FLOATRAND(1.0) * RANDSIGN();
  mc->roty = FLOATRAND(1.0) * RANDSIGN();
  mc->rotz = FLOATRAND(1.0) * RANDSIGN();

  /* bell curve from 0-6 degrees, avg 3 */
  mc->dx = (FLOATRAND(0.1) + FLOATRAND(0.1) + FLOATRAND(0.1)) / (360/2);
  mc->dy = (FLOATRAND(0.1) + FLOATRAND(0.1) + FLOATRAND(0.1)) / (360/2);
  mc->dz = (FLOATRAND(0.1) + FLOATRAND(0.1) + FLOATRAND(0.1)) / (360/2);

  mc->d_max = mc->dx * 8;

  mc->ddx = 0.00006 + FLOATRAND(0.00003);
  mc->ddy = 0.00006 + FLOATRAND(0.00003);
  mc->ddz = 0.00006 + FLOATRAND(0.00003);

  {
    char *s = do_spin;
    while (*s)
      {
        if      (*s == 'x' || *s == 'X') mc->spin_x = 1;
        else if (*s == 'y' || *s == 'Y') mc->spin_y = 1;
        else if (*s == 'z' || *s == 'Z') mc->spin_z = 1;
        else
          {
            fprintf (stderr,
         "molecule: spin must contain only the characters X, Y, or Z (not \"%s\")\n",
                     do_spin);
            /* exit (1); */
          }
        s++;
      }
  }

  mc->molecule_dlist = glGenLists(1);

  load_molecules (mi);
  mc->which =  NRAND(mc->nmolecules);
  
#ifdef STANDALONE
  mc->no_label_threshold = get_float_resource ("noLabelThreshold",
                                               "NoLabelThreshold");
  mc->wireframe_threshold = get_float_resource ("wireframeThreshold",
                                                "WireframeThreshold");
#else
  mc->no_label_threshold = 30;
  mc->wireframe_threshold = 150;
#endif

  if (wire)
    do_bonds = 1;
}


void
draw_molecule (ModeInfo *mi)
{
/*  static time_t last = 0; */
  time_t now = time ((time_t *) 0);
  molecule_configuration *mc = &mcs[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!mc->glx_context)
    return;

  if (last + timeout <= now)   /* randomize molecules every -timeout seconds */
    {
      if (mc->nmolecules == 1)
        {
          if (last != 0) goto SKIP;
          mc->which = 0;
        }
      else if (last == 0)
        {
          mc->which = NRAND(mc->nmolecules);
        }
      else
        {
          int n = mc->which;
          while (n == mc->which)
            n = NRAND(mc->nmolecules);
          mc->which = n;
        }

      last = now;


      glNewList (mc->molecule_dlist, GL_COMPILE);
      ensure_bounding_box_visible (mi);
      if (MI_IS_ICONIC(mi))
        {do_labels = False;
	 do_bonds  = True;
         do_titles = False;
	}
      else
        {
      do_labels = orig_do_labels;
      do_bonds = orig_do_bonds;
      do_titles = orig_do_titles;
        }
      cur_wire = orig_wire;

      if (mc->molecule_size > mc->no_label_threshold)
        do_labels = 0;
      if (mc->molecule_size > mc->wireframe_threshold)
        cur_wire = 1;

      if (cur_wire)
        do_bonds = 1;

      build_molecule (mi);

      glEndList();
    }
 SKIP:

  glPushMatrix ();
  glScalef(1.1, 1.1, 1.1);

  {
    GLfloat x, y, z;

    if (do_wander)
      {
        static int frame = 0;

#       define SINOID(SCALE,SIZE) \
        ((((1 + sin((frame * (SCALE)) / 2 * M_PI)) / 2.0) * (SIZE)) - (SIZE)/2)

        x = SINOID(0.031, 9.0);
        y = SINOID(0.023, 9.0);
        z = SINOID(0.017, 9.0);
        frame++;
        glTranslatef(x, y, z);
      }

    if (mc->spin_x || mc->spin_y || mc->spin_z)
      {
        x = mc->rotx;
        y = mc->roty;
        z = mc->rotz;
        if (x < 0) x = 1 - (x + 1);
        if (y < 0) y = 1 - (y + 1);
        if (z < 0) z = 1 - (z + 1);

        if (mc->spin_x) glRotatef(x * 360, 1.0, 0.0, 0.0);
        if (mc->spin_y) glRotatef(y * 360, 0.0, 1.0, 0.0);
        if (mc->spin_z) glRotatef(z * 360, 0.0, 0.0, 1.0);

        rotate(&mc->rotx, &mc->dx, &mc->ddx, mc->d_max);
        rotate(&mc->roty, &mc->dy, &mc->ddy, mc->d_max);
        rotate(&mc->rotz, &mc->dz, &mc->ddz, mc->d_max);
      }
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glCallList (mc->molecule_dlist);
  glPopMatrix ();

  if (MI_IS_FPS(mi)) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

void
release_molecule(ModeInfo * mi)
{
  if (mcs != NULL) {
	int         screen;
	int         i;

	for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
	  molecule_configuration *mc = &mcs[screen];

/*
	  for (i = 0; i < mc->nmolecules; i++) {
	    molecule *m = &mc->molecules[i];

	    if (m->atoms) {
		(void) free ((char *) m->atoms);
		m->atoms = NULL;
	    }
	    if (m->bonds) {
		(void) free ((char *) m->bonds);
		m->bonds = NULL;
	    }
	    if (m->label) {
		(void) free ((char *) m->label);
		m->label = NULL;
	    }
	  }
	  if (mc->molecules) {
		(void) free ((char *) mc->molecules);
		mc->molecules = NULL;
	  }
*/

	  if (mc->glx_context) {
		/* Display lists MUST be freed while their glXContext is current. */
		glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(mc->glx_context));
		clear_gl_error ();
		if (glIsList(mc->molecule_dlist)) {
			glDeleteLists(mc->molecule_dlist, 1);
			check_gl_error ("glDeleteLists");
			mc->molecule_dlist = 0;
		}

		/* Free font stuff */
		if (mc->xfont1) {
			int last;

			last = mc->xfont1->max_char_or_byte2;
			clear_gl_error ();
			if (glIsList(mc->font1_dlist)) {
				glDeleteLists (mc->font1_dlist,(GLuint) last+1);
				check_gl_error ("glDeleteLists");
				mc->font1_dlist = 0;
			}
			XFreeFont(MI_DISPLAY(mi),mc->xfont1);
			mc->xfont1 = (XFontStruct *) NULL;
		}
		if (mc->xfont2) {
			int last;

			last = mc->xfont2->max_char_or_byte2;
			clear_gl_error ();
			if (glIsList(mc->font2_dlist)) {
				glDeleteLists (mc->font2_dlist,(GLuint) last+1);
				check_gl_error ("glDeleteLists");
				mc->font2_dlist = 0;
			}
			XFreeFont(MI_DISPLAY(mi),mc->xfont2);
			mc->xfont2 = (XFontStruct *) NULL;
	  	}
	  }
	}
	(void) free((void *) mcs);
	mcs = NULL;
  }
  FreeAllGL(mi);
}

#endif /* USE_GL */
