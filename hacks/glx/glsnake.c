/* glsnake, Copyright (c) 2001,2002
 * Jamie Wilkinson,    Andrew Bennetts,     Peter Aylett
 * jaq@spacepants.org, andrew@puzzling.org, peter@ylett.com
 *
 * ported to xscreensaver 15th Jan 2002 by Jamie Wilkinson
 * http://spacepants.org/src/glsnake/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS     "glsnake"
#define HACK_INIT     glsnake_init
#define HACK_DRAW     glsnake_draw
#define HACK_RESHAPE  glsnake_reshape
#define sws_opts      xlockmore_opts

#define DEF_SPEED       "0.05"
#define DEF_EXPLODE     "0.03"
#define DEF_VELOCITY    "1.0"
#define DEF_ACCEL       "0.1"
#define DEF_STATICTIME  "5000"
#define DEF_YSPIN       "0.10"
#define DEF_ZSPIN       "0.14"
#define DEF_SCARYCOLOUR "False"
#define DEF_LABELS		"True"

#define DEFAULTS	"*delay:	30000        \n" \
			"*count:        30            \n" \
			"*showFPS:      False         \n" \
			"*wireframe:    False         \n" \
			"*speed:      " DEF_SPEED "   \n" \
			"*explode:    " DEF_EXPLODE " \n" \
			"*velocity:   " DEF_VELOCITY " \n" \
/*			"*accel:      " DEF_ACCEL "   \n" */ \
			"*statictime: " DEF_STATICTIME " \n" \
			"*yspin:      " DEF_YSPIN "   \n" \
			"*zspin:      " DEF_ZSPIN "   \n" \
			"*scarycolour:" DEF_SCARYCOLOUR " \n" \
			"*labels:     " DEF_LABELS "  \n" \
			"*labelfont:  -*-helvetica-medium-r-*-*-*-120-*\n" \



#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"

#ifdef USE_GL /* whole file */

#include <GL/glu.h>
#include <sys/timeb.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ZERO  0
#define LEFT  1
#define PIN   2
#define RIGHT 3

typedef struct model_s {
	char * name;
	float node[24];
} model_t;

typedef struct nodeang_s {
	float cur_angle;
	float dest_angle;
} nodeang_t;

typedef struct {
	GLXContext * glx_context;

	int node_list; /* name of the display list */
	int is_cyclic;
	int is_legal;
	int last_turn;
	int selected;
	struct timeb last_iteration;
	struct timeb last_morph;
	int morphing;
	nodeang_t node[24];
	GLfloat roty;
	GLfloat rotz;
	int paused;
	int dragging;
	int m_count;
	int m;
	int cur_model;
	int interactive;
	GLfloat colour_t[3];
	GLfloat colour_i[3];
	GLfloat colour[3];
	model_t * models;

	XFontStruct * font;
	GLuint font_list;
} glsnake_configuration;

static glsnake_configuration *glc = NULL;

static GLfloat speed;
static GLfloat explode;
static GLfloat velocity;
/* static GLfloat accel; */
static long statictime;
static GLfloat yspin;
static GLfloat zspin;
static Bool scarycolour;
static Bool labels;

static XrmOptionDescRec opts[] = {
  { "-speed", ".speed", XrmoptionSepArg, 0 },
  { "-explode", ".explode", XrmoptionSepArg, 0 },
  { "-velocity", ".velocity", XrmoptionSepArg, 0 },
/*  { "-accel", ".accel", XrmoptionSepArg, 0 }, */
  { "-statictime", ".statictime", XrmoptionSepArg, 0 },
  { "-yspin", ".yspin", XrmoptionSepArg, 0 },
  { "-zspin", ".zspin", XrmoptionSepArg, 0 },
  { "-scarycolour", ".scarycolour", XrmoptionNoArg, "True" },
  { "+scarycolour", ".scarycolour", XrmoptionNoArg, "False" },
  { "-labels", ".labels", XrmoptionNoArg, "True" },
  { "+labels", ".labels", XrmoptionNoArg, "False" },
};

static argtype vars[] = {
  {(caddr_t *) &speed, "speed", "Speed", DEF_SPEED, t_Float},
  {(caddr_t *) &explode, "explode", "Explode", DEF_EXPLODE, t_Float},
  {(caddr_t *) &velocity, "velocity", "Velocity", DEF_VELOCITY, t_Float},
/*  {(caddr_t *) &accel, "accel", "Acceleration", DEF_ACCEL, t_Float}, */
  {(caddr_t *) &statictime, "statictime", "Static Time", DEF_STATICTIME, t_Int},
  {(caddr_t *) &yspin, "yspin", "Y Spin", DEF_YSPIN, t_Float},
  {(caddr_t *) &zspin, "zspin", "Z Spin", DEF_ZSPIN, t_Float},
/*  {(caddr_t *) &interactive, "interactive", "Interactive", DEF_INTERACTIVE, t_Bool}, */
  {(caddr_t *) &scarycolour, "scarycolour", "Scary Colour", DEF_SCARYCOLOUR, t_Bool},
  {(caddr_t *) &labels, "labels", "Labels", DEF_LABELS, t_Bool},
};

ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};

/* prism models */
#define VOFFSET 0.045
float solid_prism_v[][3] = {
	/* first corner, bottom left front */
	{ VOFFSET, VOFFSET, 1.0 },
	{ VOFFSET, 0.0, 1.0 - VOFFSET },
	{ 0.0, VOFFSET, 1.0 - VOFFSET },
	/* second corner, rear */
	{ VOFFSET, VOFFSET, 0.00 },
	{ VOFFSET, 0.0, VOFFSET },
	{ 0.0, VOFFSET, VOFFSET },
	/* third, right front */
	{ 1.0 - VOFFSET / M_SQRT1_2, VOFFSET, 1.0 },
	{ 1.0 - VOFFSET / M_SQRT1_2, 0.0, 1.0 - VOFFSET },
	{ 1.0 - VOFFSET * M_SQRT1_2, VOFFSET, 1.0 - VOFFSET },
	/* fourth, right rear */
	{ 1.0 - VOFFSET / M_SQRT1_2, VOFFSET, 0.0 },
	{ 1.0 - VOFFSET / M_SQRT1_2, 0.0, VOFFSET },
	{ 1.0 - VOFFSET * M_SQRT1_2, VOFFSET, VOFFSET },
	/* fifth, upper front */
	{ VOFFSET, 1.0 - VOFFSET / M_SQRT1_2, 1.0 },
	{ VOFFSET / M_SQRT1_2, 1.0 - VOFFSET * M_SQRT1_2, 1.0 - VOFFSET },
	{ 0.0, 1.0 - VOFFSET / M_SQRT1_2, 1.0 - VOFFSET},
	/* sixth, upper rear */
	{ VOFFSET, 1.0 - VOFFSET / M_SQRT1_2, 0.0 },
	{ VOFFSET / M_SQRT1_2, 1.0 - VOFFSET * M_SQRT1_2, VOFFSET },
	{ 0.0, 1.0 - VOFFSET / M_SQRT1_2, VOFFSET }
};

/* normals */
float solid_prism_n[][3] = {
	/* corners */
	{ -VOFFSET, -VOFFSET, VOFFSET },
	{ VOFFSET, -VOFFSET, VOFFSET },
	{ -VOFFSET, VOFFSET, VOFFSET },
	{ -VOFFSET, -VOFFSET, -VOFFSET },
	{ VOFFSET, -VOFFSET, -VOFFSET },
	{ -VOFFSET, VOFFSET, -VOFFSET },
	/* edges */
	{ -VOFFSET, 0.0, VOFFSET },
	{ 0.0, -VOFFSET, VOFFSET },
	{ VOFFSET, VOFFSET, VOFFSET },
	{ -VOFFSET, 0.0, -VOFFSET },
	{ 0.0, -VOFFSET, -VOFFSET },
	{ VOFFSET, VOFFSET, -VOFFSET },
	{ -VOFFSET, -VOFFSET, 0.0 },
	{ VOFFSET, -VOFFSET, 0.0 },
	{ -VOFFSET, VOFFSET, 0.0 },
	/* faces */
	{ 0.0, 0.0, 1.0 },
	{ 0.0, -1.0, 0.0 },
	{ M_SQRT1_2, M_SQRT1_2, 0.0 },
	{ -1.0, 0.0, 0.0 },
	{ 0.0, 0.0, -1.0 }
};

/* vertices */
float wire_prism_v[][3] = {
	{ 0.0, 0.0, 1.0 },
	{ 1.0, 0.0, 1.0 },
	{ 0.0, 1.0, 1.0 },
	{ 0.0, 0.0, 0.0 },
	{ 1.0, 0.0, 0.0 },
	{ 0.0, 1.0, 0.0 }
};

/* normals */
float wire_prism_n[][3] = {
	{ 0.0, 0.0, 1.0},
	{ 0.0,-1.0, 0.0},
	{ M_SQRT1_2, M_SQRT1_2, 0.0},
	{-1.0, 0.0, 0.0},
	{ 0.0, 0.0,-1.0}
};

/* default models */
#define Z   0.0
#define L  90.0
#define P 180.0
#define R 270.0

static model_t default_models[] = { 
	{ "Ball", 
		{ R, R, L, L, R, L, R, R, L, R, L, L, R, R, L, L, R, L, R, R, L, R, L }
	},
	{ "Snow",
		{ R, R, R, R, L, L, L, L, R, R, R, R, L, L, L, L, R, R, R, R, L, L, L }
	},
	{ "Propellor",
		{ Z, Z, Z, R, L, R, Z, L, Z, Z, Z, R, L, R, Z, L, Z, Z, Z, R, L, R, Z }
	},
	{ "Flamingo",
		{ Z, P, Z, Z, Z, Z, Z, P, R, R, P, R, L, P, L, R, P, R, R, Z, Z, Z, P }
	},
	{ "Cat",
		{ Z, P, P, Z, P, P, Z, L, Z, P, P, Z, P, P, Z, P, P, Z, Z, Z, Z, Z, Z }
	},
	{ "Rooster",
		{ Z, Z, P, P, Z, L, Z, L, R, P, R, Z, P, P, Z, R, P, R, L, Z, L, Z, P }
	}
};

/* add a model to the model list */
model_t * add_model(model_t * models, char * name, int * rotations, int * count) {
	int i;
	
	(*count)++;
	models = realloc(models, sizeof(model_t) * (*count));
	models[(*count)-1].name = strdup(name);
#ifdef DEBUG
	fprintf(stderr, "resized models to %d bytes for model %s\n", sizeof(model_t) * (*count), models[(*count)-1].name);
#endif
	for (i = 0; i < 24; i++) {
		models[(*count)-1].node[i] = rotations[i] * 90.0;
	}
	return models;
}

/* filename is the name of the file to load
 * models is the pointer to where the models will be kept
 * returns a new pointer to models
 * count is number of models read
 */
model_t * load_modelfile(char * filename, model_t * models, int * count) {
	int c;
	FILE * f;
	char buffy[256];
	int rotations[24];
	int name = 1;
	int rots = 0;

	f = fopen(filename, "r");
	if (f == NULL) {
		int error_msg_len = strlen(filename) + 33 + 1;
		char * error_msg = (char *) malloc(sizeof(char) * error_msg_len);
		sprintf(error_msg, "Unable to open model data file \"%s\"", filename);
		perror(error_msg);
		free(error_msg);
		return models;
	}
		
	while ((c = getc(f)) != EOF) {
		switch (c) {
			/* ignore comments */
			case '#':
				while (c != '\n')
					c = getc(f);
				break;
			case ':':
				buffy[name-1] = '\0';
				name = 0;
				break;
			case '\n':
				if (rots > 0) {
#ifdef DEBUG
					/* print out the model we just read in */
					int i;
					printf("%s: ", buffy);
					for (i = 0; i < rots; i++) {
						switch (rotations[i]) {
							case LEFT:
								printf("L");
								break;
							case RIGHT:
								printf("R");
								break;
							case PIN:
								printf("P");
								break;
							case ZERO:
								printf("Z");
								break;
						}
					}
					printf("\n");
#endif
					models = add_model(models, buffy, rotations, count);
				}
				name = 1;
				rots = 0;
				break;
			default:
				if (name) {
					buffy[name-1] = c;
					name++;
					if (name > 255)
						fprintf(stderr, "buffy overflow warning\n");
				} else {
					switch (c) {
						case '0':
						case 'Z':
							rotations[rots] = ZERO;
							rots++;
							break;
						case '1':
						case 'L':
							rotations[rots] = LEFT;
							rots++;
							break;
						case '2':
						case 'P':
							rotations[rots] = PIN;
							rots++;
							break;
						case '3':
						case 'R':
							rotations[rots] = RIGHT;
							rots++;
							break;
						default:
							break;
					}
				}
				break;
		}
	}
	return models;
}

model_t * load_models(char * dirpath, model_t * models, int * count) {
	char name[1024];
	struct dirent * dp;
	DIR * dfd;

	if ((dfd = opendir(dirpath)) == NULL) {
		if (strstr(dirpath, "data") == NULL)
/*			fprintf(stderr, "load_models: can't read %s/\n", dirpath); */
		return models;
	}
	while ((dp = readdir(dfd)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;
		if (strlen(dirpath) + strlen(dp->d_name) + 2 > sizeof(name))
			fprintf(stderr, "load_models: name %s/%s too long\n", dirpath, dp->d_name);
		else {
			sprintf(name, "%s/%s", dirpath, dp->d_name);
			if (strcmp(&name[(int) strlen(name) - 7], "glsnake") == 0) {
#ifdef DEBUG
				fprintf(stderr, "load_models: opening %s\n", name);	
#endif
				models = load_modelfile(name, models, count);
			}
		}
	}
	closedir(dfd);
	return models;
}

/* snake metrics */
#define X_MASK 1
#define Y_MASK 2
#define Z_MASK 4
#define GETSCALAR(vec,mask) ((vec)==(mask) ? 1 : ((vec)==-(mask) ? -1 : 0 ))

int cross_product(int src_dir, int dest_dir) {
	return X_MASK * (GETSCALAR(src_dir, Y_MASK) * GETSCALAR(dest_dir, Z_MASK) -
			GETSCALAR(src_dir, Z_MASK) * GETSCALAR(dest_dir, Y_MASK)) +
		Y_MASK * (GETSCALAR(src_dir, Z_MASK) * GETSCALAR(dest_dir, X_MASK) -
			GETSCALAR(src_dir, X_MASK) * GETSCALAR(dest_dir, Z_MASK)) +
		Z_MASK * (GETSCALAR(src_dir, X_MASK) * GETSCALAR(dest_dir, Y_MASK) -
			GETSCALAR(src_dir, Y_MASK) * GETSCALAR(dest_dir, X_MASK));
}

void calc_snake_metrics(glsnake_configuration * bp) {
	int src_dir, dest_dir;
	int i, x, y, z;
	int prev_src_dir = -Y_MASK;
	int prev_dest_dir = Z_MASK;
	int grid[25][25][25];

	/* zero the grid */
	memset(&grid, 0, sizeof(int) * 25*25*25);

	bp->is_legal = 1;
	x = y = z = 12;

	/* trace path of snake and keep record for is_legal */
	for (i = 0; i < 23; i++) {
		/* establish new state variables */
		src_dir = -prev_dest_dir;
		x += GETSCALAR(prev_dest_dir, X_MASK);
		y += GETSCALAR(prev_dest_dir, Y_MASK);
		z += GETSCALAR(prev_dest_dir, Z_MASK);

		switch ((int) bp->node[i].dest_angle) {
			case (int) (ZERO * 90.0):
				dest_dir = -prev_src_dir;
				break;
			case (int) (PIN * 90.0):
				dest_dir = prev_src_dir;
				break;
			case (int) (RIGHT * 90.):
			case (int) (LEFT * 90.0):
				dest_dir = cross_product(prev_src_dir, prev_dest_dir);
				if (bp->node[i].dest_angle == (int) (RIGHT * 90.0))
					dest_dir = -dest_dir;
				break;
			default:
				/* prevent spurious "might be used uninitialised" warnings */
				dest_dir = 0;
				break;
		}

		if (grid[x][y][z] == 0)
			grid[x][y][z] = src_dir + dest_dir;
		else if (grid[x][y][z] + src_dir + dest_dir == 0)
			grid[x][y][z] = 8;
		else
			bp->is_legal = 0;

		prev_src_dir = src_dir;
		prev_dest_dir = dest_dir;
	}

	/* determine if the snake is cyclic */
	bp->is_cyclic = (dest_dir == Y_MASK && x == 12 && y == 11 && x == 12);

	/* determine last turn */
	bp->last_turn = -1;
	if (bp->is_cyclic) {
		switch (src_dir) {
			case -Z_MASK:
				bp->last_turn = ZERO * 90.0;
				break;
			case Z_MASK:
				bp->last_turn = PIN * 90.0;
				break;
			case X_MASK:
				bp->last_turn = LEFT * 90.0;
				break;
			case -X_MASK:
				bp->last_turn = RIGHT * 90.0;
				break;
		}
	}
}

void set_colours(glsnake_configuration * bp, int immediate) {
	/* set target colour */
	if (!bp->is_legal) {
		bp->colour_t[0] = 0.5;
		bp->colour_t[1] = 0.5;
		bp->colour_t[2] = 0.5;
	} else if (bp->is_cyclic) {
		bp->colour_t[0] = 0.4;
		bp->colour_t[1] = 0.8;
		bp->colour_t[2] = 0.2;
	} else {
		bp->colour_t[0] = 0.3;
		bp->colour_t[1] = 0.1;
		bp->colour_t[2] = 0.9;
	}
	if (immediate) {
		bp->colour_i[0] = bp->colour_t[0] - bp->colour[0];
		bp->colour_i[1] = bp->colour_t[1] - bp->colour[1];
		bp->colour_i[2] = bp->colour_t[2] - bp->colour[2];
	} else {
		/* instead of 50.0, I should actually work out how many times this gets
		 * called during a morph */
		bp->colour_i[0] = (bp->colour_t[0] - bp->colour[0]) / 50.0;
		bp->colour_i[1] = (bp->colour_t[1] - bp->colour[1]) / 50.0;
		bp->colour_i[2] = (bp->colour_t[2] - bp->colour[2]) / 50.0;
	}
}

void start_morph(int model_index, int immediate, glsnake_configuration * bp) {
	int i;

	for (i = 0; i < 23; i++) {
		bp->node[i].dest_angle = bp->models[model_index].node[i];
		if (immediate)
			bp->node[i].cur_angle = bp->models[model_index].node[i];
	}
	
	calc_snake_metrics(bp);
	set_colours(bp, 0);
	bp->cur_model = model_index;
	bp->morphing = 1;
}

/* convex hull */

/* model labels */
void draw_label(ModeInfo * mi) {
	glsnake_configuration *bp = &glc[MI_SCREEN(mi)];
	
	glPushAttrib(GL_TRANSFORM_BIT | GL_ENABLE_BIT);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, mi->xgwa.width, 0, mi->xgwa.height);
	glColor3f(1.0, 1.0, 0.0);
	{
		char * s;
		int i, /* w, */ l;

		if (bp->interactive)
			s = "interactive";
		else
			s = bp->models[bp->cur_model].name;

		l = strlen(s);
		/*
		w = 0;
		for (i = 0; i < l; i++) {
			w += (bp->font->per_char 
					? bp->font->per_char[((int)s[i]) - bp->font->min_char_or_byte2].rbearing 
					: bp->font->min_bounds.rbearing);
		}
		*/
		
		glRasterPos2f(10, mi->xgwa.height - 10 - (bp->font->ascent + bp->font->descent));
				/* mi->xgwa.width - w, bp->font->descent + bp->font->ascent); */

		/* fprintf(stderr, "afaf.width = %d, w = %d\n", mi->xgwa.width, w); */
		
		for (i = 0; i < l; i++)
			glCallList(bp->font_list + (int)s[i]);
	}
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
}

/* load the fonts -- this function borrowed from molecule.c */
static void load_font(ModeInfo * mi, char * res, XFontStruct ** fontp, GLuint * dlistp) {
	const char * font = get_string_resource(res, "Font");
	XFontStruct * f;
	Font id;
	int first, last;

	if (!font)
		font = "-*-helvetica-medium-r-*-*-*-120-*";

	f = XLoadQueryFont(mi->dpy, font);
	if (!f)
		f = XLoadQueryFont(mi->dpy, "fixed");

	id = f->fid;
	first = f->min_char_or_byte2;
	last = f->max_char_or_byte2;
	
	clear_gl_error();
	*dlistp = glGenLists((GLuint) last + 1);
	check_gl_error("glGenLists");
	glXUseXFont(id, first, last - first + 1, *dlistp + first);
	check_gl_error("glXUseXFont");

	*fontp = f;
}



/* window management */
void glsnake_reshape(ModeInfo *mi, int w, int h) {
	glViewport (0, 0, (GLint) w, (GLint) h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(25.0, w/(GLfloat)h, 1.0, 100.0 );
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

static void gl_init(ModeInfo *mi) {
	/* glsnake_configuration *bp = &glc[MI_SCREEN(mi)]; */
	int wire = MI_IS_WIREFRAME(mi);
	float light_pos[][3] = {{0.0,0.0,20.0},{0.0,20.0,0.0}};
	float light_dir[][3] = {{0.0,0.0,-20.0},{0.0,-20.0,0.0}};

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glEnable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
	glEnable(GL_NORMALIZE);

	if (!wire) {
		glColor3f(1.0, 1.0, 1.0);
		glLightfv(GL_LIGHT0, GL_POSITION, light_pos[0]);
		glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, light_dir[0]);
		glLightfv(GL_LIGHT1, GL_POSITION, light_pos[1]);
		glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, light_dir[1]);
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glEnable(GL_LIGHT1);
		glEnable(GL_COLOR_MATERIAL);
	}
}

/* lifted from lament.c */
#define RAND(n) ((long) ((random() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((random() & 1) ? 1 : -1)

void glsnake_init(ModeInfo *mi) {
	glsnake_configuration * bp;
	int wire = MI_IS_WIREFRAME(mi);

	if (!glc) {
		glc = (glsnake_configuration *) calloc(MI_NUM_SCREENS(mi), sizeof(glsnake_configuration));
		if (!glc) {
			fprintf(stderr, "%s: out of memory\n", progname);
			exit(1);
		}
		bp = &glc[MI_SCREEN(mi)];
	}

	bp = &glc[MI_SCREEN(mi)];

	if ((bp->glx_context = init_GL(mi)) != NULL) {
		gl_init(mi);
		glsnake_reshape(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
	}

	/* initialise config variables */
	memset(&bp->node, 0, sizeof(nodeang_t) * 24);
	bp->m_count = sizeof(default_models) / sizeof(model_t); /* overwrite this in a bit */
	bp->selected = 11;
	bp->is_cyclic = 0;
	bp->is_legal = 1;
	bp->last_turn = -1;
	bp->roty = 0.0;
	bp->rotz = 0.0;
	bp->morphing = 0;
	bp->paused = 0;
	bp->dragging = 0;
	bp->interactive = 0;

	ftime(&(bp->last_iteration));
	memcpy(&(bp->last_morph), &(bp->last_iteration), sizeof(struct timeb));
	/* srand((unsigned int) bp->last_iteration.time); */

	/* load the model files */
	/* first copy the defaults to bp->m_count */
	bp->models = (model_t *) malloc(sizeof(model_t) * bp->m_count);
	memcpy(bp->models, default_models, bp->m_count * sizeof(model_t));
	/* then add on models from the Debian model file location */
	bp->models = load_models("/usr/share/glsnake", bp->models, &(bp->m_count));

	bp->m = bp->cur_model = RAND(bp->m_count);
	start_morph(bp->cur_model, 1, bp);

	calc_snake_metrics(bp);
	set_colours(bp, 1);

	/* set up a font for the labels */
	if (labels)
		load_font(mi, "labelfont", &bp->font, &bp->font_list);
	
	bp->node_list = glGenLists(1);
	glNewList(bp->node_list, GL_COMPILE);
	if (!wire) {
		/* corners */
		glBegin(GL_TRIANGLES);
		glNormal3fv(solid_prism_n[0]);
		glVertex3fv(solid_prism_v[0]);
		glVertex3fv(solid_prism_v[2]);
		glVertex3fv(solid_prism_v[1]);
    
		glNormal3fv(solid_prism_n[1]);
		glVertex3fv(solid_prism_v[6]);
		glVertex3fv(solid_prism_v[7]);
		glVertex3fv(solid_prism_v[8]);

		glNormal3fv(solid_prism_n[2]);
		glVertex3fv(solid_prism_v[12]);
		glVertex3fv(solid_prism_v[13]);
		glVertex3fv(solid_prism_v[14]);
    
		glNormal3fv(solid_prism_n[3]);
		glVertex3fv(solid_prism_v[3]);
		glVertex3fv(solid_prism_v[4]);
		glVertex3fv(solid_prism_v[5]);
	
		glNormal3fv(solid_prism_n[4]);
		glVertex3fv(solid_prism_v[9]);
		glVertex3fv(solid_prism_v[11]);
		glVertex3fv(solid_prism_v[10]);

		glNormal3fv(solid_prism_n[5]);
		glVertex3fv(solid_prism_v[16]);
		glVertex3fv(solid_prism_v[15]);
		glVertex3fv(solid_prism_v[17]);
		glEnd();

		/* edges */
		glBegin(GL_QUADS);
		glNormal3fv(solid_prism_n[6]);
		glVertex3fv(solid_prism_v[0]);
		glVertex3fv(solid_prism_v[12]);
		glVertex3fv(solid_prism_v[14]);
		glVertex3fv(solid_prism_v[2]);
	
		glNormal3fv(solid_prism_n[7]);
		glVertex3fv(solid_prism_v[0]);
		glVertex3fv(solid_prism_v[1]);
		glVertex3fv(solid_prism_v[7]);
		glVertex3fv(solid_prism_v[6]);
	
		glNormal3fv(solid_prism_n[8]);
		glVertex3fv(solid_prism_v[6]);
		glVertex3fv(solid_prism_v[8]);
		glVertex3fv(solid_prism_v[13]);
		glVertex3fv(solid_prism_v[12]);
	
		glNormal3fv(solid_prism_n[9]);
		glVertex3fv(solid_prism_v[3]);
		glVertex3fv(solid_prism_v[5]);
		glVertex3fv(solid_prism_v[17]);
		glVertex3fv(solid_prism_v[15]);
	
		glNormal3fv(solid_prism_n[10]);
		glVertex3fv(solid_prism_v[3]);
		glVertex3fv(solid_prism_v[9]);
		glVertex3fv(solid_prism_v[10]);
		glVertex3fv(solid_prism_v[4]);
	
		glNormal3fv(solid_prism_n[11]);
		glVertex3fv(solid_prism_v[15]);
		glVertex3fv(solid_prism_v[16]);
		glVertex3fv(solid_prism_v[11]);
		glVertex3fv(solid_prism_v[9]);
	
		glNormal3fv(solid_prism_n[12]);
		glVertex3fv(solid_prism_v[1]);
		glVertex3fv(solid_prism_v[2]);
		glVertex3fv(solid_prism_v[5]);
		glVertex3fv(solid_prism_v[4]);
	
		glNormal3fv(solid_prism_n[13]);
		glVertex3fv(solid_prism_v[8]);
		glVertex3fv(solid_prism_v[7]);
		glVertex3fv(solid_prism_v[10]);
		glVertex3fv(solid_prism_v[11]);
	
		glNormal3fv(solid_prism_n[14]);
		glVertex3fv(solid_prism_v[13]);
		glVertex3fv(solid_prism_v[16]);
		glVertex3fv(solid_prism_v[17]);
		glVertex3fv(solid_prism_v[14]);
		glEnd();
	
		/* faces */
		glBegin(GL_TRIANGLES);
		glNormal3fv(solid_prism_n[15]);
		glVertex3fv(solid_prism_v[0]);
		glVertex3fv(solid_prism_v[6]);
		glVertex3fv(solid_prism_v[12]);
	
		glNormal3fv(solid_prism_n[19]);
		glVertex3fv(solid_prism_v[3]);
		glVertex3fv(solid_prism_v[15]);
		glVertex3fv(solid_prism_v[9]);
		glEnd();
	
		glBegin(GL_QUADS);
		glNormal3fv(solid_prism_n[16]);
		glVertex3fv(solid_prism_v[1]);
		glVertex3fv(solid_prism_v[4]);
		glVertex3fv(solid_prism_v[10]);
		glVertex3fv(solid_prism_v[7]);
	
		glNormal3fv(solid_prism_n[17]);
		glVertex3fv(solid_prism_v[8]);
		glVertex3fv(solid_prism_v[11]);
		glVertex3fv(solid_prism_v[16]);
		glVertex3fv(solid_prism_v[13]);
	
		glNormal3fv(solid_prism_n[18]);
		glVertex3fv(solid_prism_v[2]);
		glVertex3fv(solid_prism_v[14]);
		glVertex3fv(solid_prism_v[17]);
		glVertex3fv(solid_prism_v[5]);
		glEnd();
	} else {
		/* build wire display list */
		glBegin(GL_LINE_STRIP);
		glVertex3fv(wire_prism_v[0]);
		glVertex3fv(wire_prism_v[1]);
		glVertex3fv(wire_prism_v[2]);
		glVertex3fv(wire_prism_v[0]);
		glVertex3fv(wire_prism_v[3]);
		glVertex3fv(wire_prism_v[4]);
		glVertex3fv(wire_prism_v[5]);
		glVertex3fv(wire_prism_v[3]);
		glEnd();
	
		glBegin(GL_LINES);
		glVertex3fv(wire_prism_v[1]);
		glVertex3fv(wire_prism_v[4]);
		glVertex3fv(wire_prism_v[2]);
		glVertex3fv(wire_prism_v[5]);
		glEnd();
	}
	glEndList();
}

/* "jwz?  no way man, he's my idle" -- Jaq, 2001.
 * I forget the context :( */
void glsnake_idol(glsnake_configuration * bp) {
	/* time since last iteration */
	long iter_msec;
	/* time since the beginning of last morph */
	long morf_msec;
	float iter_angle_max;
	int i;
	struct timeb current_time;
	int still_morphing;

	/* Do nothing to the model if we are paused */
	if (bp->paused) {
		/* Avoid busy waiting when nothing is changing */
		usleep(1);
		return;
	}
	/* ftime is winDOS compatible */
	ftime(&current_time);

	/* <spiv> Well, ftime gives time with millisecond resolution.
	 * <Jaq> if current time is exactly equal to last iteration, 
	 *       then don't do this block
	 * <spiv> (or worse, perhaps... who knows what the OS will do)
	 * <spiv> So if no discernable amount of time has passed:
	 * <spiv>   a) There's no point updating the screen, because
	 *             it would be the same
	 * <spiv>   b) The code will divide by zero
	 */
	iter_msec = (long) current_time.millitm - bp->last_iteration.millitm + 
		    ((long) current_time.time - bp->last_iteration.time) * 1000L;
	if (iter_msec) {
		/* save the current time */
		memcpy(&(bp->last_iteration), &current_time, sizeof(struct timeb));
		
		/* work out if we have to switch models */
		morf_msec = bp->last_iteration.millitm - bp->last_morph.millitm +
			((long) (bp->last_iteration.time - bp->last_morph.time) * 1000L);

		if ((morf_msec > statictime) && !bp->interactive) {
			memcpy(&(bp->last_morph), &(bp->last_iteration), sizeof(struct timeb));
			start_morph(RAND(bp->m_count), 0, bp);
		}

		if (bp->interactive && !bp->morphing) {
			usleep(1);
			return;
		}

		if (!bp->dragging && !bp->interactive) {
			bp->roty += 360/((1000/yspin)/iter_msec);
			bp->rotz += 360/((1000/zspin)/iter_msec);
		}

		/* work out the maximum angle for this iteration */
		iter_angle_max = 90.0 * (velocity/1000.0) * iter_msec;

		still_morphing = 0;
		for (i = 0; i < 24; i++) {
			float cur_angle = bp->node[i].cur_angle;
			float dest_angle = bp->node[i].dest_angle;
			if (cur_angle != dest_angle) {
				still_morphing = 1;
				if (fabs(cur_angle - dest_angle) <= iter_angle_max)
					bp->node[i].cur_angle = dest_angle;
				else if (fmod(cur_angle - dest_angle + 360, 360) > 180)
					bp->node[i].cur_angle = fmod(cur_angle + iter_angle_max, 360);
				else
					bp->node[i].cur_angle = fmod(cur_angle + 360 - iter_angle_max, 360);
			}
		}

		if (!still_morphing)
			bp->morphing = 0;

		/* colour cycling */
		if (fabs(bp->colour[0] - bp->colour_t[0]) <= fabs(bp->colour_i[0]))
			bp->colour[0] = bp->colour_t[0];
		else
			bp->colour[0] += bp->colour_i[0];
		if (fabs(bp->colour[1] - bp->colour_t[1]) <= fabs(bp->colour_i[1]))
			bp->colour[1] = bp->colour_t[1];
		else
			bp->colour[1] += bp->colour_i[1];
		if (fabs(bp->colour[2] - bp->colour_t[2]) <= fabs(bp->colour_i[2]))
			bp->colour[2] = bp->colour_t[2];
		else
			bp->colour[2] += bp->colour_i[2];
	} else {
		/* We are going too fast, so we may as well let the 
		 * cpu relax a little by sleeping for a millisecond. */
		usleep(1);
	}
}

void glsnake_draw(ModeInfo *mi) {
	glsnake_configuration *bp = &glc[MI_SCREEN(mi)];
	Display *dpy = MI_DISPLAY(mi);
	Window window = MI_WINDOW(mi);

	int i;
	float ang;

	if (!bp->glx_context)
    	return;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0.0, 0.0, 20.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

	/* rotate and translate into snake space */
	glRotatef(45.0, -5.0, 0.0, 1.0);
	glTranslatef(-0.5, 0.0, 0.5);

	/* rotate the 0th junction */
	glTranslatef(0.5, 0.0, 0.5);
	/* glMultMatrix(rotation); -- quaternion rotation */
	glRotatef(bp->roty, 0.0, 1.0, 0.0);
	glRotatef(bp->rotz, 0.0, 0.0, 1.0);
	glTranslated(-0.5, 0.0, -0.5);

	/* translate middle node to centre */
	for (i = 11; i >= 0; i--) {
		ang = bp->node[i].cur_angle;
		glTranslatef(0.5, 0.5, 0.5);
		glRotatef(180+ang, -1.0, 0.0, 0.0);
		glTranslatef(-1.0 - explode, 0.0, 0.0);
		glRotatef(90, 0.0, 0.0, 1.0);
		glTranslatef(-0.5, -0.5, -0.5);
	}

	/* now draw each node along the snake */
	for (i = 0; i < 24; i++) {
		glPushMatrix();

		/* choose a colour for this node */
		if (bp->interactive && (i == bp->selected || i == bp->selected+1))
			glColor3f(1.0, 1.0, 0.0);
		else {
			if (i % 2) {
				if (scarycolour)
					glColor3f(0.6, 0.0, 0.9);
				else
					glColor3fv(bp->colour);
			} else {
				if (scarycolour)
					glColor3f(0.2, 0.9, 1.0);
				else
					glColor3f(1.0, 1.0, 1.0);
			}
		}

		/* draw the node */
		glCallList(bp->node_list);

		/* now work out where to draw the next one */

		/* interpolate between models */
		ang = bp->node[i].cur_angle;

		glTranslatef(0.5, 0.5, 0.5);
		glRotatef(90, 0.0, 0.0, -1.0);
		glTranslatef(1.0 + explode, 0.0, 0.0);
		glRotatef(180 + ang, 1.0, 0.0, 0.0);
		glTranslatef(-0.5, -0.5, -0.5);
	}

	/* clear up the matrix stack */
	for (i = 0; i < 24; i++)
		glPopMatrix();

	if (labels)
		draw_label(mi);
	
	if (mi->fps_p)
		do_fps (mi);

	glsnake_idol(bp);
	
	glFlush();
	glXSwapBuffers(dpy, window);
}

#endif /* USE_GL */
