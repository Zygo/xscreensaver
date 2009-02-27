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
 *
 * Revision History:
 *  3-May-2002: Added AI to pacman and ghosts, slowed down ghosts.
 * 26-Nov-2001: Random level generator added
 * 01-Nov-2000: Allocation checks
 * 04-Jun-1997: Compatible with xscreensaver
 *
 */

#ifndef __PACMAN_AI_H__
#define __PACMAN_AI_H__

extern void ghost_update(pacmangamestruct *pp, ghoststruct *g);
extern void pac_clear_trace(pacmanstruct *p);
extern void pac_update(ModeInfo * mi, pacmangamestruct *pp, 
                       pacmanstruct *p);

#endif /* __PACMAN_AI_H__ */
