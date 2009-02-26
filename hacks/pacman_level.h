/*-
 * Copyright (c) 2002 by Edwin de Jong <mauddib@gmx.net>.
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
 */

#ifndef __PACMAN_LEVEL_H__
#define __PACMAN_LEVEL_H__

/* typedef struct { */
/*   int x, y; */
/* } XY; */

extern int createnewlevel (ModeInfo * mi);
extern int check_pos (pacmangamestruct * pp, int y, int x, int ghostpass);
extern int check_dot (pacmangamestruct * pp, unsigned int x, unsigned int y);
extern int is_bonus_dot (int x, int y, int *idx);
extern int bonus_dot_eaten (int idx);
extern void eat_bonus_dot (int idx);
extern void bonus_dot_pos (int idx, int *x, int *y);
extern void get_jail_opening ( int *x, int *y);
#endif /* __PACMAN_LEVEL_H__ */
