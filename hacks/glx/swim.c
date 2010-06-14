/* atlantis --- Shows moving 3D sea animals */

#if 0
static const char sccsid[] = "@(#)swim.c	1.3 98/06/18 xlockmore";
#endif

/* Copyright (c) E. Lassauge, 1998. */

/*
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
 * The original code for this mode was written by Mark J. Kilgard
 * as a demo for openGL programming.
 * 
 * Porting it to xlock  was possible by comparing the original Mesa's morph3d 
 * demo with it's ported version to xlock, so thanks for Marcelo F. Vianna 
 * (look at morph3d.c) for his indirect help.
 *
 * Thanks goes also to Brian Paul for making it possible and inexpensive
 * to use OpenGL at home.
 *
 * My e-mail address is lassauge@users.sourceforge.net
 *
 * Eric Lassauge  (May-13-1998)
 *
 */

/**
 * (c) Copyright 1993, 1994, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * Permission to use, copy, modify, and distribute this software for
 * any purpose and without fee is hereby granted, provided that the above
 * copyright notice appear in all copies and that both the copyright notice
 * and this permission notice appear in supporting documentation, and that
 * the name of Silicon Graphics, Inc. not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission.
 *
 * THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU "AS-IS"
 * AND WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR OTHERWISE,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR
 * FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL SILICON
 * GRAPHICS, INC.  BE LIABLE TO YOU OR ANYONE ELSE FOR ANY DIRECT,
 * SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY
 * KIND, OR ANY DAMAGES WHATSOEVER, INCLUDING WITHOUT LIMITATION,
 * LOSS OF PROFIT, LOSS OF USE, SAVINGS OR REVENUE, OR THE CLAIMS OF
 * THIRD PARTIES, WHETHER OR NOT SILICON GRAPHICS, INC.  HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH LOSS, HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
 * POSSESSION, USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * US Government Users Restricted Rights
 * Use, duplication, or disclosure by the Government is subject to
 * restrictions set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 252.227-7013 and/or in similar or successor
 * clauses in the FAR or the DOD or NASA FAR Supplement.
 * Unpublished-- rights reserved under the copyright laws of the
 * United States.  Contractor/manufacturer is Silicon Graphics,
 * Inc., 2011 N.  Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * OpenGL(TM) is a trademark of Silicon Graphics, Inc.
 */

#ifdef STANDALONE
# include <math.h>
# include "xlockmoreI.h"	/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"            /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#include "atlantis.h"

void
FishTransform(fishRec * fish)
{

	glTranslatef(fish->y, fish->z, -fish->x);
	glRotatef(-fish->psi, 0.0, 1.0, 0.0);
	glRotatef(fish->theta, 1.0, 0.0, 0.0);
	glRotatef(-fish->phi, 0.0, 0.0, 1.0);
}

void
WhalePilot(fishRec * fish, float whalespeed, Bool whaledir)
{

	fish->phi = -20.0;
	fish->theta = 0.0;
	fish->psi += ((whaledir) ? -0.5 : 0.5);

	fish->x += whalespeed * fish->v * cos(fish->psi / RAD) * cos(fish->theta / RAD);
	fish->y += whalespeed * fish->v * sin(fish->psi / RAD) * cos(fish->theta / RAD);
	fish->z += whalespeed * fish->v * sin(fish->theta / RAD);
}

void
SharkPilot(fishRec * fish, float sharkspeed)
{
	float       X, Y, Z, tpsi, ttheta, thetal;

	fish->xt = 60000.0;
	fish->yt = 0.0;
	fish->zt = 0.0;

	X = fish->xt - fish->x;
	Y = fish->yt - fish->y;
	Z = fish->zt - fish->z;

	thetal = fish->theta;

	ttheta = RAD * atan(Z / (sqrt(X * X + Y * Y)));

	if (ttheta > fish->theta + 0.25) {
		fish->theta += 0.5;
	} else if (ttheta < fish->theta - 0.25) {
		fish->theta -= 0.5;
	}
	if (fish->theta > 90.0) {
		fish->theta = 90.0;
	}
	if (fish->theta < -90.0) {
		fish->theta = -90.0;
	}
	fish->dtheta = fish->theta - thetal;

	tpsi = RAD * atan2(Y, X);

	fish->attack = 0;

	if (fabs(tpsi - fish->psi) < 10.0) {
		fish->attack = 1;
	} else if (fabs(tpsi - fish->psi) < 45.0) {
		if (fish->psi > tpsi) {
			fish->psi -= 0.5;
			if (fish->psi < -180.0) {
				fish->psi += 360.0;
			}
		} else if (fish->psi < tpsi) {
			fish->psi += 0.5;
			if (fish->psi > 180.0) {
				fish->psi -= 360.0;
			}
		}
	} else {
		if (NRAND(100) > 98) {
			fish->sign = (fish->sign < 0 ? 1 : -1);
		}
		fish->psi += (fish->sign ? 1 : -1);
		if (fish->psi > 180.0) {
			fish->psi -= 360.0;
		}
		if (fish->psi < -180.0) {
			fish->psi += 360.0;
		}
	}

	if (fish->attack) {
		if (fish->v < 1.1) {
			fish->spurt = 1;
		}
		if (fish->spurt) {
			fish->v += 0.2;
		}
		if (fish->v > 5.0) {
			fish->spurt = 0;
		}
		if ((fish->v > 1.0) && (!fish->spurt)) {
			fish->v -= 0.2;
		}
	} else {
		if (!(NRAND(400)) && (!fish->spurt)) {
			fish->spurt = 1;
		}
		if (fish->spurt) {
			fish->v += 0.05;
		}
		if (fish->v > 3.0) {
			fish->spurt = 0;
		}
		if ((fish->v > 1.0) && (!fish->spurt)) {
			fish->v -= 0.05;
		}
	}

	fish->x += sharkspeed * fish->v * cos(fish->psi / RAD) * cos(fish->theta / RAD);
	fish->y += sharkspeed * fish->v * sin(fish->psi / RAD) * cos(fish->theta / RAD);
	fish->z += sharkspeed * fish->v * sin(fish->theta / RAD);
}

void
SharkMiss(atlantisstruct * ap, int i)
{
	int         j;
	float       avoid, thetal;
	float       X, Y, Z, R;

	for (j = 0; j < ap->num_sharks; j++) {
		if (j != i) {
			X = ap->sharks[j].x - ap->sharks[i].x;
			Y = ap->sharks[j].y - ap->sharks[i].y;
			Z = ap->sharks[j].z - ap->sharks[i].z;

			R = sqrt(X * X + Y * Y + Z * Z);

			avoid = 1.0;
			thetal = ap->sharks[i].theta;

			if (R < ap->sharksize) {
				if (Z > 0.0) {
					ap->sharks[i].theta -= avoid;
				} else {
					ap->sharks[i].theta += avoid;
				}
			}
			ap->sharks[i].dtheta += (ap->sharks[i].theta - thetal);
		}
	}
}
#endif
