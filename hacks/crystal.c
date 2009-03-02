/* -*- Mode: C; tab-width: 4 -*- */
/* crystal --- polygons moving according to plane group rules */

#if 0
static const char sccsid[] = "@(#)crystal.c	4.12 98/09/10 xlockmore";
#endif

/*-
 * Copyright (c) 1997 by Jouk Jansen <joukj@crys.chem.uva.nl>
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * The author should like to be notified if changes have been made to the
 * routine.  Response will only be guaranteed when a VMS version of the 
 * program is available.
 *
 * A moving polygon-mode. The polygons obey 2D-planegroup symmetry.
 *
 * The groupings of the cells fall in 3 categories:
 *   oblique groups 1 and 2 where the angle gamma ranges from 60 to 120 degrees
 *   square groups 3 through 11 where the angle gamma is 90 degrees
 *   hexagonal groups 12 through 17 where the angle gamma is 120 degrees
 *
 * Revision History:
 * 03-Dec-98: Random inversion of y-axis included to simulate hexagonal groups
 *             with an angle of 60 degrees.
 * 10-Sep-98: new colour scheme
 * 24-Feb-98: added option centre which turns on/off forcing the centre of
 *              the screen to be used
 *            added option maxsize which forces the dimensions to be chasen
 *              in such ua way that the largest possible part of the screen is
 *              used 
 *            When only one unit cell is drawn, it is chosen at random
 * 18-Feb-98: added support for negative numbers with -nx and -ny meaning
 *            "random" choice with given maximum
 *            added +/-grid option. If -cell is specified this option
 *            determines if one or all unit cells are drawn.
 *            -batchcount is now a parameter for all the objects on the screen
 *            instead of the number of "unique" objects
 *            The maximum size of the objects now scales with the part
 *            of the screen used.
 *            fixed "size" problem. Now very small non-vissable objects
 *            are not allowed
 * 13-Feb-98: randomized the unit cell size
 *            runtime options -/+cell (turn on/off unit cell drawing)
 *             -nx num (number of translational symmetries in x-direction
 *             -ny num (idem y-direction but ignored for square and
 *               hexagonal space groups
 *               i.e. try xlock -mode crystal -nx 3 -ny 2
 *            Fullrandom overrules the -/+cell option.
 * 05-Feb-98: Revision + bug repairs
 *            shows unit cell
 *            use part of the screen for unit cell
 *            in hexagonal and square groups a&b axis forced to be equal
 *            cell angle for oblique groups randomly chosen between 60 and 120
 *   bugs solved: planegroups with cell angles <> 90.0 now work properly
 * 19-Sep-97: Added remaining hexagonal groups
 * 12-Jun-97: Created
 */

#ifdef STANDALONE
# define DEFAULTS		"*delay:		60000	\n" \
						 "*count:		 -500	\n" \
						 "*cycles:		  200	\n" \
						 "*size:		  -15	\n" \
						 "*ncolors:		  100	\n" \
						 "*fpsSolid:		  true	\n" \

# define reshape_crystal 0
# define crystal_handle_event 0
# include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
# include "xlock.h"			/* in xlockmore distribution */
# include "color.h"
#endif /* STANDALONE */

#define DEF_CELL "True"		/* Draw unit cell */
#define DEF_GRID "False"	/* Draw unit all cell if DEF_CELL is True */
#define DEF_NX "-3"		/* number of unit cells in x-direction */
#define DEF_NX1 1		/* number of unit cells in x-direction */
#define DEF_NY "-3"		/* number of unit cells in y-direction */
#define DEF_NY1 1		/* number of unit cells in y-direction */
#define DEF_CENTRE "False"
#define DEF_MAXSIZE "False"
#define DEF_CYCLE "True"

#undef NRAND
#define NRAND(n)           ( (n) ? (int) (LRAND() % (n)) : 0)

#define min(a,b) ((a) <= (b) ? (a) : (b))

static int  nx, ny;

static Bool unit_cell, grid_cell, centre, maxsize, cycle_p;

static XrmOptionDescRec opts[] =
{
	{"-nx", "crystal.nx", XrmoptionSepArg, 0},
	{"-ny", "crystal.ny", XrmoptionSepArg, 0},
	{"-centre", ".crystal.centre", XrmoptionNoArg, "on"},
	{"+centre", ".crystal.centre", XrmoptionNoArg, "off"},
	{"-maxsize", ".crystal.maxsize", XrmoptionNoArg, "on"},
	{"+maxsize", ".crystal.maxsize", XrmoptionNoArg, "off"},
	{"-cell", ".crystal.cell", XrmoptionNoArg, "on"},
	{"+cell", ".crystal.cell", XrmoptionNoArg, "off"},
	{"-grid", ".crystal.grid", XrmoptionNoArg, "on"},
	{"+grid", ".crystal.grid", XrmoptionNoArg, "off"},
	{"-shift", ".crystal.shift", XrmoptionNoArg, "on"},
	{"+shift", ".crystal.shift", XrmoptionNoArg, "off"}
};

static argtype vars[] =
{
	{&nx, "nx", "nx", DEF_NX, t_Int},
	{&ny, "ny", "ny", DEF_NY, t_Int},
	{&centre, "centre", "Centre", DEF_CENTRE, t_Bool},
	{&maxsize, "maxsize", "Maxsize", DEF_MAXSIZE, t_Bool},
	{&unit_cell, "cell", "Cell", DEF_CELL, t_Bool},
	{&grid_cell, "grid", "Grid", DEF_GRID, t_Bool},
	{&cycle_p, "shift", "Shift", DEF_CYCLE, t_Bool}
};
static OptionStruct desc[] =
{
	{"-nx num", "Number of unit cells in x-direction"},
	{"-ny num", "Number of unit cells in y-direction"},
	{"-/+centre", "turn on/off centering on screen"},
	{"-/+maxsize", "turn on/off use of maximum part of screen"},
	{"-/+cell", "turn on/off drawing of unit cell"},
   {"-/+grid", "turn on/off drawing of grid of unit cells (if -cell is on)"},
	{"-/+shift", "turn on/off colour cycling"}
};

ENTRYPOINT ModeSpecOpt crystal_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   crystal_description =
{"crystal", "init_crystal", "draw_crystal", "release_crystal",
 "refresh_crystal", "init_crystal", NULL, &crystal_opts,
 60000, -40, 200, -15, 64, 1.0, "",
 "Shows polygons in 2D plane groups", 0, NULL};

#endif

#define DEF_NUM_ATOM 10

#define DEF_SIZ_ATOM 10

#define PI_RAD (M_PI / 180.0)

static Bool centro[17] =
{
	False,
	True,
	False,
	False,
	False,
	True,
	True,
	True,
	True,
	True,
	True,
	True,
	False,
	False,
	False,
	True,
	True
};

static Bool primitive[17] =
{
	True,
	True,
	True,
	True,
	False,
	True,
	True,
	True,
	False,
	True,
	True,
	True,
	True,
	True,
	True,
	True,
	True
};

static short numops[34] =
{
	1, 0,
	1, 0,
	9, 7,
	2, 0,
	9, 7,
	9, 7,
	4, 2,
	5, 3,
	9, 7,
	8, 6,
	10, 6,
	8, 4,
	16, 13,
	19, 13,
	16, 10,
	19, 13,
	19, 13
};

static short operation[114] =
{
	1, 0, 0, 1, 0, 0,
	-1, 0, 0, 1, 0, 1,
	-1, 0, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 0,
	-1, 0, 0, 1, 1, 1,
	1, 0, 0, 1, 1, 1,
	0, -1, 1, 0, 0, 0,
	1, 0, 0, 1, 0, 0,
	-1, 0, 0, 1, 0, 0,
	0, 1, 1, 0, 0, 0,
	-1, 0, -1, 1, 0, 0,
	1, -1, 0, -1, 0, 0,
	0, 1, 1, 0, 0, 0,
	0, -1, 1, -1, 0, 0,
	-1, 1, -1, 0, 0, 0,
	1, 0, 0, 1, 0, 0,
	0, -1, -1, 0, 0, 0,
	-1, 1, 0, 1, 0, 0,
	1, 0, 1, -1, 0, 0
};

typedef struct {
	unsigned long colour;
	int         x0, y0, velocity[2];
	float       angle, velocity_a;
	int         num_point, at_type, size_at;
	XPoint      xy[5];
} crystalatom;

typedef struct {
	Bool        painted;
	int         win_width, win_height, num_atom;
	int         planegroup, a, b, offset_w, offset_h, nx, ny;
	float       gamma;
	crystalatom *atom;
	GC          gc;
	Bool        unit_cell, grid_cell;
	Colormap    cmap;
	XColor     *colors;
	int         ncolors;
	Bool        cycle_p, mono_p, no_colors;
	unsigned long blackpixel, whitepixel, fg, bg;
	int         direction, invert;
} crystalstruct;

static crystalstruct *crystals = NULL;

static void
trans_coor(XPoint * xyp, XPoint * new_xyp, int num_points,
	   float gamma)
{
	int         i;

	for (i = 0; i <= num_points; i++) {
		new_xyp[i].x = xyp[i].x +
			(int) (xyp[i].y * sin((gamma - 90.0) * PI_RAD));
		new_xyp[i].y = (int) (xyp[i].y / cos((gamma - 90.0) * PI_RAD));
	}
}

static void
trans_coor_back(XPoint * xyp, XPoint * new_xyp,
		int num_points, float gamma, int offset_w, int offset_h ,
		int winheight , int invert )
{
	int         i;

	for (i = 0; i <= num_points; i++) {
		new_xyp[i].y = (int) (xyp[i].y * cos((gamma - 90) * PI_RAD)) +
			offset_h;
		new_xyp[i].x = xyp[i].x - (int) (xyp[i].y * sin((gamma - 90.0)
						       * PI_RAD)) + offset_w;
	   if ( invert ) new_xyp[i].y = winheight - new_xyp[i].y;
	}
}

static void
crystal_setupatom(crystalatom * atom0, float gamma)
{
	XPoint      xy[5];
	int         x0, y0;

	y0 = (int) (atom0->y0 * cos((gamma - 90) * PI_RAD));
	x0 = atom0->x0 - (int) (atom0->y0 * sin((gamma - 90.0) * PI_RAD));
	switch (atom0->at_type) {
		case 0:	/* rectangles */
			xy[0].x = x0 + (int) (2 * atom0->size_at *
					      cos(atom0->angle)) +
				(int) (atom0->size_at * sin(atom0->angle));
			xy[0].y = y0 + (int) (atom0->size_at *
					      cos(atom0->angle)) -
				(int) (2 * atom0->size_at * sin(atom0->angle));
			xy[1].x = x0 + (int) (2 * atom0->size_at *
					      cos(atom0->angle)) -
				(int) (atom0->size_at * sin(atom0->angle));
			xy[1].y = y0 - (int) (atom0->size_at *
					      cos(atom0->angle)) -
				(int) (2 * atom0->size_at * sin(atom0->angle));
			xy[2].x = x0 - (int) (2 * atom0->size_at *
					      cos(atom0->angle)) -
				(int) (atom0->size_at * sin(atom0->angle));
			xy[2].y = y0 - (int) (atom0->size_at *
					      cos(atom0->angle)) +
				(int) (2 * atom0->size_at * sin(atom0->angle));
			xy[3].x = x0 - (int) (2 * atom0->size_at *
					      cos(atom0->angle)) +
				(int) (atom0->size_at * sin(atom0->angle));
			xy[3].y = y0 + (int) (atom0->size_at *
					      cos(atom0->angle)) +
				(int) (2 * atom0->size_at *
				       sin(atom0->angle));
			xy[4].x = xy[0].x;
			xy[4].y = xy[0].y;
			trans_coor(xy, atom0->xy, 4, gamma);
			return;
		case 1:	/* squares */
			xy[0].x = x0 + (int) (1.5 * atom0->size_at *
					      cos(atom0->angle)) +
				(int) (1.5 * atom0->size_at *
				       sin(atom0->angle));
			xy[0].y = y0 + (int) (1.5 * atom0->size_at *
					      cos(atom0->angle)) -
				(int) (1.5 * atom0->size_at *
				       sin(atom0->angle));
			xy[1].x = x0 + (int) (1.5 * atom0->size_at *
					      cos(atom0->angle)) -
				(int) (1.5 * atom0->size_at *
				       sin(atom0->angle));
			xy[1].y = y0 - (int) (1.5 * atom0->size_at *
					      cos(atom0->angle)) -
				(int) (1.5 * atom0->size_at *
				       sin(atom0->angle));
			xy[2].x = x0 - (int) (1.5 * atom0->size_at *
					      cos(atom0->angle)) -
				(int) (1.5 * atom0->size_at *
				       sin(atom0->angle));
			xy[2].y = y0 - (int) (1.5 * atom0->size_at *
					      cos(atom0->angle)) +
				(int) (1.5 * atom0->size_at *
				       sin(atom0->angle));
			xy[3].x = x0 - (int) (1.5 * atom0->size_at *
					      cos(atom0->angle)) +
				(int) (1.5 * atom0->size_at *
				       sin(atom0->angle));
			xy[3].y = y0 + (int) (1.5 * atom0->size_at *
					      cos(atom0->angle)) +
				(int) (1.5 * atom0->size_at *
				       sin(atom0->angle));
			xy[4].x = xy[0].x;
			xy[4].y = xy[0].y;
			trans_coor(xy, atom0->xy, 4, gamma);
			return;
		case 2:	/* triangles */
			xy[0].x = x0 + (int) (1.5 * atom0->size_at *
					      sin(atom0->angle));
			xy[0].y = y0 + (int) (1.5 * atom0->size_at *
					      cos(atom0->angle));
			xy[1].x = x0 + (int) (1.5 * atom0->size_at *
					      cos(atom0->angle)) -
				(int) (1.5 * atom0->size_at *
				       sin(atom0->angle));
			xy[1].y = y0 - (int) (1.5 * atom0->size_at *
					      cos(atom0->angle)) -
				(int) (1.5 * atom0->size_at *
				       sin(atom0->angle));
			xy[2].x = x0 - (int) (1.5 * atom0->size_at *
					      cos(atom0->angle)) -
				(int) (1.5 * atom0->size_at *
				       sin(atom0->angle));
			xy[2].y = y0 - (int) (1.5 * atom0->size_at *
					      cos(atom0->angle)) +
				(int) (1.5 * atom0->size_at *
				       sin(atom0->angle));
			xy[3].x = xy[0].x;
			xy[3].y = xy[0].y;
			trans_coor(xy, atom0->xy, 3, gamma);
			return;
	}
}

static void
crystal_drawatom(ModeInfo * mi, crystalatom * atom0)
{
	crystalstruct *cryst;
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         j, k, l, m;

	cryst = &crystals[MI_SCREEN(mi)];
	for (j = numops[2 * cryst->planegroup + 1];
	     j < numops[2 * cryst->planegroup]; j++) {
		XPoint      xy[5], new_xy[5];
		XPoint      xy_1[5];
		int         xtrans, ytrans;

		xtrans = operation[j * 6] * atom0->x0 + operation[j * 6 + 1] *
			atom0->y0 + (int) (operation[j * 6 + 4] * cryst->a /
					   2.0);
		ytrans = operation[j * 6 + 2] * atom0->x0 + operation[j * 6 +
			       3] * atom0->y0 + (int) (operation[j * 6 + 5] *
						       cryst->b / 2.0);
		if (xtrans < 0) {
			if (xtrans < -cryst->a)
				xtrans = 2 * cryst->a;
			else
				xtrans = cryst->a;
		} else if (xtrans >= cryst->a)
			xtrans = -cryst->a;
		else
			xtrans = 0;
		if (ytrans < 0)
			ytrans = cryst->b;
		else if (ytrans >= cryst->b)
			ytrans = -cryst->b;
		else
			ytrans = 0;
		for (k = 0; k < atom0->num_point; k++) {
			xy[k].x = operation[j * 6] * atom0->xy[k].x +
				operation[j * 6 + 1] *
				atom0->xy[k].y + (int) (operation[j * 6 + 4] *
							cryst->a / 2.0) +
				xtrans;
			xy[k].y = operation[j * 6 + 2] * atom0->xy[k].x +
				operation[j * 6 + 3] *
				atom0->xy[k].y + (int) (operation[j * 6 + 5] *
							cryst->b / 2.0) +
				ytrans;
		}
		xy[atom0->num_point].x = xy[0].x;
		xy[atom0->num_point].y = xy[0].y;
		for (l = 0; l < cryst->nx; l++) {
			for (m = 0; m < cryst->ny; m++) {

				for (k = 0; k <= atom0->num_point; k++) {
					xy_1[k].x = xy[k].x + l * cryst->a;
					xy_1[k].y = xy[k].y + m * cryst->b;
				}
				trans_coor_back(xy_1, new_xy, atom0->num_point,
						cryst->gamma, cryst->offset_w,
						cryst->offset_h ,
						cryst->win_height,
						cryst->invert);
				XFillPolygon(display, window, cryst->gc, new_xy,
				  atom0->num_point, Convex, CoordModeOrigin);
			}
		}
		if (centro[cryst->planegroup] == True) {
			for (k = 0; k <= atom0->num_point; k++) {
				xy[k].x = cryst->a - xy[k].x;
				xy[k].y = cryst->b - xy[k].y;
			}
			for (l = 0; l < cryst->nx; l++) {
				for (m = 0; m < cryst->ny; m++) {

					for (k = 0; k <= atom0->num_point; k++) {
						xy_1[k].x = xy[k].x + l * cryst->a;
						xy_1[k].y = xy[k].y + m * cryst->b;
					}
					trans_coor_back(xy_1, new_xy, atom0->num_point,
							cryst->gamma,
							cryst->offset_w,
							cryst->offset_h ,
							cryst->win_height ,
							cryst->invert);
					XFillPolygon(display, window, cryst->gc,
						     new_xy,
						     atom0->num_point, Convex,
						     CoordModeOrigin);
				}
			}
		}
		if (primitive[cryst->planegroup] == False) {
			if (xy[atom0->num_point].x >= (int) (cryst->a / 2.0))
				xtrans = (int) (-cryst->a / 2.0);
			else
				xtrans = (int) (cryst->a / 2.0);
			if (xy[atom0->num_point].y >= (int) (cryst->b / 2.0))
				ytrans = (int) (-cryst->b / 2.0);
			else
				ytrans = (int) (cryst->b / 2.0);
			for (k = 0; k <= atom0->num_point; k++) {
				xy[k].x = xy[k].x + xtrans;
				xy[k].y = xy[k].y + ytrans;
			}
			for (l = 0; l < cryst->nx; l++) {
				for (m = 0; m < cryst->ny; m++) {

					for (k = 0; k <= atom0->num_point; k++) {
						xy_1[k].x = xy[k].x + l * cryst->a;
						xy_1[k].y = xy[k].y + m * cryst->b;
					}
					trans_coor_back(xy_1, new_xy, atom0->num_point,
							cryst->gamma,
							cryst->offset_w,
							cryst->offset_h ,
							cryst->win_height,
							cryst->invert);
					XFillPolygon(display, window, cryst->gc,
						     new_xy,
						     atom0->num_point, Convex,
						     CoordModeOrigin);
				}
			}
			if (centro[cryst->planegroup] == True) {
				XPoint      xy1[5];

				for (k = 0; k <= atom0->num_point; k++) {
					xy1[k].x = cryst->a - xy[k].x;
					xy1[k].y = cryst->b - xy[k].y;
				}
				for (l = 0; l < cryst->nx; l++) {
					for (m = 0; m < cryst->ny; m++) {

						for (k = 0; k <= atom0->num_point; k++) {
							xy_1[k].x = xy1[k].x + l * cryst->a;
							xy_1[k].y = xy1[k].y + m * cryst->b;
						}
						trans_coor_back(xy_1, new_xy, atom0->num_point,
								cryst->gamma,
								cryst->offset_w,
								cryst->offset_h ,
								cryst->win_height,
								cryst->invert);
						XFillPolygon(display, window,
							     cryst->gc,
						    new_xy, atom0->num_point,
						    Convex, CoordModeOrigin);
					}
				}
			}
		}
	}
}

ENTRYPOINT void init_crystal(ModeInfo * mi);
ENTRYPOINT void release_crystal(ModeInfo * mi);


ENTRYPOINT void
draw_crystal(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	crystalstruct *cryst = &crystals[MI_SCREEN(mi)];
	int         i;

#ifdef HAVE_COCOA	/* Don't second-guess Quartz's double-buffering */
    XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));
#endif

	if (cryst->no_colors) {
		release_crystal(mi);
		init_crystal(mi);
		return;
	}
	cryst->painted = True;
	MI_IS_DRAWN(mi) = True;
	XSetFunction(display, cryst->gc, GXxor);

/* Rotate colours */
	if (cryst->cycle_p) {
		rotate_colors(display, cryst->cmap, cryst->colors, cryst->ncolors,
			      cryst->direction);
		if (!(LRAND() % 1000))
			cryst->direction = -cryst->direction;
	}
	for (i = 0; i < cryst->num_atom; i++) {
		crystalatom *atom0;

		atom0 = &cryst->atom[i];

		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XSetForeground(display, cryst->gc, cryst->colors[atom0->colour].pixel);
		} else {
			XSetForeground(display, cryst->gc, atom0->colour);
		}
		crystal_drawatom(mi, atom0);
		atom0->velocity[0] += NRAND(3) - 1;
		atom0->velocity[0] = MAX(-20, MIN(20, atom0->velocity[0]));
		atom0->velocity[1] += NRAND(3) - 1;
		atom0->velocity[1] = MAX(-20, MIN(20, atom0->velocity[1]));
		atom0->x0 += atom0->velocity[0];
		/*if (cryst->gamma == 90.0) { */
		if (atom0->x0 < 0)
			atom0->x0 += cryst->a;
		else if (atom0->x0 >= cryst->a)
			atom0->x0 -= cryst->a;
		atom0->y0 += atom0->velocity[1];
		if (atom0->y0 < 0)
			atom0->y0 += cryst->b;
		else if (atom0->y0 >= cryst->b)
			atom0->y0 -= cryst->b;
		/*} */
		atom0->velocity_a += ((float) NRAND(1001) - 500.0) / 2000.0;
		atom0->angle += atom0->velocity_a;
		crystal_setupatom(atom0, cryst->gamma);
		crystal_drawatom(mi, atom0);
	}
	XSetFunction(display, cryst->gc, GXcopy);
}

ENTRYPOINT void
refresh_crystal(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	crystalstruct *cryst = &crystals[MI_SCREEN(mi)];
	int         i;

	if (!cryst->painted)
		return;
	MI_CLEARWINDOW(mi);
	XSetFunction(display, cryst->gc, GXxor);

	if (cryst->unit_cell) {
	   int y_coor1 , y_coor2;
	   
		if (MI_NPIXELS(mi) > 2)
			XSetForeground(display, cryst->gc, MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))));
		else
			XSetForeground(display, cryst->gc, MI_WHITE_PIXEL(mi));
		if (cryst->grid_cell) {
			int         inx, iny;

		   if ( cryst->invert )
		     y_coor1 = y_coor2 = cryst->win_height - cryst->offset_h;
		   else
		     y_coor1 = y_coor2 = cryst->offset_h;
			XDrawLine(display, window, cryst->gc, cryst->offset_w,
				  y_coor1, cryst->offset_w + cryst->nx * cryst->a,
				  y_coor2);
		   if ( cryst->invert )
		     {
			y_coor1 = cryst->win_height - cryst->offset_h;
			y_coor2 = cryst->win_height - (int) (cryst->ny *
							     cryst->b *
					 cos((cryst->gamma - 90) * PI_RAD)) -
			  cryst->offset_h;
		     }
		   else
		     {
			y_coor1 = cryst->offset_h;
			y_coor2 = (int) (cryst->ny * cryst->b *
					 cos((cryst->gamma - 90) * PI_RAD)) +
			  cryst->offset_h;
		     }
			XDrawLine(display, window, cryst->gc, cryst->offset_w,
				  y_coor1, (int) (cryst->offset_w - cryst->ny * cryst->b *
					  sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor2);
			inx = cryst->nx;
			for (iny = 1; iny <= cryst->ny; iny++) {
		   if ( cryst->invert )
		     {
			y_coor1 = cryst->win_height -
			  (int) (iny * cryst->b * cos((cryst->gamma - 90) *
						  PI_RAD)) - cryst->offset_h;
			y_coor2 = cryst->win_height -
			  (int) (iny * cryst->b * cos((cryst->gamma - 90) *
						      PI_RAD)) -
			  cryst->offset_h;
		     }
		   else
		     {
			y_coor1 = (int) (iny * cryst->b * cos((cryst->gamma - 90) *
						  PI_RAD)) + cryst->offset_h;
			y_coor2 = (int) (iny * cryst->b * cos((cryst->gamma - 90) * PI_RAD)) +
					  cryst->offset_h;
		     }
				XDrawLine(display, window, cryst->gc,
					  (int) (cryst->offset_w +
				     inx * cryst->a - (int) (iny * cryst->b *
					 sin((cryst->gamma - 90) * PI_RAD))),
					  y_coor1,
				    (int) (cryst->offset_w - iny * cryst->b *
					   sin((cryst->gamma - 90) * PI_RAD)),
					  y_coor2);
			}
			iny = cryst->ny;
			for (inx = 1; inx <= cryst->nx; inx++) {
			   if ( cryst->invert )
			     {
				y_coor1 =cryst->win_height -
				  (int) (iny * cryst->b *
						cos((cryst->gamma - 90) *
						    PI_RAD)) - cryst->offset_h;
				y_coor2 =cryst->win_height - cryst->offset_h;
			     }
			   else
			     {
				y_coor1 =(int) (iny * cryst->b *
						cos((cryst->gamma - 90) *
						    PI_RAD)) + cryst->offset_h;
				y_coor2 =cryst->offset_h;
			     }
				XDrawLine(display, window, cryst->gc,
					  (int) (cryst->offset_w +
				     inx * cryst->a - (int) (iny * cryst->b *
					 sin((cryst->gamma - 90) * PI_RAD))),
					  y_coor1,
					  cryst->offset_w + inx * cryst->a,
					  y_coor2);
			}
		} else {
			int         inx, iny;

			inx = NRAND(cryst->nx);
			iny = NRAND(cryst->ny);
		   if ( cryst->invert )
		     {
			y_coor1 =cryst->win_height -
			  (int) (iny * cryst->b *
						  cos((cryst->gamma - 90) *
						      PI_RAD)) -
			  cryst->offset_h;
			y_coor2 =cryst->win_height -
			  (int) ( ( iny + 1 ) * cryst->b *
						  cos((cryst->gamma - 90) *
						      PI_RAD)) -
			  cryst->offset_h;
		     }
		   else
		     {
			y_coor1 =(int) (iny * cryst->b *
						  cos((cryst->gamma - 90) *
						      PI_RAD)) +
			  cryst->offset_h;
			y_coor2 =(int) (( iny + 1 ) * cryst->b *
						  cos((cryst->gamma - 90) *
						      PI_RAD)) +
			  cryst->offset_h;
		     }
			XDrawLine(display, window, cryst->gc,
				  cryst->offset_w + inx * cryst->a - (int) (iny * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor1,
				  cryst->offset_w + (inx + 1) * cryst->a - (int) (iny * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor1);
			XDrawLine(display, window, cryst->gc,
				  cryst->offset_w + inx * cryst->a - (int) (iny * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor1,
				  cryst->offset_w + inx * cryst->a - (int) ((iny + 1) * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor2);
			XDrawLine(display, window, cryst->gc,
				  cryst->offset_w + (inx + 1) * cryst->a - (int) (iny * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor1,
				  cryst->offset_w + (inx + 1) * cryst->a - (int) ((iny + 1) * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor2);
			XDrawLine(display, window, cryst->gc,
				  cryst->offset_w + inx * cryst->a - (int) ((iny + 1) * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor2,
				  cryst->offset_w + (inx + 1) * cryst->a - (int) ((iny + 1) * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor2);
		}
	}
	for (i = 0; i < cryst->num_atom; i++) {
		crystalatom *atom0;

		atom0 = &cryst->atom[i];
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XSetForeground(display, cryst->gc, cryst->colors[atom0->colour].pixel);
		} else {
			XSetForeground(display, cryst->gc, atom0->colour);
		}
		crystal_drawatom(mi, atom0);
	}
	XSetFunction(display, cryst->gc, GXcopy);
}

ENTRYPOINT void
release_crystal(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);

	if (crystals != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			crystalstruct *cryst = &crystals[screen];

			if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
				MI_WHITE_PIXEL(mi) = cryst->whitepixel;
				MI_BLACK_PIXEL(mi) = cryst->blackpixel;
#ifndef STANDALONE
				MI_FG_PIXEL(mi) = cryst->fg;
				MI_BG_PIXEL(mi) = cryst->bg;
#endif
				if (cryst->colors && cryst->ncolors && !cryst->no_colors)
					free_colors(display, cryst->cmap, cryst->colors, cryst->ncolors);
				if (cryst->colors)
					(void) free((void *) cryst->colors);
#if 0 /* #### wrong! -jwz */
				XFreeColormap(display, cryst->cmap);
#endif
			}
			if (cryst->gc != NULL)
				XFreeGC(display, cryst->gc);
			if (cryst->atom != NULL)
				(void) free((void *) cryst->atom);
		}
		(void) free((void *) crystals);
		crystals = NULL;
	}
}

ENTRYPOINT void
init_crystal(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	crystalstruct *cryst;
	int         i, max_atoms, size_atom, neqv;
	int         cell_min;

#define MIN_CELL 200

/* initialize */
	if (crystals == NULL) {
		if ((crystals = (crystalstruct *) calloc(MI_NUM_SCREENS(mi),
					    sizeof (crystalstruct))) == NULL)
			return;
	}
	cryst = &crystals[MI_SCREEN(mi)];

	if (!cryst->gc) {
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XColor      color;

#ifndef STANDALONE
			extern char *background;
			extern char *foreground;

			cryst->fg = MI_FG_PIXEL(mi);
			cryst->bg = MI_BG_PIXEL(mi);
#endif
			cryst->blackpixel = MI_BLACK_PIXEL(mi);
			cryst->whitepixel = MI_WHITE_PIXEL(mi);
#if 0 /* #### wrong! -jwz */
			cryst->cmap = XCreateColormap(display, window,
						   MI_VISUAL(mi), AllocNone);
			XSetWindowColormap(display, window, cryst->cmap);
#else
            cryst->cmap = mi->xgwa.colormap;
#endif
			(void) XParseColor(display, cryst->cmap, "black", &color);
			(void) XAllocColor(display, cryst->cmap, &color);
			MI_BLACK_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, cryst->cmap, "white", &color);
			(void) XAllocColor(display, cryst->cmap, &color);
			MI_WHITE_PIXEL(mi) = color.pixel;
#ifndef STANDALONE
			(void) XParseColor(display, cryst->cmap, background, &color);
			(void) XAllocColor(display, cryst->cmap, &color);
			MI_BG_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, cryst->cmap, foreground, &color);
			(void) XAllocColor(display, cryst->cmap, &color);
			MI_FG_PIXEL(mi) = color.pixel;
#endif
			cryst->colors = 0;
			cryst->ncolors = 0;
		}
		if ((cryst->gc = XCreateGC(display, MI_WINDOW(mi),
			     (unsigned long) 0, (XGCValues *) NULL)) == None)
			return;
	}
/* Clear Display */
	MI_CLEARWINDOW(mi);
	cryst->painted = False;
	XSetFunction(display, cryst->gc, GXxor);


/*Set up crystal data */
	cryst->direction = (LRAND() & 1) ? 1 : -1;
	if (MI_IS_FULLRANDOM(mi)) {
		if (LRAND() & 1)
			cryst->unit_cell = True;
		else
			cryst->unit_cell = False;
	} else
		cryst->unit_cell = unit_cell;
	if (cryst->unit_cell) {
		if (MI_IS_FULLRANDOM(mi)) {
			if (LRAND() & 1)
				cryst->grid_cell = True;
			else
				cryst->grid_cell = False;
		} else
			cryst->grid_cell = grid_cell;
	}
	cryst->win_width = MI_WIDTH(mi);
	cryst->win_height = MI_HEIGHT(mi);
	cell_min = min(cryst->win_width / 2 + 1, MIN_CELL);
	cell_min = min(cell_min, cryst->win_height / 2 + 1);
	cryst->planegroup = NRAND(17);
        cryst->invert = NRAND(2);
	if (MI_IS_VERBOSE(mi))
		(void) fprintf(stdout, "Selected plane group no %d\n",
			       cryst->planegroup + 1);
	if (cryst->planegroup > 11)
		cryst->gamma = 120.0;
	else if (cryst->planegroup < 2)
		cryst->gamma = 60.0 + NRAND(60);
	else
		cryst->gamma = 90.0;
	neqv = numops[2 * cryst->planegroup] - numops[2 * cryst->planegroup + 1];
	if (centro[cryst->planegroup] == True)
		neqv = 2 * neqv;
	if (primitive[cryst->planegroup] == False)
		neqv = 2 * neqv;


	if (nx > 0)
		cryst->nx = nx;
	else if (nx < 0)
		cryst->nx = NRAND(-nx) + 1;
	else
		cryst->nx = DEF_NX1;
	if (cryst->planegroup > 8)
		cryst->ny = cryst->nx;
	else if (ny > 0)
		cryst->ny = ny;
	else if (ny < 0)
		cryst->ny = NRAND(-ny) + 1;
	else
		cryst->ny = DEF_NY1;
	neqv = neqv * cryst->nx * cryst->ny;

	cryst->num_atom = MI_COUNT(mi);
	max_atoms = MI_COUNT(mi);
	if (cryst->num_atom == 0) {
		cryst->num_atom = DEF_NUM_ATOM;
		max_atoms = DEF_NUM_ATOM;
	} else if (cryst->num_atom < 0) {
		max_atoms = -cryst->num_atom;
		cryst->num_atom = NRAND(-cryst->num_atom) + 1;
	}
	if (neqv > 1)
		cryst->num_atom = cryst->num_atom / neqv + 1;

	if (cryst->atom == NULL)
		cryst->atom = (crystalatom *) calloc(max_atoms, sizeof (
							       crystalatom));

	if (maxsize) {
		if (cryst->planegroup < 13) {
			cryst->gamma = 90.0;
			cryst->offset_w = 0;
			cryst->offset_h = 0;
			if (cryst->planegroup < 10) {
				cryst->b = cryst->win_height;
				cryst->a = cryst->win_width;
			} else {
				cryst->b = min(cryst->win_height, cryst->win_width);
				cryst->a = cryst->b;
			}
		} else {
			cryst->gamma = 120.0;
			cryst->a = (int) (cryst->win_width * 2.0 / 3.0);
			cryst->b = cryst->a;
			cryst->offset_h = (int) (cryst->b * 0.25 *
					  cos((cryst->gamma - 90) * PI_RAD));
			cryst->offset_w = (int) (cryst->b * 0.5);
		}
	} else {
		int max_repeat = 10;
		cryst->offset_w = -1;
		while (max_repeat-- && 
		  (cryst->offset_w < 4 || (int) (cryst->offset_w - cryst->b *
				    sin((cryst->gamma - 90) * PI_RAD)) < 4)
			   ) {
			cryst->b = NRAND((int) (cryst->win_height / (cos((cryst->gamma - 90) *
					    PI_RAD))) - cell_min) + cell_min;
			if (cryst->planegroup > 8)
				cryst->a = cryst->b;
			else
				cryst->a = NRAND(cryst->win_width - cell_min) + cell_min;
			cryst->offset_w = (int) ((cryst->win_width - (cryst->a - cryst->b *
						    sin((cryst->gamma - 90) *
							PI_RAD))) / 2.0);
		}
		cryst->offset_h = (int) ((cryst->win_height - cryst->b * cos((
					cryst->gamma - 90) * PI_RAD)) / 2.0);
		if (!centre) {
			if (cryst->offset_h > 0)
				cryst->offset_h = NRAND(2 * cryst->offset_h);
			cryst->offset_w = (int) (cryst->win_width - cryst->a -
						 cryst->b *
				    fabs(sin((cryst->gamma - 90) * PI_RAD)));
			if (cryst->gamma > 90.0) {
				if (cryst->offset_w > 0)
					cryst->offset_w = NRAND(cryst->offset_w) +
						(int) (cryst->b * sin((cryst->gamma - 90) * PI_RAD));
				else
					cryst->offset_w = (int) (cryst->b * sin((cryst->gamma - 90) *
								    PI_RAD));
			} else if (cryst->offset_w > 0)
				cryst->offset_w = NRAND(cryst->offset_w);
			else
				cryst->offset_w = 0;
		}
	}

	size_atom = min((int) ((float) (cryst->a) / 40.) + 1,
			(int) ((float) (cryst->b) / 40.) + 1);
	if (MI_SIZE(mi) < size_atom) {
		if (MI_SIZE(mi) < -size_atom)
			size_atom = -size_atom;
		else
			size_atom = MI_SIZE(mi);
	}
	cryst->a = cryst->a / cryst->nx;
	cryst->b = cryst->b / cryst->ny;
	if (cryst->unit_cell) {
	   int y_coor1 , y_coor2;
	   
		if (MI_NPIXELS(mi) > 2)
			XSetForeground(display, cryst->gc, MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))));
		else
			XSetForeground(display, cryst->gc, MI_WHITE_PIXEL(mi));
		if (cryst->grid_cell) {
			int         inx, iny;

		   if ( cryst->invert )
		     y_coor1 = y_coor2 = cryst->win_height - cryst->offset_h;
		   else
		     y_coor1 = y_coor2 = cryst->offset_h;
			XDrawLine(display, window, cryst->gc, cryst->offset_w,
				  y_coor1, cryst->offset_w + cryst->nx * cryst->a,
				  y_coor2);
		   if ( cryst->invert )
		     {
			y_coor1 = cryst->win_height - cryst->offset_h;
			y_coor2 = cryst->win_height - (int) (cryst->ny *
							     cryst->b *
					 cos((cryst->gamma - 90) * PI_RAD)) -
			  cryst->offset_h;
		     }
		   else
		     {
			y_coor1 = cryst->offset_h;
			y_coor2 = (int) (cryst->ny * cryst->b *
					 cos((cryst->gamma - 90) * PI_RAD)) +
			  cryst->offset_h;
		     }
			XDrawLine(display, window, cryst->gc, cryst->offset_w,
				  y_coor1, (int) (cryst->offset_w - cryst->ny * cryst->b *
					  sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor2);
			inx = cryst->nx;
			for (iny = 1; iny <= cryst->ny; iny++) {
		   if ( cryst->invert )
		     {
			y_coor1 = cryst->win_height -
			  (int) (iny * cryst->b * cos((cryst->gamma - 90) *
						  PI_RAD)) - cryst->offset_h;
			y_coor2 = cryst->win_height -
			  (int) (iny * cryst->b * cos((cryst->gamma - 90) *
						      PI_RAD)) -
			  cryst->offset_h;
		     }
		   else
		     {
			y_coor1 = (int) (iny * cryst->b * cos((cryst->gamma - 90) *
						  PI_RAD)) + cryst->offset_h;
			y_coor2 = (int) (iny * cryst->b * cos((cryst->gamma - 90) * PI_RAD)) +
					  cryst->offset_h;
		     }
				XDrawLine(display, window, cryst->gc,
					  (int) (cryst->offset_w +
				     inx * cryst->a - (int) (iny * cryst->b *
					 sin((cryst->gamma - 90) * PI_RAD))),
					  y_coor1,
				    (int) (cryst->offset_w - iny * cryst->b *
					   sin((cryst->gamma - 90) * PI_RAD)),
					  y_coor2);
			}
			iny = cryst->ny;
			for (inx = 1; inx <= cryst->nx; inx++) {
			   if ( cryst->invert )
			     {
				y_coor1 =cryst->win_height -
				  (int) (iny * cryst->b *
						cos((cryst->gamma - 90) *
						    PI_RAD)) - cryst->offset_h;
				y_coor2 =cryst->win_height - cryst->offset_h;
			     }
			   else
			     {
				y_coor1 =(int) (iny * cryst->b *
						cos((cryst->gamma - 90) *
						    PI_RAD)) + cryst->offset_h;
				y_coor2 =cryst->offset_h;
			     }
				XDrawLine(display, window, cryst->gc,
					  (int) (cryst->offset_w +
				     inx * cryst->a - (int) (iny * cryst->b *
					 sin((cryst->gamma - 90) * PI_RAD))),
					  y_coor1,
					  cryst->offset_w + inx * cryst->a,
					  y_coor2);
			}
		} else {
			int         inx, iny;

			inx = NRAND(cryst->nx);
			iny = NRAND(cryst->ny);
		   if ( cryst->invert )
		     {
			y_coor1 =cryst->win_height -
			  (int) (iny * cryst->b *
						  cos((cryst->gamma - 90) *
						      PI_RAD)) -
			  cryst->offset_h;
			y_coor2 =cryst->win_height -
			  (int) ( ( iny + 1 ) * cryst->b *
						  cos((cryst->gamma - 90) *
						      PI_RAD)) -
			  cryst->offset_h;
		     }
		   else
		     {
			y_coor1 =(int) (iny * cryst->b *
						  cos((cryst->gamma - 90) *
						      PI_RAD)) +
			  cryst->offset_h;
			y_coor2 =(int) (( iny + 1 ) * cryst->b *
						  cos((cryst->gamma - 90) *
						      PI_RAD)) +
			  cryst->offset_h;
		     }
			XDrawLine(display, window, cryst->gc,
				  cryst->offset_w + inx * cryst->a - (int) (iny * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor1,
				  cryst->offset_w + (inx + 1) * cryst->a - (int) (iny * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor1);
			XDrawLine(display, window, cryst->gc,
				  cryst->offset_w + inx * cryst->a - (int) (iny * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor1,
				  cryst->offset_w + inx * cryst->a - (int) ((iny + 1) * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor2);
			XDrawLine(display, window, cryst->gc,
				  cryst->offset_w + (inx + 1) * cryst->a - (int) (iny * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor1,
				  cryst->offset_w + (inx + 1) * cryst->a - (int) ((iny + 1) * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor2);
			XDrawLine(display, window, cryst->gc,
				  cryst->offset_w + inx * cryst->a - (int) ((iny + 1) * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor2,
				  cryst->offset_w + (inx + 1) * cryst->a - (int) ((iny + 1) * cryst->b * sin((cryst->gamma - 90) * PI_RAD)),
				  y_coor2);
		}
	}
	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
/* Set up colour map */
		if (cryst->colors && cryst->ncolors && !cryst->no_colors)
			free_colors(display, cryst->cmap, cryst->colors, cryst->ncolors);
		if (cryst->colors)
			(void) free((void *) cryst->colors);
		cryst->colors = 0;
		cryst->ncolors = MI_NCOLORS(mi);
		if (cryst->ncolors < 2)
			cryst->ncolors = 2;
		if (cryst->ncolors <= 2)
			cryst->mono_p = True;
		else
			cryst->mono_p = False;

		if (cryst->mono_p)
			cryst->colors = 0;
		else
			cryst->colors = (XColor *) malloc(sizeof (*cryst->colors) * (cryst->ncolors + 1));
		cryst->cycle_p = has_writable_cells(mi->xgwa.screen, MI_VISUAL(mi));
		if (cryst->cycle_p) {
			if (MI_IS_FULLRANDOM(mi)) {
				if (!NRAND(8))
					cryst->cycle_p = False;
				else
					cryst->cycle_p = True;
			} else {
				cryst->cycle_p = cycle_p;
			}
		}
		if (!cryst->mono_p) {
			if (!(LRAND() % 10))
				make_random_colormap(MI_DISPLAY(mi), MI_VISUAL(mi), cryst->cmap, cryst->colors, &cryst->ncolors,
						True, True, &cryst->cycle_p, True);
			else if (!(LRAND() % 2))
				make_uniform_colormap(MI_DISPLAY(mi), MI_VISUAL(mi), cryst->cmap, cryst->colors, &cryst->ncolors,
						      True, &cryst->cycle_p, True);
			else
				make_smooth_colormap(MI_DISPLAY(mi), MI_VISUAL(mi), cryst->cmap, cryst->colors, &cryst->ncolors,
						     True, &cryst->cycle_p, True);
		}
#if 0 /* #### wrong! -jwz */
		XInstallColormap(display, cryst->cmap);
#endif
		if (cryst->ncolors < 2) {
			cryst->ncolors = 2;
			cryst->no_colors = True;
		} else
			cryst->no_colors = False;
		if (cryst->ncolors <= 2)
			cryst->mono_p = True;

		if (cryst->mono_p)
			cryst->cycle_p = False;

	}
	for (i = 0; i < cryst->num_atom; i++) {
		crystalatom *atom0;

		atom0 = &cryst->atom[i];
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			if (cryst->ncolors > 2)
				atom0->colour = NRAND(cryst->ncolors - 2) + 2;
			else
				atom0->colour = 1;	/* Just in case */
			XSetForeground(display, cryst->gc, cryst->colors[atom0->colour].pixel);
		} else {
			if (MI_NPIXELS(mi) > 2)
				atom0->colour = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
			else
				atom0->colour = 1;	/*Xor'red so WHITE may not be appropriate */
			XSetForeground(display, cryst->gc, atom0->colour);
		}
		atom0->x0 = NRAND(cryst->a);
		atom0->y0 = NRAND(cryst->b);
		atom0->velocity[0] = NRAND(7) - 3;
		atom0->velocity[1] = NRAND(7) - 3;
		atom0->velocity_a = (NRAND(7) - 3) * PI_RAD;
		atom0->angle = NRAND(90) * PI_RAD;
		atom0->at_type = NRAND(3);
		if (size_atom == 0)
			atom0->size_at = DEF_SIZ_ATOM;
		else if (size_atom > 0)
			atom0->size_at = size_atom;
		else
			atom0->size_at = NRAND(-size_atom) + 1;
		atom0->size_at++;
		if (atom0->at_type == 2)
			atom0->num_point = 3;
		else
			atom0->num_point = 4;
		crystal_setupatom(atom0, cryst->gamma);
		crystal_drawatom(mi, atom0);
	}
	XSetFunction(display, cryst->gc, GXcopy);
}

XSCREENSAVER_MODULE ("Crystal", crystal)
