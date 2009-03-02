/* -*- Mode: C; tab-width: 4 -*- */
/* atunnels --- OpenGL Advanced Tunnel Screensaver */

#if !defined( lint ) && !defined( SABER )
/* static const char sccsid[] = "@(#)tunnel_draw.h    5.13 2004/07/19 xlockmore"; */
#endif

/* Copyright (c) E. Lassauge, 2002-2004. */

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
 * The original code for this mode was written by Roman Podobedov
 * Email: romka@ut.ee
 * WEB: http://romka.demonews.com
 *
 * Eric Lassauge  (March-16-2002) <lassauge AT users.sourceforge.net>
 *                                  http://lassauge.free.fr/linux.html
 *
 */

#include <GL/gl.h>
#include <GL/glx.h>

#define MAX_TEXTURE	6

extern void DrawTunnel(int do_texture, int do_light, GLuint *textures);
extern void SplashScreen(int do_wire, int do_texture, int do_light);
extern Bool InitTunnel(void);
