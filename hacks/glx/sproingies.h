/*-
 *  sproingies.c - Copyright 1996 by Ed Mackey, freely distributable.
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
 * Revision History:
 * See sproingiewrap.c
 */

struct sPosColor {   /* Position and color of the sproingie     */
	int x, y, z;     /*   Position                              */
	int frame;       /*   Current frame (0-5)                   */
	int life;        /*   Life points                           */
	GLfloat r, g, b; /*   Color RGB                             */
	int direction;   /*   Direction of next hop (left or right) */
};

typedef struct {
	int         rotx, roty, dist, wireframe, flatshade, groundlevel,
	            maxsproingies, mono;
	int         sframe, target_rx, target_ry, target_dist, target_count;
	const struct gllist *sproingies[6];
	const struct gllist *SproingieBoom;
	GLuint TopsSides;
	struct sPosColor *positions;
} sp_instance;

extern void DisplaySproingies(sp_instance *si);
extern void NextSproingieDisplay(sp_instance *si);
extern void ReshapeSproingies(int w, int h);
extern void CleanupSproingies(sp_instance *si);
extern void InitSproingies(sp_instance *, int wfmode, int grnd, int mspr,
                           int smrtspr, int mono);
