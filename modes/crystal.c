
/* -*- Mode: C; tab-width: 4 -*- */
/* crystal --- polygons moving according to plane group rules */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)crystal.c	4.05 97/09/19 xlockmore";

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
 * Have not coded non-orthogonal unit cells.
 *
 * Revision History:
 * 19-Sep-97: Added remaining hexagonal groups
 * 12-Jun-97: Created
 */

#ifdef STANDALONE
#define PROGCLASS "Crystal"
#define HACK_INIT init_crystal
#define HACK_DRAW draw_crystal
#define crystal_opts xlockmore_opts
#define DEFAULTS "*delay: 60000 \n" \
 "*count: -40 \n" \
 "*cycles: 200 \n" \
 "*size: -15 \n" \
 "*ncolors: 200 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */

ModeSpecOpt crystal_opts =
{0, NULL, 0, NULL, NULL};

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
	int         win_width, win_height, num_atom;
	int         planegroup, a, b;
	float       gamma;
	crystalatom *atom;
	GC          gc;
} crystalstruct;

static crystalstruct *crystals = NULL;

static void 
trans_coor(XPoint * xyp, XPoint * new_xyp, int num_points,
	   float gamma)
{
	int         i;

	for (i = 0; i <= num_points; i++) {
		new_xyp[i].x = xyp[i].x + xyp[i].y * sin((gamma - 90.0) * PI_RAD);
		new_xyp[i].y = xyp[i].y / cos((gamma - 90.0) * PI_RAD);
	}
}

static void 
trans_coor_back(XPoint * xyp, XPoint * new_xyp,
		int num_points, float gamma, int width,
		int height)
{
	int         i, xtrans, ytrans;

	ytrans = xyp[0].y * cos((gamma - 90) * PI_RAD);
	xtrans = xyp[0].x - ytrans * sin((gamma - 90.0) * PI_RAD);
	if (xtrans < 0)
     {
	if ( xtrans < -width )
	  xtrans = 2 * width;
	else
	  xtrans = width;
     }
	else
		xtrans = 0;
	if (ytrans < 0)
		ytrans = height;
	else
		ytrans = 0;

	for (i = 0; i <= num_points; i++) {
		new_xyp[i].y = xyp[i].y * cos((gamma - 90) * PI_RAD) +
	     ytrans;
		new_xyp[i].x = xyp[i].x - xyp[i].y * sin((gamma - 90.0) * PI_RAD) +
			xtrans;
	}
}

static void
crystal_setupatom(crystalatom * atom0, float gamma)
{
	XPoint      xy[5];

	switch (atom0->at_type) {
		case 0:	/* rectangles */
			xy[0].x = atom0->x0 + 2 * atom0->size_at * cos(atom0->angle) +
				atom0->size_at * sin(atom0->angle);
			xy[0].y = atom0->y0 + atom0->size_at * cos(atom0->angle) - 2 *
				atom0->size_at * sin(atom0->angle);
			xy[1].x = atom0->x0 + 2 * atom0->size_at * cos(atom0->angle) -
				atom0->size_at * sin(atom0->angle);
			xy[1].y = atom0->y0 - atom0->size_at * cos(atom0->angle) - 2 *
				atom0->size_at * sin(atom0->angle);
			xy[2].x = atom0->x0 - 2 * atom0->size_at * cos(atom0->angle) -
				atom0->size_at * sin(atom0->angle);
			xy[2].y = atom0->y0 - atom0->size_at * cos(atom0->angle) + 2 *
				atom0->size_at * sin(atom0->angle);
			xy[3].x = atom0->x0 - 2 * atom0->size_at * cos(atom0->angle) +
				atom0->size_at * sin(atom0->angle);
			xy[3].y = atom0->y0 + atom0->size_at * cos(atom0->angle) + 2 *
				atom0->size_at * sin(atom0->angle);
			xy[4].x = xy[0].x;
			xy[4].y = xy[0].y;
			trans_coor(xy, atom0->xy, 4, gamma);
			return;
		case 1:	/* squares */
			xy[0].x = atom0->x0 + 1.5 * atom0->size_at * cos(atom0->angle) +
				1.5 * atom0->size_at * sin(atom0->angle);
			xy[0].y = atom0->y0 + 1.5 * atom0->size_at * cos(atom0->angle) -
				1.5 * atom0->size_at * sin(atom0->angle);
			xy[1].x = atom0->x0 + 1.5 * atom0->size_at * cos(atom0->angle) -
				1.5 * atom0->size_at * sin(atom0->angle);
			xy[1].y = atom0->y0 - 1.5 * atom0->size_at * cos(atom0->angle) -
				1.5 * atom0->size_at * sin(atom0->angle);
			xy[2].x = atom0->x0 - 1.5 * atom0->size_at * cos(atom0->angle) -
				1.5 * atom0->size_at * sin(atom0->angle);
			xy[2].y = atom0->y0 - 1.5 * atom0->size_at * cos(atom0->angle) +
				1.5 * atom0->size_at * sin(atom0->angle);
			xy[3].x = atom0->x0 - 1.5 * atom0->size_at * cos(atom0->angle) +
				1.5 * atom0->size_at * sin(atom0->angle);
			xy[3].y = atom0->y0 + 1.5 * atom0->size_at * cos(atom0->angle) +
				1.5 * atom0->size_at * sin(atom0->angle);
			xy[4].x = xy[0].x;
			xy[4].y = xy[0].y;
			trans_coor(xy, atom0->xy, 4, gamma);
			return;
		case 2:	/* triangles */
			xy[0].x = atom0->x0 + 1.5 * atom0->size_at * sin(atom0->angle);
			xy[0].y = atom0->y0 + 1.5 * atom0->size_at * cos(atom0->angle);
			xy[1].x = atom0->x0 + 1.5 * atom0->size_at * cos(atom0->angle) -
				1.5 * atom0->size_at * sin(atom0->angle);
			xy[1].y = atom0->y0 - 1.5 * atom0->size_at * cos(atom0->angle) -
				1.5 * atom0->size_at * sin(atom0->angle);
			xy[2].x = atom0->x0 - 1.5 * atom0->size_at * cos(atom0->angle) -
				1.5 * atom0->size_at * sin(atom0->angle);
			xy[2].y = atom0->y0 - 1.5 * atom0->size_at * cos(atom0->angle) +
				1.5 * atom0->size_at * sin(atom0->angle);
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
	int         j, k;

	cryst = &crystals[MI_SCREEN(mi)];
	for (j = numops[2 * cryst->planegroup + 1];
	     j < numops[2 * cryst->planegroup]; j++) {
		XPoint      xy[5], new_xy[5];
		int         xtrans, ytrans;

		xtrans = operation[j * 6] * atom0->x0 + operation[j * 6 + 1] *
			atom0->y0 + operation[j * 6 + 4] * cryst->a / 2.0;
		ytrans = operation[j * 6 + 2] * atom0->x0 + operation[j * 6 + 3] *
			atom0->y0 + operation[j * 6 + 5] * cryst->b / 2.0;
		if (xtrans < 0)
	     {
		if ( xtrans < -cryst->a )
		  xtrans = 2 * cryst->a;
		  else
		  xtrans = cryst->a;
	     }
		else if (xtrans >= cryst->a)
			xtrans = -cryst->a;
		else
			xtrans = 0;
		if (ytrans < 0)
			ytrans = cryst->b;
		else if (ytrans >= cryst->b)
			ytrans = -cryst->b;
		else
			ytrans = 0;
	   xtrans=0; ytrans=0;
		for (k = 0; k < atom0->num_point; k++) {
			xy[k].x = operation[j * 6] * atom0->xy[k].x + operation[j * 6 +
										1] * atom0->xy[k].y + operation[j * 6 + 4] * cryst->a / 2.0 +
				xtrans;
			xy[k].y = operation[j * 6 + 2] * atom0->xy[k].x + operation[j *
										    6 + 3] * atom0->xy[k].y + operation[j * 6 + 5] * cryst->b /
				2.0 + ytrans;
		}
		xy[atom0->num_point].x = xy[0].x;
		xy[atom0->num_point].y = xy[0].y;
			if (cryst->gamma != 90.0) {
			trans_coor_back(xy, new_xy, atom0->num_point,
					cryst->gamma, cryst->win_width,
					cryst->win_height);
			XFillPolygon(display, window, cryst->gc, new_xy,
				  atom0->num_point, Convex, CoordModeOrigin);
			} else
				XFillPolygon(display, window, cryst->gc, xy,
					     atom0->num_point, Convex,
					     CoordModeOrigin);
		if (centro[cryst->planegroup] == True) {
			for (k = 0; k <= atom0->num_point; k++) {
				xy[k].x = cryst->a - xy[k].x;
				xy[k].y = cryst->b - xy[k].y;
			}
			if (cryst->gamma != 90.0) {
				trans_coor_back(xy, new_xy, atom0->num_point,
					      cryst->gamma, cryst->win_width,
						cryst->win_height);
				XFillPolygon(display, window, cryst->gc, new_xy,
					     atom0->num_point, Convex,
					     CoordModeOrigin);
			} else
				XFillPolygon(display, window, cryst->gc, xy,
					     atom0->num_point, Convex,
					     CoordModeOrigin);
		}
		if (primitive[cryst->planegroup] == False) {
			if (xy[atom0->num_point].x >= cryst->a / 2.0)
				xtrans = -cryst->a / 2.0;
			else
				xtrans = cryst->a / 2.0;
			if (xy[atom0->num_point].y >= cryst->b / 2.0)
				ytrans = -cryst->b / 2.0;
			else
				ytrans = cryst->b / 2.0;
			for (k = 0; k <= atom0->num_point; k++) {
				xy[k].x = xy[k].x + xtrans;
				xy[k].y = xy[k].y + ytrans;
			}
			if (cryst->gamma != 90.0) {
				trans_coor_back(xy, new_xy, atom0->num_point,
					      cryst->gamma, cryst->win_width,
						cryst->win_height);
				XFillPolygon(display, window, cryst->gc, new_xy,
					     atom0->num_point, Convex,
					     CoordModeOrigin);
			} else
				XFillPolygon(display, window, cryst->gc, xy,
					     atom0->num_point, Convex,
					     CoordModeOrigin);
			if (centro[cryst->planegroup] == True) {
				XPoint      xy1[5];

				for (k = 0; k <= atom0->num_point; k++) {
					xy1[k].x = cryst->a - xy[k].x;
					xy1[k].y = cryst->b - xy[k].y;
				}
				if (cryst->gamma != 90.0) {
					trans_coor_back(xy1, new_xy, atom0->num_point,
					      cryst->gamma, cryst->win_width,
							cryst->win_height);
					XFillPolygon(display, window, cryst->gc,
						     new_xy, atom0->num_point,
						     Convex, CoordModeOrigin);
				} else
					XFillPolygon(display, window, cryst->gc,
						     xy1, atom0->num_point,
						     Convex, CoordModeOrigin);
			}
		}
	}
}

void
draw_crystal(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	crystalstruct *cryst = &crystals[MI_SCREEN(mi)];
	int         i;

	XSetFunction(display, cryst->gc, GXxor);
	for (i = 0; i < cryst->num_atom; i++) {
		crystalatom *atom0;

		atom0 = &cryst->atom[i];
		XSetForeground(display, cryst->gc, atom0->colour);
		crystal_drawatom(mi, atom0);
		atom0->velocity[0] += NRAND(3) - 1;
		atom0->velocity[0] = MAX(-20, MIN(20, atom0->velocity[0]));
		atom0->velocity[1] += NRAND(3) - 1;
		atom0->velocity[1] = MAX(-20, MIN(20, atom0->velocity[1]));
		atom0->x0 += atom0->velocity[0];
	   if ( cryst->gamma == 90.0 )
	     {
		if (atom0->x0 < 0)
			atom0->x0 += cryst->a;
		else if (atom0->x0 >= cryst->a)
			atom0->x0 -= cryst->a;
		atom0->y0 += atom0->velocity[1];
		if (atom0->y0 < 0)
			atom0->y0 += cryst->b;
		else if (atom0->y0 >= cryst->b)
			atom0->y0 -= cryst->b;
	     }
		atom0->velocity_a += ((float) NRAND(1001) - 500.0) / 2000.0;
		atom0->angle += atom0->velocity_a;
		crystal_setupatom(atom0, cryst->gamma);
		crystal_drawatom(mi, atom0);
	}
	XSetFunction(display, cryst->gc, GXcopy);
}

void
refresh_crystal(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	crystalstruct *cryst = &crystals[MI_SCREEN(mi)];
  int i;

	XClearWindow(display, MI_WINDOW(mi));
	XSetFunction(display, cryst->gc, GXxor);
	for (i = 0; i < cryst->num_atom; i++) {
		crystalatom *atom0;

		atom0 = &cryst->atom[i];
		XSetForeground(display, cryst->gc, atom0->colour);
		crystal_drawatom(mi, atom0);
	}
	XSetFunction(display, cryst->gc, GXcopy);
}

void
release_crystal(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);

	if (crystals != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			crystalstruct *cryst = &crystals[screen];

			if (cryst->gc != NULL)
				XFreeGC(display, cryst->gc);
			if (cryst->atom != NULL)
				(void) free((void *) cryst->atom);
		}
		(void) free((void *) crystals);
		crystals = NULL;
	}
}

void
init_crystal(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	crystalstruct *cryst;
	int         i, max_atoms, size_atom;

/* initialize */
	if (crystals == NULL) {
		if ((crystals = (crystalstruct *) calloc(MI_NUM_SCREENS(mi),
					    sizeof (crystalstruct))) == NULL)
			return;
	}
	cryst = &crystals[MI_SCREEN(mi)];

	if (!cryst->gc) {
		if ((cryst->gc = XCreateGC(display, MI_WINDOW(mi),
			     (unsigned long) 0, (XGCValues *) NULL)) == None)
			return;
	}
/* Clear Display */
	XClearWindow(display, MI_WINDOW(mi));
	XSetFunction(display, cryst->gc, GXxor);

	cryst->win_width = MI_WIN_WIDTH(mi);
	cryst->win_height = MI_WIN_HEIGHT(mi);
	cryst->num_atom = MI_BATCHCOUNT(mi);
	max_atoms = MI_BATCHCOUNT(mi);
	size_atom = MI_SIZE(mi);
	if (cryst->num_atom == 0) {
		cryst->num_atom = DEF_NUM_ATOM;
		max_atoms = DEF_NUM_ATOM;
	} else if (cryst->num_atom < 0) {
		max_atoms = -cryst->num_atom;
		cryst->num_atom = NRAND(-cryst->num_atom) + 1;
	}
	if (cryst->atom == NULL)
		cryst->atom = (crystalatom *) calloc(max_atoms, sizeof (
							       crystalatom));
	cryst->planegroup = NRAND(17);
	if (MI_WIN_IS_VERBOSE(mi))
		(void) printf("Selected plane group no %d\n", cryst->planegroup + 1);
	if (cryst->planegroup > 11)
		cryst->gamma = 120.0;
	else
		cryst->gamma = 90.0;
	cryst->a = cryst->win_width;
	cryst->b = cryst->win_height / cos((cryst->gamma - 90) * PI_RAD);
	for (i = 0; i < cryst->num_atom; i++) {
		crystalatom *atom0;

		atom0 = &cryst->atom[i];
		if (MI_NPIXELS(mi) > 2)
			atom0->colour = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
		else
			atom0->colour = 1;	/* Xor'red so WHITE may not be appropriate */
		XSetForeground(display, cryst->gc, atom0->colour);
		atom0->x0 = NRAND(cryst->win_width);
		atom0->y0 = NRAND(cryst->win_height);
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
		if (atom0->at_type == 2)
			atom0->num_point = 3;
		else
			atom0->num_point = 4;
		crystal_setupatom(atom0, cryst->gamma);
		crystal_drawatom(mi, atom0);
	}
	XSetFunction(display, cryst->gc, GXcopy);
}

