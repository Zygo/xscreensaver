/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)crystal.c	4.03 97/05/07 xlockmore";

#endif

/* 
 * crystal.c - Polygons moving according to plane group rules for xlock,
 *             the X Window System lockscreen.
 *
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
 * Revision History:
 * 12-Jun-97: Created
 */

/*-
 * It is a moving polygon-mode. The polygons obbey 2D-planegroup symmetry.
 * At the moment the hexagonal groups do not work (I have not coded
 * non-orthogonal unit cells).
 */

#ifdef STANDALONE
#define PROGCLASS            "Crystal"
#define HACK_INIT            init_crystal
#define HACK_DRAW            draw_crystal
#define DEF_DELAY            60000
#define DEF_BATCHCOUNT       -40
#define DEF_CYCLES           200
#define DEF_SIZE             -15
#define DEF_NCOLORS          200
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
ModeSpecOpt crystal_opts =
{0, NULL, 0, NULL, NULL};

#endif /* STANDALONE */

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
	-1, 1, 0, -1, 0, 0,
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

} crystalstruct;

static crystalstruct *crystals = NULL;

void
crystal_setupatom(crystalatom * atom0)
{
	switch (atom0->at_type) {
		case 0:	/* rectangles */
			atom0->xy[0].x = atom0->x0 + 2 * atom0->size_at * cos(atom0->angle) +
				atom0->size_at * sin(atom0->angle);
			atom0->xy[0].y = atom0->y0 + atom0->size_at * cos(atom0->angle) - 2 *
				atom0->size_at * sin(atom0->angle);
			atom0->xy[1].x = atom0->x0 + 2 * atom0->size_at * cos(atom0->angle) -
				atom0->size_at * sin(atom0->angle);
			atom0->xy[1].y = atom0->y0 - atom0->size_at * cos(atom0->angle) - 2 *
				atom0->size_at * sin(atom0->angle);
			atom0->xy[2].x = atom0->x0 - 2 * atom0->size_at * cos(atom0->angle) -
				atom0->size_at * sin(atom0->angle);
			atom0->xy[2].y = atom0->y0 - atom0->size_at * cos(atom0->angle) + 2 *
				atom0->size_at * sin(atom0->angle);
			atom0->xy[3].x = atom0->x0 - 2 * atom0->size_at * cos(atom0->angle) +
				atom0->size_at * sin(atom0->angle);
			atom0->xy[3].y = atom0->y0 + atom0->size_at * cos(atom0->angle) + 2 *
				atom0->size_at * sin(atom0->angle);
			atom0->xy[4].x = atom0->xy[0].x;
			atom0->xy[4].y = atom0->xy[0].y;
			return;
		case 1:	/* squares */
			atom0->xy[0].x = atom0->x0 + 1.5 * atom0->size_at * cos(atom0->angle) +
				1.5 * atom0->size_at * sin(atom0->angle);
			atom0->xy[0].y = atom0->y0 + 1.5 * atom0->size_at * cos(atom0->angle) -
				1.5 * atom0->size_at * sin(atom0->angle);
			atom0->xy[1].x = atom0->x0 + 1.5 * atom0->size_at * cos(atom0->angle) -
				1.5 * atom0->size_at * sin(atom0->angle);
			atom0->xy[1].y = atom0->y0 - 1.5 * atom0->size_at * cos(atom0->angle) -
				1.5 * atom0->size_at * sin(atom0->angle);
			atom0->xy[2].x = atom0->x0 - 1.5 * atom0->size_at * cos(atom0->angle) -
				1.5 * atom0->size_at * sin(atom0->angle);
			atom0->xy[2].y = atom0->y0 - 1.5 * atom0->size_at * cos(atom0->angle) +
				1.5 * atom0->size_at * sin(atom0->angle);
			atom0->xy[3].x = atom0->x0 - 1.5 * atom0->size_at * cos(atom0->angle) +
				1.5 * atom0->size_at * sin(atom0->angle);
			atom0->xy[3].y = atom0->y0 + 1.5 * atom0->size_at * cos(atom0->angle) +
				1.5 * atom0->size_at * sin(atom0->angle);
			atom0->xy[4].x = atom0->xy[0].x;
			atom0->xy[4].y = atom0->xy[0].y;
			return;
		case 2:	/* triangles */
			atom0->xy[0].x = atom0->x0 + 1.5 * atom0->size_at * sin(atom0->angle);
			atom0->xy[0].y = atom0->y0 + 1.5 * atom0->size_at * cos(atom0->angle);
			atom0->xy[1].x = atom0->x0 + 1.5 * atom0->size_at * cos(atom0->angle) -
				1.5 * atom0->size_at * sin(atom0->angle);
			atom0->xy[1].y = atom0->y0 - 1.5 * atom0->size_at * cos(atom0->angle) -
				1.5 * atom0->size_at * sin(atom0->angle);
			atom0->xy[2].x = atom0->x0 - 1.5 * atom0->size_at * cos(atom0->angle) -
				1.5 * atom0->size_at * sin(atom0->angle);
			atom0->xy[2].y = atom0->y0 - 1.5 * atom0->size_at * cos(atom0->angle) +
				1.5 * atom0->size_at * sin(atom0->angle);
			atom0->xy[3].x = atom0->xy[0].x;
			atom0->xy[3].y = atom0->xy[0].y;
			return;
	}
}

void
crystal_drawatom(ModeInfo * mi, crystalatom * atom0)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	Window      window = MI_WINDOW(mi);
	crystalstruct *cryst;
	int         j, k;

	cryst = &crystals[MI_SCREEN(mi)];
	for (j = numops[2 * cryst->planegroup + 1];
	     j < numops[2 * cryst->planegroup]; j++) {
		XPoint      xy[5];
		int         xtrans, ytrans;

		xtrans = operation[j * 6] * atom0->x0 + operation[j * 6 + 1] *
			atom0->y0 + operation[j * 6 + 4] * cryst->a / 2.0;
		if (xtrans < 0)
			xtrans = cryst->a;
		else if (xtrans >= cryst->a)
			xtrans = -cryst->a;
		else
			xtrans = 0;
		ytrans = operation[j * 6 + 2] * atom0->x0 + operation[j * 6 + 3] *
			atom0->y0 + operation[j * 6 + 5] * cryst->b / 2.0;
		if (ytrans < 0)
			xtrans = cryst->b;
		else if (ytrans >= cryst->b)
			ytrans = -cryst->b;
		else
			ytrans = 0;
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
		XFillPolygon(display, window, gc, xy, atom0->num_point, Convex,
			     CoordModeOrigin);
		if (centro[cryst->planegroup] == True) {
			for (k = 0; k <= atom0->num_point; k++) {
				xy[k].x = cryst->a - xy[k].x;
				xy[k].y = cryst->b - xy[k].y;
			}
			XFillPolygon(display, window, gc, xy, atom0->num_point, Convex,
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
			XFillPolygon(display, window, gc, xy, atom0->num_point, Convex,
				     CoordModeOrigin);
			if (centro[cryst->planegroup] == True) {
				XPoint      xy1[5];

				for (k = 0; k <= atom0->num_point; k++) {
					xy1[k].x = cryst->a - xy[k].x;
					xy1[k].y = cryst->b - xy[k].y;
				}
				XFillPolygon(display, window, gc, xy1, atom0->num_point,
					     Convex, CoordModeOrigin);
			}
		}
	}
}

void
draw_crystal(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	crystalstruct *cryst;
	int         i;

	XSetFunction(display, gc, GXxor);
	cryst = &crystals[MI_SCREEN(mi)];
	for (i = 0; i < cryst->num_atom; i++) {
		crystalatom *atom0;

		atom0 = &cryst->atom[i];
		XSetForeground(display, gc, atom0->colour);
		crystal_drawatom(mi, atom0);
		atom0->velocity[0] += NRAND(3) - 1;
		atom0->velocity[0] = MAX(-20, MIN(20, atom0->velocity[0]));
		atom0->velocity[1] += NRAND(3) - 1;
		atom0->velocity[1] = MAX(-20, MIN(20, atom0->velocity[1]));
		atom0->x0 += atom0->velocity[0];
		if (atom0->x0 < 0)
			atom0->x0 += cryst->win_width;
		else if (atom0->x0 >= cryst->win_width)
			atom0->x0 -= cryst->win_width;
		atom0->y0 += atom0->velocity[1];
		if (atom0->y0 < 0)
			atom0->y0 += cryst->win_height;
		else if (atom0->y0 >= cryst->win_height)
			atom0->y0 -= cryst->win_height;
		atom0->velocity_a += ((float) NRAND(1001) - 500.0) / 2000.0;
		atom0->angle += atom0->velocity_a;
		crystal_setupatom(atom0);
		crystal_drawatom(mi, atom0);
	}
	XSetFunction(display, gc, GXcopy);
}

void
refresh_crystal(ModeInfo * mi)
{
}

void
release_crystal(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);

	XClearWindow(display, MI_WINDOW(mi));

	if (crystals != NULL) {
		int         i;

		for (i = 0; i < MI_NUM_SCREENS(mi); i++) {
			crystalstruct *cryst;

			cryst = &crystals[i];
			if (cryst->atom != NULL)
				(void) free((void *) cryst->atom);
		}
		(void) free((void *) crystals);
		crystals = NULL;
	}
	XSetFunction(display, gc, GXcopy);
}

void
init_crystal(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	crystalstruct *cryst;
	int         i, max_atoms, size_atom;

/* Clear Display */
	XClearWindow(display, MI_WINDOW(mi));
	XSetFunction(display, gc, GXxor);

/* initialize */
	if (crystals == NULL) {
		if ((crystals = (crystalstruct *) calloc(MI_NUM_SCREENS(mi),
					    sizeof (crystalstruct))) == NULL)
			return;
	}
	cryst = &crystals[MI_SCREEN(mi)];
	cryst->win_width = MI_WIN_WIDTH(mi);
	cryst->win_height = MI_WIN_HEIGHT(mi);
	cryst->a = cryst->win_width;
	cryst->b = cryst->win_height;
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
	cryst->planegroup = NRAND(12 /*17 hexagonal groups do not work yet */ );
	if (MI_WIN_IS_VERBOSE(mi))
		printf("Selected plane group no %d\n", cryst->planegroup + 1);
	if (cryst->planegroup > 11)
		cryst->gamma = 120.0;
	else
		cryst->gamma = 90.0;
	for (i = 0; i < cryst->num_atom; i++) {
		crystalatom *atom0;

		atom0 = &cryst->atom[i];
		if (MI_NPIXELS(mi) > 2)
			atom0->colour = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
		else
			atom0->colour = 1;	/* Xor'red so WHITE may not be appropriate */
		XSetForeground(display, gc, atom0->colour);
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
		crystal_setupatom(atom0);
		crystal_drawatom(mi, atom0);
	}
}
