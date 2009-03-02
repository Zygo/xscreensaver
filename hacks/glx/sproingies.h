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

extern void DisplaySproingies(int screen,int pause);
extern void NextSproingieDisplay(int screen,int pause);
extern void ReshapeSproingies(int w, int h);
extern void CleanupSproingies(int screen);
extern void InitSproingies(int wfmode, int grnd, int mspr, int screen, 
                           int numscreens, int mono);
