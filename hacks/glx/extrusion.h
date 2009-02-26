/* -*- Mode: C; tab-width: 4 -*- */
/* extrusion --- extrusion module for xscreensaver */
/*-
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

#ifndef __XSCREENSAVER_EXTRUSION_H__
#define __XSCREENSAVER_EXTRUSION_H__

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_COCOA
# include <OpenGL/gl.h>
# include <OpenGL/glu.h>
# include <GLUT/tube.h>    /* gle is included with GLUT on OSX */
#else  /* !HAVE_COCOA */
# include <GL/gl.h>
# include <GL/glu.h>
# ifdef HAVE_GLE3
#  include <GL/gle.h>
# else
#  include <GL/tube.h>
# endif
#endif /* !HAVE_COCOA */

extern void InitStuff_helix2(void);
extern void DrawStuff_helix2(void);
extern void InitStuff_helix3(void);
extern void DrawStuff_helix3(void);
extern void InitStuff_helix4(void);
extern void DrawStuff_helix4(void);
extern void InitStuff_joinoffset(void);
extern void DrawStuff_joinoffset(void);
extern void InitStuff_screw(void);
extern void DrawStuff_screw(void);
extern void InitStuff_taper(void);
extern void DrawStuff_taper(void);
extern void InitStuff_twistoid(void);
extern void DrawStuff_twistoid(void);

#endif /* __XSCREENSAVER_EXTRUSION_H__ */
